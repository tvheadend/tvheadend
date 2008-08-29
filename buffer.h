/*
 *  Packet / Buffer management
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

#ifndef BUFFER_H_
#define BUFFER_H_

th_pkt_t *pkt_ref(th_pkt_t *pkt);

void pkt_deref(th_pkt_t *pkt);

void pkt_init(void);

th_pkt_t *pkt_alloc(void *data, size_t datalen, int64_t pts, int64_t dts);

th_pkt_t *pkt_copy(th_stream_t *st, th_pkt_t *pkt);

void pkt_store(th_stream_t *st, th_pkt_t *pkt);

void pkt_unstore(th_stream_t *st, th_pkt_t *pkt);

int pkt_load(th_stream_t *st, th_pkt_t *pkt);

void *pkt_payload(th_pkt_t *pkt);

size_t pkt_len(th_pkt_t *pkt);

extern int64_t store_mem_size;
extern int64_t store_mem_size_max;
extern int64_t store_disk_size;
extern int64_t store_disk_size_max;
extern int store_packets;


#define pkt_payload(pkt) ((pkt)->pkt_payload)
#define pkt_len(pkt)     ((pkt)->pkt_payloadlen)

#endif /* BUFFER_H_ */
