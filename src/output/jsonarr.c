#include "../dbrelay.h"

void *dbrelay_jsondict_init(dbrelay_request_t *request);
char *dbrelay_jsondict_finalize(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_restart(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_request(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_log(void *emitter, dbrelay_request_t *request, char *error_string, int error);
void dbrelay_jsondict_add_section(void *emitter, char *ret);
char *dbrelay_jsonarr_fill(dbrelay_connection_t *conn, unsigned long flags, int *error);


dbrelay_emitapi_t dbrelay_jsonarr_api = 
{
   &dbrelay_jsondict_init,
   &dbrelay_jsondict_finalize,
   &dbrelay_jsondict_restart,
   &dbrelay_jsondict_request,
   &dbrelay_jsondict_log,
   &dbrelay_jsondict_add_section,
   &dbrelay_jsonarr_fill
};

static void dbrelay_write_json_colinfo(json_t *json, void *db, int colnum, int *maxcolname);
static void dbrelay_write_json_column(json_t *json, void *db, int colnum, int *maxcolname);
static void dbrelay_write_json_column_csv(json_t *json, void *db, int colnum);
static void dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname);
static unsigned char dbrelay_is_unnamed_column(char *colname);

extern dbrelay_dbapi_t *api;

typedef struct {
   json_t *json;
} dbrelay_emit_t;

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
dbrelay_jsonarr_fill(dbrelay_connection_t *conn, unsigned long flags, int *error)
{
   int numcols, colnum;
   char tmp[256];
   int maxcolname;
   char *ret;

   *error = 0;
   json_t *json = json_new();

   if (flags & DBRELAY_FLAG_PP) json_pretty_print(json, 1);
   if (flags & DBRELAY_FLAG_EMBEDCSV) json_set_mode(json, DBRELAY_JSON_MODE_CSV);


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
	   if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_new_array(json);
	   for (colnum=1; colnum<=numcols; colnum++) {
              dbrelay_write_json_column(json, conn->db, colnum, &maxcolname);
	      if (colnum!=numcols) json_add_json(json, ",");
           }
	   if (json_get_mode(json)==DBRELAY_JSON_MODE_STD) json_end_array(json);
           else json_add_json(json, "\\n");

           if (json_mem_exceeded(json)) {
               while (api->fetch_row(conn->db));
               json_free(json);
               *error = 1;
               return NULL;
           }
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

   ret = json_to_string(json);
   json_free(json);
   return ret;
}
static void
dbrelay_write_json_colinfo(json_t *json, void *db, int colnum, int *maxcolname)
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
      json_add_number(json, "length", tmp);
   }
   l = api->colprec(db, colnum);
   if (l!=0) {
      sprintf(tmp, "%d", l);
      json_add_number(json, "precision", tmp);
   }
   l = api->colscale(db, colnum);
   if (l!=0) {
      sprintf(tmp, "%d", l);
      json_add_number(json, "scale", tmp);
   }
   json_end_object(json);
}
static void 
dbrelay_write_json_column(json_t *json, void *db, int colnum, int *maxcolname)
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
static void 
dbrelay_write_json_column_csv(json_t *json, void *db, int colnum)
{
   unsigned char escape = 0;
   int colsize;
   char *tmp;

   colsize = api->collen(db, colnum);
   tmp = (char *) malloc(colsize > 256 ? colsize : 256);

   if (api->colvalue(db, colnum, tmp)==NULL) return;
   if (strchr(tmp, ',')) escape = 1;
   if (escape) json_add_json(json, "\\\"");
   json_add_json(json, tmp);
   if (escape) json_add_json(json, "\\\"");
   free(tmp);
}
static void 
dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname)
{
   int colsize;
   char *tmp;

   colsize = api->collen(db, colnum);
   tmp = (char *) malloc(colsize > 256 ? colsize : 256);

   if (api->colvalue(db, colnum, tmp)==NULL) {
      json_add_json(json, "null");
   } else if (api->is_quoted(db, colnum)) {
      json_add_elem(json, tmp);
   } else {
      json_add_json(json, tmp);
   }
   free(tmp);
}
