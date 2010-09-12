static void dbrelay_write_json_column_csv(json_t *json, void *db, int colnum);
static void dbrelay_write_json_column_std(json_t *json, void *db, int colnum, char *colname);

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
void 
dbrelay_append_log_json(json_t *json, dbrelay_request_t *request, char *error_string)
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
/*
 * free the current json object in case of error midstream
 */
void 
dbrelay_db_restart_json(dbrelay_request_t *request, json_t **json)
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
/*
 * echo request parameters in json output
 */
void 
dbrelay_append_request_json(json_t *json, dbrelay_request_t *request)
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
void
dbrelay_write_json_log(json_t *json, dbrelay_request_t *request, char *error_string)
{
   	json_add_key(json, "log");
   	json_new_object(json);
   	if (request->sql) json_add_string(json, "sql", request->sql);
    	json_add_string(json, "error", error_string);
        json_end_object(json);
        json_end_object(json);
}
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
void 
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
