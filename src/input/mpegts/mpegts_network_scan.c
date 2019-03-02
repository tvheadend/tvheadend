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
  idnode_notify_changed(&mm->mm_id);
  idnode_notify_changed(&mm->mm_network->mn_id);
}

/*
 * rules:
 *   1) prefer weight
 *   2) prefer muxes with the newer timestamp (scan interrupted?)
 *   3) do standard mux sorting
 */
static int
mm_cmp ( mpegts_mux_t *a, mpegts_mux_t *b )
{
  int r = b->mm_scan_weight - a->mm_scan_weight;
  if (r == 0) {
    int64_t l = b->mm_start_monoclock - a->mm_start_monoclock;
    if (l == 0)
      return mpegts_mux_compare(a, b);
    r = l > 0 ? 1 : -1;
  }
  return r;
}

/*
 * rules:
 *   1) prefer weight
 *   2) prefer muxes with the oldest timestamp
 *   3) do standard mux sorting
 */
static int
mm_cmp_idle ( mpegts_mux_t *a, mpegts_mux_t *b )
{
  int r = b->mm_scan_weight - a->mm_scan_weight;
  if (r == 0) {
    int64_t l = a->mm_start_monoclock - b->mm_start_monoclock;
    if (l == 0)
      return mpegts_mux_compare(a, b);
    r = l > 0 ? 1 : -1;
  }
  return r;
}

static void
mpegts_network_scan_queue_del0 ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE) {
    TAILQ_REMOVE(&mn->mn_scan_active, mm, mm_scan_link);
  } else if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
  } else if (mm->mm_scan_state == MM_SCAN_STATE_IPEND) {
    TAILQ_REMOVE(&mn->mn_scan_ipend, mm, mm_scan_link);
  }
}

static int
mpegts_network_scan_do_mux ( mpegts_mux_queue_t *q, mpegts_mux_t *mm )
{
  int r, state = mm->mm_scan_state;

  assert(state == MM_SCAN_STATE_PEND ||
         state == MM_SCAN_STATE_IPEND ||
         state == MM_SCAN_STATE_ACTIVE);

  /* Don't try to subscribe already tuned muxes */
  if (mm->mm_active) return 0;

  /* Attempt to tune */
  r = mpegts_mux_subscribe(mm, NULL, "scan", mm->mm_scan_weight,
                           mm->mm_scan_flags |
                           SUBSCRIPTION_ONESHOT |
                           SUBSCRIPTION_TABLES);

  /* Started */
  state = mm->mm_scan_state;
  if (!r) {
    assert(state == MM_SCAN_STATE_ACTIVE);
    return 0;
  }
  assert(state == MM_SCAN_STATE_PEND ||
         state == MM_SCAN_STATE_IPEND);

  /* No free tuners - stop */
  if (r == SM_CODE_NO_FREE_ADAPTER || r == SM_CODE_NO_ADAPTERS)
    return -1;

  /* No valid tuners (subtly different, might be able to tuner a later
   * mux)
   */
  if (r == SM_CODE_NO_VALID_ADAPTER && mm->mm_is_enabled(mm))
    return 0;

  /* Failed */
  TAILQ_REMOVE(q, mm, mm_scan_link);
  if (mm->mm_scan_result != MM_SCAN_FAIL) {
    mm->mm_scan_result = MM_SCAN_FAIL;
    idnode_changed(&mm->mm_id);
  }
  mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
  mm->mm_scan_weight = 0;
  mpegts_network_scan_notify(mm);
  return 0;
}

void
mpegts_network_scan_timer_cb ( void *p )
{
  mpegts_network_t *mn = p;
  mpegts_mux_t *mm, *nxt = NULL;

  /* Process standard Q */
  for (mm = TAILQ_FIRST(&mn->mn_scan_pend); mm != NULL; mm = nxt) {
    nxt = TAILQ_NEXT(mm, mm_scan_link);
    if (mpegts_network_scan_do_mux(&mn->mn_scan_pend, mm))
      break;
  }

  /* Process idle Q */
  for (mm = TAILQ_FIRST(&mn->mn_scan_ipend); mm != NULL; mm = nxt) {
    nxt = TAILQ_NEXT(mm, mm_scan_link);
    if (mpegts_network_scan_do_mux(&mn->mn_scan_ipend, mm))
      break;
  }

  /* Re-arm timer. Really this is just a safety measure as we'd normally
   * expect the timer to be forcefully triggered on finish of a mux scan
   */
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, sec2mono(120));
}

/******************************************************************************
 * Mux transition
 *****************************************************************************/

static void
mpegts_network_scan_mux_add ( mpegts_network_t *mn, mpegts_mux_t *mm )
{
  TAILQ_INSERT_SORTED_R(&mn->mn_scan_ipend, mpegts_mux_queue, mm,
                        mm_scan_link, mm_cmp);
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb,
                 mn, sec2mono(10));
}

static void
mpegts_network_scan_idle_mux_add ( mpegts_network_t *mn, mpegts_mux_t *mm )
{
  mm->mm_scan_weight = SUBSCRIPTION_PRIO_SCAN_IDLE;
  TAILQ_INSERT_SORTED_R(&mn->mn_scan_ipend, mpegts_mux_queue, mm,
                        mm_scan_link, mm_cmp_idle);
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb,
                 mn, sec2mono(10));
}

/* Finished */
static inline void
mpegts_network_scan_mux_done0
  ( mpegts_mux_t *mm, mpegts_mux_scan_result_t result, int weight )
{
  mpegts_network_t *mn = mm->mm_network;
  mpegts_mux_scan_state_t state = mm->mm_scan_state;

  if (result == MM_SCAN_OK || result == MM_SCAN_PARTIAL) {
    mm->mm_scan_last_seen = gclk();
    if (mm->mm_scan_first == 0)
      mm->mm_scan_first = mm->mm_scan_last_seen;
    idnode_changed(&mm->mm_id);
  }

  /* prevent double del: */
  /*   mpegts_mux_stop -> mpegts_network_scan_mux_cancel */
  mm->mm_scan_state = MM_SCAN_STATE_IDLE;
  mpegts_mux_unsubscribe_by_name(mm, "scan");
  mm->mm_scan_state = state;

  if (state == MM_SCAN_STATE_PEND || state == MM_SCAN_STATE_IPEND) {
    if (weight || mn->mn_idlescan) {
      mpegts_network_scan_queue_del0(mm);
      if (weight == 0 || weight == SUBSCRIPTION_PRIO_SCAN_IDLE)
        mpegts_network_scan_idle_mux_add(mn, mm);
      else
        mpegts_network_scan_mux_add(mn, mm);
      weight = 0;
    } else {
      mpegts_network_scan_queue_del(mm);
    }
  } else {
    if (weight == 0 && mn->mn_idlescan)
      weight = SUBSCRIPTION_PRIO_SCAN_IDLE;
    mpegts_network_scan_queue_del(mm);
  }

  if (result != MM_SCAN_NONE && mm->mm_scan_result != result) {
    mm->mm_scan_result = result;
    idnode_changed(&mm->mm_id);
  }

  /* Re-enable? */
  if (weight > 0)
    mpegts_network_scan_queue_add(mm, weight, mm->mm_scan_flags, 10);
}

/* Failed - couldn't start */
void
mpegts_network_scan_mux_fail ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_done0(mm, MM_SCAN_FAIL, 0);
}

/* Completed succesfully */
void
mpegts_network_scan_mux_done ( mpegts_mux_t *mm )
{
  mm->mm_scan_flags = 0;
  mpegts_network_scan_mux_done0(mm, MM_SCAN_OK, 0);
}

/* Partially completed (not all tables were read) */
void
mpegts_network_scan_mux_partial ( mpegts_mux_t *mm )
{
  mpegts_network_scan_mux_done0(mm, MM_SCAN_PARTIAL, 0);
}

/* Interrupted (re-add) */
void
mpegts_network_scan_mux_cancel ( mpegts_mux_t *mm, int reinsert )
{
  if (reinsert) {
    if (mm->mm_scan_state != MM_SCAN_STATE_ACTIVE)
      return;
  } else {
    if (mm->mm_scan_state == MM_SCAN_STATE_IDLE)
      return;
    mm->mm_scan_flags = 0;
  }

  mpegts_network_scan_mux_done0(mm, MM_SCAN_NONE,
                                reinsert ? mm->mm_scan_weight : 0);
}

/* Mux has been started */
void
mpegts_network_scan_mux_active ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  if (mm->mm_scan_state != MM_SCAN_STATE_PEND &&
      mm->mm_scan_state != MM_SCAN_STATE_IPEND)
    return;
  mpegts_network_scan_queue_del0(mm);
  mm->mm_scan_state = MM_SCAN_STATE_ACTIVE;
  mm->mm_scan_init  = 0;
  TAILQ_INSERT_TAIL(&mn->mn_scan_active, mm, mm_scan_link);
}

/* Mux has been reactivated */
void
mpegts_network_scan_mux_reactivate ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE)
    return;
  mpegts_network_scan_queue_del0(mm);
  mm->mm_scan_init  = 0;
  mm->mm_scan_state = MM_SCAN_STATE_ACTIVE;
  TAILQ_INSERT_TAIL(&mn->mn_scan_active, mm, mm_scan_link);
}

/******************************************************************************
 * Mux queue handling
 *****************************************************************************/

void
mpegts_network_scan_queue_del ( mpegts_mux_t *mm )
{
  mpegts_network_t *mn = mm->mm_network;
  tvhdebug(LS_MPEGTS, "removing mux %s from scan queue", mm->mm_nicename);
  mpegts_network_scan_queue_del0(mm);
  mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
  mm->mm_scan_weight = 0;
  mtimer_disarm(&mm->mm_scan_timeout);
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);
  mpegts_network_scan_notify(mm);
}

void
mpegts_network_scan_queue_add
  ( mpegts_mux_t *mm, int weight, int flags, int delay )
{
  int requeue = 0;
  mpegts_network_t *mn = mm->mm_network;

  if (!mm->mm_is_enabled(mm)) return;

  if (weight <= 0) return;

  if (weight > mm->mm_scan_weight) {
    mm->mm_scan_weight = weight;
    requeue = 1;
  }

  /* Already active */
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE)
    return;

  /* Remove entry (or ignore) */
  if (mm->mm_scan_state == MM_SCAN_STATE_PEND ||
      mm->mm_scan_state == MM_SCAN_STATE_IPEND) {
    if (!requeue)
      return;
    mpegts_network_scan_queue_del0(mm);
  }

  tvhdebug(LS_MPEGTS, "adding mux %s to scan queue weight %d flags %04X",
                      mm->mm_nicename, weight, flags);

  /* Add new entry */
  mm->mm_scan_flags |= flags;
  if (mm->mm_scan_flags == 0)
    mm->mm_scan_flags = SUBSCRIPTION_IDLESCAN;
  if (weight == SUBSCRIPTION_PRIO_SCAN_IDLE) {
    mm->mm_scan_state  = MM_SCAN_STATE_IPEND;
    TAILQ_INSERT_SORTED_R(&mn->mn_scan_ipend, mpegts_mux_queue,
                          mm, mm_scan_link, mm_cmp_idle);
  } else {
    mm->mm_scan_state  = MM_SCAN_STATE_PEND;
    TAILQ_INSERT_SORTED_R(&mn->mn_scan_pend, mpegts_mux_queue,
                          mm, mm_scan_link, mm_cmp);
  }
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, sec2mono(delay));
  mpegts_network_scan_notify(mm);
}

/******************************************************************************
 * Bouquet helper
 *****************************************************************************/

#if ENABLE_MPEGTS_DVB
static ssize_t
startswith( const char *str, const char *start )
{
  size_t len = strlen(start);
  if (!strncmp(str, start, len))
    return len;
  return -1;
}
#endif

void
mpegts_mux_bouquet_rescan ( const char *src, const char *extra )
{
#if ENABLE_MPEGTS_DVB
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  ssize_t l;
  const idclass_t *ic;
  uint32_t freq;
  int satpos;
#endif

  if (!src)
    return;
#if ENABLE_MPEGTS_DVB
  if ((l = startswith(src, "dvb-bouquet://dvbs,")) > 0) {
    uint32_t tsid, nbid;
    src += l;
    if ((satpos = dvb_sat_position_from_str(src)) == INT_MAX)
      return;
    while (*src && *src != ',')
      src++;
    if (sscanf(src, ",%x,%x", &tsid, &nbid) != 2)
      return;
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
        if (idnode_is_instance(&mm->mm_id, &dvb_mux_dvbs_class) &&
            mm->mm_tsid == tsid &&
            ((dvb_mux_t *)mm)->lm_tuning.u.dmc_fe_qpsk.orbital_pos == satpos)
          mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
    return;
  }
  if ((l = startswith(src, "dvb-bouquet://dvbt,")) > 0) {
    uint32_t tsid, nbid;
    if (sscanf(src, "%x,%x", &tsid, &nbid) != 2)
      return;
    ic = &dvb_mux_dvbt_class;
    goto tsid_lookup;
  }
  if ((l = startswith(src, "dvb-bouquet://dvbc,")) > 0) {
    uint32_t tsid, nbid;
    if (sscanf(src, "%x,%x", &tsid, &nbid) != 2)
      return;
    ic = &dvb_mux_dvbc_class;
tsid_lookup:
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
        if (idnode_is_instance(&mm->mm_id, ic) &&
            mm->mm_tsid == tsid)
          mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
    return;
  }
  if ((l = startswith(src, "dvb-bskyb://dvbs,")) > 0 ||
      (l = startswith(src, "dvb-freesat://dvbs,")) > 0) {
    if ((satpos = dvb_sat_position_from_str(src + l)) == INT_MAX)
      return;
    /* a bit tricky, but we don't have other info */
    if (!extra)
      return;
    freq = strtod(extra, NULL) * 1000;

    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
        if (idnode_is_instance(&mm->mm_id, &dvb_mux_dvbs_class) &&
            deltaU32(((dvb_mux_t *)mm)->lm_tuning.dmc_fe_freq, freq) < 2000 &&
            ((dvb_mux_t *)mm)->lm_tuning.u.dmc_fe_qpsk.orbital_pos == satpos)
          mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
    return;
  }
  if ((l = startswith(src, "dvb-fastscan://")) > 0) {
    uint32_t pid, symbol, dvbs2;
    char pol[2];
    pol[1] = '\0';
    src += l;

    if ((l = startswith(src, "DVBS2,")) > 0 || (l = startswith(src, "DVBS-2,")) > 0)
      dvbs2 = 1;
    else if ((l = startswith(src, "DVBS,")) > 0 || (l = startswith(src, "DVB-S,")) > 0)
      dvbs2 = 0;
    else
      return;
    src += l;

    if ((satpos = dvb_sat_position_from_str(src)) == INT_MAX)
      return;
    while (*src && *src != ',')
      src++;
    if (sscanf(src, ",%u,%c,%u,%u", &freq, &pol[0], &symbol, &pid) != 4)
      return;

    // search for fastscan mux
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
        if (idnode_is_instance(&mm->mm_id, &dvb_mux_dvbs_class) &&
            deltaU32(((dvb_mux_t *)mm)->lm_tuning.dmc_fe_freq, freq) < 2000 &&
            ((dvb_mux_t *)mm)->lm_tuning.u.dmc_fe_qpsk.polarisation == dvb_str2pol(pol) &&
            ((dvb_mux_t *)mm)->lm_tuning.u.dmc_fe_qpsk.orbital_pos == satpos)
        {
          tvhinfo(LS_MPEGTS, "fastscan mux found '%s', set scan state 'PENDING'", mm->mm_nicename);
          mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
          return;
        }
    tvhinfo(LS_MPEGTS, "fastscan mux not found, position:%i, frequency:%i, polarisation:%c", satpos, freq, pol[0]);

    // fastscan mux not found, try to add it automatically
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      if (mn->mn_satpos != INT_MAX && mn->mn_satpos == satpos)
      {
        dvb_mux_conf_t *mux;
        mpegts_mux_t *mm = NULL;

        mux = malloc(sizeof(dvb_mux_conf_t));
        dvb_mux_conf_init(mn, mux, dvbs2 ? DVB_SYS_DVBS2 : DVB_SYS_DVBS);
        mux->dmc_fe_freq = freq;
        mux->u.dmc_fe_qpsk.symbol_rate = symbol;
        mux->u.dmc_fe_qpsk.polarisation = dvb_str2pol(pol);
        mux->u.dmc_fe_qpsk.orbital_pos = satpos;
        mux->u.dmc_fe_qpsk.fec_inner = DVB_FEC_AUTO;
        mux->dmc_fe_modulation = dvbs2 ? DVB_MOD_PSK_8 : DVB_MOD_QPSK;
        mux->dmc_fe_rolloff = DVB_ROLLOFF_AUTO;

        mm = (mpegts_mux_t*)dvb_mux_create0((dvb_network_t*)mn,
                                            MPEGTS_ONID_NONE,
                                            MPEGTS_TSID_NONE,
                                            mux, NULL, NULL);
        if (mm) {
          char buf[256];
          mpegts_mux_post_create(mm);
          idnode_changed(&mm->mm_id);
          mn->mn_display_name(mn, buf, sizeof(buf));
          tvhinfo(LS_MPEGTS, "fastscan mux add to network '%s'", buf);
        }
        free(mux);
      }
    return;
  }
#endif
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
