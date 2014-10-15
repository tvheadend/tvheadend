
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

#include <fcntl.h>
#include "tvheadend.h"
#include "tvhpoll.h"
#include "streaming.h"
#include "tvhdhomerun_private.h"

static void tvhdhomerun_device_open_pid(tvhdhomerun_frontend_t *hfe, mpegts_pid_t *mp);

static mpegts_pid_t * tvhdhomerun_frontend_open_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner );

static tvhdhomerun_frontend_t *
tvhdhomerun_frontend_find_by_number( tvhdhomerun_device_t *hd, int num )
{
  tvhdhomerun_frontend_t *hfe;

  TAILQ_FOREACH(hfe, &hd->hd_frontends, hf_link)
    if (hfe->hf_tunerNumber == num)
      return hfe;
  return NULL;
}

static int
tvhdhomerun_frontend_is_free ( mpegts_input_t *mi )
{
  return mpegts_input_is_free(mi);
}

static int
tvhdhomerun_frontend_get_weight ( mpegts_input_t *mi, int flags )
{
  return mpegts_input_get_weight(mi, flags);
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
tvhdhomerun_frontend_is_enabled ( mpegts_input_t *mi, mpegts_mux_t *mm,
								  const char *reason )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  if (!hfe->mi_enabled) return 0;
  return 1;
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

  tvhdebug("tvhdhomerun", "starting input thread");  

  /* Get MMI */
  pthread_mutex_lock(&hfe->hf_input_thread_mutex);
  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));
  mmi = LIST_FIRST(&hfe->mi_mux_active);
  pthread_cond_signal(&hfe->hf_input_thread_cond);
  pthread_mutex_unlock(&hfe->hf_input_thread_mutex);
  if (mmi == NULL) return NULL;

  tvhdebug("tvhdhomerun", "opening client socket");  

  /* One would like to use libhdhomerun for the streaming details,
   * but that library uses threads on its own and the socket is put
   * into a ring buffer. That makes it less practical to use here,
   * so we do the whole UPD recv() stuff ourselves. And we can assume
   * POSIX here ;)
   */

  /* local IP */
  /* TODO: this is nasty */
  local_ip = hdhomerun_device_get_local_machine_addr(hfe->hf_hdhomerun_tuner);

  /* first setup a local socket for the device to stream to */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd == -1) {
    tvherror("stvhdhomerun", "failed to open socket (%d)", errno);
    return NULL;
  }

  if(fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0) {
    close(sockfd);
    tvherror("stvhdhomerun", "failed to set socket nonblocking (%d)", errno);
    return NULL;
  }

  /* enable broadcast */
  if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *) &sock_opt, sizeof(sock_opt)) < 0) {
    close(sockfd);
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to enable broadcast on socket (%d)", errno);
    return NULL;
  }

  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &sock_opt, sizeof(sock_opt)) < 0) {
    close(sockfd);
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to set address reuse on socket (%d)", errno);
    return NULL;
  }

  /* important: we need large rx buffers to accomodate the large amount of traffic */
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *) &rx_size, sizeof(rx_size)) < 0) {
    tvhlog(LOG_WARNING, "tvhdhomerun", "failed set socket rx buffer size, expect CC errors (%d)", errno);
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_addr.sin_port = 0;
  if(bind(sockfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) != 0) {
    tvhlog(LOG_ERR, "tvhdhomerun", "failed bind socket: %d", errno);
    close(sockfd);
    return NULL;
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  if(getsockname(sockfd, (struct sockaddr *) &sock_addr, &sockaddr_len) != 0) {
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to getsockname: %d", errno);
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
  tvhdebug("tvhdhomerun", "setting target to: %s", target);  
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  r = hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, target);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(r < 1) {
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to set target: %d", r);
    return NULL;
  }

  /* the poll set includes the sockfd and the pipe for IPC */
  efd = tvhpoll_create(2);
  memset(ev, 0, sizeof(ev));
  ev[0].events             = TVHPOLL_IN;
  ev[0].fd = ev[0].data.fd = sockfd;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd = ev[1].data.fd = hfe->hf_input_thread_pipe.rd;

  r = tvhpoll_add(efd, ev, 2);
  if(r < 0)
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to setup poll");

  sbuf_init_fixed(&sb, (20000000 / 8));

  /* TODO: flush buffer? */

  while(tvheadend_running) {
    nfds = tvhpoll_wait(efd, ev, 1, -1);

    if (nfds < 1) continue;
    if (ev[0].data.fd != sockfd) break;

    if((r = sbuf_read(&sb, sockfd)) < 0) {
      /* whoopsy */
      if(ERRNO_AGAIN(errno))
        continue;
      if(errno == EOVERFLOW) {
        tvhlog(LOG_WARNING, "tvhdhomerun", "%s - read() EOVERFLOW", buf);
        continue;
      }
      tvhlog(LOG_ERR, "tvhdhomerun", "%s - read() error %d (%s)",
             buf, errno, strerror(errno));
      break;
    }

    //if(r != (7*188))
      //continue; /* dude; this is not a valid packet */

    //tvhdebug("tvhdhomerun", "got r=%d (thats %d)", r, (r == 7*188));

    mpegts_input_recv_packets((mpegts_input_t*) hfe, mmi, &sb, NULL, NULL);
  }

  tvhdebug("tvhdhomerun", "setting target to none");
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  hdhomerun_device_set_tuner_target(hfe->hf_hdhomerun_tuner, "none");
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);

  sbuf_free(&sb);
  tvhpoll_destroy(efd);
  close(sockfd);
  return NULL;
}

static void tvhdhomerun_frontend_default_tables( tvhdhomerun_frontend_t *hfe, dvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)lm;

  psi_tables_default(mm);

  if (hfe->hf_type == DVB_TYPE_ATSC) {
    if (lm->lm_tuning.dmc_fe_modulation == DVB_MOD_VSB_8)
      psi_tables_atsc_t(mm);
    else
      psi_tables_atsc_c(mm);

  } else {
    psi_tables_dvb(mm);
  }
}

static void
tvhdhomerun_frontend_monitor_cb( void *aux )
{
  tvhdhomerun_frontend_t  *hfe = aux;
  mpegts_mux_instance_t   *mmi = LIST_FIRST(&hfe->mi_mux_active);
  mpegts_mux_t            *mm;
  mpegts_pid_t            *mp;
  streaming_message_t      sm;
  signal_status_t          sigstat;
  service_t               *svc;
  int                      res;

  struct hdhomerun_tuner_status_t tuner_status;
  char *tuner_status_str;

  /* Stop timer */
  if (!mmi || !hfe->hf_ready) return;

  /* re-arm */
  gtimer_arm(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);

  /* Get current status */
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_get_tuner_status(hfe->hf_hdhomerun_tuner, &tuner_status_str, &tuner_status);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 1)
    tvhwarn("tvhdhomerun", "tuner_status (%d): %s", res, tuner_status_str);

  if(tuner_status.signal_present)
    hfe->hf_status = SIGNAL_GOOD;
  else
    hfe->hf_status = SIGNAL_NONE;
    
  /* Get current mux */
  mm = mmi->mmi_mux;

  /* wait for a signal_present */
  if(!hfe->hf_locked) {
    if(tuner_status.signal_present) {
      tvhdebug("tvhdhomerun", "locked");
      hfe->hf_locked = 1;

      /* start input thread */
      tvh_pipe(O_NONBLOCK, &hfe->hf_input_thread_pipe);
      pthread_mutex_lock(&hfe->hf_input_thread_mutex);
      tvhthread_create(&hfe->hf_input_thread, NULL, tvhdhomerun_frontend_input_thread, hfe);
      pthread_cond_wait(&hfe->hf_input_thread_cond, &hfe->hf_input_thread_mutex);
      pthread_mutex_unlock(&hfe->hf_input_thread_mutex);

      /* install table handlers */
      tvhdhomerun_frontend_default_tables(hfe, (dvb_mux_t*)mm);
      /* open PIDs */
      pthread_mutex_lock(&hfe->mi_output_lock);
      RB_FOREACH(mp, &mm->mm_pids, mp_link)
        tvhdhomerun_device_open_pid(hfe, mp);
      pthread_mutex_unlock(&hfe->mi_output_lock);
    } else { // quick re-arm the timer to wait for signal lock
      gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 50);
    }
  }

  if(tuner_status.signal_present) {
    /* TODO: totaly stupid conversion from 0-100 scale to 0-655.35 */
    mmi->mmi_stats.snr = tuner_status.signal_to_noise_quality * 655.35;
    mmi->mmi_stats.signal = tuner_status.signal_strength * 655.35;
  } else {
    mmi->mmi_stats.snr = 0;
  }

  sigstat.status_text  = signal2str(hfe->hf_status);
  sigstat.snr          = mmi->mmi_stats.snr;
  sigstat.snr_scale    = mmi->mmi_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
  sigstat.signal       = mmi->mmi_stats.signal;
  sigstat.signal_scale = mmi->mmi_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
  sigstat.ber          = mmi->mmi_stats.ber;
  sigstat.unc          = mmi->mmi_stats.unc;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;

  LIST_FOREACH(svc, &hfe->mi_transports, s_active_link) {
    pthread_mutex_lock(&svc->s_stream_mutex);
    streaming_pad_deliver(&svc->s_streaming_pad, &sm);
    pthread_mutex_unlock(&svc->s_stream_mutex);
  }
}

static void tvhdhomerun_device_open_pid(tvhdhomerun_frontend_t *hfe, mpegts_pid_t *mp) {
  char *pfilter;
  char buf[1024];
  int res;

  //tvhdebug("tvhdhomerun", "adding PID 0x%x to pfilter", mp->mp_pid);

  /* get the current filter */
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_get_tuner_filter(hfe->hf_hdhomerun_tuner, &pfilter);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 1) {
      tvhlog(LOG_ERR, "tvhdhomerun", "failed to get_tuner_filter: %d", res);
      return;
  }

  tvhdebug("tvhdhomerun", "current pfilter: %s", pfilter);

  memset(buf, 0x00, sizeof(buf));
  snprintf(buf, sizeof(buf), "0x%04x", mp->mp_pid);

  if(strncmp(pfilter, buf, strlen(buf)) != 0) {
    memset(buf, 0x00, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s 0x%04x", pfilter, mp->mp_pid);
    tvhdebug("tvhdhomerun", "setting pfilter to: %s", buf);

    pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
    res = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, buf);
    pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
    if(res < 1)
      tvhlog(LOG_ERR, "tvhdhomerun", "failed to set_tuner_filter: %d", res);
  } else {
    //tvhdebug("tvhdhomerun", "pid 0x%x already present in pfilter", mp->mp_pid);
    return;
  }
}

static int tvhdhomerun_frontend_tune(tvhdhomerun_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
  hfe->hf_status          = SIGNAL_NONE;
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  char channel_buf[64];
  int res;

  snprintf(channel_buf, sizeof(channel_buf), "auto:%u", dmc->dmc_fe_freq);
  tvhlog(LOG_INFO, "tvhdhomerun", "tuning to %s", channel_buf);
  
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  res = hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, channel_buf);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(res < 1) {
      tvhlog(LOG_ERR, "tvhdhomerun", "failed to tune to %s", channel_buf);
      return SM_CODE_TUNING_FAILED;
  }
  
  hfe->hf_status = SIGNAL_NONE;

  /* start the monitoring */
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 50);
  hfe->hf_ready = 1;

  return 0;
}

static int
tvhdhomerun_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  int res, r;
  char buf1[256], buf2[256];
  
  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("tvhdhomerun", "%s - starting %s", buf1, buf2);

  /* tune to the right mux */
  res = tvhdhomerun_frontend_tune(hfe, mmi);

  /* reset the pfilters */
  pthread_mutex_lock(&hfe->hf_hdhomerun_device_mutex);
  r = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, "0x0000");
  pthread_mutex_unlock(&hfe->hf_hdhomerun_device_mutex);
  if(r < 1)
    tvhlog(LOG_ERR, "tvhdhomerun", "failed to reset pfilter: %d", r);

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
  tvhdebug("tvhdhomerun", "%s - stopping %s", buf1, buf2);

  /* join input thread */
  if(hfe->hf_input_thread_pipe.wr > 0) {
    tvh_write(hfe->hf_input_thread_pipe.wr, "", 1); // wake input thread
    tvhtrace("tvhdhomerun", "%s - waiting for input thread", buf1);
    pthread_join(hfe->hf_input_thread, NULL);
    tvh_pipe_close(&hfe->hf_input_thread_pipe);
    tvhtrace("tvhdhomerun", "%s - input thread stopped", buf1);
  }

  hfe->hf_locked = 0;
  hfe->hf_status = 0;
  hfe->hf_ready = 0;

  gtimer_arm(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 2);
}

static mpegts_pid_t *tvhdhomerun_frontend_open_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  mpegts_pid_t *mp;

  //tvhdebug("tvhdhomerun", "Open pid 0x%x\n", pid);

  if (!(mp = mpegts_input_open_pid(mi, mm, pid, type, owner))) {
    tvhdebug("tvhdhomerun", "Failed to open pid %d", pid);
    return NULL;
  }

  tvhdhomerun_device_open_pid(hfe, mp);

  return mp;
}

static void tvhdhomerun_frontend_close_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  //tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;

  tvhdebug("tvhdhomerun", "closing pid 0x%x",pid);

  mpegts_input_close_pid(mi, mm, pid, type, owner);

  //tvhdhomerun_device_update_pid_filter(hfe, mm);
}

static idnode_set_t *
tvhdhomerun_frontend_network_list ( mpegts_input_t *mi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  const idclass_t     *idc;

  if (hfe->hf_type == DVB_TYPE_T)
    idc = &dvb_network_dvbt_class;
  else if (hfe->hf_type == DVB_TYPE_C)
    idc = &dvb_network_dvbc_class;
  else if (hfe->hf_type == DVB_TYPE_ATSC)
  	idc = &dvb_network_atsc_class;
  else
    return NULL;

  return idnode_find_all(idc, NULL);
}

static void
tvhdhomerun_frontend_class_save ( idnode_t *in )
{
  tvhdhomerun_device_t *la = ((tvhdhomerun_frontend_t*)in)->hf_device;
  tvhdhomerun_device_save(la);
}

void
tvhdhomerun_frontend_save ( tvhdhomerun_frontend_t *hfe, htsmsg_t *fe )
{
  char id[16];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)hfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(hfe->hf_type));

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(hfe->hf_type), hfe->hf_tunerNumber);
  htsmsg_add_msg(fe, id, m);
  if (hfe->hf_master) {
    snprintf(id, sizeof(id), "master for #%d", hfe->hf_tunerNumber);
    htsmsg_add_u32(fe, id, hfe->hf_master);
  }
}


const idclass_t tvhdhomerun_frontend_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "tvhdhomerun_frontend",
  .ic_caption    = "HDHomeRun DVB Frontend",
  .ic_save       = tvhdhomerun_frontend_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_INT,
      .id       = "fe_number",
      .name     = "Frontend Number",
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
  .ic_caption    = "HDHomeRun DVB-T Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_dvbc_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_dvbc",
  .ic_caption    = "HDHomeRun DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t tvhdhomerun_frontend_atsc_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_atsc",
  .ic_caption    = "HDHomeRun ATSC Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

void
tvhdhomerun_frontend_delete ( tvhdhomerun_frontend_t *hfe )
{
  lock_assert(&global_lock);

  gtimer_disarm(&hfe->hf_monitor_timer);

  // hdhomerun_device_tuner_lockkey_release(hfe->hf_hdhomerun_tuner);
  hdhomerun_device_destroy(hfe->hf_hdhomerun_tuner);

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)hfe);

  /* Remove from adapter */
  TAILQ_REMOVE(&hfe->hf_device->hd_frontends, hfe, hf_link);

  pthread_mutex_destroy(&hfe->hf_input_thread_mutex);
  pthread_mutex_destroy(&hfe->hf_hdhomerun_device_mutex);
  
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
  uint32_t master = 0;

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
  else if (type == DVB_TYPE_ATSC) {
    idc = &tvhdhomerun_frontend_atsc_class;
  } else {
    tvherror("stvhdhomerun", "unknown FE type %d", type);
    return NULL;
  }

  hfe = calloc(1, sizeof(tvhdhomerun_frontend_t));
  hfe->hf_device   = hd;
  hfe->hf_type     = type;
  hfe->hf_master   = master;

  hfe->hf_hdhomerun_tuner = hdhomerun_device_create(discover_info->device_id, discover_info->ip_addr, frontend_number, hdhomerun_debug_obj);

  hfe->hf_input_thread_running = 0;
  hfe->hf_input_thread_terminating = 0;

  hfe->hf_tunerNumber = frontend_number;

  hfe = (tvhdhomerun_frontend_t*)mpegts_input_create0((mpegts_input_t*)hfe, idc, uuid, conf);
  if (!hfe) return NULL;

  /* Callbacks */
  hfe->mi_is_free      = tvhdhomerun_frontend_is_free;
  hfe->mi_get_weight   = tvhdhomerun_frontend_get_weight;
  hfe->mi_get_priority = tvhdhomerun_frontend_get_priority;
  hfe->mi_get_grace    = tvhdhomerun_frontend_get_grace;

  /* Default name */
  if (!hfe->mi_name ||
      (strncmp(hfe->mi_name, "HDHomeRun ", 7) == 0 &&
       strstr(hfe->mi_name, " Tuner ") &&
       strstr(hfe->mi_name, " #"))) { 
    char lname[256];
    snprintf(lname, sizeof(lname), "HDHomeRun %s Tuner #%i (%s)",
             dvb_type2str(type), hfe->hf_tunerNumber, hd->hd_info.ip_address);
    free(hfe->mi_name);
    hfe->mi_name = strdup(lname);
  }

  /* Input callbacks */
  hfe->mi_is_enabled     = tvhdhomerun_frontend_is_enabled;
  hfe->mi_start_mux      = tvhdhomerun_frontend_start_mux;
  hfe->mi_stop_mux       = tvhdhomerun_frontend_stop_mux;
  hfe->mi_network_list   = tvhdhomerun_frontend_network_list;
  hfe->mi_open_pid       = tvhdhomerun_frontend_open_pid;
  hfe->mi_close_pid      = tvhdhomerun_frontend_close_pid;

  /* Adapter link */
  hfe->hf_device = hd;
  TAILQ_INSERT_TAIL(&hd->hd_frontends, hfe, hf_link);

  /* Slave networks update */
  if (master) {
    tvhdhomerun_frontend_t *hfe2 = tvhdhomerun_frontend_find_by_number(hfe->hf_device, master);
    if (hfe2) {
      htsmsg_t *l = (htsmsg_t *)mpegts_input_class_network_get(hfe2);
      if (l) {
        mpegts_input_class_network_set(hfe, l);
        htsmsg_destroy(l);
      }
    }
  }

  /* mutex init */
  pthread_mutex_init(&hfe->hf_hdhomerun_device_mutex, NULL);
  pthread_mutex_init(&hfe->hf_input_thread_mutex, NULL);
  pthread_cond_init(&hfe->hf_input_thread_cond, NULL);

  return hfe;
}
