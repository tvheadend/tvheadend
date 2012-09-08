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

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "psi.h"
#include "muxer_pass.h"

#define TS_BUFFER_COUNT   100
#define TS_INJECTION_RATE 1000

/*
TODO: How often do we send the injected packets?
      Once evry packet? Once every 1000 packets?
      Or perhaps even once 10k packets?
      Maby we should messure in seconds instead?
      For now, every 1000 packets will do.
*/

typedef struct pass_muxer {
  muxer_t;

  /* File descriptor stuff */
  int   pm_fd;
  int   pm_seekable;
  int   pm_error;

  /* Filename is also used for logging */
  char *pm_filename;

  /* TS muxing */
  uint8_t  *pm_buf;
  uint8_t  *pm_pat;
  uint8_t  *pm_pmt;
  uint16_t pm_pmt_pid;
  uint32_t pm_ic; // Injection counter
  uint32_t pm_pc; // Packet counter
} pass_muxer_t;


/**
 * Figure out the mime-type for the muxed data stream
 */
static const char*
pass_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  int i;
  int has_audio;
  int has_video;
  const streaming_start_component_t *ssc;
  
  has_audio = 0;
  has_video = 0;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    has_video |= SCT_ISVIDEO(ssc->ssc_type);
    has_audio |= SCT_ISAUDIO(ssc->ssc_type);
  }

  if(has_video)
    return muxer_container_mimetype(m->m_container, 1);
  else if(has_audio)
    return muxer_container_mimetype(m->m_container, 0);
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

  if(pm->m_container == MC_MPEGTS) {
    
    memset(pm->pm_pat, 0xff, 188);
    pm->pm_pat[0] = 0x47;
    pm->pm_pat[1] = 0x40;
    pm->pm_pat[2] = 0x00;
    pm->pm_pat[3] = 0x10;
    pm->pm_pat[4] = 0x00;
    if(psi_build_pat(NULL, pm->pm_pat+5, 183, pm->pm_pmt_pid) < 0) {
      pm->m_errors++;
      tvhlog(LOG_ERR, "pass", "%s: Unable to build pat", pm->pm_filename);
      return -1;
    }

    memset(pm->pm_pmt, 0xff, 188);
    pm->pm_pmt[0] = 0x47;
    pm->pm_pmt[1] = 0x40 | (pm->pm_pmt_pid >> 8);
    pm->pm_pmt[2] = 0x00 | (pm->pm_pmt_pid >> 0);
    pm->pm_pmt[3] = 0x10;
    pm->pm_pmt[4] = 0x00;
    if(psi_build_pmt(ss, pm->pm_pmt+5, 183, ss->ss_pcr_pid) < 0) {
      pm->m_errors++;
      tvhlog(LOG_ERR, "pass", "%s: Unable to build pmt", pm->pm_filename);
      return -1;
    }
  }

  return 0;
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
  pm->pm_filename = strdup("Live stream");

  return 0;
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
  if(fd < 0) {
    pm->pm_error = errno;
    tvhlog(LOG_ERR, "pass", "%s: Unable to create file, open failed -- %s",
	   filename, strerror(errno));
    pm->m_errors++;
    return -1;
  }

  pm->pm_seekable = 1;
  pm->pm_fd       = fd;
  pm->pm_filename = strdup(filename);
  return 0;
}


/**
 * Write TS packets to the file descriptor
 */
static void
pass_muxer_write(muxer_t *m, const void *ts, size_t len)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_error) {
    pm->m_errors++;
  } else if(write(pm->pm_fd, ts, len) != len) {
    pm->pm_error = errno;
    tvhlog(LOG_ERR, "pass", "%s: Write failed -- %s", pm->pm_filename, 
	   strerror(errno));
    m->m_errors++;
  }
}


/**
 * Write TS packets to the file descriptor
 */
static void
pass_muxer_write_ts(muxer_t *m, struct th_pkt *pkt)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  int rem;

  // Inject pmt and pat into the stream
  rem = pm->pm_pc % TS_INJECTION_RATE;
  if(!rem) {
    pm->pm_pat[3] = (pm->pm_pat[3] & 0xf0) | (pm->pm_ic & 0x0f);
    pm->pm_pmt[3] = (pm->pm_pat[3] & 0xf0) | (pm->pm_ic & 0x0f);
    pass_muxer_write(m, pm->pm_pmt, 188);
    pass_muxer_write(m, pm->pm_pat, 188);
    pm->pm_ic++;
  }

  // flush buffer
  rem = pm->pm_pc % TS_BUFFER_COUNT;
  if(pm->pm_pc && !rem)
    pass_muxer_write(m, pm->pm_buf, TS_BUFFER_COUNT * 188);

  memcpy(pm->pm_buf + rem * 188, pkt, 188);
  pm->pm_pc++;
}


/**
 * Write a packet directly to the file descriptor
 */
static int
pass_muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  switch(pm->m_container) {
  case MC_MPEGTS:
    pass_muxer_write_ts(m, pkt);
    break;
  default:
    //NOP
    break;
  }

  if(!pm->pm_error)
    free(pkt);

  return pm->pm_error;
}


/**
 * NOP
 */
static int
pass_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  return 0;
}


/**
 * Close the file descriptor
 */
static int
pass_muxer_close(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_seekable && close(pm->pm_fd)) {
    pm->pm_error = errno;
    tvhlog(LOG_ERR, "pass", "%s: Unable to create file, open failed -- %s",
	   pm->pm_filename, strerror(errno));
    pm->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Free all memory associated with the muxer
 */
static void
pass_muxer_destroy(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_filename)
    free(pm->pm_filename);

  if(pm->pm_pmt)
    free(pm->pm_pmt);

  if(pm->pm_pat)
    free(pm->pm_pat);

  if(pm->pm_buf)
    free(pm->pm_buf);

  free(pm);
}


/**
 * Create a new passthrough muxer
 */
muxer_t*
pass_muxer_create(service_t *s, muxer_container_type_t mc)
{
  pass_muxer_t *pm;

  if(mc != MC_PASS)
    return NULL;

  pm = calloc(1, sizeof(pass_muxer_t));
  pm->m_open_stream  = pass_muxer_open_stream;
  pm->m_open_file    = pass_muxer_open_file;
  pm->m_init         = pass_muxer_init;
  pm->m_mime         = pass_muxer_mime;
  pm->m_write_meta   = pass_muxer_write_meta;
  pm->m_write_pkt    = pass_muxer_write_pkt;
  pm->m_close        = pass_muxer_close;
  pm->m_destroy      = pass_muxer_destroy;

  if(s->s_type == SERVICE_TYPE_V4L) {
    pm->m_container = MC_MPEGPS;
  } else {
    pm->m_container = MC_MPEGTS;
    pm->pm_pmt_pid = s->s_pmt_pid;
    pm->pm_pat = malloc(188);
    pm->pm_pmt = malloc(188);
    pm->pm_buf = malloc(188 * TS_BUFFER_COUNT);
  }

  return (muxer_t *)pm;
}

