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

#include "tvheadend.h"
#include "service.h"
#include "channels.h"
#include "input.h"
#include "settings.h"
#include "dvb_charset.h"
#include "config.h"
#include "epggrab.h"
#include "descrambler/dvbcam.h"

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
  mpegts_service_t *ms = ptr;
  if (ms && ms->s_dvb_mux)
    idnode_uuid_as_str(&ms->s_dvb_mux->mm_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
    { N_("Only preferred CA PID"),  2 },
  };
   return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
mpegts_service_subtitle_procesing ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("None"),                         SVC_PROCESS_SUBTITLE_NONE    }, //No processing.
    { N_("Save in Description"),          SVC_PROCESS_SUBTITLE_DESC    }, //Save the sub-title in the desc if desc is empty.
    { N_("Append to Description"),        SVC_PROCESS_SUBTITLE_APPEND  }, //Append, but if the desc is empty, just replace.
    { N_("Prepend to Description"),       SVC_PROCESS_SUBTITLE_PREPEND }, //Prepend, but if the desc is empty, just replace.
  };
   return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(mpegts_service)

const idclass_t mpegts_service_class =
{
  .ic_super      = &service_class,
  .ic_class      = "mpegts_service",
  .ic_caption    = N_("DVB Inputs - Services"),
  .ic_doc        = tvh_doc_mpegts_service_class,
  .ic_order      = "enabled,channel,svcname",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = N_("Network"),
      .desc     = N_("The network the service is on."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "multiplex",
      .name     = N_("Mux"),
      .desc     = N_("The mux the service is on."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_service_class_get_mux,
    },
    {
      .type     = PT_STR,
      .id       = "multiplex_uuid",
      .name     = N_("Mux UUID"),
      .desc     = N_("The mux's universally unique identifier."),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_EXPERT,
      .get      = mpegts_service_class_get_mux_uuid,
    },
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = N_("Service ID"),
      .desc     = N_("The service ID as set by the provider."),
      .opts     = PO_RDONLY | PO_ADVANCED,
      .off      = offsetof(mpegts_service_t, s_components.set_service_id),
    },
    {
      .type     = PT_U16,
      .id       = "lcn",
      .name     = N_("Local channel number"),
      .desc     = N_("The service's channel number as set by the provider."),
      .opts     = PO_RDONLY | PO_ADVANCED,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_num),
    },
    {
      .type     = PT_U16,
      .id       = "lcn_minor",
      .name     = N_("Local channel minor"),
      .desc     = N_("The service's channel minor as set by the provider."),
      .opts     = PO_RDONLY | PO_EXPERT,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_minor),
    },
    {
      .type     = PT_U16,
      .id       = "lcn2",
      .name     = N_("OpenTV channel number"),
      .desc     = N_("The OpenTV channel number as set by the provider."),
      .opts     = PO_RDONLY | PO_EXPERT,
      .off      = offsetof(mpegts_service_t, s_dvb_opentv_chnum),
    },
    {
      .type     = PT_U16,
      .id       = "srcid",
      .name     = N_("ATSC source ID"),
      .desc     = N_("The ATSC source ID as set by the provider."),
      .opts     = PO_RDONLY | PO_EXPERT,
      .off      = offsetof(mpegts_service_t, s_atsc_source_id),
    },
    {
      .type     = PT_STR,
      .id       = "svcname",
      .name     = N_("Service name"),
      .desc     = N_("The service name as set by the provider."),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_svcname),
    },
    {
      .type     = PT_STR,
      .id       = "provider",
      .name     = N_("Provider"),
      .desc     = N_("The provider's name."),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_service_t, s_dvb_provider),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = N_("CRID authority"),
      .desc     = N_("Content reference identifier authority."),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
      .off      = offsetof(mpegts_service_t, s_dvb_cridauth),
    },
    {
      .type     = PT_U16,
      .id       = "dvb_servicetype",
      .name     = N_("Service type"),
      .desc     = N_("The service type flag as defined by the DVB "
                     "specifications (e.g. 0x02 = radio, 0x11 = MPEG2 "
                     "HD TV, 0x19 = H.264 HD TV)"),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
      .off      = offsetof(mpegts_service_t, s_dvb_servicetype),
    },
    {
      .type     = PT_BOOL,
      .id       = "dvb_ignore_eit",
      .name     = N_("Ignore EPG (EIT)"),
      .desc     = N_("Enable or disable ignoring of Event Information "
                     "Table (EIT) data for this service."),
      .off      = offsetof(mpegts_service_t, s_dvb_ignore_eit),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_INT,
      .id       = "dvb_subtitle_processing",
      .name     = N_("DVB Sub-title Processing"),
      .desc     = N_("Select action to be taken with the Sub-title "
                     "provided by the broadcaster: None; Save in Description; "
                     "Append to Description; Prepend to Description. "
                     "If the Description is empty, save, "
                     "append and prepend will replace the Description."),
      .off      = offsetof(mpegts_service_t, s_dvb_subtitle_processing),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .list     = mpegts_service_subtitle_procesing,
    },
    {
      .type     = PT_BOOL,
      .id       = "dvb_ignore_matching_subtitle",
      .name     = N_("Skip Sub-title matches Title"),
      .desc     = N_("If the Sub-title and the Title contain identical content, "
                     "ignore the Sub-title and only save the Title."),
      .off      = offsetof(mpegts_service_t, s_dvb_ignore_matching_subtitle),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character set"),
      .desc     = N_("The character encoding for this service (e.g. UTF-8)."),
      .off      = offsetof(mpegts_service_t, s_dvb_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U16,
      .id       = "prefcapid",
      .name     = N_("Preferred CA PID"),
      .desc     = N_("The Preferred Conditional Access Packet "
                     "Identifier. Used for decrypting scrambled streams."),
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_INT,
      .id       = "prefcapid_lock",
      .name     = N_("Lock preferred CA PID"),
      .desc     = N_("The locking mechanism selection for The Preferred "
                     "Conditional Access Packet Identifier. See Help "
                     "for more information."),
      .off      = offsetof(mpegts_service_t, s_dvb_prefcapid_lock),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .list     = mpegts_service_pref_capid_lock_list,
    },
    {
      .type     = PT_U16,
      .id       = "force_caid",
      .name     = N_("Force CA ID (e.g. 0x2600)"),
      .desc     = N_("Force usage of entered CA ID on this service."),
      .off      = offsetof(mpegts_service_t, s_dvb_forcecaid),
      .opts     = PO_EXPERT | PO_HEXA,
    },
    {
      .type     = PT_INT,
      .id       = "pts_shift",
      .name     = N_("Shift PTS (ms)"),
      .desc     = N_("Add this value to PTS for the teletext subtitles. The time value is in milliseconds and may be negative."),
      .off      = offsetof(mpegts_service_t, s_pts_shift),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_TIME,
      .id       = "created",
      .name     = N_("Created"),
      .desc     = N_("When the service was first identified and recorded."),
      .off      = offsetof(mpegts_service_t, s_dvb_created),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "last_seen",
      .name     = N_("Last seen"),
      .desc     = N_("When the service was last seen during a mux scan."),
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
  if (!s->s_verified) return 0;
  return mm->mm_is_enabled(mm) ? s->s_enabled : 0;
}

static int
mpegts_service_is_enabled_raw(service_t *t, int flags)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_mux_t *mm    = s->s_dvb_mux;
  return mm->mm_is_enabled(mm) ? s->s_enabled : 0;
}

/*
 * Save
 */
static htsmsg_t *
mpegts_service_config_save ( service_t *t, char *filename, size_t fsize )
{
  if (filename == NULL) {
    htsmsg_t *e = htsmsg_create_map();
    service_save(t, e);
    return e;
  }
  idnode_changed(&((mpegts_service_t *)t)->s_dvb_mux->mm_id);
  return NULL;
}

/*
 * Service instance list
 */
static int
mpegts_service_enlist_raw
  ( service_t *t, tvh_input_t *ti, struct service_instance_list *sil,
    int flags, int weight )
{
  int p, w, r, added = 0, errcnt = 0;
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

    r = mi->mi_is_enabled(mi, mmi->mmi_mux, flags, weight);
    if (r == MI_IS_ENABLED_NEVER) {
      tvhtrace(LS_MPEGTS, "enlist: input %p not enabled for mux %p service %s weight %d flags %x",
                          mi, mmi->mmi_mux, s->s_nicename, weight, flags);
      continue;
    }
    if (r == MI_IS_ENABLED_RETRY) {
      /* temporary error - retry later */
      tvhtrace(LS_MPEGTS, "enlist: input %p postponed for mux %p service %s weight %d flags %x",
                          mi, mmi->mmi_mux, s->s_nicename, weight, flags);
      errcnt++;
      continue;
    }

    /* Set weight to -1 (forced) for already active mux */
    if (mmi->mmi_mux->mm_active == mmi) {
      w = -1;
      p = -1;
    } else {
      w = mi->mi_get_weight(mi, mmi->mmi_mux, flags, weight);
      p = mi->mi_get_priority(mi, mmi->mmi_mux, flags);
      if (w > 0 && mi->mi_free_weight &&
          weight >= mi->mi_free_weight && w < mi->mi_free_weight)
        w = 0;
    }

    service_instance_add(sil, t, mi->mi_instance, mi->mi_name, p, w);
    added++;
  }

  return added ? 0 : (errcnt ? SM_CODE_NO_FREE_ADAPTER : 0);
}

/*
 * Service instance list
 */
static int
mpegts_service_enlist
  ( service_t *t, tvh_input_t *ti, struct service_instance_list *sil,
    int flags, int weight )
{
  const uint16_t pid = t->s_components.set_pmt_pid;

  /* invalid PMT */
  if (pid != SERVICE_PMT_AUTO && (pid <= 0 || pid >= 8191))
    return SM_CODE_INVALID_SERVICE;

  return mpegts_service_enlist_raw(t, ti, sil, flags, weight);
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
  r = mpegts_mux_instance_start(&mmi, t, weight);

  /* Start */
  if (!r) {

    /* Open service */
    s->s_dvb_subscription_flags = flags;
    s->s_dvb_subscription_weight = weight;
    mmi->mmi_input->mi_open_service(mmi->mmi_input, s, flags, 1, weight);
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
  i->mi_open_service(i, s, s->s_dvb_subscription_flags, 0, s->s_dvb_subscription_weight);
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

  /* Update */
  memset(si, 0, sizeof(struct source_info));
  si->si_type = S_MPEG_TS;

  uuid_duplicate(&si->si_network_uuid, &m->mm_network->mn_id.in_uuid);
  uuid_duplicate(&si->si_mux_uuid, &m->mm_id.in_uuid);

  if(m->mm_network->mn_network_name != NULL)
    si->si_network = strdup(m->mm_network->mn_network_name);
  mpegts_network_get_type_str(m->mm_network, buf, sizeof(buf));
  si->si_network_type = strdup(buf);

  m->mm_display_name(m, buf, sizeof(buf));
  si->si_mux = strdup(buf);

  if(s->s_dvb_active_input) {
    mpegts_input_t *mi = s->s_dvb_active_input;
    uuid_duplicate(&si->si_adapter_uuid, &mi->ti_id.in_uuid);
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

  si->si_tsid = m->mm_tsid;
  si->si_onid = m->mm_onid;
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
  int64_t r = 0;

  if (!ms->s_dvb_mux->mm_network->mn_ignore_chnum) {
    r = ms->s_dvb_channel_num * CHANNEL_SPLIT + ms->s_dvb_channel_minor;
    if (r <= 0)
      r = ms->s_dvb_opentv_chnum * CHANNEL_SPLIT;
  }
  if (r <= 0 && ms->s_dvb_mux->mm_network->mn_sid_chnum)
    r = service_id16(ms) * CHANNEL_SPLIT;
  return r;
}

static const char *
mpegts_service_channel_name ( service_t *s )
{
  return ((mpegts_service_t*)s)->s_dvb_svcname;
}

static const char *
mpegts_service_source ( service_t *s )
{
#if ENABLE_MPEGTS_DVB
  mpegts_service_t *ms = (mpegts_service_t*)s;
  const idclass_t *mux_idc = ms->s_dvb_mux->mm_id.in_class;
  if (mux_idc == &dvb_mux_dvbs_class)   return "DVB-S";
  if (mux_idc == &dvb_mux_dvbc_class)   return "DVB-C";
  if (mux_idc == &dvb_mux_dvbt_class)   return "DVB-T";
  if (mux_idc == &dvb_mux_atsc_t_class) return "ATSC-T";
  if (mux_idc == &dvb_mux_atsc_c_class) return "ATSC-C";
  if (mux_idc == &dvb_mux_isdb_t_class) return "ISDB-T";
  if (mux_idc == &dvb_mux_isdb_c_class) return "ISDB-C";
  if (mux_idc == &dvb_mux_isdb_s_class) return "ISDB-S";
  if (mux_idc == &dvb_mux_dtmb_class)   return "DTMB";
  if (mux_idc == &dvb_mux_dab_class)    return "DAB";
#endif
  return NULL;
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
      case DVB_TYPE_ATSC_C:
        hash = 0xFFFF0000;
        break;
      case DVB_TYPE_T:
        hash = 0xEEEE0000;
        break;
      case DVB_TYPE_ATSC_T:
        hash = 0xDDDD0000;
        break;
      default:
        return NULL;
    }

    snprintf(prop_sbuf, PROP_SBUF_LEN,
             "picon://1_0_%X_%X_%X_%X_%X_0_0_0.png",
             config.picon_scheme == PICON_ISVCTYPE ? 1 : ms->s_dvb_servicetype,
             service_id16(ms),
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
mpegts_service_match_network(mpegts_network_t *mn, uint32_t hash, const idclass_t *idc)
{
  int pos, pos2;

  if (idc != &dvb_mux_dvbs_class) return 1;
  pos = hash >> 16;
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

  switch (hash & 0xFFFF0000) {
  case 0xFFFF0000: idc = &dvb_mux_dvbc_class; break;
  case 0xEEEE0000: idc = &dvb_mux_dvbt_class; break;
  case 0xDDDD0000: idc = &dvb_mux_atsc_t_class; break;
  default:         idc = &dvb_mux_dvbs_class; break;
  }
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (!idnode_is_instance(&mn->mn_id, &dvb_network_class)) continue;
    if (dvb_network_mux_class(mn) != idc) continue;
    if (!mpegts_service_match_network(mn, hash, idc)) continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      if (!idnode_is_instance(&mm->mm_id, idc)) continue;
      if (mm->mm_tsid != tsid || mm->mm_onid != onid) continue;
      if (!mpegts_service_match_mux((dvb_mux_t *)mm, hash, idc)) continue;
      LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
        if (service_id16(s) == sid)
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
mpegts_service_unref ( service_t *t )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;

  free(ms->s_dvb_svcname);
  free(ms->s_dvb_provider);
  free(ms->s_dvb_cridauth);
  free(ms->s_dvb_charset);
}

void
mpegts_service_delete ( service_t *t, int delconf )
{
  mpegts_service_t *ms = (mpegts_service_t*)t, *mms;
  mpegts_mux_t     *mm = t->s_type == STYPE_STD ? ms->s_dvb_mux : NULL;

  if (mm)
    idnode_changed(&mm->mm_id);

  /* Free memory */
  if (t->s_type == STYPE_STD)
    LIST_REMOVE(ms, s_dvb_mux_link);
  sbuf_free(&ms->s_tsbuf);

  /* Remove master/slave linking */
  while (ms->s_masters.is_count > 0) {
    mms = (mpegts_service_t *)ms->s_masters.is_array[0];
    mms->s_unlink(mms, ms);
  }
  idnode_set_clear(&ms->s_masters);
  idnode_set_clear(&ms->s_slaves);

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

static mpegts_apids_t *
mpegts_service_pid_list_ ( service_t *t, void *owner )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;
  mpegts_apids_t *pids = NULL;
  mpegts_input_t *mi = ms->s_dvb_active_input;
  mpegts_mux_t *mm;
  mpegts_pid_sub_t *mps;
  mpegts_pid_t *mp;

  if (mi == NULL) return NULL;
  tvh_mutex_lock(&mi->mi_output_lock);
  mm = ms->s_dvb_mux;
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    RB_FOREACH(mps, &mp->mp_subs, mps_link) {
      if (owner == NULL || mps->mps_owner == owner) {
        if (pids == NULL)
          pids = mpegts_pid_alloc();
        mpegts_pid_add(pids, mp->mp_pid, 0);
        break;
      }
    }
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
  return pids;
}

static mpegts_apids_t *
mpegts_service_pid_list ( service_t *t )
{
  return mpegts_service_pid_list_(t, t);
}

static void
mpegts_service_memoryinfo ( service_t *t, int64_t *size )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;
  *size += sizeof(*ms);
  *size += tvh_strlen(ms->s_nicename);
  *size += tvh_strlen(ms->s_dvb_svcname);
  *size += tvh_strlen(ms->s_dvb_provider);
  *size += tvh_strlen(ms->s_dvb_cridauth);
  *size += tvh_strlen(ms->s_dvb_charset);
}

static int
mpegts_service_unseen( service_t *t, const char *type, time_t before )
{
  mpegts_service_t *ms = (mpegts_service_t*)t;
  int pat = type && strcasecmp(type, "pat") == 0;
  if (pat && ms->s_auto != SERVICE_AUTO_PAT_MISSING) return 0;
  return ms->s_dvb_last_seen < before;
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
  mpegts_network_t *mn = mm->mm_network;
  time_t dispatch_clock = gclk();

  /* defaults for older version */
  s->s_dvb_created = dispatch_clock;
  if (!conf) {
    if (sid)     s->s_components.set_service_id = sid;
    if (pmt_pid) s->s_components.set_pmt_pid = pmt_pid;
  }

  if (service_create0((service_t*)s, STYPE_STD, class, uuid,
                      S_MPEG_TS, conf) == NULL)
    return NULL;

  /* Create */
  sbuf_init(&s->s_tsbuf);
  if (conf) {
    if (s->s_dvb_last_seen > gclk()) /* sanity check */
      s->s_dvb_last_seen = gclk();
  }
  s->s_dvb_mux        = mm;
  if ((r = dvb_servicetype_lookup(s->s_dvb_servicetype)) != -1)
    s->s_servicetype = r;
  LIST_INSERT_HEAD(&mm->mm_services, s, s_dvb_mux_link);
  
  s->s_delete         = mpegts_service_delete;
  s->s_unref          = mpegts_service_unref;
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
  s->s_source         = mpegts_service_source;
  s->s_provider_name  = mpegts_service_provider_name;
  s->s_channel_icon   = mpegts_service_channel_icon;
  s->s_mapped         = mpegts_service_mapped;
  s->s_satip_source   = mpegts_service_satip_source;
  s->s_pid_list       = mpegts_service_pid_list;
  s->s_memoryinfo     = mpegts_service_memoryinfo;
  s->s_unseen         = mpegts_service_unseen;

  tvh_mutex_lock(&s->s_stream_mutex);
  service_make_nicename((service_t*)s);
  tvh_mutex_unlock(&s->s_stream_mutex);

  tvhdebug(LS_MPEGTS, "%s - add service %04X %s",
           mm->mm_nicename, service_id16(s), s->s_dvb_svcname);

  /* Bouquet */
  mpegts_network_bouquet_trigger(mn, 1);

  /* Notification */
  idnode_notify_changed(&mm->mm_id);
  idnode_notify_changed(&mm->mm_network->mn_id);

  /* Save the create time */
  if (s->s_dvb_created == dispatch_clock)
    service_request_save((service_t *)s);

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

  if (mm->mm_sid_filter > 0 && sid != mm->mm_sid_filter)
    return NULL;

  /* Find existing service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
    if (service_id16(s) == sid) {
      if (pmt_pid && pmt_pid != s->s_components.set_pmt_pid) {
        s->s_components.set_pmt_pid = pmt_pid;
        if (s->s_pmt_mon)
          mpegts_input_open_pmt_monitor(mm, s);
        if (save) *save = 1;
      }
      if (create) {
        if ((save && *save) || s->s_dvb_last_seen + 3600 < gclk()) {
          s->s_dvb_last_seen = gclk();
          if (save) *save = 1;
        }
      }
      return s;
    }
  }

  /* Create */
  if (create) {
    s = mm->mm_network->mn_create_service(mm, sid, pmt_pid);
    s->s_dvb_created = s->s_dvb_last_seen = gclk();
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
    tvh_mutex_lock(&s->s_stream_mutex);
    if (pid == s->s_components.set_pmt_pid ||
        pid == s->s_components.set_pcr_pid)
      goto ok;
    if (elementary_stream_find(&s->s_components, pid))
      goto ok;
    tvh_mutex_unlock(&s->s_stream_mutex);
  }
  return NULL;
ok:
  tvh_mutex_unlock(&s->s_stream_mutex);
  return s;
}

/*
 * Auto-enable service
 */
void
mpegts_service_autoenable( mpegts_service_t *s, const char *where )
{
  if (!s->s_enabled && s->s_auto == SERVICE_AUTO_PAT_MISSING) {
    tvhinfo(LS_MPEGTS, "enabling service %s [sid %04X/%d] (found in %s)",
            s->s_nicename,
            service_id16(s),
            service_id16(s),
            where);
    service_set_enabled((service_t *)s, 1, SERVICE_AUTO_NORMAL);
  }
  s->s_dvb_check_seen = gclk();
}

/*
 * Raw MPEGTS Service
 */

const idclass_t mpegts_service_raw_class =
{
  .ic_super      = &service_raw_class,
  .ic_class      = "mpegts_raw_service",
  .ic_caption    = N_("MPEG-TS raw service"),
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
    tvh_mutex_lock(&mi->mi_output_lock);
    tvh_mutex_lock(&t->s_stream_mutex);
    x = t->s_pids;
    t->s_pids = p;
    if (pids && !pids->all && x && x->all) {
      mpegts_input_close_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW, t);
      mpegts_input_close_pids(mi, mm, t, 1);
      for (i = 0; i < x->count; i++) {
        pi = &x->pids[i];
        mpegts_input_open_pid(mi, mm, pi->pid, MPS_RAW, pi->weight, t, 0);
      }
    } else {
      if (pids && pids->all) {
        mpegts_input_close_pids(mi, mm, t, 1);
        mpegts_input_open_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW, MPS_WEIGHT_RAW, t, 0);
      } else {
        mpegts_pid_compare(p, x, &add, &del);
        for (i = 0; i < del.count; i++) {
          pi = &del.pids[i];
          mpegts_input_close_pid(mi, mm, pi->pid, MPS_RAW, t);
        }
        for (i = 0; i < add.count; i++) {
          pi = &add.pids[i];
          mpegts_input_open_pid(mi, mm, pi->pid, MPS_RAW, pi->weight, t, 0);
        }
        if (p) {
          for (i = 0; i < p->count; i++) {
            pi = &p->pids[i];
            mpegts_input_update_pid_weight(mi, mm, pi->pid, MPS_RAW, pi->weight, t);
          }
        }
        mpegts_pid_done(&add);
        mpegts_pid_done(&del);
      }
    }
    tvh_mutex_unlock(&t->s_stream_mutex);
    tvh_mutex_unlock(&mi->mi_output_lock);
    mpegts_mux_update_pids(mm);
  } else {
    tvh_mutex_lock(&t->s_stream_mutex);
    x = t->s_pids;
    t->s_pids = p;
    tvh_mutex_unlock(&t->s_stream_mutex);
  }
  if (x) {
    mpegts_pid_done(x);
    free(x);
  }
  return 0;
}

void
mpegts_service_update_slave_pids
  ( mpegts_service_t *s, mpegts_service_t *master, int del )
{
  mpegts_service_t *s2;
  mpegts_apids_t *pids;
  elementary_stream_t *st;
  int i;
  const int is_ddci = dvbcam_is_ddci((service_t*)s);

  lock_assert(&s->s_stream_mutex);

  if (s->s_components.set_pmt_pid == SERVICE_PMT_AUTO)
    return;

  pids = mpegts_pid_alloc();

  mpegts_pid_add(pids, s->s_components.set_pmt_pid, MPS_WEIGHT_PMT);
  mpegts_pid_add(pids, s->s_components.set_pcr_pid, MPS_WEIGHT_PCR);

  /* Ensure that filtered PIDs are not send in ts_recv_raw */
  TAILQ_FOREACH(st, &s->s_components.set_filter, es_filter_link)
    if ((is_ddci || s->s_scrambled_pass || st->es_type != SCT_CA) &&
        st->es_pid >= 0 && st->es_pid < 8192)
      mpegts_pid_add(pids, st->es_pid, mpegts_mps_weight(st));

  for (i = 0; i < s->s_masters.is_count; i++) {
    s2 = (mpegts_service_t *)s->s_masters.is_array[i];
    if (master && master != s2) continue;
    tvh_mutex_lock(&s2->s_stream_mutex);
    if (!del)
      mpegts_pid_add_group(s2->s_slaves_pids, pids);
    else
      mpegts_pid_del_group(s2->s_slaves_pids, pids);
    tvh_mutex_unlock(&s2->s_stream_mutex);
  }

  mpegts_pid_destroy(&pids);
}

static int
mpegts_service_link ( mpegts_service_t *master, mpegts_service_t *slave )
{
  tvh_mutex_lock(&slave->s_stream_mutex);
  tvh_mutex_lock(&master->s_stream_mutex);
  assert(!idnode_set_exists(&slave->s_masters, &master->s_id));
  idnode_set_alloc(&slave->s_masters, 16);
  idnode_set_add(&slave->s_masters, &master->s_id, NULL, NULL);
  idnode_set_alloc(&master->s_slaves, 16);
  idnode_set_add(&master->s_slaves, &slave->s_id, NULL, NULL);
  tvh_mutex_unlock(&master->s_stream_mutex);
  mpegts_service_update_slave_pids(slave, master, 0);
  tvh_mutex_unlock(&slave->s_stream_mutex);
  return 0;
}

static int
mpegts_service_unlink ( mpegts_service_t *master, mpegts_service_t *slave )
{
  tvh_mutex_lock(&slave->s_stream_mutex);
  mpegts_service_update_slave_pids(slave, master, 1);
  tvh_mutex_lock(&master->s_stream_mutex);
  idnode_set_remove(&slave->s_masters, &master->s_id);
  if (idnode_set_empty(&slave->s_masters))
    idnode_set_clear(&slave->s_masters);
  idnode_set_remove(&master->s_slaves, &slave->s_id);
  if (idnode_set_empty(&master->s_slaves))
    idnode_set_clear(&master->s_slaves);
  tvh_mutex_unlock(&master->s_stream_mutex);
  tvh_mutex_unlock(&slave->s_stream_mutex);
  return 0;
}

static mpegts_apids_t *
mpegts_service_raw_pid_list ( service_t *t )
{
  return mpegts_service_pid_list_(t, NULL);
}

mpegts_service_t *
mpegts_service_create_raw ( mpegts_mux_t *mm )
{
  mpegts_service_t *s = calloc(1, sizeof(*s));

  if (service_create0((service_t*)s, STYPE_RAW,
                      &mpegts_service_raw_class, NULL,
                      S_MPEG_TS, NULL) == NULL) {
    free(s);
    return NULL;
  }

  sbuf_init(&s->s_tsbuf);

  s->s_dvb_mux        = mm;

  s->s_delete         = mpegts_service_delete;
  s->s_is_enabled     = mpegts_service_is_enabled_raw;
  s->s_config_save    = mpegts_service_config_save;
  s->s_enlist         = mpegts_service_enlist_raw;
  s->s_start_feed     = mpegts_service_start;
  s->s_stop_feed      = mpegts_service_stop;
  s->s_refresh_feed   = mpegts_service_refresh;
  s->s_setsourceinfo  = mpegts_service_raw_setsourceinfo;
  s->s_grace_period   = mpegts_service_grace_period;
  s->s_channel_number = mpegts_service_channel_number;
  s->s_channel_name   = mpegts_service_channel_name;
  s->s_source         = mpegts_service_source;
  s->s_provider_name  = mpegts_service_provider_name;
  s->s_channel_icon   = mpegts_service_channel_icon;
  s->s_mapped         = mpegts_service_mapped;
  s->s_update_pids    = mpegts_service_raw_update_pids;
  s->s_link           = mpegts_service_link;
  s->s_unlink         = mpegts_service_unlink;
  s->s_satip_source   = mpegts_service_satip_source;
  s->s_pid_list       = mpegts_service_raw_pid_list;
  s->s_memoryinfo     = mpegts_service_memoryinfo;

  tvh_mutex_lock(&s->s_stream_mutex);
  free(s->s_nicename);
  s->s_nicename = strdup(mm->mm_nicename);
  tvh_mutex_unlock(&s->s_stream_mutex);

  tvhdebug(LS_MPEGTS, "%s - add raw service", mm->mm_nicename);

  return s;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
