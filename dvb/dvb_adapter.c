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

#include <libhts/htssettings.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb.h"
#include "channels.h"
#include "transports.h"
#include "subscriptions.h"
#include "teletext.h"
#include "epg.h"
#include "psi.h"
#include "dvb_support.h"
#include "dvb_dvr.h"
#include "notify.h"

struct th_dvb_adapter_queue dvb_adapters;
struct th_dvb_mux_instance_tree dvb_muxes;
static void dvb_fec_monitor(void *aux, int64_t now);


/**
 *
 */
static th_dvb_adapter_t *
tda_alloc(void)
{
  th_dvb_adapter_t *tda = calloc(1, sizeof(th_dvb_adapter_t));
  pthread_mutex_init(&tda->tda_lock, NULL);
  pthread_cond_init(&tda->tda_cond, NULL);
  TAILQ_INIT(&tda->tda_fe_cmd_queue);
  return tda;
}


/**
 * Save config for the given adapter
 */
static void
tda_save(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_str(m, "type", dvb_adaptertype_to_str(tda->tda_type));
  htsmsg_add_str(m, "displayname", tda->tda_displayname);
  htsmsg_add_u32(m, "autodiscovery", tda->tda_autodiscovery);
  hts_settings_save(m, "dvbadapters/%s", tda->tda_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
void
dvb_adapter_set_displayname(th_dvb_adapter_t *tda, const char *s)
{
  htsmsg_t *m;

  if(!strcmp(s, tda->tda_displayname))
    return;

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" renamed to \"%s\"",
	 tda->tda_displayname, s);

  m = htsmsg_create();
  htsmsg_add_str(m, "id", tda->tda_identifier);

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(s);
  
  tda_save(tda);

  htsmsg_add_str(m, "name", tda->tda_displayname);

  notify_by_msg("dvbadapter", m);
}


/**
 *
 */
void
dvb_adapter_set_auto_discovery(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_autodiscovery == on)
    return;

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" mux autodiscovery set to %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_autodiscovery = on;
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

  if(dvb_dvr_init(tda) < 0) {
    close(fe);
    free(tda);
    return;
  }
  
  snprintf(buf, sizeof(buf), "%s_%s", tda->tda_rootpath,
	   tda->tda_fe_info->name);

  r = strlen(buf);
  for(i = 0; i < r; i++)
    if(!isalnum((int)buf[i]))
      buf[i] = '_';

  tda->tda_identifier = strdup(buf);
  
  tda->tda_autodiscovery = tda->tda_type != FE_QPSK;

  /* Come up with an initial displayname, user can change it and it will
     be overridden by any stored settings later on */

  tda->tda_displayname = strdup(tda->tda_fe_info->name);

  tvhlog(LOG_INFO, "dvb",
	 "Found adapter %s (%s)", path, tda->tda_fe_info->name);

  TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);
  dtimer_arm(&tda->tda_fec_monitor_timer, dvb_fec_monitor, tda, 1);
  dvb_fe_start(tda);
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
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
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
    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    dvb_mux_load(tda);
}


/**
 *
 */
static void
dvb_notify_mux_quality(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);

  htsmsg_add_u32(m, "quality", tdmi->tdmi_quality);
  notify_by_msg("dvbmux", m);
}


/**
 *
 */
static void
dvb_notify_mux_status(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);

  htsmsg_add_str(m, "status", tdmi->tdmi_last_status);
  notify_by_msg("dvbmux", m);
}


/**
 *
 */
void
dvb_adapter_notify_reload(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "id", tda->tda_identifier);

  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("dvbadapter", m);
}


/**
 *
 */
static void
dvb_fec_monitor(void *aux, int64_t now)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;
  int i, v, vv, n;
  const char *s;
  int savemux = 0;

  dtimer_arm(&tda->tda_fec_monitor_timer, dvb_fec_monitor, tda, 1);

  tdmi = tda->tda_mux_current;
  if(tdmi == NULL)
    return;
  if(tdmi->tdmi_status == NULL) {

    v = vv = 0;
    for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++) {
      if(tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT)
	v++;
      vv += tdmi->tdmi_fec_err_histogram[i];
    }
    vv /= TDMI_FEC_ERR_HISTOGRAM_SIZE;

    if(v == TDMI_FEC_ERR_HISTOGRAM_SIZE) {
      if(LIST_FIRST(&tda->tda_transports) != NULL) {
	tvhlog(LOG_ERR, "dvb",
	       "\"%s\": Constant rate of FEC errors (average at %d / s), "
	       "last %d seconds, flushing subscribers\n", 
	       tdmi->tdmi_identifier, vv,
	       TDMI_FEC_ERR_HISTOGRAM_SIZE);

	dvb_adapter_clean(tdmi->tdmi_adapter);
      }
    }
  }
  
  n = dvb_mux_badness(tdmi);
  if(n > 0) {
    i = MAX(tdmi->tdmi_quality - n, 0);
    if(i != tdmi->tdmi_quality) {
      tdmi->tdmi_quality = i;
      dvb_notify_mux_quality(tdmi);
      savemux = 1;
    }
  } else {

    if(tdmi->tdmi_quality < 100) {
      tdmi->tdmi_quality++;
      dvb_notify_mux_quality(tdmi);
      savemux = 1;
    }
  }

  s = dvb_mux_status(tdmi);
  if(strcmp(s, tdmi->tdmi_last_status)) {
    free(tdmi->tdmi_last_status);
    tdmi->tdmi_last_status = strdup(s);
    dvb_notify_mux_status(tdmi);
    savemux = 1;
  }

  if(savemux)
    dvb_mux_save(tdmi);
}


/**
 * If nobody is subscribing, cycle thru all muxes to get some stats
 * and EIT updates
 */
void
dvb_adapter_mux_scanner(void *aux, int64_t now)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;

  dtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 10);

  if(transport_compute_weight(&tda->tda_transports) > 0)
    return; /* someone is here */

  /* Check if we have muxes pending for quickscan, if so, choose them */

  if((tdmi = RB_FIRST(&tda->tda_muxes_qscan_waiting)) != NULL) {
    RB_REMOVE(&tda->tda_muxes_qscan_waiting, tdmi, tdmi_qscan_link);
    tdmi->tdmi_quickscan = TDMI_QUICKSCAN_RUNNING;
    dvb_tune_tdmi(tdmi, 0, TDMI_IDLESCAN);
    return;
  }

  /* otherwise, just rotate */

  tdmi = tda->tda_mux_current;
  if(tdmi != NULL)
    tdmi->tdmi_quickscan = TDMI_QUICKSCAN_NONE;

  tdmi = tdmi != NULL ? RB_NEXT(tdmi, tdmi_adapter_link) : NULL;
  tdmi = tdmi != NULL ? tdmi : RB_FIRST(&tda->tda_muxes);

  if(tdmi == NULL)
    return; /* no instances */

  dvb_tune_tdmi(tdmi, 0, TDMI_IDLESCAN);
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

  while((tdmi_dst = RB_FIRST(&dst->tda_muxes)) != NULL)
    dvb_mux_destroy(tdmi_dst);

  RB_FOREACH(tdmi_src, &src->tda_muxes, tdmi_adapter_link) {

    tdmi_dst = dvb_mux_create(dst, 
			      &tdmi_src->tdmi_fe_params,
			      tdmi_src->tdmi_polarisation, 
			      tdmi_src->tdmi_switchport,
			      tdmi_src->tdmi_transport_stream_id,
			      tdmi_src->tdmi_network,
			      "copy operation");


    assert(tdmi_dst != NULL);

    LIST_FOREACH(t_src, &tdmi_src->tdmi_transports, tht_mux_link) {
      t_dst = dvb_transport_find(tdmi_dst, 
				 t_src->tht_dvb_service_id,
				 t_src->tht_pmt,
				 NULL);

      t_dst->tht_pcr_pid     = t_src->tht_pcr_pid;
      t_dst->tht_disabled    = t_src->tht_disabled;
      t_dst->tht_servicetype = t_src->tht_servicetype;
      t_dst->tht_scrambled   = t_src->tht_scrambled;

      if(t_src->tht_provider != NULL)
	t_dst->tht_provider    = strdup(t_src->tht_provider);

      if(t_src->tht_svcname != NULL)
	t_dst->tht_svcname = strdup(t_src->tht_svcname);

      if(t_src->tht_chname != NULL)
	t_dst->tht_chname = strdup(t_src->tht_chname);
      
      if(t_src->tht_ch != NULL)
	transport_map_channel(t_dst, t_src->tht_ch);

      

      LIST_FOREACH(st_src, &t_src->tht_streams, st_link) {

	st_dst = transport_add_stream(t_dst, 
				      st_src->st_pid,
				      st_src->st_type);
	
	st_dst->st_tb = (AVRational){1, 90000};
	
	memcpy(st_dst->st_lang,     st_src->st_lang, 4);
	st_dst->st_frame_duration = st_src->st_frame_duration;
	st_dst->st_caid           = st_src->st_caid;
      }
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

  char buf[400];

  if(tda->tda_rootpath != NULL)
    return -1;

  snprintf(buf, sizeof(buf), "%s/dvbadapters/%s",
	   settings_dir, tda->tda_identifier);

  unlink(buf);

  while((tdmi = RB_FIRST(&tda->tda_muxes)) != NULL)
    dvb_mux_destroy(tdmi);
  
  TAILQ_REMOVE(&dvb_adapters, tda, tda_global_link);

  free(tda->tda_identifier);
  free(tda->tda_displayname);

  free(tda);

  return 0;
}

