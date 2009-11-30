
/*
 * Copyright (C) Getco LLC
 */

#include "stringbuf.h"
#include "vodbc.h"

#define IS_SET(x) (x && strlen(x)>0)

static void dbrelay_odbc_get_error(void *db);

dbrelay_dbapi_t dbrelay_odbc_api = 
{
   &dbrelay_odbc_init,
   &dbrelay_odbc_connect,
   &dbrelay_odbc_close,
   &dbrelay_odbc_assign_request,
   &dbrelay_odbc_is_quoted,
   &dbrelay_odbc_connected,
   &dbrelay_odbc_change_db,
   &dbrelay_odbc_exec,
   &dbrelay_odbc_rowcount,
   &dbrelay_odbc_has_results,
   &dbrelay_odbc_numcols,
   &dbrelay_odbc_colname,
   &dbrelay_odbc_coltype,
   &dbrelay_odbc_collen,
   &dbrelay_odbc_colprec,
   &dbrelay_odbc_colscale,
   &dbrelay_odbc_fetch_row,
   &dbrelay_odbc_colvalue,
   &dbrelay_odbc_error,
   &dbrelay_odbc_catalogsql,
   &dbrelay_odbc_isalive
};

void dbrelay_odbc_init()
{
}
void *dbrelay_odbc_connect(dbrelay_request_t *request)
{
   odbc_db_t *odbc = (odbc_db_t *)malloc(sizeof(odbc_db_t));
   SQLRETURN ret;
   SQLCHAR sqlstate[6];
   SQLCHAR message[255];
   SQLINTEGER errnum;

   SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc->env);
   SQLSetEnvAttr(odbc->env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
   SQLAllocHandle(SQL_HANDLE_DBC, odbc->env, &odbc->dbc);

   ret = SQLConnect(odbc->dbc, (SQLCHAR *) request->sql_server, SQL_NTS, (SQLCHAR *) request->sql_user, SQL_NTS, (SQLCHAR *) request->sql_password, SQL_NTS);

   if (! SQL_SUCCEEDED(ret)) {
      SQLGetDiagRec(SQL_HANDLE_DBC, odbc->dbc, 1, sqlstate, &errnum, message, sizeof(message), NULL);
      //fprintf(stderr, "sqlstate %s %ld (%s)\n", sqlstate, errnum, message);
      if (odbc->dbc) SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc);
      if (odbc->env) SQLFreeHandle(SQL_HANDLE_ENV, odbc->env);
      free(odbc);
      return NULL;
   }

   return ((void *) odbc);
}
void dbrelay_odbc_close(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;

   if (odbc->stmt) SQLFreeHandle(SQL_HANDLE_STMT, odbc->stmt);
   if (odbc->dbc) {
      SQLDisconnect(odbc->dbc);
      SQLFreeHandle(SQL_HANDLE_DBC, odbc->dbc);
   }
   if (odbc->env) SQLFreeHandle(SQL_HANDLE_ENV, odbc->env);
   odbc->stmt=NULL;
   odbc->dbc=NULL;
   odbc->env=NULL;
}
void dbrelay_odbc_assign_request(void *db, dbrelay_request_t *request)
{
}
int dbrelay_odbc_is_quoted(void *db, int colnum)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLSMALLINT coltype;
   
   SQLDescribeCol(odbc->stmt, colnum, NULL, 0, NULL, &coltype, NULL, NULL, NULL); 

   switch (coltype) {
      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
      case SQL_DATE:
      case SQL_TIME:
      case SQL_TIMESTAMP:
         return 1;
         break;
      case SQL_DECIMAL:
      case SQL_NUMERIC:
      case SQL_SMALLINT:
      case SQL_INTEGER:
      case SQL_REAL:
      case SQL_FLOAT:
      case SQL_DOUBLE:
      case SQL_BIT:
      case SQL_TINYINT:
      case SQL_BIGINT:
      case SQL_BINARY:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
      default:
         return 0;
         break;
   }
}
char *dbrelay_odbc_get_sqltype_string(char *dest, int coltype, int collen)
{
   switch (coltype) {
      case SQL_CHAR:
         sprintf(dest, "char");
         break;
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
         sprintf(dest, "varchar");
         break;
      case SQL_WCHAR:
         sprintf(dest, "wchar");
         break;
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
         sprintf(dest, "wvarchar");
         break;
      case SQL_DATE:
         sprintf(dest, "date");
         break;
      case SQL_TIME:
         sprintf(dest, "time");
         break;
      case SQL_TIMESTAMP:
         sprintf(dest, "timestamp");
         break;
      case SQL_DECIMAL:
         sprintf(dest, "decimal");
         break;
      case SQL_NUMERIC:
         sprintf(dest, "numeric");
         break;
      case SQL_SMALLINT:
         sprintf(dest, "smallint");
         break;
      case SQL_INTEGER:
         sprintf(dest, "integer");
         break;
      case SQL_REAL:
         sprintf(dest, "real");
         break;
      case SQL_FLOAT:
         sprintf(dest, "float");
         break;
      case SQL_DOUBLE:
         sprintf(dest, "double");
         break;
      case SQL_BIT:
         sprintf(dest, "bit");
         break;
      case SQL_TINYINT:
         sprintf(dest, "tinyint");
         break;
      case SQL_BIGINT:
         sprintf(dest, "bigint");
         break;
      case SQL_BINARY:
         sprintf(dest, "binary");
         break;
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
         sprintf(dest, "varbinary");
         break;
      default:
         sprintf(dest, "unknown");
         break;
   }
   return dest;
}
unsigned char dbrelay_odbc_has_length(int coltype)
{
   switch (coltype) {
      case SQL_CHAR:
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      case SQL_WCHAR:
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
         return 1;
    }
    return 0;
}
unsigned char dbrelay_odbc_has_prec(int coltype)
{
	if (coltype==SQL_DECIMAL || coltype==SQL_NUMERIC) 
		return 1;
	else
		return 0;
}
int dbrelay_odbc_connected(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;

   if (!odbc || odbc->dbc==NULL) return FALSE;
   return TRUE;
}
int dbrelay_odbc_change_db(void *db, char *database)
{
   return TRUE;
}
int dbrelay_odbc_exec(void *db, char *sql)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLRETURN ret;

   SQLAllocHandle(SQL_HANDLE_STMT, odbc->dbc, &odbc->stmt);

   ret = SQLExecDirect(odbc->stmt, (SQLCHAR *) sql, SQL_NTS);
   odbc->querying = 1;

   if (SQL_SUCCEEDED(ret)) return TRUE;
   else {
      dbrelay_odbc_get_error(db);
      return FALSE;
   }
}
int dbrelay_odbc_rowcount(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLRETURN ret;
   SQLLEN rowcount;

   ret = SQLRowCount(odbc->stmt, &rowcount);
   
   return rowcount;
}
int dbrelay_odbc_has_results(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLRETURN ret;

   if (odbc->querying) {
      odbc->querying = 0;
      return TRUE;
   }

   ret = SQLMoreResults(odbc->stmt);
   if (ret==SQL_NO_DATA) return FALSE;
   return TRUE;
}
int dbrelay_odbc_numcols(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLRETURN ret;
   SQLSMALLINT numcols;

   ret = SQLNumResultCols(odbc->stmt, &numcols);
   return numcols;
}
char *dbrelay_odbc_colname(void *db, int colnum)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLSMALLINT namelen;
   char tmpbuf[256];

   SQLDescribeCol(odbc->stmt, colnum, (SQLCHAR *) odbc->tmpbuf, 256, &namelen, NULL, NULL, NULL, NULL); 
   tmpbuf[namelen]='\0';

   return odbc->tmpbuf;
}
void dbrelay_odbc_coltype(void *db, int colnum, char *dest)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLSMALLINT coltype;
   SQLULEN collen;
   
   SQLDescribeCol(odbc->stmt, colnum, NULL, 0, NULL, &coltype, &collen, NULL, NULL); 
   dbrelay_odbc_get_sqltype_string(dest, coltype, collen);
}
int dbrelay_odbc_collen(void *db, int colnum)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLULEN collen;

   SQLDescribeCol(odbc->stmt, colnum, NULL, 0, NULL, NULL, &collen, NULL, NULL); 

   return collen;
}
int dbrelay_odbc_colprec(void *db, int colnum)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLSMALLINT coltype;
   SQLULEN collen;
   
   SQLDescribeCol(odbc->stmt, colnum, NULL, 0, NULL, &coltype, &collen, NULL, NULL); 
   if (dbrelay_odbc_has_prec(coltype)) {
       return collen;
    }
    return 0;
}
int dbrelay_odbc_colscale(void *db, int colnum)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLSMALLINT digits;

   SQLDescribeCol(odbc->stmt, colnum, NULL, 0, NULL, NULL, NULL, &digits, NULL); 
   return digits;
}
int dbrelay_odbc_fetch_row(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLRETURN ret;

   ret = SQLFetch(odbc->stmt);
   if (SQL_SUCCEEDED(ret)) return TRUE;
   return FALSE;
}
char *dbrelay_odbc_colvalue(void *db, int colnum, char *dest)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLLEN null;

   SQLGetData(odbc->stmt, colnum, SQL_C_CHAR, dest, 256, &null);
   if (null==SQL_NULL_DATA) return NULL;

   return dest;
}

static void dbrelay_odbc_get_error(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   SQLCHAR sqlstate[6];
   SQLINTEGER errnum;

   SQLGetDiagRec(SQL_HANDLE_STMT, odbc->stmt, 1, sqlstate, &errnum, odbc->error_message, sizeof(odbc->error_message)-1, NULL);
}

char *dbrelay_odbc_error(void *db)
{
   odbc_db_t *odbc = (odbc_db_t *) db;
   return (char *) odbc->error_message;
}
char *dbrelay_odbc_catalogsql(int dbcmd, char **params)
{
   /* XXX - stub for now */
    return NULL;
}
int dbrelay_odbc_isalive(void *db)
{
   /* XXX - stub for now */
   return 1;
}
