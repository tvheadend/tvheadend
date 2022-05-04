/*
 *  Services
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

#include "tvheadend.h"
#include "service.h"
#include "subscriptions.h"
#include "streaming.h"
#include "packet.h"
#include "channels.h"
#include "notify.h"
#include "service_mapper.h"
#include "atomic.h"
#include "htsp_server.h"
#include "lang_codes.h"
#include "descrambler/descrambler.h"
#include "descrambler/caid.h"
#include "input.h"
#include "access.h"
#include "esfilter.h"
#include "bouquet.h"
#include "memoryinfo.h"
#include "config.h"

static void service_data_timeout(void *aux);
static void service_class_delete(struct idnode *self);
static htsmsg_t *service_class_save(struct idnode *self, char *filename, size_t fsize);
static void service_class_load(struct idnode *self, htsmsg_t *conf);
static int service_make_nicename0(service_t *t, char *buf, size_t len, int adapter);

struct service_queue service_all;
struct service_queue service_raw_all;
struct service_queue service_raw_remove;

static mtimer_t service_raw_remove_timer;

static void
service_class_notify_enabled ( void *obj, const char *lang )
{
  service_t *t = (service_t *)obj;
  if (t->s_enabled && t->s_auto != SERVICE_AUTO_OFF)
    t->s_auto = SERVICE_AUTO_NORMAL;
  bouquet_notify_service_enabled(t);
  if (!t->s_enabled)
    service_remove_subscriber(t, NULL, SM_CODE_SVC_NOT_ENABLED);
}

static const void *
service_class_channel_get ( void *obj )
{
  service_t *svc = obj;
  return idnode_list_get1(&svc->s_channels);
}

static char *
service_class_channel_rend ( void *obj, const char *lang )
{
  service_t *svc = obj;
  return idnode_list_get_csv1(&svc->s_channels, lang);
}

static int
service_class_channel_set
  ( void *obj, const void *p )
{
  service_t *svc = obj;
  return idnode_list_set1(&svc->s_id, &svc->s_channels,
                          &channel_class, (htsmsg_t *)p,
                          service_mapper_create);
}

static void
service_class_get_title
  ( idnode_t *self, const char *lang, char *dst, size_t dstsize )
{
  snprintf(dst, dstsize, "%s",
           service_get_full_channel_name((service_t *)self) ?: "");
}

static const void *
service_class_encrypted_get ( void *p )
{
  static int t;
  service_t *s = p;
  tvh_mutex_lock(&s->s_stream_mutex);
  t = service_is_encrypted(s);
  tvh_mutex_unlock(&s->s_stream_mutex);
  return &t;
}

static const void *
service_class_caid_get ( void *obj )
{
  static char buf[256], *s = buf;
  service_t *svc = obj;
  elementary_stream_t *st;
  caid_t *c;
  size_t l;

  buf[0] = '\0';
  TAILQ_FOREACH(st, &svc->s_components.set_all, es_link) {
    switch(st->es_type) {
    case SCT_CA:
      LIST_FOREACH(c, &st->es_caids, link) {
        l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "%s%04X:%06X",
                 l ? "," : "", c->caid, c->providerid);
      }
      break;
    default:
      break;
    }
  }
  return &s;
}

static htsmsg_t *
service_class_auto_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Auto check enabled"),  0 },
    { N_("Auto check disabled"), 1 },
    { N_("Missing In PAT/SDT"),  2 }
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
service_type_auto_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Override disabled"), ST_UNSET },
    { N_("None"),              ST_NONE  },
    { N_("Radio"),             ST_RADIO },
    { N_("SD TV"),             ST_SDTV  },
    { N_("HD TV"),             ST_HDTV  },
    { N_("FHD TV"),            ST_FHDTV },
    { N_("UHD TV"),            ST_UHDTV }
  };
  return strtab2htsmsg(tab, 1, lang);
}

PROP_DOC(servicechecking)

const idclass_t service_class = {
  .ic_class      = "service",
  .ic_caption    = N_("Service"),
  .ic_event      = "service",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_delete     = service_class_delete,
  .ic_save       = service_class_save,
  .ic_load       = service_class_load,
  .ic_get_title  = service_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/Disable service."),
      .off      = offsetof(service_t, s_enabled),
      .notify   = service_class_notify_enabled,
    },
    {
      .type     = PT_INT,
      .id       = "auto",
      .name     = N_("Automatic checking"),
      .desc     = N_("Check for the services' presence. If the service is no "
                     "longer broadcast this field will change to "
                     "Missing In PAT/SDT."),
      .doc      = prop_doc_servicechecking,
      .list     = service_class_auto_list,
      .off      = offsetof(service_t, s_auto),
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channel",
      .name     = N_("Channel"),
      .desc     = N_("The channel this service is mapped to."),
      .get      = service_class_channel_get,
      .set      = service_class_channel_set,
      .list     = channel_class_get_list,
      .rend     = service_class_channel_rend,
      .opts     = PO_NOSAVE
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority (-10..10)"),
      .desc     = N_("Service priority. Enter a value between -10 and "
                     "10. A higher value indicates a higher preference. "
                     "See Help for more info."),
      .off      = offsetof(service_t, s_prio),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "encrypted",
      .name     = N_("Encrypted"),
      .desc     = N_("The service's encryption status."),
      .get      = service_class_encrypted_get,
      .opts     = PO_NOSAVE | PO_RDONLY
    },
    {
      .type     = PT_STR,
      .id       = "caid",
      .name     = N_("CAID"),
      .desc     = N_("The Conditional Access ID used for the service."),
      .get      = service_class_caid_get,
      .opts     = PO_NOSAVE | PO_RDONLY | PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_INT,
      .id       = "s_type_user",
      .name     = N_("Type override"),
      .desc     = N_("Service type override. This value will override the "
                     "service type provided by the stream."),
      .list     = service_type_auto_list,
      .off      = offsetof(service_t, s_type_user),
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {}
  }
};

const idclass_t service_raw_class = {
  .ic_class      = "service_raw",
  .ic_caption    = N_("Service raw"),
  .ic_event      = "service_raw",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_delete     = service_class_delete,
  .ic_save       = NULL,
  .ic_get_title  = service_class_get_title,
  .ic_properties = NULL
};

/**
 * Service lock must be held
 */
void
service_stop(service_t *t)
{
  mtimer_disarm(&t->s_receive_timer);

  t->s_stop_feed(t);

  descrambler_service_stop(t);

  tvh_mutex_lock(&t->s_stream_mutex);

  t->s_tt_commercial_advice = COMMERCIAL_UNKNOWN;

  /**
   * Clean up each stream
   */
  elementary_set_clean_streams(&t->s_components);

  t->s_status = SERVICE_IDLE;
  tvhlog_limit_reset(&t->s_tei_log);

  tvh_mutex_unlock(&t->s_stream_mutex);
}


/**
 * Remove the given subscriber from the service
 *
 * if s == NULL all subscribers will be removed
 *
 * Global lock must be held
 */
void
service_remove_subscriber(service_t *t, th_subscription_t *s,
                          int reason)
{
  lock_assert(&global_lock);
  th_subscription_t *s_next;

  if(s == NULL) {
    for (s = LIST_FIRST(&t->s_subscriptions); s; s = s_next) {
      s_next = LIST_NEXT(s, ths_service_link);
      if (reason == SM_CODE_SVC_NOT_ENABLED && s->ths_channel == NULL)
        continue; /* not valid for raw service subscriptions */
      subscription_unlink_service(s, reason);
    }
  } else {
    subscription_unlink_service(s, reason);
  }
}

/**
 *
 */
int
service_start(service_t *t, int instance, int weight, int flags,
              int timeout, int postpone)
{
  int r, stimeout = 10;

  lock_assert(&global_lock);

  tvhtrace(LS_SERVICE, "starting %s", t->s_nicename);

  assert(t->s_status != SERVICE_RUNNING);
  t->s_streaming_status = 0;
  t->s_streaming_live   = 0;
  t->s_scrambled_seen   = 0;
  t->s_scrambled_pass   = !!(flags & SUBSCRIPTION_NODESCR);
  t->s_start_time       = mclk();

  tvh_mutex_lock(&t->s_stream_mutex);
  elementary_set_filter_build(&t->s_components);
  tvh_mutex_unlock(&t->s_stream_mutex);

  descrambler_caid_changed(t);

  if((r = t->s_start_feed(t, instance, weight, flags)))
    return r;

  descrambler_service_start(t);

  tvh_mutex_lock(&t->s_stream_mutex);

  t->s_status = SERVICE_RUNNING;

  /**
   * Initialize stream
   */
  elementary_set_init_filter_streams(&t->s_components);

  tvh_mutex_unlock(&t->s_stream_mutex);

  if(t->s_grace_period != NULL)
    stimeout = t->s_grace_period(t);

  stimeout += postpone;
  t->s_timeout = timeout;
  t->s_grace_delay = stimeout;
  if (stimeout > 0)
    mtimer_arm_rel(&t->s_receive_timer, service_data_timeout, t,
                   sec2mono(stimeout));
  return 0;
}


/**
 * Main entry point for starting a service based on a channel
 */
service_instance_t *
service_find_instance
  (service_t *s, channel_t *ch, tvh_input_t *ti,
   profile_chain_t *prch, service_instance_list_t *sil,
   int *error, int weight, int flags, int timeout, int postpone)
{
  idnode_list_mapping_t *ilm;
  service_instance_t *si, *next;
  profile_t *pro = prch ? prch->prch_pro : NULL;
  int r, r1;

  lock_assert(&global_lock);

  /* Build list */
  r = 0;
  if (flags & SUBSCRIPTION_SWSERVICE) {
    TAILQ_FOREACH(si, sil, si_link) {
      next = TAILQ_NEXT(si, si_link);
      if (next && next->si_s != si->si_s) {
        r++;
        break;
      }
    }
  }
  TAILQ_FOREACH(si, sil, si_link) {
    si->si_mark = 1;
    if (r && (flags & SUBSCRIPTION_SWSERVICE) != 0) {
      TAILQ_FOREACH(next, sil, si_link)
        if (next != si && si->si_s == next->si_s && si->si_error)
          next->si_error = MAX(next->si_error, si->si_error);
    }
  }

  r = 0;
  if (ch) {
    if (!ch->ch_enabled) {
      *error = SM_CODE_SVC_NOT_ENABLED;
      return NULL;
    }
    LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
      s = (service_t *)ilm->ilm_in1;
      if (s->s_is_enabled(s, flags)) {
        if (pro == NULL ||
            pro->pro_svfilter == PROFILE_SVF_NONE ||
            (pro->pro_svfilter == PROFILE_SVF_SD && service_is_sdtv(s)) ||
            (pro->pro_svfilter == PROFILE_SVF_HD && service_is_hdtv(s)) ||
            (pro->pro_svfilter == PROFILE_SVF_FHD && service_is_fhdtv(s)) ||
            (pro->pro_svfilter == PROFILE_SVF_UHD && service_is_uhdtv(s))) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
    }
    /* find a valid instance, no error and "user" idle */
    TAILQ_FOREACH(si, sil, si_link)
      if (si->si_weight < SUBSCRIPTION_PRIO_MIN && si->si_error == 0) break;
    /* SD->HD->FHD->UHD fallback */
    if (si == NULL && pro &&
        pro->pro_svfilter == PROFILE_SVF_SD) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_hdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_fhdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_uhdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      /* find a valid instance, no error and "user" idle */
      TAILQ_FOREACH(si, sil, si_link)
        if (si->si_weight < SUBSCRIPTION_PRIO_MIN && si->si_error == 0) break;
    }
    /* UHD->FHD->HD->SD fallback */
    if (si == NULL && pro &&
        pro->pro_svfilter == PROFILE_SVF_UHD) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_fhdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_hdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags) && service_is_sdtv(s)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
      /* find a valid instance, no error and "user" idle */
      TAILQ_FOREACH(si, sil, si_link)
        if (si->si_weight < SUBSCRIPTION_PRIO_MIN && si->si_error == 0) break;
    }
    /* fallback, enlist all instances */
    if (si == NULL) {
      LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
        s = (service_t *)ilm->ilm_in1;
        if (s->s_is_enabled(s, flags)) {
          r1 = s->s_enlist(s, ti, sil, flags, weight);
          if (r1 && r == 0)
            r = r1;
        }
      }
    }
    TAILQ_FOREACH(si, sil, si_link)
      if (!si->si_error) {
        r = 0;
        break;
      }
  } else {
    if (!s->s_is_enabled(s, flags)) {
      *error = SM_CODE_SVC_NOT_ENABLED;
      return NULL;
    }
    r = s->s_enlist(s, ti, sil, flags, weight);
  }

  /* Clean */
  for(si = TAILQ_FIRST(sil); si != NULL; si = next) {
    next = TAILQ_NEXT(si, si_link);
    if(si->si_mark)
      service_instance_destroy(sil, si);
  }

  /* Error handling */
  if (r) {
    if (*error < r)
      *error = r;
    return NULL;
  }

  if (TAILQ_EMPTY(sil)) {
    if (*error < SM_CODE_NO_ADAPTERS)
      *error = SM_CODE_NO_ADAPTERS;
    return NULL;
  }

  /* Debug */
  TAILQ_FOREACH(si, sil, si_link) {
    const char *name = ch ? channel_get_name(ch, NULL) : NULL;
    if (!name && s) name = s->s_nicename;
    tvhdebug(LS_SERVICE, "%d: %s si %p %s weight %d prio %d error %d (%s)",
             si->si_instance, name, si, si->si_source, si->si_weight, si->si_prio,
             si->si_error, streaming_code2txt(si->si_error));
  }

  /* Already running? */
  TAILQ_FOREACH(si, sil, si_link)
    if (si->si_s->s_status == SERVICE_RUNNING && si->si_error == 0) {
      tvhtrace(LS_SERVICE, "return already running %p", si);
      return si;
    }

  /* Forced, handle priority settings */
  si = NULL;
  TAILQ_FOREACH(next, sil, si_link)
    if (next->si_weight < 0 && next->si_error == 0)
      if (si == NULL || next->si_prio > si->si_prio)
        si = next;

  /* Idle */
  if (!si) {
    TAILQ_FOREACH_REVERSE(si, sil, service_instance_list, si_link)
      if (si->si_weight == 0 && si->si_error == 0)
        break;
  }

  /* Bump the one with lowest weight or bigger priority */
  if (!si) {
    next = NULL;
    TAILQ_FOREACH(si, sil, si_link) {
      if (si->si_error) continue;
      if (next == NULL) {
        if (si->si_weight < weight)
          next = si;
      } else {
        if ((si->si_weight < next->si_weight) ||
            (si->si_weight == next->si_weight && si->si_prio > next->si_prio))
          next = si;
      }
    }
    si = next;
  }

  /* Failed */
  if(si == NULL) {
    if (*error < SM_CODE_NO_FREE_ADAPTER)
      *error = SM_CODE_NO_FREE_ADAPTER;
    return NULL;
  }

  /* Start */
  tvhtrace(LS_SERVICE, "will start new instance %d", si->si_instance);
  if (service_start(si->si_s, si->si_instance, weight, flags, timeout, postpone)) {
    tvhtrace(LS_SERVICE, "tuning failed");
    si->si_error = SM_CODE_TUNING_FAILED;
    if (*error < SM_CODE_TUNING_FAILED)
      *error = SM_CODE_TUNING_FAILED;
    si = NULL;
  }
  return si;
}


/**
 *
 */
void
service_unref(service_t *t)
{
  if((atomic_add(&t->s_refcount, -1)) == 1) {
    if (t->s_unref)
      t->s_unref(t);
    free(t->s_nicename);
    free(t);
  }
}


/**
 *
 */
void
service_ref(service_t *t)
{
  atomic_add(&t->s_refcount, 1);
}



/**
 * Destroy a service
 */
void
service_destroy(service_t *t, int delconf)
{
  th_subscription_t *s;
  idnode_list_mapping_t *ilm;

  lock_assert(&global_lock);

  idnode_save_check(&t->s_id, delconf);

  if(t->s_delete != NULL)
    t->s_delete(t, delconf);

  service_mapper_remove(t);

  while((s = LIST_FIRST(&t->s_subscriptions)) != NULL)
    subscription_unlink_service(s, SM_CODE_SOURCE_DELETED);

  bouquet_destroy_by_service(t, delconf);

  while ((ilm = LIST_FIRST(&t->s_channels)))
    idnode_list_unlink(ilm, delconf ? t : NULL);

  idnode_unlink(&t->s_id);

  assert(t->s_status == SERVICE_IDLE);

  t->s_status = SERVICE_ZOMBIE;

  elementary_set_clean(&t->s_components, NULL, 0);

  if (t->s_hbbtv) {
    htsmsg_destroy(t->s_hbbtv);
    t->s_hbbtv = NULL;
  }

  switch (t->s_type) {
  case STYPE_RAW:
    TAILQ_REMOVE(&service_raw_all, t, s_all_link);
    break;
  case STYPE_RAW_REMOVED:
    TAILQ_REMOVE(&service_raw_remove, t, s_all_link);
    break;
  default:
    TAILQ_REMOVE(&service_all, t, s_all_link);
  }

  service_unref(t);
}

static void
service_remove_raw_timer_cb(void *aux)
{
  service_t *t;
  while ((t = TAILQ_FIRST(&service_raw_remove)) != NULL)
    service_destroy(t, 0);
}

void
service_remove_raw(service_t *t)
{
  if (t == NULL) return;
  assert(t->s_type == STYPE_RAW);
  t->s_type = STYPE_RAW_REMOVED;
  TAILQ_REMOVE(&service_raw_all, t, s_all_link);
  TAILQ_INSERT_TAIL(&service_raw_remove, t, s_all_link);
  mtimer_arm_rel(&service_raw_remove_timer, service_remove_raw_timer_cb, NULL, 0);
}

void
service_set_enabled(service_t *t, int enabled, int _auto)
{
  if (t->s_enabled != !!enabled) {
    t->s_enabled = !!enabled;
    t->s_auto = _auto;
    service_class_notify_enabled(t, NULL);
    service_request_save(t);
    idnode_notify_changed(&t->s_id);
  }
}

static int64_t
service_channel_number ( service_t *s )
{
  return 0;
}

static const char *
service_channel_name ( service_t *s )
{
  return NULL;
}

static const char *
service_provider_name ( service_t *s )
{
  return NULL;
}

void
service_memoryinfo ( service_t *s, int64_t *size )
{
  *size += sizeof(*s);
  *size += tvh_strlen(s->s_nicename);
}

/**
 * Create and initialize a new service struct
 */
service_t *
service_create0
  ( service_t *t, int service_type,
    const idclass_t *class, const char *uuid,
    int source_type, htsmsg_t *conf )
{
  lock_assert(&global_lock);

  if (idnode_insert(&t->s_id, uuid, class, 0)) {
    if (uuid)
      tvherror(LS_SERVICE, "invalid uuid '%s'", uuid);
    free(t);
    return NULL;
  }

  if (service_type == STYPE_RAW)
    TAILQ_INSERT_TAIL(&service_raw_all, t, s_all_link);
  else
    TAILQ_INSERT_TAIL(&service_all, t, s_all_link);

  tvh_mutex_init(&t->s_stream_mutex, NULL);
  t->s_type = service_type;
  t->s_type_user = ST_UNSET;
  t->s_source_type = source_type;
  t->s_refcount = 1;
  t->s_enabled = 1;
  t->s_channel_number = service_channel_number;
  t->s_channel_name   = service_channel_name;
  t->s_provider_name  = service_provider_name;
  t->s_memoryinfo     = service_memoryinfo;
  elementary_set_init(&t->s_components, LS_SERVICE, NULL, t);

  streaming_pad_init(&t->s_streaming_pad);

  /* Load config */
  if (conf)
    service_load(t, conf);

  return t;
}

/**
 *
 */
static int
service_make_nicename0(service_t *t, char *buf, size_t len, int adapter)
{
  char buf2[16];
  source_info_t si;
  char *service_name;
  int prefidx;

  lock_assert(&t->s_stream_mutex);

  t->s_setsourceinfo(t, &si);

  service_name = si.si_service;
  if (service_name == NULL || si.si_service[0] == '0') {
    snprintf(buf2, sizeof(buf2), "{PMT:%d}", t->s_components.set_pmt_pid);
    service_name = buf2;
  }

  snprintf(buf, len,
	   "%s%s%s%s%s%s%s",
	   adapter && si.si_adapter ? si.si_adapter : "",
	   adapter && si.si_adapter && si.si_network ? "/" : "",
	   si.si_network ?: "", si.si_network && si.si_mux     ? "/" : "",
	   si.si_mux     ?: "", si.si_mux     && service_name  ? "/" : "",
	   service_name ?: "");
  prefidx = (adapter && si.si_adapter ? strlen(si.si_adapter) : 0) +
            (adapter && si.si_adapter && si.si_network ? 1 : 0) +
            (si.si_network ? strlen(si.si_network) : 0) +
            (si.si_network && si.si_mux ? 1 : 0) +
            (si.si_mux ? strlen(si.si_mux) : 0);

  service_source_info_free(&si);

  return prefidx;
}

/**
 *
 */
const char *
service_make_nicename(service_t *t)
{
  int prefidx;
  char buf[256];

  prefidx = service_make_nicename0(t, buf, sizeof(buf), 0);

  free(t->s_nicename);
  t->s_nicename = strdup(buf);
  t->s_nicename_prefidx = prefidx;

  elementary_set_update_nicename(&t->s_components, t->s_nicename);

  return t->s_nicename;
}

/**
 *
 */
static void
service_data_timeout(void *aux)
{
  service_t *t = aux;
  int flags = 0;

  tvh_mutex_lock(&t->s_stream_mutex);

  if(!(t->s_streaming_status & TSS_PACKETS))
    flags |= TSS_GRACEPERIOD;
  if(!(t->s_streaming_live & TSS_LIVE))
    flags |= TSS_TIMEOUT;
  if (flags)
    service_set_streaming_status_flags(t, flags);
  t->s_streaming_live &= ~TSS_LIVE;

  tvh_mutex_unlock(&t->s_stream_mutex);

  if (t->s_timeout > 0)
    mtimer_arm_rel(&t->s_receive_timer, service_data_timeout, t,
                   sec2mono(t->s_timeout));
}

int
service_is_sdtv(const service_t *t)
{
  char s_type;
  if(t->s_type_user == ST_UNSET)
    s_type = t->s_servicetype;
  else
    s_type = t->s_type_user;
  if (s_type == ST_SDTV)
    return 1;
  else if (s_type == ST_NONE) {
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link)
      if (SCT_ISVIDEO(st->es_type) && st->es_height < 720)
        return 1;
  }
  return 0;
}

int
service_is_hdtv(const service_t *t)
{
  char s_type;
  if(t->s_type_user == ST_UNSET)
    s_type = t->s_servicetype;
  else
    s_type = t->s_type_user;
  if (s_type == ST_HDTV)
    return 1;
  else if (s_type == ST_NONE) {
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link)
      if (SCT_ISVIDEO(st->es_type) &&
          st->es_height >= 720 && st->es_height < 1080)
        return 1;
  }
  return 0;
}

int
service_is_fhdtv(const service_t *t)
{
  char s_type;
  if(t->s_type_user == ST_UNSET)
    s_type = t->s_servicetype;
  else
    s_type = t->s_type_user;
  if (s_type == ST_FHDTV)
    return 1;
  else if (s_type == ST_NONE) {
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link)
      if (SCT_ISVIDEO(st->es_type) && st->es_height >= 1080 && st->es_height < 1440)
        return 1;
  }
  return 0;
}

int
service_is_uhdtv(const service_t *t)
{
  char s_type;
  if(t->s_type_user == ST_UNSET)
    s_type = t->s_servicetype;
  else
    s_type = t->s_type_user;
  if (s_type == ST_UHDTV)
    return 1;
  else if (s_type == ST_NONE) {
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link)
      if (SCT_ISVIDEO(st->es_type) && st->es_height >= 1440)
        return 1;
  }
  return 0;
}

/**
 *
 */
int
service_is_radio(const service_t *t)
{
  int ret = 0;
  char s_type;
  if(t->s_type_user == ST_UNSET)
    s_type = t->s_servicetype;
  else
    s_type = t->s_type_user;
  if (s_type == ST_RADIO)
    return 1;
  else if (s_type == ST_NONE) {
    elementary_stream_t *st;
    TAILQ_FOREACH(st, &t->s_components.set_all, es_link) {
      if (SCT_ISVIDEO(st->es_type))
        return 0;
      else if (SCT_ISAUDIO(st->es_type))
        ret = 1;
    }
  }
  return ret;
}

/**
 * Is encrypted
 */
int
service_is_encrypted(const service_t *t)
{
  elementary_stream_t *st;
  if (((mpegts_service_t *)t)->s_dvb_forcecaid == 0xffff)
    return 0;
  if (((mpegts_service_t *)t)->s_dvb_forcecaid)
    return 1;
  TAILQ_FOREACH(st, &t->s_components.set_all, es_link)
    if (st->es_type == SCT_CA)
      return 1;
  return 0;
}

/*
 * String describing service type
 */
const char *
service_servicetype_txt ( service_t *s )
{
  static const char *types[] = {
    "HDTV", "SDTV", "Radio", "FHDTV", "UHDTV", "Other"
  };
  if (service_is_hdtv(s))  return types[0];
  if (service_is_sdtv(s))  return types[1];
  if (service_is_radio(s)) return types[2];
  if (service_is_fhdtv(s)) return types[3];
  if (service_is_uhdtv(s)) return types[4];
  return types[5];
}


/**
 *
 */
void
service_send_streaming_status(service_t *t)
{
  streaming_message_t *sm;
  lock_assert(&t->s_stream_mutex);

  sm = streaming_msg_create_code(SMT_SERVICE_STATUS, t->s_streaming_status);
  streaming_service_deliver(t, sm);
}

/**
 *
 */
void
service_set_streaming_status_flags_(service_t *t, int set)
{
  lock_assert(&t->s_stream_mutex);

  if(set == t->s_streaming_status)
    return; // Already set

  t->s_streaming_status = set;

  tvhdebug(LS_SERVICE, "%s: Status changed to %s%s%s%s%s%s%s%s%s%s",
	 service_nicename(t),
	 set & TSS_INPUT_HARDWARE ? "[Hardware input] " : "",
	 set & TSS_INPUT_SERVICE  ? "[Input on service] " : "",
	 set & TSS_MUX_PACKETS    ? "[Demuxed packets] " : "",
	 set & TSS_PACKETS        ? "[Reassembled packets] " : "",
	 set & TSS_NO_DESCRAMBLER ? "[No available descrambler] " : "",
	 set & TSS_NO_ACCESS      ? "[No access] " : "",
	 set & TSS_CA_CHECK       ? "[CA check] " : "",
	 set & TSS_TUNING         ? "[Tuning failed] " : "",
	 set & TSS_GRACEPERIOD    ? "[Graceperiod expired] " : "",
	 set & TSS_TIMEOUT        ? "[Data timeout] " : "");

  service_send_streaming_status(t);
}

/**
 *
 */
streaming_start_t *
service_build_streaming_start(service_t *t)
{
  streaming_start_t *ss;
  ss = elementary_stream_build_start(&t->s_components);
  t->s_setsourceinfo(t, &ss->ss_si);
  return ss;
}

/**
 * Restart output on a service (streams only).
 * Happens if the stream composition changes.
 * (i.e. an AC3 stream disappears, etc)
 */
void
service_restart_streams(service_t *t)
{
  streaming_message_t *sm;
  streaming_start_t *ss;
  const int had_streams = elementary_set_has_streams(&t->s_components, 1);
  const int had_components = had_streams && t->s_running;

  elementary_set_filter_build(&t->s_components);

  if(had_streams) {
    if (had_components) {
      sm = streaming_msg_create_code(SMT_STOP, SM_CODE_SOURCE_RECONFIGURED);
      streaming_service_deliver(t, sm);
    }
    ss = service_build_streaming_start(t);
    sm = streaming_msg_create_data(SMT_START, ss);
    streaming_pad_deliver(&t->s_streaming_pad, sm);
    t->s_running = 1;
  } else {
    sm = streaming_msg_create_code(SMT_STOP, SM_CODE_NO_SERVICE);
    streaming_service_deliver(t, sm);
    t->s_running = 0;
  }
}

/**
 * Restart output on a service.
 * Happens if the stream composition changes.
 * (i.e. an AC3 stream disappears, etc)
 */
void
service_restart(service_t *t)
{
  tvhtrace(LS_SERVICE, "restarting service '%s'", t->s_nicename);

  if (t->s_type == STYPE_STD) {
    tvh_mutex_lock(&t->s_stream_mutex);
    service_restart_streams(t);
    tvh_mutex_unlock(&t->s_stream_mutex);

    descrambler_caid_changed(t);
  }

  if (t->s_refresh_feed != NULL)
    t->s_refresh_feed(t);

  descrambler_service_start(t);
}

/**
 * Update one elementary stream from the parser.
 * The update should be handled only for the live streaming.
 */
void
service_update_elementary_stream(service_t *t, elementary_stream_t *src)
{
  elementary_stream_t *es;
  int change = 0, restart = 0;

  es = elementary_stream_find(&t->s_components, src->es_pid);
  if (es == NULL)
    return;

  # define _CHANGE(field) \
    if (es->field != src->field) { es->field = src->field; change++; }
  # define _RESTART(field) \
    if (es->field != src->field) { es->field = src->field; restart++; }

  _CHANGE(es_frame_duration);
  _RESTART(es_width);
  _RESTART(es_height);
  _CHANGE(es_aspect_num);
  _CHANGE(es_aspect_den);
  _RESTART(es_audio_version);
  _RESTART(es_sri);
  _RESTART(es_ext_sri);
  _CHANGE(es_channels);

  # undef _CHANGE
  # undef _RESTART

  if (change || restart)
    service_request_save(t);
  if (restart)
    atomic_set(&t->s_pending_restart, 1);
}

/**
 *
 */

static tvh_mutex_t pending_save_mutex;
static tvh_cond_t pending_save_cond;
static struct service_queue pending_save_queue;

/**
 *
 */
void
service_request_save(service_t *t)
{
  if (t->s_type != STYPE_STD)
    return;

  tvh_mutex_lock(&pending_save_mutex);

  if(!t->s_ps_onqueue) {
    t->s_ps_onqueue = 1;
    TAILQ_INSERT_TAIL(&pending_save_queue, t, s_ps_link);
    service_ref(t);
    tvh_cond_signal(&pending_save_cond, 0);
  }

  tvh_mutex_unlock(&pending_save_mutex);
}


/**
 *
 */
static void
service_class_delete(struct idnode *self)
{
  service_destroy((service_t *)self, 1);
}

/**
 *
 */
static htsmsg_t *
service_class_save(struct idnode *self, char *filename, size_t fsize)
{
  service_t *s = (service_t *)self;
  if (s->s_config_save)
    return s->s_config_save(s, filename, fsize);
  return NULL;
}

/**
 *
 */
static void
service_class_load(struct idnode *self, htsmsg_t *c)
{
  service_load((service_t *)self, c);
}

/**
 *
 */
static void *
service_saver(void *aux)
{
  service_t *t;

  tvh_mutex_lock(&pending_save_mutex);

  while(tvheadend_is_running()) {

    if((t = TAILQ_FIRST(&pending_save_queue)) == NULL) {
      tvh_cond_wait(&pending_save_cond, &pending_save_mutex);
      continue;
    }
    assert(t->s_ps_onqueue != 0);

    TAILQ_REMOVE(&pending_save_queue, t, s_ps_link);
    t->s_ps_onqueue = 0;

    tvh_mutex_unlock(&pending_save_mutex);
    tvh_mutex_lock(&global_lock);

    if(t->s_status != SERVICE_ZOMBIE && t->s_config_save)
      idnode_changed(&t->s_id);
    service_unref(t);

    tvh_mutex_unlock(&global_lock);
    tvh_mutex_lock(&pending_save_mutex);
  }

  tvh_mutex_unlock(&pending_save_mutex);
  return NULL;
}


/**
 *
 */
pthread_t service_saver_tid;

static void services_memoryinfo_update(memoryinfo_t *my)
{
  service_t *t;
  int64_t size = 0, count = 0;

  lock_assert(&global_lock);
  TAILQ_FOREACH(t, &service_all, s_all_link) {
    t->s_memoryinfo(t, &size);
    count++;
  }
  TAILQ_FOREACH(t, &service_raw_all, s_all_link) {
    t->s_memoryinfo(t, &size);
    count++;
  }
  TAILQ_FOREACH(t, &service_raw_remove, s_all_link) {
    t->s_memoryinfo(t, &size);
    count++;
  }
  memoryinfo_update(my, size, count);
}

static memoryinfo_t services_memoryinfo = {
  .my_name = "Services",
  .my_update = services_memoryinfo_update
};

void
service_init(void)
{
  memoryinfo_register(&services_memoryinfo);
  TAILQ_INIT(&pending_save_queue);
  TAILQ_INIT(&service_all);
  TAILQ_INIT(&service_raw_all);
  TAILQ_INIT(&service_raw_remove);
  idclass_register(&service_class);
  idclass_register(&service_raw_class);
  tvh_mutex_init(&pending_save_mutex, NULL);
  tvh_cond_init(&pending_save_cond, 1);
  tvh_thread_create(&service_saver_tid, NULL, service_saver, NULL, "service");
}

void
service_done(void)
{
  service_t *t;

  tvh_mutex_lock(&pending_save_mutex);
  tvh_cond_signal(&pending_save_cond, 0);
  tvh_mutex_unlock(&pending_save_mutex);
  pthread_join(service_saver_tid, NULL);

  tvh_mutex_lock(&global_lock);
  while ((t = TAILQ_FIRST(&service_raw_remove)) != NULL)
    service_destroy(t, 0);
  memoryinfo_unregister(&services_memoryinfo);
  tvh_mutex_unlock(&global_lock);
}

/**
 *
 */
void
service_source_info_free(struct source_info *si)
{
  free(si->si_adapter);
  free(si->si_network);
  free(si->si_network_type);
  free(si->si_mux);
  free(si->si_provider);
  free(si->si_service);
  free(si->si_satpos);
}


void
service_source_info_copy(source_info_t *dst, const source_info_t *src)
{
  *dst = *src;
#define COPY(x) if (src->si_##x) dst->si_##x = strdup(src->si_##x)
  COPY(adapter);
  COPY(network);
  COPY(network_type);
  COPY(mux);
  COPY(provider);
  COPY(service);
  COPY(satpos);
#undef COPY
}


/**
 *
 */
const char *
service_nicename(service_t *t)
{
  return t->s_nicename;
}

const char *
service_adapter_nicename(service_t *t, char *buf, size_t len)
{
  tvh_mutex_lock(&t->s_stream_mutex);
  service_make_nicename0(t, buf, len, 1);
  tvh_mutex_unlock(&t->s_stream_mutex);
  return buf;
}

const char *
service_tss2text(int flags)
{
  if(flags & TSS_NO_ACCESS)
    return "No access";

  if(flags & TSS_TUNING)
    return "Tuning failed";

  if(flags & TSS_NO_DESCRAMBLER)
    return "No descrambler";

  if(flags & TSS_PACKETS)
    return "Got valid packets";

  if(flags & TSS_MUX_PACKETS)
    return "Got multiplexed packets but could not decode further";

  if(flags & TSS_INPUT_SERVICE)
    return "Got packets for this service but could not decode further";

  if(flags & TSS_INPUT_HARDWARE)
    return "Sensed input from hardware but nothing for the service";

  if(flags & TSS_GRACEPERIOD)
    return "No input detected";

  if(flags & TSS_TIMEOUT)
    return "Data timeout";

  return "No status";
}


/**
 *
 */
int
tss2errcode(int tss)
{
  if(tss & TSS_NO_DESCRAMBLER)
    return SM_CODE_NO_DESCRAMBLER;

  if(tss & TSS_NO_ACCESS)
    return SM_CODE_NO_ACCESS;

  if(tss & TSS_TUNING)
    return SM_CODE_TUNING_FAILED;

  if(tss & (TSS_GRACEPERIOD|TSS_TIMEOUT))
    return SM_CODE_NO_INPUT;

  return SM_CODE_OK;
}


/**
 *
 */
void
service_refresh_channel(service_t *t)
{
  idnode_list_mapping_t *ilm;
  LIST_FOREACH(ilm, &t->s_channels, ilm_in1_link) {
    htsp_channel_update((channel_t *)ilm->ilm_in2);
  }
}


/**
 * Priority Then Weight
 */
static int
si_cmp(const service_instance_t *a, const service_instance_t *b)
{
  int r;
  r = a->si_prio - b->si_prio;

  if (!r)
    r = a->si_weight - b->si_weight;
  return r;
}

/**
 *
 */
service_instance_t *
service_instance_add(service_instance_list_t *sil,
                     struct service *s, int instance,
                     const char *source, int prio, int weight)
{
  service_instance_t *si;

  prio += 10 + MAX(-10, MIN(10, s->s_prio));

  /* Existing */
  TAILQ_FOREACH(si, sil, si_link)
    if(si->si_s == s && si->si_instance == instance)
      break;

  if(si == NULL) {
    si = calloc(1, sizeof(service_instance_t));
    si->si_s = s;
    service_ref(s);
    si->si_instance = instance;
  } else {
    si->si_mark = 0;
    if(si->si_prio == prio && si->si_weight == weight)
      return si;
    TAILQ_REMOVE(sil, si, si_link);
  }
  si->si_weight = weight;
  si->si_prio   = prio;
  strlcpy(si->si_source, source ?: "<unknown>", sizeof(si->si_source));
  TAILQ_INSERT_SORTED(sil, si, si_link, si_cmp);
  return si;
}

/**
 *
 */
void
service_instance_destroy
  (service_instance_list_t *sil, service_instance_t *si)
{
  TAILQ_REMOVE(sil, si, si_link);
  service_unref(si->si_s);
  free(si);
}

/**
 *
 */
void
service_instance_list_clear(service_instance_list_t *sil)
{
  lock_assert(&global_lock);

  service_instance_t *si;
  while((si = TAILQ_FIRST(sil)) != NULL)
    service_instance_destroy(sil, si);
}

/*
 * Get name for channel from service
 */
const char *
service_get_channel_name ( service_t *s )
{
  const char *r = NULL;
  if (s->s_channel_name) r = s->s_channel_name(s);
  if (!r) r = s->s_nicename;
  return r;
}

/*
 * Get full name for channel from service
 */
const char *
service_get_full_channel_name ( service_t *s )
{
  static char buf[256];
  const char *r = NULL;
  int         len;

  if (s->s_channel_name)
    r = s->s_channel_name(s);
  if (r == NULL)
    return s->s_nicename;

  len = s->s_nicename_prefidx;
  if (len >= sizeof(buf))
    len = sizeof(buf) - 1;
  strncpy(buf, s->s_nicename, len);
  if (len < sizeof(buf) - 1)
    buf[len++] = '/';
  buf[len] = '\0';
  if (len < sizeof(buf))
    snprintf(buf + len, sizeof(buf) - len, "%s%s", !s->s_enabled ? "---" : "", r);
  return buf;
}

/*
 * Get number for service
 */
int64_t
service_get_channel_number ( service_t *s )
{
  if (s->s_channel_number) return s->s_channel_number(s);
  return 0;
}

/*
 * Get source identificator for service
 */
const char *
service_get_source ( service_t *s )
{
  if (s->s_source) return s->s_source(s);
  return NULL;
}

/*
 * Get name for channel from service
 */
const char *
service_get_channel_icon ( service_t *s )
{
  const char *r = NULL;
  if (s->s_channel_icon) r = s->s_channel_icon(s);
  return r;
}

/*
 * Get EPG ID for channel from service
 */
const char *
service_get_channel_epgid ( service_t *s )
{
  if (s->s_channel_epgid) return s->s_channel_epgid(s);
  return NULL;
}

/*
 *
 */
void
service_mapped(service_t *s)
{
  if (s->s_mapped) s->s_mapped(s);
}

/*
 * list of known service types
 */
htsmsg_t *servicetype_list ( void )
{
  htsmsg_t *ret;//, *e;
  //int i;
  ret = htsmsg_create_list();
#ifdef TODO_FIX_THIS
 for (i = 0; i < sizeof(stypetab) / sizeof(stypetab[0]); i++ ) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "val", stypetab[i].val);
    htsmsg_add_str(e, "str", stypetab[i].str);
    htsmsg_add_msg(ret, NULL, e);
  }
#endif
  return ret;
}

void service_save ( service_t *t, htsmsg_t *m )
{
  elementary_stream_t *st;
  htsmsg_t *list, *sub, *hbbtv;

  idnode_save(&t->s_id, m);

  htsmsg_add_s32(m, "verified", t->s_verified);
  htsmsg_add_u32(m, "pcr", t->s_components.set_pcr_pid);
  htsmsg_add_u32(m, "pmt", t->s_components.set_pmt_pid);

  tvh_mutex_lock(&t->s_stream_mutex);

  list = htsmsg_create_list();
  TAILQ_FOREACH(st, &t->s_components.set_all, es_link) {

    if (st->es_type == SCT_PCR)
      continue;

    sub = htsmsg_create_map();

    htsmsg_add_u32(sub, "pid", st->es_pid);
    htsmsg_add_str(sub, "type", streaming_component_type2txt(st->es_type));
    htsmsg_add_u32(sub, "position", st->es_position);

    if(st->es_lang[0])
      htsmsg_add_str(sub, "language", st->es_lang);

    if (SCT_ISAUDIO(st->es_type)) {
      htsmsg_add_u32(sub, "audio_type", st->es_audio_type);
      if (st->es_audio_version)
        htsmsg_add_u32(sub, "audio_version", st->es_audio_version);
    }

    if(st->es_type == SCT_CA) {
      caid_t *c;
      htsmsg_t *v = htsmsg_create_list();
      LIST_FOREACH(c, &st->es_caids, link) {
        htsmsg_t *caid = htsmsg_create_map();

        htsmsg_add_u32(caid, "caid", c->caid);
        if(c->providerid)
          htsmsg_add_u32(caid, "providerid", c->providerid);
        htsmsg_add_msg(v, NULL, caid);
      }

      htsmsg_add_msg(sub, "caidlist", v);
    }

    if(st->es_type == SCT_DVBSUB) {
      htsmsg_add_u32(sub, "compositionid", st->es_composition_id);
      htsmsg_add_u32(sub, "ancillartyid", st->es_ancillary_id);
    }

    if(st->es_type == SCT_TEXTSUB)
      htsmsg_add_u32(sub, "parentpid", st->es_parent_pid);

    if(SCT_ISVIDEO(st->es_type)) {
      if(st->es_width)
	      htsmsg_add_u32(sub, "width", st->es_width);
      if(st->es_height)
	      htsmsg_add_u32(sub, "height", st->es_height);
      if(st->es_frame_duration)
        htsmsg_add_u32(sub, "duration", st->es_frame_duration);
    }

    htsmsg_add_msg(list, NULL, sub);
  }

  hbbtv = htsmsg_copy(t->s_hbbtv);

  tvh_mutex_unlock(&t->s_stream_mutex);

  htsmsg_add_msg(m, "stream", list);
  if (hbbtv)
    htsmsg_add_msg(m, "hbbtv", hbbtv);
}

/**
 *
 */
void
service_remove_unseen(const char *type, int days)
{
  service_t *s, *sn;
  time_t before = gclk() - MAX(days, 5) * 24 * 3600;

  lock_assert(&global_lock);
  for (s = TAILQ_FIRST(&service_all); s; s = sn) {
    sn = TAILQ_NEXT(s, s_all_link);
    if (s->s_unseen && s->s_unseen(s, type, before))
      service_destroy(s, 1);
  }
}

/**
 *
 */
static void
add_caid(elementary_stream_t *st, uint16_t caid, uint32_t providerid)
{
  caid_t *c = malloc(sizeof(caid_t));
  c->caid = caid;
  c->providerid = providerid;
  c->pid = 0;
  c->use = 1;
  c->filter = 0;
  c->delete_me = 0;
  LIST_INSERT_HEAD(&st->es_caids, c, link);
}

/**
 *
 */
static void
load_legacy_caid(htsmsg_t *c, elementary_stream_t *st)
{
  uint32_t a, b;
  const char *v;

  if(htsmsg_get_u32(c, "caproviderid", &b))
    b = 0;

  if(htsmsg_get_u32(c, "caidnum", &a)) {
    if((v = htsmsg_get_str(c, "caid")) != NULL) {
      a = name2caid(v);
    } else {
      return;
    }
  }

  add_caid(st, a, b);
}

/**
 *
 */
static void
load_caid(htsmsg_t *m, elementary_stream_t *st)
{
  htsmsg_field_t *f;
  htsmsg_t *c, *v = htsmsg_get_list(m, "caidlist");
  uint32_t a, b;

  if(v == NULL)
    return;

  HTSMSG_FOREACH(f, v) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "caid", &a))
      continue;

    if(htsmsg_get_u32(c, "providerid", &b))
      b = 0;

    add_caid(st, a, b);
  }
}

void service_load ( service_t *t, htsmsg_t *c )
{
  htsmsg_t *m, *hbbtv;
  htsmsg_field_t *f;
  int32_t s32;
  uint32_t u32, pid, pid2;
  elementary_stream_t *st;
  streaming_component_type_t type;
  const char *v;
  int shared_pcr = 0;

  idnode_load(&t->s_id, c);

  tvh_mutex_lock(&t->s_stream_mutex);
  if(!htsmsg_get_s32(c, "verified", &s32))
    t->s_verified = s32;
  else
    t->s_verified = 1;
  if(!htsmsg_get_u32(c, "pcr", &u32))
    t->s_components.set_pcr_pid = u32;
  if(!htsmsg_get_u32(c, "pmt", &u32))
    t->s_components.set_pmt_pid = u32;

  if (config.hbbtv) {
    hbbtv = htsmsg_get_map(c, "hbbtv");
    if (hbbtv) {
      t->s_hbbtv = htsmsg_copy(hbbtv);
    } else {
      htsmsg_destroy(t->s_hbbtv);
      t->s_hbbtv = NULL;
    }
  }

  m = htsmsg_get_list(c, "stream");
  if (m) {
    HTSMSG_FOREACH(f, m) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
        continue;

      if((v = htsmsg_get_str(c, "type")) == NULL)
        continue;

      type = streaming_component_txt2type(v);
      if(type == -1 || type == SCT_PCR)
        continue;

      if(htsmsg_get_u32(c, "pid", &pid))
        continue;

      pid2 = t->s_components.set_pcr_pid;
      if(pid > 0 && pid2 > 0 && pid == pid2)
        shared_pcr = 1;

      st = elementary_stream_create(&t->s_components, pid, type);

      if((v = htsmsg_get_str(c, "language")) != NULL)
        strlcpy(st->es_lang, lang_code_get(v), 4);

      if (SCT_ISAUDIO(type)) {
        if(!htsmsg_get_u32(c, "audio_type", &u32))
          st->es_audio_type = u32;
        if(!htsmsg_get_u32(c, "audio_version", &u32))
          st->es_audio_version = u32;
      }

      if(!htsmsg_get_u32(c, "position", &u32))
        st->es_position = u32;

      load_legacy_caid(c, st);
      load_caid(c, st);

      if(type == SCT_DVBSUB) {
        if(!htsmsg_get_u32(c, "compositionid", &u32))
	        st->es_composition_id = u32;

        if(!htsmsg_get_u32(c, "ancillartyid", &u32))
	        st->es_ancillary_id = u32;
      }

      if(type == SCT_TEXTSUB) {
        if(!htsmsg_get_u32(c, "parentpid", &u32))
	        st->es_parent_pid = u32;
      }

      if(SCT_ISVIDEO(type)) {
        if(!htsmsg_get_u32(c, "width", &u32))
	        st->es_width = u32;

        if(!htsmsg_get_u32(c, "height", &u32))
	        st->es_height = u32;

        if(!htsmsg_get_u32(c, "duration", &u32))
          st->es_frame_duration = u32;
      }
    }
  }
  if (!shared_pcr)
    elementary_stream_type_modify(&t->s_components,
                                   t->s_components.set_pcr_pid, SCT_PCR);
  else
    elementary_stream_type_destroy(&t->s_components, SCT_PCR);
  elementary_set_sort_streams(&t->s_components);
  tvh_mutex_unlock(&t->s_stream_mutex);
}
