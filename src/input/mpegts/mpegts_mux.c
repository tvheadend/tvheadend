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
#include "dvb_charset.h"

#include <assert.h>

SKEL_DECLARE(mpegts_pid_skel, mpegts_pid_t);

static void mpegts_mux_scan_timeout ( void *p );

/* ****************************************************************************
 * Mux instance (input linkage)
 * ***************************************************************************/

const idclass_t mpegts_mux_instance_class =
{
  .ic_class      = "mpegts_mux_instance",
  .ic_caption    = "MPEGTS Multiplex Phy",
};

static void
mpegts_mux_instance_delete
  ( mpegts_mux_instance_t *mmi )
{
  idnode_unlink(&mmi->mmi_id);
  LIST_REMOVE(mmi, mmi_mux_link);
  LIST_REMOVE(mmi, mmi_input_link);
  free(mmi);
}

mpegts_mux_instance_t *
mpegts_mux_instance_create0
  ( mpegts_mux_instance_t *mmi, const idclass_t *class, const char *uuid,
    mpegts_input_t *mi, mpegts_mux_t *mm )
{
  // TODO: does this need to be an idnode?
  if (idnode_insert(&mmi->mmi_id, uuid, class, 0)) {
    free(mmi);
    return NULL;
  }

  /* Setup links */
  mmi->mmi_mux   = mm;
  mmi->mmi_input = mi;
  
  /* Callbacks */
  mmi->mmi_delete = mpegts_mux_instance_delete;

  LIST_INSERT_HEAD(&mm->mm_instances, mmi, mmi_mux_link);
  LIST_INSERT_HEAD(&mi->mi_mux_instances, mmi, mmi_input_link);


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

int
mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr )
{
  int r;
  char buf[256], buf2[256];
  mpegts_mux_instance_t *mmi = *mmiptr;
  mpegts_mux_t          * mm = mmi->mmi_mux;
  mpegts_input_t        * mi = mmi->mmi_input;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  /* Already active */
  if (mm->mm_active) {
    *mmiptr = mm->mm_active;
    tvhdebug("mpegts", "%s - already active", buf);
    mpegts_mux_scan_active(mm, buf, (*mmiptr)->mmi_input);
    return 0;
  }

  /* Start */
  mi->mi_display_name(mi, buf2, sizeof(buf2));
  tvhinfo("mpegts", "%s - tuning on %s", buf, buf2);
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

  return 0;
}

int
mpegts_mux_instance_weight ( mpegts_mux_instance_t *mmi )
{
  int w = 0;
  const service_t *s;
  const th_subscription_t *ths;
  mpegts_input_t *mi = mmi->mmi_input;
  lock_assert(&mi->mi_output_lock);

  /* Direct subs */
  LIST_FOREACH(ths, &mmi->mmi_subs, ths_mmi_link) {
    w = MAX(w, ths->ths_weight);
  }

  /* Service subs */
  LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
    mpegts_service_t *ms = (mpegts_service_t*)s;
    if (ms->s_dvb_mux == mmi->mmi_mux) {
      LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
        w = MAX(w, ths->ths_weight);
      }
    }
  }

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
mpegts_mux_class_get_title ( idnode_t *self )
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
  { "IDLE",   MM_SCAN_STATE_IDLE },
  { "PEND",   MM_SCAN_STATE_PEND },
  { "ACTIVE", MM_SCAN_STATE_ACTIVE },
};

static struct strtab
scan_result_tab[] = {
 { "NONE",    MM_SCAN_NONE },
 { "OK",      MM_SCAN_OK   },
 { "FAIL",    MM_SCAN_FAIL },
};

static int
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
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_USER);

  /* Stop */
  } else if (state == MM_SCAN_STATE_IDLE) {

    /* No change */
    if (state == MM_SCAN_STATE_IDLE)
      return 0;

    /* Update */
    mpegts_network_scan_mux_cancel(mm, 0);

  /* Invalid */
  } else {
  }

  return 1;
}

static htsmsg_t *
mpegts_mux_class_scan_state_enum ( void *p )
{
  return strtab2htsmsg(scan_state_tab);
}

static htsmsg_t *
mpegts_mux_class_scan_result_enum ( void *p )
{
  return strtab2htsmsg(scan_result_tab);
}

static void
mpegts_mux_class_enabled_notify ( void *p )
{
  mpegts_mux_t *mm = p;
  if (!mm->mm_is_enabled(mm)) {
    mm->mm_stop(mm, 1);
    mpegts_network_scan_mux_cancel(mm, 0);
  }
}

static htsmsg_t *
mpegts_mux_epg_list ( void *o )
{
  static const struct strtab tab[] = {
    { "Disable",                  MM_EPG_DISABLE },
    { "Enable (auto)",            MM_EPG_ENABLE },
    { "Force (auto)",             MM_EPG_FORCE },
    { "Only EIT",                 MM_EPG_ONLY_EIT },
    { "Only UK Freesat",          MM_EPG_ONLY_UK_FREESAT },
    { "Only UK Freeview",         MM_EPG_ONLY_UK_FREEVIEW },
    { "Only Viasat Baltic",       MM_EPG_ONLY_VIASAT_BALTIC },
    { "Only OpenTV Sky UK",       MM_EPG_ONLY_OPENTV_SKY_UK },
    { "Only OpenTV Sky Italia",   MM_EPG_ONLY_OPENTV_SKY_ITALIA },
    { "Only OpenTV Sky Ausat",    MM_EPG_ONLY_OPENTV_SKY_AUSAT },
  };
  return strtab2htsmsg(tab);
}

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = "MPEGTS Multiplex",
  .ic_event      = "mpegts_mux",
  .ic_save       = mpegts_mux_class_save,
  .ic_delete     = mpegts_mux_class_delete,
  .ic_get_title  = mpegts_mux_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(mpegts_mux_t, mm_enabled),
      .def.i    = 1,
      .notify   = mpegts_mux_class_enabled_notify,
    },
    {
      .type     = PT_INT,
      .id       = "epg",
      .name     = "EPG Scan",
      .off      = offsetof(mpegts_mux_t, mm_epg),
      .def.i    = MM_EPG_ENABLE,
      .list     = mpegts_mux_epg_list,
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_network,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Name",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_name,
    },
    {
      .type     = PT_U16,
      .id       = "onid",
      .name     = "Original Network ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_onid),
    },
    {
      .type     = PT_U16,
      .id       = "tsid",
      .name     = "Transport Stream ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_tsid),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = "CRID Authority",
      .opts     = PO_RDONLY | PO_HIDDEN,
      .off      = offsetof(mpegts_mux_t, mm_crid_authority),
    },
    {
      .type     = PT_INT,
      .id       = "scan_state",
      .name     = "Scan Status",
      .off      = offsetof(mpegts_mux_t, mm_scan_state),
      .set      = mpegts_mux_class_scan_state_set,
      .list     = mpegts_mux_class_scan_state_enum,
      .opts     = PO_NOSAVE | PO_SORTKEY,
    },
    {
      .type     = PT_INT,
      .id       = "scan_result",
      .name     = "Scan Result",
      .off      = offsetof(mpegts_mux_t, mm_scan_result),
      .opts     = PO_RDONLY | PO_SORTKEY,
      .list     = mpegts_mux_class_scan_result_enum,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = "Character Set",
      .off      = offsetof(mpegts_mux_t, mm_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = "# Services",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_num_svc,
    },
    {
      .type     = PT_BOOL,
      .id       = "pmt_06_ac3",
      .name     = "PMT Descriptor 0x06 = AC-3",
      .off      = offsetof(mpegts_mux_t, mm_pmt_06_ac3),
      .opts     = PO_ADVANCED,
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
  char buf[256];

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s (%p) - deleting", buf, mm);
  
  /* Stop */
  mm->mm_stop(mm, 1);

  /* Remove from network */
  LIST_REMOVE(mm, mm_network_link);

  /* Cancel scan */
  mpegts_network_scan_queue_del(mm);

  /* Remove instances */
  while ((mmi = LIST_FIRST(&mm->mm_instances))) {
    mmi->mmi_delete(mmi);
  }

  /* Delete services */
  while ((s = LIST_FIRST(&mm->mm_services))) {
    service_destroy((service_t*)s, delconf);
  }

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
mpegts_mux_start1( mpegts_mux_instance_t *mmi )
{
  char buf[256], buf2[256];

  if (mpegts_mux_instance_start(&mmi)) {
    mpegts_mux_nice_name(mmi->mmi_mux, buf, sizeof(buf));
    mmi->mmi_input->mi_display_name(mmi->mmi_input, buf2, sizeof(buf2));
    tvhwarn("mpegts", "%s - failed to start on %s, try another", buf, buf2);
    return 1;
  }
  return 0;
}

static int
mpegts_mux_start
  ( mpegts_mux_t *mm, const char *reason, int weight, int flags )
{
  int havefree = 0, enabled = 0, index, index2, weight2, count, size = 0;
  char buf[256];
  mpegts_mux_instance_t *mmi, **all;
  int64_t aweight, *allw;

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - starting for '%s' (weight %d)",
           buf, reason, weight);

  /* Disabled */
  if (!mm->mm_is_enabled(mm)) {
    tvhwarn("mpegts", "%s - not enabled", buf);
    return SM_CODE_MUX_NOT_ENABLED;
  }

  /* Already tuned */
  if (mm->mm_active) {
    tvhtrace("mpegts", "%s - already active", buf);
    mpegts_mux_scan_active(mm, buf, mm->mm_active->mmi_input);
    return 0;
  }

  /* Create mux instances (where needed) */
  mm->mm_create_instances(mm);
  if (!LIST_FIRST(&mm->mm_instances)) {
    tvhtrace("mpegts", "%s - has no instances, tuners enabled?", buf);
    return SM_CODE_NO_VALID_ADAPTER;
  }

  /* Get the count of instances */
  LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link)
    size++;
  all  = alloca(sizeof(mpegts_mux_instance_t *) * size);
  allw = alloca(sizeof(int64_t) * size);

  /* Calculate priority+weight and sort */
  count = 0;
  LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link) {
    int e = mmi->mmi_input->mi_is_enabled(mmi->mmi_input, mm, reason);
    tvhtrace("mpegts", "%s -   mmi %p enabled %d", buf, mmi, e);
    if (!e) continue;
    enabled = 1;

    /* Already live? Try it... */
    if (e && mmi->mmi_mux->mm_active == mmi) {
      if (!mpegts_mux_start1(mmi))
        return 0;
      continue;
    }

    if (mmi->mmi_tune_failed) continue;

    tvhtrace("mpegts", "%s - found mmi %p", buf, mmi);

    aweight = ((int64_t )mmi->mmi_input->mi_get_priority(mmi->mmi_input,
                                                         mmi->mmi_mux, flags) << 32) |
                         mmi->mmi_input->mi_get_weight(mmi->mmi_input, flags);
    for (index = 0; index < count; index++) {
      if (allw[index] >= aweight)
        break;
    }
    if (index < count) {
      int j;
      for (j = count; j > index; j--) {
        allw[j] = allw[j - 1];
        all [j] = all [j - 1];
      }
    }
    all [index] = mmi;
    allw[index] = aweight;
    count++;
  }

  /* Try free inputs */
  for (index = count - 1; index >= 0; index--) {
    mpegts_input_t *mi;
    mmi = all[index];
    mi = mmi->mmi_input;
    if (mi->mi_is_free(mi)) {
      all[index] = NULL;
      havefree = 1;
      tvhtrace("mpegts", "%s - found mmi %p to boot (free)", buf, mmi);
      if (!mpegts_mux_start1(mmi))
        return 0;
    }
  }

  /* Try the lowest weight */
  for (index = 0, index2 = -1, weight2 = weight; index < count; index++) {
    if (all[index] && weight2 > (allw[index] & 0xffffffff)) {
      weight2 = allw[index] & 0xffffffff;
      index2  = index;
    }
  }

  if (index2 >= 0) {
    mmi = all[index2];
    all[index2] = NULL;
    tvhtrace("mpegts", "%s - found mmi %p to boot (lowest)", buf, mmi);
    if (!mpegts_mux_start1(mmi))
      return 0;
  }

  /* Try the all lowest weights */
  for (index = 0; index < count; index++) {
    if (all[index] && weight > (allw[index] & 0xffffffff)) {
      mmi = all[index];
      all[index] = NULL;
      tvhtrace("mpegts", "%s - found mmi %p to boot", buf, mmi);
      if (!mpegts_mux_start1(mmi))
        return 0;
    }
  }

  LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link) {
    if (!mmi->mmi_tune_failed) {
      if (!enabled) {
        tvhdebug("mpegts", "%s - no tuners enabled", buf);
        return SM_CODE_NO_VALID_ADAPTER;
      } else if (havefree) {
        tvhdebug("mpegts", "%s - no valid tuner available", buf);
        return SM_CODE_NO_VALID_ADAPTER;
      } else {
        tvhdebug("mpegts", "%s - no free tuner available", buf);
        return SM_CODE_NO_FREE_ADAPTER;
      }
    }
  }
  tvhdebug("mpegts", "%s - tuning failed, invalid config?", buf);
  return SM_CODE_TUNING_FAILED;
}

static int
mpegts_mux_has_subscribers ( mpegts_mux_t *mm )
{
  mpegts_mux_instance_t *mmi = mm->mm_active;
  if (mmi) {
    if (LIST_FIRST(&mmi->mmi_subs))
      return 1; 
    return mmi->mmi_input->mi_has_subscription(mmi->mmi_input, mm);
  }
  return 0;
}

static void
mpegts_mux_stop ( mpegts_mux_t *mm, int force )
{
  char buf[256];
  mpegts_mux_instance_t *mmi = mm->mm_active;
  mpegts_input_t *mi = NULL;
  th_subscription_t *sub;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  if (!force && mpegts_mux_has_subscribers(mm))
    return;

  /* Stop possible recursion */
  if (!mmi) return;
  mm->mm_active = NULL;

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - stopping mux", buf);

  if (mmi) {
    LIST_FOREACH(sub, &mmi->mmi_subs, ths_mmi_link)
      subscription_unlink_mux(sub, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
    mi = mmi->mmi_input;
    mi->mi_stop_mux(mi, mmi);
    mi->mi_stopped_mux(mi, mmi);
  }

  /* Flush all tables */
  tvhtrace("mpegts", "%s - flush tables", buf);
  mpegts_table_flush_all(mm);

  tvhtrace("mpegts", "%s - mi=%p", buf, (void *)mi);
  /* Flush table data queue */
  if (mi)
    mpegts_input_flush_mux(mi, mm);

  /* Ensure PIDs are cleared */
  mm->mm_last_pid = -1;
  mm->mm_last_mp = NULL;
  while ((mp = RB_FIRST(&mm->mm_pids))) {
    while ((mps = RB_FIRST(&mp->mp_subs))) {
      RB_REMOVE(&mp->mp_subs, mps, mps_link);
      free(mps);
    }
    RB_REMOVE(&mm->mm_pids, mp, mp_link);
    if (mp->mp_fd != -1) {
      tvhdebug("mpegts", "%s - close PID %04X (%d)", buf, mp->mp_pid, mp->mp_pid);
      close(mp->mp_fd);
    }
    free(mp);
  }

  /* Scanning */
  mpegts_network_scan_mux_cancel(mm, 1);
  if (mm->mm_scan_state == MM_SCAN_STATE_IDLE &&
      mm->mm_network->mn_idlescan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_IDLE);

  /* Events */
  mpegts_fire_event(mm, ml_mux_stop);

  /* Clear */
  mm->mm_active = NULL;
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
  pthread_mutex_unlock(&mm->mm_tables_lock);
  pthread_mutex_lock(&mi->mi_output_lock);
  if (subscribe) {
    mi->mi_open_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt);
    mt->mt_subscribed = 1;
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  pthread_mutex_lock(&mm->mm_tables_lock);
}

void
mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  mpegts_input_t *mi;

  lock_assert(&mm->mm_tables_lock);

  if (!mm->mm_active || !mm->mm_active->mmi_input) {
    if (mt->mt_defer_cmd) {
      TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
      mt->mt_defer_cmd = 0;
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
  mi = mm->mm_active->mmi_input;
  LIST_REMOVE(mt, mt_link);
  mm->mm_num_tables--;
  pthread_mutex_unlock(&mm->mm_tables_lock);
  pthread_mutex_lock(&mi->mi_output_lock);
  if (mt->mt_subscribed) {
    mi->mi_close_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt);
    mt->mt_subscribed = 0;
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  pthread_mutex_lock(&mm->mm_tables_lock);
}

/* **************************************************************************
 * Scanning
 * *************************************************************************/

void
mpegts_mux_scan_done ( mpegts_mux_t *mm, const char *buf, int res )
{
  mpegts_table_t *mt;

  assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);

  /* Log */
  pthread_mutex_lock(&mm->mm_tables_lock);
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (mt->mt_flags & MT_QUICKREQ) {
      const char *s = "not found";
      if (mt->mt_complete)
        s = "complete";
      else if (mt->mt_count)
        s = "incomplete";
      tvhdebug("mpegts", "%s - %s %s", buf, mt->mt_name, s);
    }
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);

  if (res)
    mpegts_network_scan_mux_done(mm);
  else
    mpegts_network_scan_mux_fail(mm);
}

static void
mpegts_mux_scan_timeout ( void *aux )
{
  int c, q;
  char buf[256];
  mpegts_mux_t *mm = aux;
  mpegts_table_t *mt, *nxt;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  /* Timeout */
  if (mm->mm_scan_init) {
    tvhinfo("mpegts", "%s - scan timed out", buf);
    mpegts_mux_scan_done(mm, buf, 0);
    return;
  }
  mm->mm_scan_init = 1;
  
  /* Check tables */
again:
  pthread_mutex_lock(&mm->mm_tables_lock);
  c = q = 0;
  for (mt = LIST_FIRST(&mm->mm_tables); mt != NULL; mt = nxt) {
    nxt = LIST_NEXT(mt, mt_link);
    if (!(mt->mt_flags & MT_QUICKREQ)) continue;
    if (!mt->mt_count) {
      pthread_mutex_unlock(&mm->mm_tables_lock);
      mpegts_table_destroy(mt);
      goto again;
    } else if (!mt->mt_complete) {
      q++;
    } else {
      c++;
    }
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);
      
  /* No DATA - give up now */
  if (!c) {
    tvhinfo("mpegts", "%s - scan no data, failed", buf);
    mpegts_mux_scan_done(mm, buf, 0);

  /* Pending tables (another 20s - bit arbitrary) */
  } else if (q) {
    tvhinfo("mepgts", "%s - scan needs more time", buf);
    gtimer_arm(&mm->mm_scan_timeout, mpegts_mux_scan_timeout, mm, 20);
    return;

  /* Complete */
  } else {
    tvhinfo("mpegts", "%s - scan complete", buf);
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
  mm->mm_start               = mpegts_mux_start;
  mm->mm_stop                = mpegts_mux_stop;
  mm->mm_create_instances    = mpegts_mux_create_instances;

  /* Table processing */
  mm->mm_open_table          = mpegts_mux_open_table;
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
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_INIT);
  else if (mm->mm_network->mn_idlescan)
    mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_IDLE);

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
  tvhtrace("mpegts", "%s - set onid %04X (%d)", buf, onid, onid);
  //idnode_notify(NULL, &mm->mm_id, 0, NULL);
  return 1;
}

int
mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid, int force )
{
  char buf[256];
  if (tsid == mm->mm_tsid)
    return 0;
  if (!force && mm->mm_tsid)
    return 0;
  mm->mm_tsid = tsid;
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  mm->mm_config_save(mm);
  tvhtrace("mpegts", "%s - set tsid %04X (%d)", buf, tsid, tsid);
  //idnode_notify(NULL, &mm->mm_id, 0, NULL);
  return 1;
}

int 
mpegts_mux_set_crid_authority ( mpegts_mux_t *mm, const char *defauth )
{
  char buf[256];
  if (defauth && !strcmp(defauth, mm->mm_crid_authority ?: ""))
    return 0;
  tvh_str_update(&mm->mm_crid_authority, defauth);
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  mm->mm_config_save(mm);
  tvhtrace("mpegts", "%s - set crid authority %s", buf, defauth);
  //idnode_notify(NULL, &mm->mm_id, 0, NULL);
  return 1;
}

void
mpegts_mux_nice_name( mpegts_mux_t *mm, char *buf, size_t len )
{
  size_t len2;

  if (len == 0 || buf == NULL)
    return;
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
  if (mm && mm->mm_network && mm->mm_network->mn_display_name)
    mm->mm_network->mn_display_name(mm->mm_network, buf, len);
}

/* **************************************************************************
 * Subscriptions
 * *************************************************************************/

void
mpegts_mux_remove_subscriber
  ( mpegts_mux_t *mm, th_subscription_t *s, int reason )
{
  subscription_unlink_mux(s, reason);
  mm->mm_stop(mm, 0);
}

int
mpegts_mux_subscribe
  ( mpegts_mux_t *mm, const char *name, int weight )
{
  int err = 0;
  th_subscription_t *s;
  s = subscription_create_from_mux(mm, weight, name, NULL,
                                   SUBSCRIPTION_NONE,
                                   NULL, NULL, NULL, &err);
  return s ? 0 : err;
}

void
mpegts_mux_unsubscribe_by_name
  ( mpegts_mux_t *mm, const char *name )
{
  mpegts_mux_instance_t *mmi;
  th_subscription_t *s, *n;

  LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link) {
    s = LIST_FIRST(&mmi->mmi_subs);
    while (s) {
      n = LIST_NEXT(s, ths_mmi_link);
      if (!strcmp(s->ths_title, name))
        subscription_unsubscribe(s);
      s = n;
    }
  }
}

/* **************************************************************************
 * Search
 * *************************************************************************/

mpegts_service_t *
mpegts_mux_find_service ( mpegts_mux_t *mm, uint16_t sid)
{
  mpegts_service_t *ms;
  LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link)
    if (ms->s_dvb_service_id == sid)
      break;
  return ms;
}

static int mp_cmp ( mpegts_pid_t *a, mpegts_pid_t *b )
{
  return a->mp_pid - b->mp_pid;
};

mpegts_pid_t *
mpegts_mux_find_pid_ ( mpegts_mux_t *mm, int pid, int create )
{
  mpegts_pid_t *mp;
  
  if (pid > 0x2000) return NULL;

  if (!create) {
    mpegts_pid_t skel;
    skel.mp_pid = pid;
    mp = RB_FIND(&mm->mm_pids, &skel, mp_link, mp_cmp);
  } else {
    SKEL_ALLOC(mpegts_pid_skel);
    mpegts_pid_skel->mp_pid = pid;
    mp = RB_INSERT_SORTED(&mm->mm_pids, mpegts_pid_skel, mp_link, mp_cmp);
    if (!mp) {
      mp = mpegts_pid_skel;
      SKEL_USED(mpegts_pid_skel);
      mp->mp_fd = -1;
      mp->mp_cc = -1;
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
