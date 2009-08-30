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
#include <errno.h>
#include <assert.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "tvhead.h"
#include "dvb.h"
#include "dvb_support.h"
#include "diseqc.h"
#include "notify.h"


/**
 * Front end monitor
 *
 * Monitor status every second
 */
static void
dvb_fe_monitor(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  fe_status_t fe_status;
  int status, v, update = 0, vv, i, fec, q;
  th_dvb_mux_instance_t *tdmi = tda->tda_mux_current;
  char buf[50];

  gtimer_arm(&tda->tda_fe_monitor_timer, dvb_fe_monitor, tda, 1);

  if(tdmi == NULL)
    return;

  /**
   * Read out front end status
   */
  if(ioctl(tda->tda_fe_fd, FE_READ_STATUS, &fe_status))
    fe_status = 0;

  if(fe_status & FE_HAS_LOCK)
    status = -1;
  else if(fe_status & (FE_HAS_SYNC | FE_HAS_VITERBI | FE_HAS_CARRIER))
    status = TDMI_FE_BAD_SIGNAL;
  else if(fe_status & FE_HAS_SIGNAL)
    status = TDMI_FE_FAINT_SIGNAL;
  else
    status = TDMI_FE_NO_SIGNAL;

  if(tda->tda_fe_monitor_hold > 0) {
    /* Post tuning threshold */
    if(status == -1) { /* We have a lock, don't hold off */
      tda->tda_fe_monitor_hold = 0; 
      /* Reset FEC counter */
      ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &fec);
    } else {
      tda->tda_fe_monitor_hold--;
      return;
    }
  }

  if(status == -1) {
    /* Read FEC counter (delta) */

    if(ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &fec))
      fec = 0;
    
    tdmi->tdmi_fec_err_histogram[tdmi->tdmi_fec_err_ptr++] = fec;
    if(tdmi->tdmi_fec_err_ptr == TDMI_FEC_ERR_HISTOGRAM_SIZE)
      tdmi->tdmi_fec_err_ptr = 0;

    v = vv = 0;
    for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++) {
      if(tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT)
	v++;
      vv += tdmi->tdmi_fec_err_histogram[i];
    }
    vv = vv / TDMI_FEC_ERR_HISTOGRAM_SIZE;

    if(v == 0) {
      status = TDMI_FE_OK;
    } else if(v == 1) {
      status = TDMI_FE_BURSTY_FEC;
    } else {
      status = TDMI_FE_CONSTANT_FEC;
    }
  }

  if(status != tdmi->tdmi_fe_status) {
    tdmi->tdmi_fe_status = status;

    if(tda->tda_logging) {
	dvb_mux_nicename(buf, sizeof(buf), tdmi);
	tvhlog(LOG_INFO, 
	       "dvb", "\"%s\" on adapter \"%s\", status changed to %s",
	       buf, tda->tda_displayname, dvb_mux_status(tdmi));
    }
    update = 1;
  }

  if(status != TDMI_FE_UNKNOWN) {
    q = tdmi->tdmi_quality + (status - TDMI_FE_OK + 1);
    q = MAX(MIN(q, 100), 0);
    if(q != tdmi->tdmi_quality) {
      tdmi->tdmi_quality = q;
      update = 1;
    }
  }

  if(update) {
    htsmsg_t *m = htsmsg_create_map();

    htsmsg_add_str(m, "id", tdmi->tdmi_identifier);
    htsmsg_add_u32(m, "quality", tdmi->tdmi_quality);
    notify_by_msg("dvbMux", m);

    dvb_mux_save(tdmi);
  }
}


/**
 * Stop the given TDMI
 */
void
dvb_fe_stop(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  assert(tdmi == tda->tda_mux_current);
  tda->tda_mux_current = NULL;

  if(tdmi->tdmi_table_initial) {
    tdmi->tdmi_table_initial = 0;
    tda->tda_initial_num_mux--;
  }

  dvb_table_flush_all(tdmi);

  assert(tdmi->tdmi_scan_queue == NULL);

  if(tdmi->tdmi_enabled) {
    tdmi->tdmi_scan_queue = &tda->tda_scan_queues[tdmi->tdmi_quality == 100];
    TAILQ_INSERT_TAIL(tdmi->tdmi_scan_queue, tdmi, tdmi_scan_link);
  }

  time(&tdmi->tdmi_lost_adapter);
}



/**
 *
 */
void
dvb_fe_tune(th_dvb_mux_instance_t *tdmi, const char *reason)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  struct dvb_frontend_parameters p = tdmi->tdmi_conf.dmc_fe_params;
  char buf[256];
  int r;
  

  lock_assert(&global_lock);

  if(tda->tda_mux_current == tdmi)
    return;
  
  if(tdmi->tdmi_scan_queue != NULL) {
    TAILQ_REMOVE(tdmi->tdmi_scan_queue, tdmi, tdmi_scan_link);
    tdmi->tdmi_scan_queue = NULL;
  }

  if(tda->tda_mux_current != NULL)
    dvb_fe_stop(tda->tda_mux_current);

    
  if(tda->tda_type == FE_QPSK) {
	
    /* DVB-S */
    dvb_satconf_t *sc;
    int port, lowfreq, hifreq, switchfreq, hiband, pol;

    lowfreq = 9750000;
    hifreq = 10600000;
    switchfreq = 11700000;
    port = 0;

    if((sc = tdmi->tdmi_conf.dmc_satconf) != NULL) {
      port = sc->sc_port;

      if(sc->sc_lnb != NULL)
	dvb_lnb_get_frequencies(sc->sc_lnb, &lowfreq, &hifreq, &switchfreq);
    }

    hiband = switchfreq && p.frequency > switchfreq;

    pol = tdmi->tdmi_conf.dmc_polarisation;
    diseqc_setup(tda->tda_fe_fd,
		 port,
		 pol == POLARISATION_HORIZONTAL ||
		 pol == POLARISATION_CIRCULAR_LEFT,
		 hiband);
      
    usleep(50000);
      
    if(hiband)
      p.frequency = abs(p.frequency - hifreq);
    else
      p.frequency = abs(p.frequency - lowfreq);
  }

  dvb_mux_nicename(buf, sizeof(buf), tdmi);

  tda->tda_fe_monitor_hold = 4;

  if(tda->tda_logging)
      tvhlog(LOG_INFO,
	     "dvb", "\"%s\" tuning to \"%s\" (%s)", tda->tda_rootpath, buf,
	     reason);

  r = ioctl(tda->tda_fe_fd, FE_SET_FRONTEND, &p);
  if(r != 0) {
    tvhlog(LOG_ERR, "dvb", "\"%s\" tuning to \"%s\""
	   " -- Front configuration failed -- %s",
	   tda->tda_rootpath, buf, strerror(errno));
  }

  tda->tda_mux_current = tdmi;

  gtimer_arm(&tda->tda_fe_monitor_timer, dvb_fe_monitor, tda, 1);

  dvb_table_add_default(tdmi);

  dvb_adapter_notify(tda);
}
