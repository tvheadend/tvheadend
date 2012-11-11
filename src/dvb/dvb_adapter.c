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
#include <sys/epoll.h>
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

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_support.h"
#include "tsdemux.h"
#include "notify.h"
#include "service.h"
#include "epggrab.h"
#include "diseqc.h"

struct th_dvb_adapter_queue dvb_adapters;
struct th_dvb_mux_instance_tree dvb_muxes;
static void *dvb_adapter_input_dvr(void *aux);


/**
 *
 */
static th_dvb_adapter_t *
tda_alloc(void)
{
  int i;
  th_dvb_adapter_t *tda = calloc(1, sizeof(th_dvb_adapter_t));
  pthread_mutex_init(&tda->tda_delivery_mutex, NULL);

  for (i = 0; i < TDA_SCANQ_NUM; i++ )
    TAILQ_INIT(&tda->tda_scan_queues[i]);
  TAILQ_INIT(&tda->tda_initial_scan_queue);
  TAILQ_INIT(&tda->tda_satconfs);
  streaming_pad_init(&tda->tda_streaming_pad);
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
  htsmsg_add_u32(m, "idleclose", tda->tda_idleclose);
  htsmsg_add_u32(m, "skip_checksubscr", tda->tda_skip_checksubscr);
  htsmsg_add_u32(m, "sidtochan", tda->tda_sidtochan);
  htsmsg_add_u32(m, "qmon", tda->tda_qmon);
  htsmsg_add_u32(m, "poweroff", tda->tda_poweroff);
  htsmsg_add_u32(m, "nitoid", tda->tda_nitoid);
  htsmsg_add_u32(m, "diseqc_version", tda->tda_diseqc_version);
  htsmsg_add_u32(m, "diseqc_repeats", tda->tda_diseqc_repeats);
  htsmsg_add_u32(m, "extrapriority", tda->tda_extrapriority);
  htsmsg_add_u32(m, "skip_initialscan", tda->tda_skip_initialscan);
  htsmsg_add_u32(m, "disable_pmt_monitor", tda->tda_disable_pmt_monitor);
  htsmsg_add_s32(m, "full_mux_rx", tda->tda_full_mux_rx);
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
dvb_adapter_set_skip_initialscan(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_skip_initialscan == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" skip initial scan set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_skip_initialscan = on;
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
dvb_adapter_set_idleclose(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_idleclose == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" idle fd close set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_idleclose = on;
  tda_save(tda);
}


/**
 *
 */
void
dvb_adapter_set_skip_checksubscr(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_skip_checksubscr == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" skip service availability check when mapping set to: %s",
   tda->tda_displayname, on ? "On" : "Off");

  tda->tda_skip_checksubscr = on;
  tda_save(tda);
}

/**
 *
 */
void
dvb_adapter_set_qmon(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_qmon == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" quality monitoring set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_qmon = on;
  tda_save(tda);
}

/**
 *
 */
void
dvb_adapter_set_sidtochan(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_sidtochan == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" use SID as channel number when mapping set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_sidtochan = on;
  tda_save(tda);
}

/**
 *
 */
void
dvb_adapter_set_poweroff(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_poweroff == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" idle poweroff set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_poweroff = on;
  tda_save(tda);
}


/**
 *
 */
void
dvb_adapter_set_nitoid(th_dvb_adapter_t *tda, int nitoid)
{
  lock_assert(&global_lock);

  if(tda->tda_nitoid == nitoid)
    return;

  tvhlog(LOG_NOTICE, "dvb", "NIT-o network id \"%d\" changed to \"%d\"",
	 tda->tda_nitoid, nitoid);

  tda->tda_nitoid = nitoid;
  
  tda_save(tda);

}


/**
 *
 */
void
dvb_adapter_set_diseqc_version(th_dvb_adapter_t *tda, unsigned int v)
{
  if(v > 1)
    v = 1;

  if(tda->tda_diseqc_version == v)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" DiSEqC version set to: %s",
	 tda->tda_displayname, v ? "1.1 / 2.1" : "1.0 / 2.0" );

  tda->tda_diseqc_version = v;
  tda_save(tda);
}

/**
 * sets the number of diseqc repeats to perform
 */
void
dvb_adapter_set_diseqc_repeats(th_dvb_adapter_t *tda, unsigned int repeats)
{
  if(tda->tda_diseqc_repeats == repeats)
    return;
  lock_assert(&global_lock);
  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" DiSEqC repeats set to: %i",
         tda->tda_displayname, repeats);
  tda->tda_diseqc_repeats = repeats;
  tda_save(tda);
}

/**
 *
 */
void
dvb_adapter_set_extrapriority(th_dvb_adapter_t *tda, int extrapriority)
{
  lock_assert(&global_lock);

  if(tda->tda_extrapriority == extrapriority)
    return;

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" extra priority \"%d\" changed to \"%d\"",
         tda->tda_displayname, tda->tda_extrapriority, extrapriority);

  tda->tda_extrapriority = extrapriority;
  tda_save(tda);
}

/**
 *
 */
void
dvb_adapter_set_disable_pmt_monitor(th_dvb_adapter_t *tda, int on)
{
  if(tda->tda_disable_pmt_monitor == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb", "Adapter \"%s\" disabled PMT monitoring set to: %s",
	 tda->tda_displayname, on ? "On" : "Off");

  tda->tda_disable_pmt_monitor = on;
  tda_save(tda);
}


/**
 *
 */
void
dvb_adapter_set_full_mux_rx(th_dvb_adapter_t *tda, int on)
{
  const char* label[] = { "Auto", "Off", "On" };
  
  if (on < -1) on = -1;
  if (on >  1) on = 1;

  if(tda->tda_full_mux_rx == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "dvb",
         "Adapter \"%s\" disabled full MUX receive set to: %s",
	 tda->tda_displayname, label[on+1]);

  tda->tda_full_mux_rx = on;
  tda_save(tda);
}


/**
 *
 */
static void
dvb_adapter_checkspeed(th_dvb_adapter_t *tda)
{
  char dev[64];

  snprintf(dev, sizeof(dev), "dvb/dvb%d.dvr0", tda->tda_adapter_num);
  tda->tda_hostconnection = get_device_connection(dev);
}




/**
 * Return 1 if an adapter is capable of receiving a full mux
 */
static int
check_full_stream(th_dvb_adapter_t *tda)
{
  struct dmx_pes_filter_params dmx_param;
  int r;

  if(tda->tda_full_mux_rx != -1)
    return tda->tda_full_mux_rx;

  if(tda->tda_hostconnection == HOSTCONNECTION_USB12)
    return 0; // Don't even bother, device <-> host interface is too slow

  if(tda->tda_hostconnection == HOSTCONNECTION_USB480)
    return 0; // USB in general appears to have CPU loading issues?

  int fd = tvh_open(tda->tda_demux_path, O_RDWR, 0);
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
tda_add(int adapter_num)
{
  char path[200], fname[256];
  int fe, i, r;
  th_dvb_adapter_t *tda;
  char buf[400];

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
  tda->tda_fe_path = strdup(fname);
  tda->tda_fe_fd       = -1;
  tda->tda_dvr_pipe[0] = -1;
  tda->tda_full_mux_rx = -1;

  tda->tda_fe_info = malloc(sizeof(struct dvb_frontend_info));

  if(ioctl(fe, FE_GET_INFO, tda->tda_fe_info)) {
    tvhlog(LOG_ALERT, "dvb", "%s: Unable to query adapter", fname);
    close(fe);
    free(tda);
    return;
  }
  if (tda->tda_idlescan || !tda->tda_idleclose)
    tda->tda_fe_fd = fe;
  else
    close(fe);

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

  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 1);
}



/**
 *
 */
static void
tda_add_from_file(const char *filename)
{
  int i, r;
  th_dvb_adapter_t *tda;
  char buf[400];

  tda = tda_alloc();

  tda->tda_adapter_num = -1;
  tda->tda_fe_fd       = -1;
  tda->tda_dvr_pipe[0] = -1;

  tda->tda_type = -1;

  snprintf(buf, sizeof(buf), "%s", filename);

  r = strlen(buf);
  for(i = 0; i < r; i++)
    if(!isalnum((int)buf[i]))
      buf[i] = '_';

  tda->tda_identifier = strdup(buf);
  
  tda->tda_autodiscovery = 0;
  tda->tda_idlescan = 0;

  tda->tda_sat = 0;
 
  tda->tda_full_mux_rx = 1;

  /* Come up with an initial displayname, user can change it and it will
     be overridden by any stored settings later on */

  tda->tda_displayname = strdup(filename);

  TAILQ_INSERT_TAIL(&dvb_adapters, tda, tda_global_link);
}

/**
 * Initiliase input
 */
static void tda_init_input (th_dvb_adapter_t *tda)
{
  if(tda->tda_type == -1 || check_full_stream(tda)) {
    tvhlog(LOG_INFO, "dvb", "Adapter %s will run in full mux mode", tda->tda_rootpath);
    dvb_input_raw_setup(tda);
  } else {
    tvhlog(LOG_INFO, "dvb", "Adapter %s will run in filtered mode", tda->tda_rootpath);
    dvb_input_filtered_setup(tda);
  }
}



/**
 *
 */
void
dvb_adapter_start ( th_dvb_adapter_t *tda )
{
  /* Open front end */
  if (tda->tda_fe_fd == -1) {
    tda->tda_fe_fd = tvh_open(tda->tda_fe_path, O_RDWR | O_NONBLOCK, 0);
    if (tda->tda_fe_fd == -1) return;
    tvhlog(LOG_DEBUG, "dvb", "%s opened frontend %s", tda->tda_rootpath, tda->tda_fe_path);
  }

  /* Start DVR thread */
  if (tda->tda_dvr_pipe[0] == -1) {
    int err = pipe(tda->tda_dvr_pipe);
    assert(err != -1);

    fcntl(tda->tda_dvr_pipe[0], F_SETFD, fcntl(tda->tda_dvr_pipe[0], F_GETFD) | FD_CLOEXEC);
    fcntl(tda->tda_dvr_pipe[0], F_SETFL, fcntl(tda->tda_dvr_pipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(tda->tda_dvr_pipe[1], F_SETFD, fcntl(tda->tda_dvr_pipe[1], F_GETFD) | FD_CLOEXEC);
    pthread_create(&tda->tda_dvr_thread, NULL, dvb_adapter_input_dvr, tda);
    tvhlog(LOG_DEBUG, "dvb", "%s started dvr thread", tda->tda_rootpath);
  }
}

void
dvb_adapter_stop ( th_dvb_adapter_t *tda )
{
  /* Poweroff */
  dvb_adapter_poweroff(tda);

  /* Don't stop/close */
  if (!tda->tda_idleclose) return;

  /* Close front end */
  if (tda->tda_fe_fd != -1) {
    tvhlog(LOG_DEBUG, "dvb", "%s closing frontend", tda->tda_rootpath);
    close(tda->tda_fe_fd);
    tda->tda_fe_fd = -1;
  }

  /* Stop DVR thread */
  if (tda->tda_dvr_pipe[0] != -1) {
    tvhlog(LOG_DEBUG, "dvb", "%s stopping thread", tda->tda_rootpath);
    int err = write(tda->tda_dvr_pipe[1], "", 1);
    assert(err != -1);
    pthread_join(tda->tda_dvr_thread, NULL);
    close(tda->tda_dvr_pipe[0]);
    close(tda->tda_dvr_pipe[1]);
    tda->tda_dvr_pipe[0] = -1;
    tvhlog(LOG_DEBUG, "dvb", "%s stopped thread", tda->tda_rootpath);
  }
}

/**
 *
 */
void
dvb_adapter_init(uint32_t adapter_mask, const char *rawfile)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  const char *name, *s;
  int i, type;
  uint32_t u32;
  th_dvb_adapter_t *tda;

  TAILQ_INIT(&dvb_adapters);

  /* Initialise hardware */
  for(i = 0; i < 32; i++) 
    if ((1 << i) & adapter_mask) 
      tda_add(i);

  /* Initialise rawts test file */
  if(rawfile)
    tda_add_from_file(rawfile);

  /* Load configuration */
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
      htsmsg_get_u32(c, "idleclose", &tda->tda_idleclose);
      htsmsg_get_u32(c, "skip_checksubscr", &tda->tda_skip_checksubscr);
      htsmsg_get_u32(c, "sidtochan", &tda->tda_sidtochan);
      htsmsg_get_u32(c, "qmon", &tda->tda_qmon);
      htsmsg_get_u32(c, "poweroff", &tda->tda_poweroff);
      htsmsg_get_u32(c, "nitoid", &tda->tda_nitoid);
      htsmsg_get_u32(c, "diseqc_version", &tda->tda_diseqc_version);
      htsmsg_get_u32(c, "diseqc_repeats", &tda->tda_diseqc_repeats);
      htsmsg_get_u32(c, "extrapriority", &tda->tda_extrapriority);
      htsmsg_get_u32(c, "skip_initialscan", &tda->tda_skip_initialscan);
      htsmsg_get_u32(c, "disable_pmt_monitor", &tda->tda_disable_pmt_monitor);
      if (htsmsg_get_s32(c, "full_mux_rx", &tda->tda_full_mux_rx))
        if (!htsmsg_get_u32(c, "disable_full_mux_rx", &u32) && u32)
          tda->tda_full_mux_rx = 0;
    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    tda_init_input(tda);

    if(tda->tda_sat)
      dvb_satconf_init(tda);

    dvb_mux_load(tda);
  }
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

  if(tda->tda_rootpath == NULL)
    return; // No hardware

  // default period
  gtimer_arm(&tda->tda_mux_scanner_timer, dvb_adapter_mux_scanner, tda, 20);

  /* No muxes */
  if(LIST_FIRST(&tda->tda_muxes) == NULL) {
    dvb_adapter_poweroff(tda);
    return;
  }

  /* Someone is actively using */
  if(service_compute_weight(&tda->tda_transports) > 0)
    return;

  if(tda->tda_mux_current != NULL &&
     LIST_FIRST(&tda->tda_mux_current->tdmi_subscriptions) != NULL)
    return; // Someone is doing full mux dump

  /* Check if we have muxes pending for quickscan, if so, choose them */
  if((tdmi = TAILQ_FIRST(&tda->tda_initial_scan_queue)) != NULL) {
    dvb_fe_tune(tdmi, "Initial autoscan");
    return;
  }

  /* Check EPG */
  if (tda->tda_mux_epg) {
    epggrab_mux_stop(tda->tda_mux_epg, 1); // timeout anything not complete
    tda->tda_mux_epg = NULL; // skip this time
  } else {
    tda->tda_mux_epg = epggrab_mux_next(tda);
  }

  /* EPG */
  if (tda->tda_mux_epg) {
    int period = epggrab_mux_period(tda->tda_mux_epg);
    if (period > 20)
      gtimer_arm(&tda->tda_mux_scanner_timer,
                 dvb_adapter_mux_scanner, tda, period);
    dvb_fe_tune(tda->tda_mux_epg, "EPG scan");
    return;

  /* Normal */
  } else if (tda->tda_idlescan) {

    /* Alternate queue */
    for(i = 0; i < TDA_SCANQ_NUM; i++) {
      tda->tda_scan_selector++;
      if (tda->tda_scan_selector == TDA_SCANQ_NUM)
        tda->tda_scan_selector = 0;
      tdmi = TAILQ_FIRST(&tda->tda_scan_queues[tda->tda_scan_selector]);
      if (tdmi) {
        dvb_fe_tune(tdmi, "Autoscan");
        return;
      }
    }
  }

  /* Ensure we stop current mux and power off (if required) */
  if (tda->tda_mux_current)
    dvb_fe_stop(tda->tda_mux_current, 0);
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
    dvb_mux_copy(dst, tdmi_src, NULL);

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
  int fd, i, r, c, efd, nfds, dmx = -1;
  uint8_t tsb[188 * 10];
  service_t *t;
  struct epoll_event ev;
  char path[256];

  snprintf(path, sizeof(path), "%s/dvr0", tda->tda_rootpath);

  fd = tvh_open(path, O_RDONLY | O_NONBLOCK, 0);
  if(fd == -1) {
    tvhlog(LOG_ALERT, "dvb", "Unable to open %s -- %s", path, strerror(errno));
    return NULL;
  }

  if(tda->tda_rawmode) {

    // Receive unfiltered raw transport stream

    dmx = tvh_open(tda->tda_demux_path, O_RDWR, 0);
    if(dmx == -1) {
      tvhlog(LOG_ALERT, "dvb", "Unable to open %s -- %s", 
	     tda->tda_demux_path, strerror(errno));
      close(fd);
      return NULL;
    }

    struct dmx_pes_filter_params dmx_param;

    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid = 0x2000;
    dmx_param.input = DMX_IN_FRONTEND;
    dmx_param.output = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags = DMX_IMMEDIATE_START;
  
    if(ioctl(dmx, DMX_SET_PES_FILTER, &dmx_param)) {
      tvhlog(LOG_ERR, "dvb",
	     "Unable to configure demuxer \"%s\" for all PIDs -- %s",
	     tda->tda_demux_path, strerror(errno));
      close(dmx);
      close(fd);
      return NULL;
    }
  }

  /* Create poll */
  efd = epoll_create(2);
  memset(&ev, 0, sizeof(ev));
  ev.events  = EPOLLIN;
  ev.data.fd = fd;
  epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
  ev.data.fd = tda->tda_dvr_pipe[0];
  epoll_ctl(efd, EPOLL_CTL_ADD, tda->tda_dvr_pipe[0], &ev);

  r = i = 0;
  while(1) {

    /* Wait for input */
    nfds = epoll_wait(efd, &ev, 1, -1);
    if (nfds < 1) continue;
    if (ev.data.fd != fd) break;

    c = read(fd, tsb+r, sizeof(tsb)-r);
    if (c < 0) {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      else if (errno == EOVERFLOW) {
        tvhlog(LOG_WARNING, "dvb", "\"%s\" read() EOVERFLOW",
               tda->tda_identifier);
        continue;
      } else {
        // TODO: should we try to recover?
        tvhlog(LOG_ERR, "dvb", "\"%s\" read() error %d",
               tda->tda_identifier, errno);
        break;
      }
    }
    r += c;

    /* not enough data */
    if (r < 188) continue;

    int wakeup_table_feed = 0;  // Just wanna wakeup once

    pthread_mutex_lock(&tda->tda_delivery_mutex);

    if(LIST_FIRST(&tda->tda_streaming_pad.sp_targets) != NULL) {
      streaming_message_t sm;
      pktbuf_t *pb = pktbuf_alloc(tsb, r);
      memset(&sm, 0, sizeof(sm));
      sm.sm_type = SMT_MPEGTS;
      sm.sm_data = pb;
      streaming_pad_deliver(&tda->tda_streaming_pad, &sm);
      pktbuf_ref_dec(pb);
    }

    /* Process */
    while (r >= 188) {
  
      /* sync */
      if (tsb[i] == 0x47) {
	      int pid = (tsb[i+1] & 0x1f) << 8 | tsb[i+2];

	      if(tda->tda_table_filter[pid]) {
	        if(!(tsb[i+1] & 0x80)) { // Only dispatch to table parser if not error
	          dvb_table_feed_t *dtf = malloc(sizeof(dvb_table_feed_t));
	          memcpy(dtf->dtf_tsb, tsb + i, 188);
	          TAILQ_INSERT_TAIL(&tda->tda_table_feed, dtf, dtf_link);
	          wakeup_table_feed = 1;
	        }
	      } else {
          LIST_FOREACH(t, &tda->tda_transports, s_active_link)
            if(t->s_dvb_mux_instance == tda->tda_mux_current)
              ts_recv_packet1(t, tsb + i, NULL);
        }

        i += 188;
        r -= 188;

      /* no sync */
      } else {
        tvhlog(LOG_DEBUG, "dvb", "\"%s\" ts sync lost", tda->tda_identifier);
        if (ts_resync(tsb, &r, &i)) break;
        tvhlog(LOG_DEBUG, "dvb", "\"%s\" ts sync found", tda->tda_identifier);
      }
    }

    if(wakeup_table_feed)
      pthread_cond_signal(&tda->tda_table_feed_cond);

    pthread_mutex_unlock(&tda->tda_delivery_mutex);

    /* reset buffer */
    if (r) memmove(tsb, tsb+i, r);
    i = 0;
  }

  if(dmx != -1)
    close(dmx);
  close(efd);
  close(fd);
  return NULL;
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

/**
 * Turn off the adapter
 */
void
dvb_adapter_poweroff(th_dvb_adapter_t *tda)
{
  if (tda->tda_fe_fd == -1) return;
  lock_assert(&global_lock);
  if (!tda->tda_poweroff || tda->tda_type != FE_QPSK)
    return;
  diseqc_voltage_off(tda->tda_fe_fd);
  tvhlog(LOG_DEBUG, "dvb", "\"%s\" is off", tda->tda_rootpath);
}
