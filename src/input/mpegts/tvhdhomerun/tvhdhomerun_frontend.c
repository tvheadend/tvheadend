
/*
 *  Tvheadend - HDHomeRun DVB frontend
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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

printf ("Enter %s\n", __PRETTY_FUNCTION__);

  TAILQ_FOREACH(hfe, &hd->hd_frontends, hf_link)
    if (hfe->hf_number == num)
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
  printf ("Enter %s\n", __PRETTY_FUNCTION__);
  tvhdhomerun_frontend_t *hfe = aux;
  mpegts_mux_instance_t *mmi = hfe->hf_mmi;
  sbuf_t sb;
  char buf[256];
  

  /* Get MMI */
  hfe->mi_display_name((mpegts_input_t*)hfe, buf, sizeof(buf));
  
  assert(mmi != NULL);
  if (mmi == NULL) {
    tvhlog(LOG_ERR, "tvhdhomerun","mmi == 0");
    hfe->hf_input_thread_running = 0;
    return NULL;
  }

  assert(hfe->hf_hdhomerun_video_sock == NULL);

  int r = hdhomerun_device_stream_start(hfe->hf_hdhomerun_tuner);
  if ( r == 0 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Failed to start stream from HDHomeRun device! (Command rejected!)");
    sleep(10);
  } else if ( r == -1 ) {
    tvhlog(LOG_ERR, "tvhdhomerun", "Failed to start stream from HDHomeRun device! (Communication error!)");
    // TODO: Mark device as invalid and remove and trigger rescan.
    sleep(10);
    return NULL;
  }

  hfe->hf_hdhomerun_video_sock = hdhomerun_device_get_video_sock(hfe->hf_hdhomerun_tuner);

  sbuf_init_fixed(&sb,VIDEO_DATA_BUFFER_SIZE_1S); // Buffersize in hdhomerun.

  while (tvheadend_running && hfe->hf_input_thread_terminating == 0 ) {

    const size_t maxRead = 188*100; // Available space in sbuf.
    size_t readDataSize = 0;
 
    pthread_mutex_lock(&hfe->hf_hdhomerun_mutex);
    uint8_t *readData = hdhomerun_video_recv(hfe->hf_hdhomerun_video_sock, maxRead, &readDataSize);
    if ( readDataSize > 0 ) {    
      //printf("readDataSize : %lu\n", readDataSize);
      sbuf_append(&sb, readData, readDataSize);
      mpegts_input_recv_packets((mpegts_input_t*)hfe, mmi,
                                 &sb, 0, NULL, NULL);
    }
    pthread_mutex_unlock(&hfe->hf_hdhomerun_mutex);
  
    if ( readDataSize < 1000 ) {
      usleep(25000);
    }
    
  }
  pthread_mutex_lock(&hfe->hf_hdhomerun_mutex);
  hdhomerun_device_stream_stop(hfe->hf_hdhomerun_tuner);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_mutex);
  hfe->hf_hdhomerun_video_sock = 0;

  sbuf_free(&sb);

  printf("Thread terminating...\n");

  hfe->hf_input_thread_terminating = 0;

  return NULL;
#undef PKTS
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

  
  // TODO: Fetch the Debug status message and parse...

  if (mmi == NULL) {
    return;
  }


  pthread_mutex_lock(&hfe->hf_hdhomerun_mutex);
  hdhomerun_device_get_tuner_status(hfe->hf_hdhomerun_tuner, &tunerBufferString, &tunerStatus);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_mutex);

  mm = mmi->mmi_mux;

  if ( tunerStatus.signal_present ) {
//    printf("Signal GOOD\n");
    hfe->hf_status = SIGNAL_GOOD;
  } else {
//    printf("Signal BAD!\n");
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
    pthread_mutex_lock(&svc->s_stream_mutex);
    streaming_pad_deliver(&svc->s_streaming_pad, &sm);
    pthread_mutex_unlock(&svc->s_stream_mutex);
  }
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1000);
}

static void tvhdhomerun_device_update_pid_filter(tvhdhomerun_frontend_t *hfe, mpegts_mux_t *mm) {
  char filterBuff[1024];
  char tmp[10];
  mpegts_pid_t *mp;

  memset(filterBuff,0,sizeof(filterBuff));

 // sprintf(filterBuff, "0x00-0x1fff");

  pthread_mutex_lock(&hfe->hf_pid_filter_mutex);
  
  pthread_mutex_lock(&hfe->mi_output_lock);
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    sprintf(tmp, "0x%X ", mp->mp_pid);
    strcat(filterBuff, tmp);
  }
  pthread_mutex_unlock(&hfe->mi_output_lock);

  if ( strlen(filterBuff) == 0) {
    sprintf(filterBuff, "0x00");
  }
  
  printf("Current pids : %s\n", filterBuff);

  if ( strncmp(hfe->hf_pid_filter_buf, filterBuff,1024) != 0) {
    memset(hfe->hf_pid_filter_buf,0,sizeof(hfe->hf_pid_filter_buf));
    strcat(hfe->hf_pid_filter_buf, filterBuff);

    pthread_mutex_lock(&hfe->hf_hdhomerun_mutex);
    printf(" ----  Setting pid-filter to : %s\n",filterBuff);
    int r = hdhomerun_device_set_tuner_filter(hfe->hf_hdhomerun_tuner, filterBuff);
    if ( r == 0 ) {
      printf("Command rejected!\n");
    } else if ( r == -1) {
      printf("Communication failure when setting pid filter.!\n");
    }
    pthread_mutex_unlock(&hfe->hf_hdhomerun_mutex);
    
  }
  pthread_mutex_unlock(&hfe->hf_pid_filter_mutex);
  

  // TODO: REMOVE

  
}

static int tvhdhomerun_frontend_tune(tvhdhomerun_frontend_t *hfe, mpegts_mux_instance_t *mmi)
{
  hfe->hf_status          = SIGNAL_NONE;
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc = &lm->lm_tuning;

  char freqBuf[64];
  snprintf(freqBuf, 64, "auto:%u", dmc->dmc_fe_freq);
  pthread_mutex_lock(&hfe->hf_hdhomerun_mutex);
  hdhomerun_device_set_tuner_channel(hfe->hf_hdhomerun_tuner, freqBuf);
  hdhomerun_device_stream_flush(hfe->hf_hdhomerun_tuner);
  pthread_mutex_unlock(&hfe->hf_hdhomerun_mutex);
  hfe->hf_status = SIGNAL_NONE;

  // mpegts_mux_t *mm = mmi->mmi_mux;

  return 0;
}



static int
tvhdhomerun_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  printf ("Enter %s\n", __PRETTY_FUNCTION__);
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  char buf1[256], buf2[256];

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

  // Used to push a "clean" filter to the device to prevent flooding.
  // tvhdhomerun_device_update_pid_filter(hfe, mmi->mmi_mux);

  // TODO: mutex-lock when creating thread and in thread...
  assert( hfe->hf_input_thread_running == 0);
  hfe->hf_input_thread_running = 1;
  tvhthread_create(&hfe->hf_input_thread, NULL,
                  tvhdhomerun_frontend_input_thread, hfe, 0);

  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);

  int r =  tvhdhomerun_frontend_tune(hfe, mmi);
  printf ("Exit %s\n", __PRETTY_FUNCTION__);
  return r;
}

static void
tvhdhomerun_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  printf ("Enter %s\n", __PRETTY_FUNCTION__);
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  char buf1[256], buf2[256];

  hfe->hf_input_thread_terminating = 1;

  pthread_join(hfe->hf_input_thread, NULL);
  hfe->hf_mmi      = NULL;

  hfe->hf_input_thread_running = 0;

  gtimer_disarm(&hfe->hf_monitor_timer);

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("tvhdhomerun", "%s - stopping %s", buf1, buf2);

}

static mpegts_pid_t *tvhdhomerun_frontend_open_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  mpegts_pid_t *mp;

  printf("Open pid 0x%x\n", pid);

  if (!(mp = mpegts_input_open_pid(mi, mm, pid, type, owner))) {
    printf("RET NULL!\n");
    return NULL;
  }

  // reschedule the timer to get a faster update of the pid-filter.
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);

  //tvhdhomerun_device_update_pid_filter(hfe, mm);

  return mp;
}

static void tvhdhomerun_frontend_close_pid( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;

  printf("Closing pid 0x%x\n",pid);

  mpegts_input_close_pid(mi, mm, pid, type, owner);

  // reschedule the timer to get a faster update of the pid-filter.
  gtimer_arm_ms(&hfe->hf_monitor_timer, tvhdhomerun_frontend_monitor_cb, hfe, 1);
  
  //tvhdhomerun_device_update_pid_filter(hfe, mm);
}

static idnode_set_t *
tvhdhomerun_frontend_network_list ( mpegts_input_t *mi )
{
  tvhdhomerun_frontend_t *hfe = (tvhdhomerun_frontend_t*)mi;
  const idclass_t     *idc;

printf ("Enter %s\n", __PRETTY_FUNCTION__);

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
  printf ("Enter %s\n", __PRETTY_FUNCTION__);

  tvhdhomerun_device_t *la = ((tvhdhomerun_frontend_t*)in)->hf_device;
  tvhdhomerun_device_save(la);
}

static int
tvhdhomerun_frontend_set_new_type
  ( tvhdhomerun_frontend_t *hfe, const char *type )
{
  printf ("Enter %s\n", __PRETTY_FUNCTION__);
  free(hfe->hf_type_override);
  hfe->hf_type_override = strdup(type);
  tvhdhomerun_device_destroy_later(hfe->hf_device, 100);
  return 1;
}

static int
tvhdhomerun_frontend_class_override_set( void *obj, const void * p )
{
  tvhdhomerun_frontend_t *hfe = obj;
  const char *s = p;

printf ("Enter %s\n", __PRETTY_FUNCTION__);
  if (hfe->hf_type_override == NULL) {
    if (strlen(p) > 0)
      return tvhdhomerun_frontend_set_new_type(hfe, s);
  } else if (strcmp(hfe->hf_type_override, s))
    return tvhdhomerun_frontend_set_new_type(hfe, s);
  return 0;
}

static htsmsg_t *
tvhdhomerun_frontend_class_override_enum( void * p )
{
  printf ("Enter %s\n", __PRETTY_FUNCTION__);
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "DVB-T");
  htsmsg_add_str(m, NULL, "DVB-C");
  return m;
}

void
tvhdhomerun_frontend_save ( tvhdhomerun_frontend_t *hfe, htsmsg_t *fe )
{
  char id[16];
  htsmsg_t *m = htsmsg_create_map();

printf ("Enter %s\n", __PRETTY_FUNCTION__);
  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)hfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(hfe->hf_type));

  htsmsg_delete_field(m, "fe_override");
  htsmsg_delete_field(m, "fe_master");

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(hfe->hf_type), hfe->hf_number);
  htsmsg_add_msg(fe, id, m);
  if (hfe->hf_type_override) {
    snprintf(id, sizeof(id), "override #%d", hfe->hf_number);
    htsmsg_add_str(fe, id, hfe->hf_type_override);
  }
  if (hfe->hf_master) {
    snprintf(id, sizeof(id), "master for #%d", hfe->hf_number);
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
      .off      = offsetof(tvhdhomerun_frontend_t, hf_number),
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
    {
      .type     = PT_STR,
      .id       = "fe_override",
      .name     = "Network Type",
      .set      = tvhdhomerun_frontend_class_override_set,
      .list     = tvhdhomerun_frontend_class_override_enum,
      .off      = offsetof(tvhdhomerun_frontend_t, hf_type_override),
    },
    {}
  }
};

const idclass_t tvhdhomerun_frontend_dvbc_class =
{
  .ic_super      = &tvhdhomerun_frontend_class,
  .ic_class      = "tvhdhomerun_frontend_dvbc",
  .ic_caption    = "HDHomeRun DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "fe_override",
      .name     = "Network Type",
      .set      = tvhdhomerun_frontend_class_override_set,
      .list     = tvhdhomerun_frontend_class_override_enum,
      .off      = offsetof(tvhdhomerun_frontend_t, hf_type_override),
    },
    {}
  }
};

void
tvhdhomerun_frontend_delete ( tvhdhomerun_frontend_t *hfe )
{
  lock_assert(&global_lock);

  printf ("Enter %s\n", __PRETTY_FUNCTION__);

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)hfe);

  /* Stop timer */
  gtimer_disarm(&hfe->hf_monitor_timer);

  /* Remove from adapter */
  TAILQ_REMOVE(&hfe->hf_device->hd_frontends, hfe, hf_link);

  free(hfe->hf_type_override);

  hdhomerun_device_destroy(hfe->hf_hdhomerun_tuner);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)hfe, 0);
}

tvhdhomerun_frontend_t * 
tvhdhomerun_frontend_create( htsmsg_t *conf, tvhdhomerun_device_t *hd, dvb_fe_type_t type, int num, struct hdhomerun_device_t *hdhomerun_tuner )
{
  const idclass_t *idc;
  const char *uuid = NULL, *override = NULL;
  char id[16], lname[256];
  tvhdhomerun_frontend_t *hfe;
  uint32_t master = 0;
  int i;

printf ("Enter %s\n", __PRETTY_FUNCTION__);
  /* Override type */
  snprintf(id, sizeof(id), "override #%d", num);
  if (conf) {
    override = htsmsg_get_str(conf, id);
    if (override) {
      i = dvb_str2type(override);
      if ((i == DVB_TYPE_T || i == DVB_TYPE_C ) && i != type)
        type = i;
      else
        override = NULL;
    }
  }
  /* Tuner slave */
  snprintf(id, sizeof(id), "master for #%d", num);

  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(type), num);
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

  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  hfe = calloc(1, sizeof(tvhdhomerun_frontend_t));
  hfe->hf_device   = hd;
  hfe->hf_number   = num;
  hfe->hf_type     = type;
  hfe->hf_master   = master;
  hfe->hf_type_override = override ? strdup(override) : NULL;

  hfe->hf_hdhomerun_tuner = hdhomerun_tuner;
  hfe->hf_input_thread_running = 0;
  hfe->hf_input_thread_terminating = 0;

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
    snprintf(lname, sizeof(lname), "HDHomeRun %s Tuner #%i (%s)",
             dvb_type2str(type), num, hd->hd_info.addr);
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

  // Init Frontend mutexes.
  pthread_mutex_init(&hfe->hf_input_thread_mutex, NULL);
  pthread_mutex_init(&hfe->hf_mutex, NULL);
  pthread_mutex_init(&hfe->hf_hdhomerun_mutex,NULL);
  pthread_mutex_init(&hfe->hf_pid_filter_mutex,NULL);
  


  /* Adapter link */
  hfe->hf_device = hd;
  TAILQ_INSERT_TAIL(&hd->hd_frontends, hfe, hf_link);

  /* Slave networks update */
  if (master) {
    tvhdhomerun_frontend_t *hfe2 = tvhdhomerun_frontend_find_by_number(hd, master);
    if (hfe2) {
      htsmsg_t *l = (htsmsg_t *)mpegts_input_class_network_get(hfe2);
      if (l) {
        mpegts_input_class_network_set(hfe, l);
        htsmsg_destroy(l);
      }
    }
  }

  return hfe;
}




