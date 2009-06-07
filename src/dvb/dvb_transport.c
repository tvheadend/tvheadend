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

#include "tvhead.h"
#include "dvb.h"
#include "channels.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "dvb_support.h"
#include "notify.h"


/*
 * Switch the adapter (which is implicitly tied to our transport)
 * to receive the given transport.
 *
 * But we only do this if 'weight' is higher than all of the current
 * transports that is subscribing to the adapter
 */
static int
dvb_transport_start(th_transport_t *t, unsigned int weight, int status, 
		    int force_start)
{
  struct dmx_pes_filter_params dmx_param;
  th_stream_t *st;
  int w, fd, pid;
  th_dvb_adapter_t *tda = t->tht_dvb_mux_instance->tdmi_adapter;
  th_dvb_mux_instance_t *tdmi = tda->tda_mux_current;

  lock_assert(&global_lock);

  if(tda->tda_rootpath == NULL)
    return 1; /* hardware not present */

  /* Check if adapter is idle, or already tuned */

  if(tdmi != NULL && tdmi != t->tht_dvb_mux_instance && !force_start) {

    w = transport_compute_weight(&tdmi->tdmi_adapter->tda_transports);
    if(w >= weight)
      return 1; /* We are outranked by weight, cant use it */

    dvb_adapter_clean(tda);
  }
  tdmi = t->tht_dvb_mux_instance;

  LIST_FOREACH(st, &t->tht_components, st_link) {
    fd = open(tda->tda_demux_path, O_RDWR);
    
    pid = st->st_pid;
    st->st_cc_valid = 0;

    if(fd == -1) {
      st->st_demuxer_fd = -1;
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to open demuxer \"%s\" for pid %d -- %s",
	     t->tht_name, tda->tda_demux_path, pid, strerror(errno));
      continue;
    }

    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid = pid;
    dmx_param.input = DMX_IN_FRONTEND;
    dmx_param.output = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags = DMX_IMMEDIATE_START;

    if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to configure demuxer \"%s\" for pid %d -- %s",
	     t->tht_name, tda->tda_demux_path, pid, strerror(errno));
      close(fd);
      fd = -1;
    }

    st->st_demuxer_fd = fd;
  }

  pthread_mutex_lock(&tda->tda_delivery_mutex);

  LIST_INSERT_HEAD(&tda->tda_transports, t, tht_active_link);
  t->tht_status = status;
  

  dvb_fe_tune(tdmi, "Transport start");

  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  return 0;
}


/**
 *
 */
static void
dvb_transport_stop(th_transport_t *t)
{
  th_dvb_adapter_t *tda = t->tht_dvb_mux_instance->tdmi_adapter;
  th_stream_t *st;

  lock_assert(&global_lock);

  pthread_mutex_lock(&tda->tda_delivery_mutex);
  LIST_REMOVE(t, tht_active_link);
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  LIST_FOREACH(st, &t->tht_components, st_link) {
    close(st->st_demuxer_fd);
    st->st_demuxer_fd = -1;
  }
  t->tht_status = TRANSPORT_IDLE;
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
  th_transport_t *t;

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
    
    t = dvb_transport_find(tdmi, sid, pmt, NULL);

    htsmsg_get_u32(c, "stype", &t->tht_servicetype);
    if(htsmsg_get_u32(c, "scrambled", &u32))
      u32 = 0;
    t->tht_scrambled = u32;

    s = htsmsg_get_str(c, "provider") ?: "unknown";
    t->tht_provider = strdup(s);

    s = htsmsg_get_str(c, "servicename") ?: "unknown";
    t->tht_svcname = strdup(s);

    s = htsmsg_get_str(c, "channelname");
    if(s != NULL) {
      t->tht_chname = strdup(s);
    } else {
      t->tht_chname = strdup(t->tht_svcname);
    }

    pthread_mutex_lock(&t->tht_stream_mutex);
    psi_load_transport_settings(c, t);
    pthread_mutex_unlock(&t->tht_stream_mutex);

    if(!htsmsg_get_u32(c, "mapped", &u32) && u32)
      transport_map_channel(t, NULL);
  }
  htsmsg_destroy(l);
}

/**
 *
 */
static void
dvb_transport_save(th_transport_t *t)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "service_id", t->tht_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->tht_pmt_pid);
  htsmsg_add_u32(m, "stype", t->tht_servicetype);
  htsmsg_add_u32(m, "scrambled", t->tht_scrambled);

  if(t->tht_provider != NULL)
    htsmsg_add_str(m, "provider", t->tht_provider);

  if(t->tht_svcname != NULL)
    htsmsg_add_str(m, "servicename", t->tht_svcname);

  if(t->tht_chname != NULL)
    htsmsg_add_str(m, "channelname", t->tht_chname);

  htsmsg_add_u32(m, "mapped", !!t->tht_ch);
  
  psi_save_transport_settings(m, t);
  
  hts_settings_save(m, "dvbtransports/%s/%s",
		    t->tht_dvb_mux_instance->tdmi_identifier,
		    t->tht_identifier);

  htsmsg_destroy(m);
}


/**
 * Called to get quality for the given transport
 *
 * We keep track of this for the entire mux (if we see errors), soo..
 * return that value 
 */
static int
dvb_transport_quality(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->tht_dvb_mux_instance;

  lock_assert(&global_lock);

  return tdmi->tdmi_quality;
}


/**
 * Generate a descriptive name for the source
 */
static const char *
dvb_transport_sourcename(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->tht_dvb_mux_instance;

  lock_assert(&global_lock);

  return tdmi->tdmi_adapter->tda_displayname;
}


/**
 * Generate a descriptive name for the source
 */
static const char *
dvb_transport_networkname(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->tht_dvb_mux_instance;

  lock_assert(&global_lock);

  return tdmi->tdmi_network;
}




/**
 * Find a transport based on 'serviceid' on the given mux
 *
 * If it cannot be found we create it if 'pmt_pid' is also set
 */
th_transport_t *
dvb_transport_find(th_dvb_mux_instance_t *tdmi, uint16_t sid, int pmt_pid,
		   int *created)
{
  th_transport_t *t;
  char tmp[200];

  lock_assert(&global_lock);

  if(created != NULL)
    *created = 0;

  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    if(t->tht_dvb_service_id == sid)
      return t;
  }
  
  if(pmt_pid == 0)
    return NULL;

  if(created != NULL)
    *created = 1;

  snprintf(tmp, sizeof(tmp), "%s_%04x", tdmi->tdmi_identifier, sid);

  t = transport_create(tmp, TRANSPORT_DVB, THT_MPEG_TS);

  t->tht_dvb_service_id = sid;
  t->tht_pmt_pid        = pmt_pid;

  t->tht_start_feed = dvb_transport_start;
  t->tht_stop_feed  = dvb_transport_stop;
  t->tht_config_change = dvb_transport_save;
  t->tht_sourcename = dvb_transport_sourcename;
  t->tht_networkname = dvb_transport_networkname;
  t->tht_dvb_mux_instance = tdmi;
  t->tht_quality_index = dvb_transport_quality;

  LIST_INSERT_HEAD(&tdmi->tdmi_transports, t, tht_mux_link);
  return t;
}
