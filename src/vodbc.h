/*
 * DB Relay is an HTTP module built on the NGiNX webserver platform which 
 * communicates with a variety of database servers and returns JSON formatted 
 * data.
 * 
 * Copyright (C) 2008-2010 Getco LLC
 * 
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) 
 * any later version. In addition, redistributions in source code and in binary 
 * form must 
 * include the above copyright notices, and each of the following disclaimers. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT OWNERS AND CONTRIBUTORS “AS IS” 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANY COPYRIGHT OWNERS OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
   SQLCHAR error_message[256];
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
char *dbrelay_odbc_catalogsql(int dbcmd, char **params);
int dbrelay_odbc_isalive(void *db);

#endif
