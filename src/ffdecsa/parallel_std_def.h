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

#define FFXOR(a,b) ((a)^(b))
#define FFAND(a,b) ((a)&(b))
#define FFOR(a,b)  ((a)|(b))
#define FFNOT(a)   (~(a))

#define B_FFAND(a,b) ((a)&(b))
#define B_FFOR(a,b)  ((a)|(b))
#define B_FFXOR(a,b) ((a)^(b))
#define B_FFSH8L(a,n) ((a)<<(n))
#define B_FFSH8R(a,n) ((a)>>(n))
