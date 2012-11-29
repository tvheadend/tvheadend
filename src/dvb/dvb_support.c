/*
 *  TV Input - DVB - Support functions
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

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>

#include "tvheadend.h"
#include "dvb_support.h"
#include "dvb.h"
#include "dvb_charset_tables.h"

static int convert_iso_8859[16] = {
  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1, 11, 12, 13
};
#define convert_utf8   14
#define convert_iso6937 15

static inline int encode_utf8(unsigned int c, char *outb, int outleft)
{
  if (c <= 0x7F && outleft >= 1) {
    *outb = c;
    return 1;
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

static inline size_t conv_utf8(const uint8_t *src, size_t srclen,
                              char *dst, size_t *dstlen)
{
  while (srclen>0 && (*dstlen)>0) {
    *dst = (char) *src;
    srclen--; (*dstlen)--;
    src++; dst++;
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
    uint8_t c = *src;
    if (c <= 0x7f) {
      // lower half of iso-8859-* is identical to utf-8
      *dst = (char) *src;
      (*dstlen)--;
      dst++;
    } else if (c <= 0x9f) {
      // codes 0x80 - 0x9f (control codes) are ignored
    } else {
      // map according to character table, skipping
      // unmapped chars (value 0 in the table)
      uint16_t uc = table[c-0xa0];
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
    uint8_t c = *src;
    if (c <= 0x7f) {
      // lower half of iso6937 is identical to utf-8
      *dst = (char) *src;
      (*dstlen)--;
      dst++;
    } else if (c <= 0x9f) {
      // codes 0x80 - 0x9f (control codes) are ignored
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
    default: return conv_8859(conv, src, srclen, dst, dstlen);
  }
}

/*
 * DVB String conversion according to EN 300 468, Annex A
 * Not all character sets are supported, but it should cover most of them
 */

int
dvb_get_string(char *dst, size_t dstlen, const uint8_t *src, size_t srclen, const char *dvb_charset, dvb_string_conv_t *conv)
{
  int ic;
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
  if (dvb_charset && strcmp("PL_AUTO", dvb_charset) == 0) {
    auto_pl_charset = 1;
    dvb_charset = NULL;
  }

  // automatic charset detection
  switch(src[0]) {
  case 0:
    return -1;

  case 0x01 ... 0x0b:
    if (auto_pl_charset && (src[0] + 4) == 5)
      ic = convert_iso6937;
    else
      ic = convert_iso_8859[src[0] + 4];
    src++; srclen--;
    break;

  case 0x0c ... 0x0f:
    return -1;

  case 0x10: /* Table A.4 */
    if(srclen < 3 || src[1] != 0 || src[2] == 0 || src[2] > 0x0f)
      return -1;

    ic = convert_iso_8859[src[2]];
    src+=3; srclen-=3;
    break;
    
  case 0x11 ... 0x14:
    return -1;

  case 0x15:
    ic = convert_utf8;
    break;
  case 0x16 ... 0x1f:
    return -1;

  default:
    if (auto_pl_charset)
      ic = convert_iso_8859[2];
    else
      ic = convert_iso6937;
    break;
  }

  // manual charset override
  if (dvb_charset != NULL && dvb_charset[0] != 0) {
    if (sscanf(dvb_charset, "ISO8859-%d", &i) > 0 && i > 0 && i < 16) {
      ic = convert_iso_8859[i];
    } else {
      ic = convert_iso6937;
    }
  }

  if(srclen < 1) {
    *dst = 0;
    return 0;
  }

  if(ic == -1)
    return -1;

  outlen = dstlen - 1;

  if (dvb_convert(ic, src, srclen, dst, &outlen) == -1) {
    return -1;
  }

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
atsc_utf16_to_utf8(uint8_t *src, int len, char *buf, int buflen)
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





/*
 * DVB time and date functions
 */

time_t
dvb_convert_date(uint8_t *dvb_buf)
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
  return (timegm(&dvb_time));
}

/**
 *
 */
static struct strtab adaptertype[] = {
  { "DVB-S",  FE_QPSK },
  { "DVB-C",  FE_QAM },
  { "DVB-T",  FE_OFDM },
  { "ATSC",   FE_ATSC },
};


int
dvb_str_to_adaptertype(const char *str)
{
  return str2val(str, adaptertype);
}

const char *
dvb_adaptertype_to_str(int type)
{
  return val2str(type, adaptertype) ?: "invalid";
}

const char *
dvb_polarisation_to_str(int pol)
{
  switch(pol) {
  case POLARISATION_VERTICAL:       return "V";
  case POLARISATION_HORIZONTAL:     return "H";
  case POLARISATION_CIRCULAR_LEFT:  return "L";
  case POLARISATION_CIRCULAR_RIGHT: return "R";
  default:                          return "X";
  }
}

const char *
dvb_polarisation_to_str_long(int pol)
{
  switch(pol) {
  case POLARISATION_VERTICAL:        return "Vertical";
  case POLARISATION_HORIZONTAL:      return "Horizontal";
  case POLARISATION_CIRCULAR_LEFT:   return "Left";
  case POLARISATION_CIRCULAR_RIGHT:  return "Right";
  default:                           return "??";
  }
}


th_dvb_adapter_t *
dvb_adapter_find_by_identifier(const char *identifier)
{
  th_dvb_adapter_t *tda;

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    if(!strcmp(identifier, tda->tda_identifier))
      return tda;
  return NULL;
}


/**
 *
 */
static const char *
nicenum(char *x, size_t siz, unsigned int v)
{
  if(v < 1000)
    snprintf(x, siz, "%d", v);
  else if(v < 1000000)
    snprintf(x, siz, "%d,%03d", v / 1000, v % 1000);
  else if(v < 1000000000)
    snprintf(x, siz, "%d,%03d,%03d", 
	     v / 1000000, (v % 1000000) / 1000, v % 1000);
  else 
    snprintf(x, siz, "%d,%03d,%03d,%03d", 
	     v / 1000000000, (v % 1000000000) / 1000000,
	     (v % 1000000) / 1000, v % 1000);
  return x;
}


/**
 * 
 */
void
dvb_mux_nicefreq(char *buf, size_t size, th_dvb_mux_instance_t *tdmi)
{
  char freq[50];

  if(tdmi->tdmi_adapter->tda_type == FE_QPSK) {
    nicenum(freq, sizeof(freq), tdmi->tdmi_conf.dmc_fe_params.frequency);
    snprintf(buf, size, "%s kHz", freq);
  } else {
    nicenum(freq, sizeof(freq), 
	    tdmi->tdmi_conf.dmc_fe_params.frequency / 1000);
    snprintf(buf, size, "%s kHz", freq);
  }
}


/**
 * 
 */
void
dvb_mux_nicename(char *buf, size_t size, th_dvb_mux_instance_t *tdmi)
{
  char freq[50];
  const char *n = tdmi->tdmi_network;

  if(tdmi->tdmi_adapter->tda_type == FE_QPSK) {
    nicenum(freq, sizeof(freq), tdmi->tdmi_conf.dmc_fe_params.frequency);
    snprintf(buf, size, "%s%s%s kHz %s (%s)", 
	     n?:"", n ? ": ":"", freq,
	     dvb_polarisation_to_str_long(tdmi->tdmi_conf.dmc_polarisation),
	     tdmi->tdmi_conf.dmc_satconf ? tdmi->tdmi_conf.dmc_satconf->sc_name : "No satconf");

  } else {
    nicenum(freq, sizeof(freq), tdmi->tdmi_conf.dmc_fe_params.frequency / 1000);
    snprintf(buf, size, "%s%s%s kHz", n?:"", n ? ": ":"", freq);
  }
}

