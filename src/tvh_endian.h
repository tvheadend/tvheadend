/*
 * Copyright (c) 2015 Jaroslav Kysela <perex@perex.cz>
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

#ifndef __TVH_ENDIAN_H
#define __TVH_ENDIAN_H

#include <byteswap.h>
#if defined(PLATFORM_DARWIN)
#include <machine/endian.h>
#elif defined(PLATFORM_FREEBSD)
#include <sys/endian.h>
#else
#include <endian.h>
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER __BYTE_ORDER
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define ENDIAN_SWAP_COND(x) (!(x))
#else
#define ENDIAN_SWAP_COND(x) (x)
#endif

#endif /* __TVH_ENDIAN_H */
