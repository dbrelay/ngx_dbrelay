#include "dbrelay.h"
#include "stdio.h"

#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#include <readline/history.h>
#endif /* HAVE_READLINE_H */
#include <errno.h>

#define QUIET 0

static char *dbrelay_readline(char *prompt);
static void dbrelay_add_history(const char *s);
static void write_flag_values(dbrelay_request_t *request, char *value);

static int istty;
static char *input_file;

static int
populate_request(int argc, char **argv, dbrelay_request_t *request)
{
    int opt;
    int required = 0;

    while ((opt = getopt(argc, argv, "h:p:u:w:c:t:d:v:f:F:T:S:C:E:")) != -1) {
          switch (opt) {
          case 'c':
                  strcpy(request->connection_name, optarg);
                  break;
          case 'C':
                  strcpy(request->js_callback, optarg);
                  break;
          case 'd':
                  strcpy(request->sql_database, optarg);
                  break;
          case 'E':
                  strcpy(request->js_error, optarg);
                  break;
          case 'f':
                  input_file = strdup(optarg);
                  break;
          case 'F':
                  write_flag_values(request, optarg);
                  break;
          case 'h':
                  strcpy(request->sql_server, optarg);
                  required++;
                  break;
          case 'l':
                  request->log_level = atoi(optarg);
                  break;
          case 'p':
                  strcpy(request->sql_port, optarg);
                  break;
          case 'S':
                  strcpy(request->sock_path, optarg);
                  break;
          case 'T':
                  request->connection_timeout = atoi(optarg);
                  break;
          case 't':
                  strcpy(request->query_tag, optarg);
                  break;
          case 'u':
                  strcpy(request->sql_user, optarg);
                  required++;
                  break;
          case 'w':
                  strcpy(request->sql_password, optarg);
                  break;
          case 'v':
                  break;
          }
    }
    if (required<2) return 0;
    return 1;
}
static void
slurp_input_file(char *fname, char **mybuf, int *bufsz, size_t *buflen, int *line)
{
   FILE *fp = NULL;
   register char *n;
   char linebuf[1024];
   char *s = NULL;

   if ((fp = fopen(fname, "r")) == NULL) {
       fprintf(stderr, "Unable to open input file '%s': %s\n", fname, strerror(errno));
       return;
   }
   while ((s = fgets(linebuf, sizeof(linebuf), fp)) != NULL) {
       while (*buflen + strlen(s) + 2 > *bufsz) {
          *bufsz *= 2;
          *mybuf = (char *) realloc(*mybuf, *bufsz);
       }
       strcpy(*mybuf + *buflen, s);
       *buflen += strlen(*mybuf + *buflen);
       n = strrchr(s, '\n');
       if (n != NULL) *n = '\0';
       (*line)++;
   }
}
int main(int argc, char **argv)
{
    u_char *json_output;
    dbrelay_request_t *request;
    dbrelay_connection_t *connections;
    pid_t pid;
    char *s, *s2, *m2;
    char *cmd = NULL;
    int line = 0;
    char prompt[20];
    char *mybuf;
    int bufsz = 4096;
    size_t buflen = 0;
    char *param0;

    istty = isatty(0);

    request = dbrelay_alloc_request();
    strcpy(request->sql_port, "1433");
    request->log_level = 0;

    if (!populate_request(argc, argv, request)) {
       printf("Usage: %s -u <user> -h <host> [-c <connection name>] [-d <database>] [-f <input file>] [-l <log level>] [-p <port>] [-t <tag>] [-v] [-w <password>]\n", argv[0]);
       exit(1);
    }

    if ((connections=dbrelay_get_shmem())==NULL) {
       dbrelay_create_shmem();
    } else {
       dbrelay_release_shmem(connections);
    }

    /* give the buffer an initial size */
    bufsz = 4096;
    mybuf = (char *) malloc(bufsz);
    mybuf[0] = '\0';
    buflen = 0;

    if (input_file) {
       slurp_input_file(input_file, &mybuf, &bufsz, &buflen, &line);
       request->sql = mybuf;
       json_output = (u_char *) dbrelay_db_run_query(request);
       printf("%s\n", json_output);
    } else 
    for (s=NULL, s2=NULL; ; free(s), free(s2), s2=NULL) {
         sprintf(prompt, "%d> ", ++line);
         s = dbrelay_readline(QUIET ? NULL : prompt);
         if (s == NULL) break;

         s2 = strdup(s);
	 if ((cmd = strtok(s2, " \t")) == NULL) cmd = "";

	 if (!strcasecmp(cmd, "exit") || 
	     !strcasecmp(cmd, "quit") || 
	     !strcasecmp(cmd, "bye")) {
		 break;
	 } else if (!strcasecmp(cmd, "go")) {
	    printf("mybuf %s\n", mybuf);
	    if (strlen(mybuf)>=6 && !strncmp(mybuf, "status", 6)) {
	       json_output = (u_char *) dbrelay_db_status(request);
	    } else if (strlen(mybuf)>=4 && !strncmp(mybuf, "kill", 4)) {
	       m2 = strdup(mybuf);
	       strtok(m2, " \t");
	       param0 = strtok(NULL, " \t\n\r");
	       fprintf(stderr, "killing %s\n", param0);
               strcpy(request->cmd,"kill");
               request->params[0] = param0;
	       json_output = dbrelay_db_cmd(request);
	       free(m2);
	    } else if (strlen(mybuf)>=11 && !strncmp(mybuf, "list tables", 11)) {
               strcpy(request->cmd,"tables");
	       json_output = (u_char *) dbrelay_db_cmd(request);
	    } else if (strlen(mybuf)>=12 && !strncmp(mybuf, "list columns", 12)) {
	       m2 = strdup(mybuf);
	       strtok(m2, " \t");
	       strtok(NULL, " \t\n\r");
	       param0 = strtok(NULL, " \t\n\r");
               strcpy(request->cmd,"columns");
               request->params[0] = param0;
	       json_output = dbrelay_db_cmd(request);
	       free(m2);
	    } else if (strlen(mybuf)>=9 && !strncmp(mybuf, "list keys", 9)) {
	       m2 = strdup(mybuf);
	       strtok(m2, " \t");
	       strtok(NULL, " \t\n\r");
	       param0 = strtok(NULL, " \t\n\r");
               strcpy(request->cmd,"pkey");
               request->params[0] = param0;
	       json_output = dbrelay_db_cmd(request);
	       free(m2);
            } else {
               request->sql = mybuf;
               json_output = (u_char *) dbrelay_db_run_query(request);
            }
            printf("%s\n", json_output);
            line = 0;
            mybuf[0] = '\0';
            buflen = 0;
         } else if (!strcasecmp(cmd, "reset")) {
            line = 0;
            mybuf[0] = '\0';
            buflen = 0;
         } else if (!strcasecmp(cmd, "load")) {
            param0 = strtok(NULL, " \t");
            fprintf(stderr, "reading %s\n", param0);
            slurp_input_file(param0, &mybuf, &bufsz, &buflen, &line);
         } else {
            while (buflen + strlen(s) + 2 > bufsz) {
                char *newbuf;
                bufsz *= 2;
                if ((newbuf = realloc(mybuf, bufsz)) == NULL) {
                    perror("dbrelay: ");
                    exit(1);
                }
                mybuf = newbuf;
            }
            dbrelay_add_history(s);
            strcpy(mybuf + buflen, s);
            /* preserve line numbering for the parser */
            strcat(mybuf + buflen, "\n");
            buflen += strlen(mybuf + buflen);
         }
    }
    request->sql = NULL;
    free(mybuf);
    dbrelay_free_request(request);
}

/**
 * adapted from FreeTDS tsql program
 */
static char *
dbrelay_readline(char *prompt)
{
        size_t sz, pos;
        char *line, *p;

#ifdef HAVE_LIBREADLINE
        if (istty)
                return readline(prompt);
#endif

        sz = 1024;
        pos = 0;
        line = (char*) malloc(sz);
        if (!line)
                return NULL;

        if (prompt && prompt[0])
                printf("%s", prompt);
        for (;;) {
                /* read a piece */
                if (fgets(line + pos, sz - pos, stdin) == NULL) {

                        if (pos)
                                return line;
                        break;
                }

                /* got end-of-line ? */
                p = strchr(line + pos, '\n');
                if (p) {
                        *p = 0;
                        return line;
                }

                /* allocate space if needed */
                pos += strlen(line + pos);
                if (pos + 1024 >= sz) {
                        sz += 1024;
                        p = (char*) realloc(line, sz);
                        if (!p)
                                break;
                        line = p;
                }
        }
        free(line);
	return NULL;
}
static void
dbrelay_add_history(const char *s)
{
#ifdef HAVE_LIBREADLINE
        if (istty)
                add_history(s);
#endif
}

static void
write_flag_values(dbrelay_request_t *request, char *value)
{
   char *flags = strdup(value);
   char *tok;

   while ((tok = strsep(&flags, ","))) {
      if (!strcmp(tok, "echosql")) request->flags|=DBRELAY_FLAG_ECHOSQL;
      else if (!strcmp(tok, "pp")) request->flags|=DBRELAY_FLAG_PP;
      else if (!strcmp(tok, "xact")) request->flags|=DBRELAY_FLAG_XACT;
   }
   free(flags);
}

