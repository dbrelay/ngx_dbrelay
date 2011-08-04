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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
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

extern dbrelay_emitapi_t dbrelay_jsondict_api;
//dbrelay_emitapi_t *emitapi = &dbrelay_jsondict_api;

#define TRUE 1
#define FALSE 0

static int dbrelay_db_get_connection(dbrelay_request_t *request);
static int dbrelay_find_placeholder(char *sql);
static int dbrelay_check_request(dbrelay_request_t *request);
static void dbrelay_db_zero_connection(dbrelay_connection_t *conn, dbrelay_request_t *request);
static int calc_time(struct timeval *start, struct timeval *now);
void dbrelay_cleanup_connector(dbrelay_connection_t *conn);

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
         if (tmpnam(conn->sock_path)==NULL) {
             dbrelay_log_error(request, "Could not get new socket name");
             return;
         }
         dbrelay_conn_launch_connector(conn->sock_path, request);
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

static dbrelay_connection_t *dbrelay_wait_for_connection(dbrelay_request_t *request, int *s)
{
   int slot = 0;
   dbrelay_connection_t *conn;
   dbrelay_connection_t *connections;
   int error;

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
         *s = dbrelay_socket_connect(conn->sock_path, 10, &error);
         // if connect fails, remove connector from list
         if (*s==-1) {
            if (error==3) // timeout
               dbrelay_log_warn(request, "can't connect to helper, cleaning up");
            dbrelay_cleanup_connector(conn);
            free(conn);
            connections = dbrelay_time_get_shmem(request);
            connections[slot].pid=0;
            dbrelay_time_release_shmem(request, connections);
         }
      }
  } while (*s==-1);

  return conn;
}
void dbrelay_cleanup_connector(dbrelay_connection_t *conn)
{
   if (conn->helper_pid) {
      if (!kill(conn->helper_pid, 0)) 
         kill(conn->helper_pid, SIGTERM);
   }
   unlink(conn->sock_path);
}
u_char *dbrelay_db_run_query(dbrelay_request_t *request)
{
   /* FIX ME */
   char error_string[500];
   u_char *ret;
   char *results;
   char *messages;
   dbrelay_connection_t *conn;
   dbrelay_connection_t *connections;
   int s = 0;
   int slot = -1;
   char *newsql;
   int have_error = 0;
   pid_t helper_pid = 0;
   void *emitter;


   emitter = request->emitapi->init(request);

   error_string[0]='\0';

   dbrelay_log_info(request, "run_query called");


   request->emitapi->request(emitter, request);

   if (!dbrelay_check_request(request)) {
        dbrelay_log_info(request, "check_request failed.");
	request->emitapi->restart(emitter, request);
        request->emitapi->log(emitter, request, "Not all required parameters submitted.", 1);

        return (u_char *) request->emitapi->finalize(emitter, request);
   }

   conn = dbrelay_wait_for_connection(request, &s);
   if (conn == NULL) {
      request->emitapi->restart(emitter, request);
      request->emitapi->log(emitter, request, "Couldn't allocate new connection", 1);
      return (u_char *) request->emitapi->finalize(emitter, request);
   }
   slot = conn->slot;

   dbrelay_log_debug(request, "Allocated connection for query");

   if (IS_SET(request->connection_name)) 
   {
      if ((helper_pid = dbrelay_conn_initialize(s, request))==-1) {
         strcpy(error_string, "Couldn't initialize connector");
         request->emitapi->restart(emitter, request);
         request->emitapi->log(emitter, request, "Couldn't initialize connector", 1);
         return (u_char *) request->emitapi->finalize(emitter, request);
      } else if (helper_pid) {
         // write the connectors pid into shared memory
         connections = dbrelay_time_get_shmem(request);
         connections[slot].helper_pid = helper_pid;
         dbrelay_time_release_shmem(request, connections);
      } // else we didn't get a pid but didn't fail, shouldn't happen
      dbrelay_log_info(request, "sending request");
      have_error = dbrelay_conn_send_request(s, request, &results, &messages);
      dbrelay_log_debug(request, "back");
      // internal error
      if (have_error==2) {
         dbrelay_log_error(request, "Error occurred on socket %s (PID: %u)", conn->sock_path, conn->helper_pid);
         // socket error of some sort, kill the connector to be safe and let it restart on its own
         dbrelay_conn_kill(s);
      }
      if (!IS_SET(results)) {
        dbrelay_log_debug(request, "received no results");
        if (IS_SET(messages)) {
           request->emitapi->restart(emitter, request);
           dbrelay_log_debug(request, "have error %s\n", messages);
           dbrelay_copy_string(error_string, messages, sizeof(error_string));
           have_error = 1;
           free(messages);
        } else {
           dbrelay_log_warn(request, "Connector returned no information");
           dbrelay_log_info(request, "Query was: %s", request->sql);
        }
      } else {
         dbrelay_log_debug(request, "received results");
         request->emitapi->add_section(emitter, results);
         if (IS_SET(messages)) {
           dbrelay_copy_string(error_string, messages, sizeof(error_string));
           have_error = 0;
         }
         free(results);
      }
      dbrelay_log_debug(request, "closing");
      dbrelay_conn_close(s);
      dbrelay_log_debug(request, "after close");
   } else {
      newsql = dbrelay_resolve_params(request, request->sql);

      if (!api->connected(conn->db)) {
        if (IS_EMPTY(request->sql_password)) {
	    strcpy(error_string, "Connection failed and no password was set, please check.\n");
	    dbrelay_copy_string(&error_string[strlen(error_string)], api->error(conn->db), sizeof(error_string) - strlen(error_string));
        } else if (!strlen(api->error(conn->db))) {
	    strcpy(error_string, "Connection failed.\n");
        } else {
            dbrelay_copy_string(error_string, api->error(conn->db), sizeof(error_string));
        }
        have_error = 1;
        request->emitapi->restart(emitter, request);
      } else {
   	dbrelay_log_debug(request, "Sending sql query");
        ret = dbrelay_exec_query(conn, request, newsql);
        if (ret==NULL) {
           request->emitapi->restart(emitter, request);
   	   dbrelay_log_debug(request, "error");
           //strcpy(error_string, request->error_message);
           if (conn->mem_exceeded) {
              strcpy(error_string, "Memory usage exceeded");
              dbrelay_log_debug(request, "Memory usage exceeded");
           } else strcpy(error_string, api->error(conn->db));
           have_error = 1;
        } else if (api->error(conn->db)) {
           request->emitapi->add_section(emitter, (char *)ret);
           strcpy(error_string, api->error(conn->db));
           free(ret);
        } else {
           request->emitapi->add_section(emitter, (char *)ret);
           free(ret);
        }
   	dbrelay_log_debug(request, "Done filling JSON output");
      }

      free(newsql);
   } // !named connection
   free(conn);

   dbrelay_log_debug(request, "error = %s\n", error_string);
   request->emitapi->log(emitter, request, error_string, have_error);

   ret = (u_char *) request->emitapi->finalize(emitter, request);
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
dbrelay_exec_query(dbrelay_connection_t *conn, dbrelay_request_t *request, char *sql)
{
  u_char *ret;
  int error;
 
  api->change_db(conn->db, request->sql_database);

  if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_BEGIN, NULL));

  if (api->exec(conn->db, sql))
  {
     ret = (u_char *) request->emitapi->fill(conn, request->flags, &error);
     if (error==1) {
        dbrelay_log_debug(request, "Memory exceeded\n");
        conn->mem_exceeded = 1;
     }
     if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_COMMIT, NULL));
  } else {
     if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_ROLLBACK, NULL));
     return NULL;
  }

  return ret;
}
dbrelay_request_t *
dbrelay_alloc_request()
{
   dbrelay_request_t *request;

   request = (dbrelay_request_t *) malloc(sizeof(dbrelay_request_t));
   memset(request, '\0', sizeof(dbrelay_request_t));
   request->http_keepalive = 1;
   request->connection_timeout = 60;
   request->emitapi = &dbrelay_jsondict_api;
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
char *
dbrelay_resolve_params(dbrelay_request_t *request, char *sql)
{
   int i = 0;
   int pos = 0, prevpos = 0;
   stringbuf_t *sb = sb_new(NULL);
   char *ret;
   char *tmpsql = strdup(sql);
   char *s, *s2;

   if (IS_SET(DBRELAY_MAGIC) && !(request->flags & DBRELAY_FLAG_NOMAGIC)) {
      sb_append(sb, DBRELAY_MAGIC);
   }
   while (request->params[i]) {
      prevpos = pos;
      pos = dbrelay_find_placeholder(&tmpsql[pos]);
      if (pos==-1) {
	 // ignore missing placeholders
         pos = prevpos;	
      } else {
         pos = prevpos + pos;
         tmpsql[pos]='\0';
         sb_append(sb, &tmpsql[prevpos]);
         if (is_quoted_param(request->params[i])) sb_append(sb, "'");
         s = strstr(request->params[i], ":");
         if (!s) {
            return NULL;
         } else s++;
         while ((s2 = strstr(s, "'"))) {
            *s2 = '\0';
            sb_append(sb, s);
            sb_append(sb, "''");
            s = s2 + 1;
         }
         sb_append(sb, s);
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
static int calc_time(struct timeval *start, struct timeval *now)
{
   int secs = now->tv_sec - start->tv_sec;
   int usecs = now->tv_usec - start->tv_usec;
   //printf("%s: %ld usecs\n", text, secs * 1000000 + usecs);
   return (secs * 1000000 + usecs);
}

void
dbrelay_copy_string(char *dest, char *src, int sz)
{
   if (strlen(src) < (unsigned int) sz) strcpy(dest, src);
   else {
      strncpy(dest, src, sz - 1);
      dest[sz-1]='\0';
   }
}

