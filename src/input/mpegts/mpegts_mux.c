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

  /* Setup links */
  mmi->mmi_mux   = mm;
  mmi->mmi_input = mi; // TODO: is this required?

  LIST_INSERT_HEAD(&mm->mm_instances, mmi, mmi_mux_link);

  return mmi;
}

const idclass_t mpegts_mux_class =
{
  .ic_class      = "mpegts_mux",
  .ic_caption    = "MPEGTS Multiplex",
  .ic_properties = (const property_t[]){
    {  PROPDEF1("onid", "Original Network ID",
               PT_INT, mpegts_mux_t, mm_onid) },
    {  PROPDEF1("tsid", "Transport Stream ID",
               PT_INT, mpegts_mux_t, mm_tsid) },

  }
};

void mpegts_mux_set_onid ( mpegts_mux_t *mm, uint16_t onid, int force )
{
  if (onid == mm->mm_onid)
    return;
  if (!force && mm->mm_onid != MPEGTS_ONID_NONE)
    return;
  mm->mm_onid = onid;
}

void mpegts_mux_set_tsid ( mpegts_mux_t *mm, uint16_t tsid, int force )
{
  if (tsid == mm->mm_tsid)
    return;
  if (!force && mm->mm_tsid != MPEGTS_TSID_NONE)
    return;
  mm->mm_tsid = tsid;
}

static void
mpegts_mux_initial_scan_link ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;

  assert(mn != NULL);
  assert(mm->mm_initial_scan_status == MM_SCAN_DONE);

  mm->mm_initial_scan_status = MM_SCAN_PENDING;
  TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm,
                    mm_initial_scan_link);
  mn->mn_initial_scan_num++;
  tvhtrace("mpegts", "added mm %p to initial scan for mn %p pending %d",
           mm, mn, mn->mn_initial_scan_num);
  mpegts_network_schedule_initial_scan(mn);
}

static void
mpegts_mux_initial_scan_timeout ( void *aux )
{
  mpegts_mux_t *mm = aux;
  tvhlog(LOG_DEBUG, "mpegts", "initial scan timed out for mm %p", mm);
  mpegts_mux_initial_scan_done(mm);
}

void
mpegts_mux_initial_scan_done ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  gtimer_disarm(&mm->mm_initial_scan_timeout);
  assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
  mn->mn_initial_scan_num--;
  mm->mm_initial_scan_status = MM_SCAN_DONE;
  TAILQ_REMOVE(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
  mpegts_network_schedule_initial_scan(mn);
  // TODO: save
}

static int
mpegts_mux_start ( mpegts_mux_t *mm, const char *reason, int weight )
{
  mpegts_network_t      *mn = mm->mm_network;
  mpegts_mux_instance_t *mmi;

  tvhtrace("mpegts", "mm %p starting for '%s' (weight %d)",
           mm, reason, weight); 

  /* Already tuned */
  if (mm->mm_active) {
    tvhtrace("mpegts", "mm %p already active", mm);
    return 0;
  }

  /* Find */
  // TODO: don't like this is unbounded, if for some reason mi_start_mux()
  //       constantly fails this will lock
  while (1) {

    /* Find free input */
    LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link)
      if (!mmi->mmi_tune_failed &&
          mmi->mmi_input->mi_is_free(mmi->mmi_input))
        break;
    if (mmi)
      tvhtrace("mpegts", "found free mmi %p", mmi);

    /* Try and remove a lesser instance */
    if (!mmi) {
      LIST_FOREACH(mmi, &mm->mm_instances, mmi_mux_link) {

        /* Bad - skip */
        if (mmi->mmi_tune_failed)
          continue;

        /* Found */
        if (weight > mmi->mmi_input->mi_current_weight(mmi->mmi_input))
          break;
      }

      if (mmi)
        tvhtrace("mpegts", "found mmi %p to boot", mmi);

      /* No free input */
      if (!mmi)
        return SM_CODE_NO_FREE_ADAPTER;
    }
    
    /* Tune */
    if (!mmi->mmi_input->mi_start_mux(mmi->mmi_input, mmi))
      break;
  }

  /* Initial scanning */
  if (mm->mm_initial_scan_status == MM_SCAN_PENDING) {
    TAILQ_REMOVE(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
    mm->mm_initial_scan_status = MM_SCAN_CURRENT;
    TAILQ_INSERT_TAIL(&mn->mn_initial_scan_current_queue, mm, mm_initial_scan_link);
    gtimer_arm(&mm->mm_initial_scan_timeout, mpegts_mux_initial_scan_timeout, mm, 10);
  }

  return 0;
}

static void
mpegts_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  if (mt->mt_pid >= 0x2000)
    return;
  if (!mm->mm_table_filter[mt->mt_pid])
    tvhtrace("mpegts", "mm %p opened table pid %04X (%d)",
             mm, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 1;
}

static void
mpegts_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  if (mt->mt_pid >= 0x2000)
    return;
  tvhtrace("mpegts", "mm %p closed table pid %04X (%d)",
           mm, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 0;
}

void
mpegts_mux_load_one ( mpegts_mux_t *mm, htsmsg_t *c )
{
  uint32_t u32;
  
  /* ONID */
  if (!htsmsg_get_u32(c, "onid", &u32))
    mm->mm_onid = u32;

  /* TSID */
  if (!htsmsg_get_u32(c, "tsid", &u32))
    mm->mm_tsid = u32;
}

mpegts_mux_t *
mpegts_mux_create0
  ( mpegts_mux_t *mm, const idclass_t *class, const char *uuid,
    mpegts_network_t *net, uint16_t onid, uint16_t tsid )
{
  idnode_insert(&mm->mm_id, uuid, class);

  /* Identification */
  mm->mm_onid                = onid;
  mm->mm_tsid                = tsid;

  /* Add to network */
  mm->mm_network             = net;
  mm->mm_start               = mpegts_mux_start;
  mpegts_mux_initial_scan_link(mm);

  /* Table processing */
  mm->mm_open_table          = mpegts_mux_open_table;
  mm->mm_close_table         = mpegts_mux_close_table;
  TAILQ_INIT(&mm->mm_table_queue);

  return mm;
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
