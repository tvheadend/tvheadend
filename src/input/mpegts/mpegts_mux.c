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
  if (r) return r;

  /* Start */
  tvhdebug("mpegts", "%s - started", buf);
  mmi->mmi_input->mi_started_mux(mmi->mmi_input, mmi);

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

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = "MPEGTS Multiplex",
  .ic_save       = mpegts_mux_class_save,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(mpegts_mux_t, mm_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_mux_class_get_network,
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
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_crid_authority),
    },
    {
      .type     = PT_BOOL,
      .id       = "initscan",
      .name     = "Initial Scan Complete",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_mux_t, mm_initial_scan_done),
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
    tune = NULL;
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
      tvhinfo("mpegts", "%s - starting for '%s' (weight %d)",
              buf, reason, weight);
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
  mpegts_mux_instance_t *mmi = mm->mm_active;
  mpegts_input_t *mi = NULL;

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - stopping mux", buf);

  if (mmi) {
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

  /* Alert listeners */
  // TODO

  /* Scanning */
  if (mm->mm_initial_scan_status == MM_SCAN_CURRENT) {
    tvhtrace("mpegts", "%s - add to pending scan Q", buf);
    mpegts_network_t *mn = mm->mm_network;
    TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
    mm->mm_initial_scan_status = MM_SCAN_PENDING;
    gtimer_disarm(&mm->mm_initial_scan_timeout);
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
  mpegts_mux_instance_t *mmi = mm->mm_active;
  if (mmi)
    return mmi->mmi_input->mi_has_subscription(mmi->mmi_input, mm);
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
  static htsmsg_t *inc = NULL;

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

  /* Notification */
  idnode_notify("mpegts_mux", &mm->mm_id, 0, NULL);
  if (!inc) {
    inc = htsmsg_create_map();
    htsmsg_set_u32(inc, "num_mux", 1);
  }
  idnode_notify(NULL, &mn->mn_id, 0, inc);

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

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
