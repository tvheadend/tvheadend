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
    assert(mm->mm_scan_state == MM_SCAN_STATE_PEND || mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);

    /* Don't try to subscribe already tuned muxes */
    if (mm->mm_active) continue;

    /* Attempt to tune */
    r = mpegts_mux_subscribe(mm, NULL, "scan", mm->mm_scan_weight,
                             mm->mm_scan_flags |
                             SUBSCRIPTION_ONESHOT |
                             SUBSCRIPTION_TABLES);

    /* Started */
    if (!r) {
      assert(mm->mm_scan_state == MM_SCAN_STATE_ACTIVE);
      continue;
    }
    assert(mm->mm_scan_state == MM_SCAN_STATE_PEND);

    /* No free tuners - stop */
    if (r == SM_CODE_NO_FREE_ADAPTER || r == SM_CODE_NO_ADAPTERS)
      break;

    /* No valid tuners (subtly different, might be able to tuner a later
     * mux)
     */
    if (r == SM_CODE_NO_VALID_ADAPTER && mm->mm_is_enabled(mm))
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
  mpegts_network_t *mn = mm->mm_network;

  mpegts_mux_unsubscribe_by_name(mm, "scan");
  if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    if (weight || mn->mn_idlescan) {
      if (!weight)
        mm->mm_scan_weight = SUBSCRIPTION_PRIO_SCAN_IDLE;
      TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
      TAILQ_INSERT_SORTED_R(&mn->mn_scan_pend, mpegts_mux_queue,
                            mm, mm_scan_link, mm_cmp);
      gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 10);
      weight = 0;
    } else {
      mpegts_network_scan_queue_del(mm);
    }
  } else {
    if (!weight && mn->mn_idlescan)
      weight = SUBSCRIPTION_PRIO_SCAN_IDLE;
    mpegts_network_scan_queue_del(mm);
  }

  if (result != MM_SCAN_NONE && mm->mm_scan_result != result) {
    mm->mm_scan_result = result;
    mm->mm_config_save(mm);
  }

  /* Re-enable? */
  if (weight > 0)
    mpegts_network_scan_queue_add(mm, weight, mm->mm_scan_flags, 10);
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
mpegts_network_scan_mux_cancel  ( mpegts_mux_t *mm, int reinsert )
{
  if (mm->mm_scan_state != MM_SCAN_STATE_ACTIVE)
    return;

  if (!reinsert)
    mm->mm_scan_flags = 0;

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
  char buf[256], buf2[256];
  if (mm->mm_scan_state == MM_SCAN_STATE_ACTIVE) {
    TAILQ_REMOVE(&mn->mn_scan_active, mm, mm_scan_link);
  } else if (mm->mm_scan_state == MM_SCAN_STATE_PEND) {
    TAILQ_REMOVE(&mn->mn_scan_pend, mm, mm_scan_link);
  }
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  mn->mn_display_name(mn, buf2, sizeof(buf2));
  tvhdebug("mpegts", "%s - removing mux %s from scan queue", buf2, buf);
  mm->mm_scan_state  = MM_SCAN_STATE_IDLE;
  mm->mm_scan_weight = 0;
  gtimer_disarm(&mm->mm_scan_timeout);
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);
  mpegts_network_scan_notify(mm);
}

void
mpegts_network_scan_queue_add
  ( mpegts_mux_t *mm, int weight, int flags, int delay )
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
  tvhdebug("mpegts", "%s - adding mux %s to scan queue weight %d flags %04X",
           buf2, buf, weight, flags);

  /* Add new entry */
  mm->mm_scan_state  = MM_SCAN_STATE_PEND;
  mm->mm_scan_flags |= flags;
  if (mm->mm_scan_flags == 0)
    mm->mm_scan_flags = SUBSCRIPTION_IDLESCAN;
  TAILQ_INSERT_SORTED_R(&mn->mn_scan_pend, mpegts_mux_queue,
                        mm, mm_scan_link, mm_cmp);
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, delay);
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
          char buf[256];
          mpegts_mux_nice_name(mm, buf, sizeof(buf));
          tvhinfo("mpegts", "fastscan mux found '%s', set scan state 'PENDING'", buf);
          mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
          return;
        }
    tvhinfo("mpegts", "fastscan mux not found, position:%i, frequency:%i, polarisation:%c", satpos, freq, pol[0]);

    // fastscan mux not found, try to add it automatically
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
      if (mn->mn_satpos != INT_MAX && mn->mn_satpos == satpos)
      {
        dvb_mux_conf_t *mux;
        mpegts_mux_t *mm = NULL;

        mux = malloc(sizeof(dvb_mux_conf_t));
        dvb_mux_conf_init(mux, dvbs2 ? DVB_SYS_DVBS2 : DVB_SYS_DVBS);
        mux->dmc_fe_freq = freq;
        mux->u.dmc_fe_qpsk.symbol_rate = symbol;
        mux->u.dmc_fe_qpsk.polarisation = dvb_str2pol(pol);
        mux->u.dmc_fe_qpsk.orbital_pos = satpos;
        mux->u.dmc_fe_qpsk.fec_inner = DVB_FEC_AUTO;
        mux->dmc_fe_modulation = dvbs2 ? DVB_MOD_PSK_8 : DVB_MOD_QPSK;
        mux->dmc_fe_rolloff = DVB_ROLLOFF_AUTO;
        mux->dmc_fe_pls_code = 1;

        mm = (mpegts_mux_t*)dvb_mux_create0((dvb_network_t*)mn,
                                            MPEGTS_ONID_NONE,
                                            MPEGTS_TSID_NONE,
                                            mux, NULL, NULL);
        if (mm)
        {
          mm->mm_config_save(mm);
          char buf[256];
          mn->mn_display_name(mn, buf, sizeof(buf));
          tvhinfo("mpegts", "fastscan mux add to network '%s'", buf);
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
