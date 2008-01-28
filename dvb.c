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

#include <libhts/htscfg.h>
#include <ffmpeg/avstring.h>

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

struct th_dvb_mux_list dvb_muxes;
struct th_dvb_adapter_list dvb_adapters_probing;
struct th_dvb_adapter_list dvb_adapters_running;

static void dvb_start_initial_scan(th_dvb_mux_instance_t *tdmi);
static void tdmi_activate(th_dvb_mux_instance_t *tdmi);
static void dvb_mux_scanner(void *aux, int64_t now);
static void dvb_fec_monitor(void *aux, int64_t now);



static void
dvb_add_adapter(const char *path)
{
  char fname[256];
  int fe, i, r;
  th_dvb_adapter_t *tda, *x;
  char c;
  char buf[400], *cp;

  snprintf(fname, sizeof(fname), "%s/frontend0", path);
  
  fe = open(fname, O_RDWR | O_NONBLOCK);
  if(fe == -1) {
    if(errno != ENOENT)
      syslog(LOG_ALERT, "Unable to open %s -- %s\n", fname, strerror(errno));
    return;
  }
  tda = calloc(1, sizeof(th_dvb_adapter_t));
  tda->tda_rootpath = strdup(path);
  tda->tda_demux_path = malloc(256);
  snprintf(tda->tda_demux_path, 256, "%s/demux0", path);
  tda->tda_dvr_path = malloc(256);
  snprintf(tda->tda_dvr_path, 256, "%s/dvr0", path);


  tda->tda_fe_fd = fe;

  tda->tda_fe_info = malloc(sizeof(struct dvb_frontend_info));

  if(ioctl(tda->tda_fe_fd, FE_GET_INFO, tda->tda_fe_info)) {
    syslog(LOG_ALERT, "%s: Unable to query adapter\n", fname);
    close(fe);
    free(tda);
    return;
  }

  if(dvb_dvr_init(tda) < 0) {
    close(fe);
    free(tda);
    return;
  }
  
  pthread_mutex_init(&tda->tda_lock, NULL);
  pthread_cond_init(&tda->tda_cond, NULL);
  TAILQ_INIT(&tda->tda_fe_cmd_queue);

  startupcounter++;

  tda->tda_info = strdup(tda->tda_fe_info->name);

  /*
   * Generate a decent unique name for the adapter.
   * If multiple adapters with the same name is found, we add
   * a sequence number at the end
   */

  for(i = 0; i < strlen(tda->tda_info); i++) {
    c = tolower(tda->tda_info[i]);
    if(isalnum(c))
      buf[i] = c;
    else
      buf[i] = '_';
  }
  cp = &buf[i];

  r = 0;

 again:
  if(r)
    sprintf(cp, "-%d", r);
  else
    *cp = 0;
  
  LIST_FOREACH(x, &dvb_adapters_probing, tda_link) {
    if(!strcmp(buf, x->tda_sname)) {
      r++;
      goto again;
    }
  }

  tda->tda_sname = strdup(buf);

  LIST_INSERT_HEAD(&dvb_adapters_probing, tda, tda_link);

  syslog(LOG_INFO, "Adding adapter %s (%s)", path, tda->tda_fe_info->name);
  dtimer_arm(&tda->tda_fec_monitor_timer, dvb_fec_monitor, tda, 1);

  dvb_fe_start(tda);
}




void
dvb_init(void)
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  char path[200];
  int i;

  for(i = 0; i < 32; i++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d", i);
    dvb_add_adapter(path);
  }

  dvb_mux_setup();

  LIST_FOREACH(tda, &dvb_adapters_probing, tda_link) {
    tdmi = LIST_FIRST(&tda->tda_muxes_configured);
    if(tdmi == NULL) {
      syslog(LOG_WARNING,
	     "No muxes configured on \"%s\" DVB adapter unused",
	     tda->tda_rootpath);
      startupcounter--;
    } else {
      dvb_start_initial_scan(tdmi);
    }
  }
}



/**
 * Based on the gived transport id and service id on the given mux
 * try to locate the transport.
 *
 * If it cannot be found we create it
 */
th_transport_t *
dvb_find_transport(th_dvb_mux_instance_t *tdmi, uint16_t tid,
		   uint16_t sid, int pmt_pid)
{
  th_transport_t *t;
  char tmp[100];

  /* XXX: Minimize this search */

  LIST_FOREACH(t, &all_transports, tht_global_link) {
    if(t->tht_dvb_mux_instance == tdmi &&
       t->tht_dvb_transport_id == tid &&
       t->tht_dvb_service_id   == sid)
      return t;
  }
  
  if(pmt_pid == 0)
    return NULL;

  t = calloc(1, sizeof(th_transport_t));
  transport_init(t, THT_MPEG_TS);

  t->tht_dvb_transport_id = tid;
  t->tht_dvb_service_id   = sid;

  t->tht_type = TRANSPORT_DVB;
  t->tht_start_feed = dvb_start_feed;
  t->tht_stop_feed  = dvb_stop_feed;
  t->tht_dvb_mux_instance = tdmi;

  snprintf(tmp, sizeof(tmp), "%s/%04x", tdmi->tdmi_uniquename, sid);
  free((void *)t->tht_uniquename);
  t->tht_uniquename = strdup(tmp);

  snprintf(tmp, sizeof(tmp), "%s/%04x", tdmi->tdmi_shortname, sid);
  free((void *)t->tht_name);
  t->tht_name = strdup(tmp);

  LIST_INSERT_HEAD(&all_transports, t, tht_global_link);

  dvb_table_add_transport(tdmi, t, pmt_pid);
  return t;
}


/**
 *
 */
static void
tdmi_activate(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  dtimer_disarm(&tdmi->tdmi_initial_scan_timer);

  tdmi->tdmi_state = TDMI_IDLE;
  
  LIST_REMOVE(tdmi, tdmi_adapter_link);
  LIST_INSERT_HEAD(&tda->tda_muxes_active, tdmi, tdmi_adapter_link);

  /* tune to next configured (but not yet active) mux */

  tdmi = LIST_FIRST(&tda->tda_muxes_configured);
  
  if(tdmi == NULL) {
    startupcounter--;
    syslog(LOG_INFO,
	   "\"%s\" Initial scan completed, adapter available",
	   tda->tda_rootpath);
    /* no more muxes to probe, link adapter to the world */
    LIST_REMOVE(tda, tda_link);
    LIST_INSERT_HEAD(&dvb_adapters_running, tda, tda_link);
    dtimer_arm(&tda->tda_mux_scanner_timer, dvb_mux_scanner, tda, 10);
    return;
  }
  dvb_start_initial_scan(tdmi);
}


/**
 *
 */
static void
tdmi_initial_scan_timeout(void *aux, int64_t now)
{
  th_dvb_mux_instance_t *tdmi = aux;
  const char *err;
#if 0
  th_dvb_table_t *tdt;
  LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link) {
    printf("%s: %d\n", tdt->tdt_name, tdt->tdt_count);
  }
#endif
  dtimer_disarm(&tdmi->tdmi_initial_scan_timer);

  if(tdmi->tdmi_status != NULL)
    err = tdmi->tdmi_status;
  else
    err = "Missing PSI tables, scan will continue";

  syslog(LOG_DEBUG, "\"%s\" Initial scan timed out -- %s",
	 tdmi->tdmi_uniquename, err);

  tdmi_activate(tdmi);
}


/**
 *
 */
void
tdmi_check_scan_status(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_table_t *tdt;

  if(tdmi->tdmi_state >= TDMI_IDLE)
    return;
  
  LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link)
    if(tdt->tdt_count == 0)
      return;

  /* All tables seen at least once */

  syslog(LOG_DEBUG, "\"%s\" Initial scan completed",
	 tdmi->tdmi_uniquename);

  tdmi_activate(tdmi);
}


/**
 *
 */
static void
dvb_start_initial_scan(th_dvb_mux_instance_t *tdmi)
{
  dvb_tune_tdmi(tdmi, 1, TDMI_INITIAL_SCAN);

  dtimer_arm(&tdmi->tdmi_initial_scan_timer,
	     tdmi_initial_scan_timeout, tdmi, 5);

}

/**
 *
 */
static void
dvb_fec_monitor(void *aux, int64_t now)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;

  dtimer_arm(&tda->tda_fec_monitor_timer, dvb_fec_monitor, tda, 1);

  tdmi = tda->tda_mux_current;

  if(tdmi != NULL && tdmi->tdmi_status == NULL) {

    if(tdmi->tdmi_fec_err_per_sec > DVB_FEC_ERROR_LIMIT) {

      if(LIST_FIRST(&tda->tda_transports) != NULL) {
	syslog(LOG_ERR, "\"%s\": Too many FEC errors (%d / s), "
	       "flushing subscribers\n", 
	       tdmi->tdmi_uniquename, tdmi->tdmi_fec_err_per_sec);
	dvb_adapter_clean(tdmi->tdmi_adapter);
      }
    }
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

  tdmi = tda->tda_mux_current;
  tdmi = tdmi != NULL ? LIST_NEXT(tdmi, tdmi_adapter_link) : NULL;
  tdmi = tdmi != NULL ? tdmi : LIST_FIRST(&tda->tda_muxes_active);

  if(tdmi == NULL)
    return; /* no instances */

  dvb_tune_tdmi(tdmi, 0, TDMI_IDLESCAN);
}
