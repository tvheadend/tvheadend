/*
 *  TV Input - DVB - Support/Conversion functions
 *  Copyright (C) 2007 Andreas Öman
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_charset_tables.h"
#include "input.h"
#include "intlconv.h"
#include "lang_str.h"
#include "settings.h"

static int convert_iso_8859[16] = {
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1, 11, 12, 13
};
#define convert_utf8   14
#define convert_iso6937 15
#define convert_ucs2 16
#define convert_gb   17

#define conv_lower(dst, dstlen, c) \
  if (c >= 0x20 || c == '\n') { *(dst++) = c; (*dstlen)--; }

static inline size_t conv_gb(const uint8_t *src, size_t srclen,
                             char *dst, size_t *dstlen)
{
    ssize_t len;
    len = intlconv_to_utf8(dst, *dstlen, "gb2312", (char *)src, srclen);
    if (len < 0 || len > *dstlen)
      return -1;
    *dstlen -= len;
    return 0;
}

static inline int encode_utf8(unsigned int c, char *outb, int outleft)
{
  if (c <= 0x7F && outleft >= 1) {
    if (c >= 0x20 || c == '\n') {
      *outb = c;
      return 1;
    }
    return 0;
  } else if (c <= 0x7FF && outleft >=2) {
    *outb++ = ((c >>  6) & 0x1F) | 0xC0;
    *outb++ = ( c        & 0x3F) | 0x80;
    return 2;
  } else if (c <= 0xFFFF && outleft >= 3) {
    *outb++ = ((c >> 12) & 0x0F) | 0xE0;
    *outb++ = ((c >>  6) & 0x3F) | 0x80;
    *outb++ = ( c        & 0x3F) | 0x80;
    return 3;
  } else if (c <= 0x10FFFF && outleft >= 4) {
    *outb++ = ((c >> 18) & 0x07) | 0xF0;
    *outb++ = ((c >> 12) & 0x3F) | 0x80;
    *outb++ = ((c >>  6) & 0x3F) | 0x80;
    *outb++ = ( c        & 0x3F) | 0x80;
    return 4;
  } else {
    return -1;
  }
}

static inline size_t conv_UCS2(const uint8_t *src, size_t srclen,char *dst, size_t *dstlen)
{
  while (srclen>0 && (*dstlen)>0){
    uint16_t uc = *src<<8|*(src+1);
    if (uc >= 0xe080 && uc <= 0xe09f) {
      // codes 0xe080 - 0xe09f (control codes) are ignored except CR/LF
      if (uc == 0xe08a) {
        *dst = '\n';
        (*dstlen)--;
        dst++;
      }
    } else {
      int len = encode_utf8(uc, dst, *dstlen);
      if (len == -1) {
        errno = E2BIG;
        return -1;
      } else {
        (*dstlen) -= len;
        dst += len;
      }
    }
    srclen-=2;
    src+=2;
  }
  if (srclen>0) {
    errno = E2BIG;
    return -1;
  }
  return 0;
}

static inline size_t conv_utf8(const uint8_t *src, size_t srclen,
                               char *dst, size_t *dstlen)
{
  while (srclen>0 && (*dstlen)>0) {
    uint_fast8_t c = *src;
    if (c <= 0x7f) {
      conv_lower(dst, dstlen, c);
    } else {
      *(dst++) = c;
      (*dstlen)--;
    }
    srclen--;
    src++;
  }
  if (srclen>0) {
    errno = E2BIG;
    return -1;
  }
  return 0;
}

static inline size_t conv_8859(int conv,
                              const uint8_t *src, size_t srclen,
                              char *dst, size_t *dstlen)
{
  uint16_t *table = conv_8859_table[conv];

  while (srclen>0 && (*dstlen)>0) {
    uint_fast8_t c = *src;
    if (c <= 0x7f) {
      conv_lower(dst, dstlen, c);
    } else if (c <= 0x9f) {
      // codes 0x80 - 0x9f (control codes) are ignored except CR/LF
      if (c == 0x8a) {
        *dst = '\n';
        (*dstlen)--;
        dst++;
      }
    } else {
      // map according to character table, skipping
      // unmapped chars (value 0 in the table)
      uint_fast16_t uc = table[c-0xa0];
      if (uc != 0) {
        int len = encode_utf8(uc, dst, *dstlen);
        if (len == -1) {
          errno = E2BIG;
          return -1;
        } else {
          (*dstlen) -= len;
          dst += len;
        }
      }
    }
    srclen--;
    src++;
  }
  if (srclen>0) {
    errno = E2BIG;
    return -1;
  }
  return 0;
}

static inline size_t conv_6937(const uint8_t *src, size_t srclen,
                              char *dst, size_t *dstlen)
{
  while (srclen>0 && (*dstlen)>0) {
    uint_fast8_t c = *src;
    if (c <= 0x7f) {
      conv_lower(dst, dstlen, c);
    } else if (c <= 0x9f) {
      // codes 0x80 - 0x9f (control codes) are ignored except CR/LF
      if (c == 0x8a) {
        *dst = '\n';
        (*dstlen)--;
        dst++;
      }
    } else {
      uint16_t uc;
      if (c >= 0xc0 && c <= 0xcf) {
        // map two-byte sequence, skipping illegal combinations.
        if (srclen<2) {
          errno = EINVAL;
          return -1;
        }
        srclen--;
        src++;
        uint8_t c2 = *src;
        if (c2 == 0x20) {
          uc = iso6937_lone_accents[c-0xc0];
        } else if (c2 >= 0x41 && c2 <= 0x5a) {
          uc = iso6937_multi_byte[c-0xc0][c2-0x41];
        } else if (c2 >= 0x61 && c2 <= 0x7a) {
          uc = iso6937_multi_byte[c-0xc0][c2-0x61+26];
        } else {
          uc = 0;
        }
      } else {
        // map according to single character table, skipping
        // unmapped chars (value 0 in the table)
        uc = iso6937_single_byte[c-0xa0];
      }
      if (uc != 0) {
        int len = encode_utf8(uc, dst, *dstlen);
        if (len == -1) {
          errno = E2BIG;
          return -1;
        } else {
          (*dstlen) -= len;
          dst += len;
        }
      }
    }
    srclen--;
    src++;
  }
  if (srclen>0) {
    errno = E2BIG;
    return -1;
  }
  return 0;
}

static inline size_t dvb_convert(int conv,
                          const uint8_t *src, size_t srclen,
                          char *dst, size_t *dstlen)
{
  switch (conv) {
    case convert_utf8: return conv_utf8(src, srclen, dst, dstlen);
    case convert_iso6937: return conv_6937(src, srclen, dst, dstlen);
    case convert_gb: return conv_gb(src,srclen,dst,dstlen);
    case convert_ucs2:return conv_UCS2(src,srclen,dst,dstlen);
    default: return conv_8859(conv, src, srclen, dst, dstlen);
  }
}

/*
 * DVB String conversion according to EN 300 468, Annex A
 * Not all character sets are supported, but it should cover most of them
 */

int
dvb_get_string
  (char *dst, size_t dstlen, const uint8_t *src, size_t srclen, 
   const char *dvb_charset, dvb_string_conv_t *conv)
{
  int ic = -1;
  size_t len, outlen;
  int i, auto_pl_charset = 0;

  if(srclen < 1) {
    *dst = 0;
    return 0;
  }

  /* Check custom conversion */
  while (conv && conv->func) {
    if (conv->type == src[0])
      return conv->func(dst, &dstlen, src, srclen);
    conv++;
  }

  // check for automatic polish charset detection
  if (dvb_charset && strcmp("AUTO_POLISH", dvb_charset) == 0) {
    auto_pl_charset = 1;
    dvb_charset = NULL;
  }

  // automatic charset detection
  switch(src[0]) {
  case 0:
    *dst = 0; // empty string (confirmed!)
    return 0;

  case 0x01 ... 0x0b: /* ISO 8859-X */
    if (auto_pl_charset && (src[0] + 4) == 5)
      ic = convert_iso6937;
    else
      ic = convert_iso_8859[src[0] + 4];
    src++; srclen--;
    break;

  case 0x0c ... 0x0f: /* reserved for the future use */
    src++; srclen--;
    break;

  case 0x10: /* ISO 8859 - Table A.4 */
    if(srclen < 3 || src[1] != 0 || src[2] == 0 || src[2] > 0x0f)
      return -1;

    ic = convert_iso_8859[src[2]];
    src+=3; srclen-=3;
    break;
    
  case 0x11: /* ISO 10646 */
    ic = convert_ucs2;
    src++; srclen--;
    break;

  case 0x12: /* KSX1001-2004 - Korean Character Set - NYI! */
    src++; srclen--;
    break;

  case 0x13: /* GB-2312-1980 */
    ic = convert_gb;
    src++; srclen--;
    break;

  case 0x14: /* Big5 subset of ISO 10646 */
    ic = convert_ucs2;
    src++; srclen--;
    break;

  case 0x15: /* UTF-8 */
    ic = convert_utf8;
    src++; srclen--;
    break;

  case 0x16 ... 0x1e: /* reserved for the future use */
    src++; srclen--;
    break;

  case 0x1f: /* Described by encoding_type_id, TS 101 162 */
    return -1; /* NYI */

  default:
    if (auto_pl_charset)
      ic = convert_iso_8859[2];
    else
      ic = convert_iso6937;
    break;
  }

  // manual charset override
  if (dvb_charset != NULL && dvb_charset[0] != 0) {
    if (!strcmp(dvb_charset, "AUTO")) {
      // ignore
    } else if (sscanf(dvb_charset, "ISO-8859-%d", &i) > 0 && i > 0 && i < 16) {
      ic = convert_iso_8859[i];
    } else if (!strcmp(dvb_charset, "ISO-6937")) {
      ic = convert_iso6937;
    } else if (!strcmp(dvb_charset, "UTF-8")) {
      ic = convert_utf8;
    } else if (!strcmp(dvb_charset, "GB2312")) {
      ic = convert_gb;
    } else if (!strcmp(dvb_charset, "UCS2")) {
      ic = convert_ucs2;
    }
  }

  if(srclen < 1) {
    *dst = 0;
    return 0;
  }

  if(ic == -1)
    return -1;

  outlen = dstlen - 1;

  if (dvb_convert(ic, src, srclen, dst, &outlen) == -1)
    return -1;

  len = dstlen - outlen - 1;
  dst[len] = 0;
  return 0;
}


int
dvb_get_string_with_len(char *dst, size_t dstlen, 
			const uint8_t *buf, size_t buflen, const char *dvb_charset,
      dvb_string_conv_t *conv)
{
  int l = buf[0];

  if(l + 1 > buflen)
    return -1;

  if(dvb_get_string(dst, dstlen, buf + 1, l, dvb_charset, conv))
    return -1;

  return l + 1;
}


/**
 *
 */
void
atsc_utf16_to_utf8(const uint8_t *src, int len, char *buf, int buflen)
{
  int i, c, r;

  for(i = 0; i < len; i++) {
    c = (src[i * 2 + 0] << 8) | src[i * 2 + 1];

    if(buflen >= 7) {
      r = put_utf8(buf, c);
      buf += r;
      buflen -= r;
    }
  }
  *buf = 0;
}

/* Decode and convert ATSC Multiple String Structures to UTF-8.
 * 
 * refer to "ATSC Standard: Program and System Information Protocol for Terrestrial Broadcast and Cable"
 *          (Document A65/2013), Section 6.10, pages 79-82
 */

lang_str_t *
atsc_get_string
  (const uint8_t *src, size_t srclen)
{
  const int bufferSize = 255*3+1; /* Max length of a text segment (255) *
                                   * Max size of a UTF-8 for Unicode characters 0x0 .. 0x33FF (3) +
                                   * NULL terminator (1) */
  lang_str_t *ls = NULL;
  int i, j, k, stringcount, segmentcount;
  int compressiontype, mode, bytecount;
  char langcode[4];
  char buf[bufferSize]; /* UTF-8 string */
  int utf8size, remainingbufleft;
  char *bufpointer;

  stringcount = src[0];
  tvhtrace(LS_MPEGTS, "atsc-str: %d strings", stringcount);

  src++;
  srclen--;

  langcode[3] = '\0';

  for (i = 0; i < stringcount && srclen >= 4; i++) {
    langcode[0]  = src[0];
    langcode[1]  = src[1];
    langcode[2]  = src[2];
    segmentcount = src[3];

    tvhtrace(LS_MPEGTS, "atsc-str:  %d: lang '%s', segments %d", i, langcode, segmentcount);

    src    += 4;
    srclen -= 4;

    /* Step through the list of text segments, decoding each and append them all together. */
    for (j = 0; j < segmentcount && srclen >= 3; j++) {
      /* Decode text segment header */
      compressiontype = src[0];
      mode            = src[1];
      bytecount       = src[2];

      src    += 3;
      srclen -= 3;

      if (bytecount > srclen)
        return ls;

      /* Only supports compression type == 0 (none) and 
       * text modes == 0x0 .. 0x6, 0x9 .. 0x10, 0x20 .. 0x27, 0x30 .. 0x33 */

      if (compressiontype == 0 && (     /* No Compression and one of these: */
           (mode >= 0x0 && mode <= 0x6) ||   /* Unicode range 0x0000 .. 0x06FF */
           (mode >= 0x9 && mode <= 0x10) ||  /* Unicode range 0x0900 .. 0x10FF */
           (mode >= 0x20 && mode <= 0x27) || /* Unicode range 0x2000 .. 0x27FF */
           (mode >= 0x30 && mode <= 0x33)    /* Unicode range 0x3000 .. 0x33FF */
         )) {
        tvhtrace(LS_MPEGTS, "atsc-str:    %d: comptype 0x%02x, mode 0x%02x, %d bytes: '%.*s'",
                 j, compressiontype, mode, bytecount, bytecount, src);

        /* Convert each decoded Unicode character to UTF-8. */
        for(k = 0, bufpointer = buf, remainingbufleft = bufferSize; k < bytecount; k++) {
          /* Make sure there is enough buffer left for the next (or last) UTF-8 character
           * [Max # bytes in a single UTF-8 character (3) + NULL terminator (1)] */
          if(remainingbufleft > (3+1)) {
            /* Construct the Unicode character and convert to UTF-8. */
            utf8size = put_utf8(bufpointer, (mode << 8) | src[k]);
            bufpointer += utf8size;
            remainingbufleft -= utf8size;
          } else {
            /* We have run out of buffer space for this text segment, then stop and truncate.
             * This can only happen if 'bufferSize' is too small. */
            tvhtrace(LS_MPEGTS, "atsc_get_string: bufferSize is too small");
            break;
          }
        }
        *bufpointer = '\0';

        if (ls == NULL)
          ls = lang_str_create();
        lang_str_append(ls, buf, langcode);
      }	else if (compressiontype == 0 && mode == 0x3F) {
        tvhtrace(LS_MPEGTS, "atsc-str:    %d: comptype 0x%02x, mode 0x%02x, %d bytes: '%.*s'",
                 j, compressiontype, mode, bytecount, bytecount, src);

        /* let the atsc_utf16_to_utf8 function do the conversion;
         * bytecount/2: since the function is handling both utf16 bytes in one count only half the length is needed */
      	atsc_utf16_to_utf8(src, bytecount/2, buf, bufferSize);

        if (ls == NULL)
          ls = lang_str_create();
        lang_str_append(ls, buf, langcode);
      } else {
        /* Unsupported text segment types:
         *
         * - compression type == 0x1 (Huffman-like coding with a fixed codebook)
         * - compression type == 0x2 (Huffman-like coding with another fixed codebook)
         * - compression type == 0x3 .. 0xFF (reserved)
         *
         * - text mode == 0x3E (Standard Compression Scheme for Unicode)
         * - text mode == 0x40, 0x41 (ATSC Standard for Taiwan)
         * - text mode == 0x48 (ATSC Standard for South Korea)
         *
         * - text mode == 0x7, 0x8, 0x11 .. 0x1F, 0x28 .. 0x2F, 0x34 .. 0x3D, 0x42 .. 0x47, 0x49 .. 0xFF (reserved)
         */

        tvhtrace(LS_MPEGTS, "atsc-str:    %d: comptype 0x%02x, mode 0x%02x, %d bytes",
                 j, compressiontype, mode, bytecount);

        /* For text segments that are not supported, write a terse diagnostic text indicating
         * the unsupported type instead of the text segment. */
        snprintf(buf, bufferSize - 1, "[comptype=0x%02X,mode=0x%02X]", compressiontype, mode);
        if (ls == NULL)
          ls = lang_str_create();
        lang_str_append(ls, buf, langcode);
      }

      /* Move on to the next text segment. */
      src += bytecount;
      srclen -= bytecount;
    }
  }
  return ls;
}

/*
 *
 */

static struct strtab dvb_timezone_strtab[] = {
  { N_("UTC"),        0 },
  { N_("Local (server) time"), 1 },
  { N_("UTC- 1"),    -1*60 },
  { N_("UTC- 2"),    -2*60 },
  { N_("UTC- 2:30"), -2*60-30 },
  { N_("UTC- 3"),    -3*60 },
  { N_("UTC- 3:30"), -3*60-30 },
  { N_("UTC- 4"),    -4*60 },
  { N_("UTC- 4:30"), -4*60-30 },
  { N_("UTC- 5"),    -5*60 },
  { N_("UTC- 6"),    -6*60 },
  { N_("UTC- 7"),    -7*60 },
  { N_("UTC- 8"),    -8*60 },
  { N_("UTC- 9"),    -9*60 },
  { N_("UTC- 9:30"), -9*60-30 },
  { N_("UTC-10"),    -10*60 },
  { N_("UTC-11"),    -11*60 },
  { N_("UTC+ 1"),     1*60 },
  { N_("UTC+ 2"),     2*60 },
  { N_("UTC+ 3"),     3*60 },
  { N_("UTC+ 4"),     4*60 },
  { N_("UTC+ 4:30"),  4*60+30 },
  { N_("UTC+ 5"),     5*60 },
  { N_("UTC+ 5:30"),  5*60+30 },
  { N_("UTC+ 5:45"),  5*60+45 },
  { N_("UTC+ 6"),     6*60 },
  { N_("UTC+ 6:30"),  6*60+30 },
  { N_("UTC+ 7"),     7*60 },
  { N_("UTC+ 8"),     8*60 },
  { N_("UTC+ 8:45"),  8*60+45 },
  { N_("UTC+ 9"),     9*60 },
  { N_("UTC+ 9:30"),  9*60+30 },
  { N_("UTC+10"),     10*60 },
  { N_("UTC+10:30"),  10*60+30 },
  { N_("UTC+11"),     11*60 },
  { N_("UTC+12"),     12*60 },
  { N_("UTC+12:45"),  12*60+45 },
  { N_("UTC+13"),     13*60 },
  { N_("UTC+14"),     14*60 }
};

htsmsg_t *
dvb_timezone_enum ( void *p, const char *lang )
{
  return strtab2htsmsg(dvb_timezone_strtab, 1, lang);
}

/*
 * DVB time and date functions
 */

time_t
dvb_convert_date(const uint8_t *dvb_buf, int tmzone)
{
  int i;
  int year, month, day, hour, min, sec;
  long int mjd;
  struct tm dvb_time;

  mjd = (dvb_buf[0] & 0xff) << 8;
  mjd += (dvb_buf[1] & 0xff);
  hour = bcdtoint(dvb_buf[2] & 0xff);
  min = bcdtoint(dvb_buf[3] & 0xff);
  sec = bcdtoint(dvb_buf[4] & 0xff);
  /*
   * Use the routine specified in ETSI EN 300 468 V1.4.1,
   * "Specification for Service Information in Digital Video Broadcasting"
   * to convert from Modified Julian Date to Year, Month, Day.
   */
  year = (int) ((mjd - 15078.2) / 365.25);
  month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
  day = mjd - 14956 - (int) (year * 365.25) - (int) (month * 30.6001);
  if (month == 14 || month == 15)
    i = 1;
  else
    i = 0;
  year += i;
  month = month - 1 - i * 12;

  dvb_time.tm_sec = sec;
  dvb_time.tm_min = min;
  dvb_time.tm_hour = hour;
  dvb_time.tm_mday = day;
  dvb_time.tm_mon = month - 1;
  dvb_time.tm_year = year;
  dvb_time.tm_isdst = -1;
  dvb_time.tm_wday = 0;
  dvb_time.tm_yday = 0;

  if (tmzone == 0) /* UTC */
    return timegm(&dvb_time);
  if (tmzone == 1) /* Local time */
    return mktime(&dvb_time);

  /* apply offset */
  return timegm(&dvb_time) - tmzone * 60;
}

static time_t _gps_leap_seconds[17] = {
	362793600,
	394329600,
	425865600,
	489024000,
	567993600,
	631152000,
	662688000,
	709948800,
	741484800,
	773020800,
	820454400,
	867715200,
	915148800,
	1136073600,
	1230768000,
	1341100800,
	1435708800,
};

time_t
atsc_convert_gpstime(uint32_t gpstime)
{
  int i;
  time_t out = gpstime + 315964800; // Add Unix - GPS epoch

  for (i = (sizeof(_gps_leap_seconds)/sizeof(time_t)) - 1; i >= 0; i--) {
    if (out > _gps_leap_seconds[i]) {
      out -= i+1;
      break;
    }
  }

  return out;
}

/*
 * DVB API helpers
 */
#if ENABLE_MPEGTS_DVB

htsmsg_t *satellites;

#define dvb_str2val(p)\
const char *dvb_##p##2str (int p)         { return val2str(p, p##tab); }\
int         dvb_str2##p   (const char *p) { return str2val(p, p##tab); }

#define DVB_EOD -10	/* end-of-data */

static const char *dvb_common2str(int p)
{
  if (p == 0)
    return "NONE";
  if (p == 1)
    return "AUTO";
  return NULL;
}

static int dvb_str2common(const char *p)
{
  if (strcmp(p, "NONE") == 0)
    return 0;
  if (strcmp(p, "AUTO") == 0)
    return 1;
  return DVB_EOD;
}

static int dvb_verify(int val, int *table)
{
  while (*table != DVB_EOD) {
    if (val == *table)
      return val;
    table++;
  }
  return 0; /* NONE */
}

const char *dvb_rolloff2str(int p)
{
  static __thread char buf[16];
  const char *res = dvb_common2str(p);
  if (res)
    return res;
  sprintf(buf, "%02i", p / 10);
  return buf;
}

int dvb_str2rolloff(const char *p)
{
  static int rolloff_table[] = {
    DVB_ROLLOFF_5,
    DVB_ROLLOFF_10,
    DVB_ROLLOFF_15,
    DVB_ROLLOFF_20,
    DVB_ROLLOFF_25,
    DVB_ROLLOFF_35,
    DVB_EOD,
  };
  int res = dvb_str2common(p);
  if (res != DVB_EOD)
    return res;
  return dvb_verify(atoi(p) * 10, rolloff_table);
}

static const struct strtab delsystab[] = {
  { "NONE",         DVB_SYS_NONE },
  { "DVB-C",        DVB_SYS_DVBC_ANNEX_A },
  { "DVBC/ANNEX_A", DVB_SYS_DVBC_ANNEX_A },
  { "DVBC_ANNEX_A", DVB_SYS_DVBC_ANNEX_A },
  { "ATSC-C",       DVB_SYS_DVBC_ANNEX_B },
  { "CableCARD",    DVB_SYS_DVBC_ANNEX_B },
  { "DVBC/ANNEX_B", DVB_SYS_DVBC_ANNEX_B },
  { "DVBC_ANNEX_B", DVB_SYS_DVBC_ANNEX_B },
  { "DVB-C/ANNEX-C",DVB_SYS_DVBC_ANNEX_C },
  { "DVBC/ANNEX_C", DVB_SYS_DVBC_ANNEX_C },
  { "DVBC_ANNEX_C", DVB_SYS_DVBC_ANNEX_C },
  { "DVBC_ANNEX_AC",DVB_SYS_DVBC_ANNEX_A }, /* for compatibility */
  { "DVB-T",        DVB_SYS_DVBT },
  { "DVBT",         DVB_SYS_DVBT },
  { "DVB-T2",       DVB_SYS_DVBT2 },
  { "DVBT2",        DVB_SYS_DVBT2 },
  { "DVB-S",        DVB_SYS_DVBS },
  { "DVBS",         DVB_SYS_DVBS },
  { "DVB-S2",       DVB_SYS_DVBS2 },
  { "DVBS2",        DVB_SYS_DVBS2 },
  { "DVB-H",        DVB_SYS_DVBH },
  { "DVBH",         DVB_SYS_DVBH },
  { "ISDB-T",       DVB_SYS_ISDBT },
  { "ISDBT",        DVB_SYS_ISDBT },
  { "ISDB-S",       DVB_SYS_ISDBS },
  { "ISDBS",        DVB_SYS_ISDBS },
  { "ISDB-C",       DVB_SYS_ISDBC },
  { "ISDBC",        DVB_SYS_ISDBC },
  { "ATSC-T",       DVB_SYS_ATSC },
  { "ATSC",         DVB_SYS_ATSC },
  { "ATSCM-H",      DVB_SYS_ATSCMH },
  { "ATSCMH",       DVB_SYS_ATSCMH },
  { "DTMB",         DVB_SYS_DTMB },
  { "DMBTH",        DVB_SYS_DTMB },	/* for compatibility */
  { "CMMB",         DVB_SYS_CMMB },
  { "DAB",          DVB_SYS_DAB },
  { "DSS",          DVB_SYS_DSS },
  { "TURBO",        DVB_SYS_TURBO }
};
dvb_str2val(delsys);

int
dvb_delsys2type ( mpegts_network_t *ln, dvb_fe_delivery_system_t delsys )
{
  switch (delsys) {
    case DVB_SYS_DVBC_ANNEX_A:
    case DVB_SYS_DVBC_ANNEX_C:
      return DVB_TYPE_C;
    case DVB_SYS_DVBT:
    case DVB_SYS_DVBT2:
    case DVB_SYS_TURBO:
      return DVB_TYPE_T;
    case DVB_SYS_DVBS:
    case DVB_SYS_DVBS2:
      return DVB_TYPE_S;
    case DVB_SYS_ATSC:
    case DVB_SYS_ATSCMH:
      return DVB_TYPE_ATSC_T;
    case DVB_SYS_DVBC_ANNEX_B:
      if (ln && idnode_is_instance(&ln->mn_id, &dvb_network_dvbc_class))
        return DVB_TYPE_C;
      if (ln && idnode_is_instance(&ln->mn_id, &dvb_network_cablecard_class))
        return DVB_TYPE_CABLECARD;
      else
        return DVB_TYPE_ATSC_C;
    case DVB_SYS_ISDBT:
      return DVB_TYPE_ISDB_T;
    case DVB_SYS_ISDBC:
      return DVB_TYPE_ISDB_C;
    case DVB_SYS_ISDBS:
      return DVB_TYPE_ISDB_S;
    case DVB_SYS_DTMB:
      return DVB_TYPE_DTMB;
    case DVB_SYS_DAB:
      return DVB_TYPE_DAB;
    default:
      return DVB_TYPE_NONE;
  }
}

const char *dvb_fec2str(int p)
{
  static __thread char buf[16];
  const char *res = dvb_common2str(p);
  if (res)
    return res;
  sprintf(buf, "%i/%i", p / 100, p % 100);
  return buf;
}

int dvb_str2fec(const char *p)
{
  static int fec_table[] = {
    DVB_FEC_1_2,
    DVB_FEC_1_3,
    DVB_FEC_1_4,
    DVB_FEC_1_5,
    DVB_FEC_2_3,
    DVB_FEC_2_5,
    DVB_FEC_2_9,
    DVB_FEC_3_4,
    DVB_FEC_3_5,
    DVB_FEC_4_5,
    DVB_FEC_4_15,
    DVB_FEC_5_6,
    DVB_FEC_5_9,
    DVB_FEC_6_7,
    DVB_FEC_7_8,
    DVB_FEC_7_9,
    DVB_FEC_7_15,
    DVB_FEC_8_9,
    DVB_FEC_8_15,
    DVB_FEC_9_10,
    DVB_FEC_9_20,
    DVB_FEC_11_15,
    DVB_FEC_11_20,
    DVB_FEC_11_45,
    DVB_FEC_13_18,
    DVB_FEC_13_45,
    DVB_FEC_14_45,
    DVB_FEC_23_36,
    DVB_FEC_25_36,
    DVB_FEC_26_45,
    DVB_FEC_28_45,
    DVB_FEC_29_45,
    DVB_FEC_31_45,
    DVB_FEC_32_45,
    DVB_FEC_77_90,
    DVB_EOD,
  };
  int res = dvb_str2common(p);
  int hi, lo;
  if (res != DVB_EOD)
    return res;
  hi = lo = 0;
  sscanf(p, "%i/%i", &hi, &lo);
  return dvb_verify(hi * 100 + lo, fec_table);
}

static const struct strtab qamtab[] = {
  { "NONE",      DVB_MOD_NONE },
  { "AUTO",      DVB_MOD_AUTO },
  { "QPSK",      DVB_MOD_QPSK },
  { "QAM4NR",    DVB_MOD_QAM_4_NR },
  { "QAM/AUTO",  DVB_MOD_QAM_AUTO },
  { "QAM-AUTO",  DVB_MOD_QAM_AUTO },
  { "QAM/16",    DVB_MOD_QAM_16 },
  { "QAM16",     DVB_MOD_QAM_16 },
  { "QAM/32",    DVB_MOD_QAM_32 },
  { "QAM32",     DVB_MOD_QAM_32 },
  { "QAM/64",    DVB_MOD_QAM_64 },
  { "QAM64",     DVB_MOD_QAM_64 },
  { "QAM/128",   DVB_MOD_QAM_128 },
  { "QAM128",    DVB_MOD_QAM_128 },
  { "QAM/256",   DVB_MOD_QAM_256 },
  { "QAM256",    DVB_MOD_QAM_256 },
  { "QAM/1024",  DVB_MOD_QAM_1024 },
  { "QAM1024",   DVB_MOD_QAM_1024 },
  { "QAM/4096",  DVB_MOD_QAM_4096 },
  { "QAM4096",   DVB_MOD_QAM_4096 },
  { "VSB/8",     DVB_MOD_VSB_8 },
  { "8VSB",      DVB_MOD_VSB_8 },
  { "VSB/16",    DVB_MOD_VSB_16 },
  { "16VSB",     DVB_MOD_VSB_16 },
  { "PSK/8",     DVB_MOD_PSK_8 },
  { "8PSK",      DVB_MOD_PSK_8 },
  { "DQPSK",     DVB_MOD_DQPSK },
  { "BPSK",      DVB_MOD_BPSK },
  { "BPSK-S",    DVB_MOD_BPSK_S },
  { "16APSK",    DVB_MOD_APSK_16 },
  { "32APSK",    DVB_MOD_APSK_32 },
  { "64APSK",    DVB_MOD_APSK_64 },
  { "128APSK",   DVB_MOD_APSK_128 },
  { "256APSK",   DVB_MOD_APSK_256 },
  { "8APSK-L",   DVB_MOD_APSK_8_L },
  { "16APSK-L",  DVB_MOD_APSK_16_L },
  { "32APSK-L",  DVB_MOD_APSK_32_L },
  { "64APSK-L",  DVB_MOD_APSK_64_L },
  { "128APSK-L", DVB_MOD_APSK_128_L },
  { "256APSK-L", DVB_MOD_APSK_256_L },
};
dvb_str2val(qam);

const char *dvb_bw2str(int p)
{
  static __thread char buf[17];
  const char *res = dvb_common2str(p);
  if (res)
    return res;
  if (p % 1000)
    sprintf(buf, "%i.%iMHz", p / 1000, p % 1000);
  else
    sprintf(buf, "%iMHz", p / 1000);
  return buf;
}

int dvb_str2bw(const char *p)
{
  static int bw_table[] = {
    DVB_BANDWIDTH_1_712_MHZ,
    DVB_BANDWIDTH_5_MHZ,
    DVB_BANDWIDTH_6_MHZ,
    DVB_BANDWIDTH_7_MHZ,
    DVB_BANDWIDTH_8_MHZ,
    DVB_BANDWIDTH_10_MHZ,
    DVB_EOD,
  };
  int len, res = dvb_str2common(p);
  int hi, lo;
  if (res != DVB_EOD)
    return res;
  len = strlen(p);
  hi = lo = 0;
  sscanf(p, "%i.%i", &hi, &lo);
  if (len > 3 && strcmp(&p[len-3], "MHz") == 0)
    hi = hi * 1000 + lo;
  return dvb_verify(hi, bw_table);
}

static const struct strtab invertab[] = {
  { "NONE",  DVB_INVERSION_UNDEFINED },
  { "AUTO",  DVB_INVERSION_AUTO },
  { "ON",    DVB_INVERSION_ON },
  { "OFF",   DVB_INVERSION_OFF },
};
dvb_str2val(inver);

static const struct strtab modetab[] = {
  { "NONE",  DVB_TRANSMISSION_MODE_NONE },
  { "AUTO",  DVB_TRANSMISSION_MODE_AUTO },
  { "1k",    DVB_TRANSMISSION_MODE_1K },
  { "2k",    DVB_TRANSMISSION_MODE_2K },
  { "8k",    DVB_TRANSMISSION_MODE_8K },
  { "4k",    DVB_TRANSMISSION_MODE_4K },
  { "16k",   DVB_TRANSMISSION_MODE_16K },
  { "32k",   DVB_TRANSMISSION_MODE_32K },
  { "C1",    DVB_TRANSMISSION_MODE_C1 },
  { "C3780", DVB_TRANSMISSION_MODE_C3780 },
};
dvb_str2val(mode);

static const struct strtab guardtab[] = {
  { "NONE",   DVB_GUARD_INTERVAL_NONE },
  { "AUTO",   DVB_GUARD_INTERVAL_AUTO },
  { "1/4",    DVB_GUARD_INTERVAL_1_4 },
  { "1/8",    DVB_GUARD_INTERVAL_1_8 },
  { "1/32",   DVB_GUARD_INTERVAL_1_32 },
  { "1/16",   DVB_GUARD_INTERVAL_1_16 },
  { "1/128",  DVB_GUARD_INTERVAL_1_128 },
  { "19/128", DVB_GUARD_INTERVAL_19_128 },
  { "19/256", DVB_GUARD_INTERVAL_19_256 },
  { "PN420",  DVB_GUARD_INTERVAL_PN420 },
  { "PN595",  DVB_GUARD_INTERVAL_PN595 },
  { "PN945",  DVB_GUARD_INTERVAL_PN945 },
};
dvb_str2val(guard);

static const struct strtab hiertab[] = {
  { "NONE", DVB_HIERARCHY_NONE },
  { "AUTO", DVB_HIERARCHY_AUTO },
  { "1",    DVB_HIERARCHY_1 },
  { "2",    DVB_HIERARCHY_2 },
  { "4",    DVB_HIERARCHY_4 },
};
dvb_str2val(hier);

static const struct strtab poltab[] = {
  { "V", DVB_POLARISATION_VERTICAL },
  { "H", DVB_POLARISATION_HORIZONTAL },
  { "L", DVB_POLARISATION_CIRCULAR_LEFT },
  { "R", DVB_POLARISATION_CIRCULAR_RIGHT },
  { "O", DVB_POLARISATION_OFF },
};
dvb_str2val(pol);

static const struct strtab typetab[] = {
  {"DVB-T",     DVB_TYPE_T},
  {"DVB-C",     DVB_TYPE_C},
  {"DVB-S",     DVB_TYPE_S},
  {"ATSC-T",    DVB_TYPE_ATSC_T},
  {"ATSC-C",    DVB_TYPE_ATSC_C},
  {"CableCARD", DVB_TYPE_CABLECARD},
  {"ISDB-T",    DVB_TYPE_ISDB_T},
  {"ISDB-C",    DVB_TYPE_ISDB_C},
  {"ISDB-S",    DVB_TYPE_ISDB_S},
  {"DAB",       DVB_TYPE_DAB},
  {"DVBT",      DVB_TYPE_T},
  {"DVBC",      DVB_TYPE_C},
  {"DVBS",      DVB_TYPE_S},
  {"ATSC",      DVB_TYPE_ATSC_T},
  {"ATSCT",     DVB_TYPE_ATSC_T},
  {"ATSCC",     DVB_TYPE_ATSC_C},
  {"ISDBT",     DVB_TYPE_ISDB_T},
  {"ISDBC",     DVB_TYPE_ISDB_C},
  {"ISDBS",     DVB_TYPE_ISDB_S}
};
dvb_str2val(type);

static const struct strtab pilottab[] = {
  {"NONE", DVB_PILOT_NONE},
  {"AUTO", DVB_PILOT_AUTO},
  {"ON",   DVB_PILOT_ON},
  {"OFF",  DVB_PILOT_OFF}
};
dvb_str2val(pilot);

static const struct strtab plsmodetab[] = {
  {"ROOT", DVB_PLS_ROOT},
  {"GOLD", DVB_PLS_GOLD},
  {"COMBO", DVB_PLS_COMBO},
};
dvb_str2val(plsmode);
#undef dvb_str2val


void
dvb_mux_conf_init ( mpegts_network_t *ln,
                    dvb_mux_conf_t *dmc,
                    dvb_fe_delivery_system_t delsys )
{
  memset(dmc, 0, sizeof(*dmc));
  dmc->dmc_fe_type      = dvb_delsys2type(ln, delsys);
  dmc->dmc_fe_delsys    = delsys;
  dmc->dmc_fe_inversion = DVB_INVERSION_AUTO;
  dmc->dmc_fe_pilot     = DVB_PILOT_AUTO;
  dmc->dmc_fe_stream_id = DVB_NO_STREAM_ID_FILTER;
  dmc->dmc_fe_pls_mode  = DVB_PLS_GOLD;
  switch (dmc->dmc_fe_type) {
  case DVB_TYPE_S:
    dmc->u.dmc_fe_qpsk.orbital_pos = INT_MAX;
    break;
  default:
    break;
  }
}


static int
dvb_mux_conf_str_dvbt ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  char hp[16];
  snprintf(hp, sizeof(hp), "%s", dvb_fec2str(dmc->u.dmc_fe_ofdm.code_rate_HP));
  return
  snprintf(buf, bufsize,
           "%s freq %d bw %s cons %s hier %s code_rate %s:%s guard %s trans %s plp_id %d",
           dvb_delsys2str(dmc->dmc_fe_delsys),
           dmc->dmc_fe_freq,
           dvb_bw2str(dmc->u.dmc_fe_ofdm.bandwidth),
           dvb_qam2str(dmc->dmc_fe_modulation),
           dvb_hier2str(dmc->u.dmc_fe_ofdm.hierarchy_information),
           hp, dvb_fec2str(dmc->u.dmc_fe_ofdm.code_rate_LP),
           dvb_guard2str(dmc->u.dmc_fe_ofdm.guard_interval),
           dvb_mode2str(dmc->u.dmc_fe_ofdm.transmission_mode),
           dmc->dmc_fe_stream_id);
}

static int
dvb_mux_conf_str_dvbc ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  const char *delsys;
  if (dmc->dmc_fe_type == DVB_TYPE_C &&
      dmc->dmc_fe_delsys == DVB_SYS_DVBC_ANNEX_B)
    delsys = "DVB-C/ANNEX_B";
  else
    delsys = dvb_delsys2str(dmc->dmc_fe_delsys);
  return
  snprintf(buf, bufsize,
           "%s freq %d sym %d mod %s fec %s ds %d plp %d",
           delsys,
           dmc->dmc_fe_freq,
           dmc->u.dmc_fe_qam.symbol_rate,
           dvb_qam2str(dmc->dmc_fe_modulation),
           dvb_fec2str(dmc->u.dmc_fe_qam.fec_inner),
           dmc->dmc_fe_data_slice,
           dmc->dmc_fe_stream_id);
}

static int
dvb_mux_conf_str_dvbs ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  const char *pol = dvb_pol2str(dmc->u.dmc_fe_qpsk.polarisation);
  const int satpos = dmc->u.dmc_fe_qpsk.orbital_pos;
  char satbuf[16];
  if (satpos != INT_MAX) {
    snprintf(satbuf, sizeof(satbuf), "%d.%d%c ", abs(satpos) / 10, abs(satpos) % 10, satpos < 0 ? 'W' : 'E');
  } else {
    satbuf[0] = '\0';
  }
  return
  snprintf(buf, bufsize,
           "%s %sfreq %d %c sym %d fec %s mod %s roff %s is_id %d pls_mode %s pls_code %d",
           dvb_delsys2str(dmc->dmc_fe_delsys),
           satbuf,
           dmc->dmc_fe_freq,
           pol ? pol[0] : 'X',
           dmc->u.dmc_fe_qpsk.symbol_rate,
           dvb_fec2str(dmc->u.dmc_fe_qpsk.fec_inner),
           dvb_qam2str(dmc->dmc_fe_modulation),
           dvb_rolloff2str(dmc->dmc_fe_rolloff),
           dmc->dmc_fe_stream_id,
           dvb_plsmode2str(dmc->dmc_fe_pls_mode),
           dmc->dmc_fe_pls_code);
}

static int
dvb_mux_conf_str_atsc_t ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  return
  snprintf(buf, bufsize,
           "%s freq %d mod %s",
           dvb_delsys2str(dmc->dmc_fe_delsys),
           dmc->dmc_fe_freq,
           dvb_qam2str(dmc->dmc_fe_modulation));
}

static int
dvb_mux_conf_str_cablecard(dvb_mux_conf_t *dmc, char *buf, size_t bufsize)
{
  return snprintf(buf, bufsize, "%s channel %u",
      dvb_type2str(dmc->dmc_fe_type),
      dmc->u.dmc_fe_cablecard.vchannel);
}

static int
dvb_mux_conf_str_isdb_t ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  char hp[16];
  snprintf(hp, sizeof(hp), "%s", dvb_fec2str(dmc->u.dmc_fe_ofdm.code_rate_HP));
  return
  snprintf(buf, bufsize,
           "%s freq %d bw %s guard %s A (%s,%s,%d,%d) B (%s,%s,%d,%d) C (%s,%s,%d,%d)",
           dvb_delsys2str(dmc->dmc_fe_delsys),
           dmc->dmc_fe_freq,
           dvb_bw2str(dmc->u.dmc_fe_isdbt.bandwidth),
           dvb_guard2str(dmc->u.dmc_fe_isdbt.guard_interval),
           dvb_fec2str(dmc->u.dmc_fe_isdbt.layers[0].fec),
           dvb_qam2str(dmc->u.dmc_fe_isdbt.layers[0].modulation),
           dmc->u.dmc_fe_isdbt.layers[0].segment_count,
           dmc->u.dmc_fe_isdbt.layers[0].time_interleaving,
           dvb_fec2str(dmc->u.dmc_fe_isdbt.layers[1].fec),
           dvb_qam2str(dmc->u.dmc_fe_isdbt.layers[1].modulation),
           dmc->u.dmc_fe_isdbt.layers[1].segment_count,
           dmc->u.dmc_fe_isdbt.layers[1].time_interleaving,
           dvb_fec2str(dmc->u.dmc_fe_isdbt.layers[2].fec),
           dvb_qam2str(dmc->u.dmc_fe_isdbt.layers[2].modulation),
           dmc->u.dmc_fe_isdbt.layers[2].segment_count,
           dmc->u.dmc_fe_isdbt.layers[2].time_interleaving);
}

int
dvb_mux_conf_str ( dvb_mux_conf_t *dmc, char *buf, size_t bufsize )
{
  switch (dmc->dmc_fe_type) {
  case DVB_TYPE_NONE:
    return
      snprintf(buf, bufsize, "NONE %s", dvb_delsys2str(dmc->dmc_fe_delsys));
  case DVB_TYPE_T:
    return dvb_mux_conf_str_dvbt(dmc, buf, bufsize);
  case DVB_TYPE_C:
  case DVB_TYPE_ATSC_C:
  case DVB_TYPE_ISDB_C:
    return dvb_mux_conf_str_dvbc(dmc, buf, bufsize);
  case DVB_TYPE_S:
    return dvb_mux_conf_str_dvbs(dmc, buf, bufsize);
  case DVB_TYPE_ATSC_T:
    return dvb_mux_conf_str_atsc_t(dmc, buf, bufsize);
  case DVB_TYPE_CABLECARD:
    return dvb_mux_conf_str_cablecard(dmc, buf, bufsize);
  case DVB_TYPE_ISDB_T:
    return dvb_mux_conf_str_isdb_t(dmc, buf, bufsize);
  default:
    return
      snprintf(buf, bufsize, "UNKNOWN MUX CONFIG");
  }
}

const char *
dvb_sat_position_to_str(int position, char *buf, size_t buflen)
{
  const int dec = position % 10;

  if (!buf || !buflen)
    return "";
  snprintf(buf, buflen, "%d", abs(position / 10));
  if (dec)
    snprintf(buf + strlen(buf), buflen - strlen(buf), ".%d", abs(dec));
  snprintf(buf + strlen(buf), buflen - strlen(buf), "%c", position < 0 ? 'W' : 'E');
  return buf;
}

int
dvb_sat_position_from_str( const char *buf )
{
  const char *s = buf;
  int min, maj;
  char c;

  if (!buf)
    return INT_MAX;
  maj = atoi(s);
  while (*s && *s != '.')
    s++;
  min = *s == '.' ? atoi(s + 1) : 0;
  if (*s != '.') s = buf;
  do {
    c = *s++;
  } while (c && c != 'W' && c != 'E');
  if (!c)
    return INT_MAX;
  if (maj > 180 || maj < 0)
    return INT_MAX;
  if (min > 9 || min < 0)
    return INT_MAX;
  return (maj * 10 + min) * (c == 'W' ? -1 : 1);
}

uint32_t
dvb_sat_pls( dvb_mux_conf_t *dmc )
{
  if (dmc->dmc_fe_pls_mode == DVB_PLS_ROOT) {
    uint32_t x, g;
    const uint32_t root = dmc->dmc_fe_pls_code & 0x3ffff;

    for (g = 0, x = 1; g < 0x3ffff; g++)  {
      if (root == x)
        return g;
      x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1);
    }
    return 0x3ffff;
  }
  return dmc->dmc_fe_pls_code & 0x3ffff;
}

#endif /* ENABLE_MPEGTS_DVB */

/**
 *
 */
void dvb_init( void )
{
#if ENABLE_MPEGTS_DVB
  satellites = hts_settings_load("satellites");
#endif
}

void dvb_done( void )
{
#if ENABLE_MPEGTS_DVB
  htsmsg_destroy(satellites);
#endif
}
