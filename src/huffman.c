/*
 *  TV headend - Huffman decoder
 *  Copyb1 (C) 2012 Adam Sutton
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "huffman.h"
#include "htsmsg.h"
#include "settings.h"

void huffman_tree_destroy ( huffman_node_t *n )
{
  if (!n) return;
  huffman_tree_destroy(n->b0);
  huffman_tree_destroy(n->b1);
  if (n->data) free(n->data);
  free(n);
}

huffman_node_t *huffman_tree_load ( const char *path )
{
  htsmsg_t *m;
  huffman_node_t *ret;
  if (!(m = hts_settings_load(path))) return NULL;
  ret = huffman_tree_build(m);
  htsmsg_destroy(m);
  return ret;
}

huffman_node_t *huffman_tree_build ( htsmsg_t *m )
{
  const char *code, *data, *c;
  htsmsg_t *e;
  htsmsg_field_t *f;
  huffman_node_t *root = calloc(1, sizeof(huffman_node_t));
  HTSMSG_FOREACH(f, m) {
    e = htsmsg_get_map_by_field(f);
    c = code = htsmsg_get_str(e, "code");
    data = htsmsg_get_str(e, "data");
    if (code && data) {
      huffman_node_t *node = root;
      while (*c) {
        if ( *c == '0' ) {
          if (!node->b0) node->b0 = calloc(1, sizeof(huffman_node_t));
          node = node->b0;
        } else if ( *c == '1' ) {
          if (!node->b1) node->b1 = calloc(1, sizeof(huffman_node_t));
          node = node->b1;
        } else {
          htsmsg_destroy(m);
          huffman_tree_destroy(root);
          return NULL;
        }
        c++;
      }
      node->data = strdup(data);
    }
  }
  return root; 
}

char *huffman_decode 
  ( huffman_node_t *tree, const uint8_t *data, size_t len, uint8_t mask,
    char *outb, int outl )
{
  char           *ret  = outb;
  huffman_node_t *node = tree;
  if (!len) return NULL;

  outl--; // leave space for NULL
  while (len) {
    len--;
    while (mask) {
      if (*data & mask) {
        node = node->b1;
      } else {
        node = node->b0;
      }
      mask >>= 1;
      if (!node) goto end;
      if (node->data) {
        char *t = node->data;
        while (*t && outl) {
          *outb = *t;
          outb++; t++; outl--;
        }
        if (!outl) goto end;
        node = tree;
      }
    }
    mask = 0x80;
    data++;
  }
end:
  *outb = '\0';
  return ret;
}
