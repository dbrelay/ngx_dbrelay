#include "stringbuf.h"
#include <stdio.h>

char *sb_to_char(stringbuf_t *string)
{
   int end = 0;
   stringbuf_node_t *node;
   char *outstr = (char *) malloc(sb_len(string) + 1);
   outstr[0] = '\0';

   node = string->head;
   do {
      if (node->part) {
         strcpy(&outstr[end], node->part);
         end += strlen(node->part);
      }
      node = node->next;
   } while (node);

   return outstr;
}

int sb_len(stringbuf_t *string)
{
   stringbuf_node_t *node;
   int len = 0;

   node = string->head;
   do {
      if (node->part) len += strlen(node->part);
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
      if (prev->part) free(prev->part);
      free(prev);
   } while (node);
   free(string);
}

stringbuf_t *sb_new(char *s)
{
   stringbuf_t *string = (stringbuf_t *) malloc(sizeof(stringbuf_t));
   memset(string, 0, sizeof(stringbuf_t));
   stringbuf_node_t *new_node = malloc(sizeof(stringbuf_node_t));
   memset(new_node, 0, sizeof(stringbuf_node_t));
   if (s) new_node->part = strdup(s); 
   string->head = new_node;
   string->tail = new_node;

   return string;
}

stringbuf_node_t *sb_append(stringbuf_t *string, char *s)
{
   stringbuf_node_t *new_node = malloc(sizeof(stringbuf_node_t));
   memset(new_node, 0, sizeof(stringbuf_node_t));
   if (s) new_node->part = strdup(s); 
   string->tail->next = new_node;
   string->tail = new_node;

   return new_node;
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
