#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define PORT 80
#define BACKLOG 8192 // Elevado para absorber ráfagas masivas

// 1. Cabeceras estáticas listas en memoria (Zero sprintf)
static const char* HTTP_HEADER = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 12\r\n"
    "Connection: keep-alive\r\n\r\n";
    
static const char* HTTP_BODY = "Hello World!";

// Estructura optimizada: No necesita malloc interno para el buffer
typedef struct {
    uv_write_t req;
    uv_buf_t bufs[2]; // Usaremos 2 buffers para Vectorized I/O
} write_batch_t;

// Pool de memoria estática para los sockets de clientes (opcional, pero ayuda)
void on_close(uv_handle_t* handle) {
    free(handle);
}

void on_write_end(uv_write_t* req, int status) {
    // Liberamos solo la estructura de control, NO los datos (son estáticos)
    free(req);
}

// Reutilizamos un buffer pequeño para lecturas para evitar mallocs gigantes
void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    static char read_buf[4096]; // Buffer estático para lectura (solo si el loop es single-thread!)
    buf->base = read_buf;
    buf->len = sizeof(read_buf);
}

void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        // 2. Vectorized I/O (writev): Enviamos header y body sin concatenar strings
        write_batch_t *batch = (write_batch_t*) malloc(sizeof(write_batch_t));
        
        // Apuntamos directamente a nuestras constantes en el segmento de DATA
        batch->bufs[0] = uv_buf_init((char*)HTTP_HEADER, strlen(HTTP_HEADER));
        batch->bufs[1] = uv_buf_init((char*)HTTP_BODY, strlen(HTTP_BODY));

        // Enviamos ambos buffers en un solo viaje al kernel
        if (uv_write(&batch->req, client, batch->bufs, 2, on_write_end)) {
            free(batch);
        }
    } else if (nread < 0) {
        if (!uv_is_closing((uv_handle_t*)client)) {
            uv_close((uv_handle_t*)client, on_close);
        }
    }
    // IMPORTANTE: Ya no liberamos buf->base porque es estático
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) return;

    uv_tcp_t* client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);

    // Optimizaciones de socket a nivel de Kernel
    uv_tcp_nodelay(client, 1); // Desactiva algoritmo de Nagle para latencia ultra-baja

    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, alloc_buffer, on_read);
    } else {
        uv_close((uv_handle_t*) client, on_close);
    }
}

int main() {
    uv_loop_t* loop = uv_default_loop();
    uv_tcp_t server;

    uv_tcp_init(loop, &server);
    
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    
    int r = uv_listen((uv_stream_t*) &server, BACKLOG, on_new_connection);
    if (r) return 1;

    printf("Server C TURBO activo en el puerto %d\n", PORT);
    return uv_run(loop, UV_RUN_DEFAULT);
}