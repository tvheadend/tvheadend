/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TV headend - Huffman decoder
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
