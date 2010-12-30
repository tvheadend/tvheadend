/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2007 Andreas Öman
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

#include <pthread.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "settings.h"

#include "tvheadend.h"
#include "dvb.h"
#include "channels.h"
#include "subscriptions.h"
#include "psi.h"
#include "dvb_support.h"
#include "notify.h"

#if 0

/**
 *
 */
static void
dvb_transport_open_demuxers(th_dvb_adapter_t *tda, service_t *t)
{
  struct dmx_pes_filter_params dmx_param;
  int fd;
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    if(st->es_pid >= 0x2000)
      continue;

    if(st->es_demuxer_fd != -1)
      continue;

    fd = tvh_open(tda->tda_demux_path, O_RDWR, 0);
    st->es_cc_valid = 0;

    if(fd == -1) {
      st->es_demuxer_fd = -1;
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to open demuxer \"%s\" for pid %d -- %s",
	     t->s_identifier, tda->tda_demux_path, 
	     st->es_pid, strerror(errno));
      continue;
    }

    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid = st->es_pid;
    dmx_param.input = DMX_IN_FRONTEND;
    dmx_param.output = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags = DMX_IMMEDIATE_START;

    if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to configure demuxer \"%s\" for pid %d -- %s",
	     t->s_identifier, tda->tda_demux_path, 
	     st->es_pid, strerror(errno));
      close(fd);
      fd = -1;
    }

    st->es_demuxer_fd = fd;
  }
}



/**
 * Switch the adapter (which is implicitly tied to our transport)
 * to receive the given transport.
 *
 * But we only do this if 'weight' is higher than all of the current
 * transports that is subscribing to the adapter
 */
static int
dvb_transport_start(service_t *t, unsigned int weight, int force_start)
{
  int w, r;
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;
  th_dvb_mux_instance_t *tdmi = tda->tda_mux_current;

  lock_assert(&global_lock);

  if(tda->tda_rootpath == NULL)
    return SM_CODE_NO_HW_ATTACHED;

  if(t->s_dvb_mux_instance && !t->s_dvb_mux_instance->tdmi_enabled)
    return SM_CODE_MUX_NOT_ENABLED; /* Mux is disabled */

  /* Check if adapter is idle, or already tuned */

  if(tdmi != NULL && 
     (tdmi != t->s_dvb_mux_instance ||
      tda->tda_hostconnection == HOSTCONNECTION_USB12)) {

    w = service_compute_weight(&tda->tda_transports);
    if(w && w >= weight && !force_start)
      /* We are outranked by weight, cant use it */
      return SM_CODE_NOT_FREE;
    
    dvb_adapter_clean(tda);
  }

  pthread_mutex_lock(&tda->tda_delivery_mutex);

  r = dvb_fe_tune(t->s_dvb_mux_instance, "Transport start");
  if(!r)
    LIST_INSERT_HEAD(&tda->tda_transports, t, s_active_link);

  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  if(!r)
    dvb_transport_open_demuxers(tda, t);

  return r;
}


/**
 *
 */
static void
dvb_transport_stop(service_t *t)
{
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;
  elementary_stream_t *st;

  lock_assert(&global_lock);

  pthread_mutex_lock(&tda->tda_delivery_mutex);
  LIST_REMOVE(t, s_active_link);
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    if(st->es_demuxer_fd != -1) {
      close(st->es_demuxer_fd);
      st->es_demuxer_fd = -1;
    }
  }
  t->s_status = SERVICE_IDLE;
}


/**
 *
 */
static void
dvb_transport_refresh(service_t *t)
{
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;

  lock_assert(&global_lock);
  dvb_transport_open_demuxers(tda, t);
}



/**
 * Load config for the given mux
 */
void
dvb_transport_load(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  uint32_t sid, pmt;
  const char *s;
  unsigned int u32;
  service_t *t;

  lock_assert(&global_lock);

  if((l = hts_settings_load("dvbtransports/%s", tdmi->tdmi_identifier)) == NULL)
    return;
  
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "service_id", &sid))
      continue;

    if(htsmsg_get_u32(c, "pmt", &pmt))
      continue;
    
    t = dvb_transport_find(tdmi, sid, pmt, f->hmf_name);

    htsmsg_get_u32(c, "stype", &t->s_servicetype);
    if(htsmsg_get_u32(c, "scrambled", &u32))
      u32 = 0;
    t->s_scrambled = u32;

    if(htsmsg_get_u32(c, "channel", &u32))
      u32 = 0;
    t->s_channel_number = u32;

    s = htsmsg_get_str(c, "provider");
    t->s_provider = s ? strdup(s) : NULL;

    s = htsmsg_get_str(c, "servicename");
    t->s_svcname = s ? strdup(s) : NULL;

    pthread_mutex_lock(&t->s_stream_mutex);
    service_make_nicename(t);
    psi_load_service_settings(c, t);
    pthread_mutex_unlock(&t->s_stream_mutex);
    
    s = htsmsg_get_str(c, "channelname");
    if(htsmsg_get_u32(c, "mapped", &u32))
      u32 = 0;
    
    if(s && u32)
      service_map_channel(t, channel_find_by_name(s, 1, 0), 0);
  }
  htsmsg_destroy(l);
}

/**
 *
 */
static void
dvb_transport_save(service_t *t)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "service_id", t->s_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);
  htsmsg_add_u32(m, "stype", t->s_servicetype);
  htsmsg_add_u32(m, "scrambled", t->s_scrambled);
  htsmsg_add_u32(m, "channel", t->s_channel_number);

  if(t->s_provider != NULL)
    htsmsg_add_str(m, "provider", t->s_provider);

  if(t->s_svcname != NULL)
    htsmsg_add_str(m, "servicename", t->s_svcname);

  if(t->s_ch != NULL) {
    htsmsg_add_str(m, "channelname", t->s_ch->ch_name);
    htsmsg_add_u32(m, "mapped", 1);
  }
  
  pthread_mutex_lock(&t->s_stream_mutex);
  psi_save_service_settings(m, t);
  pthread_mutex_unlock(&t->s_stream_mutex);
  
  hts_settings_save(m, "dvbtransports/%s/%s",
		    t->s_dvb_mux_instance->tdmi_identifier,
		    t->s_identifier);

  htsmsg_destroy(m);
  dvb_transport_notify(t);
}


/**
 * Called to get quality for the given transport
 *
 * We keep track of this for the entire mux (if we see errors), soo..
 * return that value 
 */
static int
dvb_transport_quality(service_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->s_dvb_mux_instance;

  lock_assert(&global_lock);

  return tdmi->tdmi_adapter->tda_qmon ? tdmi->tdmi_quality : 100;
}


/**
 * Generate a descriptive name for the source
 */
static void
dvb_transport_setsourceinfo(service_t *t, struct source_info *si)
{
  th_dvb_mux_instance_t *tdmi = t->s_dvb_mux_instance;
  char buf[100];

  memset(si, 0, sizeof(struct source_info));

  lock_assert(&global_lock);

  if(tdmi->tdmi_adapter->tda_rootpath  != NULL)
    si->si_device = strdup(tdmi->tdmi_adapter->tda_rootpath);

  si->si_adapter = strdup(tdmi->tdmi_adapter->tda_displayname);
  
  if(tdmi->tdmi_network != NULL)
    si->si_network = strdup(tdmi->tdmi_network);
  
  dvb_mux_nicename(buf, sizeof(buf), tdmi);
  si->si_mux = strdup(buf);

  if(t->s_provider != NULL)
    si->si_provider = strdup(t->s_provider);

  if(t->s_svcname != NULL)
    si->si_service = strdup(t->s_svcname);
}


/**
 *
 */
static int
dvb_grace_period(service_t *t)
{
  return 10;
}


/**
 * Find a transport based on 'serviceid' on the given mux
 *
 * If it cannot be found we create it if 'pmt_pid' is also set
 */
service_t *
dvb_transport_find(th_dvb_mux_instance_t *tdmi, uint16_t sid, int pmt_pid,
		   const char *identifier)
{
  service_t *t;
  char tmp[200];
  char buf[200];

  lock_assert(&global_lock);

  LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
    if(t->s_dvb_service_id == sid)
      return t;
  }
  
  if(pmt_pid == 0)
    return NULL;

  if(identifier == NULL) {
    snprintf(tmp, sizeof(tmp), "%s_%04x", tdmi->tdmi_identifier, sid);
    identifier = tmp;
  }

  dvb_mux_nicename(buf, sizeof(buf), tdmi);
  tvhlog(LOG_DEBUG, "dvb", "Add service \"%s\" on \"%s\"", identifier, buf);

  t = service_create(identifier, SERVICE_TYPE_DVB, S_MPEG_TS);

  t->s_dvb_service_id = sid;
  t->s_pmt_pid        = pmt_pid;

  t->s_start_feed    = dvb_transport_start;
  t->s_refresh_feed  = dvb_transport_refresh;
  t->s_stop_feed     = dvb_transport_stop;
  t->s_config_save   = dvb_transport_save;
  t->s_setsourceinfo = dvb_transport_setsourceinfo;
  t->s_quality_index = dvb_transport_quality;
  t->s_grace_period  = dvb_grace_period;

  t->s_dvb_mux_instance = tdmi;
  LIST_INSERT_HEAD(&tdmi->tdmi_transports, t, s_group_link);

  pthread_mutex_lock(&t->s_stream_mutex); 
  service_make_nicename(t);
  pthread_mutex_unlock(&t->s_stream_mutex); 

  dvb_adapter_notify(tdmi->tdmi_adapter);
  return t;
}


/**
 *
 */
htsmsg_t *
dvb_transport_build_msg(service_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->s_dvb_mux_instance;
  htsmsg_t *m = htsmsg_create_map();
  char buf[100];
 
  htsmsg_add_str(m, "id", t->s_identifier);
  htsmsg_add_u32(m, "enabled", t->s_enabled);
  htsmsg_add_u32(m, "channel", t->s_channel_number);

  htsmsg_add_u32(m, "sid", t->s_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);
  htsmsg_add_u32(m, "pcr", t->s_pcr_pid);
  
  htsmsg_add_str(m, "type", service_servicetype_txt(t));

  htsmsg_add_str(m, "svcname", t->s_svcname ?: "");
  htsmsg_add_str(m, "provider", t->s_provider ?: "");

  htsmsg_add_str(m, "network", tdmi->tdmi_network ?: "");

  dvb_mux_nicefreq(buf, sizeof(buf), tdmi);
  htsmsg_add_str(m, "mux", buf);

  if(t->s_ch != NULL)
    htsmsg_add_str(m, "channelname", t->s_ch->ch_name);

  return m;
}


/**
 *
 */
void
dvb_transport_notify_by_adapter(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "adapterId", tda->tda_identifier);
  notify_by_msg("dvbService", m);
}


/**
 *
 */
void
dvb_transport_notify(service_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->s_dvb_mux_instance;
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "adapterId", tdmi->tdmi_adapter->tda_identifier);
  notify_by_msg("dvbService", m);
}


/**
 * Get the signal status from a DVB transport
 */
int
dvb_transport_get_signal_status(service_t *t, signal_status_t *status)
{
  th_dvb_mux_instance_t *tdmi = t->s_dvb_mux_instance;

  status->status_text = dvb_mux_status(tdmi);
  status->snr         = tdmi->tdmi_snr;
  status->signal      = tdmi->tdmi_signal;
  status->ber         = tdmi->tdmi_ber;
  status->unc         = tdmi->tdmi_uncorrected_blocks;
  return 0;
}
#endif
