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
#include <fcntl.h>

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "muxer_pass.h"

typedef struct pass_muxer {
  muxer_t;
  int pm_fd;
  int pm_seekable;
} pass_muxer_t;


/**
 * Init the passthrough muxer with streams
 */
static const char*
pass_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  int i, has_audio, has_video;
  const streaming_start_component_t *ssc;
  const source_info_t *si = &ss->ss_si;
  muxer_container_type_t mc = m->m_container;
  
  if(si->si_type == SERVICE_TYPE_V4L)
    mc = MC_MPEGPS;
  else
    mc = MC_MPEGTS;

  has_audio = has_video = 0;
  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    has_video |= SCT_ISVIDEO(ssc->ssc_type);
    has_audio |= SCT_ISAUDIO(ssc->ssc_type);
  }

  if(has_video)
    return muxer_container_mimetype(mc, 1);
  else if(has_audio)
    return muxer_container_mimetype(mc, 0);
  else
    return muxer_container_mimetype(MC_UNKNOWN, 0);
}

/**
 * Init the passthrough muxer with streams
 */
static int
pass_muxer_init(muxer_t* m, const struct streaming_start *ss, const char *name)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  return pm->m_errors;
}


/**
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
pass_muxer_open_stream(muxer_t *m, int fd)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  pm->pm_fd       = fd;
  pm->pm_seekable = 0;

  return pm->m_errors;
}


/**
 * Open the file and set the file descriptor
 */
static int
pass_muxer_open_file(muxer_t *m, const char *filename)
{
  int fd;
  pass_muxer_t *pm = (pass_muxer_t*)m;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(fd < 0)
    pm->m_errors++;

  pm->pm_seekable = 1;
  pm->pm_fd       = fd;

  return pm->m_errors;
}


/**
 * Write a packet directly to the file descriptor
 */
static int
pass_muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  int r;
  pass_muxer_t *pm = (pass_muxer_t*)m;

  r = write(pm->pm_fd, pkt, 188);

  m->m_errors += (r != 188);

  return m->m_errors;
}


/**
 * NOP
 */
static int
pass_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  return pm->m_errors;
}


/**
 * Close the file descriptor
 */
static int
pass_muxer_close(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_seekable && close(pm->pm_fd))
    pm->m_errors++;

  return pm->m_errors;
}


/**
 * Free all memory associated with the muxer
 */
static void
pass_muxer_destroy(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  free(pm);
}


/**
 * Create a new passthrough muxer
 */
muxer_t*
pass_muxer_create(muxer_container_type_t mc)
{
  pass_muxer_t *pm;

  if(mc != MC_PASS) {
    tvhlog(LOG_ERR, "mux",  "Passthrough muxer doesn't support '%s'",
	   muxer_container_type2txt(mc));
    return NULL;
  }

  pm = calloc(1, sizeof(pass_muxer_t));
  pm->m_open_stream  = pass_muxer_open_stream;
  pm->m_open_file    = pass_muxer_open_file;
  pm->m_init         = pass_muxer_init;
  pm->m_mime         = pass_muxer_mime;
  pm->m_write_meta   = pass_muxer_write_meta;
  pm->m_write_pkt    = pass_muxer_write_pkt;
  pm->m_close        = pass_muxer_close;
  pm->m_destroy      = pass_muxer_destroy;

  return (muxer_t *)pm;
}

