/*
 *  Packet management
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

struct memoryinfo;

/**
 * Packet buffer
 */

typedef struct pktbuf {
  int pb_refcount;
  int pb_err;
  uint8_t *pb_data;
  size_t pb_size;
} pktbuf_t;

/**
 * Packets
 */
#define PKT_I_FRAME 1
#define PKT_P_FRAME 2
#define PKT_B_FRAME 3
#define PKT_NTYPES  4 

static inline char pkt_frametype_to_char ( int frametype )
{
  if (frametype == PKT_I_FRAME) return 'I';
  if (frametype == PKT_P_FRAME) return 'P';
  if (frametype == PKT_B_FRAME) return 'B';
  return ' ';
}

typedef struct th_pkt {
  int64_t pkt_dts;
  int64_t pkt_pts;
  int pkt_duration;
  int pkt_refcount;

  uint8_t pkt_commercial;
  uint8_t pkt_componentindex;
  uint8_t pkt_frametype;
  uint8_t pkt_field;  // Set if packet is only a half frame (a field)

  uint8_t pkt_channels;
  uint8_t pkt_sri;
  uint8_t pkt_ext_sri;
  uint8_t pkt_err;

  uint16_t pkt_aspect_num;
  uint16_t pkt_aspect_den;

  pktbuf_t *pkt_meta;
  pktbuf_t *pkt_payload;

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
extern struct memoryinfo pkt_memoryinfo;
extern struct memoryinfo pktbuf_memoryinfo;
extern struct memoryinfo pktref_memoryinfo;

/**
 *
 */
void pkt_ref_dec(th_pkt_t *pkt);

void pkt_ref_inc(th_pkt_t *pkt);

void pkt_ref_inc_poly(th_pkt_t *pkt, int n);

void pktref_clear_queue(struct th_pktref_queue *q);

// Reference count is transfered to queue
void pktref_enqueue(struct th_pktref_queue *q, th_pkt_t *pkt);

void pktref_remove(struct th_pktref_queue *q, th_pktref_t *pr);


th_pkt_t *pkt_alloc(const void *data, size_t datalen, int64_t pts, int64_t dts);

th_pkt_t *pkt_copy_shallow(th_pkt_t *pkt);

th_pkt_t *pkt_copy_nodata(th_pkt_t *pkt);

th_pktref_t *pktref_create(th_pkt_t *pkt);

/*
 *
 */

void pktbuf_ref_dec(pktbuf_t *pb);

void pktbuf_destroy(pktbuf_t *pb);

pktbuf_t *pktbuf_ref_inc(pktbuf_t *pb);

pktbuf_t *pktbuf_alloc(const void *data, size_t size);

pktbuf_t *pktbuf_make(void *data, size_t size);

pktbuf_t *pktbuf_append(pktbuf_t *pb, const void *data, size_t size);

static inline size_t   pktbuf_len(pktbuf_t *pb) { return pb ? pb->pb_size : 0; }
static inline uint8_t *pktbuf_ptr(pktbuf_t *pb) { return pb->pb_data; }

static inline int64_t pts_diff(int64_t a, int64_t b)
{
  a &= PTS_MASK;
  b &= PTS_MASK;
  if (b < (PTS_MASK / 4) && a > (PTS_MASK / 2))
    return b + PTS_MASK + 1 - a;
  else if (b >= a)
    return b - a;
  else
    return PTS_UNSET;
}

#endif /* PACKET_H_ */
