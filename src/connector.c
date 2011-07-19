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
#include <sys/time.h>
#include <sys/signal.h>
#include <stdarg.h>
#include "dbrelay.h"
#include "../include/dbrelay_config.h"

extern dbrelay_dbapi_t *api;
extern dbrelay_emitapi_t dbrelay_jsondict_api;
extern dbrelay_emitapi_t dbrelay_jsonarr_api;
extern dbrelay_emitapi_t dbrelay_csv_api;

#define SOCK_PATH "/tmp/dbrelay/connector"
#define DEBUG 1
#define PERSISTENT_CONN 1
#define GDB 0

#define OK 1
#define QUIT 2
#define ERR 3
#define DIE 4
#define RUN 5
#define CONT 6
#define HELO 7

void log_open();
void log_close();
void log_msg(char *fmt, ...);
void timeout(int i);
int set_timer(int secs);
void set_signal();

char app_name[DBRELAY_NAME_SZ];
char timeout_str[10];
int receive_sql;
int receive_param;
stringbuf_t *sb_sql;
stringbuf_t *sb_param;
int paramnum;
char logfilename[256];

dbrelay_request_t request;

static FILE *logfile;
struct itimerval it;
struct timeval last_accessed;

  
/* signal callback for timeout */
void timeout(int i)
{
   log_msg("Timeout reached. Exiting.\n");
   exit(0);
}

/* setup the timer for the specified timeout value */
int 
set_timer(int secs)
{
  gettimeofday(&last_accessed, NULL);
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = secs; 
  it.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &it,0);
}
void 
set_signal()
{
   struct sigaction sigact;
   sigset_t sigmask;

   memset(&sigact, 0, sizeof(sigact));
   sigact.sa_handler = &timeout;
   sigfillset(&sigmask);
   sigact.sa_mask = sigmask;
   sigaction(SIGALRM, &sigact, NULL); 
}

int
main(int argc, char **argv)
{
   unsigned int s, s2;
   char line[DBRELAY_SOCKET_BUFSIZE];
   char in_buf[DBRELAY_SOCKET_BUFSIZE];
   char buf[30];
   int in_ptr = -1;
   int len, pos = 0;
   int done = 0, ret, t = 0;
   char *results;
   char *sock_path;
   dbrelay_connection_t conn;
   unsigned char connected = 0;
   pid_t pid;
   int tries = 0;
   struct timeval now;
   char *newsql;

   if (argc>1) {
      sock_path = argv[1];
   } else {
      sock_path = SOCK_PATH;
   }

   memset(&conn, 0, sizeof(dbrelay_connection_t));
   s = dbrelay_socket_create(sock_path);

   // fork and die so parent knows we are ready
   if (!GDB && (pid=fork())) {
      exit(0);
   }

   set_timer(60); // set a default timer in case nobody attaches

   api->init();

   log_open();
   log_msg("Using socket path %s\n", sock_path);

   // register SIGALRM handler
   //signal(SIGALRM,timeout); 
   set_signal(); 

   request.emitapi = &dbrelay_jsondict_api;

   for (;;) {
      done = 0;
      // wait for connection
      tries = 0;
      do {
         log_msg("waiting for new connection\n");
         s2 = dbrelay_socket_accept(s);
         if (s2==-1) {
            log_msg("socket accept had error\n");
            tries++;
            if (tries>3) { exit(0); }
         }
         gettimeofday(&now, NULL);
         if (request.connection_timeout && now.tv_sec - last_accessed.tv_sec > request.connection_timeout) {
            log_msg("manual timeout occurred\n");
            exit(0);
         }

      } while (s2==0);
     
      log_msg("connected\n");
      in_ptr = -1;
      // get a newline terminated string from the client
      while (!done && (t=dbrelay_socket_recv_string(s2, in_buf, &in_ptr, line, 30))>0) {
           if (strlen(line)<9 || strncmp(line, ":SET PASS", 9)) log_msg("line = %s\n", line);
           ret = process_line(line);
           
           if (ret == HELO) {
              sprintf(buf, ":PID %lu\n", (long unsigned) getpid());
              dbrelay_socket_send_string(s2, buf);
           } else if (ret == QUIT) {
              log_msg("disconnect.\n"); 
              dbrelay_socket_send_string(s2, ":BYE\n");
              close(s2);
              done = 1;
           } else if (ret == RUN) {
              request.error_message[0]='\0';
              log_msg("running\n"); 
#if PERSISTENT_CONN
              if (!connected) {
#endif
                  conn.db = api->connect(&request);
                  connected = 1;
                  if (!conn.db) {
	             log_msg("login is null\n"); 
                     dbrelay_socket_send_string(s2, ":ERROR BEGIN\n");
                     log_msg("returning error %s\n", api->error(NULL));
                     dbrelay_socket_send_string(s2, api->error(NULL));
                     dbrelay_socket_send_string(s2, "\n");
                     dbrelay_socket_send_string(s2, ":ERROR END\n");
                     dbrelay_socket_send_string(s2, ":OK\n");
                     return 0;
                  }
#if PERSISTENT_CONN
              }
#endif
              log_msg("%s\n", request.sql);
              // don't timeout during query run
	      if (request.connection_timeout) set_timer(DBRELAY_HARD_TIMEOUT);

              newsql = dbrelay_resolve_params(&request, request.sql);
              log_msg("newsql %s\n", newsql);
              results = (char *) dbrelay_exec_query(&conn, &request, newsql);
              log_msg("addr = %lu\n", results);
              if (results == NULL) {
	         log_msg("results are null\n"); 
                 dbrelay_socket_send_string(s2, ":ERROR BEGIN\n");
                 if (conn.mem_exceeded) {
                    log_msg("Memory usage exceeded");
                    dbrelay_socket_send_string(s2, "Memory usage exceeded");
                 } else {
                    log_msg("error is %s\n", api->error(conn.db));
                    dbrelay_socket_send_string(s2, api->error(conn.db));
                 }
                 dbrelay_socket_send_string(s2, "\n");
                 dbrelay_socket_send_string(s2, ":ERROR END\n");
                 conn.mem_exceeded = 0;
              } else if (api->error(conn.db)) {
                 log_msg("sending results\n"); 
                 dbrelay_socket_send_string(s2, ":RESULTS BEGIN\n");
                 log_msg("%s\n", results);
                 log_msg("len = %d\n", strlen(results));
                 dbrelay_socket_send_string(s2, results);
                 dbrelay_socket_send_string(s2, "\n");
                 dbrelay_socket_send_string(s2, ":RESULTS END\n");
                 log_msg("error is %s\n", api->error(conn.db));
                 dbrelay_socket_send_string(s2, ":ERROR BEGIN\n");
                 dbrelay_socket_send_string(s2, api->error(conn.db));
                 dbrelay_socket_send_string(s2, "\n");
                 dbrelay_socket_send_string(s2, ":ERROR END\n");
              } else {
                 log_msg("sending results\n"); 
                 dbrelay_socket_send_string(s2, ":RESULTS BEGIN\n");
                 log_msg("%s\n", results);
                 log_msg("len = %d\n", strlen(results));
                 dbrelay_socket_send_string(s2, results);
                 dbrelay_socket_send_string(s2, "\n");
                 dbrelay_socket_send_string(s2, ":RESULTS END\n");
              }
              dbrelay_socket_send_string(s2, ":OK\n");
              log_msg("done\n"); 
#if !PERSISTENT_CONN
              api->close(conn.db);
#endif
              free(results);
	      if (request.connection_timeout) set_timer(request.connection_timeout);
           } else if (ret == DIE) {
              log_msg("sending BYE.\n"); 
              dbrelay_socket_send_string(s2, ":BYE\n");
              close(s2);
              log_msg("exiting.\n"); 
              log_close();
              exit(0);
           } else if (ret == OK) {
              dbrelay_socket_send_string(s2, ":OK\n");
              if (request.connection_timeout) set_timer(request.connection_timeout);
           } else if (ret == CONT) {
              log_msg("(cont)\n"); 
           } else {
              log_msg("ret = %d.\n", ret); 
              dbrelay_socket_send_string(s2, ":ERR\n");
           }
        } // recv
        if (t<=0) log_msg("client connection broken\n");
        receive_sql = 0;
        receive_param = 0;
        if (!api->isalive(conn.db)) {
           connected = 0;
        }
   } // for
   return 0;
}
int 
process_line(char *line)
{
   char arg[100];
   int len = strlen(line);
   char flag_str[10];
   char temp_str[30];

   if (receive_sql) {
      log_msg("sql mode\n");
      log_msg("line: %s\n", line);
      if (!line || strlen(line)<8 || strncmp(line, ":SQL END", 8)) {
      	sb_append(sb_sql, line);
      	//sb_append(sb_sql, "\n");
        return CONT;
      }
   } 
   if (receive_param) {
      log_msg("param mode\n");
      log_msg("line: %s\n", line);
      if (!line || strlen(line)<10 || strncmp(line, ":PARAM END", 10)) {
      	sb_append(sb_param, line);
        return CONT;
      }
   }
   if (line[strlen(line)-1]=='\n') line[strlen(line)-1]='\0';

   if (len<1 || line[0]!=':') {
      log_msg("bad protocol command returning ERR\n");
      log_msg(line);
      return ERR;
   }

   if (check_command(line, "HELO", NULL, 0)) return HELO;
   else if (check_command(line, "QUIT", NULL, 0)) return QUIT;
   else if (check_command(line, "RUN", NULL, 0)) return RUN;
   else if (check_command(line, "DIE", NULL, 0)) return DIE;
   else if (check_command(line, "SET NAME", &request.connection_name, sizeof(request.connection_name))) {
      log_msg("connection name %s\n");
      return OK;
   }
   else if (check_command(line, "SET PORT", &request.sql_port, sizeof(request.sql_port))) return OK;
   else if (check_command(line, "SET SERVER", &request.sql_server, sizeof(request.sql_server))) return OK;
   else if (check_command(line, "SET DATABASE", &request.sql_database, sizeof(request.sql_database))) return OK;
   else if (check_command(line, "SET USER", &request.sql_user, sizeof(request.sql_user))) {
     log_msg("username %s\n", request.sql_user);
     return OK;
   }
   else if (check_command(line, "SET PASSWORD", &request.sql_password, sizeof(request.sql_password))) return OK;
   else if (check_command(line, "SET APPNAME", &request.connection_name, sizeof(request.connection_name))) return OK;
   else if (check_command(line, "SET TIMEOUT", timeout_str, sizeof(timeout_str))) {
      request.connection_timeout = atol(timeout_str);
      return OK;
   }
   else if (check_command(line, "SET FLAGS", flag_str, sizeof(flag_str))) {
      request.flags = (unsigned long) atol(flag_str);
      return OK;
   }
   else if (check_command(line, "SET OUTPUT", temp_str, sizeof(temp_str))) {
      if (!strcmp(temp_str, "json-dict")) {
         request.emitapi = &dbrelay_jsondict_api;
      } else if (!strcmp(temp_str, "json-arr")) {
         request.emitapi = &dbrelay_jsonarr_api;
      } else if (!strcmp(temp_str, "csv")) {
         request.emitapi = &dbrelay_csv_api;
      }
   }
   //else if (check_command(line, "SET PARAM", temp_str, sizeof(temp_str))) {
   else if (check_command(line, "PARAM", arg, sizeof(arg))) {
      if (!strncmp(arg, "BEGIN", 5)) {
         log_msg("param mode on\n");
         paramnum = atoi(&line[13]);
         receive_param = 1;
         if (request.params[paramnum]) free(request.params[paramnum]);
         sb_param = sb_new(NULL);
         //request.params[paramnum]=strdup(temp_str);
         log_msg("param %d\n", paramnum);
      } else if (!strncmp(arg, "END", 3)) {
         log_msg("param mode off\n");
         receive_param = 0;
         request.params[paramnum] = sb_to_char(sb_param);
         if (request.params[paramnum][strlen(request.params[paramnum])-1]=='\n') request.params[paramnum][strlen(request.params[paramnum])-1]='\0';
         sb_free(sb_sql);
      } else return ERR;
      if (receive_param) return CONT;
      else return OK;
   }
   else if (check_command(line, "SQL", arg, sizeof(arg))) {
      if (!strcmp(arg, "BEGIN")) {
         log_msg("sql mode on\n");
         receive_sql = 1;
         if (request.sql) free(request.sql);
         request.sql = NULL;
         sb_sql = sb_new(NULL);
      } else if (!strcmp(arg, "END")) {
         log_msg("sql mode off\n");
         receive_sql = 0;
         request.sql = sb_to_char(sb_sql);
         //log_msg("sql");
         //log_msg(request.sql);
         sb_free(sb_sql);
      } else return ERR;
      if (receive_sql) return CONT;
      else return OK;
   }
   else if (check_command(line, "RUN", NULL, 0)) {
      return RUN;
   }

   return ERR;
}
int
check_command(char *line, char *command, char *dest, int maxsz)
{
   int cmdlen = strlen(command);

   if (strlen(line)>=cmdlen+1 && !strncmp(&line[1], command, cmdlen)) {
      if (dest && strlen(line)>cmdlen+1) {
         dbrelay_copy_string(dest, &line[cmdlen+2], maxsz);
      }
      return 1;
   } else {
      return 0;
   }
}

void log_open()
{
#if DEBUG
   char logdir[256];

   sprintf(logdir, "%s/logs", DBRELAY_PREFIX);
   sprintf(logfilename, "%s/connector%ld.log", logdir, (long unsigned) getpid());
   logfile = fopen(logfilename, "w");

#endif
}

void
log_msg(char *fmt, ...)
{
#if DEBUG
   va_list  args;
   time_t t;
   struct tm *tm;
   char today[256];

   if (!logfile) return;

   time(&t);
   tm = localtime(&t);

   strftime(today, sizeof(today), "%Y-%m-%d %H:%M:%S", tm);
   fprintf(logfile, "%s: ", today);

   va_start(args, fmt);
   vfprintf(logfile, fmt, args);
   va_end(args);

   fflush(logfile);
#endif
}
void log_close(FILE *log)
{
#if DEBUG
   if (logfile) fclose(logfile);
#endif
}

