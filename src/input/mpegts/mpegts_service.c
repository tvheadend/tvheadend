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
mpegts_service_class_get_mux_uuid ( void *ptr )
{
  static char buf[UUID_HEX_SIZE], *s = buf;
  mpegts_service_t *ms = ptr;
  if (ms && ms->s_dvb_mux)
    strcpy(buf, idnode_uuid_as_sstr(&ms->s_dvb_mux->mm_id) ?: "");
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
mpegts_service_pref_capid_lock_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Off"),                    0 },
    { N_("On"),                     1 },
    { N_("Only Preferred CA PID"),  2 },
  };
   return strtab2htsmsg(tab, 1, lang);
}

const idclass_t mpegts_service_class =
{
  .ic_super      = &service_class,
  .ic_class      = "mpegts_service",
  .ic_caption    = N_("MPEG-TS Service"),
  .ic_order      = "enabled,channel,svcname",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = N_("Network"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "multiplex",
      .name     = N_("Mux"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_mux,
    },
    {
      .type     = PT_STR,
      .id       = "multiplex_uuid",
      .name     = N_("Mux UUID"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
      .get      = mpegts_service_class_get_mux_uuid,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_service_id),
    },
    {
      .type     = PT_U16,
      .id       = "lcn",
      .name     = N_("Local Channel Number"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_num),
    },
    {
      .type     = PT_U16,
      .id       = "lcn_minor",
      .name     = N_("Local Channel Minor"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_minor),
    },
    {
      .type     = PT_U16,
      .id       = "lcn2",
      .name     = N_("OpenTV Channel Number"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_opentv_chnum),
    },
    {
      .type     = PT_U16,
      .id       = "srcid",
      .name     = "ATSC Source ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_atsc_source_id),
    },
    {
      .type     = PT_STR,
      .id       = "svcname",
      .name     = N_("Service Name"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_svcname),
    },
    {
      .type     = PT_STR,
      .id       = "provider",
      .name     = N_("Provider"),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_provider),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = N_("CRID Authority"),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_cridauth),
    },
    {
      .type     = PT_U16,
      .id       = "dvb_servicetype",
      .name     = N_("Service Type"),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_servicetype),
    },
    {
      .type     = PT_BOOL,
      .id       = "dvb_ignore_eit",
      .name     = N_("Ignore EPG (EIT)"),
      .off      = offsetof(mpegts_service_t, s_dvb_ignore_eit),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character Set"),
      .off      = offsetof(mpegts_service_t, s_dvb_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_U16,
      .id       = "prefcapid",
      .name     = N_("Preferred CA PID"),
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "prefcapid_lock",
      .name     = N_("Lock Preferred CA PID"),
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid_lock),
      .opts     = PO_ADVANCED,
      .list     = mpegts_service_pref_capid_lock_list,
    },
    {
      .type     = PT_U16,
      .id       = "force_caid",
      .name     = N_("Force CA ID (e.g. 0x2600)"),
      .off      = offsetof(mpegts_service_t, s_dvb_forcecaid),
      .opts     = PO_ADVANCED | PO_HEXA,
    },
    {
      .type     = PT_TIME,
      .id       = "created",
      .name     = N_("Created"),
      .off      = offsetof(mpegts_service_t, s_dvb_created),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "last_seen",
      .name     = N_("Last Seen"),
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
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  service_save(t, c);
  hts_settings_save(c, "input/dvb/networks/%s/muxes/%s/services/%s",
                    idnode_uuid_as_sstr(&s->s_dvb_mux->mm_network->mn_id),
                    idnode_uuid_as_str(&s->s_dvb_mux->mm_id, ubuf1),
                    idnode_uuid_as_str(&s->s_id, ubuf2));
  htsmsg_destroy(c);
}

/*
 * Service instance list
 */
static void
mpegts_service_enlist(service_t *t, tvh_input_t *ti,
                      struct service_instance_list *sil, int flags)
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

    if (ti && (tvh_input_t *)mi != ti)
      continue;

    if (!mi->mi_is_enabled(mi, mmi->mmi_mux, flags)) continue;

    /* Set weight to -1 (forced) for already active mux */
    if (mmi->mmi_mux->mm_active == mmi) {
      w = -1;
      p = -1;
    } else {
      w = mi->mi_get_weight(mi, mmi->mmi_mux, flags);
      p = mi->mi_get_priority(mi, mmi->mmi_mux, flags);
    }

    service_instance_add(sil, t, mi->mi_instance, mi->mi_name, p, w);
  }
}

/*
 * Start service
 */
static int
mpegts_service_start(service_t *t, int instance, int weight, int flags)
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
  mmi->mmi_start_weight = weight;
  r = mpegts_mux_instance_start(&mmi, t);

  /* Start */
  if (!r) {

    /* Open service */
    s->s_dvb_subscription_flags = flags;
    mmi->mmi_input->mi_open_service(mmi->mmi_input, s, flags, 1);
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
  i->mi_open_service(i, s, s->s_dvb_subscription_flags, 0);
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

#if ENABLE_MPEGTS_DVB
 if(m->mm_network != NULL && m->mm_network->mn_satpos != INT_MAX) {
    dvb_sat_position_to_str(m->mm_network->mn_satpos, buf, sizeof(buf));
    si->si_satpos = strdup(buf);
  }
#endif
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
int64_t
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

#if ENABLE_MPEGTS_DVB
static int
mpegts_picon_mux_isvalid(dvb_mux_t *mux, int orbital_position)
{
  switch (mux->mm_onid) {
    case 0:
    case 0x1111:
      return 0;
    case 1:
      return orbital_position == 192;
    case 0x00B1:
      return mux->mm_tsid != 0x00B0;
    case 0x0002:
      return abs(orbital_position-282) < 6;
    default:
      return mux->mm_tsid < 0xFF00;
  }
}
#endif

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
    dvb_mux_t *mmd = (dvb_mux_t*)ms->s_dvb_mux;
    int pos;

    switch (mmd->lm_tuning.dmc_fe_type) {
      case DVB_TYPE_S:
        if ((pos = dvb_network_get_orbital_pos(mmd->mm_network)) == INT_MAX)
          return NULL;
        if (pos < -1800 || pos > 1800)
          return NULL;
        hash = (pos >= 0 ? pos : 3600 + pos) << 16;
        if (!mpegts_picon_mux_isvalid(mmd, pos))
          hash |= ((mmd->lm_tuning.dmc_fe_freq / 1000) & 0x7fff) |
                  (mmd->lm_tuning.u.dmc_fe_qpsk.orbital_pos == DVB_POLARISATION_HORIZONTAL ? 0x8000 : 0);
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

    snprintf(prop_sbuf, PROP_SBUF_LEN,
             "picon://1_0_%X_%X_%X_%X_%X_0_0_0.png",
             ms->s_dvb_servicetype,
             ms->s_dvb_service_id,
             ms->s_dvb_mux->mm_tsid,
             ms->s_dvb_mux->mm_onid,
             hash);
    return prop_sbuf;
  }
#endif

  return NULL;
}

#if ENABLE_MPEGTS_DVB
static int
mpegts_service_match_network(mpegts_network_t *mn, uint32_t hash, const idclass_t **idc)
{
  int pos = hash >> 16, pos2;

  pos = hash >> 16;
  switch (pos) {
  case 0xFFFF: *idc = &dvb_mux_dvbc_class; return 1;
  case 0xEEEE: *idc = &dvb_mux_dvbt_class; return 1;
  case 0xDDDD: *idc = &dvb_mux_dvbt_class; return 1;
  default:     *idc = &dvb_mux_dvbs_class; break;
  }
  if (pos > 3600 || pos < 0) return 0;
  pos = pos <= 1800 ? pos : pos - 3600;
  if ((pos2 = dvb_network_get_orbital_pos(mn)) == INT_MAX) return 0;
  return deltaI32(pos, pos2) <= 20;
}
#endif

#if ENABLE_MPEGTS_DVB
static int
mpegts_service_match_mux(dvb_mux_t *mm, uint32_t hash, const idclass_t *idc)
{
  extern const idclass_t dvb_mux_dvbs_class;
  dvb_mux_t *mmd;
  int freq, pol;

  if ((hash & 0xffff) == 0) return 1;
  if (idc != &dvb_mux_dvbs_class) return 0;
  freq = (hash & 0x7fff) * 1000;
  pol = (hash & 0x8000) ? DVB_POLARISATION_HORIZONTAL : DVB_POLARISATION_VERTICAL;
  mmd = mm;
  return mmd->lm_tuning.u.dmc_fe_qpsk.orbital_pos == pol &&
         deltaU32(mmd->lm_tuning.dmc_fe_freq, freq) < 2000;
}
#endif

service_t *
mpegts_service_find_e2(uint32_t stype, uint32_t sid, uint32_t tsid,
                       uint32_t onid, uint32_t hash)
{
#if ENABLE_MPEGTS_DVB
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  mpegts_service_t *s;
  const idclass_t *idc;

  lock_assert(&global_lock);

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (!idnode_is_instance(&mn->mn_id, &dvb_network_class)) continue;
    if (!mpegts_service_match_network(mn, hash, &idc)) continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      if (!idnode_is_instance(&mm->mm_id, idc)) continue;
      if (mm->mm_tsid != tsid || mm->mm_onid != onid) continue;
      if (!mpegts_service_match_mux((dvb_mux_t *)mm, hash, idc)) continue;
      LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
        if (s->s_dvb_service_id == sid)
          return (service_t *)s;
    }
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
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Remove config */
  if (delconf && t->s_type == STYPE_STD)
    hts_settings_remove("input/dvb/networks/%s/muxes/%s/services/%s",
                      idnode_uuid_as_sstr(&mm->mm_network->mn_id),
                      idnode_uuid_as_str(&mm->mm_id, ubuf1),
                      idnode_uuid_as_str(&t->s_id, ubuf2));

  /* Free memory */
  free(ms->s_dvb_svcname);
  free(ms->s_dvb_provider);
  free(ms->s_dvb_cridauth);
  free(ms->s_dvb_charset);
  if (t->s_type == STYPE_STD)
    LIST_REMOVE(ms, s_dvb_mux_link);
  sbuf_free(&ms->s_tsbuf);

  /* Remove master/slave linking */
  LIST_SAFE_REMOVE(ms, s_masters_link);
  LIST_SAFE_REMOVE(ms, s_slaves_link);

  /* Remove PID lists */
  mpegts_pid_destroy(&ms->s_pids);
  mpegts_pid_destroy(&ms->s_slaves_pids);

  // Note: the ultimate deletion and removal from the idnode list
  //       is done in service_destroy
}

static int
mpegts_service_satip_source ( service_t *t )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;
  mpegts_network_t *mn = ms->s_dvb_mux ? ms->s_dvb_mux->mm_network : NULL;
  return mn ? mn->mn_satip_source : -1;
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

  if (service_create0((service_t*)s, STYPE_STD, class, uuid,
                      S_MPEG_TS, conf) == NULL)
    return NULL;

  /* Create */
  sbuf_init(&s->s_tsbuf);
  if (!conf) {
    if (sid)     s->s_dvb_service_id = sid;
    if (pmt_pid) s->s_pmt_pid        = pmt_pid;
  } else {
    if (s->s_dvb_last_seen > dispatch_clock) /* sanity check */
      s->s_dvb_last_seen = dispatch_clock;
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
  s->s_satip_source   = mpegts_service_satip_source;

  pthread_mutex_lock(&s->s_stream_mutex);
  service_make_nicename((service_t*)s);
  pthread_mutex_unlock(&s->s_stream_mutex);

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhlog(LOG_DEBUG, "mpegts", "%s - add service %04X %s", buf, s->s_dvb_service_id, s->s_dvb_svcname);

  /* Notification */
  idnode_notify_changed(&mm->mm_id);
  idnode_notify_changed(&mm->mm_network->mn_id);

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

/*
 * Find PID
 */
mpegts_service_t *
mpegts_service_find_by_pid ( mpegts_mux_t *mm, int pid )
{
  mpegts_service_t *s;

  lock_assert(&global_lock);

  /* Find existing service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
    pthread_mutex_lock(&s->s_stream_mutex);
    if (pid == s->s_pmt_pid || pid == s->s_pcr_pid)
      goto ok;
    if (service_stream_find((service_t *)s, pid))
      goto ok;
    pthread_mutex_unlock(&s->s_stream_mutex);
  }
  return NULL;
ok:
  pthread_mutex_unlock(&s->s_stream_mutex);
  return s;
}

/*
 * Raw MPEGTS Service
 */

const idclass_t mpegts_service_raw_class =
{
  .ic_super      = &service_raw_class,
  .ic_class      = "mpegts_raw_service",
  .ic_caption    = N_("MPEG-TS Raw Service"),
  .ic_properties = NULL
};

static void
mpegts_service_raw_setsourceinfo(service_t *t, source_info_t *si)
{
  mpegts_service_setsourceinfo(t, si);

  free(si->si_service);
  si->si_service = strdup("Raw PID Subscription");
}

static int
mpegts_service_raw_update_pids(mpegts_service_t *t, mpegts_apids_t *pids)
{
  mpegts_input_t *mi = t->s_dvb_active_input;
  mpegts_mux_t *mm = t->s_dvb_mux;
  mpegts_apids_t *p, *x;
  mpegts_apids_t add, del;
  mpegts_apid_t *pi;
  int i;

  lock_assert(&global_lock);
  if (pids) {
    p = calloc(1, sizeof(*p));
    mpegts_pid_init(p);
    mpegts_pid_copy(p, pids);
  } else
    p = NULL;
  if (mi && mm) {
    pthread_mutex_lock(&mi->mi_output_lock);
    pthread_mutex_lock(&t->s_stream_mutex);
    x = t->s_pids;
    t->s_pids = p;
    if (!pids->all && x && x->all) {
      mpegts_input_close_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW, MPS_WEIGHT_RAW, t);
      mpegts_input_close_pids(mi, mm, t, 1);
      for (i = 0; i < x->count; i++) {
        pi = &x->pids[i];
        mpegts_input_open_pid(mi, mm, pi->pid, MPS_RAW, pi->weight, t);
      }
    } else {
      if (pids->all) {
        mpegts_input_close_pids(mi, mm, t, 1);
        mpegts_input_open_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW, MPS_WEIGHT_RAW, t);
      } else {
        mpegts_pid_compare(p, x, &add, &del);
        for (i = 0; i < del.count; i++) {
          pi = &del.pids[i];
          mpegts_input_close_pid(mi, mm, pi->pid, MPS_RAW, pi->weight, t);
        }
        for (i = 0; i < add.count; i++) {
          pi = &add.pids[i];
          mpegts_input_open_pid(mi, mm, pi->pid, MPS_RAW, pi->weight, t);
        }
        mpegts_pid_done(&add);
        mpegts_pid_done(&del);
      }
    }
    pthread_mutex_unlock(&t->s_stream_mutex);
    pthread_mutex_unlock(&mi->mi_output_lock);
    mpegts_mux_update_pids(mm);
  } else {
    pthread_mutex_lock(&t->s_stream_mutex);
    x = t->s_pids;
    t->s_pids = p;
    pthread_mutex_unlock(&t->s_stream_mutex);
  }
  if (x) {
    mpegts_pid_done(x);
    free(x);
  }
  return 0;
}

static int
mpegts_service_link ( mpegts_service_t *master, mpegts_service_t *slave )
{
  pthread_mutex_lock(&master->s_stream_mutex);
  LIST_INSERT_HEAD(&slave->s_masters, master, s_masters_link);
  LIST_INSERT_HEAD(&master->s_slaves, slave, s_slaves_link);
  pthread_mutex_unlock(&master->s_stream_mutex);
  return 0;
}

static int
mpegts_service_unlink ( mpegts_service_t *master, mpegts_service_t *slave )
{
  pthread_mutex_lock(&master->s_stream_mutex);
  LIST_SAFE_REMOVE(master, s_masters_link);
  LIST_SAFE_REMOVE(slave, s_slaves_link);
  pthread_mutex_unlock(&master->s_stream_mutex);
  return 0;
}

mpegts_service_t *
mpegts_service_create_raw ( mpegts_mux_t *mm )
{
  mpegts_service_t *s = calloc(1, sizeof(*s));
  char buf[256];

  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  if (service_create0((service_t*)s, STYPE_RAW,
                      &mpegts_service_raw_class, NULL,
                      S_MPEG_TS, NULL) == NULL) {
    free(s);
    return NULL;
  }

  sbuf_init(&s->s_tsbuf);

  s->s_dvb_mux        = mm;

  s->s_delete         = mpegts_service_delete;
  s->s_is_enabled     = mpegts_service_is_enabled;
  s->s_config_save    = mpegts_service_config_save;
  s->s_enlist         = mpegts_service_enlist;
  s->s_start_feed     = mpegts_service_start;
  s->s_stop_feed      = mpegts_service_stop;
  s->s_refresh_feed   = mpegts_service_refresh;
  s->s_setsourceinfo  = mpegts_service_raw_setsourceinfo;
  s->s_grace_period   = mpegts_service_grace_period;
  s->s_channel_number = mpegts_service_channel_number;
  s->s_channel_name   = mpegts_service_channel_name;
  s->s_provider_name  = mpegts_service_provider_name;
  s->s_channel_icon   = mpegts_service_channel_icon;
  s->s_mapped         = mpegts_service_mapped;
  s->s_update_pids    = mpegts_service_raw_update_pids;
  s->s_link           = mpegts_service_link;
  s->s_unlink         = mpegts_service_unlink;
  s->s_satip_source   = mpegts_service_satip_source;

  pthread_mutex_lock(&s->s_stream_mutex);
  free(s->s_nicename);
  s->s_nicename = strdup(buf);
  pthread_mutex_unlock(&s->s_stream_mutex);

  tvhlog(LOG_DEBUG, "mpegts", "%s - add raw service", buf);

  return s;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
