
/*
 * Copyright (C) Getco LLC
 */

#include "dbrelay.h"
#include "stringbuf.h"
#include "mssql.h"

#define IS_SET(x) (x && strlen(x)>0)

dbrelay_dbapi_t dbrelay_mssql_api = 
{
   &dbrelay_mssql_init,
   &dbrelay_mssql_connect,
   &dbrelay_mssql_close,
   &dbrelay_mssql_assign_request,
   &dbrelay_mssql_is_quoted,
   &dbrelay_mssql_connected,
   &dbrelay_mssql_change_db,
   &dbrelay_mssql_exec,
   &dbrelay_mssql_rowcount,
   &dbrelay_mssql_has_results,
   &dbrelay_mssql_numcols,
   &dbrelay_mssql_colname,
   &dbrelay_mssql_coltype,
   &dbrelay_mssql_collen,
   &dbrelay_mssql_colprec,
   &dbrelay_mssql_colscale,
   &dbrelay_mssql_fetch_row,
   &dbrelay_mssql_colvalue,
   &dbrelay_mssql_error,
   &dbrelay_mssql_catalogsql,
   &dbrelay_mssql_isalive
};

int dbrelay_mssql_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
int dbrelay_mssql_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);


/* I'm not particularly happy with this, in order to return a detailed message 
 * from the msg handler, we have to use a static buffer because there is no
 * dbproc to dbsetuserdata() on.  This will go away when we change out the 
 * dblib API.
 */
static char login_error[500];
static int login_msgno;

void dbrelay_mssql_init()
{
   dberrhandle(dbrelay_mssql_err_handler);
   dbmsghandle(dbrelay_mssql_msg_handler);
}
void *dbrelay_mssql_connect(dbrelay_request_t *request)
{
   char tmpbuf[30];
   int len; 
   mssql_db_t *mssql = (mssql_db_t *) malloc(sizeof(mssql_db_t));
   memset(mssql, 0, sizeof(mssql_db_t));

   dberrhandle(dbrelay_mssql_err_handler);
   dbmsghandle(dbrelay_mssql_msg_handler);

   mssql->login = dblogin();
   if (IS_SET(request->sql_password)) 
    DBSETLPWD(mssql->login, request->sql_password); 
   else
    DBSETLPWD(mssql->login, NULL);
   dbsetlname(mssql->login, request->sql_port, DBSETPORT); 
   DBSETLUSER(mssql->login, request->sql_user);
   memset(tmpbuf, '\0', sizeof(tmpbuf));
   strcpy(tmpbuf, "dbrelay (");
   len = strlen(tmpbuf);
   if (IS_SET(request->connection_name)) {
      strncat(tmpbuf, request->connection_name, sizeof(tmpbuf) - len - 3);
   } else {
      strncat(tmpbuf, request->remote_addr, sizeof(tmpbuf) - len - 3);
   }
   strcat(tmpbuf, ")");
   DBSETLAPP(mssql->login, tmpbuf);
 
   mssql->dbproc = dbopen(mssql->login, request->sql_server);
   if (!mssql->dbproc) {
      free(mssql); 
      return NULL;
   }
   dbsetuserdata(mssql->dbproc, (BYTE *)request);

   //conn->db = (void *) mssql;
   //conn->login = mssql->login;
   //conn->dbproc = mssql->dbproc;
   return (void *) mssql;
}
void dbrelay_mssql_close(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;

   dbrelay_mssql_free_results(db);

   if (mssql->dbproc) dbclose(mssql->dbproc);
   if (mssql->login) dbloginfree(mssql->login);
   if (mssql) free(mssql);
}
void dbrelay_mssql_assign_request(void *db, dbrelay_request_t *request)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   dbsetuserdata(mssql->dbproc, (BYTE *) request);
}
int dbrelay_mssql_is_quoted(void *db, int colnum)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   int coltype = dbcoltype(mssql->dbproc, colnum);

   if (coltype == SYBVARCHAR ||
       coltype == SYBCHAR ||
       coltype == SYBTEXT ||
       coltype == SYBDATETIMN ||
       coltype == SYBDATETIME ||
       coltype == SYBDATETIME4 || 
       coltype == SYBBINARY || 
       coltype == SYBVARBINARY || 
       coltype == SYBUNIQUE) 
          return 1;
   else return 0;
}
static char *dbrelay_mssql_get_sqltype_string(char *dest, int coltype, int collen)
{
	switch (coltype) {
		case SYBVARCHAR : 
			sprintf(dest, "varchar");
			break;
		case SYBCHAR : 
			sprintf(dest, "char");
			break;
		case SYBINT4 : 
		case SYBINT2 : 
		case SYBINT1 : 
		case SYBINTN : 
			if (collen==1)
				sprintf(dest, "tinyint");
			else if (collen==2)
				sprintf(dest, "smallint");
			else if (collen==4)
				sprintf(dest, "int");
			break;
		case SYBFLT8 : 
		case SYBREAL : 
		case SYBFLTN : 
			if (collen==4) 
			    sprintf(dest, "real");
			else if (collen==8) 
			    sprintf(dest, "float");
			break;
		case SYBMONEY : 
			    sprintf(dest, "money");
			break;
		case SYBMONEY4 : 
			    sprintf(dest, "smallmoney");
			break;
		case SYBIMAGE : 
			    sprintf(dest, "image");
			break;
		case SYBTEXT : 
			    sprintf(dest, "text");
			break;
		case SYBBIT : 
			    sprintf(dest, "bit");
			break;
		case SYBDATETIME4 : 
		case SYBDATETIME : 
		case SYBDATETIMN : 
			if (collen==4)
				sprintf(dest, "smalldatetime");
			else if (collen==8)
				sprintf(dest, "datetime");
			break;
		case SYBNUMERIC : 
			sprintf(dest, "numeric");
			break;
		case SYBDECIMAL : 
			sprintf(dest, "decimal");
			break;
		case SYBUNIQUE : 
			sprintf(dest, "guid");
			break;
		case SYBBINARY : 
			sprintf(dest, "binary");
			break;
		case SYBVARBINARY : 
			sprintf(dest, "varbinary");
			break;
		default : 
			sprintf(dest, "unknown type %d", coltype);
			break;
	}
	return dest;
}
static unsigned char dbrelay_mssql_has_length(int coltype)
{
	if (coltype==SYBVARCHAR || 
            coltype==SYBCHAR ||
	    coltype==SYBVARBINARY || 
            coltype==SYBBINARY)
		return 1;
	else
		return 0;
}
static unsigned char dbrelay_mssql_has_prec(int coltype)
{
	if (coltype==SYBDECIMAL || coltype==SYBNUMERIC)
		return 1;
	else
		return 0;
}
int dbrelay_mssql_connected(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;

   if (!mssql || mssql->dbproc==NULL) return FALSE;
   return TRUE;
}
int dbrelay_mssql_change_db(void *db, char *database)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   RETCODE rc;

   rc = dbuse(mssql->dbproc, database);
   if (rc!=SUCCEED) 
      return FALSE;
   return TRUE;
}
int dbrelay_mssql_exec(void *db, char *sql)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   RETCODE rc;

   //fprintf(stderr, "sql = %s\n", sql);
   rc = dbcmd(mssql->dbproc, sql);
   rc = dbsqlexec(mssql->dbproc);

   if (rc!=SUCCEED) 
      return FALSE;

   return TRUE;
}
int dbrelay_mssql_rowcount(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;

   return dbcount(mssql->dbproc);
}
int dbrelay_mssql_free_results(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   int i;

   for (i=0; i<MSSQL_MAX_COLUMNS; i++) {
      if (mssql->colval[i]!=NULL) {
         free(mssql->colval[i]);
         mssql->colval[i]=NULL;
      }
   }
}
int dbrelay_mssql_has_results(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   int colnum;
   int colsize;
   int numcols;
   RETCODE rc;

   dbrelay_mssql_free_results(db);

   if ((rc = dbresults(mssql->dbproc)) == NO_MORE_RESULTS) return FALSE;

   numcols = dbnumcols(mssql->dbproc);
   for (colnum=1; colnum<=numcols; colnum++) {
      colsize = dbrelay_mssql_collen(db, colnum);
      mssql->colval[colnum-1] = malloc(colsize > 256 ? colsize + 1 : 256);
      mssql->colval[colnum-1][0] = '\0';
      dbbind(mssql->dbproc, colnum, NTBSTRINGBIND, 0, (BYTE *) mssql->colval[colnum-1]);
      dbnullbind(mssql->dbproc, colnum, (DBINT *) &(mssql->colnull[colnum-1]));
   }
   return TRUE;
}
int dbrelay_mssql_numcols(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   return dbnumcols(mssql->dbproc);
}
char *dbrelay_mssql_colname(void *db, int colnum)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   return dbcolname(mssql->dbproc, colnum);
}
void dbrelay_mssql_coltype(void *db, int colnum, char *dest)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   dbrelay_mssql_get_sqltype_string(dest, dbcoltype(mssql->dbproc, colnum), dbcollen(mssql->dbproc, colnum));
}
int dbrelay_mssql_collen(void *db, int colnum)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   return dbcollen(mssql->dbproc, colnum);
}
int dbrelay_mssql_colprec(void *db, int colnum)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   DBTYPEINFO *typeinfo;

   if (dbrelay_mssql_has_prec(dbcoltype(mssql->dbproc, colnum))) {
       typeinfo = dbcoltypeinfo(mssql->dbproc, colnum);
       return typeinfo->precision;
    }
    return 0;
}
int dbrelay_mssql_colscale(void *db, int colnum)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   DBTYPEINFO *typeinfo;

   if (dbrelay_mssql_has_prec(dbcoltype(mssql->dbproc, colnum))) {
       typeinfo = dbcoltypeinfo(mssql->dbproc, colnum);
       return typeinfo->scale;
   }
   return 0; 
}
int dbrelay_mssql_fetch_row(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   if (dbnextrow(mssql->dbproc)!=NO_MORE_ROWS) return TRUE; 
   return FALSE;
}
char *dbrelay_mssql_colvalue(void *db, int colnum, char *dest)
{
   mssql_db_t *mssql = (mssql_db_t *) db;

   if (mssql->colnull[colnum-1]==-1) return NULL;

   strcpy(dest, mssql->colval[colnum-1]);
   return dest;
}

int
dbrelay_mssql_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
   if (dbproc!=NULL) {
      if (msgno==5701 || msgno==5703 || msgno==5704) return 0;

      dbrelay_request_t *request = (dbrelay_request_t *) dbgetuserdata(dbproc);
      if (request!=NULL) {
         if (IS_SET(msgtext)) 
            strcat(request->error_message, "\n");
         strcat(request->error_message, msgtext);
      } else {
         login_msgno = msgno;
         strcpy(login_error, msgtext);
      }
   }

   return 0;
}
int
dbrelay_mssql_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
   //db_error = strdup(dberrstr);
   if (dbproc!=NULL) {
      //dbrelay_request_t *request = (dbrelay_request_t *) dbgetuserdata(dbproc);
      //strcat(request->error_message, dberrstr);
   }

   return INT_CANCEL;
}
char *dbrelay_mssql_error(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   
   if (mssql && mssql->dbproc) {
      dbrelay_request_t *request = (dbrelay_request_t *) dbgetuserdata(mssql->dbproc);
      if (request!=NULL) {
         return request->error_message;
      }
      return NULL;
   } else {
      return login_error;
   }
}
char *dbrelay_mssql_catalogsql(int dbcmd, char **params)
{
   char *sql;
   char *columns_mask = "SELECT COLUMN_NAME,IS_NULLABLE,DATA_TYPE,CHARACTER_MAXIMUM_LENGTH FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_NAME = '%s'";
   char *pkey_mask = "SELECT c.COLUMN_NAME \
FROM  INFORMATION_SCHEMA.TABLE_CONSTRAINTS pk , \
INFORMATION_SCHEMA.KEY_COLUMN_USAGE c \
WHERE pk.TABLE_NAME = '%s' \
AND   CONSTRAINT_TYPE = 'PRIMARY KEY' \
AND   c.TABLE_NAME = pk.TABLE_NAME \
AND   c.CONSTRAINT_NAME = pk.CONSTRAINT_NAME";

   switch (dbcmd) {
      case DBRELAY_DBCMD_BEGIN:
         return strdup("BEGIN TRAN");
         break;
      case DBRELAY_DBCMD_COMMIT:
         return strdup("COMMIT TRAN");
         break;
      case DBRELAY_DBCMD_ROLLBACK:
         return strdup("ROLLBACK TRAN");
         break;
      case DBRELAY_DBCMD_TABLES:
         return strdup("SELECT * FROM INFORMATION_SCHEMA.tables WHERE TABLE_TYPE='BASE TABLE'");
         break;
      case DBRELAY_DBCMD_COLUMNS:
         sql = malloc(strlen(columns_mask) + strlen(params[0]));
         sprintf(sql, columns_mask, params[0]);
         return sql;
         break;
      case DBRELAY_DBCMD_PKEY: 
         sql = malloc(strlen(pkey_mask) + strlen(params[0]));
         sprintf(sql, pkey_mask, params[0]);
         return sql;
         break;
   }
   return NULL;
}
int dbrelay_mssql_isalive(void *db)
{
   mssql_db_t *mssql = (mssql_db_t *) db;
   
   return !DBDEAD(mssql->dbproc);
}
