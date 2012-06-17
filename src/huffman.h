/*
 *  TV headend - Huffman decoder
 *  Copyright (C) 2012 Adam Sutton
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
  ( huffman_node_t *tree, const uint8_t *data, size_t len, uint8_t mask );

#endif
