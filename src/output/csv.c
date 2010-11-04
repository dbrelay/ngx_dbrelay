#include "../dbrelay.h"

void *dbrelay_csv_init(dbrelay_request_t *request);
char *dbrelay_csv_finalize(void *emitter, dbrelay_request_t *request);
void dbrelay_csv_restart(void *emitter, dbrelay_request_t *request);
void dbrelay_csv_request(void *emitter, dbrelay_request_t *request);
void dbrelay_csv_log(void *emitter, dbrelay_request_t *request, char *error_string, int error);
void dbrelay_csv_add_section(void *emitter, char *ret);
char *dbrelay_csv_fill(dbrelay_connection_t *conn, unsigned long flags);


dbrelay_emitapi_t dbrelay_csv_api = 
{
   &dbrelay_csv_init,
   &dbrelay_csv_finalize,
   &dbrelay_csv_restart,
   &dbrelay_csv_request,
   &dbrelay_csv_log,
   &dbrelay_csv_add_section,
   &dbrelay_csv_fill
};

static void dbrelay_write_csv_colname(stringbuf_t *sb, void *db, int colnum, int *maxcolname);
static void dbrelay_write_csv_column(stringbuf_t *sb, void *db, int colnum);
static unsigned char dbrelay_is_unnamed_column(char *colname);

extern dbrelay_dbapi_t *api;

typedef struct {
   stringbuf_t *sb;
} dbrelay_emit_t;

void
dbrelay_csv_add_section(void *e, char *ret)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;

   sb_append(emitter->sb, ret);
}
void *
dbrelay_csv_init(dbrelay_request_t *request)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) malloc(sizeof(dbrelay_emit_t));
   memset(emitter, 0, sizeof(dbrelay_emit_t));

   emitter->sb = sb_new(NULL);
   return (void *) emitter;
}
char *
dbrelay_csv_finalize(void *e, dbrelay_request_t *request)
{
   char *ret;

   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;

   ret = sb_to_char(emitter->sb);
   sb_free(emitter->sb);
   return ret;
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
char *
dbrelay_csv_fill(dbrelay_connection_t *conn, unsigned long flags)
{
   int numcols, colnum;
   int maxcolname;
   char *ret;

   stringbuf_t *sb = sb_new(NULL);

   while (api->has_results(conn->db)) 
   {
        maxcolname = 0;

	numcols = api->numcols(conn->db);
	for (colnum=1; colnum<=numcols; colnum++) {
            if (colnum!=1) sb_append(sb, ",");
            dbrelay_write_csv_colname(sb, conn->db, colnum, &maxcolname);
        }
        sb_append(sb, "\n");

        while (api->fetch_row(conn->db)) { 
           maxcolname = 0;
	   for (colnum=1; colnum<=numcols; colnum++) {
              if (colnum!=1) sb_append(sb, ",");
              dbrelay_write_csv_column(sb, conn->db, colnum);
           }
           sb_append(sb, "\n");
        }
   }
   ret = sb_to_char(sb);
   sb_free(sb);
   return ret;
}
/*
 * free the current stringbuf object in case of error midstream
 */
void 
dbrelay_csv_restart(void *e, dbrelay_request_t *request)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;
   sb_free(emitter->sb);
   emitter->sb = sb_new(NULL);
}
/*
 * echo request parameters in json output
 */
void 
dbrelay_csv_request(void *e, dbrelay_request_t *request)
{
}
void 
dbrelay_csv_log(void *e, dbrelay_request_t *request, char *error_string, int error)
{
}
static void
dbrelay_write_csv_colname(stringbuf_t *sb, void *db, int colnum, int *maxcolname)
{
   char *colname, tmpcolname[256];
   int l;

   colname = api->colname(db, colnum);
   if (dbrelay_is_unnamed_column(colname)) {
      sprintf(tmpcolname, "%d", ++(*maxcolname));
      sb_append(sb, tmpcolname);
   } else {
      l = atoi(colname); 
      if (l>0 && l>*maxcolname) {
         *maxcolname=l;
      }
      sb_append(sb, colname);
   }
}
static void 
dbrelay_write_csv_column(stringbuf_t *sb, void *db, int colnum)
{
   unsigned char escape = 0;
   int colsize;
   char *s, *first, *tmp;

   colsize = api->collen(db, colnum);
   tmp = (char *) malloc(colsize > 256 ? colsize : 256);

   if (api->colvalue(db, colnum, tmp)==NULL) {
      free(tmp);
      return;
   }
   if (strchr(tmp, ',')) escape = 1;
   if (escape) sb_append(sb, "\"");

   first = tmp;
   if (escape) {
      for (s=tmp, first=tmp; *s; s++) {
         if (*s=='"') {
            *s='\0';
            sb_append(sb, first);
            sb_append(sb, "\"\"");
            first=s+1;
         }
      }
   }
   sb_append(sb, first);

   if (escape) sb_append(sb, "\"");
   free(tmp);
}
