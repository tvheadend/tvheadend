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


#include "tvheadend.h"
#include "packet.h"
#include "string.h"
#include "atomic.h"

#ifndef PKTBUF_DATA_ALIGN
#define PKTBUF_DATA_ALIGN 64
#endif

/*
 *
 */
static void
pkt_destroy(th_pkt_t *pkt)
{
  pktbuf_ref_dec(pkt->pkt_payload);
  pktbuf_ref_dec(pkt->pkt_meta);

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
 * Reference count is transfered to queue
 */
void
pktref_enqueue(struct th_pktref_queue *q, th_pkt_t *pkt)
{
  th_pktref_t *pr = malloc(sizeof(th_pktref_t));
  pr->pr_pkt = pkt;
  TAILQ_INSERT_TAIL(q, pr, pr_link);
}


/**
 *
 */
void
pktref_remove(struct th_pktref_queue *q, th_pktref_t *pr)
{
  TAILQ_REMOVE(q, pr, pr_link);
  pkt_ref_dec(pr->pr_pkt);
  free(pr);
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

  pktbuf_ref_inc(n->pkt_meta);
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

/*
 *
 */

void
pktbuf_ref_dec(pktbuf_t *pb)
{
  if (pb) {
    if((atomic_add(&pb->pb_refcount, -1)) == 1) {
      free(pb->pb_data);
      free(pb);
    }
  }
}

pktbuf_t *
pktbuf_ref_inc(pktbuf_t *pb)
{
  if (pb) {
    atomic_add(&pb->pb_refcount, 1);
    return pb;
  }
  return NULL;
}

pktbuf_t *
pktbuf_alloc(const void *data, size_t size)
{
  pktbuf_t *pb = malloc(sizeof(pktbuf_t));
  pb->pb_refcount = 1;
  pb->pb_size = size;
  pb->pb_err = 0;

  if(size > 0) {
    pb->pb_data = malloc(size);
    if(data != NULL)
      memcpy(pb->pb_data, data, size);
  } else {
    pb->pb_data = NULL;
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

pktbuf_t *
pktbuf_append(pktbuf_t *pb, const void *data, size_t size)
{
  if (pb == NULL)
    return pktbuf_alloc(data, size);
  pb->pb_data = realloc(pb->pb_data, pb->pb_size + size);
  memcpy(pb->pb_data + pb->pb_size, data, size);
  pb->pb_size += size;
  return pb;
}
