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

struct profile;
struct muxer;
struct streaming_target;

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
  int                      prch_flags;
  struct streaming_queue   prch_sq;
  struct streaming_target *prch_st;
  struct muxer            *prch_muxer;
  struct streaming_target *prch_gh;
  struct streaming_target *prch_tsfix;
} profile_chain_t;

typedef struct profile {
  idnode_t pro_id;
  TAILQ_ENTRY(profile) pro_link;

  LIST_HEAD(,dvr_config) pro_dvr_configs;

  int pro_enabled;
  int pro_shield;
  char *pro_name;
  char *pro_comment;

  void (*pro_free)(struct profile *cac);
  void (*pro_conf_changed)(struct profile *cac);
  muxer_container_type_t (*pro_get_mc)(struct profile *cac);

  int  (*pro_open)(struct profile *cac, profile_chain_t *prch,
                   muxer_config_t *m_cfg, size_t qsize);
} profile_t;

void profile_register(const idclass_t *clazz, profile_builder_t builder);

profile_t *profile_create
  (const char *uuid, htsmsg_t *conf, int save);

int  profile_chain_raw_open(profile_chain_t *prch, size_t qsize);
void profile_chain_close(profile_chain_t *prch);

static inline profile_t *profile_find_by_uuid(const char *uuid)
  {  return (profile_t*)idnode_find(uuid, &profile_class, NULL); }
profile_t *profile_find_by_name(const char *name);

htsmsg_t * profile_class_get_list(void *o);

char *profile_validate_name(const char *name);

const char *profile_get_name(profile_t *pro);

static inline muxer_container_type_t profile_get_mc(profile_t *pro)
  { return pro->pro_get_mc(pro); }

void profile_init(void);
void profile_done(void);

#endif /* __TVH_PROFILE_H__ */
