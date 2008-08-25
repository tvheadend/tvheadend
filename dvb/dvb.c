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
#include "dvb_muxconfig.h"
#include "notify.h"

struct th_dvb_adapter_queue dvb_adapters;
struct th_dvb_mux_instance_tree dvb_muxes;
static void dvb_mux_scanner(void *aux, int64_t now);
static void dvb_fec_monitor(void *aux, int64_t now);

static void dvb_transport_save(th_transport_t *t);
static const char *dvb_source_name(th_transport_t *t);
static int dvb_transport_quality(th_transport_t *t);


static th_dvb_adapter_t *
tda_alloc(void)
{
  th_dvb_adapter_t *tda = calloc(1, sizeof(th_dvb_adapter_t));
  pthread_mutex_init(&tda->tda_lock, NULL);
  pthread_cond_init(&tda->tda_cond, NULL);
  TAILQ_INIT(&tda->tda_fe_cmd_queue);
  return tda;
}



static void
dvb_add_adapter(const char *path)
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


  /* Come up with an initial displayname, user can change it and it will
     be overridden by any stored settings later on */

  tda->tda_displayname = strdup(tda->tda_fe_info->name);

  tvhlog(LOG_INFO, "dvb",
	 "Found adapter %s (%s)", path, tda->tda_fe_info->name);

  TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);
  dtimer_arm(&tda->tda_fec_monitor_timer, dvb_fec_monitor, tda, 1);
  dvb_fe_start(tda);
}




void
dvb_init(void)
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
    dvb_add_adapter(path);
  }

  l = hts_settings_load("dvbadapters");
  if(l != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
	continue;
      
      if(dvb_adapter_find_by_identifier(f->hmf_name) != NULL) {
	/* Already loaded */
	continue;
      }

      if((name = htsmsg_get_str(c, "displayname")) == NULL)
	continue;

      if((s = htsmsg_get_str(c, "type")) == NULL ||
	 (type = dvb_str_to_adaptertype(s)) < 0)
	continue;

      tda = tda_alloc();
      tda->tda_identifier = strdup(f->hmf_name);
      tda->tda_displayname = strdup(name);
      tda->tda_type = type;

      TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);

    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    dvb_tdmi_load(tda);

}



/**
 * Find a transport based on 'serviceid' on the given mux
 *
 * If it cannot be found we create it if 'pmt_pid' is also set
 */
th_transport_t *
dvb_find_transport(th_dvb_mux_instance_t *tdmi, uint16_t sid, int pmt_pid,
		   int *created)
{
  th_transport_t *t;
  char tmp[200];

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
  t->tht_pmt            = pmt_pid;

  t->tht_start_feed = dvb_start_feed;
  t->tht_stop_feed  = dvb_stop_feed;
  t->tht_config_change = dvb_transport_save;
  t->tht_sourcename = dvb_source_name;
  t->tht_dvb_mux_instance = tdmi;
  t->tht_quality_index = dvb_transport_quality;

  LIST_INSERT_HEAD(&tdmi->tdmi_transports, t, tht_mux_link);
  return t;
}


/**
 *
 */
static void
dvb_notify_mux_quality(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);

  htsmsg_add_u32(m, "quality", 100 + tdmi->tdmi_quality * 2);
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
static void
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
  int i, v, vv;
  const char *s;

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
  

  if(dvb_mux_status(tdmi, 1) != NULL) {
    if(tdmi->tdmi_quality > -50) {
      tdmi->tdmi_quality--;
      dvb_notify_mux_quality(tdmi);
    }
  } else {

    if(tdmi->tdmi_quality < 0) {
      tdmi->tdmi_quality++;
      dvb_notify_mux_quality(tdmi);
    }
  }

  s = dvb_mux_status(tdmi, 0);

  if(s != tdmi->tdmi_last_status) {
    tdmi->tdmi_last_status = s;
    dvb_notify_mux_status(tdmi);
  }
}

/**
 * If nobody is subscribing, cycle thru all muxes to get some stats
 * and EIT updates
 */
static void
dvb_mux_scanner(void *aux, int64_t now)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;

  dtimer_arm(&tda->tda_mux_scanner_timer, dvb_mux_scanner, tda, 10);

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
static int
tdmi_cmp(th_dvb_mux_instance_t *a, th_dvb_mux_instance_t *b)
{
  if(a->tdmi_switchport != b->tdmi_switchport)
    return a->tdmi_switchport - b->tdmi_switchport;

  if(a->tdmi_fe_params.frequency != b->tdmi_fe_params.frequency)
    return a->tdmi_fe_params.frequency - b->tdmi_fe_params.frequency;

  return a->tdmi_polarisation - b->tdmi_polarisation;
}

/**
 *
 */
static int
tdmi_global_cmp(th_dvb_mux_instance_t *a, th_dvb_mux_instance_t *b)
{
  return strcmp(a->tdmi_identifier, b->tdmi_identifier);
}


/**
 * Create a new mux on the given adapter, return NULL if it already exists
 */
th_dvb_mux_instance_t *
dvb_mux_create(th_dvb_adapter_t *tda, struct dvb_frontend_parameters *fe_param,
	       int polarisation, int switchport,
	       uint16_t tsid, const char *network, const char *source)
{
  th_dvb_mux_instance_t *tdmi;
  static th_dvb_mux_instance_t *skel;
  char buf[200];
  char qpsktxt[20];
  int entries_before = tda->tda_muxes.entries;

  if(skel == NULL)
    skel = calloc(1, sizeof(th_dvb_mux_instance_t));

  skel->tdmi_polarisation = polarisation;
  skel->tdmi_switchport = switchport;
  skel->tdmi_fe_params.frequency = fe_param->frequency;

  tdmi = RB_INSERT_SORTED(&tda->tda_muxes, skel, tdmi_adapter_link, tdmi_cmp);
  if(tdmi != NULL)
    return NULL;

  tdmi = skel;
  skel = NULL;

  tdmi->tdmi_refcnt = 1;

  RB_INSERT_SORTED(&tda->tda_muxes_qscan_waiting, tdmi, tdmi_qscan_link,
		   tdmi_cmp);

  tdmi->tdmi_quickscan = TDMI_QUICKSCAN_WAITING;

  pthread_mutex_init(&tdmi->tdmi_table_lock, NULL);
  tdmi->tdmi_state = TDMI_IDLE;
  tdmi->tdmi_transport_stream_id = tsid;
  tdmi->tdmi_adapter = tda;
  tdmi->tdmi_network = network ? strdup(network) : NULL;

  if(entries_before == 0 && tda->tda_rootpath != NULL) {
    /* First mux on adapter with backing hardware, start scanner */
    dtimer_arm(&tda->tda_mux_scanner_timer, dvb_mux_scanner, tda, 1);
  }

  memcpy(&tdmi->tdmi_fe_params, fe_param, 
	 sizeof(struct dvb_frontend_parameters));

  if(tda->tda_type == FE_QPSK)
    snprintf(qpsktxt, sizeof(qpsktxt), "_%s_%d",
	     dvb_polarisation_to_str(polarisation), switchport);
  else
    qpsktxt[0] = 0;

  snprintf(buf, sizeof(buf), "%s%d%s", 
	   tda->tda_identifier,fe_param->frequency, qpsktxt);

  tdmi->tdmi_identifier = strdup(buf);

  RB_INSERT_SORTED(&dvb_muxes, tdmi, tdmi_global_link, tdmi_global_cmp);

  if(source != NULL) {
    dvb_mux_nicename(buf, sizeof(buf), tdmi);
    tvhlog(LOG_NOTICE, "dvb", "New mux \"%s\" created by %s", buf, source);

    dvb_tdmi_save(tdmi);
    dvb_adapter_notify_reload(tda);
  }

  return tdmi;
}

/**
 * Unref a TDMI and optionally free it
 */
void
dvb_mux_unref(th_dvb_mux_instance_t *tdmi)
{
  if(tdmi->tdmi_refcnt > 1) {
    tdmi->tdmi_refcnt--;
    return;
  }

  free(tdmi->tdmi_network);
  free(tdmi->tdmi_identifier);
  free(tdmi);
}

/**
 * Destroy a DVB mux (it might come back by itself very soon though :)
 */
void
dvb_mux_destroy(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  th_transport_t *t;
  char buf[400];

  snprintf(buf, sizeof(buf), "%s/dvbmuxes/%s",
	   settings_dir, tdmi->tdmi_identifier);
  unlink(buf);

  while((t = LIST_FIRST(&tdmi->tdmi_transports)) != NULL)
    transport_destroy(t);

  if(tda->tda_mux_current == tdmi)
    tdmi_stop(tda->tda_mux_current);

  dtimer_disarm(&tdmi->tdmi_initial_scan_timer);
  RB_REMOVE(&dvb_muxes, tdmi, tdmi_global_link);
  RB_REMOVE(&tda->tda_muxes, tdmi, tdmi_adapter_link);

  if(tdmi->tdmi_quickscan == TDMI_QUICKSCAN_WAITING)
    RB_REMOVE(&tda->tda_muxes_qscan_waiting, tdmi, tdmi_qscan_link);

  hts_settings_remove("dvbmuxes/%s", tdmi->tdmi_identifier);

  pthread_mutex_lock(&tda->tda_lock);
  dvb_fe_flush(tdmi);
  dvb_mux_unref(tdmi);
  pthread_mutex_unlock(&tda->tda_lock);
}




#if 0

/**
 * Load config for the given adapter
 */
static int
dvb_tda_load(th_dvb_adapter_t *tda)
{
  struct config_head cl;
  config_entry_t *ce;
  char buf[400];
  const char *v;

  snprintf(buf, sizeof(buf), "%s/dvbadapters/%s",
	   settings_dir, tda->tda_identifier);

  TAILQ_INIT(&cl);
  if(config_read_file0(buf, &cl))
    return 0;

  if((v = config_get_str_sub(&cl, "type", NULL)) == NULL)
    goto err;
  
  if((tda->tda_type = dvb_str_to_adaptertype(v)) < 0)
    goto err;

  if((v = config_get_str_sub(&cl, "displayname", NULL)) == NULL)
    goto err;

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(v);

  TAILQ_FOREACH(ce, &cl, ce_link) {
    if(ce->ce_type != CFG_SUB || strcasecmp("mux", ce->ce_key))
      continue;

    v = dvb_mux_create_str(tda,
			   config_get_str_sub(&ce->ce_sub,
					      "transportstreamid", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "network", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "frequency", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "symbol_rate", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "constellation", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "fec", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "fec_hi", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "fec_lo", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "bandwidth", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "transmission_mode", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "guard_interval", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "hierarchy", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "polarisation", NULL),
			   config_get_str_sub(&ce->ce_sub,
					      "switchport", NULL),
			   0);

    if(v != NULL)
      tvhlog(LOG_ALERT, "dvb",
	     "Unable to init saved mux on %s -- %s\n",
	     tda->tda_identifier, v);
  }
  config_free0(&cl);
  return 0;
 err:
  config_free0(&cl);
  return -1;
}
#endif

static void
dvb_transport_save(th_transport_t *t)
{
  htsmsg_t *m = htsmsg_create();


  htsmsg_add_u32(m, "service_id", t->tht_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->tht_pmt);
  htsmsg_add_u32(m, "stype", t->tht_servicetype);
  htsmsg_add_u32(m, "scrambled", t->tht_scrambled);

  if(t->tht_provider != NULL)
    htsmsg_add_str(m, "provider", t->tht_provider);

  if(t->tht_svcname != NULL)
    htsmsg_add_str(m, "servicename", t->tht_svcname);

  if(t->tht_chname != NULL)
    htsmsg_add_str(m, "channelname", t->tht_chname);

  htsmsg_add_u32(m, "mapped", !!t->tht_ch);
  
  psi_get_transport_settings(m, t);
  
  hts_settings_save(m, "dvbtransports/%s", t->tht_identifier);
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
    return tdmi->tdmi_quality;
}


/**
 * Generate a descriptive name for the source
 */
static const char *
dvb_source_name(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi;
  static char buf[200];

  tdmi = t->tht_dvb_mux_instance;

  snprintf(buf, sizeof(buf), "\"%s\" on \"%s\"",
	   tdmi->tdmi_network ?: "Unknown network",
	   tdmi->tdmi_adapter->tda_rootpath);

  return buf;
}

/**
 * 
 */
void
dvb_tda_clone(th_dvb_adapter_t *dst, th_dvb_adapter_t *src)
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
      t_dst = dvb_find_transport(tdmi_dst, 
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
    dvb_tdmi_save(tdmi_dst);
  }
  dvb_tda_save(dst);
}

/**
 * 
 */
int
dvb_tda_destroy(th_dvb_adapter_t *tda)
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

/**
 *
 */
void
dvb_tdmi_fastswitch(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_table_t *tdt;

  if(tdmi->tdmi_quickscan == TDMI_QUICKSCAN_NONE)
    return;

  pthread_mutex_lock(&tdmi->tdmi_table_lock);
  LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link) {
    if(tdt->tdt_quickreq && tdt->tdt_count == 0)
      break;
  }
  pthread_mutex_unlock(&tdmi->tdmi_table_lock);

  if(tdt != NULL)
    return; /* Still tables we've not seen */

  tdmi->tdmi_quickscan = TDMI_QUICKSCAN_NONE;
  dvb_mux_scanner(tdmi->tdmi_adapter, 0);

}


/**
 *
 */
th_dvb_mux_instance_t *
dvb_mux_find_by_identifier(const char *identifier)
{
  th_dvb_mux_instance_t skel;

  skel.tdmi_identifier = (char *)identifier;
  return RB_FIND(&dvb_muxes, &skel, tdmi_global_link, tdmi_global_cmp);
}

