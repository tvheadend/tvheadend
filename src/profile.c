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
#include "access.h"
#include "plumbing/tsfix.h"
#include "plumbing/globalheaders.h"
#if ENABLE_LIBAV
#include "lang_codes.h"
#include "plumbing/transcoding.h"
#endif
#if ENABLE_TIMESHIFT
#include "timeshift.h"
#endif
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
    return NULL;
  }
  pro = pb->build();
  if (pro == NULL) {
    tvherror("profile", "Profile class %s is not available!", s);
    return NULL;
  }
  LIST_INIT(&pro->pro_dvr_configs);
  LIST_INIT(&pro->pro_accesses);
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
  pro->pro_refcount = 1;
  TAILQ_INSERT_TAIL(&profiles, pro, pro_link);
  if (save)
    profile_class_save((idnode_t *)pro);
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
  return pro;
}

void
profile_release_(profile_t *pro)
{
  if (pro->pro_free)
    pro->pro_free(pro);
  free(pro->pro_name);
  free(pro->pro_comment);
  free(pro);
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
  access_destroy_by_profile(pro, delconf);
  profile_release(pro);
}

static void
profile_class_save ( idnode_t *in )
{
  profile_t *pro = (profile_t *)in;
  htsmsg_t *c = htsmsg_create_map();
  if (pro == profile_default)
    pro->pro_enabled = 1;
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

static uint32_t
profile_class_enabled_opts(void *o)
{
  profile_t *pro = o;
  uint32_t r = 0;
  if (pro && profile_default == pro)
    r |= PO_RDONLY;
  return r;
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

static uint32_t
profile_class_name_opts(void *o)
{
  profile_t *pro = o;
  uint32_t r = 0;
  if (pro && pro->pro_shield)
    r |= PO_RDONLY;
  return r;
}

const idclass_t profile_class =
{
  .ic_class      = "profile",
  .ic_caption    = "Stream Profile",
  .ic_event      = "profile",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = profile_class_save,
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
      .get_opts = profile_class_enabled_opts,
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
      .get_opts = profile_class_name_opts,
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
      .off      = offsetof(profile_t, pro_comment),
    },
    {
      .type     = PT_INT,
      .id       = "timeout",
      .name     = "Timeout (sec) (0=infinite)",
      .off      = offsetof(profile_t, pro_timeout),
      .def.i    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "restart",
      .name     = "Restart On Error",
      .off      = offsetof(profile_t, pro_restart),
      .def.i    = 0,
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
profile_find_by_name(const char *name, const char *alt)
{
  profile_t *pro;

  lock_assert(&global_lock);

  if (!name && alt) {
    name = alt;
    alt = NULL;
  }

  if (!name)
    return profile_default;

  TAILQ_FOREACH(pro, &profiles, pro_link) {
    if (pro->pro_enabled && !strcmp(pro->pro_name, name))
      return pro;
  }

  if (alt) {
    TAILQ_FOREACH(pro, &profiles, pro_link) {
      if (pro->pro_enabled && !strcmp(pro->pro_name, alt))
        return pro;
    }
  }

  return profile_default;
}

/*
 *
 */
profile_t *
profile_find_by_list(htsmsg_t *uuids, const char *name, const char *alt)
{
  profile_t *pro, *res = NULL;
  htsmsg_field_t *f;
  const char *uuid, *uuid2;

  pro = profile_find_by_uuid(name);
  if (!pro)
    pro  = profile_find_by_name(name, alt);
  uuid = pro ? idnode_uuid_as_str(&pro->pro_id) : "";
  if (uuids) {
    HTSMSG_FOREACH(f, uuids) {
      uuid2 = htsmsg_field_get_str(f) ?: "";
      if (strcmp(uuid, uuid2) == 0)
        return pro;
      if (!res) {
        res = profile_find_by_uuid(uuid2);
        if (!res->pro_enabled)
          res = NULL;
      }
    }
  } else {
    res = pro;
  }
  if (!res)
    res = profile_find_by_name(NULL, NULL);
  return res;
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
void
profile_get_htsp_list(htsmsg_t *array, htsmsg_t *filter)
{
  profile_t *pro;
  htsmsg_t *m;
  htsmsg_field_t *f;
  const char *uuid, *s;

  TAILQ_FOREACH(pro, &profiles, pro_link) {
    if (!pro->pro_work)
      continue;
    uuid = idnode_uuid_as_str(&pro->pro_id);
    if (filter) {
      HTSMSG_FOREACH(f, filter) {
        if (!(s = htsmsg_field_get_str(f)))
          continue;
        if (strcmp(s, uuid) == 0)
          break;
      }
      if (f == NULL)
        continue;
    }
    m = htsmsg_create_map();
    htsmsg_add_str(m, "uuid", uuid);
    htsmsg_add_str(m, "name", pro->pro_name ?: "");
    htsmsg_add_str(m, "comment", pro->pro_comment ?: "");
    htsmsg_add_msg(array, NULL, m);
  }
}

/*
 *
 */
void
profile_chain_init(profile_chain_t *prch, profile_t *pro, void *id)
{
  memset(prch, 0, sizeof(*prch));
  if (pro)
    profile_grab(pro);
  prch->prch_pro = pro;
  prch->prch_id  = id;
  streaming_queue_init(&prch->prch_sq, 0, 0);
}

/*
 *
 */
int
profile_chain_work(profile_chain_t *prch, struct streaming_target *dst,
                   uint32_t timeshift_period, int flags)
{
  profile_t *pro = prch->prch_pro;
  if (pro && pro->pro_work)
    return pro->pro_work(prch, dst, timeshift_period, flags);
  return -1;
}

/*
 *
 */
int
profile_chain_open(profile_chain_t *prch,
                   muxer_config_t *m_cfg, int flags, size_t qsize)
{
  profile_t *pro = prch->prch_pro;
  if (pro && pro->pro_open)
    return pro->pro_open(prch, m_cfg, flags, qsize);
  return -1;
}

/*
 *
 */
int
profile_chain_raw_open(profile_chain_t *prch, void *id, size_t qsize)
{
  muxer_config_t c;

  memset(&c, 0, sizeof(c));
  c.m_type = MC_RAW;
  memset(prch, 0, sizeof(*prch));
  prch->prch_id    = id;
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
#if ENABLE_TIMESHIFT
  if (prch->prch_timeshift)
    timeshift_destroy(prch->prch_timeshift);
#endif
  if (prch->prch_tsfix)
    tsfix_destroy(prch->prch_tsfix);
  if (prch->prch_gh)
    globalheaders_destroy(prch->prch_gh);
#if ENABLE_LIBAV
  if (prch->prch_transcoder)
    transcoder_destroy(prch->prch_transcoder);
#endif
  if (prch->prch_muxer)
    muxer_destroy(prch->prch_muxer);
  streaming_queue_deinit(&prch->prch_sq);
  prch->prch_st = NULL;
  if (prch->prch_pro)
    profile_release(prch->prch_pro);
}

/*
 *  HTSP Profile Class
 */
const idclass_t profile_htsp_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-htsp",
  .ic_caption    = "HTSP Stream Profile",
  .ic_properties = (const property_t[]){
    /* Ready for future extensions */
    { }
  }
};

static int
profile_htsp_work(profile_chain_t *prch,
                  streaming_target_t *dst,
                  uint32_t timeshift_period, int flags)
{
#if ENABLE_TIMESHIFT
  if (timeshift_period > 0) {
    dst = prch->prch_timeshift = timeshift_create(dst, timeshift_period);
    flags |= PRCH_FLAG_TSFIX;
  }
#endif

  if (flags & PRCH_FLAG_TSFIX)
    dst = prch->prch_tsfix = tsfix_create(dst);

  prch->prch_st = dst;

  return 0;
}

static muxer_container_type_t
profile_htsp_get_mc(profile_t *_pro)
{
  return MC_UNKNOWN;
}

static profile_t *
profile_htsp_builder(void)
{
  profile_t *pro = calloc(1, sizeof(*pro));
  pro->pro_work   = profile_htsp_work;
  pro->pro_get_mc = profile_htsp_get_mc;
  return pro;
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
  .ic_super      = &profile_class,
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
profile_mpegts_pass_open(profile_chain_t *prch,
                         muxer_config_t *m_cfg, int flags, size_t qsize)
{
  profile_mpegts_t *pro = (profile_mpegts_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    c = *m_cfg; /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_RAW)
    c.m_type = MC_PASS;
  c.m_rewrite_pat = pro->pro_rewrite_pat;
  c.m_rewrite_pmt = pro->pro_rewrite_pmt;

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
  .ic_super      = &profile_class,
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
profile_matroska_open(profile_chain_t *prch,
                      muxer_config_t *m_cfg, int flags, size_t qsize)
{
  profile_matroska_t *pro = (profile_matroska_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    c = *m_cfg; /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_WEBM)
    c.m_type = MC_MATROSKA;
  if (pro->pro_webm)
    c.m_type = MC_WEBM;

  streaming_queue_init(&prch->prch_sq, 0, qsize);
  prch->prch_gh    = globalheaders_create(&prch->prch_sq.sq_st);
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

#if ENABLE_LIBAV

static int profile_transcode_experimental_codecs = 1;

/*
 *  Transcoding + packet-like muxers
 */
typedef struct profile_transcode {
  profile_t;
  int      pro_mc;
  uint32_t pro_resolution;
  uint32_t pro_channels;
  uint32_t pro_bandwidth;
  char    *pro_language;
  char    *pro_vcodec;
  char    *pro_acodec;
  char    *pro_scodec;
} profile_transcode_t;

static htsmsg_t *
profile_class_mc_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Not set",                       MC_UNKNOWN },
    { "Matroska (mkv) /built-in",      MC_MATROSKA, },
    { "WEBM /built-in",                MC_WEBM, },
    { "MPEG-TS /av-lib",               MC_MPEGTS },
    { "MPEG-PS (DVD) /av-lib",         MC_MPEGPS },
    { "Matroska (mkv) /av-lib",        MC_AVMATROSKA },
    { "WEBM /av-lib",                  MC_AVWEBM },
  };
  return strtab2htsmsg(tab);
}

static htsmsg_t *
profile_class_channels_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Copy layout",                   0 },
    { "Mono",                          1 },
    { "Stereo",                        2 },
    { "Surround (2 Front, Rear Mono)", 3 },
    { "Quad (4.0)",                    4 },
    { "5.0",                           5 },
    { "5.1",                           6 },
    { "6.1",                           7 },
    { "7.1",                           8 }
  };
  return strtab2htsmsg(tab);
}

static htsmsg_t *
profile_class_language_list(void *o)
{
  htsmsg_t *l = htsmsg_create_list();
  const lang_code_t *lc = lang_codes;
  char buf[128];

  while (lc->code2b) {
    htsmsg_t *e = htsmsg_create_map();
    if (!strcmp(lc->code2b, "und")) {
      htsmsg_add_str(e, "key", "");
      htsmsg_add_str(e, "val", "Use original");
    } else {
      htsmsg_add_str(e, "key", lc->code2b);
      snprintf(buf, sizeof(buf), "%s (%s)", lc->desc, lc->code2b);
      buf[sizeof(buf)-1] = '\0';
      htsmsg_add_str(e, "val", buf);
    }
    htsmsg_add_msg(l, NULL, e);
    lc++;
  }
  return l;
}

static inline int
profile_class_check_sct(htsmsg_t *c, int sct)
{
  htsmsg_field_t *f;
  int64_t x;
  HTSMSG_FOREACH(f, c)
    if (!htsmsg_field_get_s64(f, &x))
      if (x == sct)
        return 1;
  return 0;
}

static htsmsg_t *
profile_class_codec_list(int (*check)(int sct))
{
  htsmsg_t *l = htsmsg_create_list(), *e, *c, *m;
  htsmsg_field_t *f;
  const char *s, *s2;
  char buf[128];
  int sct;

  e = htsmsg_create_map();
  htsmsg_add_str(e, "key", "");
  htsmsg_add_str(e, "val", "Do not use");
  htsmsg_add_msg(l, NULL, e);
  e = htsmsg_create_map();
  htsmsg_add_str(e, "key", "copy");
  htsmsg_add_str(e, "val", "Copy codec type");
  htsmsg_add_msg(l, NULL, e);
  c = transcoder_get_capabilities(profile_transcode_experimental_codecs);
  HTSMSG_FOREACH(f, c) {
    if (!(m = htsmsg_field_get_map(f)))
      continue;
    if (htsmsg_get_s32(m, "type", &sct))
      continue;
    if (!check(sct))
      continue;
    if (!(s = htsmsg_get_str(m, "name")))
      continue;
    s2 = htsmsg_get_str(m, "long_name");
    if (s2)
      snprintf(buf, sizeof(buf), "%s: %s", s, s2);
    else
      snprintf(buf, sizeof(buf), "%s", s);
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", s);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  htsmsg_destroy(c);
  return l;
}

static int
profile_class_vcodec_sct_check(int sct)
{
  return SCT_ISVIDEO(sct);
}

static htsmsg_t *
profile_class_vcodec_list(void *o)
{
  return profile_class_codec_list(profile_class_vcodec_sct_check);
}

static int
profile_class_acodec_sct_check(int sct)
{
  return SCT_ISAUDIO(sct);
}

static htsmsg_t *
profile_class_acodec_list(void *o)
{
  return profile_class_codec_list(profile_class_acodec_sct_check);
}

static int
profile_class_scodec_sct_check(int sct)
{
  return SCT_ISSUBTITLE(sct);
}

static htsmsg_t *
profile_class_scodec_list(void *o)
{
  return profile_class_codec_list(profile_class_scodec_sct_check);
}

const idclass_t profile_transcode_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-transcode",
  .ic_caption    = "Transcode",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "container",
      .name     = "Container",
      .off      = offsetof(profile_transcode_t, pro_mc),
      .def.i    = MC_MATROSKA,
      .list     = profile_class_mc_list,
    },
    {
      .type     = PT_U32,
      .id       = "resolution",
      .name     = "Resolution",
      .off      = offsetof(profile_transcode_t, pro_resolution),
      .def.u32  = 384,
    },
    {
      .type     = PT_U32,
      .id       = "channels",
      .name     = "Channels",
      .off      = offsetof(profile_transcode_t, pro_channels),
      .def.u32  = 2,
      .list     = profile_class_channels_list,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = "Language",
      .off      = offsetof(profile_transcode_t, pro_language),
      .list     = profile_class_language_list,
    },
    {
      .type     = PT_STR,
      .id       = "vcodec",
      .name     = "Video Codec",
      .off      = offsetof(profile_transcode_t, pro_vcodec),
      .def.s    = "libx264",
      .list     = profile_class_vcodec_list,
    },
    {
      .type     = PT_STR,
      .id       = "acodec",
      .name     = "Audio Codec",
      .off      = offsetof(profile_transcode_t, pro_acodec),
      .def.s    = "libvorbis",
      .list     = profile_class_acodec_list,
    },
    {
      .type     = PT_STR,
      .id       = "scodec",
      .name     = "Subtitles Codec",
      .off      = offsetof(profile_transcode_t, pro_scodec),
      .def.s    = "",
      .list     = profile_class_scodec_list,
    },
    { }
  }
};

static int
profile_transcode_work(profile_chain_t *prch,
                       streaming_target_t *dst,
                       uint32_t timeshift_period, int flags)
{
  profile_transcode_t *pro = (profile_transcode_t *)prch->prch_pro;
  transcoder_props_t props;

  memset(&props, 0, sizeof(props));
  strncpy(props.tp_vcodec, pro->pro_vcodec ?: "", sizeof(props.tp_vcodec)-1);
  strncpy(props.tp_acodec, pro->pro_acodec ?: "", sizeof(props.tp_acodec)-1);
  strncpy(props.tp_scodec, pro->pro_scodec ?: "", sizeof(props.tp_scodec)-1);
  props.tp_resolution = pro->pro_resolution >= 240 ? pro->pro_resolution : 240;
  props.tp_channels   = pro->pro_channels;
  props.tp_bandwidth  = pro->pro_bandwidth >= 64 ? pro->pro_bandwidth : 64;
  strncpy(props.tp_language, pro->pro_language ?: "", 3);

#if ENABLE_TIMESHIFT
  if (timeshift_period > 0)
    dst = prch->prch_timeshift = timeshift_create(dst, timeshift_period);
#endif
  dst = prch->prch_transcoder = transcoder_create(dst);
  if (!dst) {
    profile_chain_close(prch);
    return -1;
  }
  transcoder_set_properties(dst, &props);
  prch->prch_tsfix = tsfix_create(dst);

  prch->prch_st = prch->prch_tsfix;

  return 0;
}

static int
profile_transcode_mc_valid(int mc)
{
  switch (mc) {
  case MC_MATROSKA:
  case MC_WEBM:
  case MC_MPEGTS:
  case MC_MPEGPS:
  case MC_AVMATROSKA:
    return 1;
  default:
    return 0;
  }
}

static int
profile_transcode_open(profile_chain_t *prch,
                       muxer_config_t *m_cfg, int flags, size_t qsize)
{
  profile_transcode_t *pro = (profile_transcode_t *)prch->prch_pro;
  muxer_config_t c;
  int r;

  if (m_cfg)
    c = *m_cfg; /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (!profile_transcode_mc_valid(c.m_type)) {
    c.m_type = pro->pro_mc;
    if (!profile_transcode_mc_valid(c.m_type))
      c.m_type = MC_MATROSKA;
  }

  streaming_queue_init(&prch->prch_sq, 0, qsize);
  prch->prch_gh = globalheaders_create(&prch->prch_sq.sq_st);

  r = profile_transcode_work(prch, prch->prch_gh, 0,
                             PRCH_FLAG_SKIPZEROING | PRCH_FLAG_TSFIX);
  if (r)
    return r;

  prch->prch_muxer = muxer_create(&c);
  return 0;
}

static muxer_container_type_t
profile_transcode_get_mc(profile_t *_pro)
{
  profile_transcode_t *pro = (profile_transcode_t *)_pro;
  return pro->pro_mc;
}

static void
profile_transcode_free(profile_t *_pro)
{
  profile_transcode_t *pro = (profile_transcode_t *)_pro;
  free(pro->pro_vcodec);
  free(pro->pro_acodec);
  free(pro->pro_scodec);
}

static profile_t *
profile_transcode_builder(void)
{
  profile_transcode_t *pro = calloc(1, sizeof(*pro));
  pro->pro_free   = profile_transcode_free;
  pro->pro_work   = profile_transcode_work;
  pro->pro_open   = profile_transcode_open;
  pro->pro_get_mc = profile_transcode_get_mc;
  return (profile_t *)pro;
}

#endif /* ENABLE_TRANSCODE */

/*
 *  Initialize
 */
void
profile_init(void)
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  profile_t *pro;
  const char *name;

  LIST_INIT(&profile_builders);
  TAILQ_INIT(&profiles);

  profile_register(&profile_mpegts_pass_class, profile_mpegts_pass_builder);
  profile_register(&profile_matroska_class, profile_matroska_builder);
  profile_register(&profile_htsp_class, profile_htsp_builder);
#if ENABLE_LIBAV
  profile_transcode_experimental_codecs =
    getenv("TVHEADEND_LIBAV_NO_EXPERIMENTAL_CODECS") ? 0 : 1;
  profile_register(&profile_transcode_class, profile_transcode_builder);
#endif

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))
        continue;
      (void)profile_create(f->hmf_name, e, 0);
    }
    htsmsg_destroy(c);
  }

  name = "pass";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-mpegts");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_bool(conf, "default", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "MPEG-TS Pass-through");
    htsmsg_add_bool(conf, "rewrite_pmt", 1);
    htsmsg_add_bool(conf, "rewrite_pat", 1);
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }

  name = "matroska";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-matroska");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "Matroska");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }

  name = "htsp";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-htsp");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "HTSP Default Stream Settings");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }

#if ENABLE_LIBAV

  name = "webtv-vp8-vorbis-webm";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-transcode");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "WEBTV profile VP8/Vorbis/WEBM");
    htsmsg_add_s32 (conf, "container", MC_WEBM);
    htsmsg_add_u32 (conf, "resolution", 384);
    htsmsg_add_u32 (conf, "channels", 2);
    htsmsg_add_str (conf, "vcodec", "libvpx");
    htsmsg_add_str (conf, "acodec", "libvorbis");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }
  name = "webtv-h264-aac-mpegts";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-transcode");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "WEBTV profile H264/AAC/MPEG-TS");
    htsmsg_add_s32 (conf, "container", MC_MPEGTS);
    htsmsg_add_u32 (conf, "resolution", 384);
    htsmsg_add_u32 (conf, "channels", 2);
    htsmsg_add_str (conf, "vcodec", "libx264");
    htsmsg_add_str (conf, "acodec", "aac");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }
  name = "webtv-h264-aac-matroska";
  pro = profile_find_by_name(name, NULL);
  if (pro == NULL || strcmp(pro->pro_name, name)) {
    htsmsg_t *conf;

    conf = htsmsg_create_map();
    htsmsg_add_str (conf, "class", "profile-transcode");
    htsmsg_add_bool(conf, "enabled", 1);
    htsmsg_add_str (conf, "name", name);
    htsmsg_add_str (conf, "comment", "WEBTV profile H264/AAC/Matroska");
    htsmsg_add_s32 (conf, "container", MC_MATROSKA);
    htsmsg_add_u32 (conf, "resolution", 384);
    htsmsg_add_u32 (conf, "channels", 2);
    htsmsg_add_str (conf, "vcodec", "libx264");
    htsmsg_add_str (conf, "acodec", "aac");
    htsmsg_add_bool(conf, "shield", 1);
    (void)profile_create(NULL, conf, 1);
    htsmsg_destroy(conf);
  }
#endif
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
