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

#include "stringbuf.h"
#include "vmysql.h"

#define IS_SET(x) (x && strlen(x)>0)

dbrelay_dbapi_t dbrelay_mysql_api = 
{
   &dbrelay_mysql_init,
   &dbrelay_mysql_connect,
   &dbrelay_mysql_close,
   &dbrelay_mysql_assign_request,
   &dbrelay_mysql_is_quoted,
   &dbrelay_mysql_connected,
   &dbrelay_mysql_change_db,
   &dbrelay_mysql_exec,
   &dbrelay_mysql_rowcount,
   &dbrelay_mysql_has_results,
   &dbrelay_mysql_numcols,
   &dbrelay_mysql_colname,
   &dbrelay_mysql_coltype,
   &dbrelay_mysql_collen,
   &dbrelay_mysql_colprec,
   &dbrelay_mysql_colscale,
   &dbrelay_mysql_fetch_row,
   &dbrelay_mysql_colvalue,
   &dbrelay_mysql_error
};

void dbrelay_mysql_init()
{
}
void *dbrelay_mysql_connect(dbrelay_request_t *request)
{
   mysql_db_t *mydb = (mysql_db_t *)malloc(sizeof(mysql_db_t));
   mydb->mysql = (MYSQL *)malloc(sizeof(MYSQL));

   if(mysql_init(mydb->mysql)==NULL) return NULL;

   if (!mysql_real_connect(mydb->mysql,request->sql_server,request->sql_user, IS_SET(request->sql_password) ? request->sql_password : NULL ,NULL,0,NULL,0)) return NULL;

   return ((void *) mydb);
}
void dbrelay_mysql_close(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   if (mydb->mysql) mysql_close(mydb->mysql);
}
void dbrelay_mysql_assign_request(void *db, dbrelay_request_t *request)
{
}
int dbrelay_mysql_is_quoted(void *db, int colnum)
{
   mysql_db_t *mydb = (mysql_db_t *) db;
   
   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   int coltype = mydb->field->type;

   if (coltype == MYSQL_TYPE_VARCHAR ||
       coltype == MYSQL_TYPE_VAR_STRING ||
       coltype == MYSQL_TYPE_DATE ||
       coltype == MYSQL_TYPE_TIME ||
       coltype == MYSQL_TYPE_DATETIME ||
       coltype == MYSQL_TYPE_BLOB ||
       coltype == MYSQL_TYPE_STRING ||
       coltype == MYSQL_TYPE_LONG_BLOB ||
       coltype == MYSQL_TYPE_TINY_BLOB ||
       coltype == MYSQL_TYPE_NEWDATE ||
       coltype == MYSQL_TYPE_MEDIUM_BLOB)
          return 1;
   else return 0;
}
static char *dbrelay_mysql_get_sqltype_string(char *dest, int coltype, int collen)
{
	switch (coltype) {
           case MYSQL_TYPE_VARCHAR :
           case MYSQL_TYPE_STRING :
           case MYSQL_TYPE_VAR_STRING :
                   sprintf(dest, "varchar");
                   break;
                   sprintf(dest, "varchar");
                   break;
           case MYSQL_TYPE_BLOB :
           case MYSQL_TYPE_LONG_BLOB :
           case MYSQL_TYPE_TINY_BLOB :
           case MYSQL_TYPE_MEDIUM_BLOB :
                   sprintf(dest, "blob");
                   break;
           case MYSQL_TYPE_DATE :
           case MYSQL_TYPE_NEWDATE :
                   sprintf(dest, "date");
                   break;
           case MYSQL_TYPE_TIME :
                   sprintf(dest, "time");
                   break;
           case MYSQL_TYPE_DATETIME :
                   sprintf(dest, "datetime");
                   break;
           case MYSQL_TYPE_DECIMAL : 
           case MYSQL_TYPE_NEWDECIMAL :
                   sprintf(dest, "decimal");
                   break;
           case MYSQL_TYPE_TINY :
                   sprintf(dest, "tinyint");
                   break;
           case MYSQL_TYPE_SHORT :
                   sprintf(dest, "shortint");
                   break;
           case MYSQL_TYPE_LONG :
                   sprintf(dest, "longint");
                   break;
           case MYSQL_TYPE_FLOAT :
                   sprintf(dest, "float");
                   break;
           case MYSQL_TYPE_DOUBLE :
                   sprintf(dest, "double");
                   break;
           case MYSQL_TYPE_NULL :
                   sprintf(dest, "null");
                   break;
           case MYSQL_TYPE_TIMESTAMP :
                   sprintf(dest, "timestamp");
                   break;
           case MYSQL_TYPE_LONGLONG :
                   sprintf(dest, "longlong");
                   break;
           case MYSQL_TYPE_INT24 :
                   sprintf(dest, "int24");
                   break;
           case MYSQL_TYPE_YEAR :
                   sprintf(dest, "year");
                   break;
           case MYSQL_TYPE_BIT :
                   sprintf(dest, "bit");
                   break;
           case MYSQL_TYPE_ENUM :
                   sprintf(dest, "enum");
                   break;
           case MYSQL_TYPE_SET :
                   sprintf(dest, "enum");
                   break;
           case MYSQL_TYPE_GEOMETRY :
                   sprintf(dest, "geometry");
                   break;
	}
	return dest;
}
static unsigned char dbrelay_mysql_has_length(int coltype)
{
	if (coltype==MYSQL_TYPE_VARCHAR ||
            coltype==MYSQL_TYPE_STRING ||
            coltype==MYSQL_TYPE_VAR_STRING)
		return 1;
	else
		return 0;
}
static unsigned char dbrelay_mysql_has_prec(int coltype)
{
	if (coltype==MYSQL_TYPE_DECIMAL || MYSQL_TYPE_NEWDECIMAL)
		return 1;
	else
		return 0;
}
int dbrelay_mysql_connected(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   if (!mydb || mydb->mysql==NULL) return FALSE;
   return TRUE;
}
int dbrelay_mysql_change_db(void *db, char *database)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   if(mysql_select_db(mydb->mysql, database)!=0) return FALSE;
   return TRUE;
}
int dbrelay_mysql_exec(void *db, char *sql)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   if(mysql_real_query(mydb->mysql, sql, strlen(sql))!=0) return FALSE;
   return TRUE;
}
int dbrelay_mysql_rowcount(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   return mysql_affected_rows(mydb->mysql);
}
int dbrelay_mysql_has_results(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->result = mysql_store_result(mydb->mysql);
   if (mydb->result) return TRUE;
   return FALSE;
}
int dbrelay_mysql_numcols(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;
   return mysql_num_fields(mydb->result);
}
char *dbrelay_mysql_colname(void *db, int colnum)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   return mydb->field->name;
}
void dbrelay_mysql_coltype(void *db, int colnum, char *dest)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   dbrelay_mysql_get_sqltype_string(dest, mydb->field->type, mydb->field->length);
}
int dbrelay_mysql_collen(void *db, int colnum)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   return mydb->field->length;
}
int dbrelay_mysql_colprec(void *db, int colnum)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   return mydb->field->max_length;
}
int dbrelay_mysql_colscale(void *db, int colnum)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   mydb->field = mysql_fetch_field_direct(mydb->result, colnum-1);
   return mydb->field->decimals;
}
int dbrelay_mysql_fetch_row(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;
   mydb->row = mysql_fetch_row(mydb->result);
   if (!mydb->row) return FALSE;
   return TRUE;
}
char *dbrelay_mysql_colvalue(void *db, int colnum, char *dest)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   if (!mydb->row[colnum-1]) return NULL;

   strcpy(dest, mydb->row[colnum-1]);
   return dest;
}

char *dbrelay_mysql_error(void *db)
{
   mysql_db_t *mydb = (mysql_db_t *) db;

   return (char *) mysql_error(mydb->mysql);
}
