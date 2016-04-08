/*
 * Copyright (c) 2005 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2006 Ryan Martell. (rdm4@martellventures.com)
 * Copyright (c) 2010 Andreas Öman
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

#include <limits.h>
#include <string.h>
#include <assert.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <net/if.h>

#include <openssl/sha.h>

#include "tvheadend.h"
#include "tvh_endian.h"

/**
 * CRC32 
 */
static uint32_t crc_tab[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t
tvh_crc32(const uint8_t *data, size_t datalen, uint32_t crc)
{
  while(datalen--)
    crc = (crc << 8) ^ crc_tab[((crc >> 24) ^ *data++) & 0xff];

  return crc;
}


/**
 *
 */
static const int sample_rates[16] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025,  8000,
     7350,     0,     0,     0
};

/**
 *
 */
int
sri_to_rate(int sri)
{
  return sample_rates[sri & 0xf];
}


/**
 *
 */
int
rate_to_sri(int rate)
{
  int i;
  for(i = 0; i < 16; i++)
    if(sample_rates[i] == rate)
      return i;
  return -1;
}


/**
 *
 */
#define HEXDUMP_WIDTH 16
void
hexdump(const char *pfx, const uint8_t *data, int len)
{
  int i, j=0, l;
  char str[HEXDUMP_WIDTH+1];
  l = ((len+(HEXDUMP_WIDTH-1))/HEXDUMP_WIDTH)*HEXDUMP_WIDTH;
  str[0] = 0;
  for (i = 0; i < l; i++) {
    if (!j) printf("%s: ", pfx);
    if (i < len) {
      printf("%02X ", data[i]);
      str[j] = data[i];
      if (str[j] < ' ' || str[j] > '~') str[j] = '.';
    } else {
      printf("   ");
      str[j] = ' ';
    }
    j++;
    if (j == HEXDUMP_WIDTH) {
      str[j] = 0;
      printf("%s\n", str);
      str[0] = 0;
      j = 0;
    }
  }
}

/**
 * @file
 * @brief Base64 encode/decode
 * @author Ryan Martell <rdm4@martellventures.com> (with lots of Michael)
 */


/* ---------------- private code */
static const uint8_t map2[] =
{
    0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
};

int 
base64_decode(uint8_t *out, const char *in, int out_size)
{
    int i, v;
    uint8_t *dst = out;

    v = 0;
    for (i = 0; in[i] && in[i] != '='; i++) {
        unsigned int index= in[i]-43;
        if (index >= sizeof(map2) || map2[index] == 0xff)
            return -1;
        v = (v << 6) + map2[index];
        if (i & 3) {
            if (dst - out < out_size) {
                *dst++ = v >> (6 - 2 * (i & 3));
            }
        }
    }

    return dst - out;
}

/*
 * b64_encode: Stolen from VLC's http.c.
 * Simplified by Michael.
 * Fixed edge cases and made it work from data (vs. strings) by Ryan.
 */

char *base64_encode(char *out, int out_size, const uint8_t *in, int in_size)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *ret, *dst;
    unsigned i_bits = 0;
    int i_shift = 0;
    int bytes_remaining = in_size;

    if (in_size >= UINT_MAX / 4 ||
        out_size < BASE64_SIZE(in_size))
        return NULL;
    ret = dst = out;
    while (bytes_remaining) {
        i_bits = (i_bits << 8) + *in++;
        bytes_remaining--;
        i_shift += 8;

        do {
            *dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
            i_shift -= 6;
        } while (i_shift > 6 || (bytes_remaining == 0 && i_shift > 0));
    }
    while ((dst - ret) & 3)
        *dst++ = '=';
    *dst = '\0';

    return ret;
}

/**
 *
 */
int
put_utf8(char *out, int c)
{
  if(c == 0xfffe || c == 0xffff || (c >= 0xD800 && c < 0xE000))
    return 0;
  
  if (c < 0x80) {
    *out = c;
    return 1;
  }

  if(c < 0x800) {
    *out++ = 0xc0 | (0x1f & (c >>  6));
    *out   = 0x80 | (0x3f &  c);
    return 2;
  }

  if(c < 0x10000) {
    *out++ = 0xe0 | (0x0f & (c >> 12));
    *out++ = 0x80 | (0x3f & (c >> 6));
    *out   = 0x80 | (0x3f &  c);
    return 3;
  }

  if(c < 0x200000) {
    *out++ = 0xf0 | (0x07 & (c >> 18));
    *out++ = 0x80 | (0x3f & (c >> 12));
    *out++ = 0x80 | (0x3f & (c >> 6));
    *out   = 0x80 | (0x3f &  c);
    return 4;
  }
  
  if(c < 0x4000000) {
    *out++ = 0xf8 | (0x03 & (c >> 24));
    *out++ = 0x80 | (0x3f & (c >> 18));
    *out++ = 0x80 | (0x3f & (c >> 12));
    *out++ = 0x80 | (0x3f & (c >>  6));
    *out++ = 0x80 | (0x3f &  c);
    return 5;
  }

  *out++ = 0xfc | (0x01 & (c >> 30));
  *out++ = 0x80 | (0x3f & (c >> 24));
  *out++ = 0x80 | (0x3f & (c >> 18));
  *out++ = 0x80 | (0x3f & (c >> 12));
  *out++ = 0x80 | (0x3f & (c >>  6));
  *out++ = 0x80 | (0x3f &  c);
  return 6;
}

static void
sbuf_alloc_fail(int len)
{
  fprintf(stderr, "Unable to allocate %d bytes\n", len);
  abort();
}

void
sbuf_init(sbuf_t *sb)
{
  memset(sb, 0, sizeof(sbuf_t));
}

void
sbuf_init_fixed(sbuf_t *sb, int len)
{
  memset(sb, 0, sizeof(sbuf_t));
  sb->sb_data = malloc(len);
  if (sb->sb_data == NULL)
    sbuf_alloc_fail(len);
  sb->sb_size = len;
}

void
sbuf_free(sbuf_t *sb)
{
  free(sb->sb_data);
  sb->sb_size = sb->sb_ptr = sb->sb_err = 0;
  sb->sb_data = NULL;
}

void
sbuf_reset(sbuf_t *sb, int max_len)
{
  sb->sb_ptr = sb->sb_err = 0;
  if (sb->sb_size > max_len) {
    void *n = realloc(sb->sb_data, max_len);
    if (n) {
      sb->sb_data = n;
      sb->sb_size = max_len;
    }
  }
}

void
sbuf_reset_and_alloc(sbuf_t *sb, int len)
{
  sbuf_realloc(sb, len);
  sb->sb_ptr = sb->sb_err = 0;
}

void
sbuf_alloc_(sbuf_t *sb, int len)
{
  if(sb->sb_data == NULL) {
    sb->sb_size = len * 4 > 4000 ? len * 4 : 4000;
    sb->sb_data = malloc(sb->sb_size);
    return;
  } else {
    sb->sb_size += len * 4;
    sb->sb_data = realloc(sb->sb_data, sb->sb_size);
  }

  if(sb->sb_data == NULL)
    sbuf_alloc_fail(sb->sb_size);
}

void
sbuf_realloc(sbuf_t *sb, int len)
{
  if (sb->sb_data) {
    if (len != sb->sb_size) {
      void *n = realloc(sb->sb_data, len);
      if (n) {
        sb->sb_data = n;
        sb->sb_size = len;
      }
    }
  } else {
    sb->sb_data = malloc(len);
    sb->sb_size = len;
  }
  if (sb->sb_data == NULL)
    sbuf_alloc_fail(len);
}

void
sbuf_append(sbuf_t *sb, const void *data, int len)
{
  sbuf_alloc(sb, len);
  memcpy(sb->sb_data + sb->sb_ptr, data, len);
  sb->sb_ptr += len;
}

void
sbuf_put_be32(sbuf_t *sb, uint32_t u32)
{
  u32 = htonl(u32);
  sbuf_append(sb, &u32, 4);
}

void
sbuf_put_be16(sbuf_t *sb, uint16_t u16)
{
  u16 = htons(u16);
  sbuf_append(sb, &u16, 2);
}

void
sbuf_put_byte(sbuf_t *sb, uint8_t u8)
{
  sbuf_append(sb, &u8, 1);
}

uint16_t sbuf_peek_u16(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  if (ENDIAN_SWAP_COND(sb->sb_bswap))
    return p[0] | (((uint16_t)p[1]) << 8);
  else
    return (((uint16_t)p[0]) << 8) | p[1];
}

uint16_t sbuf_peek_u16le(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  return p[0] | (((uint16_t)p[1]) << 8);
}

uint16_t sbuf_peek_u16be(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  return (((uint16_t)p[0]) << 8) | p[1];
}

uint32_t sbuf_peek_u32(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  if (ENDIAN_SWAP_COND(sb->sb_bswap))
    return p[0] | (((uint32_t)p[1]) << 8) |
           (((uint32_t)p[2]) << 16) | (((uint32_t)p[3]) << 24);
  else
    return (((uint16_t)p[0]) << 24) | (((uint16_t)p[1]) << 16) |
            (((uint16_t)p[2]) << 8) | p[3];
}

uint32_t sbuf_peek_u32le(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  return p[0] | (((uint32_t)p[1]) << 8) |
         (((uint32_t)p[2]) << 16) | (((uint32_t)p[3]) << 24);
}

uint32_t sbuf_peek_u32be(sbuf_t *sb, int off)
{
  uint8_t *p = sb->sb_data + off;
  return (((uint16_t)p[0]) << 24) | (((uint16_t)p[1]) << 16) |
         (((uint16_t)p[2]) << 8) | p[3];
}

void 
sbuf_cut(sbuf_t *sb, int off)
{
  assert(off <= sb->sb_ptr);
  sb->sb_ptr = sb->sb_ptr - off;
  memmove(sb->sb_data, sb->sb_data + off, sb->sb_ptr);
}

ssize_t
sbuf_read(sbuf_t *sb, int fd)
{
  ssize_t n = read(fd, sb->sb_data + sb->sb_ptr, sb->sb_size - sb->sb_ptr);
  if (n > 0)
    sb->sb_ptr += n;
  return n;
}

char *
md5sum ( const char *str, int lowercase )
{
  uint8_t md5[MD5_DIGEST_LENGTH];
  char *ret = malloc((MD5_DIGEST_LENGTH * 2) + 1);
  int i;

  MD5((const unsigned char*)str, strlen(str), md5);
  for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    sprintf(&ret[i*2], lowercase ? "%02x" : "%02X", md5[i]);
  ret[MD5_DIGEST_LENGTH*2] = '\0';
  return ret;
}

#define FILE_MODE_BITS(x) (x&(S_IRWXU|S_IRWXG|S_IRWXO))

int
makedirs ( const char *subsys, const char *inpath, int mode,
           int mstrict, gid_t gid, uid_t uid )
{
  int err, ok;
  size_t x;
  struct stat st;
  char *path;

  if (!inpath || !*inpath) return -1;

  x  = 1;
  ok = 1;
  path = alloca(strlen(inpath) + 1);
  strcpy(path, inpath);
  while(ok) {
    ok = path[x];
    if (path[x] == '/' || !path[x]) {
      path[x] = 0;
      if (stat(path, &st)) {
        err = mkdir(path, mode);
        if (!err && gid != -1 && uid != -1)
          err = chown(path, uid, gid);
        if (!err && !stat(path, &st) &&
            FILE_MODE_BITS(mode) != FILE_MODE_BITS(st.st_mode)) {
          err = chmod(path, mode); /* override umode */
          if (!mstrict) {
            err = 0;
            tvhwarn(subsys, "Unable to change directory permissions "
                            "to \"%o\" for \"%s\" (keeping \"%o\")",
                            mode, path, FILE_MODE_BITS(st.st_mode));
            mode = FILE_MODE_BITS(st.st_mode);
          }
        }
        tvhtrace(subsys, "Creating directory \"%s\" with octal permissions "
                         "\"%o\" gid %d uid %d", path, mode, gid, uid);
      } else {
        err   = S_ISDIR(st.st_mode) ? 0 : 1;
        errno = ENOTDIR;
      }
      if (err) {
        tvhalert(subsys, "Unable to create dir \"%s\": %s",
                 path, strerror(errno));
        return -1;
      }
      path[x] = '/';
    }
    x++;
  }
  return 0;
}

int
rmtree ( const char *path )
{
  int err = 0;
  struct dirent de, *der;
  struct stat st;
  char buf[PATH_MAX];
  DIR *dir = opendir(path);
  if (!dir) return -1;
  while (!readdir_r(dir, &de, &der) && der) {
    if (!strcmp("..", de.d_name) || !strcmp(".", de.d_name))
      continue;
    snprintf(buf, sizeof(buf), "%s/%s", path, de.d_name);
    err = stat(buf, &st);
    if (err) break;
    if (S_ISDIR(st.st_mode))
      err = rmtree(buf);
    else
      err = unlink(buf);
    if (err) break;
  }
  closedir(dir);
  if (!err)
    err = rmdir(path);
  return err;
}

char *
regexp_escape(const char* str)
{
  const char *a;
  char *tmp, *b;
  if (!str)
    return NULL;
  a = str;
  b = tmp = malloc(strlen(str) * 2);
  while (*a) {
    switch (*a) {
      case '?':
      case '+':
      case '.':
      case '(':
      case ')':
      case '[':
      case ']':
      case '*':
      case '^':
        *b = '\\';
        b++;
        /* -fallthrough */
      default:
        break;
    }
    *b = *a;
    b++;
    a++;
  }
  *b = 0;
  return tmp;
}

/* Converts an integer value to its hex character
   http://www.geekhideout.com/urlcode.shtml */
char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str
   IMPORTANT: be sure to free() the returned string after use
   http://www.geekhideout.com/urlcode.shtml */
char *url_encode(const char *str)
{
  char *buf = malloc(strlen(str) * 3 + 1), *pbuf = buf;
  while (*str) {
    if (isalnum(*str) || *str == '-' || *str == '_' || *str == '.' || *str == '~')
      *pbuf++ = *str;
    /*else if (*str == ' ')
      *pbuf++ = '+';*/
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*str >> 4), *pbuf++ = to_hex(*str & 15);
    str++;
  }
  *pbuf = '\0';
  return buf;
}

/*
 *
 */

static inline uint32_t mpegts_word32( const uint8_t *tsb )
{
  //assert(((intptr_t)tsb & 3) == 0);
  return *(uint32_t *)tsb;
}

int
mpegts_word_count ( const uint8_t *tsb, int len, uint32_t mask )
{
  uint32_t val;
  int r = 0;

#if BYTE_ORDER == LITTLE_ENDIAN
  mask = bswap_32(mask);
#endif

  val  = mpegts_word32(tsb) & mask;

  while (len >= 188) {
    if (len >= 4*188 &&
        (mpegts_word32(tsb+0*188) & mask) == val &&
        (mpegts_word32(tsb+1*188) & mask) == val &&
        (mpegts_word32(tsb+2*188) & mask) == val &&
        (mpegts_word32(tsb+3*188) & mask) == val) {
      r   += 4*188;
      len -= 4*188;
      tsb += 4*188;
    } else if ((mpegts_word32(tsb) & mask) == val) {
      r   += 188;
      len -= 188;
      tsb += 188;
    } else {
      break;
    }
  }

  return r;
}

static void
deferred_unlink_cb(void *s, int dearmed)
{
  if (unlink((const char *)s))
    tvherror("main", "unable to remove file '%s'", (const char *)s);
  free(s);
}

typedef struct {
  char *filename;
  char *rootdir;
} deferred_unlink_t;

static void
deferred_unlink_dir_cb(void *s, int dearmed)
{
  deferred_unlink_t *du = s;
  char *p;
  int l;

  if (unlink((const char *)du->filename))
    tvherror("main", "unable to remove file '%s'", (const char *)du->filename);

  /* Remove all directories up to rootdir */

  l = strlen(du->filename) - 1;
  p = du->filename;

  for(; l >= 0; l--) {
    if(p[l] == '/') {
      p[l] = 0;
      if(strncmp(p, du->rootdir, l) == 0)
        break;
      if(rmdir(p) == -1)
        break;
    }
  }

  free(du->filename);
  free(du->rootdir);
  free(du);
}

int
deferred_unlink(const char *filename, const char *rootdir)
{
  deferred_unlink_t *du;
  char *s, *p;
  size_t l;
  int r;
  long max;

  l = strlen(filename);
  s = malloc(l + 9 + 1);
  if (s == NULL)
    return -ENOMEM;
  max = pathconf(filename, _PC_NAME_MAX);
  strcpy(s, filename);
  if (l + 10 < max) {
    p = strrchr(s, '/');
    if (p && p[1])
      p[1] = '.';
    strcpy(s + l, ".removing");
  } else {
    p = strrchr(s, '/');
    p = p && p[1] ? p + 1 : s;
    memcpy(p, ".rm.", 4);
  }
  r = rename(filename, s);
  if (r) {
    r = -errno;
    free(s);
    return r;
  }
  if (rootdir == NULL)
    tasklet_arm_alloc(deferred_unlink_cb, s);
  else {
    du = calloc(1, sizeof(*du));
    if (du == NULL) {
      free(s);
      return -ENOMEM;
    }
    du->filename = s;
    du->rootdir = strdup(rootdir);
    tasklet_arm_alloc(deferred_unlink_dir_cb, du);
  }
  return 0;
}

void
sha1_calc(uint8_t *dst,
          const uint8_t *d1, size_t d1_len,
          const uint8_t *d2, size_t d2_len)
{
  SHA_CTX shactx;

  SHA1_Init(&shactx);
  if (d1)
    SHA1_Update(&shactx, d1, d1_len);
  if (d2)
    SHA1_Update(&shactx, d2, d2_len);
  SHA1_Final(dst, &shactx);
}

uint32_t
gcdU32(uint32_t a, uint32_t b)
{
  uint32_t r;
  if (a < b) {
    while((r = b % a) != 0) {
      b = a;
      a = r;
    }
    return a;
  } else {
    while((r = a % b) != 0) {
      a = b;
      b = r;
    }
    return b;
  }
}

htsmsg_t *network_interfaces_enum(void *obj, const char *lang)
{
#if ENABLE_IFNAMES
  htsmsg_t *list = htsmsg_create_list();
  struct if_nameindex *ifnames = if_nameindex();

  if (ifnames) {
    struct if_nameindex *ifname;
    for (ifname = ifnames; ifname->if_name; ifname++) {
      htsmsg_t *entry = htsmsg_create_map();
      htsmsg_add_str(entry, "key", ifname->if_name);
      htsmsg_add_str(entry, "val", ifname->if_name);
      htsmsg_add_msg(list, NULL, entry);
    }
    if_freenameindex(ifnames);
  }

  return list;
#else
  return NULL;
#endif
}
