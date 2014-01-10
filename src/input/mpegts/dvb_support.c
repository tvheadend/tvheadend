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

const static struct strtab rollofftab[] = {
#if DVB_VER_ATLEAST(5,0)
  { "35",           ROLLOFF_35 },
  { "20",           ROLLOFF_20 },
  { "25",           ROLLOFF_25 },
  { "AUTO",         ROLLOFF_AUTO }
#endif
};
dvb_str2val(rolloff);

const static struct strtab delsystab[] = {
#if DVB_VER_ATLEAST(5,0)
  { "UNDEFINED",        SYS_UNDEFINED },
  { "DVBC_ANNEX_AC",    SYS_DVBC_ANNEX_AC },
  { "DVBC_ANNEX_B",     SYS_DVBC_ANNEX_B },
  { "DVBT",             SYS_DVBT },
  { "DVBS",             SYS_DVBS },
  { "DVBS2",            SYS_DVBS2 },
  { "DVBH",             SYS_DVBH },
  { "ISDBT",            SYS_ISDBT },
  { "ISDBS",            SYS_ISDBS },
  { "ISDBC",            SYS_ISDBC },
  { "ATSC",             SYS_ATSC },
  { "ATSCMH",           SYS_ATSCMH },
  { "DMBTH",            SYS_DMBTH },
  { "CMMB",             SYS_CMMB },
  { "DAB",              SYS_DAB },
#endif
#if DVB_VER_ATLEAST(5,1)
  { "DSS",              SYS_DSS },
#endif
#if DVB_VER_ATLEAST(5,4)
  { "DVBT2",            SYS_DVBT2 },
  { "TURBO",            SYS_TURBO }
#endif
};
dvb_str2val(delsys);

#if DVB_VER_ATLEAST(5,10)
int
dvb_delsys2type ( fe_delivery_system_t delsys )
{
  switch (delsys) {
    case SYS_DVBC_ANNEX_AC:
    case SYS_DVBC_ANNEX_B:
    case SYS_ISDBC:
      return FE_QAM;
    case SYS_DVBT:
    case SYS_DVBT2:
    case SYS_TURBO:
    case SYS_ISDBT:
      return FE_OFDM;
    case SYS_DVBS:
    case SYS_DVBS2:
    case SYS_ISDBS:
      return FE_QPSK;
    case SYS_ATSC:
    case SYS_ATSCMH:
      return FE_ATSC;
    default:
      return -1;
  }
}
#endif

const static struct strtab fectab[] = {
  { "NONE",                 FEC_NONE },
  { "1/2",                  FEC_1_2 },
  { "2/3",                  FEC_2_3 },
  { "3/4",                  FEC_3_4 },
  { "4/5",                  FEC_4_5 },
  { "5/6",                  FEC_5_6 },
  { "6/7",                  FEC_6_7 },
  { "7/8",                  FEC_7_8 },
  { "8/9",                  FEC_8_9 },
  { "AUTO",                 FEC_AUTO },
#if DVB_VER_ATLEAST(5,0)
  { "3/5",                  FEC_3_5 },
  { "9/10",                 FEC_9_10 }
#endif
};
dvb_str2val(fec);

const static struct strtab qamtab[] = {
  { "QPSK",                 QPSK },
  { "QAM16",                QAM_16 },
  { "QAM32",                QAM_32 },
  { "QAM64",                QAM_64 },
  { "QAM128",               QAM_128 },
  { "QAM256",               QAM_256 },
  { "AUTO",                 QAM_AUTO },
  { "8VSB",                 VSB_8 },
  { "16VSB",                VSB_16 },
#if DVB_VER_ATLEAST(5,0)
  { "DQPSK",                DQPSK },
#endif
#if DVB_VER_ATLEAST(5,1)
  { "8PSK",                 PSK_8 },
  { "16APSK",               APSK_16 },
  { "32APSK",               APSK_32 },
#endif
};
dvb_str2val(qam);

const static struct strtab bwtab[] = {
  { "8MHz",                 BANDWIDTH_8_MHZ },
  { "7MHz",                 BANDWIDTH_7_MHZ },
  { "6MHz",                 BANDWIDTH_6_MHZ },
  { "AUTO",                 BANDWIDTH_AUTO },
#if DVB_VER_ATLEAST(5,3)
  { "5MHz",                 BANDWIDTH_5_MHZ },
  { "10MHz",                BANDWIDTH_10_MHZ },
  { "1712kHz",              BANDWIDTH_1_712_MHZ},
#endif
};
dvb_str2val(bw);

const static struct strtab modetab[] = {
  { "2k",                   TRANSMISSION_MODE_2K },
  { "8k",                   TRANSMISSION_MODE_8K },
  { "AUTO",                 TRANSMISSION_MODE_AUTO },
#if DVB_VER_ATLEAST(5,1)
  { "4k",                   TRANSMISSION_MODE_4K },
#endif
#if DVB_VER_ATLEAST(5,3)
  { "1k",                   TRANSMISSION_MODE_1K },
  { "2k",                   TRANSMISSION_MODE_16K },
  { "32k",                  TRANSMISSION_MODE_32K },
#endif
};
dvb_str2val(mode);

const static struct strtab guardtab[] = {
  { "1/32",                 GUARD_INTERVAL_1_32 },
  { "1/16",                 GUARD_INTERVAL_1_16 },
  { "1/8",                  GUARD_INTERVAL_1_8 },
  { "1/4",                  GUARD_INTERVAL_1_4 },
  { "AUTO",                 GUARD_INTERVAL_AUTO },
#if DVB_VER_ATLEAST(5,3)
  { "1/128",                GUARD_INTERVAL_1_128 },
  { "19/128",               GUARD_INTERVAL_19_128 },
  { "19/256",               GUARD_INTERVAL_19_256},
#endif
};
dvb_str2val(guard);

const static struct strtab hiertab[] = {
  { "NONE",                 HIERARCHY_NONE },
  { "1",                    HIERARCHY_1 },
  { "2",                    HIERARCHY_2 },
  { "4",                    HIERARCHY_4 },
  { "AUTO",                 HIERARCHY_AUTO }
};
dvb_str2val(hier);

const static struct strtab poltab[] = {
  { "V",                   POLARISATION_VERTICAL },
  { "H",                   POLARISATION_HORIZONTAL },
  { "L",                   POLARISATION_CIRCULAR_LEFT },
  { "R",                   POLARISATION_CIRCULAR_RIGHT },
};
dvb_str2val(pol);

const static struct strtab typetab[] = {
  {"DVB-T", FE_OFDM},
  {"DVB-C", FE_QAM},
  {"DVB-S", FE_QPSK},
  {"ATSC",  FE_ATSC},
};
dvb_str2val(type);

const static struct strtab pilottab[] = {
#if DVB_VER_ATLEAST(5,0)
  {"AUTO", PILOT_AUTO},
  {"ON",   PILOT_ON},
  {"OFF",  PILOT_OFF}
#endif
};
dvb_str2val(pilot);
#undef dvb_str2val

/*
 * Process mux conf
 */
static const char *
dvb_mux_conf_load_dvbt ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  int r;
  const char *s;

  s = htsmsg_get_str(m, "bandwidth");
  if(s == NULL || (r = dvb_str2bw(s)) < 0)
    return "Invalid bandwidth";
  dmc->dmc_fe_params.u.ofdm.bandwidth = r;

  s = htsmsg_get_str(m, "constellation");
  if(s == NULL || (r = dvb_str2qam(s)) < 0)
    return "Invalid QAM constellation";
  dmc->dmc_fe_params.u.ofdm.constellation = r;

  s = htsmsg_get_str(m, "transmission_mode");
  if(s == NULL || (r = dvb_str2mode(s)) < 0)
    return "Invalid transmission mode";
  dmc->dmc_fe_params.u.ofdm.transmission_mode = r;

  s = htsmsg_get_str(m, "guard_interval");
  if(s == NULL || (r = dvb_str2guard(s)) < 0)
    return "Invalid guard interval";
  dmc->dmc_fe_params.u.ofdm.guard_interval = r;

  s = htsmsg_get_str(m, "hierarchy");
  if(s == NULL || (r = dvb_str2hier(s)) < 0)
    return "Invalid heirarchy information";
  dmc->dmc_fe_params.u.ofdm.hierarchy_information = r;

  s = htsmsg_get_str(m, "fec_hi");
  if(s == NULL || (r = dvb_str2fec(s)) < 0)
    return "Invalid hi-FEC";
  dmc->dmc_fe_params.u.ofdm.code_rate_HP = r;

  s = htsmsg_get_str(m, "fec_lo");
  if(s == NULL || (r = dvb_str2fec(s)) < 0)
    return "Invalid lo-FEC";
  dmc->dmc_fe_params.u.ofdm.code_rate_LP = r;

  return NULL;
}

static const char *
dvb_mux_conf_load_dvbc ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  int r;
  const char *s;

  htsmsg_get_u32(m, "symbol_rate", &dmc->dmc_fe_params.u.qam.symbol_rate);
  if(dmc->dmc_fe_params.u.qam.symbol_rate == 0)
    return "Invalid symbol rate";
    
  s = htsmsg_get_str(m, "constellation");
  if(s == NULL || (r = dvb_str2qam(s)) < 0)
    return "Invalid QAM constellation";
  dmc->dmc_fe_params.u.qam.modulation = r;

  s = htsmsg_get_str(m, "fec");
  if(s == NULL || (r = dvb_str2fec(s)) < 0)
    return "Invalid FEC";
  dmc->dmc_fe_params.u.qam.fec_inner = r;

  return NULL;
}

static const char *
dvb_mux_conf_load_dvbs ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  int r;
  const char *s;

  htsmsg_get_u32(m, "symbol_rate", &dmc->dmc_fe_params.u.qpsk.symbol_rate);
  if(dmc->dmc_fe_params.u.qpsk.symbol_rate == 0)
    return "Invalid symbol rate";
    
  s = htsmsg_get_str(m, "fec");
  if(s == NULL || (r = dvb_str2fec(s)) < 0)
    return "Invalid FEC";
  dmc->dmc_fe_params.u.qpsk.fec_inner = r;

  s = htsmsg_get_str(m, "polarisation");
  if(s == NULL || (r = dvb_str2pol(s)) < 0)
    return "Invalid polarisation";
  dmc->dmc_fe_polarisation = r;

#if DVB_VER_ATLEAST(5,0)
  s = htsmsg_get_str(m, "modulation");
  if(s == NULL || (r = dvb_str2qam(s)) < 0) {
    r = QPSK;
    tvhlog(LOG_INFO, "dvb", "no modulation, using default QPSK");
  } 
  dmc->dmc_fe_modulation = r;

  s = htsmsg_get_str(m, "rolloff");
  if(s == NULL || (r = dvb_str2rolloff(s)) < 0) {
    r = ROLLOFF_35;
    tvhlog(LOG_INFO, "dvb", "no rolloff, using default ROLLOFF_35");
  }
  dmc->dmc_fe_rolloff = r;

  // TODO: pilot mode
#endif
  return NULL;
}

static const char *
dvb_mux_conf_load_atsc ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  int r;
  const char *s;
  s = htsmsg_get_str(m, "constellation");
  if(s == NULL || (r = dvb_str2qam(s)) < 0)
    return "Invalid VSB constellation";
  dmc->dmc_fe_params.u.vsb.modulation = r;
  return NULL;
}

const char *
dvb_mux_conf_load ( fe_type_t type, dvb_mux_conf_t *dmc, htsmsg_t *m )
{
#if DVB_VER_ATLEAST(5,0)
  int r;
  const char *str;
#endif
  uint32_t u32;

  memset(dmc, 0, sizeof(dvb_mux_conf_t));
  dmc->dmc_fe_params.inversion = INVERSION_AUTO;

  /* Delivery system */
#if DVB_VER_ATLEAST(5,0)
  str = htsmsg_get_str(m, "delsys");
  if (!str || (r = dvb_str2delsys(str)) < 0) {
         if (type == FE_OFDM) r = SYS_DVBT;
    else if (type == FE_QAM)  r = SYS_DVBC_ANNEX_AC;
    else if (type == FE_QPSK) r = SYS_DVBS;
    else if (type == FE_ATSC) r = SYS_ATSC;
    else
      return "Invalid FE type";
    tvhlog(LOG_INFO, "dvb", "no delsys, using default %s", dvb_delsys2str(r));
  }
  dmc->dmc_fe_delsys           = r;
#endif

  /* Frequency */
  if (htsmsg_get_u32(m, "frequency", &u32))
    return "Invalid frequency";
  dmc->dmc_fe_params.frequency = u32;

  /* Type specific */
  if      (type == FE_OFDM) return dvb_mux_conf_load_dvbt(dmc, m);
  else if (type == FE_QAM)  return dvb_mux_conf_load_dvbc(dmc, m);
  else if (type == FE_QPSK) return dvb_mux_conf_load_dvbs(dmc, m);
  else if (type == FE_ATSC) return dvb_mux_conf_load_atsc(dmc, m);
  else
    return "Invalid FE type";
}

static void
dvb_mux_conf_save_dvbt ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  struct dvb_ofdm_parameters *ofdm = &dmc->dmc_fe_params.u.ofdm;
  htsmsg_add_str(m, "bandwidth",
                 dvb_bw2str(ofdm->bandwidth));
  htsmsg_add_str(m, "constellation",
                 dvb_qam2str(ofdm->constellation));
  htsmsg_add_str(m, "transmission_mode",
                 dvb_mode2str(ofdm->transmission_mode));
  htsmsg_add_str(m, "guard_interval",
                 dvb_guard2str(ofdm->guard_interval));
  htsmsg_add_str(m, "hierarchy",
                 dvb_hier2str(ofdm->hierarchy_information));
  htsmsg_add_str(m, "fec_hi",
                 dvb_fec2str(ofdm->code_rate_HP));
  htsmsg_add_str(m, "fec_lo",
                 dvb_fec2str(ofdm->code_rate_LP));
}

static void
dvb_mux_conf_save_dvbc ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  struct dvb_qam_parameters *qam  = &dmc->dmc_fe_params.u.qam;
  htsmsg_add_u32(m, "symbol_rate",   qam->symbol_rate);
  htsmsg_add_str(m, "constellation", dvb_qam2str(qam->modulation));
  htsmsg_add_str(m, "fec",           dvb_fec2str(qam->fec_inner));
}

static void
dvb_mux_conf_save_dvbs ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  struct dvb_qpsk_parameters *qpsk  = &dmc->dmc_fe_params.u.qpsk;
  htsmsg_add_u32(m, "symbol_rate",   qpsk->symbol_rate);
  htsmsg_add_str(m, "fec",           dvb_fec2str(qpsk->fec_inner));
  htsmsg_add_str(m, "polarisation",  dvb_pol2str(dmc->dmc_fe_polarisation));
#if DVB_VER_ATLEAST(5,0)
  htsmsg_add_str(m, "modulation",    dvb_qam2str(dmc->dmc_fe_modulation));
  htsmsg_add_str(m, "rolloff",       dvb_rolloff2str(dmc->dmc_fe_rolloff));
#endif
}

static void
dvb_mux_conf_save_atsc ( dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  htsmsg_add_str(m, "constellation",
                 dvb_qam2str(dmc->dmc_fe_params.u.vsb.modulation));
}

void
dvb_mux_conf_save ( fe_type_t type, dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  htsmsg_add_u32(m, "frequency", dmc->dmc_fe_params.frequency);
#if DVB_VER_ATLEAST(5,0)
  htsmsg_add_str(m, "delsys", dvb_delsys2str(dmc->dmc_fe_delsys));
#endif
       if (type == FE_OFDM) dvb_mux_conf_save_dvbt(dmc, m);
  else if (type == FE_QAM)  dvb_mux_conf_save_dvbc(dmc, m);
  else if (type == FE_QPSK) dvb_mux_conf_save_dvbs(dmc, m);
  else if (type == FE_ATSC) dvb_mux_conf_save_atsc(dmc, m);
}

int
dvb_bandwidth ( fe_bandwidth_t bw )
{
  switch (bw) {
#if DVB_VER_ATLEAST(5,3)
    case BANDWIDTH_10_MHZ:
      return 10000000;
    case BANDWIDTH_5_MHZ:
      return 5000000;
    case BANDWIDTH_1_712_MHZ:
      return 1712000;
#endif
    case BANDWIDTH_8_MHZ:
      return 8000000;
    case BANDWIDTH_7_MHZ:
      return 7000000;
    case BANDWIDTH_6_MHZ:
      return 6000000;
    default:
      return 0;
  }
}

#endif /* ENABLE_DVBAPI */
