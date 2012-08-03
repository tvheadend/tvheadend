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

#include "epg.h"

typedef enum {
  MC_UNKNOWN     = 0,
  MC_MATROSKA    = 1,
  MC_MPEGTS      = 2,
  MC_WEBM        = 3,
  MC_MPEGPS      = 4,
  MC_PASS        = 5,
} muxer_container_type_t;

struct muxer;

typedef struct muxer {
  int  (*m_init)       (struct muxer *, 
			const struct streaming_start *,
			const struct channel *);            // Init The muxer
  int  (*m_open)       (struct muxer *);                    // Write header 
  int  (*m_close)      (struct muxer *);                    // Write trailer
  void (*m_destroy)    (struct muxer *);                    // Free the memory
  int  (*m_write_meta) (struct muxer *, epg_broadcast_t *); // Append epg data
  int  (*m_write_pkt)  (struct muxer *, struct th_pkt *);   // Append a media packet

  int                    m_fd;         // Socket fd
  int                    m_errors;     // Number of errors
  muxer_container_type_t m_container;  // The type of the container
  const char*            m_mime;       // Mime type for the muxer
} muxer_t;

const char *muxer_container_type2txt(muxer_container_type_t mc);
muxer_container_type_t muxer_container_txt2type(const char *str);

muxer_t *muxer_create(int fd, struct service *s, muxer_container_type_t mc);

#endif
