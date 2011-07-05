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

#ifndef _JSON_H_INCLUDED_
#define _JSON_H_INCLUDED_

#include <stdio.h>
#include "stringbuf.h"

#define ARRAY 0
#define OBJECT 1

#define DBRELAY_JSON_MODE_STD 0
#define DBRELAY_JSON_MODE_CSV 1

typedef struct json_node_s {
   int node_type;
   int num_items;
   struct json_node_s *next;
} json_node_t;

typedef struct json_s {
   stringbuf_t *sb;
   int tab_level;
   json_node_t *stack;
   int pending;
   unsigned char prettyprint;
   unsigned char mode;
} json_t;


void json_push(json_t *json, int node_type);
json_t *json_new();
void json_pretty_print(json_t *json, unsigned char pp);
void json_free(json_t *json);
char *json_to_string(json_t *json);
void json_new_object(json_t *json);
void json_end_object(json_t *json);
void json_new_array(json_t *json);
void json_end_array(json_t *json);
void json_add_value(json_t *json, char *value);
void json_add_key(json_t *json, char *key);
void json_add_number(json_t *json, char *key, char *value);
void json_add_string(json_t *json, char *key, char *value);
void json_add_elem(json_t *json, char *value);
void json_add_json(json_t *json, char *value);
void json_add_null(json_t *json, char *key);
void json_push(json_t *json, int node_type);
void json_add_callback(json_t *json, char *value);
void json_end_callback(json_t *json);
int json_mem_exceeded(json_t *json);

void json_set_mode(json_t *json, unsigned char mode);
unsigned char json_get_mode(json_t *json);

#endif /* _JSON_H_INCLUDED_ */
