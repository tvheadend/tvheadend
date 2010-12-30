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
#include <limits.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "settings.h"
#include "tvheadend.h"
#include "dvb.h"
#include "dvb_support.h"
#include "tsdemux.h"
#include "notify.h"
#include "service.h"
#include "device.h"

static deviceclass_t dvb_device_class;

struct th_dvb_adapter_queue dvb_adapters;
struct th_dvb_mux_instance_tree dvb_muxes;



/**
 * Save config for the given adapter
 */
static htsmsg_t *
dvb_adapter_get_conf(device_t *d)
{
  dvb_adapter_t *da = (dvb_adapter_t *)d;

  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "diseqc_version", da->da_diseqc_version);
  return m;
}




/**
 *
 */
void
dvb_adapter_set_diseqc_version(dvb_adapter_t *da, unsigned int v)
{
  if(v > 1)
    v = 1;

  if(da->da_diseqc_version == v)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "DVB", "Adapter \"%s\" DiSEqC version set to: %s",
	 da->da_dev.d_name, v ? "1.1 / 2.1" : "1.0 / 2.0" );

  da->da_diseqc_version = v;
  device_save(&da->da_dev);
}

#if 0

/**
 *
 */
static void __attribute__((unused))
tda_add(int adapter_num)
{
  char path[200];
  char fname[256];
  int fe, i, r;
  th_dvb_adapter_t *tda;
  char buf[400];
  pthread_t ptid;

  snprintf(path, sizeof(path), "/dev/dvb/adapter%d", adapter_num);
  snprintf(fname, sizeof(fname), "%s/frontend0", path);
  
  fe = tvh_open(fname, O_RDWR | O_NONBLOCK, 0);
  if(fe == -1) {
    if(errno != ENOENT)
      tvhlog(LOG_ALERT, "dvb",
	     "Unable to open %s -- %s", fname, strerror(errno));
    return;
  }

  tda = tda_alloc();

  tda->tda_adapter_num = adapter_num;
  tda->tda_rootpath = strdup(path);
  tda->tda_demux_path = malloc(256);
  snprintf(tda->tda_demux_path, 256, "%s/demux0", path);
  tda->tda_dvr_path = malloc(256);
  snprintf(tda->tda_dvr_path, 256, "%s/dvr0", path);


  tda->tda_fe_fd = fe;

  tda->tda_fe_info = malloc(sizeof(struct dvb_frontend_info));

  if(ioctl(tda->tda_fe_fd, FE_GET_INFO, tda->tda_fe_info)) {
    tvhlog(LOG_ALERT, "DVB", "%s: Unable to query adapter", fname);
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

  dvb_adapter_checkspeed(tda);

  tvhlog(LOG_INFO, "dvb",
	 "Found adapter %s (%s) via %s", path, tda->tda_fe_info->name,
	 hostconnection2str(tda->tda_hostconnection));

  TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);

  pthread_create(&ptid, NULL, dvb_adapter_input_dvr, tda);

  dvb_table_init(tda);

  if(tda->tda_sat)
    dvb_satconf_init(tda);

  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 1);
}
#endif


/**
 *
 */
void
dvb_adapter_init(void)
{
  int i, j, fd;
  char path[PATH_MAX];
  char busid[PATH_MAX];
  struct dvb_frontend_info info;
  const char *type;
  dvb_adapter_t *da;

  for(i = 0; i < 32; i++) {
    for(j = 0; j < 32; j++) {

      snprintf(path, sizeof(path), 
	       "/sys/class/dvb/dvb%d.frontend%d/device", i, j);

      if(realpath(path, busid) == NULL)
	continue;

      snprintf(path, sizeof(path), "/dev/dvb/adapter%d/frontend%d", i, j);
      fd = tvh_open(path, O_RDONLY | O_NONBLOCK, 0);
      if(fd == -1) {
	if(errno != ENOENT)
	  tvhlog(LOG_ALERT, "dvb",
		 "Unable to open %s -- %s", path, strerror(errno));
	break;
      }
      
      if(ioctl(fd, FE_GET_INFO, &info)) {
	tvhlog(LOG_ALERT, "DVB", "%s: Unable to query adapter", path);
	close(fd);
	continue;
      }

      switch(info.type) {
      case FE_QPSK: type = "DVB/QPSK"; break;
      case FE_QAM:  type = "DVB/QAM";  break;
      case FE_OFDM: type = "DVB/OFDM"; break;
      case FE_ATSC: type = "DVB/ATSC"; break;
      default:
	type = NULL;
	continue;
      }

      close(fd);

      if(type == NULL) {
	tvhlog(LOG_ALERT, "DVB", "%s: Unable to determine adapter type", path);
	continue;
      }
      
      da = device_add(path, busid, info.name, type, &dvb_device_class);
      
      snprintf(path, sizeof(path), "/dev/dvb/adapter%d/demux%d", i, j);
      da->da_demux_path = strdup(path);

      snprintf(path, sizeof(path), "/dev/dvb/adapter%d/dvr%d", i, j);
      da->da_dvr_path = strdup(path);
    }
  }
}



#if 0
/**
 *
 */
void
dvb_adapter_init(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  const char *name, *s;
  int i, type;
  th_dvb_adapter_t *tda;

  TAILQ_INIT(&dvb_adapters);

  for(i = 0; i < 32; i++) 
    if ((1 << i) & adapter_mask) 
      tda_add(i);

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
      htsmsg_get_u32(c, "qmon", &tda->tda_qmon);
      htsmsg_get_u32(c, "dump_muxes", &tda->tda_dump_muxes);
      htsmsg_get_u32(c, "nitoid", &tda->tda_nitoid);
      htsmsg_get_u32(c, "diseqc_version", &tda->tda_diseqc_version);
    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link)
    dvb_mux_load(tda);
}
#endif

#if 0
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

  if(tda->tda_rootpath == NULL)
    return; // No hardware

  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 20);

  if(LIST_FIRST(&tda->tda_muxes) == NULL)
    return; // No muxes configured

  if(service_compute_weight(&tda->tda_transports) > 0)
    return; /* someone is here */

  /* Check if we have muxes pending for quickscan, if so, choose them */
  if((tdmi = TAILQ_FIRST(&tda->tda_initial_scan_queue)) != NULL) {
    dvb_fe_tune(tdmi, "Initial autoscan");
    return;
  }

  if(!tda->tda_idlescan && TAILQ_FIRST(&tda->tda_scan_queues[0]) == NULL) {
    /* Idlescan is disabled and no muxes are bad */

    if(!tda->tda_qmon)
      return; // Quality monitoring is disabled

    /* If the currently tuned mux is ok, we can stick to it */
    
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

  lock_assert(&global_lock);

  while((tdmi_dst = LIST_FIRST(&dst->tda_muxes)) != NULL)
    dvb_mux_destroy(tdmi_dst);

  LIST_FOREACH(tdmi_src, &src->tda_muxes, tdmi_adapter_link)
    dvb_mux_copy(dst, tdmi_src);

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
  service_t *t;
  
  lock_assert(&global_lock);

  while((t = LIST_FIRST(&tda->tda_transports)) != NULL)
    /* Flush all subscribers */
    service_remove_subscriber(t, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
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
  service_t *t;

  fd = tvh_open(tda->tda_dvr_path, O_RDONLY, 0);
  if(fd == -1) {
    tvhlog(LOG_ALERT, "DVB", "%s: unable to open dvr", tda->tda_dvr_path);
    return NULL;
  }


  while(1) {
    r = read(fd, tsb, sizeof(tsb));

    pthread_mutex_lock(&tda->tda_delivery_mutex);
    
    for(i = 0; i < r; i += 188) {
      LIST_FOREACH(t, &tda->tda_transports, s_active_link)
	if(t->s_dvb_mux_instance == tda->tda_mux_current)
	  ts_recv_packet1(t, tsb + i, NULL);
    }

    if(tda->tda_dump_fd != -1) {
      if(write(tda->tda_dump_fd, tsb, r) != r) {
	tvhlog(LOG_ERR, "dvb",
	       "\"%s\" unable to write to mux dump file -- %s",
	       tda->tda_identifier, strerror(errno));

	close(tda->tda_dump_fd);
	tda->tda_dump_fd = -1;
       }
    }

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
  service_t *t;
  int nummux = 0;
  int numsvc = 0;
  int fdiv;

  htsmsg_add_str(m, "identifier", tda->tda_identifier);
  htsmsg_add_str(m, "name", tda->tda_displayname);

  // XXX: bad bad bad slow slow slow
  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
    nummux++;
    LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
      numsvc++;
    }
  }

  htsmsg_add_str(m, "type", "dvb");

  htsmsg_add_u32(m, "services", numsvc);
  htsmsg_add_u32(m, "muxes", nummux);
  htsmsg_add_u32(m, "initialMuxes", tda->tda_initial_num_mux);

  if(tda->tda_mux_current != NULL) {
    dvb_mux_nicename(buf, sizeof(buf), tda->tda_mux_current);
    htsmsg_add_str(m, "currentMux", buf);
  }

  if(tda->tda_rootpath == NULL)
    return m;

  htsmsg_add_str(m, "path", tda->tda_rootpath);
  htsmsg_add_str(m, "hostconnection", 
		 hostconnection2str(tda->tda_hostconnection));
  htsmsg_add_str(m, "devicename", tda->tda_fe_info->name);

  htsmsg_add_str(m, "deliverySystem", 
		 dvb_adaptertype_to_str(tda->tda_type) ?: "");

  htsmsg_add_u32(m, "satConf", tda->tda_sat);

  fdiv = tda->tda_type == FE_QPSK ? 1 : 1000;

  htsmsg_add_u32(m, "freqMin", tda->tda_fe_info->frequency_min / fdiv);
  htsmsg_add_u32(m, "freqMax", tda->tda_fe_info->frequency_max / fdiv);
  htsmsg_add_u32(m, "freqStep", tda->tda_fe_info->frequency_stepsize / fdiv);

  htsmsg_add_u32(m, "symrateMin", tda->tda_fe_info->symbol_rate_min);
  htsmsg_add_u32(m, "symrateMax", tda->tda_fe_info->symbol_rate_max);
  return m;
}


/**
 *
 */
void
dvb_adapter_notify(th_dvb_adapter_t *tda)
{
  notify_by_msg("tvAdapter", dvb_adapter_build_msg(tda));
}


/**
 *
 */
static void
fe_opts_add(htsmsg_t *a, const char *title, int value)
{
  htsmsg_t *v = htsmsg_create_map();

  htsmsg_add_str(v, "title", title);
  htsmsg_add_u32(v, "id", value);
  htsmsg_add_msg(a, NULL, v);
}

/**
 *
 */
htsmsg_t *
dvb_fe_opts(th_dvb_adapter_t *tda, const char *which)
{
  htsmsg_t *a = htsmsg_create_list();
  fe_caps_t c = tda->tda_fe_info->caps;

  if(!strcmp(which, "constellations")) {
    if(c & FE_CAN_QAM_AUTO)    fe_opts_add(a, "Auto", QAM_AUTO);
    if(c & FE_CAN_QPSK) {
      fe_opts_add(a, "QPSK",     QPSK);
#if DVB_API_VERSION >= 5
      fe_opts_add(a, "PSK_8",    PSK_8);
      fe_opts_add(a, "APSK_16",  APSK_16);
      fe_opts_add(a, "APSK_32",  APSK_32);
#endif
    }
    if(c & FE_CAN_QAM_16)      fe_opts_add(a, "QAM-16",   QAM_16);
    if(c & FE_CAN_QAM_32)      fe_opts_add(a, "QAM-32",   QAM_32);
    if(c & FE_CAN_QAM_64)      fe_opts_add(a, "QAM-64",   QAM_64);
    if(c & FE_CAN_QAM_128)     fe_opts_add(a, "QAM-128",  QAM_128);
    if(c & FE_CAN_QAM_256)     fe_opts_add(a, "QAM-256",  QAM_256);
    return a;
  }

#if DVB_API_VERSION >= 5
  if(!strcmp(which, "delsys")) {
    if(c & FE_CAN_QPSK) {
      fe_opts_add(a, "SYS_DVBS",     SYS_DVBS);
      fe_opts_add(a, "SYS_DVBS2",    SYS_DVBS2);
    } else
      fe_opts_add(a, "SYS_UNDEFINED",    SYS_UNDEFINED);
    return a;
  }
#endif

  if(!strcmp(which, "transmissionmodes")) {
    if(c & FE_CAN_TRANSMISSION_MODE_AUTO) 
      fe_opts_add(a, "Auto", TRANSMISSION_MODE_AUTO);

    fe_opts_add(a, "2k", TRANSMISSION_MODE_2K);
    fe_opts_add(a, "8k", TRANSMISSION_MODE_8K);
    return a;
  }

  if(!strcmp(which, "bandwidths")) {
    if(c & FE_CAN_BANDWIDTH_AUTO) 
      fe_opts_add(a, "Auto", BANDWIDTH_AUTO);

    fe_opts_add(a, "8 MHz", BANDWIDTH_8_MHZ);
    fe_opts_add(a, "7 MHz", BANDWIDTH_7_MHZ);
    fe_opts_add(a, "6 MHz", BANDWIDTH_6_MHZ);
    return a;
  }

  if(!strcmp(which, "guardintervals")) {
    if(c & FE_CAN_GUARD_INTERVAL_AUTO)
      fe_opts_add(a, "Auto", GUARD_INTERVAL_AUTO);

    fe_opts_add(a, "1/32", GUARD_INTERVAL_1_32);
    fe_opts_add(a, "1/16", GUARD_INTERVAL_1_16);
    fe_opts_add(a, "1/8",  GUARD_INTERVAL_1_8);
    fe_opts_add(a, "1/4",  GUARD_INTERVAL_1_4);
    return a;
  }

  if(!strcmp(which, "hierarchies")) {
    if(c & FE_CAN_HIERARCHY_AUTO)
      fe_opts_add(a, "Auto", HIERARCHY_AUTO);

    fe_opts_add(a, "None", HIERARCHY_NONE);
    fe_opts_add(a, "1", HIERARCHY_1);
    fe_opts_add(a, "2", HIERARCHY_2);
    fe_opts_add(a, "4", HIERARCHY_4);
    return a;
  }

  if(!strcmp(which, "fec")) {
    if(c & FE_CAN_FEC_AUTO)
      fe_opts_add(a, "Auto", FEC_AUTO);

    fe_opts_add(a, "None", FEC_NONE);

    fe_opts_add(a, "1/2", FEC_1_2);
    fe_opts_add(a, "2/3", FEC_2_3);
    fe_opts_add(a, "3/4", FEC_3_4);
    fe_opts_add(a, "4/5", FEC_4_5);
#if DVB_API_VERSION >= 5
    fe_opts_add(a, "3/5", FEC_3_5);
#endif
    fe_opts_add(a, "5/6", FEC_5_6);
    fe_opts_add(a, "6/7", FEC_6_7);
    fe_opts_add(a, "7/8", FEC_7_8);
    fe_opts_add(a, "8/9", FEC_8_9);
#if DVB_API_VERSION >= 5
    fe_opts_add(a, "9/10", FEC_9_10);
#endif
    return a;
  }


  if(!strcmp(which, "polarisations")) {
    fe_opts_add(a, "Horizontal",     POLARISATION_HORIZONTAL);
    fe_opts_add(a, "Vertical",       POLARISATION_VERTICAL);
    fe_opts_add(a, "Circular left",  POLARISATION_CIRCULAR_LEFT);
    fe_opts_add(a, "Circular right", POLARISATION_CIRCULAR_RIGHT);
    return a;
  }

  htsmsg_destroy(a);
  return NULL;
}

#endif


/**
 * Check if an adapter is capable of passing the full mux
 */
static int
check_full_stream(dvb_adapter_t *da)
{
  struct dmx_pes_filter_params dmx_param;
  int r;

  if(da->da_dev.d_hostconnection == HOSTCONNECTION_USB12)
    return 0; // Don't even bother, device <-> host interface is too slow

  int fd = tvh_open(da->da_demux_path, O_RDWR, 0);
  if(fd == -1)
    return 0;

  memset(&dmx_param, 0, sizeof(dmx_param));
  dmx_param.pid = 0x2000;
  dmx_param.input = DMX_IN_FRONTEND;
  dmx_param.output = DMX_OUT_TS_TAP;
  dmx_param.pes_type = DMX_PES_OTHER;
  dmx_param.flags = DMX_IMMEDIATE_START;


  r = ioctl(fd, DMX_SET_PES_FILTER, &dmx_param);
  close(fd);
  return !r;
}


/**
 *
 */
static void
dvb_adapter_start(device_t *d, htsmsg_t *conf)
{
  dvb_adapter_t *da = (dvb_adapter_t *)d;

  tvhlog(LOG_INFO, "DVB", "Starting device %s (%s) '%s'",
	 d->d_path, d->d_type, d->d_devicename);

  if(check_full_stream(da)) {
    tvhlog(LOG_INFO, "DVB", "Device %s supports delivering unfiltered mux",
	   d->d_path);
    dvb_fullmux_init(da);
  }
}


static deviceclass_t dvb_device_class = {
  .dc_size = sizeof(dvb_adapter_t),
  .dc_get_conf = dvb_adapter_get_conf,
  .dc_start = dvb_adapter_start,
};
