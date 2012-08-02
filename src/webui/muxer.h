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


#ifndef MUXER_H_
#define MUXER_H_

typedef enum {
  MC_UNKNOWN = 0,
  MC_MATROSKA = 1,
  MC_MPEGTS = 2,
  MC_WEBM = 3
} muxer_container_type_t;

const char *muxer_container_mimetype(muxer_container_type_t mc, int st);
const char *muxer_container_type2txt(muxer_container_type_t mc);
muxer_container_type_t muxer_container_txt2type(const char *str);

#endif
