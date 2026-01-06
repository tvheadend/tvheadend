/*
 *  Tvheadend - MPEGTS multiplex
 *
 *  Copyright (C) 2013 Adam Sutton
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

#include "idnode.h"
#include "queue.h"
#include "input.h"
#include "subscriptions.h"
#include "streaming.h"
#include "channels.h"
#include "access.h"
#include "profile.h"
#include "dvb_charset.h"
#include "epggrab.h"
#include "config.h"

#include <assert.h>

static void mpegts_mux_scan_timeout ( void *p );
static void mpegts_mux_do_stop ( mpegts_mux_t *mm, int delconf );


/* ****************************************************************************
 * Mux instance (input linkage)
 * ***************************************************************************/

const idclass_t mpegts_mux_instance_class =
{
  .ic_super      = &tvh_input_instance_class,
  .ic_class      = "mpegts_mux_instance",
  .ic_caption    = N_("MPEG-TS multiplex PHY"),
  .ic_perm_def   = ACCESS_ADMIN
};

void
mpegts_mux_instance_delete
  ( tvh_input_instance_t *tii )
{
  mpegts_mux_instance_t *mmi = (mpegts_mux_instance_t *)tii;

  idnode_save_check(&tii->tii_id, 1);
  idnode_unlink(&tii->tii_id);
  LIST_REMOVE(mmi, mmi_mux_link);
  LIST_REMOVE(tii, tii_input_link);
  tvh_mutex_destroy(&mmi->tii_stats_mutex);
  free(mmi);
}

mpegts_mux_instance_t *
mpegts_mux_instance_create0
  ( mpegts_mux_instance_t *mmi, const idclass_t *class, const char *uuid,
    mpegts_input_t *mi, mpegts_mux_t *mm )
{
  // TODO: does this need to be an idnode?
  if (idnode_insert(&mmi->tii_id, uuid, class, 0)) {
    free(mmi);
    return NULL;
  }

  tvh_mutex_init(&mmi->tii_stats_mutex, NULL);

  /* Setup links */
  mmi->mmi_mux   = mm;
  mmi->mmi_input = mi;
  
  /* Callbacks */
  mmi->tii_delete = mpegts_mux_instance_delete;
  mmi->tii_clear_stats = tvh_input_instance_clear_stats;

  LIST_INSERT_HEAD(&mm->mm_instances, mmi, mmi_mux_link);
  LIST_INSERT_HEAD(&mi->mi_mux_instances, (tvh_input_instance_t *)mmi, tii_input_link);

  return mmi;
}

static void
mpegts_mux_scan_active
  ( mpegts_mux_t *mm, const char *buf, mpegts_input_t *mi )
{
  int t;

  /* Setup scan */
  if (mm->mm_scan_state == MM_SCAN_STATE_PEND ||
      mm->mm_scan_state == MM_SCAN_STATE_IPEND) {
    mpegts_network_scan_mux_active(mm);

    /* Get timeout */
    t = mpegts_input_grace(mi, mm);
  
    /* Setup timeout */
    mtimer_arm_rel(&mm->mm_scan_timeout, mpegts_mux_scan_timeout, mm, sec2mono(t));
  }
}

static int
mpegts_mux_keep_exists
  ( mpegts_input_t *mi )
{
  const mpegts_mux_instance_t *mmi;
  const service_t *s;
  const th_subscription_t *ths;
  int ret;

  lock_assert(&global_lock);

  if (!mi)
    return 0;

  ret = 0;
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    LIST_FOREACH(ths, &mmi->mmi_mux->mm_raw_subs, ths_mux_link) {
      s = ths->ths_service;
      if (s && s->s_type == STYPE_RAW && !strcmp(ths->ths_title, "keep")) {
        ret = 1;
        break;
      }
    }
  return ret;
}

static int
mpegts_mux_subscribe_keep
  ( mpegts_mux_t *mm, mpegts_input_t *mi )
{
  char *s;
  int r;

  s = mi->mi_linked;
  mi->mi_linked = NULL;
  tvhtrace(LS_MPEGTS, "subscribe keep for '%s' (%p)", mi->mi_name, mm);
  r = mpegts_mux_subscribe(mm, mi, "keep", SUBSCRIPTION_PRIO_KEEP,
                           SUBSCRIPTION_ONESHOT | SUBSCRIPTION_MINIMAL);
  mi->mi_linked = s;
  return r;
}

static void
mpegts_mux_subscribe_linked
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_input_t *mi2 = mpegts_input_find(mi->mi_linked);
  mpegts_mux_instance_t *mmi2;
  mpegts_network_link_t *mnl2;
  mpegts_mux_t *mm2;
  char buf1[128], buf2[128];
  const char *serr = "All";
  int r = 0;

  tvhtrace(LS_MPEGTS, "subscribe linked");

  if (!mi2)
    return;
  
  if (!mpegts_mux_keep_exists(mi) && (r = mpegts_mux_subscribe_keep(mm, mi))) {
    serr = "active1";
    goto fatal;
  }

  if (mpegts_mux_keep_exists(mi2))
    return;

  mmi2 = LIST_FIRST(&mi2->mi_mux_active);
  if (mmi2) {
    if (!mpegts_mux_subscribe_keep(mmi2->mmi_mux, mi2))
      return;
    serr = "active2";
    goto fatal;
  }

  /* Try muxes within same network */
  LIST_FOREACH(mnl2, &mi2->mi_networks, mnl_mi_link)
    if (mnl2->mnl_network == mm->mm_network)
      LIST_FOREACH(mm2, &mnl2->mnl_network->mn_muxes, mm_network_link)
        if (!mm2->mm_active && MM_SCAN_CHECK_OK(mm) &&
            !LIST_EMPTY(&mm2->mm_services))
          if (!mpegts_mux_subscribe_keep(mm2, mi2))
            return;

  /* Try all other muxes */
  LIST_FOREACH(mnl2, &mi2->mi_networks, mnl_mi_link)
    if (mnl2->mnl_network != mm->mm_network)
      LIST_FOREACH(mm2, &mnl2->mnl_network->mn_muxes, mm_network_link)
        if (!mm2->mm_active && MM_SCAN_CHECK_OK(mm) &&
            !LIST_EMPTY(&mm2->mm_services))
          if (!mpegts_mux_subscribe_keep(mm2, mi2))
            return;

fatal:
  mi ->mi_display_name(mi,  buf1, sizeof(buf1));
  mi2->mi_display_name(mi2, buf2, sizeof(buf2));
  tvherror(LS_MPEGTS, "%s - %s - linked input cannot be started (%s: %i)", buf1, buf2, serr, r);
}

void
mpegts_mux_unsubscribe_linked
  ( mpegts_input_t *mi, service_t *t )
{
  th_subscription_t *ths, *ths_next;

  if (mi) {
    tvhtrace(LS_MPEGTS, "unsubscribing linked");

    for (ths = LIST_FIRST(&subscriptions); ths; ths = ths_next) {
      ths_next = LIST_NEXT(ths, ths_global_link);
      if (ths->ths_source == (tvh_input_t *)mi && !strcmp(ths->ths_title, "keep") &&
          ths->ths_service != t)
        subscription_unsubscribe(ths, UNSUBSCRIBE_FINAL);
    }
  }
}

int
mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr, service_t *t, int weight )
{
  int r;
  char buf[256];
  mpegts_mux_instance_t *mmi = *mmiptr;
  mpegts_mux_t          * mm = mmi->mmi_mux;
  mpegts_input_t        * mi = mmi->mmi_input;
  mpegts_service_t      *  s;

  /* Already active */
  if (mm->mm_active) {
    *mmiptr = mm->mm_active;
    tvhdebug(LS_MPEGTS, "%s - already active", mm->mm_nicename);
    mpegts_mux_scan_active(mm, buf, (*mmiptr)->mmi_input);
    return 0;
  }

  /* Update nicename */
  mpegts_mux_update_nice_name(mm);

  /* Dead service check */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    s->s_dvb_check_seen = s->s_dvb_last_seen;

  mm->mm_tsid_checks = 0;

  /* Start */
  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhinfo(LS_MPEGTS, "%s - tuning on %s", mm->mm_nicename, buf);

  if (mi->mi_linked)
    mpegts_mux_unsubscribe_linked(mi, t);

  r = mi->mi_warm_mux(mi, mmi);
  if (r) return r;
  mm->mm_input_pos = 0;
  r = mi->mi_start_mux(mi, mmi, weight);
  if (r) return r;

  /* Start */
  tvhdebug(LS_MPEGTS, "%s - started", mm->mm_nicename);
  mm->mm_start_monoclock = mclk();
  mi->mi_started_mux(mi, mmi);

  /* Reset Error Counters */
  if (config.auto_clear_input_counters && mmi->tii_clear_stats) {
    mmi->tii_clear_stats((tvh_input_instance_t *)mmi);
  }

  /* Event handler */
  mpegts_fire_event(mm, ml_mux_start);

  /* Link */
  mpegts_mux_scan_active(mm, buf, mi);

  if (mi->mi_linked)
    mpegts_mux_subscribe_linked(mi, mm);

  return 0;
}

int
mpegts_mux_instance_weight ( mpegts_mux_instance_t *mmi )
{
  int w = 0;
  const service_t *s;
  const th_subscription_t *ths;
  mpegts_mux_t *mm = mmi->mmi_mux;
  lock_assert(&mmi->mmi_input->mi_output_lock);

  /* Service subs */
  LIST_FOREACH(s, &mm->mm_transports, s_active_link)
    LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link)
      w = MAX(w, ths->ths_weight);

  return w;
}

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

static htsmsg_t *
mpegts_mux_class_save ( idnode_t *self, char *filename, size_t fsize )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)self;
  if (mm->mm_config_save)
    return mm->mm_config_save(mm, filename, fsize);
  return NULL;
}

static void
mpegts_mux_class_delete ( idnode_t *self )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)self;
  if (mm->mm_delete) mm->mm_delete(mm, 1);
}

static void
mpegts_mux_class_get_title
  ( idnode_t *self, const char *lang, char *dst, size_t dstsize )
{
  mpegts_mux_nice_name((mpegts_mux_t*)self, dst, dstsize);
}

static const void *
mpegts_mux_class_get_num_svc ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm = ptr;
  mpegts_service_t *s;

  n = 0;
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    n++;

  return &n;
}

static const void *
mpegts_mux_class_get_num_chn ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm = ptr;
  mpegts_service_t *s;
  idnode_list_mapping_t *ilm;

  n = 0;
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    LIST_FOREACH(ilm, &s->s_channels, ilm_in1_link)
      n++;

  return &n;
}

static const void *
mpegts_mux_class_get_network ( void *ptr )
{
  static char buf[512], *s = buf;
  mpegts_mux_t *mm = ptr;
  if (mm && mm->mm_network && mm->mm_network->mn_display_name)
    mm->mm_network->mn_display_name(mm->mm_network, buf, sizeof(buf));
  else
    *buf = 0;
  return &s;
}

static const void *
mpegts_mux_class_get_network_uuid ( void *ptr )
{
  mpegts_mux_t *mm = ptr;
  if (mm && mm->mm_network)
    idnode_uuid_as_str(&mm->mm_network->mn_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static const void *
mpegts_mux_class_get_name ( void *ptr )
{
  static char buf[512], *s = buf;
  mpegts_mux_t *mm = ptr;
  if (mm && mm->mm_display_name)
    mm->mm_display_name(mm, buf, sizeof(buf));
  else
    *buf = 0;
  return &s;
}

static struct strtab
scan_state_tab[] = {
  { N_("IDLE"),        MM_SCAN_STATE_IDLE },
  { N_("PEND"),        MM_SCAN_STATE_PEND },
  { N_("IDLE PEND"),   MM_SCAN_STATE_IPEND },
  { N_("ACTIVE"),      MM_SCAN_STATE_ACTIVE },
};

static struct strtab
scan_result_tab[] = {
 { N_("NONE"),         MM_SCAN_NONE },
 { N_("OK"),           MM_SCAN_OK   },
 { N_("FAIL"),         MM_SCAN_FAIL },
 { N_("OK (partial)"), MM_SCAN_PARTIAL },
 { N_("IGNORE"),       MM_SCAN_IGNORE },
};

int
mpegts_mux_class_scan_state_set ( void *o, const void *p )
{
  mpegts_mux_t *mm = o;
  int state = *(int*)p;

  /* Ignore */
  if (!mm->mm_is_enabled(mm))
    return 0;
  
  /* Start */
  if (state == MM_SCAN_STATE_PEND ||
      state == MM_SCAN_STATE_IPEND ||
      state == MM_SCAN_STATE_ACTIVE) {

    /* Start (only if required) */
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_USER,
                                  SUBSCRIPTION_USERSCAN, 0);

  /* Stop */
  } else if (state == MM_SCAN_STATE_IDLE) {

    /* No change */
    if (mm->mm_scan_state == MM_SCAN_STATE_IDLE)
      return 0;

    /* Update */
    mpegts_network_scan_mux_cancel(mm, 0);

  /* Invalid */
  } else {
  }

  return 1;
}

static htsmsg_t *
mpegts_mux_class_scan_state_enum ( void *p, const char *lang )
{
  return strtab2htsmsg(scan_state_tab, 1, lang);
}

static htsmsg_t *
mpegts_mux_class_scan_result_enum ( void *p, const char *lang )
{
  return strtab2htsmsg(scan_result_tab, 1, lang);
}

static void
mpegts_mux_class_enabled_notify ( void *p, const char *lang )
{
  mpegts_mux_t *mm = p;
  if (!mm->mm_is_enabled(mm)) {
    mm->mm_stop(mm, 1, SM_CODE_MUX_NOT_ENABLED);
    mpegts_network_scan_mux_cancel(mm, 0);
    if (mm->mm_enabled == MM_IGNORE) {
      mpegts_mux_do_stop(mm, 1);
      mm->mm_scan_result = MM_SCAN_IGNORE;
    }
  }
}

static htsmsg_t *
mpegts_mux_enable_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Ignore"),                   MM_IGNORE },
    { N_("Disable"),                  MM_DISABLE },
    { N_("Enable"),                   MM_ENABLE },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
mpegts_mux_epg_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Disable"),                  MM_EPG_DISABLE },
    { N_("Enable (auto)"),            MM_EPG_ENABLE },
    { N_("Force (auto)"),             MM_EPG_FORCE },
    { N_("Manual selection"),         MM_EPG_MANUAL },
    { N_("Auto-Detected"),            MM_EPG_DETECTED },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
mpegts_mux_epg_module_id_list ( void *o, const char *lang )
{
  return epggrab_ota_module_id_list(lang);
}

static htsmsg_t *
mpegts_mux_ac3_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Standard"),                 MM_AC3_STANDARD },
    { N_("AC-3 = descriptor 6"),      MM_AC3_PMT_06 },
    { N_("Ignore descriptor 5"),      MM_AC3_PMT_N05 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(mpegts_mux)

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = N_("DVB Inputs - Multiplex"),
  .ic_event      = "mpegts_mux",
  .ic_doc        = tvh_doc_mpegts_mux_class,
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = mpegts_mux_class_save,
  .ic_delete     = mpegts_mux_class_delete,
  .ic_get_title  = mpegts_mux_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable, disable or ignore the mux. "
                     "When the mux is marked as ignore, "
                     "all discovered services are removed."),
      .off      = offsetof(mpegts_mux_t, mm_enabled),
      .def.i    = MM_ENABLE,
      .list     = mpegts_mux_enable_list,
      .notify   = mpegts_mux_class_enabled_notify,
      .opts     = PO_DOC_NLIST
    },
    {
      .type     = PT_INT,
      .id       = "epg",
      .name     = N_("EPG scan"),
      .desc     = N_("The EPG grabber to use on the mux. "
                     "Enable (auto) is the recommended value."),
      .off      = offsetof(mpegts_mux_t, mm_epg),
      .def.i    = MM_EPG_ENABLE,
      .list     = mpegts_mux_epg_list,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "epg_module_id",
      .name     = N_("EPG module id"),
      .desc     = N_("The EPG grabber to use on the mux. "
                     "The 'EPG scan' field must be set to 'manual'."),
      .off      = offsetof(mpegts_mux_t, mm_epg_module_id),
      .list     = mpegts_mux_epg_module_id_list,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = N_("Network"),
      .desc     = N_("The network the mux is on."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "network_uuid",
      .name     = N_("Network UUID"),
      .desc     = N_("The networks' universally unique identifier (UUID)."),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN | PO_EXPERT,
      .get      = mpegts_mux_class_get_network_uuid,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("The name (or frequency) the mux is on."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_name,
    },
    {
      .type     = PT_STR,
      .id       = "pnetwork_name",
      .name     = N_("Provider network name"),
      .desc     = N_("The provider's network name."),
      .off      = offsetof(mpegts_mux_t, mm_provider_network_name),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_U32,
      .id       = "onid",
      .name     = N_("Original network ID"),
      .desc     = N_("The provider's network ID."),
      .opts     = PO_RDONLY | PO_ADVANCED,
      .off      = offsetof(mpegts_mux_t, mm_onid),
    },
    {
      .type     = PT_U32,
      .id       = "tsid",
      .name     = N_("Transport stream ID"),
      .desc     = N_("The transport stream ID of the mux within the "
                     "network."),
      .opts     = PO_RDONLY | PO_ADVANCED,
      .off      = offsetof(mpegts_mux_t, mm_tsid),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = N_("CRID authority"),
      .desc     = N_("The Content reference identifier (CRID) authority."),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
      .off      = offsetof(mpegts_mux_t, mm_crid_authority),
    },
    {
      .type     = PT_INT,
      .id       = "scan_state",
      .name     = N_("Scan status"),
      .desc     = N_("The scan state. New muxes will automatically be "
                     "changed to the PEND state. You can change this to "
                     "ACTIVE to queue a scan of this mux."),
      .off      = offsetof(mpegts_mux_t, mm_scan_state),
      .set      = mpegts_mux_class_scan_state_set,
      .list     = mpegts_mux_class_scan_state_enum,
      .opts     = PO_ADVANCED | PO_NOSAVE | PO_SORTKEY | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "scan_result",
      .name     = N_("Scan result"),
      .desc     = N_("The outcome of the last scan performed."),
      .off      = offsetof(mpegts_mux_t, mm_scan_result),
      .opts     = PO_RDONLY | PO_SORTKEY | PO_DOC_NLIST,
      .list     = mpegts_mux_class_scan_result_enum,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character set"),
      .desc     = N_("The character set to use/used. You should "
                     "not have to change this unless channel names "
                      "and EPG data appear garbled."),
      .off      = offsetof(mpegts_mux_t, mm_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = N_("Services"),
      .desc     = N_("The total number of services found."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_num_svc,
    },
    {
      .type     = PT_INT,
      .id       = "num_chn",
      .name     = N_("Mapped"),
      .desc     = N_("The number of services currently mapped to "
                     "channels."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_num_chn,
    },
    {
       .type     = PT_BOOL,
       .id       = "tsid_zero",
       .name     = N_("Accept zero value for TSID"),
       .off      = offsetof(mpegts_mux_t, mm_tsid_accept_zero_value),
       .opts     = PO_EXPERT
    },
    {
      .type     = PT_INT,
      .id       = "pmt_06_ac3",
      .name     = N_("AC-3 detection"),
      .desc     = N_("Use AC-3 detection."),
      .off      = offsetof(mpegts_mux_t, mm_pmt_ac3),
      .def.i    = MM_AC3_STANDARD,
      .list     = mpegts_mux_ac3_list,
      .opts     = PO_HIDDEN | PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_BOOL,
      .id       = "eit_tsid_nocheck",
      .name     = N_("EIT - skip TSID check"),
      .desc     = N_("Skip TSID checking. For when providers use invalid "
                     "Transport Stream IDs."),
      .off      = offsetof(mpegts_mux_t, mm_eit_tsid_nocheck),
      .opts     = PO_HIDDEN | PO_EXPERT
    },
    {
      .type     = PT_U16,
      .id       = "sid_filter",
      .name     = N_("Service ID"),
      .desc     = N_("Use only this service ID, filter out others."),
      .off      = offsetof(mpegts_mux_t, mm_sid_filter),
      .opts     = PO_HIDDEN | PO_EXPERT
    },
    {
      .type     = PT_TIME,
      .id       = "created",
      .name     = N_("Created"),
      .desc     = N_("When the mux was created."),
      .off      = offsetof(mpegts_mux_t, mm_created),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "scan_first",
      .name     = N_("First scan"),
      .desc     = N_("When the mux was successfully scanned for the first time."),
      .off      = offsetof(mpegts_mux_t, mm_scan_first),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {
      .type     = PT_TIME,
      .id       = "scan_last",
      .name     = N_("Last scan"),
      .desc     = N_("When the mux was successfully scanned."),
      .off      = offsetof(mpegts_mux_t, mm_scan_last_seen),
      .opts     = PO_ADVANCED | PO_RDONLY,
    },
    {}
  }
};

/* ****************************************************************************
 * Class methods
 * ***************************************************************************/

static void
mpegts_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  snprintf(buf, len, "Multiplex [onid:%04X tsid:%04X]",
           mm->mm_onid, mm->mm_tsid);
}

static void
mpegts_mux_do_stop ( mpegts_mux_t *mm, int delconf )
{
  mpegts_mux_instance_t *mmi;
  th_subscription_t *ths;
  mpegts_service_t *s;

  /* Cancel scan */
  mpegts_network_scan_queue_del(mm);

  /* Remove instances */
  while ((mmi = LIST_FIRST(&mm->mm_instances))) {
    mmi->tii_delete((tvh_input_instance_t *)mmi);
  }

  /* Remove raw subscribers */
  while ((ths = LIST_FIRST(&mm->mm_raw_subs))) {
    subscription_unsubscribe(ths, 0);
  }

  /* Delete services */
  while ((s = LIST_FIRST(&mm->mm_services))) {
    service_destroy((service_t*)s, delconf);
  }
}

void
mpegts_mux_free ( mpegts_mux_t *mm )
{
  free(mm->mm_provider_network_name);
  free(mm->mm_crid_authority);
  free(mm->mm_charset);
  free(mm->mm_epg_module_id);
  free(mm->mm_nicename);
  free(mm);
}

void
mpegts_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  idnode_save_check(&mm->mm_id, delconf);

  tvhinfo(LS_MPEGTS, "%s (%p) - deleting", mm->mm_nicename, mm);

  /* Stop */
  mm->mm_stop(mm, 1, SM_CODE_ABORTED);

  /* Remove from network */
  LIST_REMOVE(mm, mm_network_link);

  /* Real stop */
  mpegts_mux_do_stop(mm, delconf);

  /* Free memory */
  idnode_save_check(&mm->mm_id, 1);
  idnode_unlink(&mm->mm_id);
  mpegts_mux_release(mm);
}

static htsmsg_t *
mpegts_mux_config_save ( mpegts_mux_t *mm, char *filename, size_t fsize )
{
  return NULL;
}

static int
mpegts_mux_is_enabled ( mpegts_mux_t *mm )
{
  return mm->mm_network->mn_enabled && mm->mm_enabled == MM_ENABLE;
}

static int
mpegts_mux_is_epg ( mpegts_mux_t *mm )
{
  return mm->mm_epg;
}

static void
mpegts_mux_create_instances ( mpegts_mux_t *mm )
{
  mpegts_network_link_t *mnl;
  LIST_FOREACH(mnl, &mm->mm_network->mn_inputs, mnl_mn_link) {
    mpegts_input_t *mi = mnl->mnl_input;
    if (mi->mi_is_enabled(mi, mm, 0, -1) != MI_IS_ENABLED_NEVER)
      mi->mi_create_mux_instance(mi, mm);
  }
}

static int
mpegts_mux_has_subscribers ( mpegts_mux_t *mm, const char *name )
{
  mpegts_mux_instance_t *mmi = mm->mm_active;
  if (mmi) {
    if (mmi->mmi_input->mi_has_subscription(mmi->mmi_input, mm)) {
      tvhtrace(LS_MPEGTS, "%s - keeping mux", name);
      return 1;
    }
  }
  return 0;
}

static void
mpegts_mux_stop ( mpegts_mux_t *mm, int force, int reason )
{
  char *s;
  mpegts_mux_instance_t *mmi = mm->mm_active, *mmi2;
  mpegts_input_t *mi = NULL, *mi2;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  if (!force && mpegts_mux_has_subscribers(mm, mm->mm_nicename))
    return;

  /* Stop possible recursion */
  if (!mmi) return;

  tvhdebug(LS_MPEGTS, "%s - stopping mux%s", mm->mm_nicename, force ? " (forced)" : "");

  mi = mmi->mmi_input;
  assert(mi);

  /* Linked input */
  if (mi->mi_linked) {
    mi2 = mpegts_input_find(mi->mi_linked);
    if (mi2 && (mmi2 = LIST_FIRST(&mi2->mi_mux_active)) != NULL) {
      if (mmi2 && !mpegts_mux_has_subscribers(mmi2->mmi_mux, mmi2->mmi_mux->mm_nicename)) {
        s = mi2->mi_linked;
        mi2->mi_linked = NULL;
        mpegts_mux_unsubscribe_linked(mi2, NULL);
        mi2->mi_linked = s;
      } else {
        if (!force) {
          tvhtrace(LS_MPEGTS, "%s - keeping subscribed (linked tuner active)", mm->mm_nicename);
          return;
        }
      }
    }
  }

  if (mm->mm_active != mmi)
    return;

  mi->mi_stopping_mux(mi, mmi);
  mtimer_disarm(&mm->mm_update_pids_timer); /* Stop PID timer */
  mi->mi_stop_mux(mi, mmi);
  mi->mi_stopped_mux(mi, mmi);

  assert(mm->mm_active == NULL);

  /* Flush all tables */
  tvhtrace(LS_MPEGTS, "%s - flush tables", mm->mm_nicename);
  mpegts_table_flush_all(mm);

  tvhtrace(LS_MPEGTS, "%s - mi=%p", mm->mm_nicename, (void *)mi);
  /* Flush table data queue */
  mpegts_input_flush_mux(mi, mm);

  /* Ensure PIDs are cleared */
  tvh_mutex_lock(&mi->mi_output_lock);
  mm->mm_last_pid = -1;
  mm->mm_last_mp = NULL;
  while ((mp = RB_FIRST(&mm->mm_pids))) {
    assert(mi);
    if (mp->mp_pid == MPEGTS_FULLMUX_PID ||
        mp->mp_pid == MPEGTS_TABLES_PID) {
      while ((mps = LIST_FIRST(&mm->mm_all_subs))) {
        tvhdebug(LS_MPEGTS, "%s - close PID %s subscription [%d/%p]",
                 mm->mm_nicename,
                 mp->mp_pid == MPEGTS_TABLES_PID ? "tables" : "fullmux",
                 mps->mps_type, mps->mps_owner);
        LIST_REMOVE(mps, mps_svcraw_link);
        free(mps);
      }
    } else {
      while ((mps = RB_FIRST(&mp->mp_subs))) {
        tvhdebug(LS_MPEGTS, "%s - close PID %04X (%d) [%d/%p]",
                 mm->mm_nicename,
                 mp->mp_pid, mp->mp_pid, mps->mps_type, mps->mps_owner);
        RB_REMOVE(&mp->mp_subs, mps, mps_link);
        if (mps->mps_type & (MPS_SERVICE|MPS_ALL))
          LIST_REMOVE(mps, mps_svcraw_link);
        if (mps->mps_type & MPS_RAW)
          LIST_REMOVE(mps, mps_raw_link);
        free(mps);
      }
    }
    RB_REMOVE(&mm->mm_pids, mp, mp_link);
    free(mp);
  }
  tvh_mutex_unlock(&mi->mi_output_lock);

  /* Scanning */
  mpegts_network_scan_mux_cancel(mm, 1);

  /* Events */
  mpegts_fire_event1(mm, ml_mux_stop, reason);

  free(mm->mm_fastscan_muxes);
  mm->mm_fastscan_muxes = NULL;
}

static void
mpegts_mux_update_pids_cb ( void *aux )
{
  mpegts_mux_t *mm = aux;
  mpegts_input_t *mi;

  if (mm && mm->mm_active) {
    mi = mm->mm_active->mmi_input;
    if (mi) {
      tvh_mutex_lock(&mi->mi_output_lock);
      mm->mm_update_pids_flag = 0;
      mi->mi_update_pids(mi, mm);
      tvh_mutex_unlock(&mi->mi_output_lock);
    }
  }
}

void
mpegts_mux_update_pids ( mpegts_mux_t *mm )
{
  if (mm && mm->mm_active)
    mtimer_arm_rel(&mm->mm_update_pids_timer, mpegts_mux_update_pids_cb, mm, 0);
}

void
mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt, int subscribe )
{
  mpegts_input_t *mi;

  lock_assert(&mm->mm_tables_lock);

  if (mt->mt_destroyed)
    return;
  if (!mm->mm_active || !mm->mm_active->mmi_input) {
    mt->mt_subscribed = 0;
    LIST_INSERT_HEAD(&mm->mm_tables, mt, mt_link);
    mm->mm_num_tables++;
    return;
  }
  if (mt->mt_flags & MT_DEFER) {
    if (mt->mt_defer_cmd == MT_DEFER_OPEN_PID)
      return;
    mpegts_table_grab(mt); /* thread will release the table */
    LIST_INSERT_HEAD(&mm->mm_tables, mt, mt_link);
    mm->mm_num_tables++;
    mt->mt_defer_cmd = MT_DEFER_OPEN_PID;
    TAILQ_INSERT_TAIL(&mm->mm_defer_tables, mt, mt_defer_link);
    return;
  }
  mi = mm->mm_active->mmi_input;
  LIST_INSERT_HEAD(&mm->mm_tables, mt, mt_link);
  mm->mm_num_tables++;
  if (subscribe && !mt->mt_subscribed) {
    mpegts_table_grab(mt);
    mt->mt_subscribed = 1;
    tvh_mutex_unlock(&mm->mm_tables_lock);
    tvh_mutex_lock(&mi->mi_output_lock);
    mpegts_input_open_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt, 0);
    tvh_mutex_unlock(&mi->mi_output_lock);
    tvh_mutex_lock(&mm->mm_tables_lock);
    mpegts_table_release(mt);
  }
}

void
mpegts_mux_unsubscribe_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  mpegts_input_t *mi;

  lock_assert(&mm->mm_tables_lock);

  mi = mm->mm_active->mmi_input;
  if (mt->mt_subscribed) {
    mpegts_table_grab(mt);
    mt->mt_subscribed = 0;
    tvh_mutex_unlock(&mm->mm_tables_lock);
    tvh_mutex_lock(&mi->mi_output_lock);
    mpegts_input_close_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt);
    tvh_mutex_unlock(&mi->mi_output_lock);
    tvh_mutex_lock(&mm->mm_tables_lock);
    mpegts_table_release(mt);
  }
  if ((mt->mt_flags & MT_DEFER) && mt->mt_defer_cmd == MT_DEFER_OPEN_PID) {
    TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
    mt->mt_defer_cmd = 0;
    mpegts_table_release(mt);
  }
}

void
mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  lock_assert(&mm->mm_tables_lock);

  if (!mm->mm_active || !mm->mm_active->mmi_input) {
    if (mt->mt_defer_cmd) {
      TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
      mt->mt_defer_cmd = 0;
      if (mpegts_table_release(mt))
        return;
    }
    mt->mt_subscribed = 0;
    LIST_REMOVE(mt, mt_link);
    mm->mm_num_tables--;
    return;
  }
  if (mt->mt_flags & MT_DEFER) {
    if (mt->mt_defer_cmd == MT_DEFER_CLOSE_PID)
      return;
    LIST_REMOVE(mt, mt_link);
    mm->mm_num_tables--;
    if (mt->mt_defer_cmd == MT_DEFER_OPEN_PID) {
      TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
      mt->mt_defer_cmd = 0;
      mpegts_table_release(mt);
      return;
    }
    mpegts_table_grab(mt); /* thread will free the table */
    mt->mt_defer_cmd = MT_DEFER_CLOSE_PID;
    TAILQ_INSERT_TAIL(&mm->mm_defer_tables, mt, mt_defer_link);
    return;
  }
  LIST_REMOVE(mt, mt_link);
  mm->mm_num_tables--;
  mm->mm_unsubscribe_table(mm, mt);
}

/* **************************************************************************
 * Scanning
 * *************************************************************************/

static void
mpegts_mux_scan_service_check ( mpegts_mux_t *mm )
{
  mpegts_service_t *s, *snext;
  time_t last_seen;

  /*
   * Disable "not seen" services. It's quite easy algorithm which
   * compares the time for the most recent services with others.
   * If there is a big gap (24 hours) the service will be removed.
   *
   * Note that this code is run only when the PAT table scan is
   * fully completed (all live services are known at this point).
   */
  last_seen = 0;
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    if (last_seen < s->s_dvb_check_seen)
      last_seen = s->s_dvb_check_seen;
  for (s = LIST_FIRST(&mm->mm_services); s; s = snext) {
    snext = LIST_NEXT(s, s_dvb_mux_link);
    if (s->s_enabled && s->s_auto != SERVICE_AUTO_OFF &&
        s->s_dvb_check_seen + 24 * 3600 < last_seen) {
      tvhinfo(LS_MPEGTS, "disabling service %s [sid %04X/%d] (missing in PAT/SDT)",
              s->s_nicename ?: "<unknown>",
              service_id16(s), service_id16(s));
      service_set_enabled((service_t *)s, 0, SERVICE_AUTO_PAT_MISSING);
    }
  }
}

void
mpegts_mux_scan_done ( mpegts_mux_t *mm, const char *buf, int res )
{
  mpegts_table_t *mt;
  int total = 0, incomplete = 0;

  assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);

  /* Log */
  tvh_mutex_lock(&mm->mm_tables_lock);
  mpegts_table_consistency_check(mm);
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (mt->mt_flags & MT_QUICKREQ) {
      const char *s = "not found";
      if (mt->mt_complete) {
        s = "complete";
        total++;
      } else if (mt->mt_count) {
        s = "incomplete";
        total++;
        incomplete++;
      }
      tvhdebug(LS_MPEGTS, "%s - %04X (%d) %s %s", buf, mt->mt_pid, mt->mt_pid, mt->mt_name, s);
    }
  }
  tvh_mutex_unlock(&mm->mm_tables_lock);

  /* override if all tables were found */
  if (res < 0 && incomplete <= 0 && total > 2)
    res = 1;

  if (res < 0) {
    /* is threshold 3 missing tables enough? */
    if (incomplete > 0 && total > incomplete && incomplete <= 3) {
      tvhinfo(LS_MPEGTS, "%s - scan complete (partial - %d/%d tables)", buf, incomplete, total);
      mpegts_network_scan_mux_partial(mm);
    } else {
      tvhwarn(LS_MPEGTS, "%s - scan timed out (%d/%d tables)", buf, incomplete, total);
      mpegts_network_scan_mux_fail(mm);
    }
  } else if (res) {
    tvhinfo(LS_MPEGTS, "%s scan complete", buf);
    mpegts_network_scan_mux_done(mm);
    mpegts_mux_scan_service_check(mm);
  } else {
    tvhinfo(LS_MPEGTS, "%s - scan no data, failed", buf);
    mpegts_network_scan_mux_fail(mm);
  }
}

static void
mpegts_mux_scan_timeout ( void *aux )
{
  int c, q, w;
  mpegts_mux_t *mm = aux;
  mpegts_table_t *mt;

  /* Timeout */
  if (mm->mm_scan_init) {
    mpegts_mux_scan_done(mm, mm->mm_nicename, -1);
    return;
  }
  mm->mm_scan_init = 1;
  
  /* Check tables */
again:
  tvh_mutex_lock(&mm->mm_tables_lock);
  mpegts_table_consistency_check(mm);
  c = q = w = 0;
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (!(mt->mt_flags & MT_QUICKREQ) && !mt->mt_working) continue;
    if (!mt->mt_count) {
      mpegts_table_grab(mt);
      tvh_mutex_unlock(&mm->mm_tables_lock);
      mpegts_table_destroy(mt);
      mpegts_table_release(mt);
      goto again;
    } else if (!mt->mt_complete || mt->mt_working) {
      q++;
      if (mt->mt_working)
        w++;
    } else {
      c++;
    }
  }
  tvh_mutex_unlock(&mm->mm_tables_lock);
      
  /* No DATA - give up now */
  if (!c) {
    mpegts_mux_scan_done(mm, mm->mm_nicename, 0);

  /* Pending tables (another 20s or 30s - bit arbitrary) */
  } else if (q) {
    tvhtrace(LS_MPEGTS, "%s - scan needs more time", mm->mm_nicename);
    mtimer_arm_rel(&mm->mm_scan_timeout, mpegts_mux_scan_timeout, mm, sec2mono(w ? 30 : 20));
    return;

  /* Complete */
  } else {
    mpegts_mux_scan_done(mm, mm->mm_nicename, 1);
  }
}

/* **************************************************************************
 * Creation / Config
 * *************************************************************************/

mpegts_mux_t *
mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *mn, uint32_t onid, uint32_t tsid, htsmsg_t *conf )
{
  if (idnode_insert(&mm->mm_id, uuid, class, 0)) {
    if (uuid)
      tvherror(LS_MPEGTS, "invalid mux uuid '%s'", uuid);
    free(mm);
    return NULL;
  }

  mm->mm_refcount            = 1;

  /* Enabled by default */
  mm->mm_enabled             = MM_ENABLE;
  mm->mm_epg                 = MM_EPG_ENABLE;

  /* Identification */
  mm->mm_onid                = onid;
  mm->mm_tsid                = tsid;

  /* Add to network */
  LIST_INSERT_HEAD(&mn->mn_muxes, mm, mm_network_link);
  mm->mm_network             = mn;

  /* Debug/Config */
  mm->mm_delete              = mpegts_mux_delete;
  mm->mm_free                = mpegts_mux_free;
  mm->mm_display_name        = mpegts_mux_display_name;
  mm->mm_config_save         = mpegts_mux_config_save;
  mm->mm_is_enabled          = mpegts_mux_is_enabled;
  mm->mm_is_epg              = mpegts_mux_is_epg;

  /* Start/stop */
  mm->mm_stop                = mpegts_mux_stop;
  mm->mm_create_instances    = mpegts_mux_create_instances;

  /* Table processing */
  mm->mm_open_table          = mpegts_mux_open_table;
  mm->mm_unsubscribe_table   = mpegts_mux_unsubscribe_table;
  mm->mm_close_table         = mpegts_mux_close_table;
  tvh_mutex_init(&mm->mm_tables_lock, NULL);
  TAILQ_INIT(&mm->mm_table_queue);
  TAILQ_INIT(&mm->mm_defer_tables);
  LIST_INIT(&mm->mm_descrambler_caids);
  TAILQ_INIT(&mm->mm_descrambler_tables);
  TAILQ_INIT(&mm->mm_descrambler_emms);
  tvh_mutex_init(&mm->mm_descrambler_lock, NULL);

  mm->mm_last_pid            = -1;
  mm->mm_created             = gclk();

  /* Configuration */
  if (conf)
    idnode_load(&mm->mm_id, conf);

  return mm;
}

mpegts_mux_t *
mpegts_mux_post_create ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn;

  if (mm == NULL)
    return NULL;

  mn = mm->mm_network;
  if (mm->mm_enabled == MM_IGNORE)
    mm->mm_scan_result = MM_SCAN_IGNORE;

  mpegts_mux_update_nice_name(mm);
  tvhtrace(LS_MPEGTS, "%s - created", mm->mm_nicename);

  /* Initial scan */
  if (mm->mm_scan_result == MM_SCAN_NONE || !mn->mn_skipinitscan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_INIT,
                                  SUBSCRIPTION_INITSCAN, 10);
  else if (mm->mm_network->mn_idlescan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_IDLE,
                                  SUBSCRIPTION_IDLESCAN, 10);

  return mm;
}

void
mpegts_mux_save ( mpegts_mux_t *mm, htsmsg_t *c, int refs )
{
  mpegts_service_t *ms;
  mpegts_mux_instance_t *mmi;
  htsmsg_t *root = !refs ? htsmsg_create_map() : c;
  htsmsg_t *services = !refs ? htsmsg_create_map() : htsmsg_create_list();
  htsmsg_t *e;
  char ubuf[UUID_HEX_SIZE];

  idnode_save(&mm->mm_id, root);
  LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link) {
    mmi->mmi_tune_failed = 0;
  }
  LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link) {
    if (refs) {
      htsmsg_add_uuid(services, NULL, &ms->s_id.in_uuid);
    } else {
      e = htsmsg_create_map();
      service_save((service_t *)ms, e);
      htsmsg_add_msg(services, idnode_uuid_as_str(&ms->s_id, ubuf), e);
    }
  }
  htsmsg_add_msg(root, "services", services);
  if (!refs)
    htsmsg_add_msg(c, "config", root);
}

int
mpegts_mux_set_network_name ( mpegts_mux_t *mm, const char *name )
{
  if (strcmp(mm->mm_provider_network_name ?: "", name ?: "")) {
    tvh_str_update(&mm->mm_provider_network_name, name ?: "");
    return 1;
  }
  return 0;
}

int
mpegts_mux_set_onid ( mpegts_mux_t *mm, uint32_t onid )
{
  if (onid == mm->mm_onid)
    return 0;
  mm->mm_onid = onid;
  tvhtrace(LS_MPEGTS, "%s - set onid %04X (%d)", mm->mm_nicename, onid, onid);
  idnode_changed(&mm->mm_id);
  return 1;
}

int
mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint32_t tsid, int force )
{
  if (tsid == mm->mm_tsid)
    return 0;
  if (!force && mm->mm_tsid != MPEGTS_TSID_NONE)
    return 0;
  mm->mm_tsid = tsid;
  tvhtrace(LS_MPEGTS, "%s - set tsid %04X (%d)", mm->mm_nicename, tsid, tsid);
  idnode_changed(&mm->mm_id);
  mpegts_network_scan_mux_reactivate(mm);
  return 1;
}

int 
mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth )
{
  if (defauth && !strcmp(defauth, mm->mm_crid_authority ?: ""))
    return 0;
  tvh_str_update(&mm->mm_crid_authority, defauth);
  tvhtrace(LS_MPEGTS, "%s - set crid authority %s", mm->mm_nicename, defauth);
  idnode_changed(&mm->mm_id);
  return 1;
}

int
mpegts_mux_set_epg_module ( mpegts_mux_t *mm, const char *modid )
{
  switch (mm->mm_epg) {
  case MM_EPG_ENABLE:
  case MM_EPG_DETECTED:
    break;
  default:
    return 0;
  }
  if (modid && !strcmp(modid, mm->mm_epg_module_id ?: ""))
    return 0;
  mm->mm_epg = MM_EPG_DETECTED;
  tvh_str_update(&mm->mm_epg_module_id, modid);
  tvhtrace(LS_MPEGTS, "%s - set EPG module id %s", mm->mm_nicename, modid);
  idnode_changed(&mm->mm_id);
  return 1;
}

void
mpegts_mux_nice_name( mpegts_mux_t *mm, char *buf, size_t len )
{
  size_t len2;

  if (len == 0 || buf == NULL || mm == NULL) {
    if (buf && len > 0)
      *buf = '\0';
    return;
  }
  if (mm->mm_display_name)
    mm->mm_display_name(mm, buf, len);
  else
    *buf = '\0';
  len2 = strlen(buf);
  buf += len2;
  len -= len2;
  if (len2 + 16 >= len)
    return;
  strcpy(buf, " in ");
  buf += 4;
  len -= 4;
  if (mm->mm_network && mm->mm_network->mn_display_name)
    mm->mm_network->mn_display_name(mm->mm_network, buf, len);
}

void
mpegts_mux_update_nice_name( mpegts_mux_t *mm )
{
  char buf[256];

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  free(mm->mm_nicename);
  mm->mm_nicename = strdup(buf);
}

/* **************************************************************************
 * Subscriptions
 * *************************************************************************/

void
mpegts_mux_remove_subscriber
  ( mpegts_mux_t *mm, th_subscription_t *s, int reason )
{
  tvhtrace(LS_MPEGTS, "%s - remove subscriber (reason %i)", mm->mm_nicename, reason);
  mm->mm_stop(mm, 0, reason);
}

int
mpegts_mux_subscribe
  ( mpegts_mux_t *mm, mpegts_input_t *mi,
    const char *name, int weight, int flags )
{
  profile_chain_t prch;
  th_subscription_t *s;
  int err = 0;
  memset(&prch, 0, sizeof(prch));
  prch.prch_id = mm;
  s = subscription_create_from_mux(&prch, (tvh_input_t *)mi,
                                   weight, name,
                                   SUBSCRIPTION_NONE | flags,
                                   NULL, NULL, NULL, &err);
  return s ? 0 : (err ? err : SM_CODE_UNDEFINED_ERROR);
}

void
mpegts_mux_unsubscribe_by_name
  ( mpegts_mux_t *mm, const char *name )
{
  const service_t *t;
  th_subscription_t *s, *n;

  s = LIST_FIRST(&mm->mm_raw_subs);
  while (s) {
    n = LIST_NEXT(s, ths_mux_link);
    t = s->ths_service;
    if (t && t->s_type == STYPE_RAW && !strcmp(s->ths_title, name))
      subscription_unsubscribe(s, UNSUBSCRIBE_FINAL);
    s = n;
  }
}

th_subscription_t *
mpegts_mux_find_subscription_by_name
  ( mpegts_mux_t *mm, const char *name )
{
  const service_t *t;
  th_subscription_t *s;

  LIST_FOREACH(s, &mm->mm_raw_subs, ths_mux_link) {
    t = s->ths_service;
    if (t && t->s_type == STYPE_RAW && !strcmp(s->ths_title, name))
      return s;
  }
  return NULL;
}

void
mpegts_mux_tuning_error ( const char *mux_uuid, mpegts_mux_instance_t *mmi_match )
{
  mpegts_mux_t *mm;
  mpegts_mux_instance_t *mmi;

  if (!tvh_mutex_timedlock(&global_lock, 2000000)) {
    mm = mpegts_mux_find(mux_uuid);
    if (mm) {
      if ((mmi = mm->mm_active) != NULL && mmi == mmi_match)
        if (mmi->mmi_input)
          mmi->mmi_input->mi_error(mmi->mmi_input, mm, TSS_TUNING);
    }
    tvh_mutex_unlock(&global_lock);
  }
}

/* **************************************************************************
 * Search
 * *************************************************************************/

mpegts_service_t *
mpegts_mux_find_service ( mpegts_mux_t *mm, uint16_t sid )
{
  mpegts_service_t *ms;
  LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
    if (service_id16(ms) == sid && ms->s_enabled)
      break;
  return ms;
}

static int mp_cmp ( mpegts_pid_t *a, mpegts_pid_t *b )
{
  return a->mp_pid - b->mp_pid;
}

mpegts_pid_t *
mpegts_mux_find_pid_ ( mpegts_mux_t *mm, int pid, int create )
{
  mpegts_pid_t skel, *mp;

  if (pid < 0 || pid > MPEGTS_TABLES_PID) return NULL;

  skel.mp_pid = pid;
  mp = RB_FIND(&mm->mm_pids, &skel, mp_link, mp_cmp);
  if (mp == NULL) {
    if (create) {
      mp = calloc(1, sizeof(*mp));
      mp->mp_pid = pid;
      if (!RB_INSERT_SORTED(&mm->mm_pids, mp, mp_link, mp_cmp)) {
        mp->mp_cc = -1;
      } else {
        free(mp);
        mp = NULL;
      }
    }
  }
  if (mp) {
    mm->mm_last_pid = pid;
    mm->mm_last_mp = mp;
  }
  return mp;
}

/* **************************************************************************
 * Misc
 * *************************************************************************/

int
mpegts_mux_compare ( mpegts_mux_t *a, mpegts_mux_t *b )
{
  int r = uuid_cmp(&a->mm_network->mn_id.in_uuid,
                   &b->mm_network->mn_id.in_uuid);
  if (r)
    return r;
#if ENABLE_MPEGTS_DVB
  if (idnode_is_instance(&a->mm_id, &dvb_mux_dvbs_class) &&
      idnode_is_instance(&b->mm_id, &dvb_mux_dvbs_class)) {
    dvb_mux_conf_t *mc1 = &((dvb_mux_t *)a)->lm_tuning;
    dvb_mux_conf_t *mc2 = &((dvb_mux_t *)b)->lm_tuning;
    assert(mc1->dmc_fe_type == DVB_TYPE_S);
    assert(mc2->dmc_fe_type == DVB_TYPE_S);
    r = (int)mc1->u.dmc_fe_qpsk.polarisation -
        (int)mc2->u.dmc_fe_qpsk.polarisation;
    if (r == 0)
      r = mc1->dmc_fe_freq - mc2->dmc_fe_freq;
  }
#endif
  return r ?: uuid_cmp(&a->mm_id.in_uuid, &b->mm_id.in_uuid);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
