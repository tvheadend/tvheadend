/*
 *  Tvheadend - DVB support routines and defines
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

/* 
 * Based on:
 *
 * ITU-T Recommendation H.222.0 / ISO standard 13818-1
 * EN 300 468 - V1.7.1
 */

#ifndef __TVH_DVB_SUPPORT_H__
#define __TVH_DVB_SUPPORT_H__

struct mpegts_table;

/* Defaults */


/* Tables */

#define DVB_PAT_BASE                  0x00
#define DVB_PAT_MASK                  0x00

#define DVB_PMT_BASE                  0x02
#define DVB_PMT_MASK                  0xFF

#define DVB_NIT_BASE                  0x00
#define DVB_NIT_MASK                  0x00

#define DVB_SDT_BASE                  0x40
#define DVB_SDT_MASK                  0xF8

#define DVB_BAT_BASE                  0x48
#define DVB_BAT_MASK                  0xF8

#define DVB_TELETEXT_BASE             0x2000

/* Descriptors */

#define DVB_DESC_VIDEO_STREAM         0x02
#define DVB_DESC_REGISTRATION         0x05
#define DVB_DESC_CA                   0x09
#define DVB_DESC_LANGUAGE             0x0A

/* Descriptors defined in EN 300 468 */

#define DVB_DESC_NETWORK_NAME         0x40
#define DVB_DESC_SERVICE_LIST         0x41
#define DVB_DESC_SAT_DEL              0x43
#define DVB_DESC_CABLE_DEL            0x44
#define DVB_DESC_SHORT_EVENT          0x4D
#define DVB_DESC_EXT_EVENT            0x4E
#define DVB_DESC_SERVICE              0x48
#define DVB_DESC_COMPONENT            0x50
#define DVB_DESC_CONTENT              0x54
#define DVB_DESC_PARENTAL_RAT         0x55
#define DVB_DESC_TELETEXT             0x56
#define DVB_DESC_SUBTITLE             0x59
#define DVB_DESC_TERR_DEL             0x5A
#define DVB_DESC_MULTI_NETWORK_NAME   0x5B
#define DVB_DESC_AC3                  0x6A
#define DVB_DESC_DEF_AUTHORITY        0x73
#define DVB_DESC_CRID                 0x76
#define DVB_DESC_EAC3                 0x7A
#define DVB_DESC_AAC                  0x7C
#define DVB_DESC_LOCAL_CHAN           0x83

/* String Extraction */

typedef struct dvb_string_conv
{
  uint8_t type;
  size_t  (*func) ( char *dst, size_t *dstlen,
                    const uint8_t* src, size_t srclen );
} dvb_string_conv_t;

int dvb_get_string
  (char *dst, size_t dstlen, const uint8_t *src, const size_t srclen,
   const char *dvb_charset, dvb_string_conv_t *conv);

int dvb_get_string_with_len
  (char *dst, size_t dstlen, const uint8_t *buf, size_t buflen,
   const char *dvb_charset, dvb_string_conv_t *conv);

/* Conversion */

#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

time_t dvb_convert_date(uint8_t *dvb_buf);

void atsc_utf16_to_utf8(uint8_t *src, int len, char *buf, int buflen);

/*
 * PSI processing
 */

#define FOREACH_DVB_LOOP0(ptr,len,off,min,inc,llen) \
  for ( llen = (ptr[off] & 0xF) << 8 | ptr[off+1],\
        ptr += off + 2,\
        len -= off + 2;\
        (llen > min);\
        ptr += inc, llen -= inc + min )\
    if      (llen > len)       return -1;\
    else

#define FOREACH_DVB_LOOP(ptr,len,off,min,llen)\
  FOREACH_DVB_LOOP0(ptr,len,off,min,0,llen)

#define FOREACH_DVB_DESC(ptr,len,off,llen,dtag,dlen) \
  FOREACH_DVB_LOOP0(ptr,len,off,2,dlen,llen)\
    if      (!(dtag = *ptr++)) return -1;\
    else if ((dlen = *ptr++) > llen - 2) return -1;\
    else

/* PSI descriptors */


/* PSI table callbacks */

int dvb_pat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_pmt_callback  
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tabelid);
int dvb_nit_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_bat_callback  
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_sdt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_tdt_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);

/*
 * Delivery systems and DVB API wrappers
 *
 * Note: although these are really only useful for linuxDVB, they are
 *       used in mpegts so that tsfile can be used to debug issues
 */
#if ENABLE_DVBAPI

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>

typedef struct dvb_frontend_parameters dvb_frontend_parameters_t;

typedef enum polarisation {
	POLARISATION_HORIZONTAL     = 0x00,
	POLARISATION_VERTICAL       = 0x01,
	POLARISATION_CIRCULAR_LEFT  = 0x02,
	POLARISATION_CIRCULAR_RIGHT = 0x03
} polarisation_t;

typedef struct dvb_mux_conf
{
  dvb_frontend_parameters_t dmc_fe_params;
  
  // Additional DVB-S fields
  polarisation_t            dmc_fe_polarisation;
  int                       dmc_fe_orbital_pos;
  char                      dmc_fe_orbital_dir;
#if DVB_API_VERSION >= 5
  fe_modulation_t           dmc_fe_modulation;
  fe_delivery_system_t      dmc_fe_delsys;
  fe_rolloff_t              dmc_fe_rolloff;
#endif
} dvb_mux_conf_t;

/* conversion routines */
const char *dvb_rolloff2str ( int rolloff );
const char *dvb_delsys2str  ( int delsys );
const char *dvb_fec2str     ( int fec );
const char *dvb_qam2str     ( int qam );
const char *dvb_bw2str      ( int bw );
const char *dvb_mode2str    ( int mode );
const char *dvb_guard2str   ( int guard );
const char *dvb_hier2str    ( int hier );
const char *dvb_pol2str     ( int pol );

int dvb_str2rolloff ( const char *str );
int dvb_str2delsys  ( const char *str );
int dvb_str2fec     ( const char *str );
int dvb_str2qam     ( const char *str );
int dvb_str2bw      ( const char *str );
int dvb_str2mode    ( const char *str );
int dvb_str2guard   ( const char *str );
int dvb_str2hier    ( const char *str );
int dvb_str2pol     ( const char *str );

#endif /* ENABLE_DVBAPI */

#endif /* DVB_SUPPORT_H */
