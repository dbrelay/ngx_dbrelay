/*
 * Copyright (C) Getco LLC
 */

#ifndef _DBRELAYDMSSQL_H_INCLUDED_
#define _DBRELAYMSSQL_H_INCLUDED_

#include "dbrelay.h"
#include <sybdb.h>

typedef struct mssql_db_s {
    LOGINREC *login;
    DBPROCESS *dbproc;
    char colval[256][256];
    int colnull[256];
} mssql_db_t;

void dbrelay_mssql_init();
void *dbrelay_mssql_connect(dbrelay_request_t *request);
void dbrelay_mssql_close(void *db);
void dbrelay_mssql_assign_request(void *db, dbrelay_request_t *request);
int dbrelay_mssql_is_quoted(void *db, int colnum);;
int dbrelay_mssql_connected(void *db);
int dbrelay_mssql_change_db(void *db, char *database);
int dbrelay_mssql_exec(void *db, char *sql);
int dbrelay_mssql_rowcount(void *db);
int dbrelay_mssql_has_results(void *db);
int dbrelay_mssql_numcols(void *db);
char *dbrelay_mssql_colname(void *db, int colnum);
void dbrelay_mssql_coltype(void *db, int colnum, char *dest);
int dbrelay_mssql_collen(void *db, int colnum);
int dbrelay_mssql_colprec(void *db, int colnum);
int dbrelay_mssql_colscale(void *db, int colnum);
int dbrelay_mssql_fetch_row(void *db);
char *dbrelay_mssql_colvalue(void *db, int colnum, char *dest);
char *dbrelay_mssql_error(void *db);
char *dbrelay_mssql_catalogsql(int dbcmd, char **params);
int dbrelay_mssql_isalive(void *db);


#endif
