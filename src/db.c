
/*
 * Copyright (C) Getco LLC
 */

#include "dbrelay.h"
#include "stringbuf.h"
#include "../include/dbrelay_config.h"

#ifdef HAVE_FREETDS
extern dbrelay_dbapi_t dbrelay_mssql_api;
dbrelay_dbapi_t *api = &dbrelay_mssql_api;
#endif

#ifdef HAVE_MYSQL
extern dbrelay_dbapi_t dbrelay_mysql_api;
dbrelay_dbapi_t *api = &dbrelay_mysql_api;
#endif

#ifdef HAVE_ODBC
extern dbrelay_dbapi_t dbrelay_odbc_api;
dbrelay_dbapi_t *api = &dbrelay_odbc_api;
#endif

#define IS_SET(x) (x && strlen(x)>0)
#define IS_EMPTY(x) (!x || strlen(x)==0)
#define TRUE 1
#define FALSE 0

static int dbrelay_db_fill_data(json_t *json, dbrelay_connection_t *conn);
static int dbrelay_db_get_connection(dbrelay_request_t *request);
static char *dbrelay_resolve_params(dbrelay_request_t *request, char *sql);
static int dbrelay_find_placeholder(char *sql);
static int dbrelay_check_request(dbrelay_request_t *request);
static void dbrelay_write_json_log(json_t *json, dbrelay_request_t *request, char *error_string);
void dbrelay_write_json_colinfo(json_t *json, void *db, int colnum, int *maxcolname);
void dbrelay_write_json_column(json_t *json, void *db, int colnum, int *maxcolname);
static void dbrelay_db_zero_connection(dbrelay_connection_t *conn, dbrelay_request_t *request);
static void dbrelay_write_json_column_csv(json_t *json, void *db, int colnum);
static void dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname);
static unsigned char dbrelay_is_unnamed_column(char *colname);
dbrelay_connection_t *dbrelay_time_get_shmem(dbrelay_request_t *request);
void dbrelay_time_release_shmem(dbrelay_request_t *request, dbrelay_connection_t *connections);
static int calc_time(struct timeval *start, struct timeval *now);

dbrelay_connection_t *dbrelay_time_get_shmem(dbrelay_request_t *request)
{
   struct timeval start;
   struct timeval now;
   dbrelay_connection_t *connections;

   gettimeofday(&start, NULL);
   connections = dbrelay_get_shmem();
   gettimeofday(&now, NULL);
   dbrelay_log_debug(request, "shmem attach time %d", calc_time(&start, &now));
   return connections;
}
void dbrelay_time_release_shmem(dbrelay_request_t *request, dbrelay_connection_t *connections)
{
   struct timeval start;
   struct timeval now;

   gettimeofday(&start, NULL);
   dbrelay_release_shmem(connections);
   gettimeofday(&now, NULL);
   dbrelay_log_debug(request, "shmem release time %d", calc_time(&start, &now));
}
static unsigned char dbrelay_is_unnamed_column(char *colname)
{
   /* For queries such as 'select 1'
    * SQL Server uses a blank column name
    * PostgreSQL and Vertica use "?column?"
    */
   if (!IS_SET(colname) || !strcmp(colname, "?column?")) 
      return 1;
   else
      return 0;
}
static void *dbrelay_db_open_connection(dbrelay_request_t *request)
{
   api->init();
   return api->connect(request);
}
static void dbrelay_db_populate_connection(dbrelay_request_t *request, dbrelay_connection_t *conn, void *db)
{
   memset(conn, '\0', sizeof(dbrelay_connection_t));


   /* copy parameters necessary to do connection hash match */
   if (IS_SET(request->sql_server)) 
      strcpy(conn->sql_server, request->sql_server);
   if (IS_SET(request->sql_port)) 
      strcpy(conn->sql_port, request->sql_port);
   if (IS_SET(request->sql_user))
      strcpy(conn->sql_user, request->sql_user);
   if (IS_SET(request->sql_password))
      strcpy(conn->sql_password, request->sql_password);
   if (IS_SET(request->sql_database))
      strcpy(conn->sql_database, request->sql_database);
   if (IS_SET(request->connection_name))
      strcpy(conn->connection_name, request->connection_name);

   if (!request->connection_timeout) request->connection_timeout = 60;
   conn->connection_timeout = request->connection_timeout;

   //dbrelay_log_debug(request, "prefix %s", DBRELAY_PREFIX);
   if (IS_SET(request->connection_name)) {
      if (IS_SET(request->sock_path)) {
         strcpy(conn->sock_path, request->sock_path);
      } else {
         tmpnam(conn->sock_path);
         conn->helper_pid = dbrelay_conn_launch_connector(conn->sock_path);
      }
      dbrelay_log_info(request, "socket name %s", conn->sock_path);
      conn->tm_create = time(NULL);
      conn->tm_accessed = time(NULL);
      conn->in_use++;
      conn->pid = getpid();

      return;
   }

   conn->db = db;
      
   conn->tm_create = time(NULL);
   conn->tm_accessed = time(NULL);
   conn->in_use++;

   conn->pid = getpid();
}
static int dbrelay_db_alloc_connection(dbrelay_request_t *request)
{
   int i, slot = -1;
   void *dbconn = NULL;
   dbrelay_connection_t *connections;

   /* connect to database outside holding shared mem */
   if (!IS_SET(request->connection_name)) {
      dbconn = dbrelay_db_open_connection(request);
   }

   connections = dbrelay_time_get_shmem(request);

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     if (connections[i].pid==0) {
	slot = i;
        break;
     }
   }

   /* we have exhausted the pool, log something sensible and return error */
   if (slot==-1) {
      dbrelay_log_error(request, "No free connections available!");
      dbrelay_time_release_shmem(request, connections);
      if (dbconn) api->close(dbconn);
      return -1;
   }

   dbrelay_db_populate_connection(request, &connections[slot], dbconn);
   dbrelay_log_debug(request, "allocating slot %d to request", slot);
   connections[slot].slot = slot;
   dbrelay_time_release_shmem(request, connections);
   return slot;
}
static unsigned int match(char *s1, char *s2)
{
   if (IS_EMPTY(s1) && IS_EMPTY(s2)) return TRUE;
   if (s1==NULL || s2==NULL) return FALSE;
   if (!strcmp(s1, s2)) return TRUE;
   return FALSE;
}
static unsigned int dbrelay_db_match(dbrelay_connection_t *conn, dbrelay_request_t *request)
{
   dbrelay_log_debug(request, "comparing %s and %s", conn->connection_name, request->connection_name);
   //if (conn->pid != getpid()) return FALSE;
   if (match(conn->sql_server, request->sql_server) &&
       match(conn->sql_port, request->sql_port) &&
       match(conn->sql_database, request->sql_database) &&
       match(conn->sql_user, request->sql_user) &&
       match(conn->sql_password, request->sql_password) &&
       match(conn->connection_name, request->connection_name))
         return TRUE;
   else return FALSE;
}
static unsigned int dbrelay_db_find_connection(dbrelay_request_t *request)
{
   dbrelay_connection_t *conn;
   int i;

   dbrelay_log_debug(request, "find_connection called");
   dbrelay_connection_t *connections;
   connections = dbrelay_time_get_shmem(request);
   for (i=0; i<DBRELAY_MAX_CONN; i++) {
      conn = &connections[i];
      if (conn->pid!=0) {
         if (dbrelay_db_match(conn, request)) {
            dbrelay_log_info(request, "found connection match for request at slot %d", i);
            conn->in_use++;
            conn->tm_accessed = time(NULL);
            //api->assign_request(conn->db, request);
            dbrelay_time_release_shmem(request, connections);
            return i;
         }
      }
   }
   dbrelay_time_release_shmem(request, connections);
   return -1;
}
void dbrelay_db_close_connection(dbrelay_connection_t *conn, dbrelay_request_t *request)
{
   if (!conn) {
      dbrelay_log_warn(request, "attempt to close null connection ");
      return;
   }

   dbrelay_log_info(request, "closing connection %d", conn->slot);

   if (conn->db) api->close(conn->db);
   dbrelay_db_zero_connection(conn, request);
}
static void dbrelay_db_zero_connection(dbrelay_connection_t *conn, dbrelay_request_t *request)
{
   conn->pid=0;
   conn->sql_server[0]='\0';
   conn->sql_user[0]='\0';
   conn->sql_database[0]='\0';
   conn->sql_port[0]='\0';
   conn->sql_password[0]='\0';
   conn->connection_name[0]='\0';
   conn->in_use = 0;
}
static void dbrelay_db_close_connections(dbrelay_request_t *request)
{
   dbrelay_connection_t *conn;
   dbrelay_connection_t *connections;
   int i;
   time_t now;

   now = time(NULL);
   connections = dbrelay_time_get_shmem(request);
   for (i=0; i<DBRELAY_MAX_CONN; i++) {
      conn = &connections[i];
      if (conn->pid && !IS_SET(conn->connection_name) && kill(conn->pid, 0)) {
         dbrelay_log_notice(request, "dead worker %u holding connection slot %d, cleaning up.", conn->pid, conn->slot);
         dbrelay_db_zero_connection(conn, request);
      }

      if (!conn->pid) continue;

      if (conn->tm_accessed + DBRELAY_HARD_TIMEOUT < now) {
         dbrelay_log_notice(request, "hard timing out conection %u", conn->slot);
         dbrelay_db_close_connection(conn, request);
         continue;
      }

      if (conn->in_use) continue;

      if (conn->tm_accessed + conn->connection_timeout < now) {
         dbrelay_log_notice(request, "timing out conection %u", conn->slot);
         dbrelay_db_close_connection(conn, request);
      }
   }
   dbrelay_time_release_shmem(request, connections);
}
static void dbrelay_db_free_connection(dbrelay_connection_t *conn, dbrelay_request_t *request)
{
   conn->in_use--;
   
   if (IS_EMPTY(conn->connection_name)) {
      if (conn->db) api->assign_request(conn->db, NULL);
      dbrelay_db_close_connection(conn, request);
   }
}
static int dbrelay_db_get_connection(dbrelay_request_t *request)
{
   //dbrelay_connection_t *conn;
   int slot = -1;

   /* if there is no connection name, allocate a new connection */
   if (IS_EMPTY(request->connection_name)) {
      dbrelay_log_debug(request, "empty connection name, allocating new");
      slot = dbrelay_db_alloc_connection(request);

   /* look for an matching idle connection */
   } else if ((slot = dbrelay_db_find_connection(request))==-1) {
      /* else we need to allocate a new connection */
      slot = dbrelay_db_alloc_connection(request);
      dbrelay_log_debug(request, "no match allocating slot %d", slot);
   }

   dbrelay_db_close_connections(request);

   return slot;
}

u_char *dbrelay_db_status(dbrelay_request_t *request)
{
   dbrelay_connection_t *connections;
   dbrelay_connection_t *conn;
   json_t *json = json_new();
   int i;
   char tmpstr[100];
   u_char *json_output;
   struct tm *ts;


   json_new_object(json);
   json_add_key(json, "status");
   json_new_object(json);

   json_add_key(json, "info");
   json_new_object(json);
   json_add_string(json, "build", DBRELAY_BUILD);
   sprintf(tmpstr, "0x%08x", dbrelay_get_ipc_key());
   json_add_string(json, "ipckey", tmpstr);
   json_end_object(json);

   json_add_key(json, "connections");
   json_new_array(json);

   connections = dbrelay_time_get_shmem(request);

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     conn = &connections[i];
     if (connections[i].pid!=0) {
        json_new_object(json);
        sprintf(tmpstr, "%u", conn->slot);
        json_add_number(json, "slot", tmpstr);
        sprintf(tmpstr, "%u", conn->pid);
        json_add_number(json, "pid", tmpstr);
        json_add_string(json, "name", conn->connection_name ? conn->connection_name : "");
        ts = localtime(&conn->tm_create);
        strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M:%S", ts);
        json_add_string(json, "tm_created", tmpstr);
        ts = localtime(&conn->tm_accessed);
        strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M:%S", ts);
        json_add_string(json, "tm_accessed", tmpstr);
        json_add_string(json, "sql_server", conn->sql_server ? conn->sql_server : "");
        json_add_string(json, "sql_port", conn->sql_port ? conn->sql_port : "");
        json_add_string(json, "sql_database", conn->sql_database ? conn->sql_database : "");
        json_add_string(json, "sql_user", conn->sql_user ? conn->sql_user : "");
        sprintf(tmpstr, "%ld", conn->connection_timeout);
        json_add_number(json, "connection_timeout", tmpstr);
        sprintf(tmpstr, "%u", conn->in_use);
        json_add_number(json, "in_use", tmpstr);
        json_add_string(json, "sock_path", conn->sock_path);
        sprintf(tmpstr, "%u", conn->helper_pid);
        json_add_number(json, "helper_pid", tmpstr);
        json_end_object(json);
     }
   }

   dbrelay_time_release_shmem(request, connections);

   json_end_array(json);
   json_end_object(json);
   json_end_object(json);

   json_output = (u_char *) json_to_string(json);
   json_free(json);

   return json_output;
}
/*
 * echo request parameters in json output
 */
static void dbrelay_append_request_json(json_t *json, dbrelay_request_t *request)
{
   json_add_key(json, "request");
   json_new_object(json);

   if (IS_SET(request->query_tag)) 
      json_add_string(json, "query_tag", request->query_tag);
   json_add_string(json, "sql_server", request->sql_server);
   json_add_string(json, "sql_user", request->sql_user);

   if (IS_SET(request->sql_port)) 
      json_add_string(json, "sql_port", request->sql_port);

   json_add_string(json, "sql_database", request->sql_database);

/*
 * do not return password back to client
   if (IS_SET(request->sql_password)) 
      json_add_string(json, "sql_password", request->sql_password);
*/

   json_end_object(json);

}
static void dbrelay_append_log_json(json_t *json, dbrelay_request_t *request, char *error_string)
{
   int i;
   char tmp[20];

   json_add_key(json, "log");
   json_new_object(json);
   if (request->flags & DBRELAY_FLAG_ECHOSQL) json_add_string(json, "sql", request->sql);
   if (strlen(error_string)) {
      json_add_string(json, "error", error_string);
   }
   i = 0;
   while (request->params[i]) {
      sprintf(tmp, "param%d", i);
      json_add_string(json, tmp, request->params[i]);
      i++;
   }
   json_end_object(json);

   json_end_object(json);
}
static dbrelay_connection_t *dbrelay_wait_for_connection(dbrelay_request_t *request, int *s)
{
   int slot = 0;
   dbrelay_connection_t *conn;
   dbrelay_connection_t *connections;

   do {
      dbrelay_log_debug(request, "calling get_connection");
      slot = dbrelay_db_get_connection(request);
      dbrelay_log_debug(request, "using slot %d", slot);
      if (slot==-1) {
         dbrelay_log_warn(request, "Couldn't allocate new connection");
         return NULL;
      }

      connections = dbrelay_time_get_shmem(request);
      conn = (dbrelay_connection_t *) malloc(sizeof(dbrelay_connection_t));
      memcpy(conn, &connections[slot], sizeof(dbrelay_connection_t));
      conn->slot = slot;
      dbrelay_time_release_shmem(request, connections);

      if (IS_SET(request->connection_name)) {
         dbrelay_log_info(request, "connecting to connection helper");
         dbrelay_log_info(request, "socket address %s", conn->sock_path);
         *s = dbrelay_socket_connect(conn->sock_path);
         // if connect fails, remove connector from list
         if (*s==-1) {
            unlink(conn->sock_path);
            free(conn);
            connections = dbrelay_time_get_shmem(request);
            connections[slot].pid=0;
            dbrelay_time_release_shmem(request, connections);
         }
      }
  } while (*s==-1);

  return conn;
}
void dbrelay_db_restart_json(dbrelay_request_t *request, json_t **json)
{
   if (IS_SET(request->js_error)) {
      // free json handle and start over
      json_free(*json);
      *json = json_new();
      json_add_callback(*json, request->js_error);
      json_new_object(*json);
      dbrelay_append_request_json(*json, request);
   }
}
u_char *dbrelay_db_run_query(dbrelay_request_t *request)
{
   /* FIX ME */
   char error_string[500];
   json_t *json = json_new();
   u_char *ret;
   dbrelay_connection_t *conn;
   dbrelay_connection_t *connections;
   int s = 0;
   int slot = -1;
   char *newsql;
   int have_error = 0;

   error_string[0]='\0';

   dbrelay_log_info(request, "run_query called");

   if (request->flags & DBRELAY_FLAG_PP) json_pretty_print(json, 1);


   if (IS_SET(request->js_callback)) {
      json_add_callback(json, request->js_callback);
   }

   json_new_object(json);

   dbrelay_append_request_json(json, request);

   if (!dbrelay_check_request(request)) {
	dbrelay_db_restart_json(request, &json);
        dbrelay_log_info(request, "check_request failed.");
        dbrelay_write_json_log(json, request, "Not all required parameters submitted.");

	if (IS_SET(request->js_callback) || IS_SET(request->js_error)) {
           json_end_callback(json);
	}

        ret = (u_char *) json_to_string(json);
        json_free(json);
        return ret;
   }

   newsql = dbrelay_resolve_params(request, request->sql);

   conn = dbrelay_wait_for_connection(request, &s);
   if (conn == NULL) {
      dbrelay_db_restart_json(request, &json);
      dbrelay_write_json_log(json, request, "Couldn't allocate new connection");
      if (IS_SET(request->js_callback) || IS_SET(request->js_error))
           json_end_callback(json);

      ret = (u_char *) json_to_string(json);
      json_free(json);
      return ret;
   }
   slot = conn->slot;

   dbrelay_log_debug(request, "Allocated connection for query");

   error_string[0]='\0';

   if (IS_SET(request->connection_name)) 
   {
      dbrelay_log_info(request, "sending request");
      ret = (u_char *) dbrelay_conn_send_request(s, request, &have_error);
      dbrelay_log_debug(request, "back");
      // internal error
      if (have_error==2) {
         dbrelay_log_error(request, "Error occurred on socket %s (PID: %u)", conn->sock_path, conn->helper_pid);
      }
      if (have_error) {
         dbrelay_db_restart_json(request, &json);
         dbrelay_log_debug(request, "have error %s\n", ret);
         strcpy(error_string, (char *) ret);
      } else if (!IS_SET((char *)ret)) {
         dbrelay_log_warn(request, "Connector returned no information");
         dbrelay_log_info(request, "Query was: %s", newsql);
      } else {
         json_add_json(json, ", ");
         json_add_json(json, (char *) ret);
         free(ret);
      }
      dbrelay_log_debug(request, "closing");
      dbrelay_conn_close(s);
      dbrelay_log_debug(request, "after close");
   } else {
      if (!api->connected(conn->db)) {
	//strcpy(error_string, "Failed to login");
        //if (login_msgno == 18452 && IS_EMPTY(request->sql_password)) {
        if (IS_EMPTY(request->sql_password)) {
	    strcpy(error_string, "Login failed and no password was set, please check.\n");
	    strcat(error_string, api->error(conn->db));
        } else if (!strlen(api->error(conn->db))) {
	    strcpy(error_string, "Connection failed.\n");
        } else {
	    strcpy(error_string, api->error(conn->db));
        }
        dbrelay_db_restart_json(request, &json);
      } else {
   	dbrelay_log_debug(request, "Sending sql query");
        ret = dbrelay_exec_query(conn, request->sql_database, newsql, request->flags);
        if (ret==NULL) {
           dbrelay_db_restart_json(request, &json);
   	   dbrelay_log_debug(request, "error");
           //strcpy(error_string, request->error_message);
           strcpy(error_string, api->error(conn->db));
        } else {
           json_add_json(json, ", ");
           json_add_json(json, (char *) ret);
           free(ret);
        }
   	dbrelay_log_debug(request, "Done filling JSON output");
      }
   } // !named connection
   free(conn);

   free(newsql);

   dbrelay_log_debug(request, "error = %s\n", error_string);
   dbrelay_append_log_json(json, request, error_string);

   if (IS_SET(request->js_callback) || IS_SET(request->js_error)) {
      json_end_callback(json);
   }

   ret = (u_char *) json_to_string(json);
   json_free(json);
   dbrelay_log_debug(request, "Query completed, freeing connection.");

   connections = dbrelay_time_get_shmem(request);
   /* set time accessed at end of processing so that long queries do not
    * become eligible for being timed out immediately.  
    */
   conn = &connections[slot];
   conn->tm_accessed = time(NULL);
   dbrelay_db_free_connection(conn, request);
   dbrelay_time_release_shmem(request, connections);

   return ret;
}

u_char *
dbrelay_exec_query(dbrelay_connection_t *conn, char *database, char *sql, unsigned long flags)
{
  json_t *json = json_new();
  u_char *ret;
 
  if (flags & DBRELAY_FLAG_PP) json_pretty_print(json, 1);
  if (flags & DBRELAY_FLAG_EMBEDCSV) json_set_mode(json, DBRELAY_JSON_MODE_CSV);

  api->change_db(conn->db, database);

  if (flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_BEGIN, NULL));

  if (api->exec(conn->db, sql))
  {
     dbrelay_db_fill_data(json, conn);
     if (flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_COMMIT, NULL));
  } else {
     if (flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_ROLLBACK, NULL));
     return NULL;
  }
  ret = (u_char *) json_to_string(json);
  json_free(json);

  return ret;
}
int dbrelay_db_fill_data(json_t *json, dbrelay_connection_t *conn)
{
   int numcols, colnum;
   char tmp[256];
   int maxcolname;

   json_add_key(json, "data");
   json_new_array(json);
   while (api->has_results(conn->db)) 
   {
        maxcolname = 0;
	json_new_object(json);
	json_add_key(json, "fields");
	json_new_array(json);

	numcols = api->numcols(conn->db);
	for (colnum=1; colnum<=numcols; colnum++) {
            dbrelay_write_json_colinfo(json, conn->db, colnum, &maxcolname);
        }
	json_end_array(json);
	json_add_key(json, "rows");

	if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_new_array(json);
        else json_add_json(json, "\"");

        while (api->fetch_row(conn->db)) { 
           maxcolname = 0;
	   if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_new_object(json);
	   for (colnum=1; colnum<=numcols; colnum++) {
              dbrelay_write_json_column(json, conn->db, colnum, &maxcolname);
	      if (json_get_mode(json)==DBRELAY_JSON_MODE_CSV && colnum!=numcols) json_add_json(json, ",");
           }
	   if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_end_object(json);
           else json_add_json(json, "\\n");
        }

	if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_end_array(json);
        else json_add_json(json, "\",");

        if (api->rowcount(conn->db)==-1) {
           json_add_null(json, "count");
        } else {
           sprintf(tmp, "%d", api->rowcount(conn->db));
           json_add_number(json, "count", tmp);
        }
        json_end_object(json);
   }
   /* sprintf(error_string, "rc = %d", rc); */
   json_end_array(json);

   return 0;
}

dbrelay_request_t *
dbrelay_alloc_request()
{
   dbrelay_request_t *request;

   request = (dbrelay_request_t *) malloc(sizeof(dbrelay_request_t));
   memset(request, '\0', sizeof(dbrelay_request_t));
   request->http_keepalive = 1;
   //request->flags |= DBRELAY_FLAGS_PP;

   return request;
}
void
dbrelay_free_request(dbrelay_request_t *request)
{
   if (request->sql) free(request->sql);

   free(request);
}
static int
is_quoted_param(char *param)
{
   int ret;
   char *tmp = strdup(param);
   char *s = strstr(tmp, ":");
   *s = '\0';
   if (!strcasecmp(tmp, "char") ||
       !strcasecmp(tmp, "varchar") ||
       !strcasecmp(tmp, "datetime") ||
       !strcasecmp(tmp, "smalldatetime"))
      ret = TRUE;
   else ret = FALSE;
   free(tmp);
   return ret;
}
static char *
dbrelay_resolve_params(dbrelay_request_t *request, char *sql)
{
   int i = 0;
   int pos = 0, prevpos = 0;
   stringbuf_t *sb = sb_new(NULL);
   char *ret;
   char *tmpsql = strdup(sql);

   if (IS_SET(DBRELAY_MAGIC) && !(request->flags & DBRELAY_FLAG_NOMAGIC)) {
      sb_append(sb, DBRELAY_MAGIC);
   }
   while (request->params[i]) {
      prevpos = pos;
      pos += dbrelay_find_placeholder(&tmpsql[pos]);
      if (pos==-1) {
	 // ignore missing placeholders
         pos = prevpos;	
      } else {
         tmpsql[pos]='\0';
         sb_append(sb, &tmpsql[prevpos]);
         if (is_quoted_param(request->params[i])) sb_append(sb, "'");
         sb_append(sb, strstr(request->params[i], ":") + 1);
         if (is_quoted_param(request->params[i])) sb_append(sb, "'");
         pos++;
      }
      i++;
   } 
   sb_append(sb, &tmpsql[pos]);
   ret = sb_to_char(sb);
   free(tmpsql);
   sb_free(sb);
   dbrelay_log_debug(request, "new sql %s", ret);
   return ret;
}
static int
dbrelay_find_placeholder(char *sql)
{
   int quoted = 0;
   int i = 0;
   int found = 0;
   int len = strlen(sql);

   do {
     if (sql[i]=='\'') quoted = quoted ? 0 : 1;
     if (!quoted && sql[i]=='?') found = 1;
     i++;
   } while (!found && i<len);
   if (!found) return -1;
   else return i-1;
}
static int
dbrelay_check_request(dbrelay_request_t *request)
{
   if (!request->sql && !request->cmd) return 0;
   if (!request->sql) return 0;
   if (!IS_SET(request->sql_server)) return 0;
   if (!IS_SET(request->sql_user)) return 0;
   return 1;
} 
static void
dbrelay_write_json_log(json_t *json, dbrelay_request_t *request, char *error_string)
{
   	json_add_key(json, "log");
   	json_new_object(json);
   	if (request->sql) json_add_string(json, "sql", request->sql);
    	json_add_string(json, "error", error_string);
        json_end_object(json);
        json_end_object(json);
}
void dbrelay_write_json_colinfo(json_t *json, void *db, int colnum, int *maxcolname)
{
   char tmp[256], *colname, tmpcolname[256];
   int l;

   json_new_object(json);
   colname = api->colname(db, colnum);
   if (dbrelay_is_unnamed_column(colname)) {
      sprintf(tmpcolname, "%d", ++(*maxcolname));
      json_add_string(json, "name", tmpcolname);
   } else {
      l = atoi(colname); 
      if (l>0 && l>*maxcolname) {
         *maxcolname=l;
      }
      json_add_string(json, "name", colname);
   }
   api->coltype(db, colnum, tmp);
   json_add_string(json, "sql_type", tmp);
   l = api->collen(db, colnum);
   if (l!=0) {
      sprintf(tmp, "%d", l);
      json_add_string(json, "length", tmp);
   }
   l = api->colprec(db, colnum);
   if (l!=0) {
      sprintf(tmp, "%d", l);
      json_add_string(json, "precision", tmp);
   }
   l = api->colscale(db, colnum);
   if (l!=0) {
      sprintf(tmp, "%d", l);
      json_add_string(json, "scale", tmp);
   }
   json_end_object(json);
}
void dbrelay_write_json_column(json_t *json, void *db, int colnum, int *maxcolname)
{
   char *colname, tmpcolname[256];
   int l;

   colname = api->colname(db, colnum);
   if (dbrelay_is_unnamed_column(colname)) {
      sprintf(tmpcolname, "%d", ++(*maxcolname));
   } else {
      l = atoi(colname); 
      if (l>0 && l>*maxcolname) {
         *maxcolname=l;
      }
      strcpy(tmpcolname, colname);
   }

   if (json_get_mode(json)==DBRELAY_JSON_MODE_CSV)
      dbrelay_write_json_column_csv(json, db, colnum);
   else 
      dbrelay_write_json_column_std(json, db, colnum, tmpcolname);
}
static void dbrelay_write_json_column_csv(json_t *json, void *db, int colnum)
{
   char tmp[256];
   unsigned char escape = 0;

   if (api->colvalue(db, colnum, tmp)==NULL) return;
   if (strchr(tmp, ',')) escape = 1;
   if (escape) json_add_json(json, "\\\"");
   json_add_json(json, tmp);
   if (escape) json_add_json(json, "\\\"");
}
static void dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname)
{
   char tmp[256];

   if (api->colvalue(db, colnum, tmp)==NULL) {
      json_add_null(json, colname);
   } else if (api->is_quoted(db, colnum)) {
      json_add_string(json, colname, tmp);
   } else {
      json_add_number(json, colname, tmp);
   }
}
static int calc_time(struct timeval *start, struct timeval *now)
{
   int secs = now->tv_sec - start->tv_sec;
   int usecs = now->tv_usec - start->tv_usec;
   //printf("%s: %ld usecs\n", text, secs * 1000000 + usecs);
   return (secs * 1000000 + usecs);
}

