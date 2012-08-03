/*
 *  tvheadend, simple muxer that just passes the input along
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

#include <unistd.h>

#include "tvheadend.h"
#include "streaming.h"
#include "pass_muxer.h"

/**
 * Init the passthrough muxer
 */
static int
pass_muxer_init(muxer_t* m, const struct streaming_start *ss, const struct channel *ch)
{
  return 0;
}


/**
 * NOP
 */
static int
pass_muxer_open(muxer_t *m)
{
  return m->m_errors;
}


/**
 * Write a packet directly to the socket
 */
static int
pass_muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  int r;

  r = write(m->m_fd, pkt, 188);

  m->m_errors += (r != 188);

  return m->m_errors;
}


/**
 * NOP
 */
static int
pass_muxer_write_meta(muxer_t *m, epg_broadcast_t *eb)
{
  return m->m_errors;
}


/**
 * NOP
 */
static int
pass_muxer_close(muxer_t *m)
{
  return m->m_errors;
}


/**
 * Free all memory associated with the muxer
 */
static void
pass_muxer_destroy(muxer_t *m)
{
  free(m);
}


/**
 * Create a new passthrough muxer
 */
muxer_t*
pass_muxer_create(muxer_container_type_t mc)
{
  muxer_t *m;

  if(mc != MC_PASS) {
    tvhlog(LOG_ERR, "mux",  "Passthrough muxer doesn't support '%s'",
	   muxer_container_type2txt(mc));
    return NULL;
  }

  m = calloc(1, sizeof(muxer_t));
  m->m_init         = pass_muxer_init;
  m->m_open         = pass_muxer_open;
  m->m_close        = pass_muxer_close;
  m->m_destroy      = pass_muxer_destroy;
  m->m_write_meta   = pass_muxer_write_meta;
  m->m_write_pkt    = pass_muxer_write_pkt;

  return m;
}

