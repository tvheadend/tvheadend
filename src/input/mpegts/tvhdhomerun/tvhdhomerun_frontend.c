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
#include "tcp.h"
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
  return 15;
}

static int
tvhdhomerun_frontend_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  return mpegts_input_is_enabled(mi, mm, flags, weight);
}

static void *
tvhdhomerun_frontend_input_thread ( void *aux )
{
  tvhdhomerun_frontend_t *hfe = aux;
  mpegts_mux_instance_t *mmi;
  sbuf_t sb;
  char buf[256];
  char target[64];
  uint32_t local_ip;
  int sockfd, nfds;
  int sock_opt = 1;
  int r;
  int rx_size = 1024 * 1024;
  struct sockaddr_in sock_addr;
  socklen_t sockaddr_len = sizeof(sock_addr);
  tvhpoll_event_t ev[2];
  tvhpoll_t *efd;

  tvhdebug(LS_TVHDHOMERUN, "starting input thread");

  /* Get MMI */
  tvh_mutex_lock(&hfe->hf_input_thread_mutex);
  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));
  mmi = LIST_FIRST(&hfe->mi_mux_active);
  tvh_cond_signal(&hfe->hf_input_thread_cond, 0);
  tvh_mutex_unlock(&hfe->hf_input_thread_mutex);
  if (mmi == NULL) return NULL;

  tvhdebug(LS_TVHDHOMERUN, "opening client socket");

  /* One would like to use libhdhomerun for the streaming details,
   * but that library uses threads on its own and the socket is put
   * into a ring buffer. That makes it less practical to use here,
   * so we do the whole UPD recv() stuff ourselves. And we can assume
   * POSIX here ;)
   */

  /* local IP */
  /* TODO: this is nasty */
  if (*config.local_ip == 0)
    local_ip = hdhomerun_device_get_local_machine_addr(hfe->hf_hdhomerun_tuner);
  else if (inet_pton(AF_INET, config.local_ip, &local_ip))
    local_ip = ntohl(local_ip);
  else
    tvherror(LS_TVHDHOMERUN, "failed to parse local IP (%s)", config.local_ip);

  /* first setup a local socket for the device to stream to */
  sockfd = tvh_socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1) {
    tvherror(LS_TVHDHOMERUN, "failed to open socket (%d)", errno);
    return NULL;
  }

  if(fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0) {
    close(sockfd);
    tvherror(LS_TVHDHOMERUN, "failed to set socket nonblocking (%d)", errno);
    return NULL;
  }

  /* enable broadcast */
  if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *) &sock_opt, sizeof(sock_opt)) < 0) {
    close(sockfd);
    tvherror(LS_TVHDHOMERUN, "failed to enable broadcast on socket (%d)", errno);
    return NULL;
  }

  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &sock_opt, sizeof(sock_opt)) < 0) {
    close(sockfd);
    tvherror(LS_TVHDHOMERUN, "failed to set address reuse on socket (%d)", errno);
    return NULL;
  }

  /* important: we need large rx buffers to accommodate the large amount of traffic */
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *) &rx_size, sizeof(rx_size)) < 0) {
    tvhwarn(LS_TVHDHOMERUN, "failed set socket rx buffer size, expect CC errors (%d)", errno);
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = config.local_port==0?0:htons(config.local_port + hfe->hf_tunerNumber);
  if(bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) != 0) {
    tvherror(LS_TVHDHOMERUN, "failed bind socket: %d", errno);
    close(sockfd);
    return NULL;
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  if(getsockname(sockfd, (struct sockaddr *) &sock_addr, &sockaddr_len) != 0) {
    tvherror(LS_TVHDHOMERUN, "failed to getsockname: %d", errno);
    close(sockfd);
    return NULL;
  }

  /* pretend we are smart and set video_socket; doubt that this is required though */
  //hfe->hf_hdhomerun_tuner->vs = sockfd;

  /* and tell the device to stream to the local port */
  memset(target, 0, sizeof(target));
  snprintf(target, sizeof(target), "udp://%u.%u.%u.%u:%u",
    (unsigned int)(local_ip >> 24) & 0xFF,
    (unsigned int)(local_ip >> 16) & 0xFF,
    (unsigned int)(local_ip >>  8) & 0xFF,
    (unsigned int)(local_ip >>  0) & 0xFF,
    ntohs(sock_addr.sin_port));
  tvhdebug(LS_TVHDHOMERUN, "setting target to: %s", target);
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  r = hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, target);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(r < 1) {
    tvherror(LS_TVHDHOMERUN, "failed to set target: %d", r);
    close(sockfd);
    return NULL;
  }

  /* the poll set includes the sockfd and the pipe for IPC */
  efd = tvhpoll_create(2);
  tvhpoll_event(ev+0, sockfd, TVHPOLL_IN, hfe);
  tvhpoll_event(ev+1, hfe->hf_input_thread_pipe.rd, TVHPOLL_IN,
                &hfe->hf_input_thread_pipe);

  r = tvhpoll_add(efd, ev, 2);
  if(r < 0)
    tvherror(LS_TVHDHOMERUN, "failed to setup poll");

  sbuf_init_fixed(&sb, (20000000 / 8));

  /* TODO: flush buffer? */

  while(tvheadend_is_running()) {
    nfds = tvhpoll_wait(efd, ev, 1, -1);

    if (nfds < 1) continue;
    if (ev[0].ptr != hfe) break;

    if((r = sbuf_read(&sb, sockfd)) < 0) {
      /* whoopsy */
      if(ERRNO_AGAIN(errno))
        continue;
      if(errno == EOVERFLOW) {
        tvhwarn(LS_TVHDHOMERUN, "%s - read() EOVERFLOW", buf);
        continue;
      }
      tvherror(LS_TVHDHOMERUN, "%s - read() error %d (%s)",
               buf, errno, strerror(errno));
      break;
    }

    //if(r != (7*188))
      //continue; /* dude; this is not a valid packet */

    //tvhdebug(LS_TVHDHOMERUN, "got r=%d (thats %d)", r, (r == 7*188));

    mpegts_input_recv_packets(mmi, &sb, 0, NULL);
  }

  tvhdebug(LS_TVHDHOMERUN, "setting target to none");
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, "none");
  hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);

  sbuf_free(&sb);
  tvhpoll_destroy(efd);
  close(sockfd);
  return NULL;
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
  int                      res, e;

  struct hdhomerun_tuner_status_t tuner_status;
  char *tuner_status_str;

  /* Stop timer */
  if (!mmi || !hfe->hf_ready) return;

  /* re-arm */
  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb,
                 hfe, sec2mono(1));

  /* Get current status */
  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_get_tuner_status(hfe->hf_hdhomerun_tuner, &tuner_status_str, &tuner_status);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 1)
    tvhwarn(LS_TVHDHOMERUN, "tuner_status (%d)", res);

  if(tuner_status.signal_present)
    hfe->hf_status = SIGNAL_GOOD;
  else
    hfe->hf_status = SIGNAL_NONE;

  /* Get current mux */
  mm = mmi->mmi_mux;

  /* wait for a signal_present */
  if(!hfe->hf_locked) {
    if(tuner_status.signal_present) {
      tvhdebug(LS_TVHDHOMERUN, "locked");
      hfe->hf_locked = 1;

      /* Get CableCARD variables */
      if (hfe->hf_type == DVB_TYPE_CABLECARD) {
        dvb_mux_t *lm = (dvb_mux_t *)mm;
        struct hdhomerun_tuner_vstatus_t tuner_vstatus;
        char *tuner_vstatus_str;
        tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
        res = hdhomerun_device_get_tuner_vstatus(hfe->hf_hdhomerun_tuner,
                                                 &tuner_vstatus_str, &tuner_vstatus);
        tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
        if (res < 1)
          tvhwarn(LS_TVHDHOMERUN, "tuner_vstatus (%d)", res);
        lm->lm_tuning.u.dmc_fe_cablecard.name = strdup(tuner_vstatus.name);
        sscanf(strstr(tuner_status.channel, ":"), ":%u", &lm->lm_tuning.dmc_fe_freq);
      }

      /* start input thread */
      tvh_pipe(O_NONBLOCK, &hfe->hf_input_thread_pipe);
      tvh_mutex_lock(&hfe->hf_input_thread_mutex);
      tvh_thread_create(&hfe->hf_input_thread, NULL, tvhdhomerun_frontend_input_thread, hfe, "hdhm-front");
      do {
        e = tvh_cond_wait(&hfe->hf_input_thread_cond, &hfe->hf_input_thread_mutex);
        if (e == ETIMEDOUT)
          break;
      } while (ERRNO_AGAIN(e));
      tvh_mutex_unlock(&hfe->hf_input_thread_mutex);

      /* install table handlers */
      psi_tables_install(mmi->mmi_input, mm,
                         ((dvb_mux_t *)mm)->lm_tuning.dmc_fe_delsys);

    } else { // quick re-arm the timer to wait for signal lock
      mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, ms2mono(50));
    }
  }

  tvh_mutex_lock(&mmi->tii_stats_mutex);

  if(tuner_status.signal_present) {
    /* TODO: totaly stupid conversion from 0-100 scale to 0-655.35 */
    mmi->tii_stats.snr = tuner_status.signal_to_noise_quality * 655.35;
    mmi->tii_stats.signal = tuner_status.signal_strength * 655.35;
  } else {
    mmi->tii_stats.snr = 0;
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
}

static int tvhdhomerun_frontend_tune(tvhdhomerun_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
  hfe->hf_status          = SIGNAL_NONE;
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  char channel_buf[64];
  uint32_t symbol_rate = 0;
  uint8_t bandwidth = 0;
  int res;
  char *perror;

  /* resolve the modulation type */
  switch (dmc->dmc_fe_type) {
    case DVB_TYPE_C:
      /* the symbol rate */
      symbol_rate = dmc->u.dmc_fe_qam.symbol_rate / 1000;
      switch(dmc->dmc_fe_modulation) {
        case DVB_MOD_QAM_64:
          snprintf(channel_buf, sizeof(channel_buf), "a8qam64-%d:%u", symbol_rate, dmc->dmc_fe_freq);
          break;
        case DVB_MOD_QAM_256:
          snprintf(channel_buf, sizeof(channel_buf), "a8qam256-%d:%u", symbol_rate, dmc->dmc_fe_freq);
          break;
        default:
          snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
          break;
      }
      break;
    case DVB_TYPE_T:
      bandwidth = dmc->u.dmc_fe_ofdm.bandwidth / 1000UL;
      switch (dmc->dmc_fe_modulation) {
        case DVB_MOD_AUTO:
            if (dmc->u.dmc_fe_ofdm.bandwidth == DVB_BANDWIDTH_AUTO) {
                snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
            } else {
                snprintf(channel_buf, sizeof(channel_buf), "auto%dt:%u", bandwidth, dmc->dmc_fe_freq);
            }
            break;
        case DVB_MOD_QAM_256:
            if (dmc->dmc_fe_delsys == DVB_SYS_DVBT2) {
                snprintf(channel_buf, sizeof(channel_buf), "tt%dqam256:%u", bandwidth, dmc->dmc_fe_freq);
            } else {
                snprintf(channel_buf, sizeof(channel_buf), "t%dqam256:%u", bandwidth, dmc->dmc_fe_freq);
            }
            break;
        case DVB_MOD_QAM_64:
            if (dmc->dmc_fe_delsys == DVB_SYS_DVBT2) {
                snprintf(channel_buf, sizeof(channel_buf), "tt%dqam64:%u", bandwidth, dmc->dmc_fe_freq);
            } else {
                snprintf(channel_buf, sizeof(channel_buf), "t%dqam64:%u", bandwidth, dmc->dmc_fe_freq);
            }
            break;
        default:
            /* probably won't work but never mind */
            snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
            break;
      }
      break;
    case DVB_TYPE_CABLECARD:
      snprintf(channel_buf, sizeof(channel_buf), "%u", dmc->u.dmc_fe_cablecard.vchannel);
      break;
    case DVB_TYPE_ATSC_T:
      switch(dmc->dmc_fe_modulation) {
        case DVB_MOD_VSB_8:
          snprintf(channel_buf, sizeof(channel_buf), "auto6t:%u", dmc->dmc_fe_freq);
          break;
        default:
          snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
          break;
      }
      break;
    default:
      snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
      break;
  }

  tvhinfo(LS_TVHDHOMERUN, "tuning to %s", channel_buf);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  
  res = hdhomerun_device_tuner_lockkey_request(hfe->hf_hdhomerun_tuner, &perror);
  if(res < 1) {
    tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    tvherror(LS_TVHDHOMERUN, "failed to acquire lockkey: %s", perror);
    return SM_CODE_TUNING_FAILED;
  }
  
  if (hfe->hf_type == DVB_TYPE_CABLECARD)
    res = hdhomerun_device_set_tuner_vchannel(hfe->hf_hdhomerun_tuner, channel_buf);
  else
    res = hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, channel_buf);

  if(res < 1) {
    hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
    tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    tvherror(LS_TVHDHOMERUN, "failed to tune to %s", channel_buf);
    return SM_CODE_TUNING_FAILED;
  }

  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  
  hfe->hf_status = SIGNAL_NONE;

  /* start the monitoring */
  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, ms2mono(50));
  hfe->hf_ready = 1;

  return 0;
}

static int
tvhdhomerun_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  int res, r;
  char buf1[256], buf2[256];

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug(LS_TVHDHOMERUN, "%s - starting %s", buf1, buf2);

  /* tune to the right mux */
  res = tvhdhomerun_frontend_tune(hfe, mmi);

  /* reset the pfilters */
  if (hfe->hf_type != DVB_TYPE_CABLECARD) {
    tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
    r = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, "0x0000");
    tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    if(r < 1)
      tvherror(LS_TVHDHOMERUN, "failed to reset pfilter: %d", r);
  }

  return res;
}

static void
tvhdhomerun_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  char buf1[256], buf2[256];

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug(LS_TVHDHOMERUN, "%s - stopping %s", buf1, buf2);

  /* join input thread */
  if(hfe->hf_input_thread_pipe.wr > 0) {
    tvh_write(hfe->hf_input_thread_pipe.wr, "", 1); // wake input thread
    tvhtrace(LS_TVHDHOMERUN, "%s - waiting for input thread", buf1);
    pthread_join(hfe->hf_input_thread, NULL);
    tvh_pipe_close(&hfe->hf_input_thread_pipe);
    tvhtrace(LS_TVHDHOMERUN, "%s - input thread stopped", buf1);
  }

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, "none");
  hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);

  hfe->hf_locked = 0;
  hfe->hf_status = 0;
  hfe->hf_ready = 0;

  mtimer_arm_rel(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, sec2mono(2));
}

static void tvhdhomerun_frontend_update_pids_appendPidRange(int a, int b, int *firstDelimiter, char **pBuffer, const char **endBuffer )
 /* A helper function that writes a range of pids to the 'buffer'.  This function is called more than once from tvhdhomerun_frontend_update_pids. */
{
  if(*firstDelimiter) /* Don't bother printing a space before the first range of pids. */
     *firstDelimiter = 0;  /* Set this to false the first time. */
  else {
    if(*pBuffer < *endBuffer) /* Check if 'buffer' is full. */
      *pBuffer += snprintf(*pBuffer, *endBuffer-*pBuffer, " "); /* After the first range, separate pid ranges by a space. */
  }
  if(*pBuffer < *endBuffer) { /* Check if 'buffer' is full. */
    if(a == b)
      *pBuffer += snprintf(*pBuffer, *endBuffer-*pBuffer, "0x%04x", a); /* First and last pid in a range are the same, then that one pid is appended. */
    else
      *pBuffer += snprintf(*pBuffer, *endBuffer-*pBuffer, "0x%04x-0x%04x", a, b); /* Append a range of pids to 'buffer'. */
  }
}

static void tvhdhomerun_frontend_update_pids( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  mpegts_apids_t pids, wpids;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;
  int i;
  int res;

  /* The length of the buffer for list of hdhomerun formatted list of pids. MUST BE GREATER THAN 14! */
  const unsigned int bufferSize = 1024;

  /* Temporary storage for preparing the list of pid ranges to send to the hdhomerun device by the set filter command.
   * (hdhomerun's filter format example: "0x0000-0x0001 0x0030-0x0031 0x0034-0x0036 0x00a0-0x00a1 0x00a4-0x00a5 0x1ffb") */
  char buffer[bufferSize];

  mpegts_pid_init(&pids);
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    if (mp->mp_pid == MPEGTS_FULLMUX_PID)
      pids.all = 1;
    else if (mp->mp_pid < MPEGTS_FULLMUX_PID) {
      RB_FOREACH(mps, &mp->mp_subs, mps_link)
        mpegts_pid_add(&pids, mp->mp_pid, mps->mps_weight);
    }
  }

  mpegts_pid_weighted(&wpids, &pids, 128 /* max? */, MPS_WEIGHT_ALLLIMIT);
  if (hfe->hf_type == DVB_TYPE_CABLECARD)
    return;

  buffer[0] = '\0'; /* Initialize buffer to handle the case where no pids are requested. */

  /* Check if the turn-on-all-pids flag is set.  If so, ask the hdhomerun to send all pids ("0x0000-0x1fff").  */
  if(wpids.all) {
      tvhdebug(LS_TVHDHOMERUN, "setting PID filter full mux");
      snprintf(buffer, bufferSize, "0x0000-0x1fff");
  }
  /* Otherwise, if we have any pids, proceed to find consecutive runs of pids to group them into a list of ranges. */
  else {
    if(wpids.count > 0) {

      /* 'begin' is the first pid in a consecutive run of pids.
       * 'prev' is the previous pid while walking the list of pids and
       * 'curr' is the current pid. */
      int begin, prev, curr;

      const char *endBuffer = buffer + bufferSize; /* Have the address after the end of buffer on hand to help avoid writing past the buffer. */
      char *pBuffer = buffer; /* Move this pointer through the buffer as we write formatted pids. */
      int firstDelimiter = -1; /* Set this to 'true' so that we can skip writing the first delimiter/space. */

      /* Walk the list of pids and keep track of runs of consecutive pids. Setup the state for the first pid. */
      for (i = 1, prev = begin = wpids.pids[0].pid; i < wpids.count; i++) {

        curr = wpids.pids[i].pid;
        /* make sure the pid maps to a max of 0x1FFF, API will reject the call otherwise */
        if(curr > 0x1FFF) {
          tvherror(LS_TVHDHOMERUN, "pid %d is too large, masking to API maximum of 0x1FFF", curr);
          curr = (curr & 0x1FFF);
        }

        /* Check to see if there is a break in a run of consecutive pids. */
        if(prev + 1 != curr) {
          /* If the current pid is NOT +1 more than the previous pid, then this is the end of a range of pids.
           * Write out this range of consecutive pids. */
          tvhdhomerun_frontend_update_pids_appendPidRange(begin, prev, &firstDelimiter, &pBuffer, &endBuffer);

          /* Also, this is the start of a new range of pids.  Set 'begin' to the beginning of the next range. */
          begin = curr;
        }
        prev = curr;  /* At bottom of the loop, current pid is now the previous pid. */
        if(pBuffer >= endBuffer)  /* We have reached the end of the 'buffer', no need to continue walking through the list. */
          break;
      }
      /* We are at the end of the list of pids, write the final range of consecutive pids to the 'buffer'. */
      tvhdhomerun_frontend_update_pids_appendPidRange(begin, prev, &firstDelimiter, &pBuffer, &endBuffer);

      if(pBuffer >= endBuffer) { /* We could not fit the list of ranges of pids into the 'buffer' and have an incomplete/mangled 'buffer'
                                * so as a backup, we will request all pids except NULL packets (0x1fff). */
        tvhdebug(LS_TVHDHOMERUN, "pfilter list is too big for buffer[%d] = \"%s\"(truncated)", bufferSize, buffer);
        snprintf(buffer, bufferSize, "0x0000-0x1ffe");
      }
    }
  }
  tvhdebug(LS_TVHDHOMERUN, "setting pfilter to: %s", buffer);

  tvh_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  /* Send the specially formatted list of pid ranges to the hdhomerun device. */
  res = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, buffer);
  tvh_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 1)
    tvherror(LS_TVHDHOMERUN, "failed to set_tuner_filter: %d", res);

  mpegts_pid_done(&wpids);
  mpegts_pid_done(&pids);
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
  if (ntype && (mn == NULL || mn->mn_wizard)) {
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
  lock_assert(&global_lock);

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)hfe);

  mtimer_disarm(&hfe->hf_monitor_timer);

  hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  hdhomerun_device_destroy(hfe->hf_hdhomerun_tuner);

  /* Remove from adapter */
  TAILQ_REMOVE(&hfe->hf_device->hd_frontends, hfe, hf_link);

  tvh_mutex_destroy(&hfe->hf_input_thread_mutex);
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

  hfe->hf_input_thread_running = 0;
  hfe->hf_input_thread_terminating = 0;

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
  tvh_mutex_init(&hfe->hf_input_thread_mutex, NULL);
  tvh_cond_init(&hfe->hf_input_thread_cond, 1);

  return hfe;
}
