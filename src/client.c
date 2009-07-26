#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "dbrelay.h"
#include "../include/dbrelay_config.h"

#define DEBUG 0

void dbrelay_conn_set_option(int s, char *option, char *value);

void
dbrelay_conn_kill(int s)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;

   dbrelay_socket_send_string(s, ":DIE\n");
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf);
   close(s);
}
void
dbrelay_conn_close(int s)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;

   dbrelay_socket_send_string(s, ":QUIT\n");
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf);
   close(s);
}
char *
dbrelay_conn_send_request(int s, dbrelay_request_t *request, int *error)
{
   stringbuf_t *sb_rslt;
   char *json_output;
   int results = 0;
   int errors = 0;
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;
   char tmp[20];
   int t;

   dbrelay_log_debug(request, "setting options");
   dbrelay_conn_set_option(s, "SERVER", request->sql_server);
   dbrelay_log_debug(request, "SERVER sent");
   dbrelay_conn_set_option(s, "DATABASE", request->sql_database);
   dbrelay_conn_set_option(s, "USER", request->sql_user);
   //dbrelay_conn_set_option(s, "PASSWORD", request->sql_password);
   sprintf(tmp, "%ld", request->connection_timeout);
   dbrelay_log_info(request, "timeout %s", tmp);
   dbrelay_conn_set_option(s, "TIMEOUT", tmp);
   sprintf(tmp, "%lu", request->flags);
   dbrelay_conn_set_option(s, "FLAGS", tmp);
   dbrelay_conn_set_option(s, "APPNAME", request->connection_name);

   dbrelay_socket_send_string(s, ":SQL BEGIN\n");
   dbrelay_socket_send_string(s, request->sql);
   dbrelay_socket_send_string(s, "\n");
   dbrelay_socket_send_string(s, ":SQL END\n");
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf);

   dbrelay_socket_send_string(s, ":RUN\n");
   sb_rslt = sb_new(NULL);
   dbrelay_log_debug(request, "receiving results");
   while ((t=dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf))>0) {
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
      sb_rslt=sb_new(NULL);
      sb_append(sb_rslt, "Internal Error: connector terminated connection unexpectedly.");
      dbrelay_log_error(request, "Connector terminated connection unexpectedly.");
   }
   dbrelay_log_debug(request, "finished receiving results");
   json_output = sb_to_char(sb_rslt);
   dbrelay_log_debug(request, "receiving results");
   sb_free(sb_rslt);
   return json_output;
}

void
dbrelay_conn_set_option(int s, char *option, char *value)
{
   char out_buf[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   int in_ptr = -1;
   char set_string[100];

   sprintf(set_string, ":SET %s %s\n", option, value);
   dbrelay_socket_send_string(s, set_string);
   dbrelay_socket_recv_string(s, in_buf, &in_ptr, out_buf);
   //fprintf(stderr, "set %s returned %s\n", option, out_buf);
}

pid_t dbrelay_conn_launch_connector(char *sock_path)
{
   //char *argv[] = {"dbrelay-connector", sock_path, NULL};
   pid_t child = 0;
   char connector_path[256]; 
   char line[256]; 
   FILE *connector;

   //if ((child = fork())==0) {
     strcpy(connector_path, DBRELAY_PREFIX);
     strcat(connector_path, "/sbin/connector");
     strcat(connector_path, " ");
     strcat(connector_path, sock_path);
     //execv(connector_path, argv);
     //printf("cmd = %s\n", connector_path);
     connector = popen(connector_path, "r");
     //printf("popen\n");
   //} else {
     /* wait for connector to be ready, signaled by dead parent */
     //waitpid(child, NULL, 0);
   //}
     while (fgets(line, 256, connector)!=NULL) {
        if (strlen(line)>4 && !strncmp(line, ":PID", 4)) {
           child = atol(&line[5]);
	}
     }
     pclose(connector);

     return child;
}
