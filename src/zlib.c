/*
 *  TV headend - zlib integration
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

#include "tvheadend.h"

#define ZLIB_CONST 1
#include <zlib.h>
#ifndef z_const
#define z_const
#endif

/* **************************************************************************
 * Compression/Decompression
 * *************************************************************************/

uint8_t *tvh_gzip_inflate ( const uint8_t *data, size_t size, size_t orig )
{
  int err;
  z_stream zstr;
  uint8_t *bufout;

  /* Setup buffers */
  bufout = malloc(orig);

  /* Setup zlib */
  memset(&zstr, 0, sizeof(zstr));
  inflateInit2(&zstr, MAX_WBITS + 16 /* gzip */);
  zstr.avail_in  = size;
  zstr.next_in   = (z_const uint8_t *)data;
  zstr.avail_out = orig;
  zstr.next_out  = bufout;

  /* Decompress */
  err = inflate(&zstr, Z_NO_FLUSH);
  if ( err != Z_STREAM_END || zstr.avail_out != 0 ) {
    free(bufout);
    bufout = NULL;
  }
  inflateEnd(&zstr);

  return bufout;
}

uint8_t *tvh_gzip_deflate ( const uint8_t *data, size_t orig, size_t *size )
{
  int err;
  z_stream zstr;
  uint8_t *bufout;

  /* Setup buffers */
  bufout = malloc(orig);

  /* Setup zlib */
  memset(&zstr, 0, sizeof(zstr));
  err = deflateInit2(&zstr, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16 /* gzip */, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  zstr.avail_in  = orig;
  zstr.next_in   = (z_const uint8_t *)data;
  zstr.avail_out = orig;
  zstr.next_out  = bufout;

  /* Compress */
  while (1) {
    err = deflate(&zstr, Z_FINISH);

    /* Need more space */
    if (err == Z_OK && zstr.avail_out == 0) {
      bufout         = realloc(bufout, zstr.total_out * 2);
      zstr.avail_out = zstr.total_out;
      zstr.next_out  = bufout + zstr.total_out;
      continue;
    }

    /* Error */
    if ( (err != Z_STREAM_END && err != Z_OK) || zstr.total_out == 0 ) {
      free(bufout);
      bufout = NULL;
    } else {
      *size  = zstr.total_out;
    }
    break;
  }
  deflateEnd(&zstr);

  return bufout;
}

int tvh_gzip_deflate_fd ( int fd, const uint8_t *data, size_t orig, size_t *size, int speed )
{
  int r = 0, err;
  z_stream zstr;
  uint8_t *bufout;
  size_t alloc;

  assert(speed >= Z_BEST_SPEED && speed <= Z_BEST_COMPRESSION);

  /* Setup buffers */
  *size  = 0;
  alloc  = MIN(orig, 256*1024);
  bufout = malloc(alloc);

  /* Setup zlib */
  memset(&zstr, 0, sizeof(zstr));
  err = deflateInit2(&zstr, speed, Z_DEFLATED, MAX_WBITS + 16 /* gzip */, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  zstr.avail_in  = orig;
  zstr.next_in   = (z_const uint8_t *)data;
  zstr.avail_out = alloc;
  zstr.next_out  = bufout;

  /* Compress */
  while (1) {
    err = deflate(&zstr, Z_FINISH);

    /* Need more space */
    if (err == Z_OK && zstr.avail_out == 0) {
      r = tvh_write(fd, bufout, alloc);
      if (r) {
        r = -1;
        break;
      }
      zstr.avail_out = alloc;
      zstr.next_out  = bufout;
      continue;
    }

    /* Error */
    if ( (err != Z_STREAM_END && err != Z_OK) || zstr.total_out == 0 ) {
      r = -1;
    } else {
      if (zstr.avail_out != alloc)
        r = tvh_write(fd, bufout, alloc - zstr.avail_out);
      *size = zstr.total_out;
    }
    break;
  }
  deflateEnd(&zstr);
  free(bufout);

  return r;
}

int tvh_gzip_deflate_fd_header ( int fd, const uint8_t *data, size_t orig,
                                 size_t *deflated_size, int speed )
{
  uint8_t data2[4];
  size_t size = 0;
  int r;

  if (deflated_size) *deflated_size = 0;
  r = tvh_write(fd, "\xff\xffGZIP00\x00\x00\x00\x00", 12);
  if (r)
    return 1;
  if (orig > 0) {
    r = tvh_gzip_deflate_fd(fd, data, orig, &size, 3) < 0;
    if (r || size > UINT_MAX)
      return 1;
  }
  r = lseek(fd, 8, SEEK_SET) != (off_t)8;
  if (r)
    return 1;
  if (deflated_size) *deflated_size = size + 12;
  data2[0] = (orig >> 24) & 0xff;
  data2[1] = (orig >> 16) & 0xff;
  data2[2] = (orig >> 8) & 0xff;
  data2[3] = (orig & 0xff);
  return tvh_write(fd, data2, 4);
}
