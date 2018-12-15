/*
 *  Tvheadend
 *  Copyright (C) 2010 Andreas Öman
 *                2018 Jaroslav Kysela
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ESSTREAM_H__
#define ESSTREAM_H__

#include "descrambler/descrambler.h"
#include "input/mpegts/dvb.h"

/**
 *
 */
typedef struct service service_t;
typedef struct streaming_start streaming_start_t;

typedef enum streaming_component_type streaming_component_type_t;
typedef struct elementary_info elementary_info_t;
typedef struct elementary_stream elementary_stream_t;
typedef struct elementary_set elementary_set_t;

/**
 * Stream component types
 */
enum streaming_component_type {
  SCT_NONE = -1,
  SCT_UNKNOWN = 0,
  SCT_RAW = 1,
  SCT_PCR,     /* MPEG-TS PCR data */
  SCT_CAT,     /* MPEG-TS CAT (EMM) data */
  SCT_CA,      /* MPEG-TS ECM data */
  SCT_HBBTV,   /* HBBTV info */
  /* standard codecs */
  SCT_MPEG2VIDEO,
  SCT_MPEG2AUDIO,
  SCT_H264,
  SCT_AC3,
  SCT_TELETEXT,
  SCT_DVBSUB,
  SCT_AAC,     /* AAC-LATM in MPEG-TS, ADTS + AAC in packet form */
  SCT_MPEGTS,
  SCT_TEXTSUB,
  SCT_EAC3,
  SCT_MP4A,    /* ADTS + AAC in MPEG-TS and packet form */
  SCT_VP8,
  SCT_VORBIS,
  SCT_HEVC,
  SCT_VP9,
  SCT_THEORA,
  SCT_OPUS,
  SCT_FLAC,
  SCT_LAST = SCT_FLAC
};

#define SCT_MASK(t) (1 << (t))

#define SCT_ISVIDEO(t) ((t) == SCT_MPEG2VIDEO || (t) == SCT_H264 || \
			(t) == SCT_VP8 || (t) == SCT_HEVC || \
			(t) == SCT_VP9 || (t) == SCT_THEORA)

#define SCT_ISAUDIO(t) ((t) == SCT_MPEG2AUDIO || (t) == SCT_AC3 || \
			(t) == SCT_AAC  || (t) == SCT_MP4A || \
			(t) == SCT_EAC3 || (t) == SCT_VORBIS || \
			(t) == SCT_OPUS || (t) == SCT_FLAC)

#define SCT_ISAV(t) (SCT_ISVIDEO(t) || SCT_ISAUDIO(t))

#define SCT_ISSUBTITLE(t) ((t) == SCT_TEXTSUB || (t) == SCT_DVBSUB)

/**
 * Stream info, one media component for a service.
 */
struct elementary_info {
  int es_index;
  int16_t es_pid;
  int es_type;

  int es_frame_duration;

  int es_width;
  int es_height;

  uint16_t es_aspect_num;
  uint16_t es_aspect_den;

  char es_lang[4];           /* ISO 639 2B 3-letter language code */
  uint8_t es_audio_type;     /* Audio type */
  uint8_t es_audio_version;  /* Audio version/layer */
  uint8_t es_sri;
  uint8_t es_ext_sri;
  uint16_t es_channels;

  uint16_t es_composition_id;
  uint16_t es_ancillary_id;

  uint16_t es_parent_pid;    /* For subtitle streams originating from
				a teletext stream. this is the pid
				of the teletext stream */
};

/**
 * Stream, one media component for a service.
 */
struct elementary_stream {
  elementary_info_t;

  TAILQ_ENTRY(elementary_stream) es_link;
  TAILQ_ENTRY(elementary_stream) es_filter_link;

  uint32_t es_position;
  struct service *es_service;
  char *es_nicename;

  /* PID related */
  int8_t es_pid_opened;      /* PID is opened */
  int8_t es_cc;              /* Last CC */
  int8_t es_delete_me;       /* Temporary flag for deleting streams */

  struct caid_list es_caids; /* CA ID's on this stream */

  tvhlog_limit_t es_cc_log;  /* CC error log limiter */
  uint32_t es_filter;        /* Filter temporary variable */
};

/*
 * Group of elementary streams
 */
struct elementary_set {
  TAILQ_HEAD(, elementary_stream) set_all;
  TAILQ_HEAD(, elementary_stream) set_filter;

  uint16_t set_pmt_pid;      /* PMT PID number */
  uint16_t set_pcr_pid;      /* PCR PID number */
  uint16_t set_service_id;   /* MPEG-TS DVB service ID number */

  int set_subsys;
  char *set_nicename;
  service_t *set_service;

  /* Cache lookups */
  uint16_t set_last_pid;
  elementary_stream_t *set_last_es;
};

/*
 * Prototypes
 */
void elementary_set_init
  (elementary_set_t *set, int subsys, const char *nicename, service_t *t);
void elementary_set_clean(elementary_set_t *set, service_t *t, int keep_nicename);
void elementary_set_update_nicename(elementary_set_t *set, const char *nicename);
void elementary_set_clean_streams(elementary_set_t *set);
void elementary_set_stream_destroy(elementary_set_t *set, elementary_stream_t *es);
void elementary_set_init_filter_streams(elementary_set_t *set);
void elementary_set_filter_build(elementary_set_t *set);
elementary_stream_t *elementary_stream_create_parent
  (elementary_set_t *set, int pid, int parent_pid, streaming_component_type_t type);
static inline elementary_stream_t *elementary_stream_create
  (elementary_set_t *set, int pid, streaming_component_type_t type)
{ return elementary_stream_create_parent(set, pid, -1, type); }
elementary_stream_t *elementary_stream_find_(elementary_set_t *set, int pid);
elementary_stream_t *elementary_stream_type_find
  (elementary_set_t *set, streaming_component_type_t type);
static inline elementary_stream_t *elementary_stream_find
  (elementary_set_t *set, int pid)
  {
    if (set->set_last_pid != (pid))
      return elementary_stream_find_(set, pid);
    else
      return set->set_last_es;
  }
elementary_stream_t *elementary_stream_find_parent(elementary_set_t *set, int pid, int parent_pid);
elementary_stream_t *elementary_stream_type_modify
  (elementary_set_t *set, int pid, streaming_component_type_t type);
void elementary_stream_type_destroy
  (elementary_set_t *set, streaming_component_type_t type);
int elementary_stream_has_audio_or_video(elementary_set_t *set);
int elementary_stream_has_no_audio(elementary_set_t *set, int filtered);
int elementary_set_has_streams(elementary_set_t *set, int filtered);
void elementary_set_sort_streams(elementary_set_t *set);
streaming_start_t *elementary_stream_build_start(elementary_set_t *set);
elementary_set_t *elementary_stream_create_from_start
  (elementary_set_t *set, streaming_start_t *ss, size_t es_size);


#endif // ESSTREAM_H__
