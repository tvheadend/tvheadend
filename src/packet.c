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
#include "streaming.h"
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
pkt_alloc(streaming_component_type_t type, const uint8_t *data, size_t datalen,
          int64_t pts, int64_t dts, int64_t pcr)
{
  th_pkt_t *pkt;
  pktbuf_t *payload;

  if (datalen > 0) {
    payload = pktbuf_alloc(data, datalen);
    if (payload == NULL)
      return NULL;
  } else {
    payload = NULL;
  }

  pkt = calloc(1, sizeof(th_pkt_t));
  if (pkt) {
    pkt->pkt_type = type;
    pkt->pkt_payload = payload;
    pkt->pkt_dts = dts;
    pkt->pkt_pts = pts;
    pkt->pkt_pcr = pcr;
    pkt->pkt_refcount = 1;
    memoryinfo_alloc(&pkt_memoryinfo, sizeof(*pkt));
  } else {
    if (payload)
      pktbuf_ref_dec(payload);
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
    blacklisted_memcpy(n, pkt, sizeof(*pkt));

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
    blacklisted_memcpy(n, pkt, sizeof(*pkt));

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
pkt_trace_(const char *file, int line, int subsys, th_pkt_t *pkt,
           const char *fmt, ...)
{
  char buf[512], _pcr[22], _dts[22], _pts[22], _type[2], _meta[20];
  va_list args;

  va_start(args, fmt);
  if (SCT_ISVIDEO(pkt->pkt_type) && pkt->v.pkt_frametype) {
    _type[0] = pkt_frametype_to_char(pkt->v.pkt_frametype);
    _type[1] = '\0';
  } else {
    _type[0] = '\0';
  }
  if (pkt->pkt_meta)
    snprintf(_meta, sizeof(_meta), " meta %zu", pktbuf_len(pkt->pkt_meta));
  else
    _meta[0] = '\0';
  snprintf(buf, sizeof(buf),
           "%s%spkt stream %d %s%s%s"
           " pcr %s dts %s pts %s"
           " dur %d len %zu err %i%s%s",
           fmt ? fmt : "",
           fmt ? " (" : "",
           pkt->pkt_componentindex,
           streaming_component_type2txt(pkt->pkt_type),
           _type[0] ? " type " : "", _type,
           pts_to_string(pkt->pkt_pcr, _pcr),
           pts_to_string(pkt->pkt_dts, _dts),
           pts_to_string(pkt->pkt_pts, _pts),
           pkt->pkt_duration,
           pktbuf_len(pkt->pkt_payload),
           pkt->pkt_err,
           _meta,
           fmt ? ")" : "");
  tvhlogv(file, line, LOG_TRACE, subsys, buf, &args);
  va_end(args);
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
 * Reference count is transferred to queue
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
 * Reference count is transferred to queue
 */
void
pktref_enqueue_sorted(struct th_pktref_queue *q, th_pkt_t *pkt,
                      int (*cmp)(const void *, const void *))
{
  th_pktref_t *pr = malloc(sizeof(th_pktref_t));
  if (pr) {
    pr->pr_pkt = pkt;
    TAILQ_INSERT_SORTED(q, pr, pr_link, cmp);
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
th_pkt_t *
pktref_first(struct th_pktref_queue *q)
{
  th_pktref_t *pr;

  pr = TAILQ_FIRST(q);
  if (pr)
    return pr->pr_pkt;
  return NULL;
}

/**
 *
 */
th_pkt_t *
pktref_get_first(struct th_pktref_queue *q)
{
  th_pktref_t *pr;
  th_pkt_t *pkt;

  pr = TAILQ_FIRST(q);
  if (pr) {
    pkt = pr->pr_pkt;
    TAILQ_REMOVE(q, pr, pr_link);
    free(pr);
    memoryinfo_free(&pktref_memoryinfo, sizeof(*pr));
    return pkt;
  }
  return NULL;
}

/**
 *
 */
void
pktref_insert_head(struct th_pktref_queue *q, th_pkt_t *pkt)
{
  th_pktref_t *pr;

  pr = pktref_create(pkt);
  TAILQ_INSERT_HEAD(q, pr, pr_link);
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
pktbuf_alloc(const uint8_t *data, size_t size)
{
  pktbuf_t *pb;
  uint8_t *buffer;

  buffer = size > 0 ? malloc(size) : NULL;
  if (buffer) {\
    if (data != NULL)
      memcpy(buffer, data, size);
  } else if (size > 0) {
    return NULL;
  }
  pb = malloc(sizeof(pktbuf_t));
  if (pb == NULL) {
    free(buffer);
    return NULL;
  }
  pb->pb_refcount = 1;
  pb->pb_data = buffer;
  pb->pb_size = size;
  pb->pb_err = 0;
  memoryinfo_alloc(&pktbuf_memoryinfo, sizeof(*pb) + size);
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

/*
 *
 */

const char *pts_to_string(int64_t pts, char *buf)
{
  if (pts == PTS_UNSET)
    return strcpy(buf, "<unset>");
  snprintf(buf, 22, "%"PRId64, pts);
  return buf;
}
