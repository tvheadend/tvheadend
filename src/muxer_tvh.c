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
#include "epg.h"
#include "channels.h"
#include "dvr/mkmux.h"
#include "muxer_tvh.h"

typedef struct tvh_muxer {
  muxer_t;
  mk_mux_t *tm_ref;
} tvh_muxer_t;


/**
 * Figure out the mimetype
 */
static const char*
tvh_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  //TODO: iterate streams

  return muxer_container_mimetype(tm->m_container, 1);
}


/**
 * Init the builtin mkv muxer with streams
 */
static int
tvh_muxer_init(muxer_t* m, const struct streaming_start *ss, const char *name)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_init(tm->tm_ref, name, ss);

  return tm->m_errors;
}


/**
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
tvh_muxer_open_stream(muxer_t *m, int fd)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_open_stream(tm->tm_ref, fd);

  return tm->m_errors;
}


/**
 * Open a file
 */
static int
tvh_muxer_open_file(muxer_t *m, const char *filename)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;
  
  tm->m_errors += mk_mux_open_file(tm->tm_ref, filename);

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
tvh_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_write_meta(tm->tm_ref, NULL, eb);

  return tm->m_errors;
}


/**
 * Close the muxer and append trailer to output
 */
static int
tvh_muxer_close(muxer_t *m)
{
  tvh_muxer_t *tm = (tvh_muxer_t*)m;

  tm->m_errors += mk_mux_close(tm->tm_ref);

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
  tm->m_open_stream  = tvh_muxer_open_stream;
  tm->m_open_file    = tvh_muxer_open_file;
  tm->m_mime         = tvh_muxer_mime;
  tm->m_init         = tvh_muxer_init;
  tm->m_write_meta   = tvh_muxer_write_meta;
  tm->m_write_pkt    = tvh_muxer_write_pkt;
  tm->m_close        = tvh_muxer_close;
  tm->m_destroy      = tvh_muxer_destroy;
  tm->tm_ref         = mk_mux_create();

  return (muxer_t*)tm;
}

