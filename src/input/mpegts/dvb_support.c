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

#include <linux/dvb/frontend.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_charset_tables.h"
#include "../mpegts.h"

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
    if (!strcmp(dvb_charset, "AUTO")) {
      // ignore
    } else if (sscanf(dvb_charset, "ISO-8859-%d", &i) > 0 && i > 0 && i < 16) {
      ic = convert_iso_8859[i];
    } else if (!strcmp(dvb_charset, "ISO-6937")) {
      ic = convert_iso6937;
    } else if (!strcmp(dvb_charset, "UTF-8")) {
      ic = convert_utf8;
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

/*
 * DVB time and date functions
 */

time_t
dvb_convert_date(const uint8_t *dvb_buf)
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

/*
 * DVB API helpers
 */
#if ENABLE_DVBAPI


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
  static char __thread buf[16];
  const char *res = dvb_common2str(p);
  if (res)
    return res;
  sprintf(buf, "%02i", p / 10);
  return buf;
}

int dvb_str2rolloff(const char *p)
{
  static int rolloff_table[] = {
    DVB_ROLLOFF_20,
    DVB_ROLLOFF_25,
    DVB_ROLLOFF_35,
    DVB_EOD,
  };
  int res = dvb_str2common(p);
  if (res != DVB_EOD)
    return res;
  res = atoi(p) * 10;
  return dvb_verify(atoi(p) * 10, rolloff_table);
}

const static struct strtab delsystab[] = {
  { "NONE",         DVB_SYS_NONE },
  { "DVBC_ANNEX_A", DVB_SYS_DVBC_ANNEX_A },
  { "DVBC_ANNEX_B", DVB_SYS_DVBC_ANNEX_B },
  { "DVBC_ANNEX_C", DVB_SYS_DVBC_ANNEX_C },
  { "DVBC_ANNEX_AC",DVB_SYS_DVBC_ANNEX_A }, /* for compatibility */
  { "DVBT",         DVB_SYS_DVBT },
  { "DVBT2",        DVB_SYS_DVBT2 },
  { "DVBS",         DVB_SYS_DVBS },
  { "DVBS2",        DVB_SYS_DVBS2 },
  { "DVBH",         DVB_SYS_DVBH },
  { "ISDBT",        DVB_SYS_ISDBT },
  { "ISDBS",        DVB_SYS_ISDBS },
  { "ISDBC",        DVB_SYS_ISDBC },
  { "ATSC",         DVB_SYS_ATSC },
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
dvb_delsys2type ( dvb_fe_delivery_system_t delsys )
{
  switch (delsys) {
    case DVB_SYS_DVBC_ANNEX_A:
    case DVB_SYS_DVBC_ANNEX_B:
    case DVB_SYS_DVBC_ANNEX_C:
    case DVB_SYS_ISDBC:
      return DVB_TYPE_C;
    case DVB_SYS_DVBT:
    case DVB_SYS_DVBT2:
    case DVB_SYS_TURBO:
    case DVB_SYS_ISDBT:
      return DVB_TYPE_T;
    case DVB_SYS_DVBS:
    case DVB_SYS_DVBS2:
    case DVB_SYS_ISDBS:
      return DVB_TYPE_S;
    case DVB_SYS_ATSC:
    case DVB_SYS_ATSCMH:
      return DVB_TYPE_ATSC;
    default:
      return DVB_TYPE_NONE;
  }
}

const char *dvb_fec2str(int p)
{
  static char __thread buf[16];
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

const static struct strtab qamtab[] = {
  { "NONE",      DVB_MOD_NONE },
  { "AUTO",      DVB_MOD_AUTO },
  { "QPSK",      DVB_MOD_QPSK },
  { "QAM4NR",    DVB_MOD_QAM_4_NR },
  { "QAM-AUTO",  DVB_MOD_QAM_AUTO },
  { "QAM16",     DVB_MOD_QAM_16 },
  { "QAM32",     DVB_MOD_QAM_32 },
  { "QAM64",     DVB_MOD_QAM_64 },
  { "QAM128",    DVB_MOD_QAM_128 },
  { "QAM256",    DVB_MOD_QAM_256 },
  { "8VSB",      DVB_MOD_VSB_8 },
  { "16VSB",     DVB_MOD_VSB_16 },
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
  static char __thread buf[16];
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

const static struct strtab modetab[] = {
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

const static struct strtab guardtab[] = {
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

const static struct strtab hiertab[] = {
  { "NONE", DVB_HIERARCHY_NONE },
  { "AUTO", DVB_HIERARCHY_AUTO },
  { "1",    DVB_HIERARCHY_1 },
  { "2",    DVB_HIERARCHY_2 },
  { "4",    DVB_HIERARCHY_4 },
};
dvb_str2val(hier);

const static struct strtab poltab[] = {
  { "V", DVB_POLARISATION_VERTICAL },
  { "H", DVB_POLARISATION_HORIZONTAL },
  { "L", DVB_POLARISATION_CIRCULAR_LEFT },
  { "R", DVB_POLARISATION_CIRCULAR_RIGHT },
};
dvb_str2val(pol);

const static struct strtab typetab[] = {
  {"DVB-T", DVB_TYPE_T},
  {"DVB-C", DVB_TYPE_C},
  {"DVB-S", DVB_TYPE_S},
  {"ATSC",  DVB_TYPE_ATSC},
};
dvb_str2val(type);

const static struct strtab pilottab[] = {
  {"NONE", DVB_PILOT_AUTO},
  {"AUTO", DVB_PILOT_AUTO},
  {"ON",   DVB_PILOT_ON},
  {"OFF",  DVB_PILOT_OFF}
};
dvb_str2val(pilot);
#undef dvb_str2val

#endif /* ENABLE_DVBAPI */

/**
 *
 */
void dvb_done( void )
{
  extern SKEL_DECLARE(mpegts_table_state_skel, struct mpegts_table_state);
  extern SKEL_DECLARE(mpegts_pid_sub_skel, mpegts_pid_sub_t);
  extern SKEL_DECLARE(mpegts_pid_skel, mpegts_pid_t);

  SKEL_FREE(mpegts_table_state_skel);
  SKEL_FREE(mpegts_pid_sub_skel);
  SKEL_FREE(mpegts_pid_skel);
}
