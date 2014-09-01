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

#include "htsmsg.h"

#define MC_REWRITE_PAT 0x0001
#define MC_REWRITE_PMT 0x0002

#define MC_IS_EOS_ERROR(e) ((e) == EPIPE || (e) == ECONNRESET)

typedef enum {
  MC_UNKNOWN     = 0,
  MC_MATROSKA    = 1,
  MC_MPEGTS      = 2,
  MC_MPEGPS      = 3,
  MC_PASS        = 4,
  MC_RAW         = 5,
  MC_WEBM        = 6,
} muxer_container_type_t;

typedef enum {
  MC_CACHE_UNKNOWN      = 0,
  MC_CACHE_SYSTEM       = 1,
  MC_CACHE_DONTKEEP     = 2,
  MC_CACHE_SYNC         = 3,
  MC_CACHE_SYNCDONTKEEP = 4,
  MC_CACHE_LAST         = MC_CACHE_SYNCDONTKEEP
} muxer_cache_type_t;

/* Muxer configuration used when creating a muxer. */
typedef struct muxer_config {
  int                  m_flags;
  int                  m_cache;

/* 
 * directory_permissions should really be in dvr.h as it's not really needed for the muxer
 * but it's kept with file_permissions for neatness
 */

  int                  m_file_permissions;
  int                  m_directory_permissions; 
} muxer_config_t;

struct muxer;
struct streaming_start;
struct th_pkt;
struct epg_broadcast;
struct service;

typedef struct muxer {
  int         (*m_open_stream)(struct muxer *, int fd);                 // Open for socket streaming
  int         (*m_open_file)  (struct muxer *, const char *filename);   // Open for file storage
  const char* (*m_mime)       (struct muxer *,                          // Figure out the mimetype
			       const struct streaming_start *);
  int         (*m_init)       (struct muxer *,                          // Init The muxer with streams
			       const struct streaming_start *,
			       const char *);
  int         (*m_reconfigure)(struct muxer *,                          // Reconfigure the muxer on
			       const struct streaming_start *);         // stream changes
  int         (*m_close)      (struct muxer *);                         // Close the muxer
  void        (*m_destroy)    (struct muxer *);                         // Free the memory
  int         (*m_write_meta) (struct muxer *, struct epg_broadcast *); // Append epg data
  int         (*m_write_pkt)  (struct muxer *,                          // Append a media packet
			       streaming_message_type_t,
			       void *);
  int         (*m_add_marker) (struct muxer *);                         // Add a marker (or chapter)

  int                    m_eos;        // End of stream
  int                    m_errors;     // Number of errors
  muxer_container_type_t m_container;  // The type of the container
  muxer_config_t         m_config;     // general configuration
} muxer_t;


// type <==> string converters
const char *           muxer_container_type2txt  (muxer_container_type_t mc);
const char*            muxer_container_type2mime (muxer_container_type_t mc, int video);

muxer_container_type_t muxer_container_txt2type  (const char *str);
muxer_container_type_t muxer_container_mime2type (const char *str);

const char*            muxer_container_suffix(muxer_container_type_t mc, int video);

int muxer_container_list(htsmsg_t *array);

// Muxer factory
muxer_t *muxer_create(muxer_container_type_t mc, const muxer_config_t *m_cfg);

// Wrapper functions
int         muxer_open_file   (muxer_t *m, const char *filename);
int         muxer_open_stream (muxer_t *m, int fd);
int         muxer_init        (muxer_t *m, const struct streaming_start *ss, const char *name);
int         muxer_reconfigure (muxer_t *m, const struct streaming_start *ss);
int         muxer_add_marker  (muxer_t *m);
int         muxer_close       (muxer_t *m);
int         muxer_destroy     (muxer_t *m);
int         muxer_write_meta  (muxer_t *m, struct epg_broadcast *eb);
int         muxer_write_pkt   (muxer_t *m, streaming_message_type_t smt, void *data);
const char* muxer_mime        (muxer_t *m, const struct streaming_start *ss);
const char* muxer_suffix      (muxer_t *m, const struct streaming_start *ss);

// Cache
const char *       muxer_cache_type2txt(muxer_cache_type_t t);
muxer_cache_type_t muxer_cache_txt2type(const char *str);
void               muxer_cache_update(muxer_t *m, int fd, off_t off, size_t size);
int                muxer_cache_list(htsmsg_t *array);

#endif
