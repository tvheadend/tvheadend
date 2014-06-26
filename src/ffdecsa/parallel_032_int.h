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

#include "parallel_std_def.h"

typedef unsigned int group;
#define GROUP_PARALLELISM 32
#define FF0()      0x0
#define FF1()      0xffffffff

/* 64 rows of 32 bits */

void static inline FFTABLEIN(unsigned char *tab, int g, unsigned char *data){
  *(((int *)tab)+g)=*((int *)data);
  *(((int *)tab)+32+g)=*(((int *)data)+1);
}

void static inline FFTABLEOUT(unsigned char *data, unsigned char *tab, int g){
  *((int *)data)=*(((int *)tab)+g);
  *(((int *)data)+1)=*(((int *)tab)+32+g);
}

void static inline FFTABLEOUTXORNBY(int n, unsigned char *data, unsigned char *tab, int g){
  int j;
  for(j=0;j<n;j++){
    *(data+j)^=*(tab+4*(g+(j>=4?32-1:0))+j);
  }
}

typedef unsigned int batch;
#define BYTES_PER_BATCH 4
#define B_FFN_ALL_29() 0x29292929
#define B_FFN_ALL_02() 0x02020202
#define B_FFN_ALL_04() 0x04040404
#define B_FFN_ALL_10() 0x10101010
#define B_FFN_ALL_40() 0x40404040
#define B_FFN_ALL_80() 0x80808080

#define M_EMPTY()
