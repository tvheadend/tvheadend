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

#if ENABLE_LIBAV
#include "lav_muxer.h"
#endif

/**
 * Get the mime type for a service muxer
 */
static const char*
muxer_container_mimetype(muxer_container_type_t mc, struct service *s)
{
  switch(mc) {

  case MC_MATROSKA:
    if(s->s_servicetype == ST_RADIO)
      return "audio/x-matroska";
    else
      return "video/x-matroska";

  case MC_MPEGTS:
    if(s->s_servicetype == ST_RADIO)
      return "audio/x-mpegts";
    else
      return "video/x-mpegts";

  case MC_WEBM:
    if(s->s_servicetype == ST_RADIO)
      return "audio/webm";
    else
      return "video/webm";

  default:
    return "application/octet-stream";
  }
}


/**
 * Convert a container type to a string
 */
const char*
muxer_container_type2txt(muxer_container_type_t mc)
{
  switch(mc) {

  case MC_MATROSKA:
    return "matroska";

  case MC_MPEGTS:
    return "mpegts";

  case MC_WEBM:
    return "webm";

  default:
    return "unknown";
  }
}


/**
 * Convert a string to a container type
 */
muxer_container_type_t
muxer_container_txt2type(const char *str)
{
  if(!str)
    return MC_UNKNOWN;

  else if(!strcmp("matroska", str))
    return MC_MATROSKA;

  else if(!strcmp("mpegts", str))
    return MC_MPEGTS;

  else if(!strcmp("webm", str))
    return MC_WEBM;

  else
    return MC_UNKNOWN;
}


/**
 * Create a new muxer
 */
muxer_t* 
muxer_create(int fd, struct service *s, muxer_container_type_t mc)
{
  muxer_t *m;

#if ENABLE_LIBAV
  m = lav_muxer_create(mc);
#else
  //m = tvh_muxer_create(mc);
  
#endif

  if(m) {
    m->m_fd        = fd;
    m->m_container = mc;
    m->m_mime      = muxer_container_mimetype(mc, s);
  }

  return m;
}

