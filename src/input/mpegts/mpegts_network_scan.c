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

static int
mm_cmp ( mpegts_mux_t *a, mpegts_mux_t *b )
{
  return a->mm_scan_weight - b->mm_scan_weight;
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
  mpegts_mux_t *mm, *mark = NULL;
  int r;

  /* Process Q */
  while ((mm = TAILQ_FIRST(&mpegts_network_scan_pend))) {
    assert(mm->mm_scan_state == MM_SCAN_STATE_PEND);

    /* Stop (looped) */
    if (mm == mark) break;

    /* Attempt to tune */
    // TODO: change reason?
    r = mpegts_mux_subscribe(mm, "scan", mm->mm_scan_weight);

    /* Stop (no free tuners) */
    if (r == SM_CODE_NO_FREE_ADAPTER)
      break;

    /* Started */
    if (!r) {
      assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);
      continue;
    }

    /* Available tuners can't be used
     * Note: this is subtly different it does not imply there are no free
     *       tuners, just that none of the free ones can service this mux.
     *       therefore we move this to the back of the queue and see if we
     *       can find one we can tune
     */
    if (r == SM_CODE_NO_VALID_ADAPTER) {
      if (!mark) mark = mm;
      TAILQ_INSERT_SORTED(&mpegts_network_scan_pend, mm, mm_scan_link, mm_cmp);
      continue;
    }

    /* Failed */
    mpegts_network_scan_mux_fail(mm);
  }

  /* Re-arm (backstop, most things will auto-rearm at point of next event
   * such as timeout of scan or completion)
   */
  mpegts_network_scan_timer_arm(10);
}

/******************************************************************************
 * Mux transition
 *****************************************************************************/

/* Failed - couldn't start */
void
mpegts_network_scan_mux_fail    ( mpegts_mux_t *mm )
{
  mm->mm_scan_ok = 0;
  mpegts_network_scan_queue_del(mm);
}

/* Completed succesfully */
void
mpegts_network_scan_mux_done    ( mpegts_mux_t *mm )
{
  mm->mm_scan_ok    = 1;
  mpegts_network_scan_queue_del(mm);
}

/* Failed - no input */
void
mpegts_network_scan_mux_timeout ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_fail(mm);
}

/* Interrupted (re-add) */
void
mpegts_network_scan_mux_cancel  ( mpegts_mux_t *mm, int reinsert )
{
  assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);

  /* Remove */
  mpegts_network_scan_queue_del(mm);

  /* Re-insert */
  if (reinsert)
    mpegts_network_scan_queue_add(mm, mm->mm_scan_weight);
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
  if (mm->mm_scan_state != MM_SCAN_STATE_IDLE)
    TAILQ_REMOVE(&mpegts_network_scan_active, mm, mm_scan_link);
  mm->mm_scan_state = MM_SCAN_STATE_IDLE;
  gtimer_disarm(&mm->mm_scan_timeout);
}

void
mpegts_network_scan_queue_add ( mpegts_mux_t *mm, int weight )
{
  int reload = 0;

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

  /* Add new entry */
  mm->mm_scan_state = MM_SCAN_STATE_PEND;
  TAILQ_INSERT_SORTED(&mpegts_network_scan_pend, mm, mm_scan_link, mm_cmp);
  mpegts_network_scan_timer_arm(0);
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
