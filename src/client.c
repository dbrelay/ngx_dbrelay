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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dbrelay.h"
#include "../include/dbrelay_config.h"
#ifndef CMDLINE
#include <ngx_http.h>
#endif

#define DEBUG 0

char *dbrelay_conn_socket_error(dbrelay_request_t *request);

void
dbrelay_conn_kill(int s)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;

   dbrelay_socket_send_string(s, ":DIE\n");
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0);
   close(s);
}
void
dbrelay_conn_close(int s)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;

   dbrelay_socket_send_string(s, ":QUIT\n");
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0);
   close(s);
}
pid_t
dbrelay_conn_initialize(int s, dbrelay_request_t *request)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;
   pid_t childpid;

   if (dbrelay_socket_send_string(s, ":HELO\n")<0) return -1;
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0);
   if (strncmp(":PID ", out_buf, 5)) return -1;
   // trim trailing stuff
   while (out_buf[strlen(out_buf)-1]=='\n') {
      out_buf[strlen(out_buf)-1]='\0';
   }
   childpid = (pid_t) atoi(&out_buf[5]);
   return childpid;
}
char *
dbrelay_conn_send_request(int s, dbrelay_request_t *request, int *error)
{
   stringbuf_t *sb_rslt = NULL;
   char *json_output;
   int results = 0;
   int errors = 0;
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;
   char tmp[20];
   int t;

   *error = 2;

   dbrelay_log_debug(request, "setting options");
   if (dbrelay_conn_set_option(s, "SERVER", request->sql_server)<0) 
      return dbrelay_conn_socket_error(request);
   dbrelay_log_debug(request, "SERVER sent");
   if (dbrelay_conn_set_option(s, "DATABASE", request->sql_database)<0) 
      return dbrelay_conn_socket_error(request);
   if (dbrelay_conn_set_option(s, "USER", request->sql_user)<0) 
      return dbrelay_conn_socket_error(request);
   if (request->sql_password && strlen(request->sql_password)) {
      if (dbrelay_conn_set_option(s, "PASSWORD", request->sql_password)<0)
         return dbrelay_conn_socket_error(request);
   }
   sprintf(tmp, "%ld", request->connection_timeout);
   dbrelay_log_info(request, "timeout %s", tmp);
   if (dbrelay_conn_set_option(s, "TIMEOUT", tmp)<0) 
      return dbrelay_conn_socket_error(request);
   sprintf(tmp, "%lu", request->flags);
   if (dbrelay_conn_set_option(s, "FLAGS", tmp)<0) 
      return dbrelay_conn_socket_error(request);
   if (dbrelay_conn_set_option(s, "APPNAME", request->connection_name)<0) 
      return dbrelay_conn_socket_error(request);
   if (request->output_style && strlen(request->output_style)) {
      if (dbrelay_conn_set_option(s, "OUTPUT", request->output_style)<0) 
          return dbrelay_conn_socket_error(request);
   }

   if (dbrelay_socket_send_string(s, ":SQL BEGIN\n")<0) 
      return dbrelay_conn_socket_error(request);
   if (dbrelay_socket_send_string(s, request->sql)<0) 
      return dbrelay_conn_socket_error(request);
   if (dbrelay_socket_send_string(s, "\n")<0) 
      return dbrelay_conn_socket_error(request);
   if (dbrelay_socket_send_string(s, ":SQL END\n")<0) 
      return dbrelay_conn_socket_error(request);
   *error = 0;

   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0);

   if (dbrelay_socket_send_string(s, ":RUN\n")<0)
      return dbrelay_conn_socket_error(request);

   sb_rslt = sb_new(NULL);
   dbrelay_log_debug(request, "receiving results");
   while ((t=dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0))>0) {
      if (out_buf[strlen(out_buf)-1]=='\n') out_buf[strlen(out_buf)-1]='\0';
      //dbrelay_log_debug(request, "result line = %s", out_buf);
      if (!strcmp(out_buf, ":BYE") ||
	 !strcmp(out_buf, ":OK") ||
	 !strcmp(out_buf, ":ERR")) break;
      //dbrelay_log_debug(request, "in %s", in_buf);
      //dbrelay_log_debug(request, "out %s", out_buf);
      if (!strcmp(out_buf, ":RESULTS END")) results = 0;
      if (!strcmp(out_buf, ":ERROR END")) errors = 0;
      //printf("%s\n", out_buf);
      if (results || errors) {
	 sb_append(sb_rslt, out_buf);
	 //sb_append(sb_rslt, "\n");
      }
      if (!strcmp(out_buf, ":RESULTS BEGIN")) {
	 dbrelay_log_debug(request, "results begun\n");
	 results = 1;
      }
      if (!strcmp(out_buf, ":ERROR BEGIN")) {
	 dbrelay_log_debug(request, "have errors\n");
	 *error = 1;
	 errors = 1;
      }
   }
   if (t==-1) {
      // broken socket
      *error=2;
      sb_free(sb_rslt);
             return dbrelay_conn_socket_error(request);
   }
   dbrelay_log_debug(request, "finished receiving results");
   json_output = sb_to_char(sb_rslt);
   dbrelay_log_debug(request, "receiving results");
   sb_free(sb_rslt);
   return json_output;
}
char *
dbrelay_conn_socket_error(dbrelay_request_t *request)
{
   stringbuf_t *sb_rslt2;
   char *json_output;

   sb_rslt2=sb_new(NULL);
   sb_append(sb_rslt2, "Internal Error: connector terminated connection unexpectedly.");
   json_output = sb_to_char(sb_rslt2);
   sb_free(sb_rslt2);
   dbrelay_log_error(request, "Connector terminated connection unexpectedly.");

   /* XXX - FIX ME, should we kill the connector at this point? */
   return json_output;
}

int
dbrelay_conn_set_option(int s, char *option, char *value)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;
   char *set_string;

   set_string = (char *) malloc(10 + strlen(option) + strlen(value));
   sprintf(set_string, ":SET %s %s\n", option, value);
   /* don't wait for return on failed send */
   if (dbrelay_socket_send_string(s, set_string)==-1) {
      free(set_string);
      return -1; 
   }
   free(set_string);
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf, 0);
   //fprintf(stderr, "set %s returned %s\n", option, out_buf);
   return 0; 
}

pid_t dbrelay_conn_launch_connector(char *sock_path, dbrelay_request_t *request)
{
   char *argv[] = {"dbrelay-connector", sock_path, NULL};
   pid_t child = 0;
   char connector_path[256]; 
   //char line[256]; 
   //FILE *connector;
   struct stat statbuf;
#ifndef CMDLINE
     ngx_http_request_t *r = request->nginx_request;
#endif

   //sprintf(connector_path, "%s/sbin/connector %s", DBRELAY_PREFIX, sock_path);
   sprintf(connector_path, "%s/sbin/connector", DBRELAY_PREFIX);
   if (stat(connector_path, &statbuf)==-1) return (pid_t) -1;

   if ((child = fork())==0) {
#ifndef CMDLINE
     ngx_close_connection(r->connection);
#endif
     execv(connector_path, argv);
     perror("execv");
     //printf("cmd = %s\n", connector_path);
     //connector = popen(connector_path, "r");
     //printf("popen\n");
   } else {
     /* wait for connector to be ready, signaled by dead parent */
     /* waitpid fails because nginx reaps child in signal handler */
     waitpid(child, NULL, 0);
   }
/*
     while (fgets(line, 256, connector)!=NULL) {
        if (strlen(line)>4 && !strncmp(line, ":PID", 4)) {
           child = atol(&line[5]);
	}
     }
     pclose(connector);
*/

     return child;
}
