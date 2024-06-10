/*
 *  TV headend - UUID generation routines
 *
 *  Copyright (C) 2014 Adam Sutton
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

#include "tvheadend.h"
#include "uuid.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define RANDOM_PATH "/dev/urandom"

uint8_t ___uuid_empty[UUID_BIN_SIZE] = {0};

static int fd = -1;

/* **************************************************************************
 * Utilities
 * *************************************************************************/

/**
 *
 */
static inline int hexnibble(char c) {
  switch (c) {
    case '0' ... '9':
      return c - '0';
    case 'a' ... 'f':
      return c - 'a' + 10;
    case 'A' ... 'F':
      return c - 'A' + 10;
    default:
      return -1;
  }
}

/**
 *
 */
int hex2bin(uint8_t* buf, size_t buflen, const char* str) {
  int hi, lo;

  while (*str) {
    if (buflen == 0)
      return -1;
    if ((hi = hexnibble(*str++)) == -1)
      return -1;
    if ((lo = hexnibble(*str++)) == -1)
      return -1;

    *buf++ = hi << 4 | lo;
    buflen--;
  }
  return 0;
}

/**
 *
 */
char* bin2hex(char* dst, size_t dstlen, const uint8_t* src, size_t srclen) {
  static const char table[] = "0123456789abcdef";
  char*             ret     = dst;
  while (dstlen > 2 && srclen > 0) {
    *dst++ = table[*src >> 4];
    *dst++ = table[*src & 0xf];
    src++;
    srclen--;
    dstlen -= 2;
  }
  *dst = 0;
  return ret;
}

/* **************************************************************************
 * UUID Handling
 * *************************************************************************/

void uuid_init(void) {
  fd = tvh_open(RANDOM_PATH, O_RDONLY, 0);
  if (fd == -1) {
    tvherror(LS_UUID, "failed to open %s", RANDOM_PATH);
    exit(1);
  }
}

void uuid_random(uint8_t* buf, size_t bufsize) {
  if (read(fd, buf, bufsize) != bufsize) {
    tvherror(LS_UUID, "random failed: %s", strerror(errno));
    exit(1);
  }
}

/* Initialise binary */
int uuid_set(tvh_uuid_t* u, const char* str) {
  if (str) {
    if (strlen(str) != UUID_HEX_SIZE - 1) {
      memset(u, 0, sizeof(*u));
      tvherror(LS_UUID, "wrong uuid string size (%zd)", strlen(str));
      return -EINVAL;
    }
    if (hex2bin(u->bin, sizeof(u->bin), str)) {
      memset(u, 0, sizeof(*u));
      tvherror(LS_UUID, "wrong uuid string '%s'", str);
      return -EINVAL;
    }
  } else if (read(fd, u->bin, sizeof(u->bin)) != sizeof(u->bin)) {
    tvherror(LS_UUID, "failed to read from %s", RANDOM_PATH);
    return -EINVAL;
  }
  return 0;
}

/* Initialise hex string */
char* uuid_get_hex(const tvh_uuid_t* u, char* dst) {
  assert(dst);
  return bin2hex(dst, UUID_HEX_SIZE, u->bin, sizeof(u->bin));
}

/* Validate the hexadecimal representation of uuid */
int uuid_hexvalid(const char* uuid) {
  int i;
  if (uuid == NULL)
    return 0;
  for (i = 0; i < UUID_HEX_SIZE - 1; i++)
    if (hexnibble(uuid[i]) < 0)
      return 0;
  return 1;
}

/* Init uuid set */
void uuid_set_init(tvh_uuid_set_t* us, uint32_t alloc_chunk) {
  memset(us, 0, sizeof(*us));
  us->us_alloc_chunk = alloc_chunk ?: 10;
}

/* Copy uuid set */
tvh_uuid_set_t* uuid_set_copy(tvh_uuid_set_t* dst, const tvh_uuid_set_t* src) {
  size_t size;
  memset(dst, 0, sizeof(*dst));
  dst->us_alloc_chunk = src->us_alloc_chunk;
  size                = sizeof(tvh_uuid_t) * src->us_size;
  dst->us_array       = malloc(size);
  if (dst->us_array == NULL)
    return NULL;
  memcpy(dst->us_array, src->us_array, size);
  dst->us_size  = src->us_size;
  dst->us_count = src->us_count;
  return dst;
}

/* Add an uuid to set */
tvh_uuid_t* uuid_set_add(tvh_uuid_set_t* us, const tvh_uuid_t* u) {
  tvh_uuid_t* nu;

  if (us->us_count >= us->us_size) {
    nu = realloc(us->us_array, sizeof(*u) * (us->us_size + us->us_alloc_chunk));
    if (nu == NULL)
      return NULL;
    us->us_array = nu;
    us->us_size += us->us_alloc_chunk;
  }
  nu  = &us->us_array[us->us_count++];
  *nu = *u;
  return nu;
}

/* Free uuid set */
void uuid_set_free(tvh_uuid_set_t* us) {
  if (us) {
    free(us->us_array);
    us->us_size  = 0;
    us->us_count = 0;
  }
}

/* Destroy uuid set */
void uuid_set_destroy(tvh_uuid_set_t* us) {
  if (us) {
    uuid_set_free(us);
    free(us);
  }
}
