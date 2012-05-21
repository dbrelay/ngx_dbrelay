/*
 * DB Relay is an HTTP module built on the NGiNX webserver platform which 
 * communicates with a variety of database servers and returns JSON formatted 
 * data.
 * 
 * Copyright (C) 2008-2010 Getco LLC
 * Copyright (C) 2002-2009 Igor Syosev
 * 
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) 
 * any later version. In addition, redistributions in source code and in binary 
 * form must 
 * include the above copyright notices, and each of the following disclaimers. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT OWNERS AND CONTRIBUTORS “AS IS” 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANY COPYRIGHT OWNERS OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_upstream.h>
#include "dbrelay.h"
#if HAVE_FREETDS
#include <sybdb.h>
#endif

#ifndef DDEBUG
#define DDEBUG 1
#endif
#include "ngx_http_dbrelay_ddebug.h"


typedef struct {
    ngx_http_upstream_conf_t   upstream;
    ngx_str_t   origin;
} ngx_http_dbrelay_loc_conf_t;

void parse_post_query_string(ngx_chain_t *bufs, dbrelay_request_t *request);
void parse_post_query_file(ngx_temp_file_t *bufs, dbrelay_request_t *request);
void parse_get_query_string(ngx_str_t args, dbrelay_request_t *request);
static char *ngx_http_dbrelay_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//static ngx_int_t ngx_http_dbrelay_create_request(ngx_http_request_t *r);
static void *ngx_http_dbrelay_create_loc_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_dbrelay_send_response(ngx_http_request_t *r);
ngx_int_t ngx_http_dbrelay_init_master(ngx_log_t *log);
void ngx_http_dbrelay_exit_master(ngx_cycle_t *cycle);
static void write_flag_values(dbrelay_request_t *request, char *value);
static unsigned int accepts_application_json(ngx_http_request_t *r);
static u_char *get_header_value(ngx_http_request_t *r, char *header_key);

extern dbrelay_emitapi_t dbrelay_jsondict_api;
extern dbrelay_emitapi_t dbrelay_jsonarr_api;
extern dbrelay_emitapi_t dbrelay_csv_api;

static ngx_command_t  ngx_http_dbrelay_commands[] = {

    { ngx_string("dbrelay"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_dbrelay_set,
      0,
      0,
      NULL },

    { ngx_string("dbrelay_origin"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_dbrelay_loc_conf_t,origin),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_dbrelay_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_dbrelay_create_loc_conf, /* create location configuration */
    NULL                           /* merge location configuration */
};


ngx_module_t  ngx_http_dbrelay_module = {
    NGX_MODULE_V1,
    &ngx_http_dbrelay_module_ctx, /* module context */
    ngx_http_dbrelay_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    ngx_http_dbrelay_init_master,  /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    ngx_http_dbrelay_exit_master,  /* exit master */
    NGX_MODULE_V1_PADDING
};

ngx_int_t
ngx_http_dbrelay_init_master(ngx_log_t *log)
{
   dd("entering");
#if HAVE_FREETDS
    dbinit();
#endif

   dd("returning NGX_OK");
   return NGX_OK;
}

void 
ngx_http_dbrelay_exit_master(ngx_cycle_t *cycle)
{
   dbrelay_connection_t *connections;
   int i, s;
   pid_t pid = 0;
   int error;

   dd("entering");

   connections = dbrelay_get_shmem();

   if (!connections) return;

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     if (connections[i].sock_path && strlen(connections[i].sock_path)) {
         s = dbrelay_socket_connect(connections[i].sock_path, 2, &error);
         if (s!=-1) dbrelay_conn_kill(s);
     }
     if (connections[i].helper_pid) {
        pid = connections[i].helper_pid;
	if (!kill(pid, 0)) kill(pid, SIGTERM);
     }
   }

   usleep(500000); // give graceful kills some time to work

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     if (connections[i].helper_pid) {
        if (!kill(pid, 0)) kill(pid, SIGKILL);
     }
   }

   dbrelay_release_shmem(connections);
   dbrelay_destroy_shmem();
   dd("returning");
}
static unsigned int
origin_matches(ngx_http_request_t *r, ngx_str_t origin)
{
    char *origin_string, *s;
    int match = 0;
    u_char *header_value;

    dd("entering");

    header_value = get_header_value(r, "Origin");
    if (!header_value) {
       return 0;
    }
    dd("origin header %s", header_value);

    origin_string = (char *) malloc(origin.len + 1);
    memcpy(origin_string, origin.data, origin.len);
    origin_string[origin.len]='\0';

    s = strtok((char *)origin_string, ",");
    if (s) do {
          dd("origin arg");
          if (s[0]=='*') {
            if (!strcmp(&s[1], (char *) &header_value[strlen((char *) header_value) - strlen(s) + 1])) match=1;
          } else if (!strcmp(s, (char *) header_value)) match = 1;
          s = strtok(NULL, ",");
    } while (s);
    free(header_value);
    free(origin_string);
    dd("returning %d", match);
    return match;
}
static unsigned int
accepts_application_json(ngx_http_request_t *r)
{
    u_char        *header_value;
    unsigned int   have = 0;
    char          *s, *s2;

    dd("entering");
    /*
     Note: WebKit and IE Accept headers are hopelessly broken, we are
     looking for a user agent that accepts application/json regardless
     of ordering or weight, otherwise we will serve text/plain.
     */
    header_value = get_header_value(r, "Accept");
    if (header_value) {
       s = strtok((char *)header_value, ",");
       if (s) do {
          /* eliminate ;q= qualifier */
          for (s2 = s; *s2; s2++) {
             if (*s2==';') *s2='\0'; break;
          }
          if (!strcmp("application/json", s)) have = 1;
          s = strtok(NULL, ",");
       } while (s);
       free(header_value);
    }
    dd("returning %d", have);
    return have;
}
static u_char *
get_header_value(ngx_http_request_t *r, char *header_key)
{
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    u_char *retstr;
    u_int i;

    dd("entering");

    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (!strncmp(header_key, (char *) header[i].key.data, header[i].key.len)) {
           retstr = (u_char *) malloc(header[i].value.len);
           memcpy(retstr, header[i].value.data, header[i].value.len);
           retstr[header[i].value.len]='\0';
           dd("returning %s", retstr);
           return retstr;
        }
    }
    dd("returning NULL");
    return NULL;
}
static void
ngx_http_dbrelay_request_body_handler(ngx_http_request_t *r)
{
    ngx_int_t                 rc;

    dd("entering");

    rc = ngx_http_dbrelay_send_response(r);
    dd("returning");
}

/*
 * Non-upstream version of handler.  
 * 
 * This method blocks while querying the database.
 */
static ngx_int_t
ngx_http_dbrelay_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_log_t                 *log;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_dbrelay_loc_conf_t  *vlcf;
    u_char *header_value;

    dd("entering");

    log = r->connection->log;

    vlcf = ngx_http_get_module_loc_conf(r, ngx_http_dbrelay_module);

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
        ngx_log_error(NGX_LOG_WARN, log, 0, "unsupported method, returning not allowed");
        dd("returning NGX_HTTP_NOT_ALLOWED");
        return NGX_HTTP_NOT_ALLOWED;
    }

    if (r->uri.data[r->uri.len - 1] == '/') {
        dd("returning NGX_DECLINED");
        return NGX_DECLINED;
    }

    /* TODO: Win32 */
    if (r->zero_in_uri) {
        dd("returning NGX_DECLINED");
        return NGX_DECLINED;
    }

    r->root_tested = 1;

    r->request_body_in_file_only = 1;
    r->request_body_in_persistent_file = 1;
    r->request_body_in_clean_file = 1;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    dd("getting Origin header");

    header_value = get_header_value(r, "Origin");
    if (header_value) {
       ngx_log_error(NGX_LOG_DEBUG, log, 0, "Origin: \"%s\"", header_value);
       free(header_value);
    }
    dd("checking for Origin match");
    if (vlcf->origin.len && !origin_matches(r, vlcf->origin)) {
       dd("returning NGX_HTTP_FORBIDDEN");
       return NGX_HTTP_FORBIDDEN;
    }
    dd("checked");

    dd("reading request_body");
    rc = ngx_http_read_client_request_body(r, ngx_http_dbrelay_request_body_handler);

    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        dd("failed to read client request body");
        dd("returning %ld", rc);
        return rc;
    }

    dd("returning NGX_DONE");
    return NGX_DONE;
}

static ngx_int_t
ngx_http_dbrelay_send_response(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_log_t                 *log;
    ngx_buf_t                 *b;
    ngx_chain_t                out;
    u_char *json_output;
    u_char *header_value;
    dbrelay_request_t *request;
    size_t len;
    int cplength;
    ngx_http_dbrelay_loc_conf_t  *vlcf;

    dd("entering");

    vlcf = ngx_http_get_module_loc_conf(r, ngx_http_dbrelay_module);


    log = r->connection->log;

    request = dbrelay_alloc_request();
    request->log = log;
    request->log_level = 0;
    request->nginx_request = (void *) r;

    dd("parsing query string");
    /* is GET method? */
    if (r->method==NGX_HTTP_GET || r->method==NGX_HTTP_HEAD) { //r->args.len>0) {
        dd("GET method");
        dd("args length = %d", (int) r->args.len);
	parse_get_query_string(r->args, request);
    } else
    /* is POST method? */
    if (r->request_body->temp_file && r->request_body->temp_file->file.fd!=NGX_INVALID_FILE) {
        dd("POST method (from temp file)");
	parse_post_query_file(r->request_body->temp_file, request);
    } else if (r->request_body->buf && r->request_body->buf->pos!=NULL) {
        dd("POST method");
	parse_post_query_string(r->request_body->bufs, request);
    } 
    /* FIX ME - need to check to see if we have everything and error if not */

    if (!request->http_keepalive) {
       r->keepalive = 0;
    }

    dd("sql_server = %s", request->sql_server);
    if (request->sql) dd("sql = %s", request->sql_server);
	    
    log->action = "sending response to client";

    cplength = r->connection->addr_text.len > DBRELAY_OBJ_SZ - 1 ? DBRELAY_OBJ_SZ - 1 : r->connection->addr_text.len;
    strncpy(request->remote_addr, (char *) r->connection->addr_text.data, cplength);
    request->remote_addr[cplength] = '\0';
    dd("remote_addr = %s", request->remote_addr);

    if (strlen(request->cmd)) json_output = (u_char *) dbrelay_db_cmd(request);
    else if (request->status) json_output = (u_char *) dbrelay_db_status(request);
    else json_output = (u_char *) dbrelay_db_run_query(request);
    dbrelay_free_request(request);

    /* we need to allocate all before the header would be sent */
    len = ngx_strlen(json_output);
    b = ngx_create_temp_buf(r->pool, len + 1);
    //b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        dd("returning NGX_HTTP_INTERNAL_SERVER_ERROR");
	return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->last = ngx_cpymem(b->last, json_output, len);
    free(json_output);

    b->last_buf = 1;


    dd("getting Accept header");
    header_value = get_header_value(r, "Accept");
    if (header_value) {
       dd("Accept header = %s", header_value);
       free(header_value);
    }

    if (accepts_application_json(r)) {
       r->headers_out.content_type.len = sizeof("application/json") - 1;
       r->headers_out.content_type.data = (u_char *) "application/json";
    } else {
       r->headers_out.content_type.len = sizeof("text/plain") - 1;
       r->headers_out.content_type.data = (u_char *) "text/plain";
    }
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = len;
    r->headers_out.last_modified_time = 23349600;
    r->allow_ranges = 1;

    if (r != r->main && len == 0) {
        rc = ngx_http_send_header(r);
    }

    dd("sending headers");
    rc = ngx_http_send_header(r);
    dd("calling output filters");
    rc = ngx_http_output_filter(r, &out);
    dd("finalizing request");
    ngx_http_finalize_request(r, rc);
    dd("returning %ld", rc);
    return rc;
}

static char *
ngx_http_dbrelay_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    dd("entering");

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_dbrelay_handler;

    dd("returning NGX_CONF_OK");
    return NGX_CONF_OK;
}

static void *
ngx_http_dbrelay_create_loc_conf(ngx_conf_t *cf)
{
    //ngx_str_t default_origin = ngx_string("*");
    ngx_http_dbrelay_loc_conf_t  *conf;

    dd("entering");

#if HAVE_FREETDS
    dbinit();
#endif

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_dbrelay_loc_conf_t));
    if (conf == NULL) {
    	dd("returning NGX_CONF_ERROR");
        return NGX_CONF_ERROR;
    }
    dd("returning");
    return conf;
}
static void 
write_value(dbrelay_request_t *request, char *key, char *value)
{
   u_char *dst, *src;
   unsigned int i;
   unsigned char noprint=0;
   char *log_levels[] = { "debug", "informational", "notice", "warning", "error", "critical" };
   char *log_level_scopes[] = { "server", "connection", "query" };

   dd("entering");

   /* simple unescape of '+', ngx_unescape_uri doesn't do this for us */
   for (i=0;i<strlen(value);i++) {
      if (value[i]=='+') value[i]=' ';
   }

   dst = (u_char *) value; src = (u_char *) value;
   ngx_unescape_uri(&dst, &src, strlen(value), 0);
   *dst = '\0';

   if (!strcmp(key, "cmd")) {
      dbrelay_copy_string(request->cmd, value, DBRELAY_OBJ_SZ);
   } else if (!strcmp(key, "status")) {
      request->status = 1;
   } else if (!strcmp(key, "sql_dbtype")) {
      dbrelay_copy_string(request->sql_dbtype, value, DBRELAY_OBJ_SZ);
   } else if (!strcmp(key, "sql_database")) {
      dbrelay_copy_string(request->sql_database, value, DBRELAY_OBJ_SZ);
   } else if (!strcmp(key, "sql_server")) {
      dbrelay_copy_string(request->sql_server, value, DBRELAY_NAME_SZ);
   } else if (!strcmp(key, "sql_user")) {
      dbrelay_copy_string(request->sql_user, value, DBRELAY_OBJ_SZ);
   } else if (!strcmp(key, "sql_port")) {
      dbrelay_copy_string(request->sql_port, value, 6);
   } else if (!strcmp(key, "sql")) {
      request->sql = strdup(value);
   } else if (!strcmp(key, "query_tag")) {
      dbrelay_copy_string(request->query_tag, value, DBRELAY_NAME_SZ);
   } else if (!strcmp(key, "sql_password")) {
      dbrelay_copy_string(request->sql_password, value, DBRELAY_OBJ_SZ);
      noprint = 1;
   } else if (!strcmp(key, "connection_name")) {
      dbrelay_copy_string(request->connection_name, value, DBRELAY_NAME_SZ);
   } else if (!strcmp(key, "connection_timeout")) {
      request->connection_timeout = atol(value);
   } else if (!strcmp(key, "http_keepalive")) {
      request->http_keepalive = atoi(value);
   } else if (!strcmp(key, "log_level")) {
      for (i=0; i<sizeof(log_levels)/sizeof(char *); i++)
         if (!strcmp(value,log_levels[i])) request->log_level = i;
   } else if (!strcmp(key, "log_level_scope")) {
      for (i=0; i<sizeof(log_level_scopes)/sizeof(char *); i++)
         if (!strcmp(value,log_level_scopes[i])) request->log_level_scope = i;
   } else if (!strncmp(key, "param", 5)) {
      i = atoi(&key[5]);
      if (i>DBRELAY_MAX_PARAMS) {
         dbrelay_log_error(request, "param%d exceeds DBRELAY_MAX_PARAMS", i);
      } else {
         request->params[i] = strdup(value);
      }
   } else if (!strcmp(key, "flags")) {
      write_flag_values(request, value);
   } else if (!strcmp(key, "js_callback")) {
      dbrelay_copy_string(request->js_callback, value, DBRELAY_NAME_SZ);
   } else if (!strcmp(key, "js_error")) {
      dbrelay_copy_string(request->js_error, value, DBRELAY_NAME_SZ);
   } else if (!strcmp(key, "out")) {
      dbrelay_copy_string(request->output_style, value, DBRELAY_NAME_SZ);
      if (!strcmp(value, "json-dict")) {
         request->emitapi = &dbrelay_jsondict_api;
      } else if (!strcmp(value, "json-arr")) {
         request->emitapi = &dbrelay_jsonarr_api;
      } else if (!strcmp(value, "csv")) {
         request->emitapi = &dbrelay_csv_api;
      }
   }
   
   if (!noprint) {
      dbrelay_log_debug(request, "key %s", key);
      dbrelay_log_debug(request, "value %s", value);
   }
   dd("returning");
}
static void 
write_flag_values(dbrelay_request_t *request, char *value)
{
   char *flags = strdup(value);
   char *tok;

   dd("entering");

   while ((tok = strsep(&flags, ","))) {
      if (!strcmp(tok, "echosql")) request->flags|=DBRELAY_FLAG_ECHOSQL; 
      else if (!strcmp(tok, "pp")) request->flags|=DBRELAY_FLAG_PP; 
      else if (!strcmp(tok, "xact")) request->flags|=DBRELAY_FLAG_XACT; 
      else if (!strcmp(tok, "embedcsv")) request->flags|=DBRELAY_FLAG_EMBEDCSV; 
      else if (!strcmp(tok, "nomagic")) request->flags|=DBRELAY_FLAG_NOMAGIC; 
      else if (!strcmp(tok, "noempty")) request->flags|=DBRELAY_FLAG_NOEMPTY; 
   }
   free(flags);
   dd("returning");
}
void parse_post_query_file(ngx_temp_file_t *temp_file, dbrelay_request_t *request)
{
   ssize_t bytes_read;
   u_char *buf;
   char key[100];
   char *value;
   char *s, *k = key, *v;
   int target = 0;
   unsigned long bufsz = 0;
   int chop = 0;
   struct stat statbuf;

   dd("entering");

   fstat(temp_file->file.fd, &statbuf);
   buf = (u_char *) malloc(statbuf.st_size);

   lseek(temp_file->file.fd, 0, SEEK_SET);
   while ((bytes_read = read(temp_file->file.fd, &buf[bufsz], statbuf.st_size-bufsz))>0 && bufsz<=(unsigned long)statbuf.st_size) 
   {
      bufsz += bytes_read;
   }
   
   value = (char *) malloc(bufsz);
   v = value;
   dd("post data = %lu bytes", bufsz);

   for (s= (char *)buf; s !=  (char *)&buf[bufsz]; s++)
   { 
	      if (*s=='&') {
		  *k='\0';
		  *v='\0';
		  write_value(request, key, value);
		  target=0;
		  k=key;
	      } else if (*s=='=') {
		  target=1;
		  v=value;
	      } else if (target==0) {
		  *k++=*s;
	      } else {
		  *v++=*s;
	      }
   }
   *k='\0';
   while (v>=value && (*v=='\n' || *v=='\r')) {
     *v--='\0';
     chop = 1;
   }
   if (!chop) *v='\0';
   write_value(request, key, value);
   free(value);
   free(buf);
   dd("returning");
}
void parse_post_query_string(ngx_chain_t *bufs, dbrelay_request_t *request)
{
   char key[100];
   char *value;
   char *s, *k = key, *v;
   int target = 0;
   ngx_buf_t *buf;
   ngx_chain_t *chain;
   unsigned long bufsz = 0;
   int chop = 0;

   dd("entering");

   for (chain = bufs; chain!=NULL; chain = chain->next) 
   {
      buf = chain->buf;
      bufsz += (buf->last - buf->pos) + 1;
   }
   value = (char *) malloc(bufsz);
   v = value;
   dd("post data = %lu bytes", bufsz);

   for (chain = bufs; chain!=NULL; chain = chain->next) 
   {
      buf = chain->buf;
      for (s= (char *)buf->pos; s !=  (char *)buf->last; s++)
      { 
	      if (*s=='&') {
		  *k='\0';
		  *v='\0';
		  write_value(request, key, value);
		  target=0;
		  k=key;
	      } else if (*s=='=') {
		  target=1;
		  v=value;
	      } else if (target==0) {
		  *k++=*s;
	      } else {
		  *v++=*s;
	      }
      }
   }
   *k='\0';
   while (v>=value && (*v=='\n' || *v=='\r')) {
     *v--='\0';
     chop = 1;
   }
   if (!chop) *v='\0';
   write_value(request, key, value);
   free(value);
   dd("returning");
}
void parse_get_query_string(ngx_str_t args, dbrelay_request_t *request)
{
   char key[100];
   char *value;
   char *s, *k, *v;
   int target = 0;

   dd("entering");
   if (args.len==0) {
      dd("no GET data");
      dd("returning");
      return;
   }

   value = malloc(args.len);
   k = key;
   v = value;

   for (s=(char *)args.data; *s && s < (((char *)args.data) + args.len); s++)
   { 
      if (*s=='&') {
         *k='\0';
	 *v='\0';
	 write_value(request, key, value);
         target=0;
         k=key;
      } else if (*s=='=') {
         target=1;
         v=value;
         *k='\0';
      } else if (target==0) {
         *k++=*s;
      } else {
	 *v++=*s;
      }
   }
   *k='\0';
   while (v>=value && (*v=='\n' || *v=='\r')) *v--='\0';
   *v='\0';
   write_value(request, key, value);
   free(value);
   dd("returning");
}

