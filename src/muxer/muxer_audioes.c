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
  const streaming_start_component_t *ssc;
  
  has_audio = 0;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    has_audio |= SCT_ISAUDIO(ssc->ssc_type);
  }

  if(has_audio)
    return muxer_container_type2mime(MC_AUDIOES, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}


/**
 * Init the builtin mkv muxer with streams
 */
static int
audioes_muxer_init(muxer_t* m, struct streaming_start *ss, const char *name)
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

  // TODO: pkt->pkt_componentindex
  /* TODO: ^ What does this even mean? */	

  if(pkt->pkt_componentindex != am->am_index) {
    pkt_ref_dec(pkt);
    return am->error;
  }

  if(am->am_error) {
    am->m_errors++;
  } else if(tvh_write(am->am_fd, pktbuf_ptr(pkt->pkt_payload), pktbuf_len(pkt->pkt_payload))) {
    am->am_error = errno;
    tvherror(LS_AUDIOES, "%s: Write failed -- %s", am->am_filename, 
           strerror(errno));
    /* TODO: Do some EOS handling here. Whatever that is. See muxer_pass.c:415 */
    am->m_errors++;
	/* TODO: A muxer_cache_update() call is still missing here. */
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

	if(am->am_filename)
		free(am->am_filename);
	
	free(am);	
}


/**
 * Create a new builtin muxer
 */
muxer_t*
audioes_muxer_create(const muxer_config_t *m_cfg)
{
  audioes_muxer_t *am;

  if(m_cfg->m_type != MC_AUDIOES)
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
  //am->m_container    = *m_cfg; //WTF DOES THIS DO?!

  return (muxer_t*)am;
}

