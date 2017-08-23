/*
 *  "muxer" to write mpeg audio elementary streams
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
#include <sys/stat.h>

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
  off_t am_off;

  /* Filename is also used for logging */
  char *am_filename;
} audioes_muxer_t;


/**
 *
 */
static int
audioes_muxer_type(streaming_component_type_t type)
{
  muxer_container_type_t mc = MC_UNKNOWN;
  switch (type) {
  case SCT_MPEG2AUDIO:  mc = MC_MPEG2AUDIO; break;
  case SCT_AC3:         mc = MC_AC3; break;
  case SCT_EAC3:        mc = MC_AC3; break;
  case SCT_AAC:         mc = MC_AAC; break;
  case SCT_MP4A:        mc = MC_MP4A; break;
  case SCT_VORBIS:      mc = MC_VORBIS; break;
  default: break;
  }
  return mc;
}


/**
 *
 */
static const streaming_start_component_t *
audioes_get_component(muxer_t *m, const struct streaming_start *ss)
{
  const streaming_start_component_t *ssc;
  muxer_container_type_t mc = MC_UNKNOWN;
  int i, count;

  for (i = count = 0; i < ss->ss_num_components;i++) {
    ssc = &ss->ss_components[i];
    if ((!ssc->ssc_disabled) && (SCT_ISAUDIO(ssc->ssc_type))) {
      if (m->m_config.u.audioes.m_force_type != MC_UNKNOWN) {
        mc = audioes_muxer_type(ssc->ssc_type);
        if (m->m_config.u.audioes.m_force_type != mc)
          continue;
      }
      if (m->m_config.u.audioes.m_index == count)
        return ssc;
      count++;
    }
  }
  return NULL;
}


/**
 * Figure out the mimetype
 */
static const char *
audioes_muxer_mime(muxer_t *m, const struct streaming_start *ss)
{
  muxer_container_type_t mc = MC_UNKNOWN;
  const streaming_start_component_t *ssc;

  if (m->m_config.u.audioes.m_force_type != MC_UNKNOWN)
    return muxer_container_type2mime(m->m_config.u.audioes.m_force_type, 0);

  ssc = audioes_get_component(m, ss);
  if (ssc)
    mc = audioes_muxer_type(ssc->ssc_type);

  return muxer_container_type2mime(mc, 0);
}


/**
 * Reconfigure the muxer
 */
static int
audioes_muxer_reconfigure(muxer_t *m, const struct streaming_start *ss)
{
  audioes_muxer_t *am = (audioes_muxer_t*)m;
  const streaming_start_component_t *ssc;

  am->am_index = -1;

  ssc = audioes_get_component(m, ss);
  if (ssc)
    am->am_index = ssc->ssc_index;

  return 0;
}


/**
 * Init the builtin mkv muxer with streams
 */
static int
audioes_muxer_init(muxer_t* m, struct streaming_start *ss, const char *name)
{
  return audioes_muxer_reconfigure(m, ss);
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
  am->am_off      = 0;
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

  tvhtrace(LS_AUDIOES, "Creating file \"%s\" with file permissions \"%o\"", filename, am->m_config.m_file_permissions);
 
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, am->m_config.m_file_permissions);

  if(fd < 0) {
    am->am_error = errno;
    tvherror(LS_AUDIOES, "%s: Unable to create file, open failed -- %s",
	     filename, strerror(errno));
    am->m_errors++;
    return -1;
  }

  /* bypass umask settings */
  if (fchmod(fd, am->m_config.m_file_permissions))
    tvherror(LS_AUDIOES, "%s: Unable to change permissions -- %s",
             filename, strerror(errno));

  am->am_seekable = 1;
  am->am_off      = 0;
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
  size_t size;

  assert(smt == SMT_PACKET);

  if (pkt->pkt_componentindex != am->am_index) {
    pkt_ref_dec(pkt);
    return am->error;
  }

  size = pktbuf_len(pkt->pkt_payload);
  if (size == 0) {
    pkt_ref_dec(pkt);
    return am->error;
  }

  if (am->am_error) {
    am->m_errors++;
  } else if (tvh_write(am->am_fd, pktbuf_ptr(pkt->pkt_payload), size)) {
    am->am_error = errno;
    if (!MC_IS_EOS_ERROR(errno)) {
      tvherror(LS_AUDIOES, "%s: Write failed -- %s", am->am_filename,
               strerror(errno));
    } else {
      am->m_eos = 1;
    }
    am->m_errors++;
    if (am->am_seekable) {
      muxer_cache_update(m, am->am_fd, am->am_off, 0);
      am->am_off = lseek(am->am_fd, 0, SEEK_CUR);
    }
  } else {
    if (am->am_seekable)
      muxer_cache_update(m, am->am_fd, am->am_off, 0);
    am->am_off += size;
  }

  pkt_ref_dec(pkt);
  return am->error;
}


/**
 * NOP
 */
static int
audioes_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb, const char *comment)
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
    tvherror(LS_AUDIOES, "%s: Unable to close file -- %s",
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
  audioes_muxer_t *am = (audioes_muxer_t*)m;

  if (am->am_filename)
    free(am->am_filename);
  muxer_config_free(&am->m_config);
  muxer_hints_free(am->m_hints);
  free(am);
}


/**
 * Create a new builtin muxer
 */
muxer_t*
audioes_muxer_create(const muxer_config_t *m_cfg,
                     const muxer_hints_t *hints)
{
  audioes_muxer_t *am;

  if(m_cfg->m_type != MC_MPEG2AUDIO &&
     m_cfg->m_type != MC_AC3 &&
     m_cfg->m_type != MC_AAC &&
     m_cfg->m_type != MC_MP4A &&
     m_cfg->m_type != MC_VORBIS)
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

  return (muxer_t*)am;
}
