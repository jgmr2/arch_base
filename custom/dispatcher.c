#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

// Función auxiliar para enviar respuestas JSON
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

// --- TUS MODULOS / LOGICA ---

static ngx_int_t logic_hola(ngx_http_request_t *r) {
    return send_json_response(r, "{\"status\":\"success\",\"message\":\"Hola desde C nativo\"}");
}

static ngx_int_t logic_version(ngx_http_request_t *r) {
    return send_json_response(r, "{\"version\":\"1.26.0-custom\",\"engine\":\"Nginx-Static\"}");
}

// --- EL DESPACHADOR (GATEWAY) ---

static ngx_int_t ngx_http_dispatcher_handler(ngx_http_request_t *r) {
    // Solo permitimos GET para estas APIs
    if (!(r->method & NGX_HTTP_GET)) return NGX_HTTP_NOT_ALLOWED;

    // Enrutamiento simple por texto en la URI
    if (ngx_strstr(r->uri.data, "/api/hola")) {
        return logic_hola(r);
    }
    
    if (ngx_strstr(r->uri.data, "/api/version")) {
        return logic_version(r);
    }

    // Si no coincide ninguna ruta
    return send_json_response(r, "{\"error\":\"Endpoint no encontrado en modulo C\"}");
}

// Registro técnico del módulo
static char *ngx_http_dispatcher_cmd(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_dispatcher_handler;
    return NGX_CONF_OK;
}

static ngx_command_t ngx_http_dispatcher_commands[] = {
    { ngx_string("hola_mundo_dispatcher"),
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