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

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "settings.h"

#include "tvhead.h"
#include "transports.h"
#include "dvb.h"
#include "dvb_support.h"
#include "tsdemux.h"
#include "notify.h"

struct th_dvb_adapter_queue dvb_adapters;
struct th_dvb_mux_instance_tree dvb_muxes;
static void *dvb_adapter_input_dvr(void *aux);


/**
 *
 */
static th_dvb_adapter_t *
tda_alloc(void)
{
  th_dvb_adapter_t *tda = calloc(1, sizeof(th_dvb_adapter_t));
  pthread_mutex_init(&tda->tda_delivery_mutex, NULL);

  TAILQ_INIT(&tda->tda_scan_queues[0]);
  TAILQ_INIT(&tda->tda_scan_queues[1]);
  TAILQ_INIT(&tda->tda_initial_scan_queue);
  TAILQ_INIT(&tda->tda_satconfs);
  return tda;
}


/**
 * Save config for the given adapter
 */
static void
tda_save(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_str(m, "type", dvb_adaptertype_to_str(tda->tda_type));
  htsmsg_add_str(m, "displayname", tda->tda_displayname);
  htsmsg_add_u32(m, "autodiscovery", tda->tda_autodiscovery);
  htsmsg_add_u32(m, "idlescan", tda->tda_idlescan);
  htsmsg_add_u32(m, "logging", tda->tda_logging);
  hts_settings_save(m, "dvbadapters/%s", tda->tda_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
void
dvb_adapter_set_displayname(th_dvb_adapter_t *tda, const char *s)
{
  lock_assert(&global_lock);

  if(!strcmp(s, tda->tda_displayname))
    return;

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" renamed to \"%s\"",
	 tda->tda_displayname, s);

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(s);
  
  tda_save(tda);

  dvb_adapter_notify(tda);
}


/**
 *
 */
void
dvb_adapter_set_auto_discovery(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_autodiscovery == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" mux autodiscovery set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_autodiscovery = on;
  tda_save(tda);
}


/**
 *
 */
void
dvb_adapter_set_idlescan(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_idlescan == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" idle mux scanning set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_idlescan = on;
  tda_save(tda);
}


/**
 *
 */
void
dvb_adapter_set_logging(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_logging == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" detailed logging set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_logging = on;
  tda_save(tda);
}


/**
 *
 */
static void
tda_add(const char *path)
{
  char fname[256];
  int fe, i, r;
  th_dvb_adapter_t *tda;
  char buf[400];
  pthread_t ptid;

  snprintf(fname, sizeof(fname), "%s/frontend0", path);
  
  fe = open(fname, O_RDWR | O_NONBLOCK);
  if(fe == -1) {
    if(errno != ENOENT)
      tvhlog(LOG_ALERT, "dvb",
	     "Unable to open %s -- %s\n", fname, strerror(errno));
    return;
  }

  tda = tda_alloc();

  tda->tda_rootpath = strdup(path);
  tda->tda_demux_path = malloc(256);
  snprintf(tda->tda_demux_path, 256, "%s/demux0", path);
  tda->tda_dvr_path = malloc(256);
  snprintf(tda->tda_dvr_path, 256, "%s/dvr0", path);


  tda->tda_fe_fd = fe;

  tda->tda_fe_info = malloc(sizeof(struct dvb_frontend_info));

  if(ioctl(tda->tda_fe_fd, FE_GET_INFO, tda->tda_fe_info)) {
    tvhlog(LOG_ALERT, "dvb", "%s: Unable to query adapter\n", fname);
    close(fe);
    free(tda);
    return;
  }

  tda->tda_type = tda->tda_fe_info->type;

  snprintf(buf, sizeof(buf), "%s_%s", tda->tda_rootpath,
	   tda->tda_fe_info->name);

  r = strlen(buf);
  for(i = 0; i < r; i++)
    if(!isalnum((int)buf[i]))
      buf[i] = '_';

  tda->tda_identifier = strdup(buf);
  
  tda->tda_autodiscovery = tda->tda_type != FE_QPSK;
  tda->tda_idlescan = 1;

  tda->tda_sat = tda->tda_type == FE_QPSK;

  /* Come up with an initial displayname, user can change it and it will
     be overridden by any stored settings later on */

  tda->tda_displayname = strdup(tda->tda_fe_info->name);

  tvhlog(LOG_INFO, "dvb",
	 "Found adapter %s (%s)", path, tda->tda_fe_info->name);

  TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);

  pthread_create(&ptid, NULL, dvb_adapter_input_dvr, tda);

  dvb_table_init(tda);

  if(tda->tda_sat)
    dvb_satconf_init(tda);

  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 1);
}


/**
 *
 */
void
dvb_adapter_init(void)
{
  char path[200];
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  const char *name, *s;
  int i, type;
  th_dvb_adapter_t *tda;

  TAILQ_INIT(&dvb_adapters);

  for(i = 0; i < 32; i++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d", i);
    tda_add(path);
  }

  l = hts_settings_load("dvbadapters");
  if(l != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;
      
      name = htsmsg_get_str(c, "displayname");

      if((s = htsmsg_get_str(c, "type")) == NULL ||
	 (type = dvb_str_to_adaptertype(s)) < 0)
	continue;

      if((tda = dvb_adapter_find_by_identifier(f->hmf_name)) == NULL) {
	/* Not discovered by hardware, create it */

	tda = tda_alloc();
	tda->tda_identifier = strdup(f->hmf_name);
	tda->tda_type = type;
	TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);
      } else {
	if(type != tda->tda_type)
	  continue; /* Something is wrong, ignore */
      }

      free(tda->tda_displayname);
      tda->tda_displayname = strdup(name);

      htsmsg_get_u32(c, "autodiscovery", &tda->tda_autodiscovery);
      htsmsg_get_u32(c, "idlescan", &tda->tda_idlescan);
      htsmsg_get_u32(c, "logging", &tda->tda_logging);
    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    dvb_mux_load(tda);
}


/**
 * If nobody is subscribing, cycle thru all muxes to get some stats
 * and EIT updates
 */
void
dvb_adapter_mux_scanner(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;
  int i;

  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 20);

  if(LIST_FIRST(&tda->tda_muxes) == NULL)
    return; // No muxes configured

  if(transport_compute_weight(&tda->tda_transports) > 0)
    return; /* someone is here */

  /* Check if we have muxes pending for quickscan, if so, choose them */
  if((tdmi = TAILQ_FIRST(&tda->tda_initial_scan_queue)) != NULL) {
    dvb_fe_tune(tdmi, "Initial autoscan");
    return;
  }

  if(!tda->tda_idlescan && TAILQ_FIRST(&tda->tda_scan_queues[0]) == NULL) {
    /* Idlescan is disabled and no muxes are bad.
       If the currently tuned mux is ok, we can stick to it */
    
    tdmi = tda->tda_mux_current;

    if(tdmi != NULL && tdmi->tdmi_quality > 90)
      return;
  }

  /* Alternate between the other two (bad and OK) */
  for(i = 0; i < 2; i++) {
    tda->tda_scan_selector = !tda->tda_scan_selector;
    tdmi = TAILQ_FIRST(&tda->tda_scan_queues[tda->tda_scan_selector]);
    if(tdmi != NULL) {
      dvb_fe_tune(tdmi, "Autoscan");
      return;
    }
  }
}

/**
 * 
 */
void
dvb_adapter_clone(th_dvb_adapter_t *dst, th_dvb_adapter_t *src)
{
  th_dvb_mux_instance_t *tdmi_src, *tdmi_dst;
  th_transport_t *t_src, *t_dst;
  th_stream_t *st_src, *st_dst;

  lock_assert(&global_lock);

  while((tdmi_dst = LIST_FIRST(&dst->tda_muxes)) != NULL)
    dvb_mux_destroy(tdmi_dst);

  LIST_FOREACH(tdmi_src, &src->tda_muxes, tdmi_adapter_link) {

    tdmi_dst = dvb_mux_create(dst, 
			      &tdmi_src->tdmi_fe_params,
			      tdmi_src->tdmi_polarisation, 
			      tdmi_src->tdmi_satconf,
			      tdmi_src->tdmi_transport_stream_id,
			      tdmi_src->tdmi_network,
			      "copy operation", tdmi_src->tdmi_enabled,
			      NULL);


    assert(tdmi_dst != NULL);

    LIST_FOREACH(t_src, &tdmi_src->tdmi_transports, tht_mux_link) {
      t_dst = dvb_transport_find(tdmi_dst, 
				 t_src->tht_dvb_service_id,
				 t_src->tht_pmt_pid, NULL);

      t_dst->tht_pcr_pid     = t_src->tht_pcr_pid;
      t_dst->tht_enabled     = t_src->tht_enabled;
      t_dst->tht_servicetype = t_src->tht_servicetype;
      t_dst->tht_scrambled   = t_src->tht_scrambled;

      if(t_src->tht_provider != NULL)
	t_dst->tht_provider    = strdup(t_src->tht_provider);

      if(t_src->tht_svcname != NULL)
	t_dst->tht_svcname = strdup(t_src->tht_svcname);

      if(t_src->tht_ch != NULL)
	transport_map_channel(t_dst, t_src->tht_ch, 0);

      pthread_mutex_lock(&t_src->tht_stream_mutex);

      LIST_FOREACH(st_src, &t_src->tht_components, st_link) {

	st_dst = transport_stream_create(t_dst, 
					 st_src->st_pid,
					 st_src->st_type);
	
	st_dst->st_tb = (AVRational){1, 90000};
	
	memcpy(st_dst->st_lang, st_src->st_lang, 4);
	st_dst->st_frame_duration = st_src->st_frame_duration;
	st_dst->st_caid           = st_src->st_caid;
      }

      t_dst->tht_config_change(t_dst); // Save config
      pthread_mutex_unlock(&t_src->tht_stream_mutex);

    }
    dvb_mux_save(tdmi_dst);
  }
  tda_save(dst);
}

/**
 * 
 */
int
dvb_adapter_destroy(th_dvb_adapter_t *tda)
{
  th_dvb_mux_instance_t *tdmi;

  if(tda->tda_rootpath != NULL)
    return -1;

  lock_assert(&global_lock);

  hts_settings_remove("dvbadapters/%s", tda->tda_identifier);

  while((tdmi = LIST_FIRST(&tda->tda_muxes)) != NULL)
    dvb_mux_destroy(tdmi);
  
  TAILQ_REMOVE(&dvb_adapters, tda, tda_global_link);

  free(tda->tda_identifier);
  free(tda->tda_displayname);

  free(tda);

  return 0;
}


/**
 *
 */
void
dvb_adapter_clean(th_dvb_adapter_t *tda)
{
  th_transport_t *t;
  
  lock_assert(&global_lock);

  while((t = LIST_FIRST(&tda->tda_transports)) != NULL)
    transport_remove_subscriber(t, NULL); /* Flushes all subscribers */
}



/**
 *
 */
static void *
dvb_adapter_input_dvr(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  int fd, i, r;
  uint8_t tsb[188 * 10];
  th_transport_t *t;

  fd = open(tda->tda_dvr_path, O_RDONLY);
  if(fd == -1) {
    tvhlog(LOG_ALERT, "dvb", "%s: unable to open dvr", tda->tda_dvr_path);
    return NULL;
  }

  while(1) {
    r = read(fd, tsb, sizeof(tsb));

    pthread_mutex_lock(&tda->tda_delivery_mutex);
    
    for(i = 0; i < r; i += 188)
      LIST_FOREACH(t, &tda->tda_transports, tht_active_link)
	if(t->tht_dvb_mux_instance == tda->tda_mux_current)
	  ts_recv_packet1(t, tsb + i);

    pthread_mutex_unlock(&tda->tda_delivery_mutex);
  }
}

/**
 *
 */
htsmsg_t *
dvb_adapter_build_msg(th_dvb_adapter_t *tda)
{
  char buf[100];
  htsmsg_t *m = htsmsg_create_map();
  th_dvb_mux_instance_t *tdmi;
  th_transport_t *t;

  int nummux = 0;
  int numsvc = 0;

  htsmsg_add_str(m, "identifier", tda->tda_identifier);
  htsmsg_add_str(m, "name", tda->tda_displayname);
  htsmsg_add_str(m, "path", tda->tda_rootpath);
  htsmsg_add_str(m, "devicename", tda->tda_fe_info->name);

  // XXX: bad bad bad slow slow slow
  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
    nummux++;
    LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
      numsvc++;
    }
  }

  htsmsg_add_u32(m, "services", numsvc);
  htsmsg_add_u32(m, "muxes", nummux);
  htsmsg_add_u32(m, "initialMuxes", tda->tda_initial_num_mux);

  if(tda->tda_mux_current != NULL) {
    dvb_mux_nicename(buf, sizeof(buf), tda->tda_mux_current);
    htsmsg_add_str(m, "currentMux", buf);
  }

  htsmsg_add_u32(m, "satConf", tda->tda_sat);
  return m;
}


/**
 *
 */
void
dvb_adapter_notify(th_dvb_adapter_t *tda)
{
  notify_by_msg("dvbAdapter", dvb_adapter_build_msg(tda));
}
