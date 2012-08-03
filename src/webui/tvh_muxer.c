/*
 *  tvheadend, wrapper for the builtin dvr muxer
 *  Copyright (C) 2012 John Törnblom
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "streaming.h"
#include "tvh_muxer.h"
#include "dvr/mkmux.h"

typedef struct tvh_muxer {
  muxer_t;
  mk_mux_t *tm_ref;
} tvh_muxer_t;


/**
 * Init the muxer with streams and write header
 */
static int
tvh_muxer_init(muxer_t* m, const struct streaming_start *ss, const struct channel *ch)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->tm_ref = mk_mux_stream_create(tm->m_fd, ss, ch);
  if(!tm->tm_ref)
    return -1;

  return 0;
}


/**
 * NOP
 */
static int
tvh_muxer_open(muxer_t *m)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  return tm->m_errors;
}


/**
 * Write a packet to the muxer
 */
static int
tvh_muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_write_pkt(tm->tm_ref, pkt);

  return tm->m_errors;
}


/**
 * Append meta data when a channel changes its programme
 */
static int
tvh_muxer_write_meta(muxer_t *m, epg_broadcast_t *eb)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_append_meta(tm->tm_ref, eb);

  return tm->m_errors;
}


/**
 * Close the muxer and append trailer to output
 */
static int
tvh_muxer_close(muxer_t *m)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  mk_mux_close(tm->tm_ref);

  return tm->m_errors;
}


/**
 * Free all memory associated with the muxer
 */
static void
tvh_muxer_destroy(muxer_t *m)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  if(tm->tm_ref)
    free(tm->tm_ref);

  free(tm);
}


/**
 * Create a new builtin muxer
 */
muxer_t*
tvh_muxer_create(muxer_container_type_t mc)
{
  tvh_muxer_t *tm;

  if(mc != MC_MATROSKA) {
    tvhlog(LOG_ERR, "mux",  "Builtin muxer doesn't support '%s'",
	   muxer_container_type2txt(mc));
    return NULL;
  }

  tm = calloc(1, sizeof(tvh_muxer_t));
  tm->m_init         = tvh_muxer_init;
  tm->m_open         = tvh_muxer_open;
  tm->m_close        = tvh_muxer_close;
  tm->m_destroy      = tvh_muxer_destroy;
  tm->m_write_meta   = tvh_muxer_write_meta;
  tm->m_write_pkt    = tvh_muxer_write_pkt;

  return (muxer_t*)tm;
}

