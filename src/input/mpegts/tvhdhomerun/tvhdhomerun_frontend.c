
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


static void tvhdhomerun_device_update_pid_filter(tvhdhomerun_frontend_t *hfe, mpegts_mux_t *mm);

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
tvhdhomerun_frontend_get_weight ( mpegts_input_t *mi )
{
  return mpegts_input_get_weight(mi);
}

static int
tvhdhomerun_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  return mpegts_input_get_priority(mi, mm);
}

static int
tvhdhomerun_frontend_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  return 15;
}

static int
tvhdhomerun_frontend_is_enabled ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  if (!hfe->mi_enabled) return 0;
  return 1;
}


static void *
tvhdhomerun_frontend_input_thread ( void *aux )
{
  tvhdhomerun_frontend_t *hfe = aux;
  mpegts_mux_instance_t *mmi = hfe->hf_mmi;
  sbuf_t sb;
  char buf[256];

  tvhdebug("tvhdhomerun", "Starting input-thread.\n");  

  /* Get MMI */
  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));

  PTHREAD_MUTEX_LOCK(&hfe->hf_input_mux_lock);

  if (mmi == NULL) {
    tvhlog(LOG_ERR, "tvhdhomerun","mmi == 0");
    hfe->hf_input_thread_running = 0;
    PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);
    return NULL;
  }
  PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);

  int r = hdhomerun_device_stream_start(hfe->hf_hdhomerun_tuner);
  
  if ( r == 0 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Failed to start stream from HDHomeRun device! (Command rejected!)");
    return NULL;
  } else if ( r == -1 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Failed to start stream from HDHomeRun device! (Communication error!)");
    return NULL;
  } else if ( r != 1 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "UNKNOWN ERROR(%d) %s:%s:%u",r ,__FILE__,__FUNCTION__,__LINE__);
  }

  sbuf_init_fixed(&sb,VIDEO_DATA_BUFFER_SIZE_1S); // Buffersize in libhdhomerun.

  while (tvheadend_running && hfe->hf_input_thread_terminating == 0 ) {
    PTHREAD_MUTEX_LOCK(&hfe->hf_input_mux_lock);
    if ( tvheadend_running && hfe->hf_input_thread_terminating == 0 ) {
      const size_t maxRead = sb.sb_size-sb.sb_ptr; // Available space in sbuf.
      size_t readDataSize = 0;
   
      // TODO: Rewrite input-thread to read the udp stream straight of instead of the wrapper-functions
      // of libhdhomerun.
      uint8_t *readData = hdhomerun_device_stream_recv(hfe->hf_hdhomerun_tuner, maxRead, &readDataSize);
      if ( readDataSize > 0 ) {
        sbuf_append(&sb, readData, readDataSize);
      }
      PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);
      if ( readDataSize > 0 ) {
        mpegts_input_recv_packets((mpegts_input_t*)hfe, mmi, &sb, NULL, NULL);
      }
      usleep(125000);
    }
  }

  hdhomerun_device_stream_stop(hfe->hf_hdhomerun_tuner);

  sbuf_free(&sb);

  tvhdebug("tvhdhomerun", "Terminating input-thread.\n");

  hfe->hf_input_thread_terminating = 0;
  PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);

  return NULL;
}

static void tvhdhomerun_frontend_default_tables( tvhdhomerun_frontend_t *hfe, dvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)lm;

  psi_tables_default(mm);

  /* ATSC */
  if (hfe->hf_type == DVB_TYPE_ATSC) {
    if (lm->lm_tuning.dmc_fe_modulation == DVB_MOD_VSB_8)
      psi_tables_atsc_t(mm);
    else
      psi_tables_atsc_c(mm);

  /* DVB */
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
  streaming_message_t      sm;
  signal_status_t          sigstat;
  service_t               *svc;

  struct hdhomerun_tuner_status_t tunerStatus;
  char *tunerBufferString;

  hdhomerun_device_get_tuner_status(hfe->hf_hdhomerun_tuner, &tunerBufferString, &tunerStatus);

  if (mmi == NULL) {
    // Not tuned, keep on sending heartbeats to the device to prevent a timeout.
    gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 5000);
    return;
  }

  

  mm = mmi->mmi_mux;

  if ( tunerStatus.signal_present ) {
    hfe->hf_status = SIGNAL_GOOD;
  } else {
    hfe->hf_status = SIGNAL_NONE;
  }

  if ( hfe->hf_status == SIGNAL_NONE ) {
    mmi->mmi_stats.snr = 0;
  } else {
    mmi->mmi_stats.snr = tunerStatus.signal_to_noise_quality;
    tvhdhomerun_frontend_default_tables(hfe, (dvb_mux_t*)mm);
    tvhdhomerun_device_update_pid_filter(hfe, mm);
  }
  mmi->mmi_stats.signal = tunerStatus.signal_strength;

  sigstat.status_text  = signal2str(hfe->hf_status);
  sigstat.snr          = mmi->mmi_stats.snr;
  sigstat.signal       = mmi->mmi_stats.signal;
  sigstat.ber          = mmi->mmi_stats.ber;
  sigstat.unc          = mmi->mmi_stats.unc;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;
  LIST_FOREACH(svc, &hfe->mi_transports, s_active_link) {
    PTHREAD_MUTEX_LOCK(&svc->s_stream_mutex);
    streaming_pad_deliver(&svc->s_streaming_pad, &sm);
    PTHREAD_MUTEX_UNLOCK(&svc->s_stream_mutex);
  }
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1000);
}

static void tvhdhomerun_device_update_pid_filter(tvhdhomerun_frontend_t *hfe, mpegts_mux_t *mm) {
  char filterBuff[1024];
  char tmp[10];
  mpegts_pid_t *mp;

  memset(filterBuff,0,sizeof(filterBuff));

  PTHREAD_MUTEX_LOCK(&hfe->hf_pid_filter_mutex);
  
  PTHREAD_MUTEX_LOCK(&hfe->mi_output_lock);
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    sprintf(tmp, "0x%X ", mp->mp_pid);
    strcat(filterBuff, tmp);
  }
  PTHREAD_MUTEX_UNLOCK(&hfe->mi_output_lock);

  if ( strlen(filterBuff) == 0) {
    sprintf(filterBuff, "0x00");
  }
  
  tvhdebug("tvhdhomerun", "Current pids : %s", filterBuff);

  if ( strncmp(hfe->hf_pid_filter_buf, filterBuff,1024) != 0) {
    memset(hfe->hf_pid_filter_buf,0,sizeof(hfe->hf_pid_filter_buf));
    strcat(hfe->hf_pid_filter_buf, filterBuff);

    tvhdebug("tvhdhomerun", " ----  Setting pid-filter to : %s",filterBuff);
    int r = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, filterBuff);

    if ( r == 0 ) {
      tvhlog(LOG_ERR, "tvhdhomerun", "Command rejected when setting pid filter!");
    } else if ( r == -1) {
      tvhlog(LOG_ERR, "tvhdhomerun", "Communication failure when setting pid filter!");
    } else if ( r != 1 ) {
      tvhlog(LOG_ERR, "tvhdhomerun", "UNKNOWN ERROR(%d) %s:%s:%u",r ,__FILE__,__FUNCTION__,__LINE__);
    }
    
  }
  PTHREAD_MUTEX_UNLOCK(&hfe->hf_pid_filter_mutex);
  
}

static int tvhdhomerun_frontend_tune(tvhdhomerun_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
  hfe->hf_status          = SIGNAL_NONE;
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  struct hdhomerun_tuner_status_t tunerStatus;

  char freqBuf[64];
  snprintf(freqBuf, 64, "auto:%u", dmc->dmc_fe_freq);
  
  int r = hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, freqBuf);
  
  if ( r == 0 ) {
      tvhlog(LOG_ERR, "tvhdhomerun", "Command rejected when tuning to '%s'!",freqBuf);
      return SM_CODE_TUNING_FAILED;
    } else if ( r == -1) {
      tvhlog(LOG_ERR, "tvhdhomerun", "Communication failure when tuning to '%s'!",freqBuf);
      return SM_CODE_TUNING_FAILED;
    } else if ( r != 1 ) {
      tvhlog(LOG_ERR, "tvhdhomerun", "UNKNOWN ERROR(%d) %s:%s:%u",r ,__FILE__,__FUNCTION__,__LINE__);
      return SM_CODE_TUNING_FAILED;
    }

  if ( r == 0 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Command rejected when setting channel!");
    return SM_CODE_TUNING_FAILED;
  } else if ( r == -1) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Communication failure when setting channel!");
    return SM_CODE_TUNING_FAILED;
  } else if ( r != 1 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "UNKNOWN ERROR(%d) %s:%s:%u",r ,__FILE__,__FUNCTION__,__LINE__);
    return SM_CODE_TUNING_FAILED;
  }
  hfe->hf_status = SIGNAL_NONE;

  hdhomerun_device_wait_for_lock(hfe->hf_hdhomerun_tuner, &tunerStatus);
  if ( tunerStatus.signal_present ) {
    hfe->hf_status = SIGNAL_GOOD;
  } else {
    hfe->hf_status = SIGNAL_NONE;
  }


  return 0;
}

static void tvhdhomerun_frontend_stop_inputthread(tvhdhomerun_frontend_t *hfe) {
  if ( hfe->hf_input_thread_running == 1 ) {
    hfe->hf_input_thread_terminating = 1;
    tvhlog(LOG_INFO, "tvhdhomerun", "Stopping iput thread - wait");
    pthread_join(hfe->hf_input_thread, NULL);
    hfe->hf_input_thread_running = 0;
    hfe->hf_input_thread = 0;
    tvhlog(LOG_INFO, "tvhdhomerun", "Stopping input thread - stopped");
  }  
}

static int
tvhdhomerun_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  char buf1[256], buf2[256];

  PTHREAD_MUTEX_LOCK(&hfe->hf_input_mux_lock);

  mpegts_mux_instance_t *cur = LIST_FIRST(&hfe->mi_mux_active);

  hfe->hf_mmi             = mmi;
  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("tvhdhomerun", "%s - stopping %s", buf1, buf2);

  if (cur != NULL) {
    // Already tuned to this MUX
    if (mmi == cur)
      return 0;
    // Stop current 
    cur->mmi_mux->mm_stop(cur->mmi_mux, 1);
  }
  assert(LIST_FIRST(&hfe->mi_mux_active) == NULL);

  tvhdhomerun_frontend_stop_inputthread(hfe);

  if ( hfe->hf_input_thread_running == 0 ) {
    tvhlog(LOG_INFO, "tvhdhomerun", "Starting input thread.");
    hfe->hf_input_thread_running = 1;
    tvhthread_create(&hfe->hf_input_thread, NULL,
                     tvhdhomerun_frontend_input_thread, hfe, 0);

  }
  
  int r =  tvhdhomerun_frontend_tune(hfe, mmi);
  PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);
  return r;
}

static void
tvhdhomerun_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  char buf1[256], buf2[256];

  PTHREAD_MUTEX_LOCK(&hfe->hf_input_mux_lock);

  tvhdhomerun_frontend_stop_inputthread(hfe);

  hfe->hf_mmi      = NULL;

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("tvhdhomerun", "%s - stopping %s", buf1, buf2);
  PTHREAD_MUTEX_UNLOCK(&hfe->hf_input_mux_lock);
}

static mpegts_pid_t *tvhdhomerun_frontend_open_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  mpegts_pid_t *mp;

  tvhdebug("tvhdhomerun", "Open pid 0x%x\n", pid);

  if (!(mp = mpegts_input_open_pid(mi, mm, pid, type, owner))) {
    tvhdebug("tvhdhomerun", "Failed to open pid %d",pid);
    return NULL;
  }

  // Trigger the monitor to update the pid-filter.
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);

  return mp;
}

static void tvhdhomerun_frontend_close_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;

  tvhdebug("tvhdhomerun", "Closing pid 0x%x\n",pid);

  mpegts_input_close_pid(mi, mm, pid, type, owner);

  // Trigger the monitor to update the pid-filter.
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);
  
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
  else
    return NULL;

  return idnode_find_all(idc);
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
  pthread_mutex_destroy(&hfe->hf_mutex);
  pthread_mutex_destroy(&hfe->hf_pid_filter_mutex);
  pthread_mutex_destroy(&hfe->hf_input_mux_lock);
  
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
  else {
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

  // TODO: Need a better heartbeat, or we will need to recreate the hdhomerun-device if something fails.
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);

  return hfe;
}




