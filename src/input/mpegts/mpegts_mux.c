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

#include <assert.h>

static void mpegts_mux_scan_timeout ( void *p );
/* ****************************************************************************
 * Mux instance (input linkage)
 * ***************************************************************************/

const idclass_t mpegts_mux_instance_class =
{
  .ic_super      = &tvh_input_instance_class,
  .ic_class      = "mpegts_mux_instance",
  .ic_caption    = N_("MPEG-TS Multiplex Phy"),
  .ic_perm_def   = ACCESS_ADMIN
};

void
mpegts_mux_instance_delete
  ( tvh_input_instance_t *tii )
{
  mpegts_mux_instance_t *mmi = (mpegts_mux_instance_t *)tii;

  idnode_unlink(&tii->tii_id);
  LIST_REMOVE(mmi, mmi_mux_link);
  LIST_REMOVE(tii, tii_input_link);
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
  if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    mpegts_network_scan_mux_active(mm);

    /* Get timeout */
    t = mpegts_input_grace(mi, mm);
  
    /* Setup timeout */
    gtimer_arm(&mm->mm_scan_timeout, mpegts_mux_scan_timeout, mm, t);
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
  tvhtrace("mpegts", "subscribe keep for '%s' (%p)", mi->mi_name, mm);
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

  tvhtrace("mpegts", "subscribe linked");

  if (!mpegts_mux_keep_exists(mi) && (r = mpegts_mux_subscribe_keep(mm, mi))) {
    serr = "active1";
    goto fatal;
  }

  if (!mi2)
    return;

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
  tvherror("mpegts", "%s - %s - linked input cannot be started (%s: %i)", buf1, buf2, serr, r);
}

void
mpegts_mux_unsubscribe_linked
  ( mpegts_input_t *mi, service_t *t )
{
  th_subscription_t *ths, *ths_next;

  if (mi) {
    tvhtrace("mpegts", "unsubscribing linked");

    for (ths = LIST_FIRST(&subscriptions); ths; ths = ths_next) {
      ths_next = LIST_NEXT(ths, ths_global_link);
      if (ths->ths_source == (tvh_input_t *)mi && !strcmp(ths->ths_title, "keep") &&
          ths->ths_service != t)
        subscription_unsubscribe(ths, 0);
    }
  }
}

int
mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr, service_t *t )
{
  int r;
  char buf[256], buf2[256];
  mpegts_mux_instance_t *mmi = *mmiptr;
  mpegts_mux_t          * mm = mmi->mmi_mux;
  mpegts_input_t        * mi = mmi->mmi_input;
  mpegts_service_t      *  s;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  /* Already active */
  if (mm->mm_active) {
    *mmiptr = mm->mm_active;
    tvhdebug("mpegts", "%s - already active", buf);
    mpegts_mux_scan_active(mm, buf, (*mmiptr)->mmi_input);
    return 0;
  }

  /* Dead service check */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    s->s_dvb_check_seen = s->s_dvb_last_seen;

  /* Start */
  mi->mi_display_name(mi, buf2, sizeof(buf2));
  tvhinfo("mpegts", "%s - tuning on %s", buf, buf2);

  if (mi->mi_linked)
    mpegts_mux_unsubscribe_linked(mi, t);

  r = mi->mi_warm_mux(mi, mmi);
  if (r) return r;
  r = mi->mi_start_mux(mi, mmi);
  if (r) return r;

  /* Start */
  tvhdebug("mpegts", "%s - started", buf);
  mi->mi_started_mux(mi, mmi);

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

static void
mpegts_mux_class_save ( idnode_t *self )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)self;
  if (mm->mm_config_save) mm->mm_config_save(mm);
}

static void
mpegts_mux_class_delete ( idnode_t *self )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)self;
  if (mm->mm_delete) mm->mm_delete(mm, 1);
}

static const char *
mpegts_mux_class_get_title ( idnode_t *self, const char *lang )
{
  static __thread char buf[256];
  mpegts_mux_nice_name((mpegts_mux_t*)self, buf, sizeof(buf));
  return buf;
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
  static char buf[UUID_HEX_SIZE], *s = buf;
  mpegts_mux_t *mm = ptr;
  if (mm && mm->mm_network)
    strcpy(buf, idnode_uuid_as_sstr(&mm->mm_network->mn_id) ?: "");
  else
    *buf = 0;
  return &s;
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
  { N_("IDLE"),   MM_SCAN_STATE_IDLE },
  { N_("PEND"),   MM_SCAN_STATE_PEND },
  { N_("ACTIVE"), MM_SCAN_STATE_ACTIVE },
};

static struct strtab
scan_result_tab[] = {
 { N_("NONE"),         MM_SCAN_NONE },
 { N_("OK"),           MM_SCAN_OK   },
 { N_("FAIL"),         MM_SCAN_FAIL },
 { N_("OK (partial)"), MM_SCAN_PARTIAL },
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
  if (state == MM_SCAN_STATE_PEND || state == MM_SCAN_STATE_ACTIVE) {

    /* No change */
    if (mm->mm_scan_state != MM_SCAN_STATE_IDLE)
      return 0;

    /* Start */
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
  }
}

static htsmsg_t *
mpegts_mux_epg_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Disable"),                  MM_EPG_DISABLE },
    { N_("Enable (auto)"),            MM_EPG_ENABLE },
    { N_("Force (auto)"),             MM_EPG_FORCE },
    { N_("Only EIT"),                 MM_EPG_ONLY_EIT },
    { N_("Only UK Freesat"),          MM_EPG_ONLY_UK_FREESAT },
    { N_("Only UK Freeview"),         MM_EPG_ONLY_UK_FREEVIEW },
    { N_("Only Viasat Baltic"),       MM_EPG_ONLY_VIASAT_BALTIC },
    { N_("Only Bulsatcom 39E"),       MM_EPG_ONLY_BULSATCOM_39E },
    { N_("Only OpenTV Sky UK"),       MM_EPG_ONLY_OPENTV_SKY_UK },
    { N_("Only OpenTV Sky Italia"),   MM_EPG_ONLY_OPENTV_SKY_ITALIA },
    { N_("Only OpenTV Sky Ausat"),    MM_EPG_ONLY_OPENTV_SKY_AUSAT },
  };
  return strtab2htsmsg(tab, 1, lang);
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

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = N_("MPEG-TS Multiplex"),
  .ic_event      = "mpegts_mux",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = mpegts_mux_class_save,
  .ic_delete     = mpegts_mux_class_delete,
  .ic_get_title  = mpegts_mux_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(mpegts_mux_t, mm_enabled),
      .def.i    = 1,
      .notify   = mpegts_mux_class_enabled_notify,
    },
    {
      .type     = PT_INT,
      .id       = "epg",
      .name     = N_("EPG Scan"),
      .off      = offsetof(mpegts_mux_t, mm_epg),
      .def.i    = MM_EPG_ENABLE,
      .list     = mpegts_mux_epg_list,
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = N_("Network"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "network_uuid",
      .name     = N_("Network UUID"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_HIDDEN,
      .get      = mpegts_mux_class_get_network_uuid,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_name,
    },
    {
      .type     = PT_U16,
      .id       = "onid",
      .name     = N_("Original Network ID"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_onid),
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = N_("Transport Stream ID"),
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_tsid),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = N_("CRID Authority"),
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_mux_t, mm_crid_authority),
    },
    {
      .type     = PT_INT,
      .id       = "scan_state",
      .name     = N_("Scan Status"),
      .off      = offsetof(mpegts_mux_t, mm_scan_state),
      .set      = mpegts_mux_class_scan_state_set,
      .list     = mpegts_mux_class_scan_state_enum,
      .opts     = PO_NOSAVE | PO_SORTKEY,
    },
    {
      .type     = PT_INT,
      .id       = "scan_result",
      .name     = N_("Scan Result"),
      .off      = offsetof(mpegts_mux_t, mm_scan_result),
      .opts     = PO_RDONLY | PO_SORTKEY,
      .list     = mpegts_mux_class_scan_result_enum,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character Set"),
      .off      = offsetof(mpegts_mux_t, mm_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = N_("# Services"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_num_svc,
    },
    {
      .type     = PT_INT,
      .id       = "num_chn",
      .name     = N_("# Channels"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_num_chn,
    },
    {
      .type     = PT_INT,
      .id       = "pmt_06_ac3",
      .name     = N_("AC-3 Detection"),
      .off      = offsetof(mpegts_mux_t, mm_pmt_ac3),
      .def.i    = MM_AC3_STANDARD,
      .list     = mpegts_mux_ac3_list,
      .opts     = PO_HIDDEN | PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "eit_tsid_nocheck",
      .name     = N_("EIT - skip TSID check"),
      .off      = offsetof(mpegts_mux_t, mm_eit_tsid_nocheck),
      .opts     = PO_HIDDEN | PO_ADVANCED
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

void
mpegts_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  mpegts_mux_instance_t *mmi;
  mpegts_service_t *s;
  th_subscription_t *ths;
  char buf[256];

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s (%p) - deleting", buf, mm);
  
  /* Stop */
  mm->mm_stop(mm, 1, SM_CODE_ABORTED);

  /* Remove from network */
  LIST_REMOVE(mm, mm_network_link);

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

  /* Stop PID timer */
  gtimer_disarm(&mm->mm_update_pids_timer);

  /* Free memory */
  idnode_unlink(&mm->mm_id);
  free(mm->mm_crid_authority);
  free(mm->mm_charset);
  free(mm);
}

static void
mpegts_mux_config_save ( mpegts_mux_t *mm )
{
}

static int
mpegts_mux_is_enabled ( mpegts_mux_t *mm )
{
  return mm->mm_enabled;
}

static int
mpegts_mux_is_epg ( mpegts_mux_t *mm )
{
  return mm->mm_epg;
}

static void
mpegts_mux_create_instances ( mpegts_mux_t *mm )
{
}

static int
mpegts_mux_has_subscribers ( mpegts_mux_t *mm, const char *name )
{
  mpegts_mux_instance_t *mmi = mm->mm_active;
  if (mmi) {
    if (mmi->mmi_input->mi_has_subscription(mmi->mmi_input, mm)) {
      tvhtrace("mpegts", "%s - keeping mux", name);
      return 1;
    }
  }
  return 0;
}

static void
mpegts_mux_stop ( mpegts_mux_t *mm, int force, int reason )
{
  char buf[256], buf2[256], *s;
  mpegts_mux_instance_t *mmi = mm->mm_active, *mmi2;
  mpegts_input_t *mi = NULL, *mi2;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  if (!force && mpegts_mux_has_subscribers(mm, buf))
    return;

  /* Stop possible recursion */
  if (!mmi) return;

  tvhdebug("mpegts", "%s - stopping mux%s", buf, force ? " (forced)" : "");

  mi = mmi->mmi_input;
  assert(mi);

  /* Linked input */
  if (mi->mi_linked) {
    mi2 = mpegts_input_find(mi->mi_linked);
    if (mi2 && (mmi2 = LIST_FIRST(&mi2->mi_mux_active)) != NULL) {
      mpegts_mux_nice_name(mmi2->mmi_mux, buf2, sizeof(buf2));
      if (mmi2 && !mpegts_mux_has_subscribers(mmi2->mmi_mux, buf2)) {
        s = mi2->mi_linked;
        mi2->mi_linked = NULL;
        mpegts_mux_unsubscribe_linked(mi2, NULL);
        mi2->mi_linked = s;
      } else {
        if (!force) {
          tvhtrace("mpegts", "%s - keeping subscribed (linked tuner active)", buf);
          return;
        }
      }
    }
  }

  if (mm->mm_active != mmi)
    return;

  mi->mi_stopping_mux(mi, mmi);
  mi->mi_stop_mux(mi, mmi);
  mi->mi_stopped_mux(mi, mmi);

  assert(mm->mm_active == NULL);

  /* Flush all tables */
  tvhtrace("mpegts", "%s - flush tables", buf);
  mpegts_table_flush_all(mm);

  tvhtrace("mpegts", "%s - mi=%p", buf, (void *)mi);
  /* Flush table data queue */
  mpegts_input_flush_mux(mi, mm);

  /* Ensure PIDs are cleared */
  pthread_mutex_lock(&mi->mi_output_lock);
  mm->mm_last_pid = -1;
  mm->mm_last_mp = NULL;
  while ((mp = RB_FIRST(&mm->mm_pids))) {
    assert(mi);
    if (mp->mp_pid == MPEGTS_FULLMUX_PID ||
        mp->mp_pid == MPEGTS_TABLES_PID) {
      while ((mps = LIST_FIRST(&mm->mm_all_subs))) {
        tvhdebug("mpegts", "%s - close PID %s subscription [%d/%p]",
                 buf, mp->mp_pid == MPEGTS_TABLES_PID ? "tables" : "fullmux",
                 mps->mps_type, mps->mps_owner);
        LIST_REMOVE(mps, mps_svcraw_link);
        free(mps);
      }
    } else {
      while ((mps = RB_FIRST(&mp->mp_subs))) {
        tvhdebug("mpegts", "%s - close PID %04X (%d) [%d/%p]", buf,
                 mp->mp_pid, mp->mp_pid, mps->mps_type, mps->mps_owner);
        RB_REMOVE(&mp->mp_subs, mps, mps_link);
        if (mps->mps_type & (MPS_SERVICE|MPS_RAW|MPS_ALL))
          LIST_REMOVE(mps, mps_svcraw_link);
        free(mps);
      }
    }
    RB_REMOVE(&mm->mm_pids, mp, mp_link);
    free(mp);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);

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
      pthread_mutex_lock(&mi->mi_output_lock);
      mm->mm_update_pids_flag = 0;
      mi->mi_update_pids(mi, mm);
      pthread_mutex_unlock(&mi->mi_output_lock);
    }
  }
}

void
mpegts_mux_update_pids ( mpegts_mux_t *mm )
{
  if (mm && mm->mm_active)
    gtimer_arm(&mm->mm_update_pids_timer, mpegts_mux_update_pids_cb, mm, 0);
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
    pthread_mutex_unlock(&mm->mm_tables_lock);
    pthread_mutex_lock(&mi->mi_output_lock);
    mpegts_input_open_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt);
    pthread_mutex_unlock(&mi->mi_output_lock);
    pthread_mutex_lock(&mm->mm_tables_lock);
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
    pthread_mutex_unlock(&mm->mm_tables_lock);
    pthread_mutex_lock(&mi->mi_output_lock);
    mpegts_input_close_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt);
    pthread_mutex_unlock(&mi->mi_output_lock);
    pthread_mutex_lock(&mm->mm_tables_lock);
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
      tvhinfo("mpegts", "disabling service %s [sid %04X/%d] (missing in PAT/SDT)",
              s->s_nicename ?: "<unknown>", s->s_dvb_service_id, s->s_dvb_service_id);
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
  pthread_mutex_lock(&mm->mm_tables_lock);
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
      tvhdebug("mpegts", "%s - %04X (%d) %s %s", buf, mt->mt_pid, mt->mt_pid, mt->mt_name, s);
    }
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);

  if (res < 0) {
    /* is threshold 3 missing tables enough? */
    if (incomplete > 0 && total > incomplete && incomplete <= 3) {
      tvhinfo("mpegts", "%s - scan complete (partial - %d/%d tables)", buf, total, incomplete);
      mpegts_network_scan_mux_partial(mm);
    } else {
      tvhinfo("mpegts", "%s - scan timed out (%d/%d tables)", buf, total, incomplete);
      mpegts_network_scan_mux_fail(mm);
    }
  } else if (res) {
    tvhinfo("mpegts", "%s scan complete", buf);
    mpegts_network_scan_mux_done(mm);
    mpegts_mux_scan_service_check(mm);
  } else {
    tvhinfo("mpegts", "%s - scan no data, failed", buf);
    mpegts_network_scan_mux_fail(mm);
  }
}

static void
mpegts_mux_scan_timeout ( void *aux )
{
  int c, q, w;
  char buf[256];
  mpegts_mux_t *mm = aux;
  mpegts_table_t *mt;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  /* Timeout */
  if (mm->mm_scan_init) {
    mpegts_mux_scan_done(mm, buf, -1);
    return;
  }
  mm->mm_scan_init = 1;
  
  /* Check tables */
again:
  pthread_mutex_lock(&mm->mm_tables_lock);
  mpegts_table_consistency_check(mm);
  c = q = w = 0;
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (!(mt->mt_flags & MT_QUICKREQ) && !mt->mt_working) continue;
    if (!mt->mt_count) {
      mpegts_table_grab(mt);
      pthread_mutex_unlock(&mm->mm_tables_lock);
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
  pthread_mutex_unlock(&mm->mm_tables_lock);
      
  /* No DATA - give up now */
  if (!c) {
    mpegts_mux_scan_done(mm, buf, 0);

  /* Pending tables (another 20s or 30s - bit arbitrary) */
  } else if (q) {
    tvhtrace("mpegts", "%s - scan needs more time", buf);
    gtimer_arm(&mm->mm_scan_timeout, mpegts_mux_scan_timeout, mm, w ? 30 : 20);
    return;

  /* Complete */
  } else {
    mpegts_mux_scan_done(mm, buf, 1);
  }
}

/* **************************************************************************
 * Creation / Config
 * *************************************************************************/

mpegts_mux_t *
mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *mn, uint16_t onid, uint16_t tsid, htsmsg_t *conf )
{
  char buf[256];

  if (idnode_insert(&mm->mm_id, uuid, class, 0)) {
    if (uuid)
      tvherror("mpegts", "invalid mux uuid '%s'", uuid);
    free(mm);
    return NULL;
  }

  /* Enabled by default */
  mm->mm_enabled             = 1;
  mm->mm_epg                 = 1;

  /* Identification */
  mm->mm_onid                = onid;
  mm->mm_tsid                = tsid;

  /* Add to network */
  LIST_INSERT_HEAD(&mn->mn_muxes, mm, mm_network_link);
  mm->mm_network             = mn;

  /* Debug/Config */
  mm->mm_delete              = mpegts_mux_delete;
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
  pthread_mutex_init(&mm->mm_tables_lock, NULL);
  TAILQ_INIT(&mm->mm_table_queue);
  TAILQ_INIT(&mm->mm_defer_tables);
  LIST_INIT(&mm->mm_descrambler_caids);
  TAILQ_INIT(&mm->mm_descrambler_tables);
  TAILQ_INIT(&mm->mm_descrambler_emms);
  pthread_mutex_init(&mm->mm_descrambler_lock, NULL);

  mm->mm_last_pid            = -1;

  /* Configuration */
  if (conf)
    idnode_load(&mm->mm_id, conf);

  /* Initial scan */
  if (mm->mm_scan_result == MM_SCAN_NONE || !mn->mn_skipinitscan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_INIT,
                                  SUBSCRIPTION_INITSCAN, 10);
  else if (mm->mm_network->mn_idlescan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_IDLE,
                                  SUBSCRIPTION_IDLESCAN, 10);

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - created", buf);

  return mm;
}

void
mpegts_mux_save ( mpegts_mux_t *mm, htsmsg_t *c )
{
  idnode_save(&mm->mm_id, c);
}

int
mpegts_mux_set_onid ( mpegts_mux_t *mm, uint16_t onid )
{
  char buf[256];
  if (onid == mm->mm_onid)
    return 0;
  mm->mm_onid = onid;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  mm->mm_config_save(mm);
  idnode_notify_changed(&mm->mm_id);
  return 1;
}

int
mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid, int force )
{
  if (tsid == mm->mm_tsid)
    return 0;
  if (!force && mm->mm_tsid)
    return 0;
  mm->mm_tsid = tsid;
  mm->mm_config_save(mm);
  if (tvhtrace_enabled()) {
    char buf[256];
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    tvhtrace("mpegts", "%s - set tsid %04X (%d)", buf, tsid, tsid);
  }
  idnode_notify_changed(&mm->mm_id);
  return 1;
}

int 
mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth )
{
  if (defauth && !strcmp(defauth, mm->mm_crid_authority ?: ""))
    return 0;
  tvh_str_update(&mm->mm_crid_authority, defauth);
  mm->mm_config_save(mm);
  if (tvhtrace_enabled()) {
    char buf[256];
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    tvhtrace("mpegts", "%s - set crid authority %s", buf, defauth);
  }
  idnode_notify_changed(&mm->mm_id);
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

/* **************************************************************************
 * Subscriptions
 * *************************************************************************/

void
mpegts_mux_remove_subscriber
  ( mpegts_mux_t *mm, th_subscription_t *s, int reason )
{
  if (tvhtrace_enabled()) {
    char buf[256];
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    tvhtrace("mpegts", "%s - remove subscriber (reason %i)", buf, reason);
  }
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
      subscription_unsubscribe(s, 0);
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
  struct timespec timeout;

  timeout.tv_sec = 2;
  timeout.tv_nsec = 0;

  if (!pthread_mutex_timedlock(&global_lock, &timeout)) {
    mm = mpegts_mux_find(mux_uuid);
    if (mm) {
      if ((mmi = mm->mm_active) != NULL && mmi == mmi_match)
        if (mmi->mmi_input)
          mmi->mmi_input->mi_tuning_error(mmi->mmi_input, mm);
    }
    pthread_mutex_unlock(&global_lock);
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
    if (ms->s_dvb_service_id == sid && ms->s_enabled)
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

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
