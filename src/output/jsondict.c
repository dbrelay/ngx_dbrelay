#include "../dbrelay.h"

void *dbrelay_jsondict_init(dbrelay_request_t *request);
char *dbrelay_jsondict_finalize(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_restart(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_request(void *emitter, dbrelay_request_t *request);
void dbrelay_jsondict_log(void *emitter, dbrelay_request_t *request, char *error_string);
void dbrelay_jsondict_add_section(void *emitter, char *ret);
char *dbrelay_jsondict_fill(dbrelay_connection_t *conn, unsigned long flags);


dbrelay_emitapi_t dbrelay_jsondict_api = 
{
   &dbrelay_jsondict_init,
   &dbrelay_jsondict_finalize,
   &dbrelay_jsondict_restart,
   &dbrelay_jsondict_request,
   &dbrelay_jsondict_log,
   &dbrelay_jsondict_add_section,
   &dbrelay_jsondict_fill
};

static void dbrelay_write_json_colinfo(json_t *json, void *db, int colnum, int *maxcolname);
static void dbrelay_write_json_column(json_t *json, void *db, int colnum, int *maxcolname);
static void dbrelay_write_json_column_csv(json_t *json, void *db, int colnum);
static void dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname);
static unsigned char dbrelay_is_unnamed_column(char *colname);
static void dbrelay_emit_log_json(json_t *json, dbrelay_request_t *request, char *error_string);

extern dbrelay_dbapi_t *api;

typedef struct {
   json_t *json;
} dbrelay_emit_t;

void
dbrelay_jsondict_add_section(void *e, char *ret)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;

   json_add_json(emitter->json, ", ");
   json_add_json(emitter->json, (char *) ret);
}
void *
dbrelay_jsondict_init(dbrelay_request_t *request)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) malloc(sizeof(dbrelay_emit_t));
   memset(emitter, 0, sizeof(dbrelay_emit_t));

   json_t *json = json_new();

   if (request->flags & DBRELAY_FLAG_PP) json_pretty_print(json, 1);

   if (IS_SET(request->js_callback)) {
      json_add_callback(json, request->js_callback);
   }

   json_new_object(json);

   emitter->json = json;
   return (void *) emitter;
}
char *
dbrelay_jsondict_finalize(void *e, dbrelay_request_t *request)
{
   char *ret;

   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;

   if (IS_SET(request->js_callback) || IS_SET(request->js_error)) {
      json_end_callback(emitter->json);
   }

   ret = json_to_string(emitter->json);
   json_free(emitter->json);
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
dbrelay_jsondict_fill(dbrelay_connection_t *conn, unsigned long flags)
{
   int numcols, colnum;
   char tmp[256];
   int maxcolname;
   char *ret;

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

   ret = json_to_string(json);
   json_free(json);
   return ret;
}
/*
 * free the current json object in case of error midstream
 */
void 
dbrelay_jsondict_restart(void *e, dbrelay_request_t *request)
{
  
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e; 

   if (IS_SET(request->js_error)) {
      // free json handle and start over
      json_free(emitter->json);
      emitter->json = json_new();
      json_add_callback(emitter->json, request->js_error);
      json_new_object(emitter->json);
      dbrelay_jsondict_request(emitter, request);
   }
}
/*
 * echo request parameters in json output
 */
void 
dbrelay_jsondict_request(void *e, dbrelay_request_t *request)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;

   json_add_key(emitter->json, "request");
   json_new_object(emitter->json);

   if (IS_SET(request->query_tag)) 
      json_add_string(emitter->json, "query_tag", request->query_tag);
   json_add_string(emitter->json, "sql_server", request->sql_server);
   json_add_string(emitter->json, "sql_user", request->sql_user);

   if (IS_SET(request->sql_port)) 
      json_add_string(emitter->json, "sql_port", request->sql_port);

   json_add_string(emitter->json, "sql_database", request->sql_database);

/*
 * do not return password back to client
   if (IS_SET(request->sql_password)) 
      json_add_string(emitter->json, "sql_password", request->sql_password);
*/

   json_end_object(emitter->json);

}
void 
dbrelay_jsondict_log(void *e, dbrelay_request_t *request, char *error_string)
{
   dbrelay_emit_t *emitter = (dbrelay_emit_t *) e;
   dbrelay_emit_log_json(emitter->json, request, error_string);
}
static void 
dbrelay_emit_log_json(json_t *json, dbrelay_request_t *request, char *error_string)
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
      json_add_null(json, colname);
   } else if (api->is_quoted(db, colnum)) {
      json_add_string(json, colname, tmp);
   } else {
      json_add_number(json, colname, tmp);
   }
   free(tmp);
}
