/*
 *  Tvheadend - Network Scanner
 *
 *  Copyright (C) 2014 Adam Sutton
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

#include "input.h"

/******************************************************************************
 * Timer
 *****************************************************************************/

/* Notify */
static void
mpegts_network_scan_notify ( mpegts_mux_t *mm )
{
  idnode_updated(&mm->mm_id);
  idnode_updated(&mm->mm_network->mn_id);
}

static int
mm_cmp ( mpegts_mux_t *a, mpegts_mux_t *b )
{
  return b->mm_scan_weight - a->mm_scan_weight;
}

void
mpegts_network_scan_timer_cb ( void *p )
{
  mpegts_network_t *mn = p;
  mpegts_mux_t *mm, *nxt = NULL;
  int r;

  /* Process Q */
  for (mm = TAILQ_FIRST(&mn->mn_scan_pend); mm != NULL; mm = nxt) {
    nxt = TAILQ_NEXT(mm, mm_scan_link);
    assert(mm->mm_scan_state == MM_SCAN_STATE_PEND);

    /* Attempt to tune */
    r = mpegts_mux_subscribe(mm, "scan", mm->mm_scan_weight);

    /* Started */
    if (!r) {
      assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);
      continue;
    }
    assert(mm->mm_scan_state == MM_SCAN_STATE_PEND);

    /* No free tuners - stop */
    if (r == SM_CODE_NO_FREE_ADAPTER)
      break;

    /* No valid tuners (subtly different, might be able to tuner a later
     * mux)
     */
    if (r == SM_CODE_NO_VALID_ADAPTER)
      continue;

    /* Failed */
    TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
    if (mm->mm_scan_result != MM_SCAN_FAIL) {
      mm->mm_scan_result = MM_SCAN_FAIL;
      mm->mm_config_save(mm);
    }
    mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
    mm->mm_scan_weight = 0;
    mpegts_network_scan_notify(mm);
  }

  /* Re-arm timer. Really this is just a safety measure as we'd normally
   * expect the timer to be forcefully triggered on finish of a mux scan
   */
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 120);
}

/******************************************************************************
 * Mux transition
 *****************************************************************************/

/* Finished */
static inline void
mpegts_network_scan_mux_done0
  ( mpegts_mux_t *mm, mpegts_mux_scan_result_t result, int weight )
{
  mpegts_mux_unsubscribe_by_name(mm, "scan");
  mpegts_network_scan_queue_del(mm);

  if (result != MM_SCAN_NONE && mm->mm_scan_result != result) {
    mm->mm_scan_result = result;
    mm->mm_config_save(mm);
  }

  /* Re-enable? */
  if (weight > 0)
    mpegts_network_scan_queue_add(mm, weight);
}

/* Failed - couldn't start */
void
mpegts_network_scan_mux_fail    ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_done0(mm, MM_SCAN_FAIL, 0);
}

/* Completed succesfully */
void
mpegts_network_scan_mux_done    ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_done0(mm, MM_SCAN_OK, 0);
}

/* Failed - no input */
void
mpegts_network_scan_mux_timeout ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_done0(mm, MM_SCAN_FAIL, 0);
}

/* Interrupted (re-add) */
void
mpegts_network_scan_mux_cancel  ( mpegts_mux_t *mm, int reinsert )
{
  if (mm->mm_scan_state != MM_SCAN_STATE_ACTIVE)
    return;

  mpegts_network_scan_mux_done0(mm, MM_SCAN_NONE,
                                reinsert ? mm->mm_scan_weight : 0);
}

/* Mux has been started */
void
mpegts_network_scan_mux_active ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  if (mm->mm_scan_state != MM_SCAN_STATE_PEND)
    return;
  mm->mm_scan_state = MM_SCAN_STATE_ACTIVE;
  mm->mm_scan_init  = 0;
  TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
  TAILQ_INSERT_TAIL(&mn->mn_scan_active, mm, mm_scan_link);
}

/******************************************************************************
 * Mux queue handling
 *****************************************************************************/

void
mpegts_network_scan_queue_del ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE) {
    TAILQ_REMOVE(&mn->mn_scan_active, mm, mm_scan_link);
  } else if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
  }
  mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
  mm->mm_scan_weight = 0;
  gtimer_disarm(&mm->mm_scan_timeout);
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);
  mpegts_network_scan_notify(mm);
}

void
mpegts_network_scan_queue_add ( mpegts_mux_t *mm, int weight )
{
  int reload = 0;
  char buf[256], buf2[256];;
  mpegts_network_t *mn = mm->mm_network;

  if (!mm->mm_is_enabled(mm)) return;

  if (weight <= 0) return;

  if (weight > mm->mm_scan_weight) {
    mm->mm_scan_weight = weight;
    reload             = 1;
  }

  /* Already active */
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE)
    return;

  /* Remove entry (or ignore) */
  if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    if (!reload)
      return;
    TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
  }

  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  mn->mn_display_name(mn, buf2, sizeof(buf2));
  tvhdebug("mpegts", "%s - adding mux %s to queue weight %d",
           buf2, buf, weight);

  /* Add new entry */
  mm->mm_scan_state = MM_SCAN_STATE_PEND;
  TAILQ_INSERT_SORTED_R(&mn->mn_scan_pend, mpegts_mux_queue,
                        mm, mm_scan_link, mm_cmp);
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);
  mpegts_network_scan_notify(mm);
}

/******************************************************************************
 * Subsystem setup / tear down
 *****************************************************************************/

void
mpegts_network_scan_init ( void )
{
}

void
mpegts_network_scan_done ( void )
{
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
