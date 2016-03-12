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
#include "memoryinfo.h"

#ifndef PKTBUF_DATA_ALIGN
#define PKTBUF_DATA_ALIGN 64
#endif

memoryinfo_t pkt_memoryinfo = { .my_name = "Packets" };
memoryinfo_t pktbuf_memoryinfo = { .my_name = "Packet buffers" };
memoryinfo_t pktref_memoryinfo = { .my_name = "Packet references" };

/*
 *
 */
static void
pkt_destroy(th_pkt_t *pkt)
{
  if (pkt) {
    pktbuf_ref_dec(pkt->pkt_payload);
    pktbuf_ref_dec(pkt->pkt_meta);

    free(pkt);
    memoryinfo_free(&pkt_memoryinfo, sizeof(*pkt));
  }
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
  if (pkt) {
    if(datalen)
      pkt->pkt_payload = pktbuf_alloc(data, datalen);
    pkt->pkt_dts = dts;
    pkt->pkt_pts = pts;
    pkt->pkt_refcount = 1;
    memoryinfo_alloc(&pkt_memoryinfo, sizeof(*pkt));
  }
  return pkt;
}


/**
 *
 */
th_pkt_t *
pkt_copy_shallow(th_pkt_t *pkt)
{
  th_pkt_t *n = malloc(sizeof(th_pkt_t));

  if (n) {
    *n = *pkt;

    n->pkt_refcount = 1;

    pktbuf_ref_inc(n->pkt_meta);
    pktbuf_ref_inc(n->pkt_payload);

    memoryinfo_alloc(&pkt_memoryinfo, sizeof(*pkt));
  }

  return n;
}


/**
 *
 */
th_pkt_t *
pkt_copy_nodata(th_pkt_t *pkt)
{
  th_pkt_t *n = malloc(sizeof(th_pkt_t));

  if (n) {
    *n = *pkt;

    n->pkt_refcount = 1;

    n->pkt_meta = n->pkt_payload = NULL;

    memoryinfo_alloc(&pkt_memoryinfo, sizeof(*pkt));
  }

  return n;
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

  if (q) {
    while((pr = TAILQ_FIRST(q)) != NULL) {
      TAILQ_REMOVE(q, pr, pr_link);
      pkt_ref_dec(pr->pr_pkt);
      free(pr);
      memoryinfo_free(&pktref_memoryinfo, sizeof(*pr));
    }
  }
}


/**
 * Reference count is transfered to queue
 */
void
pktref_enqueue(struct th_pktref_queue *q, th_pkt_t *pkt)
{
  th_pktref_t *pr = malloc(sizeof(th_pktref_t));
  if (pr) {
    pr->pr_pkt = pkt;
    TAILQ_INSERT_TAIL(q, pr, pr_link);
    memoryinfo_alloc(&pktref_memoryinfo, sizeof(*pr));
  }
}


/**
 *
 */
void
pktref_remove(struct th_pktref_queue *q, th_pktref_t *pr)
{
  if (pr) {
    if (q)
      TAILQ_REMOVE(q, pr, pr_link);
    pkt_ref_dec(pr->pr_pkt);
    free(pr);
    memoryinfo_free(&pktref_memoryinfo, sizeof(*pr));
  }
}


/**
 *
 */
th_pktref_t *
pktref_create(th_pkt_t *pkt)
{
  th_pktref_t *pr = malloc(sizeof(th_pktref_t));
  if (pr) {
    pr->pr_pkt = pkt;
    memoryinfo_alloc(&pktref_memoryinfo, sizeof(*pr));
  }
  return pr;
}

/*
 *
 */

void
pktbuf_destroy(pktbuf_t *pb)
{
  if (pb) {
    memoryinfo_free(&pktbuf_memoryinfo, sizeof(*pb) + pb->pb_size);
    free(pb->pb_data);
    free(pb);
  }
}

void
pktbuf_ref_dec(pktbuf_t *pb)
{
  if (pb) {
    if((atomic_add(&pb->pb_refcount, -1)) == 1) {
      memoryinfo_free(&pktbuf_memoryinfo, sizeof(*pb) + pb->pb_size);
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

  if (pb == NULL) return NULL;
  pb->pb_refcount = 1;
  pb->pb_size = size;
  pb->pb_err = 0;
  if(size > 0) {
    pb->pb_data = malloc(size);
    if (pb->pb_data != NULL) {
      if (data != NULL)
        memcpy(pb->pb_data, data, size);
    } else {
      pb->pb_size = 0;
    }
  } else {
    pb->pb_data = NULL;
  }
  memoryinfo_alloc(&pktbuf_memoryinfo, sizeof(*pb) + pb->pb_size);
  return pb;
}

pktbuf_t *
pktbuf_make(void *data, size_t size)
{
  pktbuf_t *pb = malloc(sizeof(pktbuf_t));
  if (pb) {
    pb->pb_refcount = 1;
    pb->pb_size = size;
    pb->pb_data = data;
    memoryinfo_alloc(&pktbuf_memoryinfo, sizeof(*pb) + pb->pb_size);
  }
  return pb;
}

pktbuf_t *
pktbuf_append(pktbuf_t *pb, const void *data, size_t size)
{
  void *ndata;
  if (pb == NULL)
    return pktbuf_alloc(data, size);
  ndata = realloc(pb->pb_data, pb->pb_size + size);
  if (ndata) {
    pb->pb_data = ndata;
    memcpy(ndata + pb->pb_size, data, size);
    pb->pb_size += size;
    memoryinfo_append(&pktbuf_memoryinfo, size);
  }
  return pb;
}
