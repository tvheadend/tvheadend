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
#include <fcntl.h>

#include "tvheadend.h"
#include "service.h"
#include "muxer.h"
#include "muxer/muxer_mkv.h"
#include "muxer/muxer_pass.h"
#if CONFIG_LIBAV
#include "muxer/muxer_libav.h"
#endif

#if defined(PLATFORM_DARWIN)
#define fdatasync(fd)       fcntl(fd, F_FULLFSYNC)
#elif defined(PLATFORM_FREEBSD)
#define fdatasync(fd)       fsync(fd)
#endif

/**
 * Mime type for containers containing only audio
 */
static struct strtab container_audio_mime[] = {
  { "application/octet-stream", MC_UNKNOWN },
  { "audio/x-matroska",         MC_MATROSKA },
  { "audio/x-matroska",         MC_AVMATROSKA },
  { "audio/webm",               MC_WEBM },
  { "audio/webm",               MC_AVWEBM },
  { "audio/mp2t",               MC_MPEGTS },
  { "audio/mpeg",               MC_MPEGPS },
  { "audio/mp4",                MC_AVMP4 },
  { "application/octet-stream", MC_PASS },
  { "application/octet-stream", MC_RAW },
};


/**
 * Mime type for containers
 */
static struct strtab container_video_mime[] = {
  { "application/octet-stream", MC_UNKNOWN },
  { "video/x-matroska",         MC_MATROSKA },
  { "video/x-matroska",         MC_AVMATROSKA },
  { "video/webm",               MC_WEBM },
  { "video/webm",               MC_AVWEBM },
  { "video/mp2t",               MC_MPEGTS },
  { "video/mpeg",               MC_MPEGPS },
  { "video/mp4",                MC_AVMP4 },
  { "application/octet-stream", MC_PASS },
  { "application/octet-stream", MC_RAW },
};


/**
 * Name of the container
 */
static struct strtab container_name[] = {
  { "unknown",    MC_UNKNOWN },
  { "matroska",   MC_MATROSKA },
  { "webm",       MC_WEBM },
  { "mpegts",     MC_MPEGTS },
  { "mpegps",     MC_MPEGPS },
  { "pass",       MC_PASS },
  { "raw",        MC_RAW },
  { "avmatroska", MC_AVMATROSKA },
  { "avwebm",     MC_AVWEBM },
  { "avmp4",      MC_AVMP4 },
};


/**
 * filename suffix of audio-only streams
 */
static struct strtab container_audio_file_suffix[] = {
  { "bin",  MC_UNKNOWN },
  { "mka",  MC_MATROSKA },
  { "webm", MC_WEBM },
  { "ts",   MC_MPEGTS },
  { "mpeg", MC_MPEGPS },
  { "bin",  MC_PASS },
  { "bin",  MC_RAW },
  { "mka",  MC_AVMATROSKA },
  { "webm", MC_AVWEBM },
  { "mp4",  MC_AVMP4 },
};


/**
 * filename suffix of video streams
 */
static struct strtab container_video_file_suffix[] = {
  { "bin",  MC_UNKNOWN },
  { "mkv",  MC_MATROSKA },
  { "webm", MC_WEBM },
  { "ts",   MC_MPEGTS },
  { "mpeg", MC_MPEGPS },
  { "bin",  MC_PASS },
  { "bin",  MC_RAW },
  { "mkv",  MC_AVMATROSKA },
  { "webm", MC_AVWEBM },
  { "mp4",  MC_AVMP4 },
};


/**
 * Get the mime type for a container
 */
const char*
muxer_container_type2mime(muxer_container_type_t mc, int video)
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
 * Get the mime type for a filename
 */
const char*
muxer_container_filename2mime(const char *filename, int video)
{
  int mc = MC_UNKNOWN;
  const char *suffix;
  
  if(filename) {
    suffix = strrchr(filename, '.');
    if (suffix == NULL)
      suffix = filename;
    else
      suffix++;
    if(video)
      mc = str2val(suffix, container_video_file_suffix);
    else
      mc = str2val(suffix, container_audio_file_suffix);
  }

  return muxer_container_type2mime(mc, 1);
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
 * Convert a container name to a container type
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
 * Convert a mime-string to a container type
 */
muxer_container_type_t
muxer_container_mime2type(const char *str)
{
  muxer_container_type_t mc;

  if(!str)
    return MC_UNKNOWN;

  mc = str2val(str, container_video_mime);
  if(mc == -1)
    mc = str2val(str, container_audio_mime);

  if(mc == -1)
    return MC_UNKNOWN;

  return mc;
}


/**
 * Create a new muxer
 */
muxer_t* 
muxer_create(const muxer_config_t *m_cfg)
{
  muxer_t *m;

  assert(m_cfg);

  m = pass_muxer_create(m_cfg);

  if(!m)
    m = mkv_muxer_create(m_cfg);

#if CONFIG_LIBAV
  if(!m)
    m = lav_muxer_create(m_cfg);
#endif

  if(!m) {
    tvhlog(LOG_ERR, "mux", "Can't find a muxer that supports '%s' container",
	   muxer_container_type2txt(m_cfg->m_type));
    return NULL;
  }
  
  memcpy(&m->m_config, m_cfg, sizeof(muxer_config_t));

  return m;
}

/**
 * Figure out the file suffix by looking at the mime type
 */
const char*
muxer_suffix(muxer_t *m,  const struct streaming_start *ss)
{
  const char *mime;
  muxer_container_type_t mc;
  int video;

  if(!m || !ss)
    return NULL;

  mime  = m->m_mime(m, ss);
  video = memcmp("audio", mime, 5);
  mc = muxer_container_mime2type(mime);

  return muxer_container_suffix(mc, video);
}

/**
 * cache type conversions
 */
static struct strtab cache_types[] = {
  { "Unknown",            MC_CACHE_UNKNOWN },
  { "System",             MC_CACHE_SYSTEM },
  { "Do not keep",        MC_CACHE_DONTKEEP },
  { "Sync",               MC_CACHE_SYNC },
  { "Sync + Do not keep", MC_CACHE_SYNCDONTKEEP }
};

const char*
muxer_cache_type2txt(muxer_cache_type_t c)
{
  return val2str(c, cache_types);
}

muxer_cache_type_t
muxer_cache_txt2type(const char *str)
{
  int r = str2val(str, cache_types);
  if (r < 0)
    r = MC_CACHE_UNKNOWN;
  return r;
}

/**
 * cache scheme
 */
void
muxer_cache_update(muxer_t *m, int fd, off_t pos, size_t size)
{
  switch (m->m_config.m_cache) {
  case MC_CACHE_UNKNOWN:
  case MC_CACHE_SYSTEM:
    break;
  case MC_CACHE_SYNC:
    fdatasync(fd);
    break;
  case MC_CACHE_SYNCDONTKEEP:
    fdatasync(fd);
    /* fall through */
  case MC_CACHE_DONTKEEP:
#if defined(PLATFORM_DARWIN)
    fcntl(fd, F_NOCACHE, 1);
#elif !ENABLE_ANDROID
    posix_fadvise(fd, pos, size, POSIX_FADV_DONTNEED);
#endif
    break;
  default:
    abort();
  }
}

/**
 * Get a list of supported cache schemes
 */
int
muxer_cache_list(htsmsg_t *array)
{
  htsmsg_t *mc;
  int c;
  const char *s;

  for (c = 0; c <= MC_CACHE_LAST; c++) {
    mc = htsmsg_create_map();
    s = muxer_cache_type2txt(c);
    htsmsg_add_u32(mc, "index",       c);
    htsmsg_add_str(mc, "description", s);
    htsmsg_add_msg(array, NULL, mc);
  }

  return c;
}
