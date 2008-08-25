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
#include <iconv.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "dvb_support.h"
#include "dvb.h"

/*
 * DVB String conversion according to EN 300 468, Annex A
 * Not all character sets are supported, but it should cover most of them
 */

int
dvb_get_string(char *dst, size_t dstlen, const uint8_t *src, 
	       size_t srclen, const char *target_encoding)
{
  iconv_t ic;
  const char *encoding;
  char encbuf[20];
  int len;
  char *in, *out;
  size_t inlen, outlen;
  int utf8 = 0;
  int i;
  unsigned char *tmp;
  int r;

  if(srclen < 1) {
    *dst = 0;
    return 0;
  }

  switch(src[0]) {
  case 0:
    return -1;

  case 0x01 ... 0x0b:
    snprintf(encbuf, sizeof(encbuf), "ISO_8859-%d", src[0] + 4);
    encoding = encbuf;
    src++; srclen--;
    break;

  case 0x0c ... 0x0f:
    return -1;

  case 0x10: /* Table A.4 */
    if(srclen < 3 || src[1] != 0 || src[2] == 0 || src[2] > 0x0f)
      return -1;

    snprintf(encbuf, sizeof(encbuf), "ISO_8859-%d", src[2]);
    encoding = encbuf;
    src+=3; srclen-=3;
    break;
    
  case 0x11 ... 0x14:
    return -1;

  case 0x15:
    encoding = "UTF8";
    utf8 = 1;
    break;
  case 0x16 ... 0x1f:
    return -1;

  default:
    encoding = "LATIN1"; /* Default to latin-1 */
    break;
  }

  if(srclen < 1) {
    *dst = 0;
    return 0;
  }

  tmp = alloca(srclen + 1);
  memcpy(tmp, src, srclen);
  tmp[srclen] = 0;


  /* Escape control codes */

  if(!utf8) {
    for(i = 0; i < srclen; i++) {
      if(tmp[i] >= 0x80 && tmp[i] <= 0x9f)
	tmp[i] = ' ';
    }
  }

  ic = iconv_open(target_encoding, encoding);
  if(ic == (iconv_t) -1) {
    return -1;
  }

  inlen = srclen;
  outlen = dstlen - 1;

  out = dst;
  in = (char *)tmp;

  while(inlen > 0) {
    r = iconv(ic, &in, &inlen, &out, &outlen);

    if(r == (size_t) -1) {
      if(errno == EILSEQ) {
	in++;
	inlen--;
	continue;
      } else {
	return -1;
      }
    }
  }

  iconv_close(ic);
  len = dstlen - outlen - 1;
  dst[len] = 0;
  return 0;
}


int
dvb_get_string_with_len(char *dst, size_t dstlen, 
			const uint8_t *buf, size_t buflen, 
			const char *target_encoding)
{
  int l = buf[0];

  if(l + 1 > buflen)
    return -1;

  if(dvb_get_string(dst, dstlen, buf + 1, l, target_encoding))
    return -1;

  return l + 1;
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
  case POLARISATION_VERTICAL:   return "V";
  case POLARISATION_HORIZONTAL: return "H";
  default:                      return "X";
  }
}

static const char *
dvb_polarisation_to_str_long(int pol)
{
  switch(pol) {
  case POLARISATION_VERTICAL:   return "Vertical";
  case POLARISATION_HORIZONTAL: return "Horizontal";
  default:                      return "??";
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
 *  Return a readable status text for the given mux
 */
const char *
dvb_mux_status(th_dvb_mux_instance_t *tdmi)
{
  int i, v = 0;

  for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++)
    v += tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT;
  
  if(v == TDMI_FEC_ERR_HISTOGRAM_SIZE)
    return "Constant FEC";
  else if(v > 0)
    return "Bursty FEC";

  return tdmi->tdmi_status ?: "OK";
}


/**
 * Return a badness value (0-3)
 */
int
dvb_mux_badness(th_dvb_mux_instance_t *tdmi)
{
  int i, v = 0;

  if(tdmi->tdmi_status)
    return 3;

  for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++)
    v += tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT;

  if(v == TDMI_FEC_ERR_HISTOGRAM_SIZE)
    return 2;
  else if(v > 0)
    return 1;

  return 0;
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
dvb_mux_nicename(char *buf, size_t size, th_dvb_mux_instance_t *tdmi)
{
  char freq[50];


  if(tdmi->tdmi_adapter->tda_type == FE_QPSK) {
    nicenum(freq, sizeof(freq), tdmi->tdmi_fe_params.frequency);
    snprintf(buf, size, "%s kHz %s port %d", freq,
	     dvb_polarisation_to_str_long(tdmi->tdmi_polarisation),
	     tdmi->tdmi_switchport);
  } else {
    nicenum(freq, sizeof(freq), tdmi->tdmi_fe_params.frequency / 1000);
    snprintf(buf, size, "%s kHz", freq);
  }
}

