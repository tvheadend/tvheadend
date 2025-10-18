/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * TV headend - Huffman decoder
 */

#ifndef __TVH_HUFFMAN_H__
#define __TVH_HUFFMAN_H__

#include <sys/types.h>
#include "htsmsg.h"

typedef struct huffman_node
{
  struct huffman_node *b0;
  struct huffman_node *b1;
  char                *data;
} huffman_node_t;

void huffman_tree_destroy ( huffman_node_t *tree );
huffman_node_t *huffman_tree_load  ( const char *path );
huffman_node_t *huffman_tree_build ( htsmsg_t *codes );
char *huffman_decode 
  ( huffman_node_t *tree, const uint8_t *data, size_t len, uint8_t mask,
    char *outb, int outl );

#endif
