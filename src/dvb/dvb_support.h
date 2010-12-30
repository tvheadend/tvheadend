/*
 *  TV Input - Linux DVB interface - Support
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

#ifndef DVB_SUPPORT_H
#define DVB_SUPPORT_H

#include "dvb.h"

#define DVB_DESC_VIDEO_STREAM 0x02
#define DVB_DESC_REGISTRATION 0x05
#define DVB_DESC_CA           0x09
#define DVB_DESC_LANGUAGE     0x0a

/* Descriptors defined in EN 300 468 */

#define DVB_DESC_NETWORK_NAME 0x40
#define DVB_DESC_SERVICE_LIST 0x41
#define DVB_DESC_SAT          0x43
#define DVB_DESC_CABLE        0x44
#define DVB_DESC_SHORT_EVENT  0x4d
#define DVB_DESC_EXT_EVENT    0x4e
#define DVB_DESC_SERVICE      0x48
#define DVB_DESC_CONTENT      0x54
#define DVB_DESC_TELETEXT     0x56
#define DVB_DESC_SUBTITLE     0x59
#define DVB_DESC_AC3          0x6a
#define DVB_DESC_EAC3         0x7a
#define DVB_DESC_AAC          0x7c
#define DVB_DESC_LOCAL_CHAN   0x83

int dvb_get_string(char *dst, size_t dstlen, const uint8_t *src, 
		   const size_t srclen);

int dvb_get_string_with_len(char *dst, size_t dstlen, 
			    const uint8_t *buf, size_t buflen);

#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

time_t dvb_convert_date(uint8_t *dvb_buf);

const char *dvb_adaptertype_to_str(int type);
int dvb_str_to_adaptertype(const char *str);
const char *dvb_polarisation_to_str(int pol);
const char *dvb_polarisation_to_str_long(int pol);
th_dvb_mux_instance_t *dvb_mux_find_by_identifier(const char *identifier);
void dvb_mux_nicename(char *buf, size_t size, th_dvb_mux_instance_t *tdmi);
int dvb_mux_badness(th_dvb_mux_instance_t *tdmi);
const char *dvb_mux_status(th_dvb_mux_instance_t *tdmi);
void dvb_conversion_init(void);
void dvb_mux_nicefreq(char *buf, size_t size, th_dvb_mux_instance_t *tdmi);

void atsc_utf16_to_utf8(uint8_t *src, int len, char *buf, int buflen);

#endif /* DVB_SUPPORT_H */
