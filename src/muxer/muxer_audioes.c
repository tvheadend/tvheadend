/*
 *  "muxer" to write raw audio streams
 *  Copyright (C) 2013 Dave Chapman
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

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "channels.h"
#include "muxer_audioes.h"

typedef struct audioes_muxer {
  muxer_t;

  int error;

  /* Info about the service */
  int   am_index;

  /* File descriptor stuff */
  int   am_fd;
  int   am_seekable;
  int   am_error;

  /* Filename is also used for logging */
  char *am_filename;
} audioes_muxer_t;


/**
 * Figure out the mimetype
 */
static const char*
audioes_muxer_mime(muxer_t* m, const struct streaming_start *ss)
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

  if(has_audio)
    return muxer_container_type2mime(m->m_container, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}


/**
 * Init the builtin mkv muxer with streams
 */
static int
audioes_muxer_init(muxer_t* m, const struct streaming_start *ss, const char *name)
{
  audioes_muxer_t *am = (audioes_muxer_t*)m;
  const streaming_start_component_t *ssc;
  int i;

  am->am_index = -1;

  for(i = 0; i < ss->ss_num_components;i++) {
    ssc = &ss->ss_components[i];

    if ((!ssc->ssc_disabled) && (SCT_ISAUDIO(ssc->ssc_type))) {
      am->am_index = ssc->ssc_index;
      break;
    }
  }
  fprintf(stderr,"audioes_muxer_init - am_index=%d\n",am->am_index);

  return 0;
}


static int
audioes_muxer_reconfigure(muxer_t* m, const struct streaming_start *ss)
{
  /* TODO: Check our stream still exists? */

  return 0;
}


/*
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
audioes_muxer_open_stream(muxer_t *m, int fd)
{
  audioes_muxer_t *am = (audioes_muxer_t*)m;

  am->am_fd       = fd;
  am->am_seekable = 0;
  am->am_filename = strdup("Live stream");

  return 0;
}


/**
 * Open the file and set the file descriptor
 */
static int
audioes_muxer_open_file(muxer_t *m, const char *filename)
{
  int fd;
  audioes_muxer_t *am = (audioes_muxer_t*)m;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(fd < 0) {
    am->am_error = errno;
    tvhlog(LOG_ERR, "audioes", "%s: Unable to create file, open failed -- %s",
           filename, strerror(errno));
    am->m_errors++;
    return -1;
  }

  am->am_seekable = 1;
  am->am_fd       = fd;
  am->am_filename = strdup(filename);
  return 0;
}

/**
 * Write a packet to the muxer
 */
static int
audioes_muxer_write_pkt(muxer_t *m, streaming_message_type_t smt, void *data)
{
  th_pkt_t *pkt = (th_pkt_t*)data;
  audioes_muxer_t *am = (audioes_muxer_t*)m;

  assert(smt == SMT_PACKET);

fprintf(stderr,"audioes_muxer_write_pkt - index=%d, length=%d\n",pkt->pkt_componentindex, (int)pktbuf_len(pkt->pkt_payload));
  // TODO: pkt->pkt_componentindex

  if(pkt->pkt_componentindex != am->am_index) {
    pkt_ref_dec(pkt);
    return am->error;
  }

  if(am->am_error) {
    am->m_errors++;
  } else if(tvh_write(am->am_fd, pktbuf_ptr(pkt->pkt_payload), pktbuf_len(pkt->pkt_payload))) {
    am->am_error = errno;
    tvhlog(LOG_ERR, "audioes", "%s: Write failed -- %s", am->am_filename, 
           strerror(errno));
    am->m_errors++;
  }

  pkt_ref_dec(pkt);
  return am->error;
}


/**
 * Append meta data when a channel changes its programme
 */
static int
audioes_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  return 0;
}


/**
 * Close the muxer
 */
static int
audioes_muxer_close(muxer_t *m)
{
  audioes_muxer_t *am = (audioes_muxer_t*)m;

  if ((am->am_seekable) && (close(am->am_fd))) {
    am->am_error = errno;
    tvhlog(LOG_ERR, "audioes", "%s: Unable to close file -- %s",
           am->am_filename, strerror(errno));
    am->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Free all memory associated with the muxer
 */
static void
audioes_muxer_destroy(muxer_t *m)
{
}


/**
 * Create a new builtin muxer
 */
muxer_t*
audioes_muxer_create(muxer_container_type_t mc)
{
  audioes_muxer_t *am;

  if(mc != MC_AUDIOES)
    return NULL;

  am = calloc(1, sizeof(audioes_muxer_t));
  am->m_open_stream  = audioes_muxer_open_stream;
  am->m_open_file    = audioes_muxer_open_file;
  am->m_mime         = audioes_muxer_mime;
  am->m_init         = audioes_muxer_init;
  am->m_reconfigure  = audioes_muxer_reconfigure;
  am->m_write_meta   = audioes_muxer_write_meta;
  am->m_write_pkt    = audioes_muxer_write_pkt;
  am->m_close        = audioes_muxer_close;
  am->m_destroy      = audioes_muxer_destroy;
  am->m_container    = mc;

  return (muxer_t*)am;
}

