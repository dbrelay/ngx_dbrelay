/*
 *  * Copyright (C) Getco LLC
 *   */

#ifndef _DBRELAYDMYSQL_H_INCLUDED_
#define _DBRELAYMYSQL_H_INCLUDED_

#include "mysql.h"
#include "dbrelay.h"

#define TRUE 1
#define FALSE 0

typedef struct mysql_db_s {
   MYSQL *mysql;
   MYSQL_RES *result;
   MYSQL_ROW row;
   MYSQL_FIELD *field;
} mysql_db_t;

void dbrelay_mysql_init();
void *dbrelay_mysql_connect(dbrelay_request_t *request);
void dbrelay_mysql_close(void *db);
void dbrelay_mysql_assign_request(void *db, dbrelay_request_t *request);
int dbrelay_mysql_is_quoted(void *db, int colnum);;
int dbrelay_mysql_connected(void *db);
int dbrelay_mysql_change_db(void *db, char *database);
int dbrelay_mysql_exec(void *db, char *sql);
int dbrelay_mysql_rowcount(void *db);
int dbrelay_mysql_has_results(void *db);
int dbrelay_mysql_numcols(void *db);
char *dbrelay_mysql_colname(void *db, int colnum);
void dbrelay_mysql_coltype(void *db, int colnum, char *dest);
int dbrelay_mysql_collen(void *db, int colnum);
int dbrelay_mysql_colprec(void *db, int colnum);
int dbrelay_mysql_colscale(void *db, int colnum);
int dbrelay_mysql_fetch_row(void *db);
char *dbrelay_mysql_colvalue(void *db, int colnum, char *dest);
char *dbrelay_mysql_error(void *db);

#endif
