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

#include <assert.h>

static void
mpegts_mux_initial_scan_timeout ( void *aux );

/* ****************************************************************************
 * Mux instance (input linkage)
 * ***************************************************************************/

const idclass_t mpegts_mux_instance_class =
{
  .ic_class      = "mpegts_mux_instance",
  .ic_caption    = "MPEGTS Multiplex Phy",
  .ic_properties = (const property_t[]){
  }
};

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

  LIST_INSERT_HEAD(&mm->mm_instances, mmi, mmi_mux_link);

  return mmi;
}

int
mpegts_mux_instance_start ( mpegts_mux_instance_t **mmiptr )
{
  int r;
  char buf[256], buf2[256];;
  mpegts_mux_instance_t *mmi = *mmiptr;
  mpegts_mux_t           *mm = mmi->mmi_mux;
  mpegts_network_t       *mn = mm->mm_network;
  mm->mm_display_name(mm, buf, sizeof(buf));

  /* Already active */
  if (mm->mm_active) {
    *mmiptr = mm->mm_active;
    tvhdebug("mpegts", "%s - already active", buf);
    return 0;
  }

  /* Start */
  mmi->mmi_input->mi_display_name(mmi->mmi_input, buf2, sizeof(buf2));
  tvhinfo("mpegts", "%s - tuning on %s", buf, buf2);
  r = mmi->mmi_input->mi_start_mux(mmi->mmi_input, mmi);
  if (!r) return r;

  /* Start */
  tvhdebug("mpegts", "%s - started", buf);
  LIST_INSERT_HEAD(&mmi->mmi_input->mi_mux_active, mmi,
                    mmi_active_link);
  mm->mm_active = mmi;

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

  return 0;
}

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

static void
mpegts_mux_class_save ( idnode_t *self )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)self;
  mm->mm_config_save(mm);
}

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = "MPEGTS Multiplex",
  .ic_save       = mpegts_mux_class_save,
  .ic_properties = (const property_t[]){
    {  PROPDEF1("enabled", "Enabled",
                PT_BOOL, mpegts_mux_t, mm_enabled) },
    {  PROPDEF1("onid", "Original Network ID",
                PT_U16, mpegts_mux_t, mm_onid) },
    {  PROPDEF1("tsid", "Transport Stream ID",
                PT_U16, mpegts_mux_t, mm_tsid) },
    {  PROPDEF2("crid_authority", "CRID Authority",
                PT_STR, mpegts_mux_t, mm_crid_authority, 1) },
    {  PROPDEF2("init_scan_done", "Initial Scan Complete",
                PT_BOOL, mpegts_mux_t, mm_initial_scan_done, 1) },
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

static int
mpegts_mux_start ( mpegts_mux_t *mm, const char *reason, int weight )
{
  int pass, fail;
  char buf[256];
  mpegts_mux_instance_t *mmi, *tune;

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - starting for '%s' (weight %d)",
          buf, reason, weight);

  /* Disabled */
  if (!mm->mm_is_enabled(mm)) {
    tvhwarn("mpegts", "%s - not enabled", buf);
    return SM_CODE_MUX_NOT_ENABLED;
  }

  /* Already tuned */
  if (mm->mm_active) {
    tvhtrace("mpegts", "%s - already active", buf);
    return 0;
  }
  
  /* Create mux instances (where needed) */
  mm->mm_create_instances(mm);
  if (!LIST_FIRST(&mm->mm_instances)) {
    tvhtrace("mpegts", "%s - has no instances", buf);
    return SM_CODE_NO_FREE_ADAPTER;
  }

  /* Find */
  fail = 0;
  pass = 0;
  mmi  = NULL;
  while (pass < 2) {
    if (!mmi) mmi = LIST_FIRST(&mm->mm_instances);
  
    /* First pass - free only */
    if (!pass) {

      if (!mmi->mmi_tune_failed &&
          mmi->mmi_input->mi_is_enabled(mmi->mmi_input) &&
          mmi->mmi_input->mi_is_free(mmi->mmi_input)) {
        tune = mmi;
        tvhtrace("mpegts", "%s - found free mmi %p", buf, mmi);
      }
    
    /* Second pass - non-free */
    } else {

      /* Enabled, valid and lower weight */
      if (mmi->mmi_input->mi_is_enabled(mmi->mmi_input) &&
          !mmi->mmi_tune_failed &&
          (weight > mmi->mmi_input->mi_current_weight(mmi->mmi_input))) {
        tune = mmi;
        tvhtrace("mpegts", "%s - found mmi %p to boot", buf, mmi);
      }
    }
    
    /* Tune */
    if (tune) {
      if (!(fail = mpegts_mux_instance_start(&tune))) break;
      tune = NULL;
      tvhwarn("mpegts", "%s - failed to start, try another", buf);
    }

    /* Next */
    mmi = LIST_NEXT(mmi, mmi_mux_link);
    if (!mmi) pass++;
  }
  
  if (!tune) {
    tvhdebug("mpegts", "%s - no free input (fail=%d)", buf, fail);
    return SM_CODE_NO_FREE_ADAPTER;
  }

  return 0;
}

static void
mpegts_mux_stop ( mpegts_mux_t *mm )
{
  char buf[256];
  service_t *s, *t;
  mpegts_mux_instance_t *mmi = mm->mm_active;
  mpegts_input_t *mi;

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - stopping mux", buf);

  if (mmi) {
    mi = mmi->mmi_input;
    mi->mi_stop_mux(mi, mmi);
    mm->mm_active = NULL;
    LIST_REMOVE(mmi, mmi_active_link);

    /* Flush all subscribers */
    tvhtrace("mpegts", "%s - flush subscribers", buf);
    s = LIST_FIRST(&mi->mi_transports);
    while (s) {
      t = s;
      s = LIST_NEXT(t, s_active_link);
      if (((mpegts_service_t*)s)->s_dvb_mux != mm)
        continue;
      service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
    }
  }

  /* Flush all tables */
  tvhtrace("mpegts", "%s - flush tables", buf);
  mpegts_table_flush_all(mm);

  /* Alert listeners */
  // TODO

  /* Scanning */
  if (mm->mm_initial_scan_status == MM_SCAN_CURRENT) {
    tvhtrace("mpegts", "%s - add to pending scan Q", buf);
    mpegts_network_t *mn = mm->mm_network;
    TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
    mm->mm_initial_scan_status = MM_SCAN_PENDING;
    TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
    mpegts_network_schedule_initial_scan(mn);
  }

  /* Clear */
  mm->mm_active = NULL;
}

static void
mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  char buf[256];
  if (mt->mt_pid >= 0x2000)
    return;
  mm->mm_display_name(mm, buf, sizeof(buf));
  if (!mm->mm_table_filter[mt->mt_pid])
    tvhtrace("mpegts", "%s - opened table %s pid %04X (%d)",
             buf, mt->mt_name, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 1;
}

static void
mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  char buf[256];
  if (mt->mt_pid >= 0x2000)
    return;
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - closed table %s pid %04X (%d)",
           buf, mt->mt_name, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 0;
}

/* **************************************************************************
 * Scanning
 * *************************************************************************/

static int
mpegts_mux_has_subscribers ( mpegts_mux_t *mm )
{
  service_t *t;
  mpegts_mux_instance_t *mmi = mm->mm_active;
  if (mmi) {
    LIST_FOREACH(t, &mmi->mmi_input->mi_transports, s_active_link)
      if (((mpegts_service_t*)t)->s_dvb_mux == mm)
        return 1;
  }
  return 0;
}

static void
mpegts_mux_initial_scan_link ( mpegts_mux_t *mm )
{
  char buf1[256], buf2[256];
  mpegts_network_t *mn = mm->mm_network;

  assert(mn != NULL);
  assert(mm->mm_initial_scan_status == MM_SCAN_DONE);

  mm->mm_initial_scan_status = MM_SCAN_PENDING;
  TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm,
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
  tvhdebug("mpegts", "%s - initial scan timed", buf);
  mpegts_mux_initial_scan_done(mm);
}

void
mpegts_mux_initial_scan_done ( mpegts_mux_t *mm )
{
  char buf[256];
  mpegts_network_t *mn = mm->mm_network;
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhinfo("mpegts", "%s - initial scan complete", buf);
  gtimer_disarm(&mm->mm_initial_scan_timeout);
  assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
  mn->mn_initial_scan_num--;
  mm->mm_initial_scan_status = MM_SCAN_DONE;
  TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
  mpegts_network_schedule_initial_scan(mn);

  /* Stop */
  if (!mpegts_mux_has_subscribers(mm)) {
    tvhtrace("mpegts", "%s - no active subscribers, stop", buf);
    mm->mm_stop(mm);
  }

  /* Save */
  mm->mm_initial_scan_done = 1;
  mm->mm_config_save(mm);
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
  return 1;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
