/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Mariusz Białończyk
 *
 * tvheadend, dvb charset config
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
