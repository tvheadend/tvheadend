/*
 *  tvheadend, generic muxing utils
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

#include "tvheadend.h"
#include "service.h"
#include "muxer.h"
#include "muxer_tvh.h"
#include "muxer_pass.h"


/**
 * Mime type for containers containing only audio
 */
static struct strtab container_audio_mime[] = {
  { "application/octet-stream", MC_UNKNOWN },
  { "audio/x-matroska",         MC_MATROSKA },
  { "audio/x-mpegts",           MC_MPEGTS },
  { "audio/mpeg",               MC_MPEGPS },
  { "application/octet-stream", MC_PASS },
};


/**
 * Mime type for containers
 */
static struct strtab container_video_mime[] = {
  { "application/octet-stream", MC_UNKNOWN },
  { "video/x-matroska",         MC_MATROSKA },
  { "video/x-mpegts",           MC_MPEGTS },
  { "video/mpeg",               MC_MPEGPS },
  { "application/octet-stream", MC_PASS },
};


/**
 * Name of the container
 */
static struct strtab container_name[] = {
  { "unknown",  MC_UNKNOWN },
  { "matroska", MC_MATROSKA },
  { "mpegts",   MC_MPEGTS },
  { "mpegps",   MC_MPEGPS },
  { "pass",     MC_PASS },
};


/**
 * filename suffix of audio-only streams
 */
static struct strtab container_audio_file_suffix[] = {
  { "bin",  MC_UNKNOWN },
  { "mka",  MC_MATROSKA },
  { "ts",   MC_MPEGTS },
  { "mpeg", MC_MPEGPS },
  { "bin",  MC_PASS },
};


/**
 * filename suffix of video streams
 */
static struct strtab container_video_file_suffix[] = {
  { "bin",  MC_UNKNOWN },
  { "mkv",  MC_MATROSKA },
  { "ts",   MC_MPEGTS },
  { "mpeg", MC_MPEGPS },
  { "bin",  MC_PASS },
};


/**
 * Get the mime type for a container
 */
const char*
muxer_container_mimetype(muxer_container_type_t mc, int video)
{
  const char *str;

  if(video)
    str = val2str(mc, container_video_mime);
  else
    str = val2str(mc, container_audio_mime);

  if(!str)
    str = val2str(MC_UNKNOWN, container_video_mime);

  return str;
}


/**
 * Get the suffix used in file names
 */
const char*
muxer_container_suffix(muxer_container_type_t mc, int video)
{
  const char *str;
  if(video)
    str = val2str(mc, container_video_file_suffix);
  else
    str = val2str(mc, container_audio_file_suffix);

  if(!str)
    str = val2str(MC_UNKNOWN, container_video_file_suffix);

  return str;
}


/**
 * Convert a container type to a string
 */
const char*
muxer_container_type2txt(muxer_container_type_t mc)
{
  const char *str;

  str = val2str(mc, container_name);
  if(!str)
    return "unknown";
 
  return str;
}


/**
 * Convert a string to a container type
 */
muxer_container_type_t
muxer_container_txt2type(const char *str)
{
  muxer_container_type_t mc;
  
  if(!str)
    return MC_UNKNOWN;

  mc = str2val(str, container_name);
  if(mc == -1)
    return MC_UNKNOWN;

  return mc;
}


/**
 * Create a new muxer
 */
muxer_t* 
muxer_create(service_t *s, muxer_container_type_t mc)
{
  muxer_t *m;

  m = pass_muxer_create(s, mc);

  if(!m)
    m = tvh_muxer_create(mc);

  if(!m)
    tvhlog(LOG_ERR, "mux", "Can't find a muxer that supports '%s' container",
	   muxer_container_type2txt(mc));

  return m;
}


/**
 * sanity wrapper arround m_mime()
 */
const char*
muxer_mime(muxer_t *m,  const struct streaming_start *ss)
{
  if(!m || !ss)
    return NULL;

  return m->m_mime(m, ss);
}


/**
 * Figure out the file suffix by looking at the mime type
 */
const char*
muxer_suffix(muxer_t *m,  const struct streaming_start *ss)
{
  const char *mime;
  int video;

  if(!m || !ss)
    return NULL;

  mime  = m->m_mime(m, ss);
  video = memcmp("audio", mime, 5);

  return muxer_container_suffix(m->m_container, video);
}


/**
 * sanity wrapper arround m_init()
 */
int
muxer_init(muxer_t *m, const struct streaming_start *ss, const char *name)
{
  if(!m || !ss)
    return -1;

  return m->m_init(m, ss, name);
}


/**
 * sanity wrapper arround m_reconfigure()
 */
int
muxer_reconfigure(muxer_t *m, const struct streaming_start *ss)
{
  if(!m || !ss)
    return -1;

  return m->m_reconfigure(m, ss);
}


/**
 * sanity wrapper arround m_open_file()
 */
int
muxer_open_file(muxer_t *m, const char *filename)
{
  if(!m || !filename)
    return -1;

  return m->m_open_file(m, filename);
}


/**
 * sanity wrapper arround m_open_stream()
 */
int
muxer_open_stream(muxer_t *m, int fd)
{
  if(!m || fd < 0)
    return -1;

  return m->m_open_stream(m, fd);
}


/**
 * sanity wrapper arround m_close()
 */
int
muxer_close(muxer_t *m)
{
  if(!m)
    return -1;

  return m->m_close(m);
}

/**
 * sanity wrapper arround m_destroy()
 */
int
muxer_destroy(muxer_t *m)
{
  if(!m)
    return -1;

  m->m_destroy(m);

  return 0;
}


/**
 * sanity wrapppper arround m_write_meta()
 */
int
muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  if(!m || !eb)
    return -1;

  return m->m_write_meta(m, eb);
}


/**
 * sanity wrapper arround m_write_pkt()
 */
int
muxer_write_pkt(muxer_t *m, streaming_message_type_t smt, void *data)
{
  if(!m || !data)
    return -1;

  return m->m_write_pkt(m, smt, data);
}


