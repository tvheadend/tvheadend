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
  free(pkt->pkt_payload);
  free(pkt->pkt_globaldata);
  free(pkt);
}


/**
 * Allocate a new packet and give it a refcount of one (which caller is
 * suppoed to take care of)
 */
th_pkt_t *
pkt_alloc(void *data, size_t datalen, int64_t pts, int64_t dts)
{
  th_pkt_t *pkt;

  pkt = calloc(1, sizeof(th_pkt_t));
  pkt->pkt_payloadlen = datalen;
  if(datalen > 0) {
    pkt->pkt_payload = malloc(datalen + FF_INPUT_BUFFER_PADDING_SIZE);
    if(data != NULL)
      memcpy(pkt->pkt_payload, data, datalen);
  }

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
pkt_merge_global(th_pkt_t *pkt)
{
  th_pkt_t *n;

  if(pkt->pkt_globaldata == NULL)
    return pkt;

  n = malloc(sizeof(th_pkt_t));
  *n = *pkt;

  n->pkt_refcount = 1;
  n->pkt_globaldata = NULL;
  n->pkt_globaldata_len = 0;
  
  n->pkt_payloadlen = pkt->pkt_globaldata_len + pkt->pkt_payloadlen;

  n->pkt_payload = malloc(n->pkt_payloadlen + FF_INPUT_BUFFER_PADDING_SIZE);
  memcpy(n->pkt_payload, pkt->pkt_globaldata, pkt->pkt_globaldata_len);
  memcpy(n->pkt_payload + pkt->pkt_globaldata_len, pkt->pkt_payload,
	 pkt->pkt_payloadlen);

  pkt_ref_dec(pkt);
  
  return n;
}
