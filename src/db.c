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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See 
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

#include "dbrelay.h"
#include "stringbuf.h"
#include "../include/dbrelay_config.h"

#ifdef HAVE_FREETDS
extern dbrelay_dbapi_t dbrelay_mssql_api;
dbrelay_dbapi_t *api = &dbrelay_mssql_api;
#endif

#ifdef HAVE_MYSQL
extern dbrelay_dbapi_t dbrelay_mysql_api;
dbrelay_dbapi_t *api = &dbrelay_mysql_api;
#endif

#ifdef HAVE_ODBC
extern dbrelay_dbapi_t dbrelay_odbc_api;
dbrelay_dbapi_t *api = &dbrelay_odbc_api;
#endif

#define TRUE 1
#define FALSE 0

static int dbrelay_find_placeholder(char *sql);

u_char *
dbrelay_exec_query(dbrelay_connection_t *conn, dbrelay_request_t *request, char *sql)
{
  u_char *ret;
 
  api->change_db(conn->db, request->sql_database);

  if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_BEGIN, NULL));

  if (api->exec(conn->db, sql))
  {
     ret = (u_char *) request->emitapi->fill(conn, request->flags);
     if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_COMMIT, NULL));
  } else {
     if (request->flags & DBRELAY_FLAG_XACT) api->exec(conn->db, api->catalogsql(DBRELAY_DBCMD_ROLLBACK, NULL));
     return NULL;
  }

  return ret;
}

void
dbrelay_copy_string(char *dest, char *src, int sz)
{
   if (strlen(src) < (unsigned int) sz) strcpy(dest, src);
   else {
      strncpy(dest, src, sz - 1);
      dest[sz-1]='\0';
   }
}
static int
is_quoted_param(char *param)
{
   int ret;
   char *tmp = strdup(param);
   char *s = strstr(tmp, ":");
   *s = '\0';
   if (!strcasecmp(tmp, "char") ||
       !strcasecmp(tmp, "varchar") ||
       !strcasecmp(tmp, "datetime") ||
       !strcasecmp(tmp, "smalldatetime"))
      ret = TRUE;
   else ret = FALSE;
   free(tmp);
   return ret;
}
char *
dbrelay_resolve_params(dbrelay_request_t *request, char *sql)
{
   int i = 0;
   int pos = 0, prevpos = 0;
   stringbuf_t *sb = sb_new(NULL);
   char *ret;
   char *tmpsql = strdup(sql);

   if (IS_SET(DBRELAY_MAGIC) && !(request->flags & DBRELAY_FLAG_NOMAGIC)) {
      sb_append(sb, DBRELAY_MAGIC);
   }
   while (request->params[i]) {
      prevpos = pos;
      pos += dbrelay_find_placeholder(&tmpsql[pos]);
      if (pos==-1) {
	 // ignore missing placeholders
         pos = prevpos;	
      } else {
         tmpsql[pos]='\0';
         sb_append(sb, &tmpsql[prevpos]);
         if (is_quoted_param(request->params[i])) sb_append(sb, "'");
         sb_append(sb, strstr(request->params[i], ":") + 1);
         if (is_quoted_param(request->params[i])) sb_append(sb, "'");
         pos++;
      }
      i++;
   } 
   sb_append(sb, &tmpsql[pos]);
   ret = sb_to_char(sb);
   free(tmpsql);
   sb_free(sb);
   dbrelay_log_debug(request, "new sql %s", ret);
   return ret;
}
static int
dbrelay_find_placeholder(char *sql)
{
   int quoted = 0;
   int i = 0;
   int found = 0;
   int len = strlen(sql);

   do {
     if (sql[i]=='\'') quoted = quoted ? 0 : 1;
     if (!quoted && sql[i]=='?') found = 1;
     i++;
   } while (!found && i<len);
   if (!found) return -1;
   else return i-1;
}
int
dbrelay_check_request(dbrelay_request_t *request)
{
   if (!request->sql && !request->cmd) return 0;
   if (!request->sql) return 0;
   if (!IS_SET(request->sql_server)) return 0;
   if (!IS_SET(request->sql_user)) return 0;
   return 1;
} 
