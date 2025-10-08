/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2015 Jaroslav Kysela <perex@perex.cz>
 */

#ifndef __TVH_ENDIAN_H
#define __TVH_ENDIAN_H

#if defined(PLATFORM_DARWIN)
#include <machine/endian.h>
#define bswap_16(x) _OSSwapInt16(x)
#define bswap_32(x) _OSSwapInt32(x)
#define bswap_64(x) _OSSwapInt64(x)
#elif defined(PLATFORM_FREEBSD)
#include <sys/endian.h>
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)
#else
#include <byteswap.h>
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
