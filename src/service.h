/*
 *  Tvheadend
 *  Copyright (C) 2010 Andreas Öman
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

#ifndef SERVICE_H__
#define SERVICE_H__

#include "htsmsg.h"
#include "idnode.h"
#include "profile.h"
#include "descrambler.h"

extern const idclass_t service_class;
extern const idclass_t service_raw_class;

extern struct service_queue service_all;
extern struct service_queue service_raw_all;
extern struct service_queue service_raw_remove;

struct channel;
struct tvh_input;
struct mpegts_apids;

/**
 * Source information
 */
typedef struct source_info {
  tvh_uuid_t si_adapter_uuid;
  tvh_uuid_t si_network_uuid;
  tvh_uuid_t si_mux_uuid;
  char *si_adapter;
  char *si_network;
  char *si_network_type;
  char *si_satpos;
  char *si_mux;
  char *si_provider;
  char *si_service;
  int   si_type;
} source_info_t;

/**
 * Stream, one media component for a service.
 */
typedef struct elementary_stream {

  TAILQ_ENTRY(elementary_stream) es_link;
  TAILQ_ENTRY(elementary_stream) es_filt_link;
  int es_position;
  struct service *es_service;

  streaming_component_type_t es_type;
  int es_index;

  uint16_t es_aspect_num;
  uint16_t es_aspect_den;

  char es_lang[4];           /* ISO 639 2B 3-letter language code */
  uint8_t es_audio_type;     /* Audio type */

  uint16_t es_composition_id;
  uint16_t es_ancillary_id;

  int16_t es_pid;
  uint16_t es_parent_pid;    /* For subtitle streams originating from 
				a teletext stream. this is the pid
				of the teletext stream */
  int8_t es_pid_opened;      /* PID is opened */

  int8_t es_cc;             /* Last CC */

  int es_peak_presentation_delay; /* Max seen diff. of DTS and PTS */

  /* For service stream packet reassembly */

  sbuf_t es_buf;

  uint8_t  es_incomplete;
  uint8_t  es_header_mode;
  uint32_t es_header_offset;
  uint32_t es_startcond;
  uint32_t es_startcode;
  uint32_t es_startcode_offset;
  int es_parser_state;
  int es_parser_ptr;
  void *es_priv;          /* Parser private data */

  sbuf_t es_buf_a;        // Audio packet reassembly

  uint8_t *es_global_data;
  int es_global_data_len;

  struct th_pkt *es_curpkt;
  struct streaming_message_queue es_backlog;
  int64_t es_curpts;
  int64_t es_curdts;
  int64_t es_prevdts;
  int64_t es_nextdts;
  int es_frame_duration;
  int es_width;
  int es_height;

  int es_meta_change;

  /* CA ID's on this stream */
  struct caid_list es_caids;

  /* */

  int es_delete_me;      /* Temporary flag for deleting streams */

  /* Error log limiters */

  tvhlog_limit_t es_cc_log;
  tvhlog_limit_t es_pes_log;
  
  char *es_nicename;

  /* Teletext subtitle */ 
  char es_blank; // Last subtitle was blank

  /* SI section processing (horrible hack) */
  void *es_section;

  /* Filter temporary variable */
  uint32_t es_filter;

} elementary_stream_t;


typedef TAILQ_HEAD(service_instance_list, service_instance) service_instance_list_t;

/**
 *
 */
typedef struct service_instance {

  TAILQ_ENTRY(service_instance) si_link;

  int si_prio;

  struct service *si_s; // A reference is held
  int si_instance;       // Discriminator when having multiple adapters, etc

  int si_error;        /* Set if subscription layer deem this cand
                          to be broken. We typically set this if we
                          have not seen any demuxed packets after
                          the grace period has expired.
                          The actual value is current time
                       */

  time_t si_error_time;


  int si_weight;         // Highest weight that holds this cand

  int si_mark;           // For mark & sweep

  char si_source[128];

} service_instance_t;


/**
 *
 */
service_instance_t *service_instance_add(service_instance_list_t *sil,
                                         struct service *s,
                                         int instance,
                                         const char *source,
                                         int prio,
                                         int weight);

void service_instance_destroy
  (service_instance_list_t *sil, service_instance_t *si);

void service_instance_list_clear(service_instance_list_t *sil);

/**
 *
 */
typedef struct service_lcn {
  LIST_ENTRY(service_lcn) sl_link;
  void     *sl_bouquet;
  uint64_t  sl_lcn;
  uint8_t   sl_seen;
} service_lcn_t;


/**
 *
 */
#define SERVICE_AUTO_NORMAL       0
#define SERVICE_AUTO_OFF          1
#define SERVICE_AUTO_PAT_MISSING  2

/**
 *
 */
typedef struct service {
  idnode_t s_id;

  TAILQ_ENTRY(service) s_all_link;

  enum {
    /**
     * Transport is idle.
     */
    SERVICE_IDLE,

    /**
     * Transport producing output
     */
    SERVICE_RUNNING,

    /**
     * Destroyed, but pointer is till valid. 
     * This would be the case if transport_destroy() did not actually free 
     * the transport because there are references held to it.
     *
     * Reference counts can be used so that code can hold a pointer to 
     * a transport without having the global lock.
     *
     * Note: No fields in the transport may be accessed without the
     * global lock held. Thus, the global_lock must be reaquired and
     * then s_status must be checked. If it is ZOMBIE the code must
     * just drop the refcount and pretend that the transport never
     * was there in the first place.
     */
    SERVICE_ZOMBIE, 
  } s_status;

  /**
   * Refcount, operated using atomic.h ops.
   */ 
  int s_refcount;

  /**
   * Service type, standard or raw (for mux or partial mux streaming)
   */
  enum {
    STYPE_STD,
    STYPE_RAW,
    STYPE_RAW_REMOVED
  } s_type;

  /**
   * Source type is used to determine if an output requesting
   * MPEG-TS can shortcut all the parsing and remuxing.
   */ 
  enum {
    S_MPEG_TS,
    S_MPEG_PS,
    S_OTHER,
  } s_source_type;

  /**
   * Service type
   */
  enum {
    ST_UNSET = -1,
    ST_NONE = 0,
    ST_OTHER,
    ST_SDTV,
    ST_HDTV,
    ST_UHDTV,
    ST_RADIO
  } s_servicetype;

// TODO: should this really be here?

  /**
   * PID carrying the programs PCR.
   * XXX: We don't support transports that does not carry
   * the PCR in one of the content streams.
   */
  uint16_t s_pcr_pid;

  /**
   * PID for the PMT of this MPEG-TS stream.
   */
  uint16_t s_pmt_pid;

  /**
   * Set if transport is enabled (the default).  If disabled it should
   * not be considered when chasing for available transports during
   * subscription scheduling.
   */
  int s_enabled;
  int s_auto;
  int s_prio;
  int s_type_user;

  LIST_ENTRY(service) s_active_link;

  LIST_HEAD(, th_subscription) s_subscriptions;

  int (*s_is_enabled)(struct service *t, int flags);

  int (*s_enlist)(struct service *s, struct tvh_input *ti,
                  service_instance_list_t *sil, int flags, int weight);

  int (*s_start_feed)(struct service *s, int instance, int weight, int flags);

  void (*s_refresh_feed)(struct service *t);

  void (*s_stop_feed)(struct service *t);

  htsmsg_t *(*s_config_save)(struct service *t, char *filename, size_t fsize);

  void (*s_setsourceinfo)(struct service *t, struct source_info *si);

  int (*s_grace_period)(struct service *t);

  void (*s_delete)(struct service *t, int delconf);

  void (*s_unref)(struct service *t);

  int (*s_satip_source)(struct service *t);

  void (*s_memoryinfo)(struct service *t, int64_t *size);

  int (*s_unseen)(struct service *t, const char *type, time_t before);

  /**
   * Channel info
   */
  int64_t     (*s_channel_number) (struct service *);
  const char *(*s_channel_name)   (struct service *);
  const char *(*s_channel_epgid)  (struct service *);
  htsmsg_t   *(*s_channel_tags)   (struct service *);
  const char *(*s_provider_name)  (struct service *);
  const char *(*s_channel_icon)   (struct service *);
  void        (*s_mapped)         (struct service *);

  /**
   * Name usable for displaying to user
   */
  char *s_nicename;
  int   s_nicename_prefidx;

  /**
   * Teletext...
   */
  th_commercial_advice_t s_tt_commercial_advice;
  time_t s_tt_clock;   /* Network clock as determined by teletext decoder */
 
  /**
   * Channel mapping
   */
  idnode_list_head_t s_channels;

  /**
   * Service mapping, see service_mapper.c form details
   */
  int s_sm_onqueue;

  /**
   * Pending save.
   *
   * transport_request_save() will enqueue the transport here.
   * We need to do this if we don't hold the global lock.
   * This happens when we update PMT from within the TS stream itself.
   * Then we hold the stream mutex, and thus, can not obtain the global lock
   * as it would cause lock inversion.
   */
  int s_ps_onqueue;
  TAILQ_ENTRY(service) s_ps_link;

  /**
   * Timer which is armed at transport start. Once it fires
   * it will check if any packets has been parsed. If not the status
   * will be set to TRANSPORT_STATUS_NO_INPUT
   */
  mtimer_t s_receive_timer;
  /**
   * Stream start time
   */
  int     s_timeout;
  int     s_grace_delay;
  int64_t s_start_time;


  /*********************************************************
   *
   * Streaming part of transport
   *
   * All access to fields below this must be protected with
   * s_stream_mutex held.
   *
   * Note: Code holding s_stream_mutex should _never_ 
   * acquire global_lock while already holding s_stream_mutex.
   *
   */

  /**
   * Mutex to be held during streaming.
   * This mutex also protects all elementary_stream_t instances for this
   * transport.
   */
  pthread_mutex_t s_stream_mutex;

  /**
   *
   */			   
  int s_streaming_status;

  // Progress
#define TSS_INPUT_HARDWARE   0x1
#define TSS_INPUT_SERVICE    0x2
#define TSS_MUX_PACKETS      0x4
#define TSS_PACKETS          0x8
#define TSS_NO_ACCESS        0x10


  // Errors
#define TSS_GRACEPERIOD      0x10000
#define TSS_NO_DESCRAMBLER   0x20000
#define TSS_TIMEOUT          0x40000
#define TSS_TUNING           0x80000

#define TSS_ERRORS           0xffff0000


  /**
   *
   */
  int s_streaming_live;
  int s_running;

  // Live status
#define TSS_LIVE             0x01

  /**
   * For simple streaming sources (such as video4linux) keeping
   * track of the video and audio stream is convenient.
   */
#ifdef MOVE_TO_V4L
  elementary_stream_t *s_video;
  elementary_stream_t *s_audio;
#endif
 
  /**
   * Descrambling support
   */

  struct th_descrambler_list s_descramblers;
  uint8_t s_scrambled_seen;
  uint8_t s_scrambled_pass;
  th_descrambler_runtime_t *s_descramble;
  void *s_descrambler; /* last active descrambler */
  descramble_info_t *s_descramble_info;

  /**
   * List of all and filtered components.
   */
  struct elementary_stream_queue s_components;
  struct elementary_stream_queue s_filt_components;
  int s_last_pid;
  elementary_stream_t *s_last_es;

  /**
   * Delivery pad, this is were we finally deliver all streaming output
   */
  streaming_pad_t s_streaming_pad;

  tvhlog_limit_t s_tei_log;

  int64_t s_current_pts;

  /*
   * Local channel numbers per bouquet
   */
  LIST_HEAD(,service_lcn) s_lcns;

} service_t;





void service_init(void);
void service_done(void);

int service_start(service_t *t, int instance, int weight, int flags,
                  int timeout, int postpone);
void service_stop(service_t *t);

void service_build_filter(service_t *t);

service_t *service_create0(service_t *t, int service_type, const idclass_t *idc,
                           const char *uuid, int source_type, htsmsg_t *conf);

#define service_create(t, y, c, u, s, m)\
  (struct t*)service_create0(calloc(1, sizeof(struct t), y, &t##_class, c, u, s, m)

void service_unref(service_t *t);

void service_ref(service_t *t);

static inline service_t *service_find(const char *identifier)
  { return idnode_find(identifier, &service_class, NULL); }
#define service_find_by_identifier service_find

service_instance_t *service_find_instance(struct service *s,
                                          struct channel *ch,
                                          struct tvh_input *source,
                                          profile_chain_t *prch,
                                          service_instance_list_t *sil,
                                          int *error, int weight,
                                          int flags, int timeout,
                                          int postpone);

elementary_stream_t *service_stream_find_(service_t *t, int pid);

static inline elementary_stream_t *
service_stream_find(service_t *t, int pid)
{
  if (t->s_last_pid != (pid))
    return service_stream_find_(t, pid);
  else
    return t->s_last_es;
}

elementary_stream_t *service_stream_create(service_t *t, int pid,
				     streaming_component_type_t type);

void service_settings_write(service_t *t);

const char *service_servicetype_txt(service_t *t);

int service_has_audio_or_video(service_t *t);
int service_is_sdtv(service_t *t);
int service_is_uhdtv(service_t *t);
int service_is_hdtv(service_t *t);
int service_is_radio(service_t *t);
int service_is_other(service_t *t);
#define service_is_tv(s) (service_is_hdtv(s) || service_is_sdtv(s) || service_is_uhdtv(s))

int service_is_encrypted ( service_t *t );

void service_set_enabled ( service_t *t, int enabled, int _auto );

void service_destroy(service_t *t, int delconf);

void service_remove_raw(service_t *);

void service_remove_subscriber(service_t *t, struct th_subscription *s,
			       int reason);


void service_send_streaming_status(service_t *t);

void service_set_streaming_status_flags_(service_t *t, int flag);

static inline void
service_set_streaming_status_flags(service_t *t, int flag)
{
  int n = t->s_streaming_status;
  if ((n & flag) != flag)
    service_set_streaming_status_flags_(t, n | flag);
}

static inline void
service_reset_streaming_status_flags(service_t *t, int flag)
{
  int n = t->s_streaming_status;
  if ((n & flag) != 0)
    service_set_streaming_status_flags_(t, n & ~flag);
}


struct streaming_start;
struct streaming_start *service_build_stream_start(service_t *t);

void service_restart(service_t *t);

void service_stream_destroy(service_t *t, elementary_stream_t *st);

void service_request_save(service_t *t, int restart);

void service_source_info_free(source_info_t *si);

void service_source_info_copy(source_info_t *dst, const source_info_t *src);

void service_make_nicename(service_t *t);

const char *service_nicename(service_t *t);

const char *service_component_nicename(elementary_stream_t *st);

const char *service_adapter_nicename(service_t *t, char *buf, size_t len);

const char *service_tss2text(int flags);

static inline int service_tss_is_error(int flags)
{
  return flags & TSS_ERRORS ? 1 : 0;
}

void service_refresh_channel(service_t *t);

int tss2errcode(int tss);

htsmsg_t *servicetype_list (void);

void service_load ( service_t *s, htsmsg_t *c );

void service_save ( service_t *s, htsmsg_t *c );

void service_remove_unseen(const char *type, int days);

void sort_elementary_streams(service_t *t);

const char *service_get_channel_name (service_t *s);
const char *service_get_full_channel_name (service_t *s);
int64_t     service_get_channel_number (service_t *s);
const char *service_get_channel_icon (service_t *s);
const char *service_get_channel_epgid (service_t *s);

void service_memoryinfo (service_t *s, int64_t *size);

void service_mapped (service_t *s);

#endif // SERVICE_H__
