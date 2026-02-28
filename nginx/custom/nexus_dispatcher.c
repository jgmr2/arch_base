#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* --- 1. ESTRUCTURAS DEL NEXUS (Capa de Aplicación en RAM) --- */

typedef struct {
    ngx_atomic_t      total_requests; // Contador atómico compartido
    ngx_atomic_t      active_connections;
    u_char            engine_status[32];
} ngx_shm_data_t;

// Puntero global para acceder al Nexus desde cualquier worker
static ngx_shm_data_t  *nexus_data;

/* --- 2. UTILIDADES DE RESPUESTA --- */

static ngx_int_t send_json_response(ngx_http_request_t *r, const char *json_text) {
    ngx_buf_t    *b;
    ngx_chain_t   out;
    size_t        len = ngx_strlen(json_text);

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;
    ngx_str_set(&r->headers_out.content_type, "application/json");

    ngx_http_send_header(r);

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;

    b->pos = (u_char *) json_text;
    b->last = b->pos + len;
    b->memory = 1;
    b->last_buf = 1;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

/* --- 3. GESTIÓN DE MEMORIA COMPARTIDA (Slab Allocator) --- */

static ngx_int_t
ngx_http_shm_nexus_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_slab_pool_t  *shpool;

    // Si ya existe la data (recarga de nginx), la reutilizamos
    if (data) {
        shm_zone->data = data;
        nexus_data = data;
        return NGX_OK;
    }

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    // Reservamos el bloque en el Nexus
    nexus_data = ngx_slab_alloc(shpool, sizeof(ngx_shm_data_t));
    if (nexus_data == NULL) return NGX_ERROR;

    // Inicialización limpia
    ngx_memzero(nexus_data, sizeof(ngx_shm_data_t));
    ngx_cpystrn(nexus_data->engine_status, (u_char *) "Nexus-Running", 14);

    shm_zone->data = nexus_data;
    return NGX_OK;
}

/* --- 4. LÓGICA DE LOS ENDPOINTS (Bypass OSI) --- */

static ngx_int_t logic_hola(ngx_http_request_t *r) {
    u_char response[256];
    
    // Incremento atómico: Velocidad pura sin semáforos
    ngx_atomic_fetch_add(&nexus_data->total_requests, 1);

    ngx_sprintf(response, 
        "{\"status\":\"success\",\"requests\":%uA,\"engine\":\"%s\"}", 
        nexus_data->total_requests, nexus_data->engine_status);

    return send_json_response(r, (char *) response);
}

static ngx_int_t logic_version(ngx_http_request_t *r) {
    return send_json_response(r, "{\"version\":\"1.26.0-shm-nexus\",\"bypass_layers\":\"OSI 2-5\"}");
}

/* --- 5. EL DESPACHADOR (GATEWAY) --- */

static ngx_int_t ngx_http_dispatcher_handler(ngx_http_request_t *r) {
    if (!(r->method & NGX_HTTP_GET)) return NGX_HTTP_NOT_ALLOWED;

    if (ngx_strstr(r->uri.data, "/api/hola")) {
        return logic_hola(r);
    }
    
    if (ngx_strstr(r->uri.data, "/api/version")) {
        return logic_version(r);
    }

    return send_json_response(r, "{\"error\":\"Nexus Endpoint not found\"}");
}

/* --- 6. REGISTRO DE DIRECTIVAS Y MÓDULO --- */

static char *ngx_http_shm_nexus_zone_cmd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_str_t        *value = cf->args->elts;
    ngx_shm_zone_t   *shm_zone;
    ssize_t           size;

    size = ngx_parse_size(&value[1]);
    if (size == NGX_ERROR) return "invalid size";

    // Registro de la zona SHM
    shm_zone = ngx_shared_memory_add(cf, &value[0], size, &ngx_http_dispatcher_module);
    if (shm_zone == NULL) return NGX_CONF_ERROR;

    shm_zone->init = ngx_http_shm_nexus_init_zone;
    return NGX_CONF_OK;
}

static char *ngx_http_dispatcher_cmd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_dispatcher_handler;
    return NGX_CONF_OK;
}

static ngx_command_t ngx_http_dispatcher_commands[] = {
    { ngx_string("shm_nexus_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_shm_nexus_zone_cmd,
      0, 0, NULL },

    { ngx_string("shm_nexus_dispatcher"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_dispatcher_cmd,
      0, 0, NULL },

    ngx_null_command
};

static ngx_http_module_t ngx_http_dispatcher_module_ctx = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

ngx_module_t ngx_http_dispatcher_module = {
    NGX_MODULE_V1,
    &ngx_http_dispatcher_module_ctx,
    ngx_http_dispatcher_commands,
    NGX_HTTP_MODULE,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NGX_MODULE_V1_PADDING
};