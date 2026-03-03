#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <liburing.h>
#include <errno.h>
#include "db_handler.h" // Importamos tu lógica de UDS

#define PORT 80
#define BACKLOG 8192
#define MAX_CONNECTIONS 4096

enum { ACCEPT, READ, WRITE };

typedef struct {
    int fd;
    int type;
    char buffer[2048];
} conn_info;

struct sockaddr_in g_client_addr;
socklen_t g_client_len = sizeof(g_client_addr);
conn_info g_accept_conn; 

// --- FUNCIONES DE APOYO ---

void add_accept(struct io_uring *ring, int server_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) return;
    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&g_client_addr, &g_client_len, 0);
    g_accept_conn.type = ACCEPT;
    g_accept_conn.fd = server_fd;
    io_uring_sqe_set_data(sqe, &g_accept_conn);
}

void add_read(struct io_uring *ring, int client_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) return;
    conn_info *conn = malloc(sizeof(conn_info));
    if (!conn) return;
    conn->fd = client_fd;
    conn->type = READ;
    io_uring_prep_recv(sqe, client_fd, conn->buffer, sizeof(conn->buffer), 0);
    io_uring_sqe_set_data(sqe, conn);
}

void add_write(struct io_uring *ring, int client_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) return;

    conn_info *conn = malloc(sizeof(conn_info));
    if (!conn) return;
    conn->fd = client_fd;
    conn->type = WRITE;

    // 1. Consultar la DB a través del Socket Unix (UDS)
    char db_val[64];
    fetch_biometrics_count(db_val, sizeof(db_val));

    // 2. Formatear el cuerpo de la respuesta
    char body[128];
    int body_len = snprintf(body, sizeof(body), "Registros Biometrics: %s\n", db_val);

    // 3. Construir Header HTTP + Body
    // Usamos el buffer del struct conn para almacenar la respuesta y que sea segura para io_uring
    int full_len = snprintf(conn->buffer, sizeof(conn->buffer),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %d\r\n"
             "Connection: keep-alive\r\n\r\n"
             "%s", body_len, body);

    io_uring_prep_send(sqe, client_fd, conn->buffer, full_len, 0);
    io_uring_sqe_set_data(sqe, conn);
}

// --- MAIN LOOP ---

int main() {
    struct io_uring ring;
    // Inicialización de io_uring
    int ret = io_uring_queue_init(MAX_CONNECTIONS, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "Error inicializando io_uring: %s\n", strerror(-ret));
        return 1;
    }

    // Configuración del Socket de Servidor (TCP para el mundo exterior)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error en bind");
        return 1;
    }
    listen(server_fd, BACKLOG);

    add_accept(&ring, server_fd);

    printf("Server io_uring + Postgres UDS activo en puerto %d\n", PORT);
    fflush(stdout);

    while (1) {
        struct io_uring_cqe *cqe;
        
        ret = io_uring_submit_and_wait(&ring, 1);
        if (ret < 0) continue;

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) continue;

        conn_info *conn = (conn_info *)io_uring_cqe_get_data(cqe);
        
        if (conn) {
            if (cqe->res < 0) {
                if (conn->type != ACCEPT) {
                    close(conn->fd);
                    free(conn);
                }
            } else {
                switch (conn->type) {
                    case ACCEPT:
                        add_read(&ring, cqe->res); 
                        add_accept(&ring, server_fd); 
                        break;
                    case READ:
                        if (cqe->res == 0) { 
                            close(conn->fd);
                        } else {
                            add_write(&ring, conn->fd);
                        }
                        free(conn);
                        break;
                    case WRITE:
                        // Volvemos a modo lectura para mantener la conexión Keep-Alive
                        add_read(&ring, conn->fd);
                        free(conn);
                        break;
                }
            }
        }
        io_uring_cqe_seen(&ring, cqe);
    }
    
    io_uring_queue_exit(&ring);
    return 0;
}