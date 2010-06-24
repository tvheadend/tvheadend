/**
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


#include "tvhead.h"
#include "packet.h"
#include "string.h"
#include "atomic.h"

/*
 *
 */
static void
pkt_destroy(th_pkt_t *pkt)
{
  if(pkt->pkt_payload != NULL)
    pktbuf_ref_dec(pkt->pkt_payload);

  if(pkt->pkt_header != NULL)
    pktbuf_ref_dec(pkt->pkt_header);
  free(pkt);
}


/**
 * Allocate a new packet and give it a refcount of one (which caller is
 * suppoed to take care of)
 */
th_pkt_t *
pkt_alloc(const void *data, size_t datalen, int64_t pts, int64_t dts)
{
  th_pkt_t *pkt;

  pkt = calloc(1, sizeof(th_pkt_t));
  if(datalen)
    pkt->pkt_payload = pktbuf_alloc(data, datalen);
  pkt->pkt_dts = dts;
  pkt->pkt_pts = pts;
  pkt->pkt_refcount = 1;
  return pkt;
}

/**
 *
 */
void
pkt_ref_dec(th_pkt_t *pkt)
{
  if((atomic_add(&pkt->pkt_refcount, -1)) == 1)
    pkt_destroy(pkt);
}


/**
 *
 */
void
pkt_ref_inc(th_pkt_t *pkt)
{
  atomic_add(&pkt->pkt_refcount, 1);
}

/**
 *
 */
void
pkt_ref_inc_poly(th_pkt_t *pkt, int n)
{
  atomic_add(&pkt->pkt_refcount, n);
}


/**
 *
 */
void
pktref_clear_queue(struct th_pktref_queue *q)
{
  th_pktref_t *pr;

  while((pr = TAILQ_FIRST(q)) != NULL) {
    TAILQ_REMOVE(q, pr, pr_link);
    pkt_ref_dec(pr->pr_pkt);
    free(pr);
  }
}


/**
 *
 */
th_pkt_t *
pkt_merge_header(th_pkt_t *pkt)
{
  th_pkt_t *n;
  size_t s;

  if(pkt->pkt_header == NULL)
    return pkt;

  n = malloc(sizeof(th_pkt_t));
  *n = *pkt;

  n->pkt_refcount = 1;
  n->pkt_header = NULL;

  s = pktbuf_len(pkt->pkt_payload) + pktbuf_len(pkt->pkt_header);
  n->pkt_payload = pktbuf_alloc(NULL, s);

  memcpy(pktbuf_ptr(n->pkt_payload),
	 pktbuf_ptr(pkt->pkt_header),
	 pktbuf_len(pkt->pkt_header));

  memcpy(pktbuf_ptr(n->pkt_payload) + pktbuf_len(pkt->pkt_header),
	 pktbuf_ptr(pkt->pkt_payload),
	 pktbuf_len(pkt->pkt_payload));

  pkt_ref_dec(pkt);
  return n;
}



/**
 *
 */
th_pkt_t *
pkt_copy_shallow(th_pkt_t *pkt)
{
  th_pkt_t *n = malloc(sizeof(th_pkt_t));
  *n = *pkt;

  n->pkt_refcount = 1;

  if(n->pkt_header)
    pktbuf_ref_inc(n->pkt_header);

  if(n->pkt_payload)
    pktbuf_ref_inc(n->pkt_payload);

  return n;
}


/**
 *
 */
th_pktref_t *
pktref_create(th_pkt_t *pkt)
{
  th_pktref_t *pr = malloc(sizeof(th_pktref_t));
  pr->pr_pkt = pkt;
  return pr;
}



void 
pktbuf_ref_dec(pktbuf_t *pb)
{
  if((atomic_add(&pb->pb_refcount, -1)) == 1) {
    free(pb->pb_data);
    free(pb);
  }
}

void
pktbuf_ref_inc(pktbuf_t *pb)
{
  atomic_add(&pb->pb_refcount, 1);
}

pktbuf_t *
pktbuf_alloc(const void *data, size_t size)
{
  pktbuf_t *pb = malloc(sizeof(pktbuf_t));
  pb->pb_refcount = 1;
  pb->pb_size = size;

  if(size > 0) {
    pb->pb_data = malloc(size);
    if(data != NULL)
      memcpy(pb->pb_data, data, size);
  }
  return pb;
}

pktbuf_t *
pktbuf_make(void *data, size_t size)
{
  pktbuf_t *pb = malloc(sizeof(pktbuf_t));
  pb->pb_refcount = 1;
  pb->pb_size = size;
  pb->pb_data = data;
  return pb;
}
