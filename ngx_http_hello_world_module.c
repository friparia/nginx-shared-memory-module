#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct{
  ngx_shm_zone_t *shm_zone;
} ngx_http_hello_world_loc_conf_t;

typedef struct{
  int count;
} ngx_http_hello_world_shm_count_t;

static char* ngx_http_hello_world(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
static void* ngx_http_hello_world_create_loc_conf(ngx_conf_t* cf);
static char* ngx_http_hello_world_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);

static ngx_int_t ngx_http_hello_world_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);

static ngx_command_t ngx_http_hello_world_commands[] = {
  {
    ngx_string("hello_world"), //The command name
    NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_http_hello_world, //The command handler
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_hello_world_loc_conf_t, shm_zone),
    NULL
  },
  ngx_null_command
};

static ngx_http_module_t ngx_http_hello_world_module_ctx = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  ngx_http_hello_world_create_loc_conf,
  ngx_http_hello_world_merge_loc_conf
};

ngx_module_t ngx_http_hello_world_module = {
  NGX_MODULE_V1,
  &ngx_http_hello_world_module_ctx,
  ngx_http_hello_world_commands,
  NGX_HTTP_MODULE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NGX_MODULE_V1_PADDING
};


static ngx_int_t ngx_http_hello_world_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data){
  ngx_slab_pool_t *shpool;
  ngx_http_hello_world_shm_count_t *shm_count;
  if(data){ 
    shm_zone->data = data;
    return NGX_OK;
  }
  shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;
  shm_count = ngx_slab_alloc(shpool, sizeof *shm_count);
  shm_count->count = 0;
  shm_zone->data = shm_count;
  return NGX_OK;
}


static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t* r){
  ngx_http_hello_world_loc_conf_t *lccf;
  ngx_shm_zone_t *shm_zone;
  ngx_slab_pool_t *shpool;
  int count;
  ngx_int_t rc;
  ngx_buf_t* b;
  ngx_chain_t out;

  lccf = ngx_http_get_module_loc_conf(r, ngx_http_hello_world_module);

  if(lccf->shm_zone == NULL){
    return NGX_DECLINED;
  }
  shm_zone = lccf->shm_zone;

  shpool = (ngx_slab_pool_t *) lccf->shm_zone->shm.addr;
  ngx_shmtx_lock(&shpool->mutex);
  count = ((ngx_http_hello_world_shm_count_t *)shm_zone->data)->count;
  count = count+1;
  ((ngx_http_hello_world_shm_count_t *)shm_zone->data)->count = count;
  ngx_shmtx_unlock(&shpool->mutex);

  
  r->headers_out.content_type.len = sizeof("text/plain") - 1;
  r->headers_out.content_type.data = (u_char*)"text/plain";

  b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
  /*  */
  out.buf = b;
  out.next = NULL;
  char string[10];
  sprintf(string, "%d", count);
  ngx_str_t count_str = ngx_string(string);
  b->pos = count_str.data;
  b->last = count_str.data + count_str.len;
  b->memory = 1;
  b->last_buf = 1;

  r->headers_out.content_type.len = sizeof("text/html") - 1;
  r->headers_out.content_type.data = (u_char *) "text/html";
  r->headers_out.status = NGX_HTTP_OK;
  r->headers_out.content_length_n = count_str.len;

  rc = ngx_http_send_header(r);
  if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
    return rc;
  }

  return ngx_http_output_filter(r, &out);
}

static char* ngx_http_hello_world(ngx_conf_t* cf, ngx_command_t* cmd, void* conf){
  ngx_http_core_loc_conf_t *clcf;

  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_hello_world_handler;
  ngx_conf_set_str_slot(cf, cmd, conf);

  return NGX_CONF_OK;
}

static void* ngx_http_hello_world_create_loc_conf(ngx_conf_t* cf){
  ngx_http_hello_world_loc_conf_t* conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_hello_world_loc_conf_t));
  if(conf == NULL){
    return NGX_CONF_ERROR;
  }
  return conf;
}

static char* ngx_http_hello_world_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child){
  ngx_shm_zone_t *shm_zone;
  ngx_str_t *shm_name;
  ngx_http_hello_world_loc_conf_t* prev = parent;
  ngx_http_hello_world_loc_conf_t* conf = child;

  shm_name = ngx_palloc(cf->pool, sizeof *shm_name);
  shm_name->len = sizeof("shared_memory") - 1;
  shm_name->data = (unsigned char *) "shared_memory";
  shm_zone = ngx_shared_memory_add(cf, shm_name, 8 * ngx_pagesize, &ngx_http_hello_world_module);

  if(shm_zone == NULL){
    return NGX_CONF_ERROR;
  }

  shm_zone->init = ngx_http_hello_world_init_shm_zone;
  conf->shm_zone = shm_zone;

  ngx_conf_merge_ptr_value(conf->shm_zone, prev->shm_zone, NULL);
  return NGX_CONF_OK;
}

