/*
 *  Packet nanagement
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef PACKET_H_
#define PACKET_H_


/**
 * Packets
 */
#define PKT_I_FRAME 1
#define PKT_P_FRAME 2
#define PKT_B_FRAME 3
#define PKT_NTYPES  4 

typedef struct th_pkt {
  int64_t pkt_dts;
  int64_t pkt_pts;
  int pkt_duration;
  int pkt_refcount;

  uint8_t pkt_componentindex;
  uint8_t pkt_frametype;
  uint8_t pkt_commercial;

  uint8_t *pkt_payload;
  int pkt_payloadlen;

} th_pkt_t;


/**
 * A packet reference
 */
typedef struct th_pktref {
  TAILQ_ENTRY(th_pktref) pr_link;
  th_pkt_t *pr_pkt;
} th_pktref_t;


/**
 *
 */
void pkt_ref_dec(th_pkt_t *pkt);

void pkt_ref_inc(th_pkt_t *pkt);

void pkt_ref_inc_poly(th_pkt_t *pkt, int n);

void pktref_clear_queue(struct th_pktref_queue *q);

th_pkt_t *pkt_alloc(void *data, size_t datalen, int64_t pts, int64_t dts);

#endif /* PACKET_H_ */
