/*
 * Copyright (C) Getco LLC
 */

#ifndef _DBRELAYDB_H_INCLUDED_
#define _DBRELAYDB_H_INCLUDED_

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
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

typedef struct {
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

} dbrelay_dbapi_t;

u_char *dbrelay_db_run_query(dbrelay_request_t *request);
u_char *dbrelay_db_status(dbrelay_request_t *request);
void dbrelay_db_close_connection(dbrelay_connection_t *conn, dbrelay_request_t *request);


void dbrelay_log_debug(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_info(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_notice(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_warn(dbrelay_request_t *request, const char *fmt, ...);
void dbrelay_log_error(dbrelay_request_t *request, const char *fmt, ...);

dbrelay_request_t *dbrelay_alloc_request();
void dbrelay_free_request(dbrelay_request_t *request);

/* shmem.c */
void dbrelay_create_shmem();
dbrelay_connection_t *dbrelay_get_shmem();
void dbrelay_release_shmem(dbrelay_connection_t *connections);
void dbrelay_destroy_shmem();
key_t dbrelay_get_ipc_key();

/* connection.c */
char *dbrelay_conn_send_request(int s, dbrelay_request_t *request, int *error);
void dbrelay_conn_set_option(int s, char *option, char *value);
pid_t dbrelay_conn_launch_connector(char *sock_path);
u_char *dbrelay_exec_query(dbrelay_connection_t *conn, char *database, char *sql, unsigned long flags); 
void dbrelay_conn_kill(int s);
void dbrelay_conn_close(int s);

/* admin.c */
u_char *dbrelay_db_cmd(dbrelay_request_t *request);

/* socket.c */
int dbrelay_socket_connect(char *sock_path);
int dbrelay_socket_recv_string(int s, char *in_buf, int *in_ptr, char *out_buf);
void dbrelay_socket_send_string(int s, char *str);

#endif /* _DBRELAY_H_INCLUDED_ */
