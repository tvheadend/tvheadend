/*
 *  tvheadend, dvb charset config
 *  Copyright (C) 2012 Mariusz Białończyk
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

#ifndef __TVH_DVB_CHARSET_H__
#define __TVH_DVB_CHARSET_H__

typedef struct dvb_charset {
  LIST_ENTRY(dvb_charset) link;
 uint16_t onid;
 uint16_t tsid;
 uint16_t sid;
 const char *charset;
} dvb_charset_t;

void dvb_charset_init ( void );
void dvb_charset_done ( void );

struct mpegts_network;
struct mpegts_mux;
struct mpegts_service;

const char *dvb_charset_find
  (struct mpegts_network *mn, struct mpegts_mux *mm, struct mpegts_service *s);

htsmsg_t *dvb_charset_enum ( void*, const char *lang );

#endif /* __TVH_DVB_CHARSET_H__ */
