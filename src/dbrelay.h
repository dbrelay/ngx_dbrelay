/*
 * DB Relay is an HTTP module built on the NGiNX webserver platform which 
 * communicates with a variety of database servers and returns JSON formatted 
 * data.
 * 
 * Copyright (C) 2008-2010 Getco LLC
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

#ifndef _DBRELAYDB_H_INCLUDED_
#define _DBRELAYDB_H_INCLUDED_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include "../include/config.h"

#ifndef CMDLINE
#include <ngx_core.h>
#include <ngx_config.h>
#endif

#include "stringbuf.h"
#include "json.h"

#define DBRELAY_MAX_CONN 1000
#define DBRELAY_MAX_PARAMS 100
#define DBRELAY_OBJ_SZ 31
#define DBRELAY_NAME_SZ 101
#define DBRELAY_SOCKET_BUFSIZE 4096

#define DBRELAY_HARD_TIMEOUT 28800

#define DBRELAY_LOG_SCOPE_SERVER 1
#define DBRELAY_LOG_SCOPE_CONN 2
#define DBRELAY_LOG_SCOPE_QUERY 3

#ifndef CMDLINE
#define DBRELAY_LOG_LVL_DEBUG   NGX_LOG_DEBUG
#define DBRELAY_LOG_LVL_INFO    NGX_LOG_INFO
#define DBRELAY_LOG_LVL_NOTICE  NGX_LOG_NOTICE
#define DBRELAY_LOG_LVL_WARN    NGX_LOG_WARN
#define DBRELAY_LOG_LVL_ERROR   NGX_LOG_ERR
#define DBRELAY_LOG_LVL_CRIT    NGX_LOG_CRIT
#else
#define DBRELAY_LOG_LVL_DEBUG   1
#define DBRELAY_LOG_LVL_INFO    2
#define DBRELAY_LOG_LVL_NOTICE  3
#define DBRELAY_LOG_LVL_WARN    4
#define DBRELAY_LOG_LVL_ERROR   5
#define DBRELAY_LOG_LVL_CRIT    6
#endif

#define DBRELAY_FLAG_ECHOSQL 0x01
#define DBRELAY_FLAG_PP      0x02
#define DBRELAY_FLAG_XACT    0x04
#define DBRELAY_FLAG_EMBEDCSV    0x08
#define DBRELAY_FLAG_NOMAGIC    0x10

#define DBRELAY_DBCMD_TABLES    0
#define DBRELAY_DBCMD_COLUMNS   1
#define DBRELAY_DBCMD_PKEY      2
#define DBRELAY_DBCMD_BEGIN     3
#define DBRELAY_DBCMD_COMMIT    4
#define DBRELAY_DBCMD_ROLLBACK  5

#ifdef CMDLINE
   typedef struct ngx_log_s {} ngx_log_t;
   typedef unsigned char u_char;
#endif

#if HAVE_MSG_NOSIGNAL
#define NET_FLAGS MSG_NOSIGNAL
#else
#define NET_FLAGS 0
#endif

#define IS_SET(x) (x && strlen(x)>0)
#define IS_EMPTY(x) (!x || strlen(x)==0)


typedef struct dbrelay_dbapi_s dbrelay_dbapi_t;
typedef struct dbrelay_emitapi_s dbrelay_emitapi_t;

typedef struct {
   int status;
   char cmd[DBRELAY_NAME_SZ];
   char sql_server[DBRELAY_NAME_SZ];
   char sql_port[6];
   char sql_database[DBRELAY_OBJ_SZ];
   char sql_user[DBRELAY_OBJ_SZ];
   char sql_password[DBRELAY_OBJ_SZ];
   char *sql;
   char query_tag[DBRELAY_NAME_SZ];
   char connection_name[DBRELAY_NAME_SZ];
   long connection_timeout;
   int http_keepalive;
   int log_level;
   int log_level_scope;
   ngx_log_t *log;
   char error_message[4000];
   char *params[DBRELAY_MAX_PARAMS];
   char sql_dbtype[DBRELAY_OBJ_SZ];
   char remote_addr[DBRELAY_OBJ_SZ];
   char sock_path[256];  /* explicitly specify socket path */
   unsigned long flags;
   char js_callback[DBRELAY_NAME_SZ];
   char js_error[DBRELAY_NAME_SZ];
   void *nginx_request;
   char output_style[DBRELAY_NAME_SZ];
   dbrelay_dbapi_t *dbapi;
   dbrelay_emitapi_t *emitapi;
} dbrelay_request_t;

typedef struct {
   char sql_server[DBRELAY_NAME_SZ];
   char sql_port[6];
   char sql_database[DBRELAY_OBJ_SZ];
   char sql_user[DBRELAY_OBJ_SZ];
   char sql_password[DBRELAY_OBJ_SZ];
   char connection_name[DBRELAY_NAME_SZ];
   long connection_timeout;
   time_t tm_create;
   time_t tm_accessed;
   unsigned int in_use;
   unsigned int slot;
   pid_t pid;
   pid_t helper_pid;
   char sock_path[DBRELAY_NAME_SZ];
   void *db;
} dbrelay_connection_t;

typedef void (*dbrelay_db_init)(void);
typedef void *(*dbrelay_db_connect)(dbrelay_request_t *request);
typedef void (*dbrelay_db_close)(void *db);
typedef void (*dbrelay_db_assign_request)(void *db, dbrelay_request_t *request);
typedef int (*dbrelay_db_is_quoted)(void *db, int colnum);;
typedef int (*dbrelay_db_connected)(void *db);
typedef int (*dbrelay_db_change_db)(void *db, char *database);
typedef int (*dbrelay_db_exec)(void *db, char *sql);
typedef int (*dbrelay_db_rowcount)(void *db);
typedef int (*dbrelay_db_has_results)(void *db);
typedef int (*dbrelay_db_numcols)(void *db);
typedef char *(*dbrelay_db_colname)(void *db, int colnum);
typedef void (*dbrelay_db_coltype)(void *db, int colnum, char *dest);
typedef int (*dbrelay_db_collen)(void *db, int colnum);
typedef int (*dbrelay_db_colprec)(void *db, int colnum);
typedef int (*dbrelay_db_colscale)(void *db, int colnum);
typedef int (*dbrelay_db_fetch_row)(void *db);
typedef char *(*dbrelay_db_colvalue)(void *db, int colnum, char *dest);
typedef char *(*dbrelay_db_error)(void *db);
typedef char *(*dbrelay_db_catalogsql)(int dbcmd, char **params);
typedef int (*dbrelay_db_isalive)(void *db);

struct dbrelay_dbapi_s {
   dbrelay_db_init init;
   dbrelay_db_connect connect;
   dbrelay_db_close close;
   dbrelay_db_assign_request assign_request;
   dbrelay_db_is_quoted is_quoted;
   dbrelay_db_connected connected;
   dbrelay_db_change_db change_db;
   dbrelay_db_exec exec;
   dbrelay_db_rowcount rowcount;
   dbrelay_db_has_results has_results;
   dbrelay_db_numcols numcols;
   dbrelay_db_colname colname;
   dbrelay_db_coltype coltype;
   dbrelay_db_collen collen;
   dbrelay_db_colprec colprec;
   dbrelay_db_colscale colscale;
   dbrelay_db_fetch_row fetch_row;
   dbrelay_db_colvalue colvalue;
   dbrelay_db_error error;
   dbrelay_db_catalogsql catalogsql;
   dbrelay_db_isalive isalive;

};

typedef void *(*dbrelay_emit_init)(dbrelay_request_t *request);
typedef char *(*dbrelay_emit_finalize)(void *emitter, dbrelay_request_t *request);
typedef void (*dbrelay_emit_restart)(void *emitter, dbrelay_request_t *request);
typedef void (*dbrelay_emit_request)(void *emitter, dbrelay_request_t *request);
typedef void (*dbrelay_emit_log)(void *emitter, dbrelay_request_t *request, char *error_string);
typedef void (*dbrelay_emit_add_section)(void *emitter, char *ret);
typedef char *(*dbrelay_emit_fill)(dbrelay_connection_t *conn, unsigned long flags);


struct dbrelay_emitapi_s {
   dbrelay_emit_init init;
   dbrelay_emit_finalize finalize;
   dbrelay_emit_restart restart;
   dbrelay_emit_request request;
   dbrelay_emit_log log;
   dbrelay_emit_add_section add_section;
   dbrelay_emit_fill fill;
};

u_char *dbrelay_db_run_query(dbrelay_request_t *request);
u_char *dbrelay_db_status(dbrelay_request_t *request);
void dbrelay_db_close_connection(dbrelay_connection_t *conn, dbrelay_request_t *request);
void dbrelay_copy_string(char *dest, char *src, int sz);
char *dbrelay_resolve_params(dbrelay_request_t *request, char *sql);
int dbrelay_check_request(dbrelay_request_t *request);



void dbrelay_log_debug(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_info(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_notice(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_warn(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_error(dbrelay_request_t *request, const char *fmt, ...);

dbrelay_request_t *dbrelay_alloc_request();
void dbrelay_free_request(dbrelay_request_t *request);

/* ngx_dbrelay_module.c */
u_char *ngx_http_dbrelay_get_shm_addr();

/* shmem.c */
void dbrelay_create_shmem();
dbrelay_connection_t *dbrelay_get_shmem();
void dbrelay_release_shmem(dbrelay_connection_t *connections);
void dbrelay_destroy_shmem();
key_t dbrelay_get_ipc_key();

/* connection.c */
pid_t dbrelay_conn_initialize(int s, dbrelay_request_t *request);
char *dbrelay_conn_send_request(int s, dbrelay_request_t *request, int *error);
int dbrelay_conn_set_option(int s, char *option, char *value);
pid_t dbrelay_conn_launch_connector(char *sock_path, dbrelay_request_t *request);
u_char *dbrelay_exec_query(dbrelay_connection_t *conn, dbrelay_request_t *request, char *sql); 
void dbrelay_conn_kill(int s);
void dbrelay_conn_close(int s);
dbrelay_connection_t *dbrelay_time_get_shmem(dbrelay_request_t *request);
void dbrelay_time_release_shmem(dbrelay_request_t *request, dbrelay_connection_t *connections);

/* admin.c */
u_char *dbrelay_db_cmd(dbrelay_request_t *request);

/* socket.c */
int dbrelay_socket_connect(char *sock_path, int timeout, int *error);
int dbrelay_socket_recv_string(int s, char *in_buf, int *in_ptr, char *out_buf, int timeout);
int dbrelay_socket_send_string(int s, char *str);

#endif /* _DBRELAY_H_INCLUDED_ */
