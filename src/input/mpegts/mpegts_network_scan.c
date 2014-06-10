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
 * Data
 *****************************************************************************/

mpegts_mux_queue_t mpegts_network_scan_pend;    // Pending muxes
mpegts_mux_queue_t mpegts_network_scan_active;  // Active muxes
gtimer_t           mpegts_network_scan_timer;   // Timer for activity


/******************************************************************************
 * Timer
 *****************************************************************************/

static void mpegts_network_scan_timer_cb ( void *p );

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

static void
mpegts_network_scan_timer_arm ( int period )
{
  gtimer_arm(&mpegts_network_scan_timer,
             mpegts_network_scan_timer_cb,
             NULL,
             period);
}

static void
mpegts_network_scan_timer_cb ( void *p )
{
  mpegts_mux_t *mm, *nxt = NULL;
  int r;

  /* Process Q */
  for (mm = TAILQ_FIRST(&mpegts_network_scan_pend); mm != NULL; mm = nxt) {
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

    /* Either there are no free tuners, or no valid tuners
     *
     * Although these are subtly different, the reality is that in this
     * context we need to treat each the same. We simply skip over this
     * mux and see if anything else can be tuned as it may use other 
     * tuners
     */
    if (r == SM_CODE_NO_FREE_ADAPTER ||
        r == SM_CODE_NO_VALID_ADAPTER)
      continue;

    /* Failed */
    TAILQ_REMOVE(&mpegts_network_scan_pend, mm, mm_scan_link);
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
  mpegts_network_scan_timer_arm(120);
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
  if (mm->mm_network->mn_idlescan && !weight)
    weight = SUBSCRIPTION_PRIO_SCAN_IDLE;
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
  if (mm->mm_scan_state != MM_SCAN_STATE_PEND)
    return;
  mm->mm_scan_state = MM_SCAN_STATE_ACTIVE;
  mm->mm_scan_init  = 0;
  TAILQ_REMOVE(&mpegts_network_scan_pend, mm, mm_scan_link);
  TAILQ_INSERT_TAIL(&mpegts_network_scan_active, mm, mm_scan_link);
}

/******************************************************************************
 * Mux queue handling
 *****************************************************************************/

void
mpegts_network_scan_queue_del ( mpegts_mux_t *mm )
{
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE) {
    TAILQ_REMOVE(&mpegts_network_scan_active, mm, mm_scan_link);
  } else if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    TAILQ_REMOVE(&mpegts_network_scan_pend, mm, mm_scan_link);
  }
  mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
  mm->mm_scan_weight = 0;
  gtimer_disarm(&mm->mm_scan_timeout);
  mpegts_network_scan_timer_arm(0);
  mpegts_network_scan_notify(mm);
}

void
mpegts_network_scan_queue_add ( mpegts_mux_t *mm, int weight )
{
  int reload = 0;
  char buf[256];

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
    TAILQ_REMOVE(&mpegts_network_scan_pend, mm, mm_scan_link);
  }

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhdebug("mpegts", "adding mux %p:%s to queue weight %d", mm, buf, weight);

  /* Add new entry */
  mm->mm_scan_state = MM_SCAN_STATE_PEND;
  TAILQ_INSERT_SORTED_R(&mpegts_network_scan_pend, mpegts_mux_queue,
                        mm, mm_scan_link, mm_cmp);
  mpegts_network_scan_timer_arm(0);
  mpegts_network_scan_notify(mm);
}

/******************************************************************************
 * Subsystem setup / tear down
 *****************************************************************************/

void
mpegts_network_scan_init ( void )
{
  TAILQ_INIT(&mpegts_network_scan_pend);
  TAILQ_INIT(&mpegts_network_scan_active);
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
