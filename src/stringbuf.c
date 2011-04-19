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
#include <stdio.h>

char *sb_to_char(stringbuf_t *string)
{
   int end = 0;
   stringbuf_node_t *node;
   char *outstr = (char *) malloc(sb_len(string) + 1);

   node = string->head;
   do {
      if (node->last) {
         strncpy(&outstr[end], node->part, node->last);
         end += node->last;
      }
      node = node->next;
   } while (node);
   outstr[end] = '\0';

   return outstr;
}

int sb_len(stringbuf_t *string)
{
   stringbuf_node_t *node;
   int len = 0;

   node = string->head;
   do {
      len += node->last + 1;
      node = node->next;
   } while (node);

   return len;
}

void sb_free(stringbuf_t *string)
{
   stringbuf_node_t *node;
   stringbuf_node_t *prev;

   if (!string) return;

   node = string->head;
   do {
      prev = node;
      node = node->next;
      free(prev);
   } while (node);
   free(string);
}

stringbuf_t *sb_new(char *s)
{
   stringbuf_t *string = (stringbuf_t *) malloc(sizeof(stringbuf_t));
   memset(string, 0, sizeof(stringbuf_t));
   stringbuf_node_t *new_node = (stringbuf_node_t *) malloc(sizeof(stringbuf_node_t));
   memset(new_node, 0, sizeof(stringbuf_node_t));
   string->head = new_node;
   string->tail = new_node;
   string->block_count = 1;
   if (s) sb_append(string, s);

   return string;
}

stringbuf_node_t *sb_append(stringbuf_t *string, char *s)
{
   stringbuf_node_t *new_node;
   stringbuf_node_t *node = string->tail;
   int len = strlen(s);
   int left = 0, start = 0;
 
   while (node->last + len > SB_BLOCK_SIZE)
   { 
      left = SB_BLOCK_SIZE - node->last;
      if (left) 
      {
         //printf("copying %d bytes from %d to %d\n", left, start, node->last);
         memcpy(&node->part[node->last], &s[start], left);
         node->last = SB_BLOCK_SIZE;
         len -= left;
         start += left;
      }
      new_node = malloc(sizeof(stringbuf_node_t));
      memset(new_node, 0, sizeof(stringbuf_node_t));
      string->block_count++;
      node->next = new_node;
      string->tail = new_node;
      node = new_node;
   }
   //printf("copying %d bytes from %d to %d. ", len, start, node->last);
   memcpy(&node->part[node->last], &s[start], len);
   node->last+=len;
   //printf("last now %d\n", node->last);

   return string->tail;
}

/*
main() {
   stringbuf_t *sb = sb_new("first\n");
   sb_append(sb, "second\n");
   sb_append(sb, "third\n");
   sb_append(sb, "fourth\n");
   sb_append(sb, "fifth\n");
   sb_append(sb, "sixth\n");
   printf("%s", sb_to_char(sb));
}
*/
