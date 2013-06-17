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

/* PIDs */

#define DVB_PAT_PID                   0x00
#define DVB_CAT_PID                   0x02
#define DVB_NIT_PID                   0x10
#define DVB_SDT_PID                   0x11
#define DVB_BAT_PID                   0x11

/* Tables */

#define DVB_PAT_BASE                  0x00
#define DVB_PAT_MASK                  0x00

#define DVB_CAT_BASE                  0x01
#define DVB_CAT_MASK                  0xFF

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
#define DVB_DESC_BOUQUET_NAME         0x47
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

#define DVB_LOOP_INIT(ptr, len, off, lptr, llen)\
do {\
  llen = ((ptr[off] & 0xF) << 8) | ptr[off+1];\
  lptr = 2 + off + ptr;\
  ptr += 2 + off + llen;\
  len -= 2 + off + llen;\
  if (len < 0) {tvhtrace("psi", "len < 0"); return -1; }\
} while(0)

#define DVB_LOOP_EACH(ptr, len, min)\
  for ( ; len > min ; )\

#define DVB_LOOP_FOREACH(ptr, len, off, lptr, llen, min)\
  DVB_LOOP_INIT(ptr, len, off, lptr, llen);\
  DVB_LOOP_EACH(lptr, llen, min)

#define DVB_DESC_EACH(ptr, len, dtag, dlen, dptr)\
  DVB_LOOP_EACH(ptr, len, 2)\
    if      (!(dtag  = ptr[0]))      {tvhtrace("psi", "1");return -1;}\
    else if ((dlen  = ptr[1]) < 0)   {tvhtrace("psi", "2");return -1;}\
    else if (!(dptr  = ptr+2))       {tvhtrace("psi", "3");return -1;}\
    else if ( (len -= 2 + dlen) < 0) {tvhtrace("psi", "4");return -1;}\
    else if (!(ptr += 2 + dlen))     {tvhtrace("psi", "5");return -1;}\
    else

#define DVB_DESC_FOREACH(ptr, len, off, lptr, llen, dtag, dlen, dptr)\
  DVB_LOOP_INIT(ptr, len, off, lptr, llen);\
  DVB_DESC_EACH(lptr, llen, dtag, dlen, dptr)\

/* PSI descriptors */


/* PSI table callbacks */

int dvb_pat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);
int dvb_cat_callback
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

#define DVB_VER_INT(maj,min) (((maj) << 16) + (min))

#define DVB_VER_ATLEAST(maj, min) \
 (DVB_VER_INT(DVB_API_VERSION,  DVB_API_VERSION_MINOR) >= DVB_VER_INT(maj, min))

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

const char *dvb_mux_conf_load
  ( fe_type_t type, dvb_mux_conf_t *dmc, htsmsg_t *m );
void dvb_mux_conf_save
  ( fe_type_t type, dvb_mux_conf_t *dmc, htsmsg_t *m );

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
const char *dvb_type2str    ( int type );
#define dvb_feclo2str dvb_fec2str
#define dvb_fechi2str dvb_fec2str

int dvb_str2rolloff ( const char *str );
int dvb_str2delsys  ( const char *str );
int dvb_str2fec     ( const char *str );
int dvb_str2qam     ( const char *str );
int dvb_str2bw      ( const char *str );
int dvb_str2mode    ( const char *str );
int dvb_str2guard   ( const char *str );
int dvb_str2hier    ( const char *str );
int dvb_str2pol     ( const char *str );
int dvb_str2type    ( const char *str );
#define dvb_str2feclo dvb_str2fec
#define dvb_str2fechi dvb_str2fec

int dvb_bandwidth   ( enum fe_bandwidth bw );

#endif /* ENABLE_DVBAPI */

#endif /* DVB_SUPPORT_H */
