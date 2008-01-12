/* FFdecsa -- fast decsa algorithm
 *
 * Copyright (C) 2003-2004  fatih89r
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef FFDECSA_H
#define FFDECSA_H

//----- public interface

// -- how many packets can be decrypted at the same time
// This is an info about internal decryption parallelism.
// You should try to call decrypt_packets with more packets than the number
// returned here for performance reasons (use get_suggested_cluster_size to know
// how many).
int get_internal_parallelism(void);

// -- how many packets you should have in a cluster when calling decrypt_packets
// This is a suggestion to achieve optimal performance; typically a little
// higher than what get_internal_parallelism returns.
// Passing less packets could slow down the decryption.
// Passing more packets is never bad (if you don't spend a lot of time building
// the list).
int get_suggested_cluster_size(void);

// -- alloc & free the key structure
void *get_key_struct(void);
void free_key_struct(void *keys);

// -- set control words, 8 bytes each
void set_control_words(void *keys, const unsigned char *even, const unsigned char *odd);

// -- set even control word, 8 bytes
void set_even_control_word(void *keys, const unsigned char *even);

// -- set odd control word, 8 bytes
void set_odd_control_word(void *keys, const unsigned char *odd);

// -- get control words, 8 bytes each
//void get_control_words(void *keys, unsigned char *even, unsigned char *odd);

// -- decrypt many TS packets
// This interface is a bit complicated because it is designed for maximum speed.
// Please read doc/how_to_use.txt.
int decrypt_packets(void *keys, unsigned char **cluster);

#endif
