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

#include <stdio.h>
#include "json.h"
#include "../include/config.h"

int json_mem_exceeded(json_t *json)
{
   if (DBRELAY_MAX_MEMUSAGE && json->sb->block_count > DBRELAY_MAX_MEMUSAGE) return 1;
   return 0;
}
json_t *json_new()
{
   json_t *json = (json_t *) malloc(sizeof(json_t));
   memset(json, 0, sizeof(json_t));
   json->sb = sb_new(NULL);

   return json;
}
void json_pretty_print(json_t *json, unsigned char pp)
{
  json->prettyprint = pp; 
}
void json_free(json_t *json)
{
   json_node_t *node = json->stack;
   json_node_t *prev;

   while (node) {
	prev = node;
	node = node->next;
	free(prev);
   }
   sb_free(json->sb);
   free(json);
}
char *json_to_string(json_t *json)
{
   return sb_to_char(json->sb);
}
static void json_tab(json_t *json)
{
   int i;

   if (!json->prettyprint) return;

   for(i=0; i<json->tab_level; i++) 
   {
      sb_append(json->sb, "   ");
   }
}
void json_new_object(json_t *json)
{ 
   if (!json->pending) {
      if (json->stack && json->stack->num_items) {
          sb_append(json->sb, ",");
          if (json->prettyprint) sb_append(json->sb, "\n");
      }
      json_tab(json);
   }
   sb_append(json->sb, "{");
   if (json->prettyprint) sb_append(json->sb, "\n");
   json->tab_level++;

   if (json->stack) json->stack->num_items++;
   json_push(json, OBJECT);
   json->pending = 0;
}
void json_end_object(json_t *json)
{
   json_node_t *node;

   json->tab_level--;
   if (json->prettyprint) sb_append(json->sb, "\n");
   json_tab(json);

   sb_append(json->sb, "}");
   node = json->stack;
   if (node) {
   	json->stack = node->next;
   	free(node);
   }
}

void json_new_array(json_t *json)
{
   if (!json->pending) {
      if (json->stack && json->stack->num_items) {
          sb_append(json->sb, ",");
          if (json->prettyprint) sb_append(json->sb, "\n");
      }
      json_tab(json);
   }
   sb_append(json->sb, "[");
   if (json->prettyprint) sb_append(json->sb, "\n");
   json->tab_level++;

   if (json->stack) json->stack->num_items++;
   json_push(json, ARRAY);
   json->pending = 0;
}
void json_end_array(json_t *json)
{
   json_node_t *node;

   json->tab_level--;
   if (json->prettyprint) sb_append(json->sb, "\n");
   json_tab(json);

   sb_append(json->sb, "]");
   node = json->stack;
   if (node) {
   	json->stack = node->next;
   	free(node);
   }
}

void json_add_value(json_t *json, char *value)
{
   if (json->stack && json->stack->num_items) {
      if (!json->pending) sb_append(json->sb, ",");
   }
   sb_append(json->sb, value);
   if (json->stack) json->stack->num_items++;
}
void json_add_key(json_t *json, char *key)
{
   json_node_t *node = json->stack;
   if (node) {
      if (node->num_items) {
         if (!json->pending) sb_append(json->sb, ", ");
      } else {
         json_tab(json);
      }
      node->num_items++;
   }
   sb_append(json->sb, "\"");
   sb_append(json->sb, key);
   sb_append(json->sb, "\"");
   if (json->prettyprint) sb_append(json->sb, " ");
   sb_append(json->sb, ":");
   if (json->prettyprint) sb_append(json->sb, " ");
   json->pending = 1;
}
void json_add_number(json_t *json, char *key, char *value)
{
   json_add_key(json, key);
   sb_append(json->sb, value);
   json->pending = 0;
}
static int is_printable(char c)
{
   if (c<' ' || c=='\"' || c=='\\') return 0;
   else return 1;
}
static void append_nonprintable(stringbuf_t *sb, char c)
{
   char buf[7]; /* '\u1234' and null */

   switch (c) {
      case '\"': sb_append(sb, "\\\""); break;
      case '\\': sb_append(sb, "\\\\"); break;
      case '/': sb_append(sb, "\\/"); break;
      case '\b': sb_append(sb, "\\b"); break;
      case '\f': sb_append(sb, "\\f"); break;
      case '\n': sb_append(sb, "\\n"); break;
      case '\r': sb_append(sb, "\\r"); break;
      case '\t': sb_append(sb, "\\t"); break;
      default: 
         sprintf(buf, "\\u%02x%02x", 0, (unsigned char) c);
         sb_append(sb, buf); 
      break;
   }
}
void json_add_null(json_t *json, char *key)
{
   json_add_key(json, key);
   sb_append(json->sb, "null");
   json->pending = 0;
}
void json_add_string(json_t *json, char *key, char *value)
{
   char *s, *first, *tmp;
   char c;
   
   tmp = strdup(value);

   json_add_key(json, key);
   sb_append(json->sb, "\"");
   for (s=tmp, first=tmp; *s; s++) {
      if (!is_printable(*s)) {
         c = *s;
         *s='\0';
         sb_append(json->sb, first);
         append_nonprintable(json->sb, c);
         first=s+1;	
      }
   }
   sb_append(json->sb, first);
   sb_append(json->sb, "\"");
   json->pending = 0;

   free(tmp);
}
void json_add_elem(json_t *json, char *value)
{
   char *s, *first, *tmp;
   char c;
   
   tmp = strdup(value);

   sb_append(json->sb, "\"");
   for (s=tmp, first=tmp; *s; s++) {
      if (!is_printable(*s)) {
         c = *s;
         *s='\0';
         sb_append(json->sb, first);
         append_nonprintable(json->sb, c);
         first=s+1;	
      }
   }
   sb_append(json->sb, first);
   sb_append(json->sb, "\"");
   json->pending = 0;

   free(tmp);
}
void json_add_json(json_t *json, char *value)
{
   sb_append(json->sb, value);
}

void json_push(json_t *json, int node_type)
{
   json_node_t *node = (json_node_t *) malloc(sizeof(json_node_t));
   memset(node, 0, sizeof(json_node_t));
   node->node_type = node_type;
   node->next = json->stack;
   json->stack = node;
}

void json_add_callback(json_t *json, char *value)
{
   sb_append(json->sb, value);
   sb_append(json->sb, "(");
}
void json_end_callback(json_t *json)
{
   sb_append(json->sb, ");");
}
void json_set_mode(json_t *json, unsigned char mode)
{
   json->mode = mode;
}
unsigned char json_get_mode(json_t *json)
{
   return json->mode;
}
/*
main()
{
   char *buf;
   json_t *json = json_new();
   json_pretty_print(json, 1);

   json_new_array(json);
   json_new_array(json);
   json_new_object(json);
   json_add_string(json, "id", "1");
   json_add_string(json, "name", "name 1");
   json_end_object(json);
   json_new_object(json);
   json_add_string(json, "id", "2");
   json_add_string(json, "name", "name 2");
   json_end_object(json);
   json_end_array(json);
   json_new_array(json);
   json_add_string(json, "company", "XYZ");
   json_add_number(json, "port", "80");
   json_end_array(json);
   json_new_object(json);
   json_add_key(json, "request");
   json_new_object(json);
   json_add_string(json, "company", "ABC");
   json_add_string(json, "category", "SMALLCAP");
   json_end_object(json);
   json_add_number(json, "count", "3");
   json_end_object(json);
   json_end_array(json);

   buf = json_to_string(json);
   printf("%s\n", buf);
   json_free(json);
}
*/
