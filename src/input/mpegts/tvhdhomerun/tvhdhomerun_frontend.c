/*
 *  Tvheadend - HDHomeRun DVB frontend
 *
 *  Copyright (C) 2014 Patric Karlström
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

#include "libhdhomerun/hdhomerun.h"

#include <fcntl.h>
#include "tvheadend.h"
#include "tvhpoll.h"
#include "streaming.h"
#include "udp.h"
#include "tvhdhomerun_private.h"

#include <arpa/inet.h>
#include "config.h"

static int
tvhdhomerun_frontend_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  return mpegts_input_get_weight(mi, mm, flags, weight);
}

static int
tvhdhomerun_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  return mpegts_input_get_priority(mi, mm, flags);
}

static int
tvhdhomerun_frontend_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  return hfe->hf_grace_period > 0 ? MINMAX(hfe->hf_grace_period, 1, 60) : 5;
}

static int
tvhdhomerun_frontend_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  return mpegts_input_is_enabled(mi, mm, flags, weight);
}

static void
tvhdhomerun_frontend_stop_streaming(tvhdhomerun_frontend_t *hfe)
{
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, "none");
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);

  hfe->hf_locked = 0;
  hfe->hf_running  = 0;
  hfe->hf_status = SIGNAL_NONE;
  hfe->hf_last_tune = getfastmonoclock();
}

static void
tvhdhomerun_frontend_monitor_cb( void *aux )
{
  tvhdhomerun_frontend_t  *hfe = aux;
  mpegts_mux_instance_t   *mmi = LIST_FIRST(&hfe->mi_mux_active);
  mpegts_mux_t            *mm;
  streaming_message_t      sm;
  signal_status_t          sigstat;
  service_t               *svc;
  int                      res;
  char                     buf[256];

  struct hdhomerun_tuner_status_t tuner_status;

  /* Stop timer */
  if (!mmi) return;

  if (!hfe->hf_tables) {
    psi_tables_install(mmi->mmi_input, mmi->mmi_mux,
                       ((dvb_mux_t *)mmi->mmi_mux)->lm_tuning.dmc_fe_delsys);
    hfe->hf_tables = 1;
  }

  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));

  /* Get current status */
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_get_tuner_status(hfe->hf_hdhomerun_tuner, NULL, &tuner_status);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 0)
    tvhdebug(LS_TVHDHOMERUN, "%s - tuner_status: device %s communication error", buf, hfe->hf_device->hd_info.friendlyname);
  if(res == 0)
    tvhdebug(LS_TVHDHOMERUN, "%s - tuner_status: device %s in use", buf, hfe->hf_device->hd_info.friendlyname);

  if(tuner_status.signal_present)
    hfe->hf_status = SIGNAL_GOOD;
  else if (tuner_status.lock_supported || tuner_status.lock_unsupported)
    hfe->hf_status = SIGNAL_UNKNOWN;
  else
    hfe->hf_status = SIGNAL_NONE;

  /* Get current mux */
  mm = mmi->mmi_mux;

  /* wait for a signal_present */
  if(!hfe->hf_locked) {
    if(tuner_status.signal_present) {
      tvhdebug(LS_TVHDHOMERUN, "%s - tuner_status: signal locked", buf);
      hfe->hf_locked = 1;

      /* Get CableCARD variables */
      if (hfe->hf_type == DVB_TYPE_CABLECARD) {
        dvb_mux_t *lm = (dvb_mux_t *)mm;
        struct hdhomerun_tuner_vstatus_t tuner_vstatus;
        tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
        res = hdhomerun_device_get_tuner_vstatus(hfe->hf_hdhomerun_tuner,
                                                 NULL, &tuner_vstatus);
        tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
        if(res < 0)
          tvhdebug(LS_TVHDHOMERUN, "%s - tuner_vstatus: device %s communication error", buf, hfe->hf_device->hd_info.friendlyname);
        if(res == 0)
          tvhdebug(LS_TVHDHOMERUN, "%s - tuner_vstatus: device %s in use", buf, hfe->hf_device->hd_info.friendlyname);

        free(lm->lm_tuning.u.dmc_fe_cablecard.name);
        lm->lm_tuning.u.dmc_fe_cablecard.name = strdup(tuner_vstatus.name);

        char *p = strchr(tuner_status.channel, ':');
        if (p)
          sscanf(p, ":%u", &lm->lm_tuning.dmc_fe_freq);
      }

    }
  }

  tvh_mutex_lock(&mmi->tii_stats_mutex);

  if(tuner_status.signal_present) {
    /* TODO: totaly stupid conversion from 0-100 scale to 0-655.35 */
    mmi->tii_stats.snr = tuner_status.signal_to_noise_quality * 655.35;
    mmi->tii_stats.signal = tuner_status.signal_strength * 655.35;
  } else {
    mmi->tii_stats.snr = SIGNAL_NONE;
    mmi->tii_stats.signal = SIGNAL_NONE;
  }

  sigstat.status_text  = signal2str(hfe->hf_status);
  sigstat.snr          = mmi->tii_stats.snr;
  sigstat.snr_scale    = mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
  sigstat.signal       = mmi->tii_stats.signal;
  sigstat.signal_scale = mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
  sigstat.ber          = mmi->tii_stats.ber;
  sigstat.unc          = atomic_get(&mmi->tii_stats.unc);
  memset(&sm, 0, sizeof(sm));
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;

  tvh_mutex_unlock(&mmi->tii_stats_mutex);

  LIST_FOREACH(svc, &mmi->mmi_mux->mm_transports, s_active_link) {
    tvh_mutex_lock(&svc->s_stream_mutex);
    streaming_service_deliver(svc, streaming_msg_clone(&sm));
    tvh_mutex_unlock(&svc->s_stream_mutex);
  }

  /* re-arm */
  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb,
                 hfe, sec2mono(1));
}

static uint32_t get_local_ip(struct hdhomerun_device_t *tuner)
{
  uint32_t local_ip;
  if (*config.local_ip == 0)
    return hdhomerun_device_get_local_machine_addr(tuner);

  if (inet_pton(AF_INET, config.local_ip, &local_ip) == 1)
    return ntohl(local_ip);

  return hdhomerun_device_get_local_machine_addr(tuner);
}

static int tvhdhomerun_frontend_lockkey_request(tvhdhomerun_frontend_t *hfe, const char *name)
{
  int res;
  char *error = NULL;

  /*
   * Attempt to aquire lock.
   */
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_tuner_lockkey_request(hfe->hf_hdhomerun_tuner, &error);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if (res > 0)  {
    return res;
  }
  if (res < 0) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s communication error", name, hfe->hf_device->hd_info.friendlyname);
    return res;
  }

  /*
   * In use - check target.
   */
  char *target;
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
	res = hdhomerun_device_get_tuner_target(hfe->hf_hdhomerun_tuner, &target);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
	if (res < 0) {
		tvherror(LS_TVHDHOMERUN, "%s - lockkey_request_target: device %s communication error", name, hfe->hf_device->hd_info.friendlyname);
		return -1;
	}
	if (res == 0) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request_target: device %s in use, failed to read target", name, hfe->hf_device->hd_info.friendlyname);
		return -1;
	}

	if (strcmp(target, "none") == 0) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s in use, no target set", name, hfe->hf_device->hd_info.friendlyname);
		return -1;
	}

	if ((strncmp(target, "udp://", 6) != 0) && (strncmp(target, "rtp://", 6) != 0)) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s in use by %s", name, hfe->hf_device->hd_info.friendlyname, target);
		return -1;
	}

	unsigned int a[4];
	unsigned int target_port;
	if (sscanf(target + 6, "%u.%u.%u.%u:%u", &a[0], &a[1], &a[2], &a[3], &target_port) != 5) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s in use, unexpected target set (%s)", name, hfe->hf_device->hd_info.friendlyname, target);
		return -1;
	}

	uint32_t target_ip = (uint32_t)((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0));
	uint32_t local_ip = get_local_ip(hfe->hf_hdhomerun_tuner);
	if (target_ip != local_ip) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s in use by %s", name, hfe->hf_device->hd_info.friendlyname, target);
		return -1;
	}

	/*
   * Dead local target, force clear lock.
   */
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
	res = hdhomerun_device_tuner_lockkey_force(hfe->hf_hdhomerun_tuner);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
	if (res < 0) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_force: device %s communication error", name, hfe->hf_device->hd_info.friendlyname);
		return -1;
	}
	if (res == 0) {
    tvherror(LS_TVHDHOMERUN, "%s - lockkey_force: device %s in use by local machine, dead target, failed to force release lockkey", name, hfe->hf_device->hd_info.friendlyname);
		return -1;
	}

  tvhinfo(LS_TVHDHOMERUN, "%s - lockkey_force: device %s in use by local machine, dead target, lockkey force successful", name, hfe->hf_device->hd_info.friendlyname);

	/*
   * Attempt to aquire lock.
   */
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
	res = hdhomerun_device_tuner_lockkey_request(hfe->hf_hdhomerun_tuner, &error);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
	if (res > 0) {
		return res;
	}
	if (res < 0) {
		tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s communication error", name, hfe->hf_device->hd_info.friendlyname);
		return res;
	}

  tvherror(LS_TVHDHOMERUN, "%s - lockkey_request: device %s still in use after lockkey force (%s)", name, hfe->hf_device->hd_info.friendlyname, error);
	return -1;
}

static void
tvhdhomerun_build_channel_buf(const dvb_mux_conf_t *dmc, char *channel_buf, size_t channel_len)
{
  uint32_t symbol_rate = 0;
  uint8_t bandwidth = 0;

  switch (dmc->dmc_fe_type) {

    case DVB_TYPE_C:
      symbol_rate = dmc->u.dmc_fe_qam.symbol_rate / 1000;
      switch (dmc->dmc_fe_modulation) {
        case DVB_MOD_QAM_64:
          snprintf(channel_buf, channel_len, "a8qam64-%u:%u",
                   symbol_rate, dmc->dmc_fe_freq);
          break;
        case DVB_MOD_QAM_256:
          snprintf(channel_buf, channel_len, "a8qam256-%u:%u",
                   symbol_rate, dmc->dmc_fe_freq);
          break;
        default:
          snprintf(channel_buf, channel_len, "auto:%u", dmc->dmc_fe_freq);
          break;
      }
      break;

    case DVB_TYPE_T:
      bandwidth = dmc->u.dmc_fe_ofdm.bandwidth / 1000UL;
      switch (dmc->dmc_fe_modulation) {
        case DVB_MOD_AUTO:
          if (dmc->u.dmc_fe_ofdm.bandwidth == DVB_BANDWIDTH_AUTO)
            snprintf(channel_buf, channel_len, "auto:%u", dmc->dmc_fe_freq);
          else
            snprintf(channel_buf, channel_len, "auto%dt:%u",
                     bandwidth, dmc->dmc_fe_freq);
          break;

        case DVB_MOD_QAM_256:
          if (dmc->dmc_fe_delsys == DVB_SYS_DVBT2)
            snprintf(channel_buf, channel_len, "tt%dqam256:%u",
                     bandwidth, dmc->dmc_fe_freq);
          else
            snprintf(channel_buf, channel_len, "t%dqam256:%u",
                     bandwidth, dmc->dmc_fe_freq);
          break;

        case DVB_MOD_QAM_64:
          if (dmc->dmc_fe_delsys == DVB_SYS_DVBT2)
            snprintf(channel_buf, channel_len, "tt%dqam64:%u",
                     bandwidth, dmc->dmc_fe_freq);
          else
            snprintf(channel_buf, channel_len, "t%dqam64:%u",
                     bandwidth, dmc->dmc_fe_freq);
          break;

        default:
          snprintf(channel_buf, channel_len, "auto:%u", dmc->dmc_fe_freq);
          break;
      }
      break;

    case DVB_TYPE_CABLECARD:
      snprintf(channel_buf, channel_len, "%u",
               dmc->u.dmc_fe_cablecard.vchannel);
      break;

    case DVB_TYPE_ATSC_T:
      switch (dmc->dmc_fe_modulation) {
        case DVB_MOD_VSB_8:
          snprintf(channel_buf, channel_len, "auto6t:%u", dmc->dmc_fe_freq);
          break;
        default:
          snprintf(channel_buf, channel_len, "auto:%u", dmc->dmc_fe_freq);
          break;
      }
      break;

    default:
      snprintf(channel_buf, channel_len, "auto:%u", dmc->dmc_fe_freq);
      break;
  }
}

static int tvhdhomerun_frontend_tune(tvhdhomerun_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  char channel_buf[64];
  char buf[256];
  int res;

  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));

  /* resolve the modulation type */
  tvhdhomerun_build_channel_buf(dmc, channel_buf, sizeof(channel_buf));

  tvhdebug(LS_TVHDHOMERUN, "%s - tuning to %s", buf, channel_buf);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  
  if (hfe->hf_type == DVB_TYPE_CABLECARD)
    res = hdhomerun_device_set_tuner_vchannel(hfe->hf_hdhomerun_tuner, channel_buf);
  else
    res = hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, channel_buf);

  if(res < 0) {
    tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    tvhdebug(LS_TVHDHOMERUN, "%s - tune error: device %s communication error", buf, hfe->hf_device->hd_info.friendlyname);
    return SM_CODE_TUNING_FAILED;
  }
  if(res == 0) {
    tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    tvhdebug(LS_TVHDHOMERUN, "%s - tune error: device %s in use", buf, hfe->hf_device->hd_info.friendlyname);
    return SM_CODE_TUNING_FAILED;
  }

  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  
  hfe->hf_status = SIGNAL_NONE;

  /* start the monitoring */
  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, 
                 hfe, ms2mono(50));

  return 0;
}

static void
tvhdhomerun_frontend_request_cleanup
  (tvhdhomerun_frontend_t *hfe, tvhdhomerun_tune_req_t *tr)
{
  if (tr && tr != hfe->hf_req) {
    mpegts_pid_done(&tr->hf_pids);
    free(tr);
  }
  if (tr == hfe->hf_req_thread)
    hfe->hf_req_thread = NULL;
}

static int
tvhdhomerun_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  dvb_mux_t *lm = (dvb_mux_t *)mmi->mmi_mux;
  tvhdhomerun_tune_req_t *tr;
  char buf1[256];

  hfe->mi_display_name((mpegts_input_t*)hfe, buf1, sizeof(buf1));
  tvhdebug(LS_TVHDHOMERUN, "%s - starting %s", buf1, lm->mm_nicename);

  tr = calloc(1, sizeof(*tr));
  tr->hf_mmi = mmi;

  mpegts_pid_init(&tr->hf_pids);

  tvh_mutex_lock(&hfe->hf_dvr_lock);
  hfe->hf_req     = tr;
  hfe->hf_running = 1;
  hfe->hf_tables  = 0;
  tvh_mutex_unlock(&hfe->hf_dvr_lock);
  tvh_mutex_lock(&mmi->tii_stats_mutex);
  hfe->hf_status    = SIGNAL_NONE;
  mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
  mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
  tvh_mutex_unlock(&mmi->tii_stats_mutex);

  /* notify thread that we are ready */
  tvh_write(hfe->hf_dvr_pipe.wr, "s", 1);

  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, 
                 hfe, ms2mono(50));

  return 0;
}

static void
tvhdhomerun_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;

  tvh_mutex_lock(&hfe->hf_dvr_lock);

  tvhdhomerun_tune_req_t *old = hfe->hf_req;
  hfe->hf_req = NULL;

  if (old && old != hfe->hf_req_thread) {
    mpegts_pid_done(&old->hf_pids);
    free(old);
  }

  tvh_mutex_unlock(&hfe->hf_dvr_lock);

  tvh_write(hfe->hf_dvr_pipe.wr, "x", 1); /* stop tune */
  mtimer_disarm(&hfe->hf_monitor_timer);
}

static void
tvhdhomerun_frontend_update_pids ( mpegts_input_t *mi, mpegts_mux_t *mm ) 
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  tvhdhomerun_tune_req_t *tr;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  tvh_mutex_lock(&hfe->hf_dvr_lock);
  if ((tr = hfe->hf_req) != NULL) {
    mpegts_pid_done(&tr->hf_pids);
    RB_FOREACH(mp, &mm->mm_pids, mp_link) {
      if (mp->mp_pid == MPEGTS_FULLMUX_PID) {
        if (hfe->hf_device->hd_fullmux_ok) {
          tr->hf_pids.all = 1;
        } else {
          mpegts_service_t *s;
          elementary_stream_t *st;
          int w = 0;
          RB_FOREACH(mps, &mp->mp_subs, mps_link)
            w = MAX(w, mps->mps_weight);
          LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
            mpegts_pid_add(&tr->hf_pids, s->s_components.set_pmt_pid, w);
            mpegts_pid_add(&tr->hf_pids, s->s_components.set_pcr_pid, w);
            TAILQ_FOREACH(st, &s->s_components.set_all, es_link)
              if (st->es_pid < MPEGTS_FULLMUX_PID)
                mpegts_pid_add(&tr->hf_pids, st->es_pid, w);
          }
        }
      } else if (mp->mp_pid < MPEGTS_FULLMUX_PID) {
        RB_FOREACH(mps, &mp->mp_subs, mps_link)
          mpegts_pid_add(&tr->hf_pids, mp->mp_pid, mps->mps_weight);
      }
    }
    mpegts_pid_add(&tr->hf_pids, 0, MPS_WEIGHT_PMT_SCAN);
  }
  tvh_mutex_unlock(&hfe->hf_dvr_lock);

  tvh_write(hfe->hf_dvr_pipe.wr, "c", 1);
}

static void tvhdhomerun_frontend_update_pids_append_pid_range(int a, int b, int *firstDelimiter, char **pBuffer, const char *endBuffer )
 /* A helper function that writes a range of pids to the 'buffer'.  This function is called more than once from tvhdhomerun_frontend_update_pids. */
{
  if(*firstDelimiter) /* Don't bother printing a space before the first range of pids. */
     *firstDelimiter = 0;  /* Set this to false the first time. */
  else {
    if(*pBuffer < endBuffer) /* Check if 'buffer' is full. */
      *pBuffer += snprintf(*pBuffer, endBuffer-*pBuffer, " "); /* After the first range, separate pid ranges by a space. */
  }
  if(*pBuffer < endBuffer) { /* Check if 'buffer' is full. */
    if(a == b)
      *pBuffer += snprintf(*pBuffer, endBuffer-*pBuffer, "0x%04x", a); /* First and last pid in a range are the same, then that one pid is appended. */
    else
      *pBuffer += snprintf(*pBuffer, endBuffer-*pBuffer, "0x%04x-0x%04x", a, b); /* Append a range of pids to 'buffer'. */
  }
}

static void tvhdhomerun_frontend_pid_changed( tvhdhomerun_frontend_t *hfe, const char *name )
{
  tvhdhomerun_tune_req_t *tr;
  tvhdhomerun_device_t *hd = hfe->hf_device;
  mpegts_apids_t wpid;
  int max_pids_count = hd->hd_pids_max;
  int res, overlimit;
  const unsigned int bufferSize = 1024;
  char buffer[bufferSize];

  tvh_mutex_lock(&hfe->hf_dvr_lock);

  tr = hfe->hf_req_thread;

  if (!hfe->hf_running || !hfe->hf_req || !tr) {
    tvh_mutex_unlock(&hfe->hf_dvr_lock);
    return;
  }

  if (hfe->hf_type == DVB_TYPE_CABLECARD) {
    tvh_mutex_unlock(&hfe->hf_dvr_lock);
    return;
  }

  buffer[0] = '\0'; /* Initialize buffer to handle the case where no pids are requested. */

  if(tr->hf_pids.all) {

all:
    tvhdebug(LS_TVHDHOMERUN, "%s - setting PID filter full mux", name);
    snprintf(buffer, bufferSize, "0x0000-0x1fff");

  } else {

    overlimit = mpegts_pid_weighted(&wpid, &tr->hf_pids,
                                    max_pids_count, MPS_WEIGHT_ALLLIMIT);

    if (overlimit > 0 && hd->hd_fullmux_ok) {
      mpegts_pid_done(&wpid);
      goto all;
    }

    if(wpid.count > 0) {
      int begin, prev, curr;

      begin = prev = wpid.pids[0].pid;
      const char *endBuffer = buffer + bufferSize; /* Have the address after the end of buffer on hand to help avoid writing past the buffer. */
      char *pBuffer = buffer; /* Move this pointer through the buffer as we write formatted pids. */
      int firstDelimiter = -1; /* Set this to 'true' so that we can skip writing the first delimiter/space. */

      /* Walk the list of pids and keep track of runs of consecutive pids. Setup the state for the first pid. */
      for (int i = 1; i < wpid.count; i++) {

        curr = wpid.pids[i].pid;
        /* make sure the pid maps to a max of 0x1FFF, API will reject the call otherwise */
        if(curr > 0x1FFF) {
          tvherror(LS_TVHDHOMERUN, "%s - pid %d is too large, masking to API maximum of 0x1FFF", name, curr);
          curr = (curr & 0x1FFF);
        }

        /* Check to see if there is a break in a run of consecutive pids. */
        if(prev + 1 != curr) {
          /* If the current pid is NOT +1 more than the previous pid, then this is the end of a range of pids.
            * Write out this range of consecutive pids. */
          tvhdhomerun_frontend_update_pids_append_pid_range(begin, prev, &firstDelimiter, &pBuffer, endBuffer);

          /* Also, this is the start of a new range of pids.  Set 'begin' to the beginning of the next range. */
          begin = curr;
        }
        prev = curr;  /* At bottom of the loop, current pid is now the previous pid. */
        if(pBuffer >= endBuffer)  /* We have reached the end of the 'buffer', no need to continue walking through the list. */
          break;
      }
      /* We are at the end of the list of pids, write the final range of consecutive pids to the 'buffer'. */
      tvhdhomerun_frontend_update_pids_append_pid_range(begin, prev, &firstDelimiter, &pBuffer, endBuffer);

      if(pBuffer >= endBuffer) { /* We could not fit the list of ranges of pids into the 'buffer' and have an incomplete/mangled 'buffer'
                                * so as a backup, we will request all pids except NULL packets (0x1fff). */
        tvhdebug(LS_TVHDHOMERUN, "%s - pfilter list is too big for buffer[%d] = \"%s\"(truncated)", name, bufferSize, buffer);
        snprintf(buffer, bufferSize, "0x0000-0x1ffe");
      }
    }
    mpegts_pid_done(&wpid);
  }

  tvh_mutex_unlock(&hfe->hf_dvr_lock);

  if (buffer[0] == '\0') {
    return;
  }

  tvhtrace(LS_TVHDHOMERUN, "%s - setting pfilter to: %s", name, buffer);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  /* Send the specially formatted list of pid ranges to the hdhomerun device. */
  res = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, buffer);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if (res == 0)
    tvherror(LS_TVHDHOMERUN, "%s - failed to set_tuner_filter, device %s in use", name, hfe->hf_device->hd_info.friendlyname);
  if (res < 1)
    tvherror(LS_TVHDHOMERUN, "%s - failed to set set_tuner_filter, device %s communication error", name, hfe->hf_device->hd_info.friendlyname);

  tvh_mutex_unlock(&hfe->hf_dvr_lock);
}

static ssize_t
tvhdhomerun_udp_read ( sbuf_t *sb, udp_connection_t *udp, udp_multirecv_t *um )
{
  int i, n;
  struct iovec *iovec;
  ssize_t res = 0;

  n = udp_multirecv_read(um, udp->fd, UDP_PKTS, &iovec);
  if (n < 0)
    return -1;

  for (i = 0; i < n; i++, iovec++) {
    if (iovec->iov_len <= 0)
      continue;
    if (*(uint8_t *)iovec->iov_base != 0x47) {
      continue;
    }
    sbuf_append(sb, iovec->iov_base, iovec->iov_len);
    res += iovec->iov_len;
  }

  return res;
}

static void *
tvhdhomerun_frontend_input_thread(void *aux)
{
  tvhdhomerun_frontend_t *hfe = aux;
  tvhdhomerun_tune_req_t *tr = NULL;
  mpegts_mux_instance_t *mmi = NULL;
  sbuf_t sb;
  tvhpoll_t *efd;
  udp_connection_t *udp = NULL;
  udp_multirecv_t um;
  char buf[256];
  char target[64];
  int nfds;
  uint64_t last_activity, fatal_timeout;
  uint8_t lockkey = 0, running = 0;

  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));

  efd = tvhpoll_create(2);
  tvhpoll_add1(efd, hfe->hf_dvr_pipe.rd, TVHPOLL_IN, NULL);

  sbuf_init(&sb);
  udp_multirecv_init(&um, 0, 0);

new_tune:
  tvhdhomerun_frontend_stop_streaming(hfe);
  sbuf_free(&sb);
  udp_multirecv_free(&um);
  if (udp) {
    tvhpoll_rem1(efd, udp->fd);
    udp_close(udp);
    udp = NULL;
  }
  udp_multirecv_init(&um, UDP_PKTS, UDP_PKT_SIZE);
  sbuf_init(&sb);
  running = 0;
  mmi = NULL;

  while (tvheadend_is_running()) {
    tvhpoll_event_t ev;
    nfds = tvhpoll_wait(efd, &ev, 1, lockkey ? 55 : -1);

    if (lockkey && (getfastmonoclock() - hfe->hf_last_tune > 50000)) { /* 50ms */
      tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
      hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
      tvhtrace(LS_TVHDHOMERUN, "%s - fast input lockkey release", buf);
      tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
      lockkey = 0;
    }

    if (nfds <= 0)
      continue;

    if (ev.ptr == NULL) {
      uint8_t cmd;
      if (read(hfe->hf_dvr_pipe.rd, &cmd, 1) != 1)
        continue;

      if (cmd == 'e') {
        tvhdebug(LS_TVHDHOMERUN, "HDHR input thread exit");
        goto done;
      }
      if (cmd == 's') {
        break;
      }
    }
  }

  if (!tvheadend_is_running())
    goto done;

  tvh_mutex_lock(&hfe->hf_dvr_lock);
  tvhdhomerun_frontend_request_cleanup(hfe, tr);
  tr = hfe->hf_req;
  hfe->hf_req_thread = tr;
  tvh_mutex_unlock(&hfe->hf_dvr_lock);

  if (!tr || !tr->hf_mmi)
    goto new_tune;

  mmi = tr->hf_mmi;

  int res = tvhdhomerun_frontend_lockkey_request(hfe, buf);
  if(res < 1) {
    tvherror(LS_TVHDHOMERUN, "%s - failed to acquire lockkey", buf);
    goto new_tune;
  } else {
    lockkey = 1;
  }

  uint32_t local_ip;
  int port;

  local_ip = get_local_ip(hfe->hf_hdhomerun_tuner);

  if (local_ip == 0) {
    tvherror(LS_TVHDHOMERUN, "%s - could not determine local machine IP address", buf);
    goto new_tune;
  }

  port = (config.local_port == 0) ? 0 : (config.local_port + hfe->hf_tunerNumber);

  udp = udp_bind(LS_TVHDHOMERUN, "hdhomerun", NULL, port, NULL,
                 NULL, BUFFER_SIZE, 0, 0);
  
  if (udp == NULL || udp == UDP_FATAL_ERROR) {
    tvherror(LS_TVHDHOMERUN, "%s - udp_bind failed", buf);
    goto new_tune;
  }

  if (fcntl(udp->fd, F_SETFL, O_NONBLOCK) != 0) {
    tvherror(LS_TVHDHOMERUN, "%s - failed to set socket nonblocking (%d)", buf, errno);
    goto new_tune;
  }

  if (port == 0) {
    struct sockaddr_in sin;
    socklen_t slen = sizeof(sin);
    if (getsockname(udp->fd, (struct sockaddr *)&sin, &slen) != 0) {
      tvherror(LS_TVHDHOMERUN, "%s - getsockname failed (%d)", buf, errno);
      goto new_tune;
    }
    port = ntohs(sin.sin_port);
  }

  snprintf(target, sizeof(target), "udp://%u.%u.%u.%u:%u",
    (unsigned int)(local_ip >> 24) & 0xFF,
    (unsigned int)(local_ip >> 16) & 0xFF,
    (unsigned int)(local_ip >>  8) & 0xFF,
    (unsigned int)(local_ip >>  0) & 0xFF,
    port);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  int r = hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, target);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if (r < 0) {
    tvherror(LS_TVHDHOMERUN, "%s - failed to set target, device %s communication error", buf, hfe->hf_device->hd_info.friendlyname);
    goto new_tune;
  }
  if (r == 0) {
    tvherror(LS_TVHDHOMERUN, "%s - failed to set target, device %s in use", buf, hfe->hf_device->hd_info.friendlyname);
    goto new_tune;
  }

  tvhpoll_add1(efd, udp->fd, TVHPOLL_IN, hfe);
  
  if (tvhdhomerun_frontend_tune(hfe, mmi)) {
    goto new_tune;
  }

  lockkey = 1;
  running = 1;
  last_activity = mclk();
  fatal_timeout = sec2mono(5 + MINMAX(hfe->hf_grace_period, 0, 60));

  while (tvheadend_is_running() && running) {

    tvhpoll_event_t ev;
    nfds = tvhpoll_wait(efd, &ev, 1, 250);

    if (last_activity + fatal_timeout < mclk()) {
      tvhwarn(LS_TVHDHOMERUN, "%s - no data received, restarting tune", buf);
      goto new_tune;
    }

    if (nfds <= 0)
      continue;

    if (ev.ptr == NULL) {
      uint8_t cmd;
      if (read(hfe->hf_dvr_pipe.rd, &cmd, 1) != 1)
        continue;

      if (cmd == 'e') {
        goto done;
      } else if (cmd == 'x') {
        running = 0;
        break;
      } else if (cmd == 's') {
        goto new_tune;
      } else if (cmd == 'c') { 
        tvhdhomerun_frontend_pid_changed(hfe, buf);
      }
      continue;
    }

    if (ev.ptr == hfe) {
      ssize_t r = tvhdhomerun_udp_read(&sb, udp, &um);
      if (r < 0) {
        if (ERRNO_AGAIN(errno))
          continue;
        if (errno == EOVERFLOW) {
          tvhwarn(LS_TVHDHOMERUN, "%s - recvmsg() EOVERFLOW", buf);
          continue;
        }
        tvherror(LS_TVHDHOMERUN, "%s - multirecv error %d (%s)", buf, errno, strerror(errno));
        goto new_tune;
      }

      last_activity = mclk();

      tvh_mutex_lock(&hfe->hf_dvr_lock);
      if (hfe->hf_req_thread == tr && tr->hf_mmi == mmi)
        mpegts_input_recv_packets(mmi, &sb, 0, NULL);
      else {
        tvh_mutex_unlock(&hfe->hf_dvr_lock);
        goto new_tune;
      }
      tvh_mutex_unlock(&hfe->hf_dvr_lock);
    }
  }

  goto new_tune;

done:
  tvhdhomerun_frontend_stop_streaming(hfe);
  if (udp) {
    tvhpoll_rem1(efd, udp->fd);
    udp_close(udp);
  }
  tvhpoll_rem1(efd, hfe->hf_dvr_pipe.rd);
  tvhdhomerun_frontend_request_cleanup(hfe, tr);
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  tvhtrace(LS_TVHDHOMERUN, "%s - final lockkey release", buf);
  sbuf_free(&sb);
  udp_multirecv_free(&um);
  tvhpoll_destroy(efd);
  return NULL;
}

static idnode_set_t *
tvhdhomerun_frontend_network_list ( mpegts_input_t *mi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;

  return dvb_network_list_by_fe_type(hfe->hf_type);
}

static void
tvhdhomerun_frontend_class_changed ( idnode_t *in )
{
  tvhdhomerun_device_t *la = ((tvhdhomerun_frontend_t*)in)->hf_device;
  tvhdhomerun_device_changed(la);
}

void
tvhdhomerun_frontend_save ( tvhdhomerun_frontend_t *hfe, htsmsg_t *fe )
{
  char id[16], ubuf[UUID_HEX_SIZE];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)hfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(hfe->hf_type));
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&hfe->ti_id, ubuf));

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(hfe->hf_type), hfe->hf_tunerNumber);
  htsmsg_add_msg(fe, id, m);
}


const idclass_t tvhdhomerun_frontend_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "tvhdhomerun_frontend",
  .ic_caption    = N_("HDHomeRun DVB frontend"),
  .ic_changed    = tvhdhomerun_frontend_class_changed,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_INT,
      .id       = "fe_number",
      .name     = N_("Frontend number"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(tvhdhomerun_frontend_t, hf_tunerNumber),
    },
    {
      .type     = PT_INT,
      .id       = "grace_period",
      .name     = N_("Grace period"),
      .desc     = N_("Force the grace period for which HDHomeRun client waits "
                     "for the data from server. After this grace period, "
                     "the tuner is handled as dead. The default value is "
                     "5 seconds."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(tvhdhomerun_frontend_t, hf_grace_period),
    },
    {}
  }
};

const idclass_t tvhdhomerun_frontend_dvbt_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_dvbt",
  .ic_caption    = N_("HDHomeRun DVB-T frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_dvbc_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_dvbc",
  .ic_caption    = N_("HDHomeRun DVB-C frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_atsc_t_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_atsc_t",
  .ic_caption    = N_("HDHomeRun ATSC-T frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_atsc_c_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_atsc_c",
  .ic_caption    = N_("HDHomeRun ATSC-C frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_cablecard_class =
{
  .ic_super = &tvhdhomerun_frontend_class,
  .ic_class = "tvhdhomerun_frontend_cablecard",
  .ic_caption = N_("HDHomeRun CableCARD frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_isdbt_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_isdbt",
  .ic_caption    = N_("HDHomeRun ISDB-T frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

static mpegts_network_t *
tvhdhomerun_frontend_wizard_network ( tvhdhomerun_frontend_t *hfe )
{
  return (mpegts_network_t *)LIST_FIRST(&hfe->mi_networks);
}

static htsmsg_t *
tvhdhomerun_frontend_wizard_get( tvh_input_t *ti, const char *lang )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)ti;
  mpegts_network_t *mn;
  const idclass_t *idc = NULL;

  mn = tvhdhomerun_frontend_wizard_network(hfe);
  if (mn == NULL || (mn && mn->mn_wizard))
    idc = dvb_network_class_by_fe_type(hfe->hf_type);
  return mpegts_network_wizard_get((mpegts_input_t *)hfe, idc, mn, lang);
}

static void
tvhdhomerun_frontend_wizard_set( tvh_input_t *ti, htsmsg_t *conf, const char *lang )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)ti;
  const char *ntype = htsmsg_get_str(conf, "mpegts_network_type");
  mpegts_network_t *mn;
  htsmsg_t *nlist;

  mn = tvhdhomerun_frontend_wizard_network(hfe);
  mpegts_network_wizard_create(ntype, &nlist, lang);
  if (nlist && ntype && (mn == NULL || mn->mn_wizard)) {
    htsmsg_add_str(nlist, NULL, ntype);
    mpegts_input_set_networks((mpegts_input_t *)hfe, nlist);
    htsmsg_destroy(nlist);
    if (tvhdhomerun_frontend_wizard_network(hfe))
      mpegts_input_set_enabled((mpegts_input_t *)hfe, 1);
    tvhdhomerun_device_changed(hfe->hf_device);
  } else {
    htsmsg_destroy(nlist);
  }
}

void
tvhdhomerun_frontend_delete ( tvhdhomerun_frontend_t *hfe )
{
  char buf1[256];

  lock_assert(&global_lock);

  hfe->mi_display_name((mpegts_input_t *)hfe, buf1, sizeof(buf1));

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)hfe);

  /* Stop thread */
  if (hfe->hf_dvr_pipe.wr > 0) {
    tvh_write(hfe->hf_dvr_pipe.wr, "e", 1);
    tvhtrace(LS_TVHDHOMERUN, "%s - waiting for control thread", buf1);
    pthread_join(hfe->hf_dvr_thread, NULL);
    tvh_pipe_close(&hfe->hf_dvr_pipe);
    tvhdebug(LS_TVHDHOMERUN, "%s - stopped control thread", buf1);
  }

  mtimer_disarm(&hfe->hf_monitor_timer);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  tvhtrace(LS_TVHDHOMERUN, "%s - frontend delete lockkey release", buf1);
  hdhomerun_device_destroy(hfe->hf_hdhomerun_tuner);

  /* Remove from adapter */
  TAILQ_REMOVE(&hfe->hf_device->hd_frontends, hfe, hf_link);

  tvh_mutex_destroy(&hfe->hf_dvr_lock);
  tvh_mutex_destroy(&hfe->hf_hdhomerun_device_mutex);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)hfe, 0);
}

tvhdhomerun_frontend_t *
tvhdhomerun_frontend_create(tvhdhomerun_device_t *hd, struct hdhomerun_discover_device_t *discover_info, htsmsg_t *conf, dvb_fe_type_t type, unsigned int frontend_number )
{
  const idclass_t *idc;
  const char *uuid = NULL;
  char id[16];
  tvhdhomerun_frontend_t *hfe;

  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%u", dvb_type2str(type), frontend_number);
  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  /* Class */
  if (type == DVB_TYPE_T)
    idc = &tvhdhomerun_frontend_dvbt_class;
  else if (type == DVB_TYPE_C)
    idc = &tvhdhomerun_frontend_dvbc_class;
  else if (type == DVB_TYPE_ATSC_T)
    idc = &tvhdhomerun_frontend_atsc_t_class;
  else if (type == DVB_TYPE_ATSC_C)
    idc = &tvhdhomerun_frontend_atsc_c_class;
  else if (type == DVB_TYPE_CABLECARD)
    idc = &tvhdhomerun_frontend_cablecard_class;
  else if (type == DVB_TYPE_ISDB_T)
    idc = &tvhdhomerun_frontend_isdbt_class;
  else {
    tvherror(LS_TVHDHOMERUN, "unknown FE type %d", type);
    return NULL;
  }

  hfe = calloc(1, sizeof(tvhdhomerun_frontend_t));
  hfe->hf_device   = hd;
  hfe->hf_type     = type;

  hfe->hf_hdhomerun_tuner = hdhomerun_device_create(discover_info->device_id, discover_info->ip_addr, frontend_number, hdhomerun_debug_obj);

  hfe->hf_tunerNumber = frontend_number;

  hfe = (tvhdhomerun_frontend_t*)mpegts_input_create0((mpegts_input_t*)hfe, idc, uuid, conf);
  if (!hfe) return NULL;

  /* Set some initial CableCARD settings */
  if (type == DVB_TYPE_CABLECARD) {
    hfe->mi_ota_epg = 0;
    hfe->mi_idlescan = 0;
    hfe->mi_remove_scrambled_bits = 1;
  }

  /* Callbacks */
  hfe->mi_get_weight   = tvhdhomerun_frontend_get_weight;
  hfe->mi_get_priority = tvhdhomerun_frontend_get_priority;
  hfe->mi_get_grace    = tvhdhomerun_frontend_get_grace;

  /* Default name */
  if (!hfe->mi_name ||
      (strncmp(hfe->mi_name, "HDHomeRun ", 7) == 0 &&
       strstr(hfe->mi_name, " Tuner ") &&
       strstr(hfe->mi_name, " #"))) {
    char lname[256];
    char ip[64];
    tcp_get_str_from_ip(&hd->hd_info.ip_address, ip, sizeof(ip));
    snprintf(lname, sizeof(lname), "HDHomeRun %s Tuner #%i (%s)",
             dvb_type2str(type), hfe->hf_tunerNumber, ip);
    free(hfe->mi_name);
    hfe->mi_name = strdup(lname);
  }

  /* Input callbacks */
  hfe->ti_wizard_get     = tvhdhomerun_frontend_wizard_get;
  hfe->ti_wizard_set     = tvhdhomerun_frontend_wizard_set;
  hfe->mi_is_enabled     = tvhdhomerun_frontend_is_enabled;
  hfe->mi_start_mux      = tvhdhomerun_frontend_start_mux;
  hfe->mi_stop_mux       = tvhdhomerun_frontend_stop_mux;
  hfe->mi_network_list   = tvhdhomerun_frontend_network_list;
  hfe->mi_update_pids    = tvhdhomerun_frontend_update_pids;
  hfe->mi_empty_status   = mpegts_input_empty_status;

  /* Adapter link */
  hfe->hf_device = hd;
  TAILQ_INSERT_TAIL(&hd->hd_frontends, hfe, hf_link);

  /* mutex init */
  tvh_mutex_init(&hfe->hf_hdhomerun_device_mutex, NULL);
  tvh_mutex_init(&hfe->hf_dvr_lock, NULL);

  tvh_pipe(O_NONBLOCK, &hfe->hf_dvr_pipe);
  tvh_thread_create(&hfe->hf_dvr_thread, NULL,
                    tvhdhomerun_frontend_input_thread, hfe, "hdhm-front");

  return hfe;
}
