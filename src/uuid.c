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

uint8_t ___uuid_empty[UUID_BIN_SIZE] = { 0 };

static int fd = -1;

/* **************************************************************************
 * Utilities
 * *************************************************************************/

/**
 *
 */
static inline int
hexnibble(char c)
{
  switch(c) {
  case '0' ... '9':    return c - '0';
  case 'a' ... 'f':    return c - 'a' + 10;
  case 'A' ... 'F':    return c - 'A' + 10;
  default:
    return -1;
  }
}


/**
 *
 */
int
hex2bin(uint8_t *buf, size_t buflen, const char *str)
{
  int hi, lo;

  while(*str) {
    if(buflen == 0)
      return -1;
    if((hi = hexnibble(*str++)) == -1)
      return -1;
    if((lo = hexnibble(*str++)) == -1)
      return -1;

    *buf++ = hi << 4 | lo;
    buflen--;
  }
  return 0;
}

/**
 *
 */
void
bin2hex(char *dst, size_t dstlen, const uint8_t *src, size_t srclen)
{
  while(dstlen > 2 && srclen > 0) {
    *dst++ = "0123456789abcdef"[*src >> 4];
    *dst++ = "0123456789abcdef"[*src & 0xf];
    src++;
    srclen--;
    dstlen -= 2;
  }
  *dst = 0;
}

/* **************************************************************************
 * UUID Handling
 * *************************************************************************/

void
uuid_init ( void )
{
  fd = tvh_open(RANDOM_PATH, O_RDONLY, 0);
  if (fd == -1) {
    tvherror("uuid", "failed to open %s", RANDOM_PATH);
    exit(1);
  }
}

void
uuid_random ( uint8_t *buf, size_t bufsize )
{
  if (read(fd, buf, bufsize) != bufsize) {
    tvherror("uuid", "random failed: %s", strerror(errno));
    exit(1);
  }
}

/* Initialise binary */
int
uuid_init_bin ( tvh_uuid_t *u, const char *str )
{
  memset(u, 0, sizeof(tvh_uuid_t));
  if (str) {
    if (strlen(str) != UUID_HEX_SIZE - 1) {
      tvherror("uuid", "wrong uuid size");
      return -EINVAL;
    }
    return hex2bin(u->bin, sizeof(u->bin), str);
  } else if (read(fd, u->bin, sizeof(u->bin)) != sizeof(u->bin)) {
    tvherror("uuid", "failed to read from %s", RANDOM_PATH);
    return -EINVAL;
  }
  return 0;
}

/* Initialise hex string */
int
uuid_init_hex ( tvh_uuid_t *u, const char *str )
{
  tvh_uuid_t tmp;
  if (uuid_init_bin(&tmp, str))
    return 1;
  return uuid_bin2hex(&tmp, u);
}

/* Convert bin to hex string */
int
uuid_bin2hex ( const tvh_uuid_t *a, tvh_uuid_t *b )
{
  tvh_uuid_t tmp;
  memset(&tmp, 0, sizeof(tmp));
  bin2hex(tmp.hex, sizeof(tmp.hex), a->bin, sizeof(a->bin));
  memcpy(b, &tmp, sizeof(tmp));
  return 0;
}

/* Convert hex string to bin (in place) */
int
uuid_hex2bin ( const tvh_uuid_t *a, tvh_uuid_t *b )
{ 
  tvh_uuid_t tmp;
  memset(&tmp, 0, sizeof(tmp));
  if (hex2bin(tmp.bin, sizeof(tmp.bin), a->hex))
    return 1;
  memcpy(b, &tmp, sizeof(tmp));
  return 0;
}

/* Validate hex string */
int
uuid_hexvalid ( const char *uuid )
{
  int i;
  if (uuid == NULL)
    return 0;
  for (i = 0; i < UUID_HEX_SIZE - 1; i++)
    if (hexnibble(uuid[i]) < 0)
      return 0;
  return 1;
}
