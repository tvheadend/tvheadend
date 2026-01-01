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
#include "transcoding/transcode.h"
#include "transcoding/codec.h"
#endif
#if ENABLE_TIMESHIFT
#include "timeshift.h"
#include "input/mpegts/iptv/iptv_private.h"
#endif
#include "dvr/dvr.h"

extern const idclass_t profile_htsp_class;

profile_builders_queue profile_builders;

struct profile_entry_queue profiles;
static LIST_HEAD(,profile_chain) profile_chains;

static profile_t *profile_default;

/*
 *
 */

void
profile_register(const idclass_t *clazz, profile_builder_t builder)
{
  profile_build_t *pb = calloc(1, sizeof(*pb)), *pb2;
  idclass_register(clazz);
  pb->clazz = clazz;
  pb->build = builder;
  pb2 = LIST_FIRST(&profile_builders);
  if (pb2) {
    /* append tail */
    while (LIST_NEXT(pb2, link))
      pb2 = LIST_NEXT(pb2, link);
    LIST_INSERT_AFTER(pb2, pb, link);
  } else {
    LIST_INSERT_HEAD(&profile_builders, pb, link);
  }
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
    tvherror(LS_PROFILE, "wrong class %s!", s);
    return NULL;
  }
  pro = pb->build();
  if (pro == NULL) {
    tvherror(LS_PROFILE, "Profile class %s is not available!", s);
    return NULL;
  }
  LIST_INIT(&pro->pro_dvr_configs);
  LIST_INIT(&pro->pro_accesses);
  pro->pro_swservice = 1;
  pro->pro_contaccess = 1;
  pro->pro_ca_timeout = 2000;
  if (idnode_insert(&pro->pro_id, uuid, pb->clazz, 0)) {
    if (uuid)
      tvherror(LS_PROFILE, "invalid uuid '%s'", uuid);
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
    idnode_changed(&pro->pro_id);
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
  char ubuf[UUID_HEX_SIZE];
  idnode_save_check(&pro->pro_id, delconf);
  pro->pro_enabled = 0;
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
  if (delconf)
    hts_settings_remove("profile/%s", idnode_uuid_as_str(&pro->pro_id, ubuf));
  TAILQ_REMOVE(&profiles, pro, pro_link);
  idnode_unlink(&pro->pro_id);
  dvr_config_destroy_by_profile(pro, delconf);
  access_destroy_by_profile(pro, delconf);
  profile_release(pro);
}

static htsmsg_t *
profile_class_save ( idnode_t *in, char *filename, size_t fsize )
{
  profile_t *pro = (profile_t *)in;
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  if (pro == profile_default)
    pro->pro_enabled = 1;
  idnode_save(in, c);
  if (pro->pro_shield)
    htsmsg_add_bool(c, "shield", 1);
  if (filename)
    snprintf(filename, fsize, "profile/%s", idnode_uuid_as_str(in, ubuf));
  if (pro->pro_conf_changed)
    pro->pro_conf_changed(pro);
  return c;
}

static void
profile_class_get_title
  ( idnode_t *in, const char *lang, char *dst, size_t dstsize )
{
  profile_t *pro = (profile_t *)in;
  if (pro->pro_name && pro->pro_name[0]) {
    snprintf(dst, dstsize, "%s", pro->pro_name);
  } else {
    snprintf(dst, dstsize, "%s", idclass_get_caption(in->in_class, lang));
  }
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
profile_class_enabled_opts(void *o, uint32_t opts)
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
      idnode_changed(&old->pro_id);
    return 1;
  }
  return 0;
}

static uint32_t
profile_class_name_opts(void *o, uint32_t opts)
{
  profile_t *pro = o;
  uint32_t r = 0;
  if (pro && pro->pro_shield)
    r |= PO_RDONLY;
  return r;
}

static htsmsg_t *
profile_class_priority_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Unset (default)"),          PROFILE_SPRIO_NOTSET },
    { N_("Important"),                PROFILE_SPRIO_IMPORTANT },
    { N_("High"),                     PROFILE_SPRIO_HIGH, },
    { N_("Normal"),                   PROFILE_SPRIO_NORMAL },
    { N_("Low"),                      PROFILE_SPRIO_LOW },
    { N_("Unimportant"),              PROFILE_SPRIO_UNIMPORTANT },
    { N_("DVR override: important"),   PROFILE_SPRIO_DVR_IMPORTANT },
    { N_("DVR override: high"),        PROFILE_SPRIO_DVR_HIGH },
    { N_("DVR override: normal"),      PROFILE_SPRIO_DVR_NORMAL },
    { N_("DVR override: low"),         PROFILE_SPRIO_DVR_LOW },
    { N_("DVR override: unimportant"), PROFILE_SPRIO_DVR_UNIMPORTANT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
profile_class_svfilter_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("None"),                       PROFILE_SVF_NONE },
    { N_("SD: standard definition"),    PROFILE_SVF_SD },
    { N_("HD: high definition"),        PROFILE_SVF_HD },
    { N_("FHD: full high definition"), PROFILE_SVF_FHD },
    { N_("UHD: ultra high definition"), PROFILE_SVF_UHD },
  };
  return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(profile)

const idclass_t profile_class =
{
  .ic_class      = "profile",
  .ic_caption    = N_("Stream - Stream Profiles"),
  .ic_event      = "profile",
  .ic_doc        = tvh_doc_profile_class,
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = profile_class_save,
  .ic_get_title  = profile_class_get_title,
  .ic_delete     = profile_class_delete,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "class",
      .name     = N_("Class"),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .get      = profile_class_class_get,
      .set      = profile_class_class_set,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Profile name"),
      .desc     = N_("The name of the profile."),
      .off      = offsetof(profile_t, pro_name),
      .get_opts = profile_class_name_opts,
      .notify   = idnode_notify_title_changed_lang,
      .group    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable profile."),
      .off      = offsetof(profile_t, pro_enabled),
      .get_opts = profile_class_enabled_opts,
      .group    = 1,
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "default",
      .name     = N_("Default"),
      .desc     = N_("Set as default profile."),
      .set      = profile_class_default_set,
      .get      = profile_class_default_get,
      .opts     = PO_EXPERT,
      .group    = 1
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field. You can enter whatever you "
                     "like here."),
      .off      = offsetof(profile_t, pro_comment),
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "timeout",
      .name     = N_("Data timeout (sec) (0=infinite)"),
      .desc     = N_("The number of seconds to wait for data. "
                     "It handles the situations where no data "
                     "are received at start or the input stream "
                     "is stalled."),
      .off      = offsetof(profile_t, pro_timeout),
      .def.i    = 5,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "timeout_start",
      .name     = N_("Data start timeout (sec) (0=default)"),
      .desc     = N_("The number of seconds to wait for data "
                     "when stream is starting."),
      .off      = offsetof(profile_t, pro_timeout_start),
      .opts     = PO_EXPERT,
      .def.i    = 0,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Default priority"),
      .desc     = N_("If no specific priority was requested. This "
                     "gives certain users a higher priority by "
                     "assigning a streaming profile with a higher "
                     "priority."),
      .list     = profile_class_priority_list,
      .off      = offsetof(profile_t, pro_prio),
      .opts     = PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
      .def.i    = PROFILE_SPRIO_NORMAL,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "fpriority",
      .name     = N_("Force priority"),
      .desc     = N_("Force profile to use this priority."),
      .off      = offsetof(profile_t, pro_fprio),
      .opts     = PO_EXPERT,
      .group    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "restart",
      .name     = N_("Restart on error"),
      .desc     = N_("Restart streaming on error. This is useful for "
                     "DVR so a recording isn't aborted if an error occurs."),
      .off      = offsetof(profile_t, pro_restart),
      .opts     = PO_EXPERT,
      .def.i    = 0,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "contaccess",
      .name     = N_("Continue if descrambling fails"),
      .desc     = N_("Don't abort streaming when an encrypted stream "
                     "can't be decrypted by a CA client that normally "
                     "should be able to decrypt the stream."),
      .off      = offsetof(profile_t, pro_contaccess),
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "catimeout",
      .name     = N_("Descrambling timeout (ms)"),
      .desc     = N_("Check the descrambling status after this timeout."),
      .off      = offsetof(profile_t, pro_ca_timeout),
      .opts     = PO_EXPERT,
      .def.i    = 2000,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "swservice",
      .name     = N_("Switch to another service"),
      .desc     = N_("If something fails, try to switch to a different "
                     "service on another network. Do not try to iterate "
                     "through all inputs/tuners which are capable to "
                     "receive the service."),
      .off      = offsetof(profile_t, pro_swservice),
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "svfilter",
      .name     = N_("Preferred service video type"),
      .desc     = N_("The selected video type should be preferred when "
                     "multiple services are available for a channel."),
      .list     = profile_class_svfilter_list,
      .off      = offsetof(profile_t, pro_svfilter),
      .opts     = PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
      .def.i    = PROFILE_SVF_NONE,
      .group    = 1
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
static profile_t *
profile_find_by_name2(const char *name, const char *alt, int all)
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
    if ((all || pro->pro_enabled) && !strcmp(profile_get_name(pro), name))
      return pro;
  }

  if (alt) {
    TAILQ_FOREACH(pro, &profiles, pro_link) {
      if ((all || pro->pro_enabled) && !strcmp(profile_get_name(pro), alt))
        return pro;
    }
  }

  return profile_default;
}

/*
 *
 */
profile_t *
profile_find_by_name(const char *name, const char *alt)
{
  return profile_find_by_name2(name, alt, 0);
}

/*
 *
 */
int
profile_verify(profile_t *pro, int sflags)
{
  if (!pro)
    return 0;
  if (!pro->pro_enabled)
    return 0;
  if ((sflags & SUBSCRIPTION_HTSP) != 0 && !pro->pro_work)
    return 0;
  if ((sflags & SUBSCRIPTION_HTSP) == 0 && !pro->pro_open)
    return 0;
  sflags &= pro->pro_sflags;
  sflags &= SUBSCRIPTION_PACKET|SUBSCRIPTION_MPEGTS;
  return sflags ? 1 : 0;
}

/*
 *
 */
profile_t *
profile_find_by_list
  (htsmsg_t *uuids, const char *name, const char *alt, int sflags)
{
  profile_t *pro, *res = NULL;
  htsmsg_field_t *f;
  const char *uuid, *uuid2;
  char ubuf[UUID_HEX_SIZE];

  pro = profile_find_by_uuid(name);
  if (!pro)
    pro = profile_find_by_name(name, alt);
  if (!profile_verify(pro, sflags))
    pro = NULL;
  if (uuids) {
    uuid = pro ? idnode_uuid_as_str(&pro->pro_id, ubuf) : "";
    HTSMSG_FOREACH(f, uuids) {
      uuid2 = htsmsg_field_get_str(f) ?: "";
      if (strcmp(uuid, uuid2) == 0 && profile_verify(pro, sflags))
        return pro;
      if (!res) {
        res = profile_find_by_uuid(uuid2);
        if (!profile_verify(res, sflags))
          res = NULL;
      }
    }
  } else {
    res = pro;
  }
  if (!res) {
    res = profile_find_by_name((sflags & SUBSCRIPTION_HTSP) ? "htsp" : NULL, NULL);
    if (!profile_verify(res, sflags))
      tvherror(LS_PROFILE, "unable to select a working profile (asked '%s' alt '%s')", name, alt);
  }
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
    if (name && !strcmp(profile_get_name(pro), name))
      return strdup(name);
  }

  if (profile_default)
    return strdup(profile_get_name(profile_default));

  return NULL;
}

/*
 *
 */
htsmsg_t *
profile_class_get_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "profile/list");
  htsmsg_add_str(m, "event", "profile");
  htsmsg_add_u32(p, "all",  1);
  htsmsg_add_msg(m, "params", p);
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
  char ubuf[UUID_HEX_SIZE];

  TAILQ_FOREACH(pro, &profiles, pro_link) {
    if (!pro->pro_work)
      continue;
    uuid = idnode_uuid_as_str(&pro->pro_id, ubuf);
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
static void
profile_deliver(profile_chain_t *prch, streaming_message_t *sm)
{
  if (prch->prch_start_pending) {
    profile_sharer_t *prsh = prch->prch_sharer;
    streaming_message_t *sm2;
    if (!prsh->prsh_start_msg) {
      streaming_msg_free(sm);
      return;
    }
    sm2 = streaming_msg_create_data(SMT_START,
                                   streaming_start_copy(prsh->prsh_start_msg));
    streaming_target_deliver(prch->prch_post_share, sm2);
    prch->prch_start_pending = 0;
  }
  if (sm)
    streaming_target_deliver(prch->prch_post_share, sm);
}

/*
 *
 */
static void
profile_input(void *opaque, streaming_message_t *sm)
{
  profile_chain_t *prch = opaque, *prch2;
  profile_sharer_t *prsh = prch->prch_sharer;

  if (prsh == NULL) {
    streaming_msg_free(sm);
    return;
  }

  if (sm->sm_type == SMT_START) {
    if (!prsh->prsh_master)
      prsh->prsh_master = prch;
    prch->prch_stop = 0;
  }

  if (prch == prsh->prsh_master) {
    if (sm->sm_type == SMT_STOP) {
      prch->prch_stop = 1;
      /* elect new master */
      prsh->prsh_master = NULL;
      LIST_FOREACH(prch2, &prsh->prsh_chains, prch_sharer_link)
        if (!prch2->prch_stop) {
          prsh->prsh_master = prch2;
          break;
        }
      if (prsh->prsh_master)
        goto direct;
    }
    streaming_target_deliver(prch->prch_share, sm);
    return;
  }

  if (sm->sm_type == SMT_STOP) {
    prch->prch_stop = 1;
  } else if (sm->sm_type == SMT_START) {
    prch->prch_stop = 0;
    prch->prch_start_pending = 1;
    streaming_msg_free(sm);
    sm = NULL;
  } else if (sm->sm_type == SMT_PACKET || sm->sm_type == SMT_MPEGTS) {
    streaming_msg_free(sm);
    return;
  }

direct:
  profile_deliver(prch, sm);
}

static htsmsg_t *
profile_input_info(void *opaque, htsmsg_t *list)
{
  profile_chain_t *prch = opaque;
  streaming_target_t *st = prch->prch_share;
  htsmsg_add_str(list, NULL, "profile input");
  st->st_ops.st_info(st->st_opaque, list);
  st = prch->prch_post_share;
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t profile_input_ops = {
  .st_cb   = profile_input,
  .st_info = profile_input_info
};

/*
 *
 */
static void
profile_input_queue(void *opaque, streaming_message_t *sm)
{
  profile_chain_t *prch = opaque;
  profile_sharer_t *prsh = prch->prch_sharer;
  profile_sharer_message_t *psm = malloc(sizeof(*psm));
  psm->psm_prch = prch;
  psm->psm_sm = sm;
  tvh_mutex_lock(&prsh->prsh_queue_mutex);
  if (prsh->prsh_queue_run) {
    TAILQ_INSERT_TAIL(&prsh->prsh_queue, psm, psm_link);
    tvh_cond_signal(&prsh->prsh_queue_cond, 0);
  } else {
    streaming_msg_free(sm);
    free(psm);
  }
  tvh_mutex_unlock(&prsh->prsh_queue_mutex);
}

static htsmsg_t *
profile_input_queue_info(void *opaque, htsmsg_t *list)
{
  htsmsg_add_str(list, NULL, "profile queue input");
  profile_input_info(opaque, list);
  return list;
}

static streaming_ops_t profile_input_queue_ops = {
  .st_cb   = profile_input_queue,
  .st_info = profile_input_queue_info
};

/*
 *
 */
static void
profile_sharer_deliver(profile_chain_t *prch, streaming_message_t *sm)
{
  if (sm->sm_type == SMT_PACKET) {
    if (!prch->prch_ts_delta)
      goto deliver;
    th_pkt_t *pkt = sm->sm_data;
    if (prch->prch_ts_delta == PTS_UNSET)
      prch->prch_ts_delta = MAX(0, pkt->pkt_dts - 10000);
    /*
     * time correction here
     */
    if (pkt->pkt_pts >= prch->prch_ts_delta &&
        pkt->pkt_dts >= prch->prch_ts_delta &&
        pkt->pkt_pcr >= prch->prch_ts_delta) {
      th_pkt_t *n = pkt_copy_shallow(pkt);
      pkt_ref_dec(pkt);
      n->pkt_pts -= prch->prch_ts_delta;
      n->pkt_dts -= prch->prch_ts_delta;
      n->pkt_pcr -= prch->prch_ts_delta;
      sm->sm_data = n;
    } else {
      pkt_trace(LS_PROFILE, pkt, "packet drop (delta %"PRId64")", prch->prch_ts_delta);
      streaming_msg_free(sm);
      return;
    }
  }
deliver:
  profile_deliver(prch, sm);
}

/*
 *
 */
static void
profile_sharer_input(void *opaque, streaming_message_t *sm)
{
  profile_sharer_t *prsh = opaque;
  profile_chain_t *prch, *next, *run = NULL;

  if (sm->sm_type == SMT_STOP) {
    if (prsh->prsh_start_msg)
      streaming_start_unref(prsh->prsh_start_msg);
    prsh->prsh_start_msg = NULL;
  }
  for (prch = LIST_FIRST(&prsh->prsh_chains); prch; prch = next) {
    next = LIST_NEXT(prch, prch_sharer_link);
    if (prch == prsh->prsh_master) {
      if (sm->sm_type == SMT_START) {
        if (prsh->prsh_start_msg)
          streaming_start_unref(prsh->prsh_start_msg);
        prsh->prsh_start_msg = streaming_start_copy(sm->sm_data);
      }
      if (run)
        profile_sharer_deliver(run, streaming_msg_clone(sm));
      run = prch;
      continue;
    } else if (sm->sm_type == SMT_STOP) {
      run = prch;
      continue;
    }
    if (sm->sm_type != SMT_PACKET && sm->sm_type != SMT_MPEGTS)
      continue;
    if (prch->prch_stop)
      continue;
    if (run)
      profile_sharer_deliver(run, streaming_msg_clone(sm));
    run = prch;
  }

  if (run)
    profile_sharer_deliver(run, sm);
  else
    streaming_msg_free(sm);
}

static htsmsg_t *
profile_sharer_input_info(void *opaque, htsmsg_t *list)
{
  htsmsg_add_str(list, NULL, "profile sharer input");
  return list;
}

static streaming_ops_t profile_sharer_input_ops = {
  .st_cb   = profile_sharer_input,
  .st_info = profile_sharer_input_info
};

/*
 *
 */
static profile_sharer_t *
profile_sharer_find(profile_chain_t *prch)
{
  profile_sharer_t *prsh = NULL;
  profile_chain_t *prch2;
  int do_queue = prch->prch_can_share != NULL;

  LIST_FOREACH(prch2, &profile_chains, prch_link) {
    if (prch2->prch_id != prch->prch_id)
      continue;
    if (prch2 == prch)
      continue;
    if (prch2->prch_can_share && prch2->prch_can_share(prch2, prch)) {
      prsh = prch2->prch_sharer;
      break;
    }
  }
  if (!prsh) {
    prsh = calloc(1, sizeof(*prsh));
    prsh->prsh_do_queue = do_queue;
    tvh_mutex_init(&prsh->prsh_queue_mutex, NULL);
    tvh_cond_init(&prsh->prsh_queue_cond, 1);
    TAILQ_INIT(&prsh->prsh_queue);
    streaming_target_init(&prsh->prsh_input, &profile_sharer_input_ops, prsh, 0);
    LIST_INIT(&prsh->prsh_chains);
  }
  return prsh;
}

/*
 *
 */
static void *
profile_sharer_thread(void *aux)
{
  profile_sharer_t *prsh = aux;
  profile_sharer_message_t *psm;
  int run = 1;

  while (run) {
    tvh_mutex_lock(&prsh->prsh_queue_mutex);
    run = prsh->prsh_queue_run;
    psm = TAILQ_FIRST(&prsh->prsh_queue);
    if (run && psm == NULL) {
      tvh_cond_wait(&prsh->prsh_queue_cond, &prsh->prsh_queue_mutex);
      run = prsh->prsh_queue_run;
      psm = TAILQ_FIRST(&prsh->prsh_queue);
    }
    if (run && psm) {
      if (psm) {
        profile_input(psm->psm_prch, psm->psm_sm);
        TAILQ_REMOVE(&prsh->prsh_queue, psm, psm_link);
        free(psm);
      }
    }
    tvh_mutex_unlock(&prsh->prsh_queue_mutex);
  }
  return NULL;
}

/*
 *
 */
static int
profile_sharer_postinit(profile_sharer_t *prsh)
{
  int r;

  if (!prsh->prsh_do_queue)
    return 0;
  if (prsh->prsh_queue_run)
    return 0;
  prsh->prsh_queue_run = 1;
  r = tvh_thread_create(&prsh->prsh_queue_thread, NULL,
                        profile_sharer_thread, prsh, "sharer");
  if (r) {
    prsh->prsh_queue_run = 0;
    tvherror(LS_PROFILE, "unable to create sharer thread");
  }
  return r;
}

/*
 *
 */
static int
profile_sharer_create(profile_sharer_t *prsh,
                      profile_chain_t *prch,
                      streaming_target_t *dst)
{
  prch->prch_post_share = dst;
  tvh_mutex_lock(&prsh->prsh_queue_mutex);
  prch->prch_ts_delta = LIST_EMPTY(&prsh->prsh_chains) ? 0 : PTS_UNSET;
  LIST_INSERT_HEAD(&prsh->prsh_chains, prch, prch_sharer_link);
  prch->prch_sharer = prsh;
  if (!prsh->prsh_master)
    prsh->prsh_master = prch;
  tvh_mutex_unlock(&prsh->prsh_queue_mutex);
  return 0;
}

/*
 *
 */
static void
profile_sharer_destroy(profile_chain_t *prch)
{
  profile_sharer_t *prsh = prch->prch_sharer;
  profile_sharer_message_t *psm, *psm2;

  if (prsh == NULL)
    return;
  LIST_REMOVE(prch, prch_sharer_link);
  if (LIST_EMPTY(&prsh->prsh_chains)) {
    if (prsh->prsh_queue_run) {
      tvh_mutex_lock(&prsh->prsh_queue_mutex);
      prsh->prsh_queue_run = 0;
      tvh_cond_signal(&prsh->prsh_queue_cond, 0);
	    prch->prch_sharer = NULL;
      prch->prch_post_share = NULL;
      tvh_mutex_unlock(&prsh->prsh_queue_mutex);
      pthread_join(prsh->prsh_queue_thread, NULL);
      while ((psm = TAILQ_FIRST(&prsh->prsh_queue)) != NULL) {
        streaming_msg_free(psm->psm_sm);
        TAILQ_REMOVE(&prsh->prsh_queue, psm, psm_link);
        free(psm);
      }
    }
    if (prsh->prsh_tsfix)
      tsfix_destroy(prsh->prsh_tsfix);
#if ENABLE_LIBAV
    if (prsh->prsh_transcoder)
      transcoder_destroy(prsh->prsh_transcoder);
#endif
    if (prsh->prsh_start_msg)
      streaming_start_unref(prsh->prsh_start_msg);
    free(prsh);
  } else {
    if (prsh->prsh_queue_run) {
      tvh_mutex_lock(&prsh->prsh_queue_mutex);
      for (psm = TAILQ_FIRST(&prsh->prsh_queue); psm; psm = psm2) {
        psm2 = TAILQ_NEXT(psm, psm_link);
        if (psm->psm_prch != prch) continue;
        if (psm->psm_sm->sm_type == SMT_PACKET ||
            psm->psm_sm->sm_type == SMT_MPEGTS)
          streaming_msg_free(psm->psm_sm);
        else
          profile_input(psm->psm_prch, psm->psm_sm);
        TAILQ_REMOVE(&prsh->prsh_queue, psm, psm_link);
        free(psm);
      }
      prch->prch_sharer = NULL;
      prch->prch_post_share = NULL;
      if (prsh->prsh_master == prch)
        prsh->prsh_master = NULL;
      tvh_mutex_unlock(&prsh->prsh_queue_mutex);
    } else {
      tvh_mutex_lock(&prsh->prsh_queue_mutex);
      prch->prch_sharer = NULL;
      prch->prch_post_share = NULL;
      if (prsh->prsh_master == prch)
        prsh->prsh_master = NULL;
      tvh_mutex_unlock(&prsh->prsh_queue_mutex);
	  } 
  }
}

/*
 *
 */
void
profile_chain_init(profile_chain_t *prch, profile_t *pro, void *id, int queue)
{
  memset(prch, 0, sizeof(*prch));
  if (pro)
    profile_grab(pro);
  prch->prch_pro = pro;
  prch->prch_id  = id;
  if (queue) {
    streaming_queue_init(&prch->prch_sq, 0, 0);
    prch->prch_sq_used = 1;
  }
  LIST_INSERT_HEAD(&profile_chains, prch, prch_link);
  prch->prch_linked = 1;
  prch->prch_stop = 1;
}

/*
 *
 */
int
profile_chain_work(profile_chain_t *prch, struct streaming_target *dst,
                   uint32_t timeshift_period)
{
  profile_t *pro = prch->prch_pro;
  if (pro && pro->pro_work)
    return pro->pro_work(prch, dst, timeshift_period);
  return -1;
}

/*
 *
 */
int
profile_chain_reopen(profile_chain_t *prch,
                     muxer_config_t *m_cfg,
                     muxer_hints_t *hints, int flags)
{
  profile_t *pro = prch->prch_pro;
  if (pro && pro->pro_reopen)
    return pro->pro_reopen(prch, m_cfg, hints, flags);
  return -1;
}

/*
 *
 */
int
profile_chain_open(profile_chain_t *prch,
                   muxer_config_t *m_cfg,
                   muxer_hints_t *hints,
                   int flags, size_t qsize)
{
  profile_t *pro = prch->prch_pro;
  if (pro && pro->pro_open)
    return pro->pro_open(prch, m_cfg, hints, flags, qsize);
  return -1;
}

/*
 *
 */
int
profile_chain_raw_open(profile_chain_t *prch, void *id, size_t qsize, int muxer)
{
  muxer_config_t c;

  memset(prch, 0, sizeof(*prch));
  prch->prch_id    = id;
  prch->prch_flags = SUBSCRIPTION_MPEGTS;
  streaming_queue_init(&prch->prch_sq, SMT_PACKET, qsize);
  prch->prch_sq_used = 1;
  prch->prch_st    = &prch->prch_sq.sq_st;
  if (muxer) {
    memset(&c, 0, sizeof(c));
    c.m_type = MC_RAW;
    prch->prch_muxer = muxer_create(&c, NULL);
  }
  return 0;
}

/*
 *
 */

static const int prio2weight[] = {
  [PROFILE_SPRIO_DVR_IMPORTANT]   = 525,
  [PROFILE_SPRIO_DVR_HIGH]        = 425,
  [PROFILE_SPRIO_DVR_NORMAL]      = 325,
  [PROFILE_SPRIO_DVR_LOW]         = 225,
  [PROFILE_SPRIO_DVR_UNIMPORTANT] = 175,
  [PROFILE_SPRIO_IMPORTANT]       = 150,
  [PROFILE_SPRIO_HIGH]            = 125,
  [PROFILE_SPRIO_NORMAL]          = 100,
  [PROFILE_SPRIO_LOW]             = 75,
  [PROFILE_SPRIO_UNIMPORTANT]     = 50,
  [PROFILE_SPRIO_NOTSET]          = 0
};

int profile_chain_weight(profile_chain_t *prch, int custom)
{
  int w, w2;

  w = 100;
  if (prch->prch_pro) {
    if (!prch->prch_pro->pro_fprio && custom > 0)
      return custom;
    if (idnode_is_instance(&prch->prch_pro->pro_id, &profile_htsp_class))
      w = 150;
    w2 = prch->prch_pro->pro_prio;
    if (w2 > 0 && w2 < ARRAY_SIZE(prio2weight))
       w = prio2weight[w2];
  } else {
    if (custom > 0)
      return custom;
  }
  return w;
}

/*
 *
 */
void
profile_chain_close(profile_chain_t *prch)
{
  if (prch == NULL)
    return;

  profile_sharer_destroy(prch);

#if ENABLE_TIMESHIFT
  if (prch->prch_timeshift) {
    timeshift_destroy(prch->prch_timeshift);
    prch->prch_timeshift = NULL;
  }
#endif
  if (prch->prch_gh) {
    globalheaders_destroy(prch->prch_gh);
    prch->prch_gh = NULL;
  }
  if (prch->prch_tsfix) {
    tsfix_destroy(prch->prch_tsfix);
    prch->prch_tsfix = NULL;
  }
  if (prch->prch_muxer) {
    muxer_destroy(prch->prch_muxer);
    prch->prch_muxer = NULL;
  }

  prch->prch_st = NULL;

  if (prch->prch_sq_used) {
    streaming_queue_deinit(&prch->prch_sq);
    prch->prch_sq_used = 0;
  }

  if (prch->prch_linked) {
    LIST_REMOVE(prch, prch_link);
    prch->prch_linked = 0;
  }

  if (prch->prch_pro) {
    profile_release(prch->prch_pro);
    prch->prch_pro = NULL;
  }

  prch->prch_id = NULL;
}

/*
 *  HTSP Profile Class
 */
const idclass_t profile_htsp_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-htsp",
  .ic_caption    = N_("HTSP Stream Profile"),
  .ic_properties = (const property_t[]){
    /* Ready for future extensions */
    { }
  }
};

static int
profile_htsp_work(profile_chain_t *prch,
                  streaming_target_t *dst,
                  uint32_t timeshift_period)
{
  profile_sharer_t *prsh;

  prsh = profile_sharer_find(prch);
  if (!prsh)
    goto fail;

  if (!prsh->prsh_tsfix)
    prsh->prsh_tsfix = tsfix_create(&prsh->prsh_input);
  prch->prch_share = prsh->prsh_tsfix;

#if ENABLE_TIMESHIFT
  if (timeshift_period > 0)
    dst = prch->prch_timeshift = timeshift_create(dst, timeshift_period);
#endif

  dst = prch->prch_gh = globalheaders_create(dst);

  if (profile_sharer_create(prsh, prch, dst))
    goto fail;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  streaming_target_init(&prch->prch_input,
                        prsh->prsh_do_queue ?
                          &profile_input_queue_ops : &profile_input_ops, prch,
                        0);
  prch->prch_st = &prch->prch_input;
  if (profile_sharer_postinit(prsh))
    goto fail;
  return 0;

fail:
  profile_chain_close(prch);
  return -1;
}

static profile_t *
profile_htsp_builder(void)
{
  profile_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_work   = profile_htsp_work;
  return pro;
}

/*
 *  MPEG-TS passthrough muxer
 */
typedef struct profile_mpegts {
  profile_t;
  uint16_t pro_rewrite_sid;
  int pro_rewrite_pmt;
  int pro_rewrite_pat;
  int pro_rewrite_sdt;
  int pro_rewrite_nit;
  int pro_rewrite_eit;
} profile_mpegts_t;

static int
profile_pass_rewrite_sid_set (void *in, const void *v)
{
  profile_mpegts_t *pro = (profile_mpegts_t *)in;
  const uint16_t *val = v;
  if (*val != pro->pro_rewrite_sid) {
    if (*val > 0) {
      pro->pro_rewrite_pmt = 1;
      pro->pro_rewrite_pat = 1;
      pro->pro_rewrite_sdt = 1;
      pro->pro_rewrite_nit = 1;
      pro->pro_rewrite_eit = 1;
    }
    pro->pro_rewrite_sid = *val;
    return 1;
  }
  return 0;
}

static int
profile_pass_int_set (void *in, const void *v, int *prop)
{
  profile_mpegts_t *pro = (profile_mpegts_t *)in;
  int val = *(int *)v;
  if (pro->pro_rewrite_sid > 0) val = 1;
  if (val != *prop) {
    *prop = val;
    return 1;
  }
  return 0;
}

static int
profile_pass_rewrite_pmt_set (void *in, const void *v)
{
  return profile_pass_int_set(in, v, &((profile_mpegts_t *)in)->pro_rewrite_pmt);
}

static int
profile_pass_rewrite_pat_set (void *in, const void *v)
{
  return profile_pass_int_set(in, v, &((profile_mpegts_t *)in)->pro_rewrite_pat);
}

static int
profile_pass_rewrite_sdt_set (void *in, const void *v)
{
  return profile_pass_int_set(in, v, &((profile_mpegts_t *)in)->pro_rewrite_sdt);
}

static int
profile_pass_rewrite_nit_set (void *in, const void *v)
{
  return profile_pass_int_set(in, v, &((profile_mpegts_t *)in)->pro_rewrite_nit);
}

static int
profile_pass_rewrite_eit_set (void *in, const void *v)
{
  return profile_pass_int_set(in, v, &((profile_mpegts_t *)in)->pro_rewrite_eit);
}

const idclass_t profile_mpegts_pass_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-mpegts",
  .ic_caption    = N_("MPEG-TS Pass-thru/built-in"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Rewrite MPEG-TS SI Table(s) Settings"),
      .number = 2,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Rewrite Service ID"),
      .desc     = N_("Rewrite service identifier (SID) using the specified "
                     "value (usually 1). Zero means no rewrite."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_sid),
      .set      = profile_pass_rewrite_sid_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pmt",
      .name     = N_("Rewrite PMT"),
      .desc     = N_("Rewrite PMT (Program Map Table) packets to only "
                     "include information about the currently-streamed "
                     "service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_pmt),
      .set      = profile_pass_rewrite_pmt_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pat",
      .name     = N_("Rewrite PAT"),
      .desc     = N_("Rewrite PAT (Program Association Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_pat),
      .set      = profile_pass_rewrite_pat_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_sdt",
      .name     = N_("Rewrite SDT"),
      .desc     = N_("Rewrite SDT (Service Description Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_sdt),
      .set      = profile_pass_rewrite_sdt_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_nit",
      .name     = N_("Rewrite NIT"),
      .desc     = N_("Rewrite NIT (Network Information Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_nit),
      .set      = profile_pass_rewrite_nit_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_eit",
      .name     = N_("Rewrite EIT"),
      .desc     = N_("Rewrite EIT (Event Information Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_t, pro_rewrite_eit),
      .set      = profile_pass_rewrite_eit_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 2
    },
    { }
  }
};

static int
profile_mpegts_pass_reopen(profile_chain_t *prch,
                           muxer_config_t *m_cfg,
                           muxer_hints_t *hints, int flags)
{
  profile_mpegts_t *pro = (profile_mpegts_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_RAW)
    c.m_type = MC_PASS;
  c.u.pass.m_rewrite_sid = pro->pro_rewrite_sid;
  c.u.pass.m_rewrite_pat = pro->pro_rewrite_pat;
  c.u.pass.m_rewrite_pmt = pro->pro_rewrite_pmt;
  c.u.pass.m_rewrite_sdt = pro->pro_rewrite_sdt;
  c.u.pass.m_rewrite_nit = pro->pro_rewrite_nit;
  c.u.pass.m_rewrite_eit = pro->pro_rewrite_eit;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_mpegts_pass_open(profile_chain_t *prch,
                         muxer_config_t *m_cfg,
                         muxer_hints_t *hints,
                         int flags, size_t qsize)
{
  prch->prch_flags = SUBSCRIPTION_MPEGTS;

  prch->prch_sq.sq_st.st_reject_filter = SMT_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  prch->prch_st    = &prch->prch_sq.sq_st;

  return profile_mpegts_pass_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_mpegts_pass_builder(void)
{
  profile_mpegts_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_MPEGTS;
  pro->pro_reopen = profile_mpegts_pass_reopen;
  pro->pro_open   = profile_mpegts_pass_open;
  pro->pro_rewrite_sid = 1;
  pro->pro_rewrite_pat = 1;
  pro->pro_rewrite_pmt = 1;
  pro->pro_rewrite_sdt = 1;
  pro->pro_rewrite_nit = 1;
  pro->pro_rewrite_eit = 1;
  return (profile_t *)pro;
}

/*
 *  MPEG-TS spawn muxer
 */
typedef struct profile_mpegts_spawn {
  profile_t;
  char *pro_cmdline;
  char *pro_mime;
  int   pro_killsig;
  int   pro_killtimeout;
  uint16_t pro_rewrite_sid;
  int pro_rewrite_pmt;
  int pro_rewrite_pat;
  int pro_rewrite_sdt;
  int pro_rewrite_nit;
  int pro_rewrite_eit;
} profile_mpegts_spawn_t;

static int
profile_spawn_rewrite_sid_set (void *in, const void *v)
{
  profile_mpegts_spawn_t *pro = (profile_mpegts_spawn_t *)in;
  const uint16_t *val = v;
  if (*val != pro->pro_rewrite_sid) {
    if (*val > 0) {
      pro->pro_rewrite_pmt = 1;
      pro->pro_rewrite_pat = 1;
      pro->pro_rewrite_sdt = 1;
      pro->pro_rewrite_nit = 1;
      pro->pro_rewrite_eit = 1;
    }
    pro->pro_rewrite_sid = *val;
    return 1;
  }
  return 0;
}

static int
profile_spawn_int_set (void *in, const void *v, int *prop)
{
  profile_mpegts_spawn_t *pro = (profile_mpegts_spawn_t *)in;
  int val = *(int *)v;
  if (pro->pro_rewrite_sid > 0) val = 1;
  if (val != *prop) {
    *prop = val;
    return 1;
  }
  return 0;
}

static int
profile_spawn_rewrite_pmt_set (void *in, const void *v)
{
  return profile_spawn_int_set(in, v, &((profile_mpegts_spawn_t *)in)->pro_rewrite_pmt);
}

static int
profile_spawn_rewrite_pat_set (void *in, const void *v)
{
  return profile_spawn_int_set(in, v, &((profile_mpegts_spawn_t *)in)->pro_rewrite_pat);
}

static int
profile_spawn_rewrite_sdt_set (void *in, const void *v)
{
  return profile_spawn_int_set(in, v, &((profile_mpegts_spawn_t *)in)->pro_rewrite_sdt);
}

static int
profile_spawn_rewrite_nit_set (void *in, const void *v)
{
  return profile_spawn_int_set(in, v, &((profile_mpegts_spawn_t *)in)->pro_rewrite_nit);
}

static int
profile_spawn_rewrite_eit_set (void *in, const void *v)
{
  return profile_spawn_int_set(in, v, &((profile_mpegts_spawn_t *)in)->pro_rewrite_eit);
}


const idclass_t profile_mpegts_spawn_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-mpegts-spawn",
  .ic_caption    = N_("MPEG-TS Spawn/built-in"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Spawn Settings"),
      .number = 2,
    },
    {
      .name   = N_("Rewrite MPEG-TS SI Table(s) Settings"),
      .number = 3,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "cmdline",
      .name     = N_("Command line"),
      .desc     = N_("Command line to run a task which accepts MPEG-TS stream"
                     " on stdin and writes output to stdout in format specified"
                     " by the selected mime type."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_cmdline),
      .opts     = PO_MULTILINE,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "mime",
      .name     = N_("Mime type"),
      .desc     = N_("Mime type string (for example 'video/mp2t')."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_mime),
      .group    = 2
    },
    {
      .type     = PT_INT,
      .id       = "killsig",
      .name     = N_("Kill signal (pipe)"),
      .desc     = N_("Kill signal to send to the spawn."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_killsig),
      .list     = proplib_kill_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .def.i    = TVH_KILL_TERM,
      .group    = 2
    },
    {
      .type     = PT_INT,
      .id       = "kill_timeout",
      .name     = N_("Kill timeout (pipe/secs)"),
      .desc     = N_("Number of seconds to wait for spawn to die."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_killtimeout),
      .opts     = PO_EXPERT,
      .def.i    = 15,
      .group    = 2
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Rewrite Service ID"),
      .desc     = N_("Rewrite service identifier (SID) using the specified "
                     "value (usually 1). Zero means no rewrite."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_sid),
      .set      = profile_spawn_rewrite_sid_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pmt",
      .name     = N_("Rewrite PMT"),
      .desc     = N_("Rewrite PMT (Program Map Table) packets to only "
                     "include information about the currently-streamed "
                     "service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_pmt),
      .set      = profile_spawn_rewrite_pmt_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pat",
      .name     = N_("Rewrite PAT"),
      .desc     = N_("Rewrite PAT (Program Association Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_pat),
      .set      = profile_spawn_rewrite_pat_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_sdt",
      .name     = N_("Rewrite SDT"),
      .desc     = N_("Rewrite SDT (Service Description Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_sdt),
      .set      = profile_spawn_rewrite_sdt_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_nit",
      .name     = N_("Rewrite NIT"),
      .desc     = N_("Rewrite NIT (Network Information Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_nit),
      .set      = profile_spawn_rewrite_nit_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_eit",
      .name     = N_("Rewrite EIT"),
      .desc     = N_("Rewrite EIT (Event Information Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_mpegts_spawn_t, pro_rewrite_eit),
      .set      = profile_spawn_rewrite_eit_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    { }
  }
};

static int
profile_mpegts_spawn_reopen(profile_chain_t *prch,
                            muxer_config_t *m_cfg,
                            muxer_hints_t *hints, int flags)
{
  profile_mpegts_spawn_t *pro = (profile_mpegts_spawn_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_RAW)
    c.m_type = MC_PASS;
  c.u.pass.m_rewrite_sid = pro->pro_rewrite_sid;
  c.u.pass.m_rewrite_pat = pro->pro_rewrite_pat;
  c.u.pass.m_rewrite_pmt = pro->pro_rewrite_pmt;
  c.u.pass.m_rewrite_sdt = pro->pro_rewrite_sdt;
  c.u.pass.m_rewrite_nit = pro->pro_rewrite_nit;
  c.u.pass.m_rewrite_eit = pro->pro_rewrite_eit;

  mystrset(&c.u.pass.m_cmdline, pro->pro_cmdline);
  mystrset(&c.u.pass.m_mime, pro->pro_mime);
  c.u.pass.m_killsig = pro->pro_killsig;
  c.u.pass.m_killtimeout = pro->pro_killtimeout;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_mpegts_spawn_open(profile_chain_t *prch,
                          muxer_config_t *m_cfg,
                          muxer_hints_t *hints,
                          int flags, size_t qsize)
{
  prch->prch_flags = SUBSCRIPTION_MPEGTS;

  prch->prch_sq.sq_st.st_reject_filter = SMT_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  prch->prch_st    = &prch->prch_sq.sq_st;

  return profile_mpegts_spawn_reopen(prch, m_cfg, hints, flags);
}

static void
profile_mpegts_spawn_free(profile_t *_pro)
{
  profile_mpegts_spawn_t *pro = (profile_mpegts_spawn_t *)_pro;
  free(pro->pro_cmdline);
  free(pro->pro_mime);
}

static profile_t *
profile_mpegts_spawn_builder(void)
{
  profile_mpegts_spawn_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_MPEGTS;
  pro->pro_free   = profile_mpegts_spawn_free;
  pro->pro_reopen = profile_mpegts_spawn_reopen;
  pro->pro_open   = profile_mpegts_spawn_open;
  pro->pro_rewrite_sid = 1;
  pro->pro_rewrite_pat = 1;
  pro->pro_rewrite_pmt = 1;
  pro->pro_rewrite_sdt = 1;
  pro->pro_rewrite_nit = 1;
  pro->pro_rewrite_eit = 1;
  pro->pro_killsig = TVH_KILL_TERM;
  pro->pro_killtimeout = 15;
  return (profile_t *)pro;
}

/*
 *  Matroska muxer
 */
typedef struct profile_matroska {
  profile_t;
  int pro_webm;
  int pro_dvbsub_reorder;
} profile_matroska_t;

const idclass_t profile_matroska_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-matroska",
  .ic_caption    = N_("Matroska (mkv)/built-in"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Matroska Specific Settings"),
      .number = 2,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "webm",
      .name     = N_("WEBM"),
      .desc     = N_("Use WEBM format."),
      .off      = offsetof(profile_matroska_t, pro_webm),
      .opts     = PO_ADVANCED,
      .def.i    = 0,
      .group    = 2
    },
    {
      .type     = PT_BOOL,
      .id       = "dvbsub_reorder",
      .name     = N_("Reorder DVBSUB"),
      .desc     = N_("Reorder DVB subtitle packets."),
      .off      = offsetof(profile_matroska_t, pro_dvbsub_reorder),
      .opts     = PO_ADVANCED,
      .def.i    = 1,
      .group    = 2
    },
    { }
  }
};

static int
profile_matroska_reopen(profile_chain_t *prch,
                        muxer_config_t *m_cfg,
                        muxer_hints_t *hints, int flags)
{
  profile_matroska_t *pro = (profile_matroska_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_WEBM)
    c.m_type = MC_MATROSKA;
  if (pro->pro_webm)
    c.m_type = MC_WEBM;

  c.u.mkv.m_dvbsub_reorder = pro->pro_dvbsub_reorder;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_matroska_open(profile_chain_t *prch,
                      muxer_config_t *m_cfg,
                      muxer_hints_t *hints,
                      int flags, size_t qsize)
{
  streaming_target_t *dst;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  dst = prch->prch_gh    = globalheaders_create(&prch->prch_sq.sq_st);
  dst = prch->prch_tsfix = tsfix_create(dst);
  prch->prch_st    = dst;

  return profile_matroska_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_matroska_builder(void)
{
  profile_matroska_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_reopen = profile_matroska_reopen;
  pro->pro_open   = profile_matroska_open;
  pro->pro_dvbsub_reorder = 1;
  return (profile_t *)pro;
}


/*
 *  Audioes Muxer
 */
typedef struct profile_audio {
  profile_t;
  int pro_mc;
  int pro_index;
} profile_audio_t;

static htsmsg_t *
profile_class_mc_audio_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Any"),                          MC_UNKNOWN },
    { N_("MPEG-2 audio"),                 MC_MPEG2AUDIO, },
    { N_("AC3 audio"),                    MC_AC3, },
    { N_("AAC audio"),                    MC_AAC },
    { N_("MP4 audio"),                    MC_MP4A },
    { N_("Vorbis audio"),                 MC_VORBIS },
    { N_("AC-4 audio"),                   MC_AC4, },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t profile_audio_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-audio",
  .ic_caption    = N_("Audio stream"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "type",
      .name     = N_("Audio type"),
      .desc     = N_("Pick the stream with given audio type only."),
      .opts     = PO_DOC_NLIST,
      .off      = offsetof(profile_audio_t, pro_mc),
      .list     = profile_class_mc_audio_list,
      .group    = 1
    },
    {
      .type     = PT_INT,
      .id       = "index",
      .name     = N_("Stream index"),
      .desc     = N_("Stream index (starts with zero)."),
      .off      = offsetof(profile_audio_t, pro_index),
      .group    = 1
    },
    { }
  }
};


static int
profile_audio_reopen(profile_chain_t *prch,
                     muxer_config_t *m_cfg,
                     muxer_hints_t *hints, int flags)
{
  muxer_config_t c;
  profile_audio_t *pro = (profile_audio_t *)prch->prch_pro;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  c.m_type = pro->pro_mc != MC_UNKNOWN ? pro->pro_mc : MC_MPEG2AUDIO;
  c.u.audioes.m_force_type = pro->pro_mc;
  c.u.audioes.m_index = pro->pro_index;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_audio_open(profile_chain_t *prch,
                   muxer_config_t *m_cfg,
                   muxer_hints_t *hints,
                   int flags, size_t qsize)
{
  int r;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  r = profile_htsp_work(prch, &prch->prch_sq.sq_st, 0);
  if (r) {
    profile_chain_close(prch);
    return r;
  }

  return profile_audio_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_audio_builder(void)
{
  profile_audio_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_reopen = profile_audio_reopen;
  pro->pro_open   = profile_audio_open;
  return (profile_t *)pro;
}


#if ENABLE_LIBAV

/*
 *  LibAV/MPEG-TS muxer
 */
typedef struct profile_libav_mpegts {
  profile_t;
} profile_libav_mpegts_t;

const idclass_t profile_libav_mpegts_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-libav-mpegts",
  .ic_caption    = N_("MPEG-TS/av-lib"),
  .ic_properties = (const property_t[]){
    { }
  }
};

static int
profile_libav_mpegts_reopen(profile_chain_t *prch,
                            muxer_config_t *m_cfg,
                            muxer_hints_t *hints, int flags)
{
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  c.m_type = MC_MPEGTS;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_libav_mpegts_open(profile_chain_t *prch,
                          muxer_config_t *m_cfg,
                          muxer_hints_t *hints,
                          int flags, size_t qsize)
{
  int r;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  r = profile_htsp_work(prch, &prch->prch_sq.sq_st, 0);
  if (r) {
    profile_chain_close(prch);
    return r;
  }

  return profile_libav_mpegts_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_libav_mpegts_builder(void)
{
  profile_libav_mpegts_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_reopen = profile_libav_mpegts_reopen;
  pro->pro_open   = profile_libav_mpegts_open;
  return (profile_t *)pro;
}

/*
 *  LibAV/Matroska muxer
 */
typedef struct profile_libav_matroska {
  profile_t;
  int pro_webm;
} profile_libav_matroska_t;

const idclass_t profile_libav_matroska_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-libav-matroska",
  .ic_caption    = N_("Matroska/av-lib"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Configuration"),
      .number = 1,
    },
    {
      .name   = N_("Matroska specific"),
      .number = 2,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "webm",
      .name     = N_("WEBM"),
      .desc     = N_("Use WEBM format."),
      .off      = offsetof(profile_libav_matroska_t, pro_webm),
      .opts     = PO_ADVANCED,
      .def.i    = 0,
      .group    = 2
    },
    { }
  }
};

static int
profile_libav_matroska_reopen(profile_chain_t *prch,
                              muxer_config_t *m_cfg,
                              muxer_hints_t *hints, int flags)
{
  profile_libav_matroska_t *pro = (profile_libav_matroska_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_AVWEBM)
    c.m_type = MC_AVMATROSKA;
  if (pro->pro_webm)
    c.m_type = MC_AVWEBM;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_libav_matroska_open(profile_chain_t *prch,
                            muxer_config_t *m_cfg,
                            muxer_hints_t *hints,
                            int flags, size_t qsize)
{
  int r;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  r = profile_htsp_work(prch, &prch->prch_sq.sq_st, 0);
  if (r) {
    profile_chain_close(prch);
    return r;
  }

  return profile_libav_matroska_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_libav_matroska_builder(void)
{
  profile_libav_matroska_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_reopen = profile_libav_matroska_reopen;
  pro->pro_open   = profile_libav_matroska_open;
  return (profile_t *)pro;
}

/*
 *  LibAV/MP4 muxer
 */
typedef struct profile_libav_mp4 {
  profile_t;
} profile_libav_mp4_t;

const idclass_t profile_libav_mp4_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-libav-mp4",
  .ic_caption    = N_("MP4/av-lib"),
};

static int
profile_libav_mp4_reopen(profile_chain_t *prch,
                         muxer_config_t *m_cfg,
                         muxer_hints_t *hints, int flags)
{
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (c.m_type != MC_AVMP4)
    c.m_type = MC_AVMP4;

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_libav_mp4_open(profile_chain_t *prch,
                       muxer_config_t *m_cfg,
                       muxer_hints_t *hints,
                       int flags, size_t qsize)
{
  int r;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  r = profile_htsp_work(prch, &prch->prch_sq.sq_st, 0);
  if (r) {
    profile_chain_close(prch);
    return r;
  }

  return profile_libav_mp4_reopen(prch, m_cfg, hints, flags);
}

static profile_t *
profile_libav_mp4_builder(void)
{
  profile_libav_mp4_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_reopen = profile_libav_mp4_reopen;
  pro->pro_open   = profile_libav_mp4_open;
  return (profile_t *)pro;
}

/*
 *  Transcoding + packet-like muxers
 */

typedef struct profile_transcode {
  profile_t;
  int   pro_mc;
#if ENABLE_LIBAV
  uint16_t pro_rewrite_sid;
  int   pro_rewrite_pmt;
  int   pro_rewrite_nit;
#endif
  char *pro_vcodec;
  char *pro_src_vcodec;
  char *pro_acodec;
  char *pro_src_acodec;
  char *pro_scodec;
  char *pro_src_scodec;
} profile_transcode_t;

#if ENABLE_LIBAV
static int
profile_transcode_rewrite_sid_set (void *in, const void *v)
{
  profile_transcode_t *pro = (profile_transcode_t *)in;
  const uint16_t *val = v;
  if (*val != pro->pro_rewrite_sid) {
    if (*val > 0) {
      pro->pro_rewrite_pmt = 1;
      pro->pro_rewrite_nit = 1;
    }
    pro->pro_rewrite_sid = *val;
    return 1;
  }
  return 0;
}

static int
profile_transcode_int_set (void *in, const void *v, int *prop)
{
  profile_transcode_t *pro = (profile_transcode_t *)in;
  int val = *(int *)v;
  if (pro->pro_rewrite_sid > 0) val = 1;
  if (val != *prop) {
    *prop = val;
    return 1;
  }
  return 0;
}

static int
profile_transcode_rewrite_pmt_set (void *in, const void *v)
{
  return profile_transcode_int_set(in, v, &((profile_transcode_t *)in)->pro_rewrite_pmt);
}

static int
profile_transcode_rewrite_nit_set (void *in, const void *v)
{
  return profile_transcode_int_set(in, v, &((profile_transcode_t *)in)->pro_rewrite_nit);
}
#endif

static htsmsg_t *
profile_class_mc_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Not set"),                      MC_UNKNOWN },
    { N_("Matroska (mkv)/built-in"),      MC_MATROSKA, },
    { N_("WEBM/built-in"),                MC_WEBM, },
    { N_("MPEG-TS/av-lib"),               MC_MPEGTS },
    { N_("MPEG-PS (DVD)/av-lib"),         MC_MPEGPS },
    { N_("Raw Audio Stream"),             MC_MPEG2AUDIO },
    { N_("Matroska (mkv)/av-lib"),        MC_AVMATROSKA },
    { N_("WEBM/av-lib"),                  MC_AVWEBM },
    { N_("MP4/av-lib"),                   MC_AVMP4 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static int
htsmsg_add_entry(htsmsg_t *list, const char *key, const char *val)
{
  htsmsg_t *map = NULL;

  if ((map = htsmsg_create_map())) {
    htsmsg_add_str(map, "key", key);
    htsmsg_add_str(map, "val", val);
    htsmsg_add_msg(list, NULL, map);
    return 0;
  }
  return -1;
}

static htsmsg_t *
profile_class_codec_profile_make_list(const char *lang)
{
  htsmsg_t *list = NULL;

  if ((list = htsmsg_create_list())) {
    if (htsmsg_add_entry(list, "", tvh_gettext_lang(lang, N_("Disabled"))) ||
      htsmsg_add_entry(list, "copy", tvh_gettext_lang(lang, N_("Copy")))) {
      htsmsg_destroy(list);
      list = NULL;
    }
  }
  return list;
}

static htsmsg_t *
profile_class_codec_profiles_list(enum AVMediaType media_type, const char *lang)
{
  htsmsg_t *list = NULL, *profiles = NULL, *map = NULL;
  htsmsg_field_t *field;

  if ((list = profile_class_codec_profile_make_list(lang)) &&
      (profiles = codec_get_profiles_list(media_type))) {
    HTSMSG_FOREACH(field, profiles) {
      if (!(map = htsmsg_detach_submsg(field))) {
        htsmsg_destroy(list);
        list = NULL;
        break;
      }
      htsmsg_add_msg(list, NULL, map);
    }
    htsmsg_destroy(profiles);
    profiles = NULL;
  }
  return list;
}

static htsmsg_t *
profile_class_pro_vcodec_list(void *o, const char *lang)
{
  return profile_class_codec_profiles_list(AVMEDIA_TYPE_VIDEO, lang);
}

static int
profile_class_src_codec_set(profile_transcode_t *pro,
                            char **str,
                            const void *p)
{
  char *s = htsmsg_list_2_csv((htsmsg_t *)p, ',', 0);
  int change = 0;
  if (s) {
    change = strcmp(*str ?: "", s) != 0;
    free(*str);
    *str = s;
  }
  return change;
}

static const void *
profile_class_src_codec_get(profile_transcode_t *pro,
                            char *str)
{
  return htsmsg_csv_2_list(str, ',');
}

static const struct strtab_str profile_class_src_vcodec_tab[] = {
  { "MPEG2VIDEO",            "MPEG2VIDEO" },
  { "H264",                  "H264" },
  { "VP8",                   "VP8" },
  { "HEVC",                  "HEVC" },
  { "VP9",                   "VP9" },
  { "THEORA",                "THEORA" },
};

static int
profile_class_src_vcodec_set ( void *obj, const void *p )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_set(pro, &pro->pro_src_vcodec, p);
}

static const void *
profile_class_src_vcodec_get ( void *obj )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_get(pro, pro->pro_src_vcodec);
}

static htsmsg_t *
profile_class_src_vcodec_list ( void *o, const char *lang )
{
  return strtab2htsmsg_str(profile_class_src_vcodec_tab, 1, lang);
}

static htsmsg_t *
profile_class_pro_acodec_list(void *o, const char *lang)
{
  return profile_class_codec_profiles_list(AVMEDIA_TYPE_AUDIO, lang);
}

static const struct strtab_str profile_class_src_acodec_tab[] = {
  { "MPEG2AUDIO",            "MPEG2AUDIO" },
  { "AC3",                   "AC3" },
  { "AAC",                   "AAC" },
  { "MP4A",                  "MP4A" },
  { "EAC3",                  "EAC3" },
  { "VORBIS",                "VORBIS" },
  { "OPUS",                  "OPUS" },
  { "AC-4",                  "AC-4" },
};

static int
profile_class_src_acodec_set ( void *obj, const void *p )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_set(pro, &pro->pro_src_acodec, p);
}

static const void *
profile_class_src_acodec_get ( void *obj )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_get(pro, pro->pro_src_acodec);
}

static htsmsg_t *
profile_class_src_acodec_list ( void *o, const char *lang )
{
  return strtab2htsmsg_str(profile_class_src_acodec_tab, 1, lang);
}

static htsmsg_t *
profile_class_pro_scodec_list(void *o, const char *lang)
{
  return profile_class_codec_profiles_list(AVMEDIA_TYPE_SUBTITLE, lang);
}

static const struct strtab_str profile_class_src_scodec_tab[] = {
  { "DVBSUB",                "DVBSUB" },
  { "TEXTSUB",               "TEXTSUB" },
};

static int
profile_class_src_scodec_set ( void *obj, const void *p )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_set(pro, &pro->pro_src_scodec, p);
}

static const void *
profile_class_src_scodec_get ( void *obj )
{
  profile_transcode_t *pro = (profile_transcode_t *)obj;
  return profile_class_src_codec_get(pro, pro->pro_src_scodec);
}

static htsmsg_t *
profile_class_src_scodec_list ( void *o, const char *lang )
{
  return strtab2htsmsg_str(profile_class_src_scodec_tab, 1, lang);
}

const idclass_t profile_transcode_class =
{
  .ic_super      = &profile_class,
  .ic_class      = "profile-transcode",
  .ic_caption    = N_("Transcode/av-lib"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Transcoding Settings"),
      .number = 2,
    },
#if ENABLE_LIBAV
    {
      .name   = N_("Rewrite MPEG-TS SI Table(s) Settings"),
      .number = 3,
    },
#endif
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "container",
      .name     = N_("Container"),
      .desc     = N_("Container to use for the transcoded stream."),
      .off      = offsetof(profile_transcode_t, pro_mc),
      .def.i    = MC_MATROSKA,
      .list     = profile_class_mc_list,
      .opts     = PO_DOC_NLIST,
      .group    = 1
    },
#if ENABLE_LIBAV
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Rewrite Service ID"),
      .desc     = N_("Rewrite service identifier (SID) using the specified "
                     "value (usually 1). Zero means no rewrite; preserving "
                     "MPEG-TS original network and transport stream IDs"),
      .off      = offsetof(profile_transcode_t, pro_rewrite_sid),
      .set      = profile_transcode_rewrite_sid_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_pmt",
      .name     = N_("Rewrite PMT"),
      .desc     = N_("Rewrite PMT (Program Map Table) packets to only "
                     "include information about the currently-streamed "
                     "service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_transcode_t, pro_rewrite_pmt),
      .set      = profile_transcode_rewrite_pmt_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
    {
      .type     = PT_BOOL,
      .id       = "rewrite_nit",
      .name     = N_("Rewrite NIT"),
      .desc     = N_("Rewrite NIT (Network Information Table) packets "
                     "to only include information about the currently-"
                     "streamed service. "
                     "Rewrite can be unset only if 'Rewrite Service ID' "
                     "is set to zero."),
      .off      = offsetof(profile_transcode_t, pro_rewrite_nit),
      .set      = profile_transcode_rewrite_nit_set,
      .opts     = PO_EXPERT,
      .def.i    = 1,
      .group    = 3
    },
#endif
    {
      .type     = PT_STR,
      .id       = "pro_vcodec",
      .name     = N_("Video codec profile"),
      .desc     = N_("Select video codec profile to use for transcoding."),
      .off      = offsetof(profile_transcode_t, pro_vcodec),
      .list     = profile_class_pro_vcodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "src_vcodec",
      .name     = N_("Source video codec"),
      .desc     = N_("Transcode video only for selected codecs."),
      .get      = profile_class_src_vcodec_get,
      .set      = profile_class_src_vcodec_set,
      .list     = profile_class_src_vcodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "pro_acodec",
      .name     = N_("Audio codec profile"),
      .desc     = N_("Select audio codec profile to use for transcoding."),
      .off      = offsetof(profile_transcode_t, pro_acodec),
      .list     = profile_class_pro_acodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "src_acodec",
      .name     = N_("Source audio codec"),
      .desc     = N_("Transcode audio only for selected codecs."),
      .get      = profile_class_src_acodec_get,
      .set      = profile_class_src_acodec_set,
      .list     = profile_class_src_acodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .id       = "pro_scodec",
      .name     = N_("Subtitle codec profile"),
      .desc     = N_("Select subtitle codec profile to use for transcoding."),
      .off      = offsetof(profile_transcode_t, pro_scodec),
      .list     = profile_class_pro_scodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "src_scodec",
      .name     = N_("Source subtitle codec"),
      .desc     = N_("Transcode subtitle only for selected codecs."),
      .get      = profile_class_src_scodec_get,
      .set      = profile_class_src_scodec_set,
      .list     = profile_class_src_scodec_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
      .group    = 2
    },
    { }
  }
};

static int
profile_transcode_can_share(profile_chain_t *prch,
                            profile_chain_t *joiner)
{
  profile_transcode_t *pro1 = (profile_transcode_t *)prch->prch_pro;
  profile_transcode_t *pro2 = (profile_transcode_t *)joiner->prch_pro;
  if (pro1 == pro2)
    return 1;
  if (!idnode_is_instance(&pro2->pro_id, &profile_transcode_class))
    return 0;
  /*
   * Do full params check here, note that profiles might differ
   * only in the muxer setup.
   */
  if (strcmp(pro1->pro_vcodec ?: "", pro2->pro_vcodec ?: ""))
    return 0;
  if (strcmp(pro1->pro_src_vcodec ?: "", pro2->pro_src_vcodec ?: ""))
    return 0;
  if (strcmp(pro1->pro_acodec ?: "", pro2->pro_acodec ?: ""))
    return 0;
  if (strcmp(pro1->pro_src_acodec ?: "", pro2->pro_src_acodec ?: ""))
    return 0;
  if (strcmp(pro1->pro_scodec ?: "", pro2->pro_scodec ?: ""))
    return 0;
  if (strcmp(pro1->pro_src_scodec ?: "", pro2->pro_src_scodec ?: ""))
    return 0;
  return 1;
}

static int
profile_transcode_work(profile_chain_t *prch,
                       streaming_target_t *dst,
                       uint32_t timeshift_period)
{
  profile_sharer_t *prsh;
  profile_transcode_t *pro = (profile_transcode_t *)prch->prch_pro;
  const char *profiles[AVMEDIA_TYPE_NB] = { NULL };
  const char *src_codecs[AVMEDIA_TYPE_NB] = { NULL };

  prsh = profile_sharer_find(prch);
  if (!prsh)
    goto fail;

  prch->prch_can_share = profile_transcode_can_share;

  profiles[AVMEDIA_TYPE_VIDEO] = pro->pro_vcodec ?: "";
  profiles[AVMEDIA_TYPE_AUDIO] = pro->pro_acodec ?: "";
  profiles[AVMEDIA_TYPE_SUBTITLE] = pro->pro_scodec ?: "";

  src_codecs[AVMEDIA_TYPE_VIDEO] = pro->pro_src_vcodec ?: "";
  src_codecs[AVMEDIA_TYPE_AUDIO] = pro->pro_src_acodec ?: "";
  src_codecs[AVMEDIA_TYPE_SUBTITLE] = pro->pro_src_scodec ?: "";

  dst = prch->prch_gh = globalheaders_create(dst);

#if ENABLE_TIMESHIFT
  if (timeshift_period > 0)
    dst = prch->prch_timeshift = timeshift_create(dst, timeshift_period);
#endif
  if (profile_sharer_create(prsh, prch, dst))
    goto fail;
  if (!prsh->prsh_transcoder) {
    assert(!prsh->prsh_tsfix);
    dst = prsh->prsh_transcoder = transcoder_create(&prsh->prsh_input,
                                                    profiles, src_codecs);
    if (!dst)
      goto fail;
    prsh->prsh_tsfix = tsfix_create(dst);
  }
  prch->prch_share = prsh->prsh_tsfix;
  streaming_target_init(&prch->prch_input,
                        prsh->prsh_do_queue ?
                          &profile_input_queue_ops : &profile_input_ops, prch,
                        0);
  prch->prch_st = &prch->prch_input;
  if (profile_sharer_postinit(prsh))
    goto fail;
  return 0;
fail:
  profile_chain_close(prch);
  return -1;
}

static int
profile_transcode_mc_valid(int mc)
{
  switch (mc) {
  case MC_MATROSKA:
  case MC_WEBM:
  case MC_MPEGTS:
  case MC_MPEGPS:
  case MC_MPEG2AUDIO:
  case MC_AC3:
  case MC_AC4:
  case MC_AAC:
  case MC_VORBIS:
  case MC_AVMATROSKA:
  case MC_AVMP4:
    return 1;
  default:
    return 0;
  }
}

static int
profile_transcode_reopen(profile_chain_t *prch,
                         muxer_config_t *m_cfg,
                         muxer_hints_t *hints, int flags)
{
  profile_transcode_t *pro = (profile_transcode_t *)prch->prch_pro;
  muxer_config_t c;

  if (m_cfg)
    muxer_config_copy(&c, m_cfg); /* do not alter the original parameter */
  else
    memset(&c, 0, sizeof(c));
  if (!profile_transcode_mc_valid(c.m_type)) {
    c.m_type = pro->pro_mc;
    if (!profile_transcode_mc_valid(c.m_type))
      c.m_type = MC_MATROSKA;
  }
  #if ENABLE_LIBAV
  c.u.transcode.m_rewrite_sid = pro->pro_rewrite_sid;
  c.u.transcode.m_rewrite_pmt = pro->pro_rewrite_pmt;
  c.u.transcode.m_rewrite_nit = pro->pro_rewrite_nit;
  #endif

  assert(!prch->prch_muxer);
  prch->prch_muxer = muxer_create(&c, hints);
  return 0;
}

static int
profile_transcode_open(profile_chain_t *prch,
                       muxer_config_t *m_cfg,
                       muxer_hints_t *hints,
                       int flags, size_t qsize)
{
  int r;

  prch->prch_can_share = profile_transcode_can_share;

  prch->prch_flags = SUBSCRIPTION_PACKET;
  prch->prch_sq.sq_maxsize = qsize;

  r = profile_transcode_work(prch, &prch->prch_sq.sq_st, 0);
  if (r) {
    profile_chain_close(prch);
    return r;
  }

  return profile_transcode_reopen(prch, m_cfg, hints, flags);
}

static void
profile_transcode_free(profile_t *_pro)
{
  profile_transcode_t *pro = (profile_transcode_t *)_pro;
  free(pro->pro_vcodec);
  free(pro->pro_src_vcodec);
  free(pro->pro_acodec);
  free(pro->pro_src_acodec);
  free(pro->pro_scodec);
  free(pro->pro_src_scodec);
}

static profile_t *
profile_transcode_builder(void)
{
  profile_transcode_t *pro = calloc(1, sizeof(*pro));
  pro->pro_sflags = SUBSCRIPTION_PACKET;
  pro->pro_free   = profile_transcode_free;
  pro->pro_work   = profile_transcode_work;
  pro->pro_reopen = profile_transcode_reopen;
  pro->pro_open   = profile_transcode_open;
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
  LIST_INIT(&profile_chains);

  profile_register(&profile_mpegts_pass_class, profile_mpegts_pass_builder);
  profile_register(&profile_mpegts_spawn_class, profile_mpegts_spawn_builder);
  profile_register(&profile_matroska_class, profile_matroska_builder);
  profile_register(&profile_htsp_class, profile_htsp_builder);
  profile_register(&profile_audio_class, profile_audio_builder);
#if ENABLE_LIBAV
  profile_register(&profile_libav_mpegts_class, profile_libav_mpegts_builder);
  profile_register(&profile_libav_matroska_class, profile_libav_matroska_builder);
  profile_register(&profile_libav_mp4_class, profile_libav_mp4_builder);
  profile_register(&profile_transcode_class, profile_transcode_builder);
#endif

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))
        continue;
      (void)profile_create(htsmsg_field_name(f), e, 0);
    }
    htsmsg_destroy(c);
  }

  if ((c = hts_settings_load("profiles")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))
        continue;
      if ((name = htsmsg_get_str(e, "name")) == NULL)
        continue;
      pro = profile_find_by_name2(name, NULL, 1);
      if (pro == NULL || strcmp(profile_get_name(pro), name)) {
        if (!htsmsg_field_find(e, "enabled"))
          htsmsg_add_bool(e, "enabled", 1);
        if (!htsmsg_field_find(e, "priority"))
          htsmsg_add_s32(e, "priority", PROFILE_SPRIO_NORMAL);
        if (!htsmsg_field_find(e, "shield"))
          htsmsg_add_bool(e, "shield", 1);
        (void)profile_create(NULL, e, 1);
      }
    }
    htsmsg_destroy(c);
  }

#if ENABLE_LIBAV
  if ((c = hts_settings_load("transcoder/profiles")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))
        continue;
      if ((name = htsmsg_get_str(e, "name")) == NULL)
        continue;
      pro = profile_find_by_name2(name, NULL, 1);
      if (pro == NULL || strcmp(profile_get_name(pro), name)) {
transcoder_create:
        htsmsg_add_str(e, "class", "profile-transcode");
        if (!htsmsg_field_find(e, "enabled"))
          htsmsg_add_bool(e, "enabled", 1);
        if (!htsmsg_field_find(e, "priority"))
          htsmsg_add_s32(e, "priority", PROFILE_SPRIO_NORMAL);
        if (!htsmsg_field_find(e, "shield"))
          htsmsg_add_bool(e, "shield", 1);
        (void)profile_create(NULL, e, 1);
      } else if (pro && idnode_is_instance(&pro->pro_id, &profile_transcode_class)) {
        profile_transcode_t *prot = (profile_transcode_t *)pro;
        if (tvh_str_default(prot->pro_vcodec, NULL) == NULL) {
          profile_delete(pro, 1);
          goto transcoder_create;
        }
      }
    }
    htsmsg_destroy(c);
  }
#endif

  /* Assign the default profile if config files are corrupted */
  if (!profile_default) {
    pro = profile_find_by_name2("pass", NULL, 1);
    if (pro == NULL)
      tvhabort(LS_PROFILE, "no default streaming profile! reinstall data files");
    profile_default = pro;
  }
}

void
profile_done(void)
{
  profile_t *pro;
  profile_build_t *pb;

  tvh_mutex_lock(&global_lock);
  profile_default = NULL;
  while ((pro = TAILQ_FIRST(&profiles)) != NULL)
    profile_delete(pro, 0);
  while ((pb = LIST_FIRST(&profile_builders)) != NULL) {
    LIST_REMOVE(pb, link);
    free(pb);
  }
  tvh_mutex_unlock(&global_lock);
}
