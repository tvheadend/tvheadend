/* FFdecsa -- fast decsa algorithm
 *
 * Copyright (C) 2007 Dark Avenger
 *               2003-2004  fatih89r
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

#ifndef FFTABLE_H
#define FFTABLE_H

void static inline FFTABLEIN(unsigned char *tab, int g, unsigned char *data)
{
#if 0
  *(((int *)tab)+2*g)=*((int *)data);
  *(((int *)tab)+2*g+1)=*(((int *)data)+1);
#else
  *(((long long *)tab)+g)=*((long long *)data);
#endif
}

void static inline FFTABLEOUT(unsigned char *data, unsigned char *tab, int g)
{
#if 1
  *((int *)data)=*(((int *)tab)+2*g);
  *(((int *)data)+1)=*(((int *)tab)+2*g+1);
#else
  *((long long *)data)=*(((long long *)tab)+g);
#endif
}

void static inline FFTABLEOUTXORNBY(int n, unsigned char *data, unsigned char *tab, int g)
{
  int j;
  for(j=0;j<n;j++) *(data+j)^=*(tab+8*g+j);
}

#undef XOREQ_BEST_BY
static inline void XOREQ_BEST_BY(unsigned char *d, unsigned char *s)
{
	XOR_BEST_BY(d, d, s);
}

#endif //FFTABLE_H 
