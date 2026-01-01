/*
 *  tvheadend, Stream Profile
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_PROFILE_H__
#define __TVH_PROFILE_H__

#include "tvheadend.h"
#include "idnode.h"
#include "muxer.h"

typedef enum {
  PROFILE_SPRIO_NOTSET = 0,
  PROFILE_SPRIO_IMPORTANT,
  PROFILE_SPRIO_HIGH,
  PROFILE_SPRIO_NORMAL,
  PROFILE_SPRIO_LOW,
  PROFILE_SPRIO_UNIMPORTANT,
  PROFILE_SPRIO_DVR_IMPORTANT,
  PROFILE_SPRIO_DVR_HIGH,
  PROFILE_SPRIO_DVR_NORMAL,
  PROFILE_SPRIO_DVR_LOW,
  PROFILE_SPRIO_DVR_UNIMPORTANT
} profile_sprio_t;

typedef enum {
  PROFILE_SVF_NONE = 0,
  PROFILE_SVF_SD,
  PROFILE_SVF_HD,
  PROFILE_SVF_FHD,
  PROFILE_SVF_UHD
} profile_svfilter_t;



struct profile;
struct muxer;
struct streaming_target;
struct streaming_start;

extern const idclass_t profile_class;
extern const idclass_t profile_mpegts_pass_class;
extern const idclass_t profile_matroska_class;

TAILQ_HEAD(profile_entry_queue, profile);

extern struct profile_entry_queue profiles;

typedef struct profile *(*profile_builder_t)(void);

typedef struct profile_build {
  LIST_ENTRY(profile_build) link;
  const idclass_t *clazz;
  profile_builder_t build;
} profile_build_t;

typedef LIST_HEAD(, profile_build) profile_builders_queue;

extern profile_builders_queue profile_builders;

typedef struct profile_chain {
  LIST_ENTRY(profile_chain) prch_link;
  int                       prch_linked;

  struct profile_sharer    *prch_sharer;
  LIST_ENTRY(profile_chain) prch_sharer_link;

  struct profile           *prch_pro;
  void                     *prch_id;

  int64_t                   prch_ts_delta;

  int                       prch_flags;
  int                       prch_stop;
  int                       prch_start_pending;
  int                       prch_sq_used;
  struct streaming_queue    prch_sq;
  struct streaming_target  *prch_post_share;
  struct streaming_target  *prch_st;
  struct muxer             *prch_muxer;
  struct streaming_target  *prch_gh;
  struct streaming_target  *prch_tsfix;
#if ENABLE_TIMESHIFT
  struct streaming_target  *prch_timeshift;
#endif
  struct streaming_target   prch_input;
  struct streaming_target  *prch_share;

  int (*prch_can_share)(struct profile_chain *prch,
                        struct profile_chain *joiner);
} profile_chain_t;

typedef struct profile {
  idnode_t pro_id;
  TAILQ_ENTRY(profile) pro_link;

  int pro_refcount;

  LIST_HEAD(,dvr_config) pro_dvr_configs;
  idnode_list_head_t pro_accesses;

  int pro_sflags;
  int pro_enabled;
  int pro_shield;
  char *pro_name;
  char *pro_comment;
  int pro_prio;
  int pro_fprio;
  int pro_timeout;
  int pro_timeout_start;
  int pro_restart;
  int pro_contaccess;
  int pro_ca_timeout;
  int pro_swservice;
  int pro_svfilter;

  void (*pro_free)(struct profile *pro);
  void (*pro_conf_changed)(struct profile *pro);

  int (*pro_work)(profile_chain_t *prch, struct streaming_target *dst,
                  uint32_t timeshift_period);
  int (*pro_reopen)(profile_chain_t *prch, muxer_config_t *m_cfg,
                    muxer_hints_t *hints, int flags);
  int (*pro_open)(profile_chain_t *prch, muxer_config_t *m_cfg,
                  muxer_hints_t *hints, int flags, size_t qsize);
} profile_t;

typedef struct profile_sharer_message {
  TAILQ_ENTRY(profile_sharer_message) psm_link;
  profile_chain_t *psm_prch;
  streaming_message_t *psm_sm;
} profile_sharer_message_t;

typedef struct profile_sharer {
  uint32_t                  prsh_do_queue: 1;
  uint32_t                  prsh_queue_run: 1;
  pthread_t                 prsh_queue_thread;
  tvh_mutex_t               prsh_queue_mutex;
  tvh_cond_t                prsh_queue_cond;
  TAILQ_HEAD(,profile_sharer_message) prsh_queue;
  streaming_target_t        prsh_input;
  LIST_HEAD(,profile_chain) prsh_chains;
  struct profile_chain     *prsh_master;
  struct streaming_start   *prsh_start_msg;
  struct streaming_target  *prsh_tsfix;
#if ENABLE_LIBAV
  struct streaming_target  *prsh_transcoder;
#endif
} profile_sharer_t;

void profile_register(const idclass_t *clazz, profile_builder_t builder);

profile_t *profile_create
  (const char *uuid, htsmsg_t *conf, int save);

static inline void profile_grab( profile_t *pro )
  { pro->pro_refcount++; }

void profile_release_( profile_t *pro );
static inline void profile_release( profile_t *pro )
  {
    assert(pro->pro_refcount > 0);
    if (--pro->pro_refcount == 0) profile_release_(pro);
  }

int profile_chain_work(profile_chain_t *prch, struct streaming_target *dst,
                       uint32_t timeshift_period);
int profile_chain_reopen(profile_chain_t *prch,
                         muxer_config_t *m_cfg,
                         muxer_hints_t *hints, int flags);
int profile_chain_open(profile_chain_t *prch,
                       muxer_config_t *m_cfg,
                       muxer_hints_t *hints,
                       int flags, size_t qsize);
void profile_chain_init(profile_chain_t *prch, profile_t *pro, void *id, int queue);
int  profile_chain_raw_open(profile_chain_t *prch, void *id, size_t qsize, int muxer);
void profile_chain_close(profile_chain_t *prch);
int  profile_chain_weight(profile_chain_t *prch, int custom);

static inline profile_t *profile_find_by_uuid(const char *uuid)
  {  return (profile_t*)idnode_find(uuid, &profile_class, NULL); }
profile_t *profile_find_by_name(const char *name, const char *alt);
profile_t *profile_find_by_list(htsmsg_t *uuids, const char *name,
                                const char *alt, int sflags);
int profile_verify(profile_t *pro, int sflags);

htsmsg_t * profile_class_get_list(void *o, const char *lang);

char *profile_validate_name(const char *name);

const char *profile_get_name(profile_t *pro);

void profile_get_htsp_list(htsmsg_t *array, htsmsg_t *filter);

void profile_init(void);
void profile_done(void);

#endif /* __TVH_PROFILE_H__ */
