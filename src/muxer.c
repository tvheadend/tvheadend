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
#if ENABLE_LIBAV
#include "muxer_libav.h"
#endif


/**
 * Mime type for containers containing only audio
 */
static struct strtab container_audio_mime[] = {
  { "application/octet-stream", MC_UNKNOWN },
  { "audio/x-matroska",         MC_MATROSKA },
  { "audio/x-mpegts",           MC_MPEGTS },
  { "audio/webm",               MC_WEBM },
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
  { "video/webm",               MC_WEBM },
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
  { "webm",     MC_WEBM },
  { "pass",     MC_PASS },
};


/**
 * Name of the container
 */
static struct strtab container_file_suffix[] = {
  { "bin",  MC_UNKNOWN },
  { "mkv",  MC_MATROSKA },
  { "ts",   MC_MPEGTS },
  { "mpeg", MC_MPEGPS },
  { "webm", MC_WEBM },
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
muxer_container_suffix(muxer_container_type_t mc)
{
  const char *str;

  str = val2str(mc, container_file_suffix);

  if(!str)
    str = val2str(MC_UNKNOWN, container_file_suffix);

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
muxer_create(muxer_container_type_t mc)
{
  muxer_t *m = NULL;

  if(mc == MC_PASS)
    m = pass_muxer_create(mc);

#if ENABLE_LIBAV
  if(!m)
    m = lav_muxer_create(mc);
#endif

  if(!m)
    m = tvh_muxer_create(mc);
  
  if(m) {
    m->m_container = mc;
  }

  return m;
}


/**
 *
 */
const char*
muxer_mime(muxer_t *m,  const struct streaming_start *ss)
{
  if(!m || !ss)
    return NULL;

  return m->m_mime(m, ss);
}


/**
 *
 */
int
muxer_init(muxer_t *m, const struct streaming_start *ss, const char *name)
{
  if(!m || !ss)
    return -1;

  return m->m_init(m, ss, name);
}


/**
 *
 */
int
muxer_open_file(muxer_t *m, const char *filename)
{
  if(!m || !filename)
    return -1;

  return m->m_open_file(m, filename);
}


/**
 *
 */
int
muxer_open_stream(muxer_t *m, int fd)
{
  if(!m || fd < 0)
    return -1;

  return m->m_open_stream(m, fd);
}


/**
 *
 */
int
muxer_close(muxer_t *m)
{
  if(!m)
    return -1;

  return m->m_close(m);
}

/**
 *
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
 *
 */
int
muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
{
  if(!m || !eb)
    return -1;

  return m->m_write_meta(m, eb);
}


/**
 *
 */
int
muxer_write_pkt(muxer_t *m, struct th_pkt *pkt)
{
  if(!m || !pkt)
    return -1;

  return m->m_write_pkt(m, pkt);
}


