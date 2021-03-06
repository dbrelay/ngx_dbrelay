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

#include "dbrelay.h"
#include "stringbuf.h"
#include <sys/signal.h>

#define SUCCESS 0
#define FAIL 1
#define NOTFOUND 2

u_char *dbrelay_json_error(char *error_string);
int dbrelay_admin_kill(dbrelay_request_t *request, char *sock_path);
u_char *dbrelay_admin_tables(dbrelay_request_t *request);
u_char *dbrelay_admin_columns(dbrelay_request_t *request);
u_char *dbrelay_admin_pkey(dbrelay_request_t *request);
static int check_params(char **params, int needed);

extern dbrelay_dbapi_t *api;

char *dbrelay_admin_result_text(int ret)
{
   switch (ret) {
      case SUCCESS: return "succeeded";
      case FAIL: return "failed";
      case NOTFOUND: return "not found";
      default: return "unknown result";
   }
}
static int check_params(char **params, int needed)
{
   int i;

   for (i=0; i<needed; i++)
      if (params[0]==NULL) return 0;
   return 1;
}
u_char *dbrelay_db_cmd(dbrelay_request_t *request)
{
   json_t *json = json_new();
   int ret = 0;
   u_char *json_output;

   if (!strcmp(request->cmd, "kill")) {
      if (!check_params(request->params, 1)) 
         return (u_char *) dbrelay_json_error("No parameter specified");
      ret = dbrelay_admin_kill(request, request->params[0]);
   } else if (!strcmp(request->cmd, "tables")) {
      return (u_char *) dbrelay_admin_tables(request);
   } else if (!strcmp(request->cmd, "columns")) {
      if (!check_params(request->params, 1)) 
         return (u_char *) dbrelay_json_error("No parameter specified");
      return (u_char *) dbrelay_admin_columns(request);
   } else if (!strcmp(request->cmd, "pkey")) {
      if (!check_params(request->params, 1)) 
         return (u_char *) dbrelay_json_error("No parameter specified");
      return (u_char *) dbrelay_admin_pkey(request);
   } else {
      return (u_char *) dbrelay_json_error("Unknown admin command");
   }

   json_new_object(json);

   json_add_key(json, "cmd");
   json_new_object(json);
   json_add_string(json, "status", dbrelay_admin_result_text(ret));
   json_end_object(json);

   json_end_object(json);
   json_output = (u_char *) json_to_string(json);
   json_free(json);

   return json_output;
}

u_char *dbrelay_admin_tables(dbrelay_request_t *request)
{

   dbrelay_log_debug(request, "calling tables");
   request->sql = api->catalogsql(DBRELAY_DBCMD_TABLES, NULL);
   return dbrelay_db_run_query(request);
}
u_char *dbrelay_admin_columns(dbrelay_request_t *request)
{
   dbrelay_log_debug(request, "calling columns");
   request->sql = api->catalogsql(DBRELAY_DBCMD_COLUMNS, request->params);
   dbrelay_log_debug(request, "sql %s", request->sql);
   return dbrelay_db_run_query(request);
}
u_char *dbrelay_admin_pkey(dbrelay_request_t *request)
{
   request->sql = api->catalogsql(DBRELAY_DBCMD_PKEY, request->params);
   return dbrelay_db_run_query(request);
}

int dbrelay_admin_kill(dbrelay_request_t *request, char *sock_path)
{
   pid_t pid = -1;
   int slot = -1;
   dbrelay_connection_t *connections;
   dbrelay_connection_t *conn;
   int i;
   int error;

   int s = dbrelay_socket_connect(sock_path, 10, &error);
   if (s==-1) return FAIL;

   dbrelay_conn_kill(s);

   connections = dbrelay_get_shmem();

   for (i=0; i<DBRELAY_MAX_CONN; i++) {
     conn = &connections[i];
     if (conn->pid!=0) {
        if (!strcmp(conn->sock_path, sock_path))
	{
	   pid = conn->helper_pid;
           slot = i;
	}
     }
   }

   dbrelay_release_shmem(connections);

   if (pid==-1) return NOTFOUND;

   if (kill(pid, 0)) kill(pid, SIGTERM);

   if (slot!=-1) {
      connections = dbrelay_get_shmem();
      conn = &connections[slot];
      dbrelay_db_close_connection(conn, request);
      dbrelay_release_shmem(connections);
   }
   return SUCCESS;
}
u_char *dbrelay_json_error(char *error_string)
{
   u_char *json_output;
   json_t *json = json_new();

   json_new_object(json);

   json_add_key(json, "log");
   json_new_object(json);
   json_add_string(json, "error", error_string);
   
   json_end_object(json); //log

   json_end_object(json);

   json_output = (u_char *) json_to_string(json);
   json_free(json);

   return json_output;
}
