#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <GeoIP.h>
#include <GeoIPCity.h>


typedef struct {
    GeoIP        *city;
} ngx_http_geoip_format_conf_t;

static const char *_mk_NA(const char *p)
{
    return p ? p : "N/A";
}

static void *ngx_http_geoip_format_create_conf(ngx_conf_t *cf);
static char *ngx_http_geoip_city(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void ngx_http_geoip_format_cleanup(void *data);


static ngx_command_t  ngx_http_geoip_format_commands[] = {


    { ngx_string("geoip_format_city"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_geoip_city,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_geoip_format_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    ngx_http_geoip_format_create_conf,     /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_geoip_format_module = {
    NGX_MODULE_V1,
    &ngx_http_geoip_format_module_ctx,            /* module context */
    ngx_http_geoip_format_commands,               /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_geoip_format_handler(ngx_http_request_t *r)
{
    struct sockaddr_in        *sin;
    ngx_addr_t                addr;
    ngx_str_t                 ipv;
    ngx_table_elt_t           *ch;
    GeoIPRecord               *gr;
    ngx_http_complex_value_t  cv;
    ngx_http_geoip_format_conf_t     *gcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (ngx_http_arg(r,(u_char *)"ip", 2, &ipv) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_parse_addr(r->pool, &addr, ipv.data, ipv.len) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));

    ch = ngx_list_push(&r->headers_out.headers);

    if (ch == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ch->hash = 1;
    ngx_str_set(&ch->key, "X-IS-HEEEE");
    ngx_str_set(&ch->value, "12345645646");

    cv.value.data = ngx_pcalloc(r->pool,64);
    if(cv.value.data == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    sin = (struct sockaddr_in *) addr.sockaddr;

    gcf = ngx_http_get_module_main_conf(r, ngx_http_geoip_format_module);

    if (gcf->city) {
        gr = GeoIP_record_by_ipnum(gcf->city,  ntohl(sin->sin_addr.s_addr));
    }

    if(gr == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cv.value.len = ngx_snprintf(cv.value.data,64,"Hello %s , %s",_mk_NA(gr->city),_mk_NA(gr->country_name)) - cv.value.data;

    GeoIPRecord_delete(gr);
    return ngx_http_send_response(r, NGX_HTTP_OK , NULL, &cv);
}


static void *
ngx_http_geoip_format_create_conf(ngx_conf_t *cf)
{
    ngx_pool_cleanup_t     *cln;
    ngx_http_geoip_format_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_geoip_format_conf_t));
    if (conf == NULL) {
        return NULL;
    }


    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NULL;
    }
    

    cln->handler = ngx_http_geoip_format_cleanup;
    cln->data = conf;

    return conf;
}


static char *
ngx_http_geoip_city(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_geoip_format_conf_t  *gcf = conf;

    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t  *value;

    if (gcf->city) {
        return "is duplicate";
    }

    value = cf->args->elts;

    gcf->city = GeoIP_open((char *) value[1].data, GEOIP_MEMORY_CACHE);

    if (gcf->city == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "GeoIP_open(\"%V\") failed", &value[1]);

        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 3) {
        if (ngx_strcmp(value[2].data, "utf8") == 0) {
            GeoIP_set_charset (gcf->city, GEOIP_CHARSET_UTF8);

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                              "invalid parameter \"%V\"", &value[2]);
            return NGX_CONF_ERROR;
        }   
    }   

    switch (gcf->city->databaseType) {

    case GEOIP_CITY_EDITION_REV0:
    case GEOIP_CITY_EDITION_REV1:
        clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
        clcf->handler = ngx_http_geoip_format_handler;
        return NGX_CONF_OK;
    default:
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid GeoIP City database \"%V\" type:%d",
                           &value[1], gcf->city->databaseType);
        return NGX_CONF_ERROR;
    }
}




static void
ngx_http_geoip_format_cleanup(void *data)
{
    ngx_http_geoip_format_conf_t  *gcf = data;

    if (gcf->city) {
        GeoIP_delete(gcf->city);
    }
}
