/*
 *  MPEGTS (DVB) based service
 *
 *  Copyright (C) 2013 Andreas Öman
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

#include <assert.h>

#include "service.h"
#include "channels.h"
#include "input.h"
#include "settings.h"
#include "dvb_charset.h"
#include "config.h"
#include "epggrab.h"

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t service_class;

static const void *
mpegts_service_class_get_mux ( void *ptr )
{
  static char buf[512], *s = buf;
  mpegts_service_t *ms = ptr;
  if (ms->s_dvb_mux && ms->s_dvb_mux->mm_display_name)
    ms->s_dvb_mux->mm_display_name(ms->s_dvb_mux, buf, sizeof(buf));
  else
    *buf = 0;
  return &s;
}

static const void *
mpegts_service_class_get_network ( void *ptr )
{
  static char buf[512], *s = buf;
  mpegts_service_t *ms = ptr;
  mpegts_network_t *mn = ms->s_dvb_mux ? ms->s_dvb_mux->mm_network : NULL;
  if (mn && mn->mn_display_name)
    mn->mn_display_name(mn, buf, sizeof(buf));
  else
    *buf = 0;
  return &s;
}

static htsmsg_t *
mpegts_service_pref_capid_lock_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Off",                0 },
    { "On",                 1 },
    { "Only Pref. CA PID",  2 },
  };
   return strtab2htsmsg(tab);
}

const idclass_t mpegts_service_class =
{
  .ic_super      = &service_class,
  .ic_class      = "mpegts_service",
  .ic_caption    = "MPEGTS Service",
  .ic_order      = "enabled,channel,svcname",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "multiplex",
      .name     = "Mux",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_mux,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = "Service ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_service_id),
    },
    {
      .type     = PT_U16,
      .id       = "lcn",
      .name     = "Local Channel Number",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_num),
    },
    {
      .type     = PT_U16,
      .id       = "lcn_minor",
      .name     = "Local Channel Minor",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_minor),
    },
    {
      .type     = PT_U16,
      .id       = "lcn2",
      .name     = "OpenTV Channel Number",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_opentv_chnum),
    },
    {
      .type     = PT_STR,
      .id       = "svcname",
      .name     = "Service Name",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_svcname),
    },
    {
      .type     = PT_STR,
      .id       = "provider",
      .name     = "Provider",
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_provider),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = "CRID Authority",
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_cridauth),
    },
    {
      .type     = PT_U16,
      .id       = "dvb_servicetype",
      .name     = "Service Type",
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_servicetype),
    },
    {
      .type     = PT_BOOL,
      .id       = "dvb_ignore_eit",
      .name     = "Ignore EPG (EIT)",
      .off      = offsetof(mpegts_service_t, s_dvb_ignore_eit),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = "Character Set",
      .off      = offsetof(mpegts_service_t, s_dvb_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_U16,
      .id       = "prefcapid",
      .name     = "Pref. CA PID",
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "prefcapid_lock",
      .name     = "Lock Pref. CA PID",
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid_lock),
      .opts     = PO_ADVANCED,
      .list     = mpegts_service_pref_capid_lock_list,
    },
    {
      .type     = PT_U16,
      .id       = "force_caid",
      .name     = "Force CA ID (e.g. 0x2600)",
      .off      = offsetof(mpegts_service_t, s_dvb_forcecaid),
      .opts     = PO_ADVANCED | PO_HEXA,
    },
    {
      .type     = PT_TIME,
      .id       = "created",
      .name     = "Created",
      .off      = offsetof(mpegts_service_t, s_dvb_created),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "last_seen",
      .name     = "Last Seen",
      .off      = offsetof(mpegts_service_t, s_dvb_last_seen),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {},
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

/*
 * Check the service is enabled
 */
static int
mpegts_service_is_enabled(service_t *t, int flags)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_mux_t *mm    = s->s_dvb_mux;
  return mm->mm_is_enabled(mm) ? s->s_enabled : 0;
}

/*
 * Save
 */
static void
mpegts_service_config_save ( service_t *t )
{
  htsmsg_t *c = htsmsg_create_map();
  mpegts_service_t *s = (mpegts_service_t*)t;
  service_save(t, c);
  hts_settings_save(c, "input/dvb/networks/%s/muxes/%s/services/%s",
                    idnode_uuid_as_str(&s->s_dvb_mux->mm_network->mn_id),
                    idnode_uuid_as_str(&s->s_dvb_mux->mm_id),
                    idnode_uuid_as_str(&s->s_id));
  htsmsg_destroy(c);
}

/*
 * Service instance list
 */
static void
mpegts_service_enlist(service_t *t, struct service_instance_list *sil, int flags)
{
  int p = 0, w;
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_input_t        *mi;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mmi;

  assert(s->s_source_type == S_MPEG_TS);

  /* Create instances */
  m->mm_create_instances(m);

  /* Enlist available */
  LIST_FOREACH(mmi, &m->mm_instances, mmi_mux_link) {
    if (mmi->mmi_tune_failed)
      continue;

    mi = mmi->mmi_input;

    if (!mi->mi_is_enabled(mi, mmi->mmi_mux, flags)) continue;

    /* Set weight to -1 (forced) for already active mux */
    if (mmi->mmi_mux->mm_active == mmi) {
      w = -1;
      p = -1;
    } else {
      w = mi->mi_get_weight(mi, flags);
      p = mi->mi_get_priority(mi, mmi->mmi_mux, flags);
    }

    service_instance_add(sil, t, mi->mi_instance, mi->mi_name, p, w);
  }
}

/*
 * Start service
 */
static int
mpegts_service_start(service_t *t, int instance)
{
  int r;
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mmi;

  /* Validate */
  assert(s->s_status      == SERVICE_IDLE);
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Find */
  LIST_FOREACH(mmi, &m->mm_instances, mmi_mux_link)
    if (mmi->mmi_input->mi_instance == instance)
      break;
  assert(mmi != NULL);
  if (mmi == NULL)
    return SM_CODE_UNDEFINED_ERROR;

  /* Start Mux */
  r = mpegts_mux_instance_start(&mmi);

  /* Start */
  if (!r) {

    /* Open service */
    mmi->mmi_input->mi_open_service(mmi->mmi_input, s, 1);
  }

  return r;
}

/*
 * Stop service
 */
static void
mpegts_service_stop(service_t *t)
{
  mpegts_service_t *s  = (mpegts_service_t*)t;
  mpegts_input_t   *i  = s->s_dvb_active_input;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Stop */
  if (i)
    i->mi_close_service(i, s);

  /* Save some memory */
  sbuf_free(&s->s_tsbuf);
}

/*
 * Refresh
 */
static void
mpegts_service_refresh(service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_input_t   *i = s->s_dvb_active_input;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  assert(i != NULL);
  lock_assert(&global_lock);

  /* Re-open */
  i->mi_open_service(i, s, 0);
}

/*
 * Source info
 */
static void
mpegts_service_setsourceinfo(service_t *t, source_info_t *si)
{
  char buf[256];
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Update */
  memset(si, 0, sizeof(struct source_info));
  si->si_type = S_MPEG_TS;

  if(m->mm_network->mn_network_name != NULL)
    si->si_network = strdup(m->mm_network->mn_network_name);

  m->mm_display_name(m, buf, sizeof(buf));
  si->si_mux = strdup(buf);

  if(s->s_dvb_active_input) {
    mpegts_input_t *mi = s->s_dvb_active_input;
    mi->mi_display_name(mi, buf, sizeof(buf));
    si->si_adapter = strdup(buf);
  }

  if(s->s_dvb_provider != NULL)
    si->si_provider = strdup(s->s_dvb_provider);

  if(s->s_dvb_svcname != NULL)
    si->si_service = strdup(s->s_dvb_svcname);

  if(m->mm_network != NULL && m->mm_network->mn_satpos != INT_MAX) {
    dvb_sat_position_to_str(m->mm_network->mn_satpos, buf, sizeof(buf));
    si->si_satpos = strdup(buf);
  }
}

/*
 * Grace period
 */
static int
mpegts_service_grace_period(service_t *t)
{
  int r = 0;
  mpegts_service_t *ms = (mpegts_service_t*)t;
  mpegts_mux_t     *mm = ms->s_dvb_mux;
  mpegts_input_t   *mi = ms->s_dvb_active_input;
  assert(mi != NULL);

  if (mi && mi->mi_get_grace)
    r = mi->mi_get_grace(mi, mm);
  
  return r ?: 10;  
}

/*
 * Channel number
 */
static int64_t
mpegts_service_channel_number ( service_t *s )
{
  mpegts_service_t *ms = (mpegts_service_t*)s;
  int r = 0;

  if (!ms->s_dvb_mux->mm_network->mn_ignore_chnum) {
    r = ms->s_dvb_channel_num * CHANNEL_SPLIT + ms->s_dvb_channel_minor;
    if (r <= 0)
      r = ms->s_dvb_opentv_chnum * CHANNEL_SPLIT;
  }
  if (r <= 0 && ms->s_dvb_mux->mm_network->mn_sid_chnum)
    r = ms->s_dvb_service_id * CHANNEL_SPLIT;
  return r;
}

static const char *
mpegts_service_channel_name ( service_t *s )
{
  return ((mpegts_service_t*)s)->s_dvb_svcname;
}

static const char *
mpegts_service_provider_name ( service_t *s )
{
  return ((mpegts_service_t*)s)->s_dvb_provider;
}

static const char *
mpegts_service_channel_icon ( service_t *s )
{
  /* DVB? */
#if ENABLE_MPEGTS_DVB
  mpegts_service_t *ms = (mpegts_service_t*)s;
  extern const idclass_t dvb_mux_class;
  if (ms->s_dvb_mux &&
      idnode_is_instance(&ms->s_dvb_mux->mm_id, &dvb_mux_class)) {
    int32_t hash = 0;
    static char buf[128];
    dvb_mux_t *mmd = (dvb_mux_t*)ms->s_dvb_mux;
    int pos;

    switch ( mmd->lm_tuning.dmc_fe_type) {
      case DVB_TYPE_S:
        if ((pos = dvb_network_get_orbital_pos(mmd->mm_network)) == INT_MAX)
          return NULL;
        if (pos < -1800 || pos > 1800)
          return NULL;
        hash = (pos >= 0 ? pos : 3600 + pos) << 16;
        break;
      case DVB_TYPE_C:
        hash = 0xFFFF0000;
        break;
      case DVB_TYPE_T:
        hash = 0xEEEE0000;
        break;
      case DVB_TYPE_ATSC:
        hash = 0xDDDD0000;
        break;
      default:
        return NULL;
    }

    snprintf(buf, sizeof(buf),
             "picon://1_0_%X_%X_%X_%X_%X_0_0_0.png",
             ms->s_dvb_servicetype,
             ms->s_dvb_service_id,
             ms->s_dvb_mux->mm_tsid,
             ms->s_dvb_mux->mm_onid,
             hash);
    return buf;
  }
#endif

  return NULL;
}

static void
mpegts_service_mapped ( service_t *t )
{
  epggrab_ota_queue_mux(((mpegts_service_t *)t)->s_dvb_mux);
}

void
mpegts_service_delete ( service_t *t, int delconf )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;
  mpegts_mux_t     *mm = ms->s_dvb_mux;

  /* Remove config */
  if (delconf)
    hts_settings_remove("input/dvb/networks/%s/muxes/%s/services/%s",
                      idnode_uuid_as_str(&mm->mm_network->mn_id),
                      idnode_uuid_as_str(&mm->mm_id),
                      idnode_uuid_as_str(&t->s_id));

  /* Free memory */
  free(ms->s_dvb_svcname);
  free(ms->s_dvb_provider);
  free(ms->s_dvb_cridauth);
  free(ms->s_dvb_charset);
  LIST_REMOVE(ms, s_dvb_mux_link);
  sbuf_free(&ms->s_tsbuf);

  // Note: the ultimate deletion and removal from the idnode list
  //       is done in service_destroy
}

/* **************************************************************************
 * Creation/Location
 * *************************************************************************/

/*
 * Create service
 */
mpegts_service_t *
mpegts_service_create0
  ( mpegts_service_t *s, const idclass_t *class, const char *uuid,
    mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, htsmsg_t *conf )
{
  int r;
  char buf[256];

  /* defaults for older version */
  s->s_dvb_created = dispatch_clock;

  if (service_create0((service_t*)s, class, uuid, S_MPEG_TS, conf) == NULL)
    return NULL;

  /* Create */
  sbuf_init(&s->s_tsbuf);
  if (!conf) {
    if (sid)     s->s_dvb_service_id = sid;
    if (pmt_pid) s->s_pmt_pid        = pmt_pid;
  }
  s->s_dvb_mux        = mm;
  if ((r = dvb_servicetype_lookup(s->s_dvb_servicetype)) != -1)
    s->s_servicetype = r;
  LIST_INSERT_HEAD(&mm->mm_services, s, s_dvb_mux_link);
  
  s->s_delete         = mpegts_service_delete;
  s->s_is_enabled     = mpegts_service_is_enabled;
  s->s_config_save    = mpegts_service_config_save;
  s->s_enlist         = mpegts_service_enlist;
  s->s_start_feed     = mpegts_service_start;
  s->s_stop_feed      = mpegts_service_stop;
  s->s_refresh_feed   = mpegts_service_refresh;
  s->s_setsourceinfo  = mpegts_service_setsourceinfo;
  s->s_grace_period   = mpegts_service_grace_period;
  s->s_channel_number = mpegts_service_channel_number;
  s->s_channel_name   = mpegts_service_channel_name;
  s->s_provider_name  = mpegts_service_provider_name;
  s->s_channel_icon   = mpegts_service_channel_icon;
  s->s_mapped         = mpegts_service_mapped;

  pthread_mutex_lock(&s->s_stream_mutex);
  service_make_nicename((service_t*)s);
  pthread_mutex_unlock(&s->s_stream_mutex);

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhlog(LOG_DEBUG, "mpegts", "%s - add service %04X %s", buf, s->s_dvb_service_id, s->s_dvb_svcname);

  /* Notification */
  idnode_notify_simple(&mm->mm_id);
  idnode_notify_simple(&mm->mm_network->mn_id);

  /* Save the create time */
  if (s->s_dvb_created == dispatch_clock)
    service_request_save((service_t *)s, 0);

  return s;
}

/*
 * Find service
 */
mpegts_service_t *
mpegts_service_find
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, 
    int create, int *save )
{
  mpegts_service_t *s;

  /* Validate */
  lock_assert(&global_lock);

  /* Find existing service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
    if (s->s_dvb_service_id == sid) {
      if (pmt_pid && pmt_pid != s->s_pmt_pid) {
        s->s_pmt_pid = pmt_pid;
        if (save) *save = 1;
      }
      if (create) {
        if ((save && *save) || s->s_dvb_last_seen + 3600 < dispatch_clock) {
          s->s_dvb_last_seen = dispatch_clock;
          if (save) *save = 1;
        }
      }
      return s;
    }
  }

  /* Create */
  if (create) {
    s = mm->mm_network->mn_create_service(mm, sid, pmt_pid);
    s->s_dvb_created = s->s_dvb_last_seen = dispatch_clock;
    if (save) *save = 1;
  }

  return s;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
