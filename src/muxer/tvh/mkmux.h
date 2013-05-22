/*
 *  Matroska muxer
 *  Copyright (C) 2010 Andreas Öman
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

#ifndef MKMUX_H__
#define MKMUX_H__

typedef struct mk_mux mk_mux_t;

struct epg_broadcast;
struct streaming_start;
struct dvr_entry;
struct th_pkt;
struct channel;
struct event;

mk_mux_t *mk_mux_create(void);

int mk_mux_open_file  (mk_mux_t *mkm, const char *filename);
int mk_mux_open_stream(mk_mux_t *mkm, int fd);

int mk_mux_init(mk_mux_t *mkm, const char *title, 
		const struct streaming_start *ss);

int mk_mux_write_pkt (mk_mux_t *mkm, struct th_pkt *pkt);
int mk_mux_write_meta(mk_mux_t *mkm, const struct dvr_entry *de,
		      const struct epg_broadcast *eb);

int mk_mux_insert_chapter(mk_mux_t *mkm);

int  mk_mux_close  (mk_mux_t *mkm);
void mk_mux_destroy(mk_mux_t *mkm);

#endif // MKMUX_H__
