/*
 *  * Copyright (C) Getco LLC
 *   */

#ifndef _DBRELAYDMYSQL_H_INCLUDED_
#define _DBRELAYMYSQL_H_INCLUDED_

#include "sql.h"
#include "sqlext.h"
#include "dbrelay.h"

#define TRUE 1
#define FALSE 0

typedef struct odbc_db_s {
   SQLHENV env;
   SQLHDBC dbc;
   SQLHSTMT stmt;
   unsigned char querying;
   char tmpbuf[256];
} odbc_db_t;

void dbrelay_odbc_init();
void *dbrelay_odbc_connect(dbrelay_request_t *request);
void dbrelay_odbc_close(void *db);
void dbrelay_odbc_assign_request(void *db, dbrelay_request_t *request);
int dbrelay_odbc_is_quoted(void *db, int colnum);;
int dbrelay_odbc_connected(void *db);
int dbrelay_odbc_change_db(void *db, char *database);
int dbrelay_odbc_exec(void *db, char *sql);
int dbrelay_odbc_rowcount(void *db);
int dbrelay_odbc_has_results(void *db);
int dbrelay_odbc_numcols(void *db);
char *dbrelay_odbc_colname(void *db, int colnum);
void dbrelay_odbc_coltype(void *db, int colnum, char *dest);
int dbrelay_odbc_collen(void *db, int colnum);
int dbrelay_odbc_colprec(void *db, int colnum);
int dbrelay_odbc_colscale(void *db, int colnum);
int dbrelay_odbc_fetch_row(void *db);
char *dbrelay_odbc_colvalue(void *db, int colnum, char *dest);
char *dbrelay_odbc_error(void *db);

#endif
