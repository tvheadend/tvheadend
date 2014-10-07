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

#include "tvheadend.h"
#include "settings.h"
#include "profile.h"
#include "streaming.h"
#include "plumbing/tsfix.h"
#include "plumbing/globalheaders.h"
#include "dvr/dvr.h"

profile_builders_queue profile_builders;

struct profile_entry_queue profiles;

static profile_t *profile_default;

static void profile_class_save ( idnode_t *in );

/*
 *
 */

void
profile_register(const idclass_t *clazz, profile_builder_t builder)
{
  profile_build_t *pb = calloc(1, sizeof(*pb));
  pb->clazz = clazz;
  pb->build = builder;
  LIST_INSERT_HEAD(&profile_builders, pb, link);
}

static profile_build_t *
profile_class_find(const char *name)
{
  profile_build_t *pb;
  LIST_FOREACH(pb, &profile_builders, link) {
    if (strcmp(pb->clazz->ic_class, name) == 0)
      return pb;
  }
  return NULL;
}

profile_t *
profile_create
  (const char *uuid, htsmsg_t *conf, int save)
{
  profile_t *pro = NULL;
  profile_build_t *pb = NULL;
  const char *s;

  lock_assert(&global_lock);

  if ((s = htsmsg_get_str(conf, "class")) != NULL)
    pb = profile_class_find(s);
  if (pb == NULL) {
    tvherror("profile", "wrong class %s!", s);
    abort();
  }
  pro = pb->build();
  if (pro == NULL) {
    tvherror("profile", "Profile class %s is not available!", s);
    return NULL;
  }
  LIST_INIT(&pro->pro_dvr_configs);
  if (idnode_insert(&pro->pro_id, uuid, pb->clazz, 0)) {
    if (uuid)
      tvherror("profile", "invalid uuid '%s'", uuid);
    free(pro);
    return NULL;
  }
  if (conf) {
    int b;
    idnode_load(&pro->pro_id, conf);
    if (!htsmsg_get_bool(conf, "shield", &b))
      pro->pro_shield = !!b;
  }
  TAILQ_INSERT_TAIL(&profiles, pro, pro_link);
  if (save)
    profile_class_save((idnode_t *)pro);
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
  return pro;
}

static void
profile_delete(profile_t *pro, int delconf)
{
  pro->pro_enabled = 0;
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
  if (delconf)
    hts_settings_remove("profile/%s", idnode_uuid_as_str(&pro->pro_id));
  TAILQ_REMOVE(&profiles, pro, pro_link);
  idnode_unlink(&pro->pro_id);
  dvr_config_destroy_by_profile(pro, delconf);
  if (pro->pro_free)
    pro->pro_free(pro);
  free(pro->pro_name);
  free(pro->pro_comment);
  free(pro);
}

static void
profile_class_save ( idnode_t *in )
{
  profile_t *pro = (profile_t *)in;
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(in, c);
  if (pro->pro_shield)
    htsmsg_add_bool(c, "shield", 1);
  hts_settings_save(c, "profile/%s", idnode_uuid_as_str(in));
  htsmsg_destroy(c);
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
}

static const char *
profile_class_get_title ( idnode_t *in )
{
  profile_t *pro = (profile_t *)in;
  static char buf[32];
  if (pro->pro_name && pro->pro_name[0])
    return pro->pro_name;
  snprintf(buf, sizeof(buf), "%s", in->in_class->ic_caption);
  return buf;
}

static void
profile_class_delete(idnode_t *self)
{
  profile_t *pro = (profile_t *)self;
  if (pro->pro_shield)
    return;
  profile_delete(pro, 1);
}

static const void *
profile_class_class_get(void *o)
{
  profile_t *pro = o;
  static const char *ret;
  ret = pro->pro_id.in_class->ic_class;
  return &ret;
}

static int
profile_class_class_set(void *o, const void *v)
{
  /* just ignore, create fcn does the right job */
  return 0;
}

static const void *
profile_class_default_get(void *o)
{
  static int res;
  res = o == profile_default;
  return &res;
}

static int
profile_class_default_set(void *o, const void *v)
{
  profile_t *pro = o, *old;
  if (*(int *)v && pro != profile_default) {
    old = profile_default;
    profile_default = pro;
    if (old)
      profile_class_save(&old->pro_id);
    return 1;
  }
  return 0;
}

const idclass_t profile_class =
{
  .ic_class      = "profile",
  .ic_caption    = "Stream Profile",
  .ic_save       = profile_class_save,
  .ic_event      = "profile",
  .ic_get_title  = profile_class_get_title,
  .ic_delete     = profile_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "class",
      .name     = "Class",
      .opts     = PO_RDONLY | PO_HIDDEN,
      .get      = profile_class_class_get,
      .set      = profile_class_class_set,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(profile_t, pro_enabled),
    },
    {
      .type     = PT_BOOL,
      .id       = "default",
      .name     = "Default",
      .set      = profile_class_default_set,
      .get      = profile_class_default_get,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Profile Name",
      .off      = offsetof(profile_t, pro_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
      .off      = offsetof(profile_t, pro_comment),
    },
    { }
  }
};

/*
 *
 */
const char *
profile_get_name(profile_t *pro)
{
  if (pro->pro_name && *pro->pro_name) return pro->pro_name;
  return "";
}

/*
 *
 */
profile_t *
profile_find_by_name(const char *name)
{
  profile_t *pro;

  lock_assert(&global_lock);

  if (!name)
    return profile_default;

  TAILQ_FOREACH(pro, &profiles, pro_link) {
    if (!strcmp(pro->pro_name, name))
      return pro;
  }

  return profile_default;
}

/*
 *
 */
char *
profile_validate_name(const char *name)
{
  profile_t *pro;

  lock_assert(&global_lock);

  TAILQ_FOREACH(pro, &profiles, pro_link) {
    if (name && !strcmp(pro->pro_name, name))
      return strdup(name);
  }

  if (profile_default)
    return strdup(profile_default->pro_name);

  return NULL;
}

/*
 *
 */
htsmsg_t *
profile_class_get_list(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "profile/list");
  htsmsg_add_str(m, "event", "profile");
  return m;
}

/*
 *
 */
int
profile_chain_raw_open(profile_chain_t *prch, size_t qsize)
{
  muxer_config_t c;

  memset(&c, 0, sizeof(c));
  c.m_type = MC_RAW;
  memset(prch, 0, sizeof(*prch));
  prch->prch_flags = SUBSCRIPTION_RAW_MPEGTS;
  streaming_queue_init(&prch->prch_sq, SMT_PACKET, qsize);
  prch->prch_st    = &prch->prch_sq.sq_st;
  prch->prch_muxer = muxer_create(&c);
  return 0;
}

/*
 *
 */
void
profile_chain_close(profile_chain_t *prch)
{
  if (prch->prch_tsfix)
    tsfix_destroy(prch->prch_tsfix);
  if (prch->prch_gh)
    globalheaders_destroy(prch->prch_gh);
  if (prch->prch_muxer)
    muxer_destroy(prch->prch_muxer);
  streaming_queue_deinit(&prch->prch_sq);
}

/*
 *  MPEG-TS passthrough muxer
 */
typedef struct profile_mpegts {
  profile_t;
  int pro_rewrite_pmt;
  int pro_rewrite_pat;
} profile_mpegts_t;

const idclass_t profile_mpegts_pass_class =
{
  .ic_super     = &profile_class,
  .ic_class      = "profile-mpegts",
  .ic_caption    = "MPEG-TS Pass-through",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pmt",
      .name     = "Rewrite PMT",
      .off      = offsetof(profile_mpegts_t, pro_rewrite_pmt),
      .def.i    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pat",
      .name     = "Rewrite PAT",
      .off      = offsetof(profile_mpegts_t, pro_rewrite_pat),
      .def.i    = 1,
    },
    { }
  }
};

static int
profile_mpegts_pass_open(profile_t *_pro, profile_chain_t *prch,
                         muxer_config_t *m_cfg, size_t qsize)
{
  profile_mpegts_t *pro = (profile_mpegts_t *)_pro;
  muxer_config_t c;

  if (m_cfg)
    c = *m_cfg; /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_RAW)
    c.m_type = MC_PASS;
  c.m_rewrite_pat = pro->pro_rewrite_pat;
  c.m_rewrite_pmt = pro->pro_rewrite_pmt;

  memset(prch, 0, sizeof(*prch));
  prch->prch_flags = SUBSCRIPTION_RAW_MPEGTS;
  streaming_queue_init(&prch->prch_sq, SMT_PACKET, qsize);
  prch->prch_muxer = muxer_create(&c);
  prch->prch_st    = &prch->prch_sq.sq_st;
  return 0;
}

static muxer_container_type_t
profile_mpegts_pass_get_mc(profile_t *_pro)
{
  return MC_PASS;
}

static profile_t *
profile_mpegts_pass_builder(void)
{
  profile_mpegts_t *pro = calloc(1, sizeof(*pro));
  pro->pro_open   = profile_mpegts_pass_open;
  pro->pro_get_mc = profile_mpegts_pass_get_mc;
  return (profile_t *)pro;
}

/*
 *  Matroska muxer
 */
typedef struct profile_matroska {
  profile_t;
  int pro_webm;
} profile_matroska_t;

const idclass_t profile_matroska_class =
{
  .ic_super     = &profile_class,
  .ic_class      = "profile-matroska",
  .ic_caption    = "Matroska (mkv)",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "webm",
      .name     = "WEBM",
      .off      = offsetof(profile_matroska_t, pro_webm),
      .def.i    = 0,
    },
    { }
  }
};

static int
profile_matroska_open(profile_t *_pro, profile_chain_t *prch,
                      muxer_config_t *m_cfg, size_t qsize)
{
  profile_matroska_t *pro = (profile_matroska_t *)_pro;
  muxer_config_t c;

  if (m_cfg)
    c = *m_cfg; /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_WEBM)
    c.m_type = MC_MATROSKA;
  if (pro->pro_webm)
    c.m_type = MC_WEBM;

  memset(prch, 0, sizeof(*prch));
  streaming_queue_init(&prch->prch_sq, 0, qsize);
  prch->prch_gh = globalheaders_create(&prch->prch_sq.sq_st);
  prch->prch_tsfix = tsfix_create(prch->prch_gh);
  prch->prch_muxer = muxer_create(&c);
  prch->prch_st    = prch->prch_tsfix;
  return 0;
}

static muxer_container_type_t
profile_matroska_get_mc(profile_t *_pro)
{
  profile_matroska_t *pro = (profile_matroska_t *)_pro;
  if (pro->pro_webm)
    return MC_WEBM;
  return MC_MATROSKA;
}

static profile_t *
profile_matroska_builder(void)
{
  profile_matroska_t *pro = calloc(1, sizeof(*pro));
  pro->pro_open   = profile_matroska_open;
  pro->pro_get_mc = profile_matroska_get_mc;
  return (profile_t *)pro;
}

/*
 *  Initialize
 */
void
profile_init(void)
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  LIST_INIT(&profile_builders);
  TAILQ_INIT(&profiles);

  profile_register(&profile_mpegts_pass_class, profile_mpegts_pass_builder);
  profile_register(&profile_matroska_class, profile_matroska_builder);

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))
        continue;
      (void)profile_create(f->hmf_name, e, 0);
    }
    htsmsg_destroy(c);
  }

  if (TAILQ_EMPTY(&profiles)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-mpegts");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_bool(conf, "default", 1);
    htsmsg_add_str (conf, "name", "pass");
    htsmsg_add_str (conf, "comment", "MPEG-TS Pass-through");
    htsmsg_add_bool(conf, "rewrite_pmt", 1);
    htsmsg_add_bool(conf, "rewrite_pat", 1);
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-matroska");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", "matroska");
    htsmsg_add_str (conf, "comment", "Matroska");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }
}

void
profile_done(void)
{
  profile_t *pro;
  profile_build_t *pb;

  pthread_mutex_lock(&global_lock);
  profile_default = NULL;
  while ((pro = TAILQ_FIRST(&profiles)) != NULL)
    profile_delete(pro, 0);
  while ((pb = LIST_FIRST(&profile_builders)) != NULL) {
    LIST_REMOVE(pb, link);
    free(pb);
  }
  pthread_mutex_unlock(&global_lock);
}
