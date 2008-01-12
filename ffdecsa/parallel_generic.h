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



#if 0
//// generics
#define COPY4BY(d,s)     do{ int *pd=(int *)(d), *ps=(int *)(s); \
                             *pd = *ps; }while(0)
#define COPY8BY(d,s)     do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd = *ps; }while(0)
#define COPY16BY(d,s)    do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd = *ps; \
			     *(pd+1) = *(ps+1); }while(0)
#define COPY32BY(d,s)    do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd = *ps; \
			     *(pd+1) = *(ps+1) \
			     *(pd+2) = *(ps+2) \
			     *(pd+3) = *(ps+3); }while(0)
#define XOR4BY(d,s1,s2)  do{ int *pd=(int *)(d), *ps1=(int *)(s1), *ps2=(int *)(s2); \
                             *pd = *ps1  ^ *ps2; }while(0)
#define XOR8BY(d,s1,s2)  do{ long long int *pd=(long long int *)(d), *ps1=(long long int *)(s1), *ps2=(long long int *)(s2); \
                             *pd = *ps1  ^ *ps2; }while(0)
#define XOR16BY(d,s1,s2) do{ long long int *pd=(long long int *)(d), *ps1=(long long int *)(s1), *ps2=(long long int *)(s2); \
                             *pd = *ps1  ^ *ps2; \
                             *(pd+8) = *(ps1+8)  ^ *(ps2+8); }while(0)
#define XOR32BY(d,s1,s2) do{ long long int *pd=(long long int *)(d), *ps1=(long long int *)(s1), *ps2=(long long int *)(s2); \
                             *pd = *ps1  ^ *ps2; \
                             *(pd+1) = *(ps1+1)  ^ *(ps2+1); \
                             *(pd+2) = *(ps1+2)  ^ *(ps2+2); \
                             *(pd+3) = *(ps1+3)  ^ *(ps2+3); }while(0)
#define XOR32BV(d,s1,s2) do{ int *const pd=(int *const)(d), *ps1=(const int *const)(s1), *ps2=(const int *const)(s2); \
                             int z; \
			     for(z=0;z<8;z++){ \
                               pd[z]=ps1[z]^ps2[z]; \
			     } \
                           }while(0)
#define XOREQ4BY(d,s)    do{ int *pd=(int *)(d), *ps=(int *)(s); \
                             *pd ^= *ps; }while(0)
#define XOREQ8BY(d,s)    do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd ^= *ps; }while(0)
#define XOREQ16BY(d,s)   do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd ^= *ps; \
			     *(pd+1) ^=*(ps+1); }while(0)
#define XOREQ32BY(d,s)   do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                             *pd ^= *ps; \
			     *(pd+1) ^=*(ps+1); \
			     *(pd+2) ^=*(ps+2); \
			     *(pd+3) ^=*(ps+3); }while(0)
#define XOREQ32BY4(d,s)  do{ int *pd=(int *)(d), *ps=(int *)(s); \
                             *pd ^= *ps; \
			     *(pd+1) ^=*(ps+1); \
			     *(pd+2) ^=*(ps+2); \
			     *(pd+3) ^=*(ps+3); \
			     *(pd+4) ^=*(ps+4); \
			     *(pd+5) ^=*(ps+5); \
			     *(pd+6) ^=*(ps+6); \
			     *(pd+7) ^=*(ps+7); }while(0)
#define XOREQ32BV(d,s)   do{ unsigned char *pd=(unsigned char *)(d), *ps=(unsigned char *)(s); \
                             int z; \
			     for(z=0;z<32;z++){ \
                               pd[z]^=ps[z]; \
			     } \
                           }while(0)

#else
#define XOR_4_BY(d,s1,s2)    do{ int *pd=(int *)(d), *ps1=(int *)(s1), *ps2=(int *)(s2); \
                               *pd = *ps1  ^ *ps2; }while(0)
#define XOR_8_BY(d,s1,s2)    do{ long long int *pd=(long long int *)(d), *ps1=(long long int *)(s1), *ps2=(long long int *)(s2); \
                               *pd = *ps1  ^ *ps2; }while(0)
#define XOREQ_4_BY(d,s)      do{ int *pd=(int *)(d), *ps=(int *)(s); \
                               *pd ^= *ps; }while(0)
#define XOREQ_8_BY(d,s)      do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                               *pd ^= *ps; }while(0)
#define COPY_4_BY(d,s)       do{ int *pd=(int *)(d), *ps=(int *)(s); \
                               *pd = *ps; }while(0)
#define COPY_8_BY(d,s)       do{ long long int *pd=(long long int *)(d), *ps=(long long int *)(s); \
                               *pd = *ps; }while(0)

#define BEST_SPAN            8
#define XOR_BEST_BY(d,s1,s2) do{ XOR_8_BY(d,s1,s2); }while(0);
#define XOREQ_BEST_BY(d,s)   do{ XOREQ_8_BY(d,s); }while(0);
#define COPY_BEST_BY(d,s)    do{ COPY_8_BY(d,s); }while(0);

#define END_MM             do{ }while(0);
#endif
