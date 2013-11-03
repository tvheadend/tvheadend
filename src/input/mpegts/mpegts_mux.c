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
#include "input/mpegts.h"
#include "subscriptions.h"
#include "dvb_charset.h"

#include <assert.h>

static void
mpegts_mux_initial_scan_timeout ( void *aux );
static void
mpegts_mux_initial_scan_link ( mpegts_mux_t *mm );

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
  idnode_insert(&mmi->mmi_id, uuid, class);
  // TODO: does this need to be an idnode?

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
mpegts_mux_add_to_current ( mpegts_mux_t *mm, const char *buf )
{
  mpegts_network_t       *mn = mm->mm_network;
  /* Initial scanning */
  if (mm->mm_initial_scan_status == MM_SCAN_PENDING) {
    tvhtrace("mpegts", "%s - adding to current scan Q", buf);
    TAILQ_REMOVE(&mn->mn_initial_scan_pending_queue, mm,
                 mm_initial_scan_link);
    mm->mm_initial_scan_status = MM_SCAN_CURRENT;
    TAILQ_INSERT_TAIL(&mn->mn_initial_scan_current_queue, mm,
                      mm_initial_scan_link);
    gtimer_arm(&mm->mm_initial_scan_timeout,
               mpegts_mux_initial_scan_timeout, mm, 30);
  }
}

int
mpegts_mux_instance_start
  ( mpegts_mux_instance_t **mmiptr )
{
  int r;
  char buf[256], buf2[256];;
  mpegts_mux_instance_t *mmi = *mmiptr;
  mpegts_mux_t           *mm = mmi->mmi_mux;
  mm->mm_display_name(mm, buf, sizeof(buf));

  /* Already active */
  if (mm->mm_active) {
    *mmiptr = mm->mm_active;
    tvhdebug("mpegts", "%s - already active", buf);
    mpegts_mux_add_to_current(mm, buf);
    return 0;
  }

  /* Start */
  mmi->mmi_input->mi_display_name(mmi->mmi_input, buf2, sizeof(buf2));
  tvhinfo("mpegts", "%s - tuning on %s", buf, buf2);
  r = mmi->mmi_input->mi_start_mux(mmi->mmi_input, mmi);
  if (r) return r;

  /* Start */
  tvhdebug("mpegts", "%s - started", buf);
  mmi->mmi_input->mi_started_mux(mmi->mmi_input, mmi);

  /* Event handler */
  mpegts_fire_event(mm, ml_mux_start);

  /* Link */
  mpegts_mux_add_to_current(mm, buf);

  return 0;
}

int
mpegts_mux_instance_weight ( mpegts_mux_instance_t *mmi )
{
  int w = 0;
  const service_t *s;
  const th_subscription_t *ths;
  mpegts_input_t *mi = mmi->mmi_input;
  lock_assert(&mi->mi_delivery_mutex);

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
  if (mm->mm_delete) mm->mm_delete(mm);
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

static void
mpegts_mux_class_initscan_notify ( void *p )
{
  mpegts_mux_t *mm = p;
  mpegts_network_t *mn = mm->mm_network;

  /* Start */
  if (!mm->mm_initial_scan_done) {
    if (mm->mm_initial_scan_status == MM_SCAN_DONE)
      mpegts_mux_initial_scan_link(mm);

  /* Stop */
  } else {
    if (mm->mm_initial_scan_status == MM_SCAN_CURRENT)
      mpegts_mux_initial_scan_done(mm);
    else if (mm->mm_initial_scan_status == MM_SCAN_PENDING)
      TAILQ_REMOVE(&mn->mn_initial_scan_pending_queue, mm,
                   mm_initial_scan_link);
  }
}

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = "MPEGTS Multiplex",
  .ic_event      = "mpegts_mux",
  .ic_save       = mpegts_mux_class_save,
  .ic_delete     = mpegts_mux_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(mpegts_mux_t, mm_enabled),
      .def.i    = 1,
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
      .type     = PT_BOOL,
      .id       = "initscan",
      .name     = "Initial Scan Complete",
      .off      = offsetof(mpegts_mux_t, mm_initial_scan_done),
      .notify   = mpegts_mux_class_initscan_notify,
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
mpegts_mux_delete ( mpegts_mux_t *mm )
{
  mpegts_mux_instance_t *mmi;
  mpegts_network_t *mn = mm->mm_network;
  mpegts_service_t *s;
  char buf[256];

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - deleting", buf);
  
  /* Stop */
  mm->mm_stop(mm, 1);

  /* Remove from lists */
  LIST_REMOVE(mm, mm_network_link);
  if (mm->mm_initial_scan_status != MM_SCAN_DONE) {
    TAILQ_REMOVE(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
  }
  while ((mmi = LIST_FIRST(&mm->mm_instances))) {
     mmi->mmi_delete(mmi);
  }

  /* Delete services */
  while ((s = LIST_FIRST(&mm->mm_services))) {
    service_destroy((service_t*)s);
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

static void
mpegts_mux_create_instances ( mpegts_mux_t *mm )
{
}

// TODO: this does not follow the same logic for ordering things
//       as the service_enlist() routine that's used for service
//       arbitration

static int
mpegts_mux_start
  ( mpegts_mux_t *mm, const char *reason, int weight )
{
  int pass, havefree = 0, enabled = 0;
  char buf[256];
  mpegts_mux_instance_t *mmi, *tune;

  mm->mm_display_name(mm, buf, sizeof(buf));
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
    mpegts_mux_add_to_current(mm, buf);
    return 0;
  }
  
  /* Create mux instances (where needed) */
  mm->mm_create_instances(mm);
  if (!LIST_FIRST(&mm->mm_instances)) {
    tvhtrace("mpegts", "%s - has no instances, tuners enabled?", buf);
    return SM_CODE_NO_VALID_ADAPTER;
  }

  /* Find */
  pass = 0;
  mmi  = NULL;
  while (pass < 2) {
    tune = NULL;
    if (!mmi) mmi = LIST_FIRST(&mm->mm_instances);
    tvhtrace("mpegts", "%s - checking mmi %p", buf, mmi);

    /* First pass - free only */
    if (!pass) {
      int e = mmi->mmi_input->mi_is_enabled(mmi->mmi_input);
      int f = mmi->mmi_input->mi_is_free(mmi->mmi_input);
      tvhtrace("mpegts", "%s -   enabled %d free %d", buf, e, f);
      if (e) enabled = 1;

      if (e && f) {
        havefree = 1;
        if (!mmi->mmi_tune_failed) {
          tune = mmi;
          tvhtrace("mpegts", "%s - found free mmi %p", buf, mmi);
        }
      }
    
    /* Second pass - non-free */
    } else {

      /* Enabled, valid and lower weight */
      if (mmi->mmi_input->mi_is_enabled(mmi->mmi_input) &&
          !mmi->mmi_tune_failed &&
          (weight > mmi->mmi_input->mi_get_weight(mmi->mmi_input))) {
        tune = mmi;
        tvhtrace("mpegts", "%s - found mmi %p to boot", buf, mmi);
      }
    }
    
    /* Tune */
    if (tune) {
      tvhinfo("mpegts", "%s - starting for '%s' (weight %d)",
              buf, reason, weight);
      if (!mpegts_mux_instance_start(&tune)) break;
      tune = NULL;
      tvhwarn("mpegts", "%s - failed to start, try another", buf);
    }

    /* Next */
    mmi = LIST_NEXT(mmi, mmi_mux_link);
    if (!mmi) pass++;
  }
  
  if (!tune) {
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
  
  return 0;
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

  mm->mm_display_name(mm, buf, sizeof(buf));
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

  /* Flush table data queue */
  if (mi)
    mpegts_input_flush_mux(mi, mm);

  /* Ensure PIDs are cleared */
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
  if (mm->mm_initial_scan_status == MM_SCAN_CURRENT) {
    tvhtrace("mpegts", "%s - add to pending scan Q", buf);
    mpegts_network_t *mn = mm->mm_network;
    TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
    mm->mm_initial_scan_status = MM_SCAN_PENDING;
    gtimer_disarm(&mm->mm_initial_scan_timeout);
    if (mm->mm_initial_scan_done)
      TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
    else
      TAILQ_INSERT_HEAD(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
    mpegts_network_schedule_initial_scan(mn);
  }

  mpegts_fire_event(mm, ml_mux_stop);

  /* Clear */
  mm->mm_active = NULL;
}

void
mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  int type = MPS_TABLE;
  if (mt->mt_flags & MT_RECORD) type |= MPS_STREAM;
  mpegts_input_t *mi;
  if (!mm->mm_active || !mm->mm_active->mmi_input) return;
  mi = mm->mm_active->mmi_input;
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  mi->mi_open_pid(mi, mm, mt->mt_pid, type, mt);
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

void
mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  mpegts_input_t *mi;
  int type = MPS_TABLE;
  if (mt->mt_flags & MT_RECORD) type |= MPS_STREAM;
  if (!mm->mm_active || !mm->mm_active->mmi_input) return;
  mi = mm->mm_active->mmi_input;
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  mi->mi_close_pid(mi, mm, mt->mt_pid, type, mt);
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

/* **************************************************************************
 * Scanning
 * *************************************************************************/

static void
mpegts_mux_initial_scan_link ( mpegts_mux_t *mm )
{
  char buf1[256], buf2[256];
  mpegts_network_t *mn = mm->mm_network;

  assert(mn != NULL);
  assert(mm->mm_initial_scan_status == MM_SCAN_DONE);

  mm->mm_initial_scan_status = MM_SCAN_PENDING;
  if (mm->mm_initial_scan_done)
    TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm,
                      mm_initial_scan_link);
  else
    TAILQ_INSERT_HEAD(&mn->mn_initial_scan_pending_queue, mm,
                      mm_initial_scan_link);
  mn->mn_initial_scan_num++;
  
  mn->mn_display_name(mn, buf1, sizeof(buf1));
  mm->mm_display_name(mm, buf2, sizeof(buf2));
  tvhdebug("mpegts", "%s - added %s to scan pending q",
           buf1, buf2);
  mpegts_network_schedule_initial_scan(mn);
}

static void
mpegts_mux_initial_scan_timeout ( void *aux )
{
  char buf[256];
  mpegts_mux_t *mm = aux;
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - initial scan timed out", buf);
  mpegts_mux_initial_scan_done(mm);
}

void
mpegts_mux_initial_scan_done ( mpegts_mux_t *mm )
{
  char buf[256];
  mpegts_network_t *mn = mm->mm_network;

  /* Stop */
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - initial scan complete", buf);
  gtimer_disarm(&mm->mm_initial_scan_timeout);
  assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
  mn->mn_initial_scan_num--;
  mm->mm_initial_scan_status = MM_SCAN_DONE;
  TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
  mpegts_network_schedule_initial_scan(mn);

  /* Unsubscribe */
  mpegts_mux_unsubscribe_by_name(mm, "initscan");

  /* Save */
  mm->mm_initial_scan_done = 1;
  mm->mm_config_save(mm);
  idnode_updated(&mm->mm_id);
  idnode_updated(&mm->mm_network->mn_id);
}

void
mpegts_mux_initial_scan_fail ( mpegts_mux_t *mm )
{
  char buf[256];
  mpegts_network_t *mn = mm->mm_network;

  /* Stop */
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - initial scan failed (remove mux)", buf);
  gtimer_disarm(&mm->mm_initial_scan_timeout);
  mn->mn_initial_scan_num--;
  mm->mm_initial_scan_status = MM_SCAN_DONE;

  /* Save */
  mm->mm_initial_scan_done = 1;
  mm->mm_config_save(mm);
  idnode_updated(&mm->mm_id);
  idnode_updated(&mm->mm_network->mn_id);
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

  idnode_insert(&mm->mm_id, uuid, class);

  /* Enabled by default */
  mm->mm_enabled             = 1;

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

  /* Start/stop */
  mm->mm_start               = mpegts_mux_start;
  mm->mm_stop                = mpegts_mux_stop;
  mm->mm_create_instances    = mpegts_mux_create_instances;

  /* Table processing */
  mm->mm_open_table          = mpegts_mux_open_table;
  mm->mm_close_table         = mpegts_mux_close_table;
  TAILQ_INIT(&mm->mm_table_queue);

  /* Configuration */
  if (conf)
    idnode_load(&mm->mm_id, conf);

  /* Initial scan */
  if (!mm->mm_initial_scan_done || !mn->mn_skipinitscan)
    mpegts_mux_initial_scan_link(mm);

  mm->mm_display_name(mm, buf, sizeof(buf));
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
  mm->mm_display_name(mm, buf, sizeof(buf));
  mm->mm_config_save(mm);
  tvhtrace("mpegts", "%s - set onid %04X (%d)", buf, onid, onid);
  //idnode_notify(NULL, &mm->mm_id, 0, NULL);
  return 1;
}

int
mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid )
{
  char buf[256];
  if (tsid == mm->mm_tsid)
    return 0;
  mm->mm_tsid = tsid;
  mm->mm_display_name(mm, buf, sizeof(buf));
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
  mm->mm_display_name(mm, buf, sizeof(buf));
  mm->mm_config_save(mm);
  tvhtrace("mpegts", "%s - set crid authority %s", buf, defauth);
  //idnode_notify(NULL, &mm->mm_id, 0, NULL);
  return 1;
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
mpegts_mux_find_pid ( mpegts_mux_t *mm, int pid, int create )
{
  mpegts_pid_t *mp;
  
  if (pid > 0x2000) return NULL;

  if (!create) {
    mpegts_pid_t skel;
    skel.mp_pid = pid;
    mp = RB_FIND(&mm->mm_pids, &skel, mp_link, mp_cmp);
  } else {
    static mpegts_pid_t *skel = NULL;
    if (!skel)
      skel = calloc(1, sizeof(mpegts_pid_t));
    skel->mp_pid = pid;
    mp = RB_INSERT_SORTED(&mm->mm_pids, skel, mp_link, mp_cmp);
    if (!mp) {
      mp        = skel;
      skel      = NULL;
      mp->mp_fd = -1;
    }
  }
  return mp;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
