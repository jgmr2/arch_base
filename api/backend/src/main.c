#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <liburing.h>
#include <errno.h>

#define PORT 80
#define BACKLOG 8192
#define MAX_CONNECTIONS 4096

enum { ACCEPT, READ, WRITE };

typedef struct {
    int fd;
    int type;
    char buffer[2048];
} conn_info;

// Datos persistentes para evitar Segfaults por Stack-Scraping
struct sockaddr_in g_client_addr;
socklen_t g_client_len = sizeof(g_client_addr);
conn_info g_accept_conn; // Una estructura fija para el proceso de ACCEPT

static const char* RESPONSE = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 12\r\n"
    "Connection: keep-alive\r\n\r\n"
    "Hello World!";

void add_accept(struct io_uring *ring, int server_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) return;

    io_uring_prep_accept(sqe, server_fd, (struct sockaddr *)&g_client_addr, &g_client_len, 0);
    
    // NO usamos malloc aquí para el accept inicial, usamos la global
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

    io_uring_prep_send(sqe, client_fd, RESPONSE, strlen(RESPONSE), 0);
    io_uring_sqe_set_data(sqe, conn);
}

int main() {
    struct io_uring ring;
    int ret = io_uring_queue_init(MAX_CONNECTIONS, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: %s\n", strerror(-ret));
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(server_fd, BACKLOG);

    add_accept(&ring, server_fd);

    printf("Server io_uring activo en puerto %d\n", PORT);
    fflush(stdout);

    while (1) {
        struct io_uring_cqe *cqe;
        
        // 1. Submit y esperar (Operación atómica para evitar loops vacíos)
        ret = io_uring_submit_and_wait(&ring, 1);
        if (ret < 0) continue;

        // 2. Procesar el CQE
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) continue;

        conn_info *conn = (conn_info *)io_uring_cqe_get_data(cqe);
        
        if (conn) {
            if (cqe->res < 0) {
                // Error en socket o conexión abortada
                if (conn->type != ACCEPT) {
                    close(conn->fd);
                    free(conn);
                }
            } else {
                switch (conn->type) {
                    case ACCEPT:
                        // El ACCEPT usa g_accept_conn (no se libera)
                        add_read(&ring, cqe->res); 
                        add_accept(&ring, server_fd); // Re-armar
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
                        add_read(&ring, conn->fd);
                        free(conn);
                        break;
                }
            }
        }
        io_uring_cqe_seen(&ring, cqe);
    }
    return 0;
}