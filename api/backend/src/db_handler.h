#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <stdio.h>
#include <libpq-fe.h>

#define DB_CONN_STR "host=/tmp user=postgres dbname=biometrics"

// Variable global para mantener la conexión viva
static PGconn *global_conn = NULL;

/* Inicializa la conexión una sola vez al arrancar el server */
int init_db_connection() {
    global_conn = PQconnectdb(DB_CONN_STR);
    if (PQstatus(global_conn) != CONNECTION_OK) {
        fprintf(stderr, "Error conexión inicial: %s\n", PQerrorMessage(global_conn));
        return -1;
    }
    return 0;
}

void fetch_biometrics_count(char *dest, size_t max_len) {
    // Verificamos si la conexión sigue viva (por si la DB se reinicia)
    if (PQstatus(global_conn) != CONNECTION_OK) {
        PQreset(global_conn);
    }

    PGresult *res = PQexec(global_conn, "SELECT count(*) FROM biometrics");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        snprintf(dest, max_len, "Query Error");
    } else {
        char *val = PQgetvalue(res, 0, 0);
        snprintf(dest, max_len, "%s", val);
    }

    PQclear(res);
    // IMPORTANTE: NO hacemos PQfinish aquí
}

/* Se llama al apagar el servidor */
void close_db_connection() {
    if (global_conn) PQfinish(global_conn);
}

#endif