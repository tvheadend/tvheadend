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


/**
 *
 */
const char*
muxer_container_mimetype(muxer_container_type_t mc, int st)
{
  switch(mc) {
  case MC_MATROSKA:
    if(st == ST_RADIO)
      return "audio/x-matroska";
    else
      return "video/x-matroska";

  case MC_MPEGTS:
    if(st == ST_RADIO)
      return "audio/x-mpegts";
    else
      return "video/x-mpegts";

  case MC_WEBM:
    if(st == ST_RADIO)
      return "audio/webm";
    else
      return "video/webm";

  default:
    return "application/octet-stream";
  }
}


/**
 * 
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
 *
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

