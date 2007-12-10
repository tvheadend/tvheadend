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
#include "dvb_support.h"


typedef struct dvb_fe_cmd {
  TAILQ_ENTRY(dvb_fe_cmd) link;
  th_dvb_mux_instance_t *tdmi;
} dvb_fe_cmd_t;


/**
 * On some cards the FEC readout, tuning and such things takes a very long
 * time (~0.5 s). Therefore we need to do the tuning and monitoring in a
 * separate thread
 */
static void *
dvb_fe_manager(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  struct timespec ts;
  dvb_fe_cmd_t *c;
  int i, v;
  th_dvb_mux_instance_t *tdmi = NULL;
  fe_status_t fe_status;
  th_dvb_table_t *tdt;

  while(1) {
    ts.tv_sec = time(NULL) + 1;
    ts.tv_nsec = 0;
    
    pthread_mutex_lock(&tda->tda_lock);
    pthread_cond_timedwait(&tda->tda_cond, &tda->tda_lock, &ts);
    c = TAILQ_FIRST(&tda->tda_fe_cmd_queue);
    if(c != NULL)
      TAILQ_REMOVE(&tda->tda_fe_cmd_queue, c, link);

    pthread_mutex_unlock(&tda->tda_lock);

    if(c != NULL) {

      /* Switch to a new mux */

      tdmi = c->tdmi;

      i = ioctl(tda->tda_fe_fd, FE_SET_FRONTEND,
		tdmi->tdmi_mux->tdm_fe_params);

      if(i != 0) {
	syslog(LOG_ERR, "\"%s\" tuning to %dHz"
	       " -- Front configuration failed -- %s",
	       tda->tda_path, tdmi->tdmi_mux->tdm_fe_params->frequency,
	       strerror(errno));
      }
      free(c);

      time(&tdmi->tdmi_got_adapter);

      /* Reset FEC counter */

      ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &v);

      /* Now that we have tuned, start demuxing of tables */

      pthread_mutex_lock(&tdmi->tdmi_table_lock);
      LIST_FOREACH(tdt, &tdmi->tdmi_tables, tdt_link) {
	if(tdt->tdt_fparams == NULL)
	  continue;

	ioctl(tdt->tdt_fd, DMX_SET_FILTER, tdt->tdt_fparams);
	free(tdt->tdt_fparams);
	tdt->tdt_fparams = NULL;
      }

      pthread_mutex_unlock(&tdmi->tdmi_table_lock);
    }

    if(tdmi == NULL)
      continue;

    fe_status = 0;
    ioctl(tda->tda_fe_fd, FE_READ_STATUS, &fe_status);
    
    if(fe_status & FE_HAS_LOCK) {
      tdmi->tdmi_status = NULL;
    } else if(fe_status & FE_HAS_SYNC)
      tdmi->tdmi_status = "No lock, but sync ok";
    else if(fe_status & FE_HAS_VITERBI)
      tdmi->tdmi_status = "No lock, but FEC stable";
    else if(fe_status & FE_HAS_CARRIER)
      tdmi->tdmi_status = "No lock, but carrier present";
    else if(fe_status & FE_HAS_SIGNAL)
      tdmi->tdmi_status = "No lock, but faint signal present";
    else
      tdmi->tdmi_status = "No signal";
    
    ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &v);
    tdmi->tdmi_fec_err_per_sec = (tdmi->tdmi_fec_err_per_sec * 7 + v) / 8;
  }
}


/**
 * Startup the FE management thread
 */
void
dvb_fe_start(th_dvb_adapter_t *tda)
{
  pthread_t ptid;
  pthread_create(&ptid, NULL, dvb_fe_manager, tda);
}


/**
 * Stop the given TDMI
 */
static void
tdmi_stop(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_table_t *tdt;

  pthread_mutex_lock(&tdmi->tdmi_table_lock);

  while((tdt = LIST_FIRST(&tdmi->tdmi_tables)) != NULL)
    dvb_tdt_destroy(tdt);

  pthread_mutex_unlock(&tdmi->tdmi_table_lock);

  tdmi->tdmi_state = TDMI_IDLE;
  time(&tdmi->tdmi_lost_adapter);
}


/**
 * Tune an adapter to a mux instance (but only if needed)
 */
void
dvb_tune_tdmi(th_dvb_mux_instance_t *tdmi, int maylog, tdmi_state_t state)
{
  dvb_fe_cmd_t *c;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  tdmi->tdmi_state = state;

  if(tda->tda_mux_current == tdmi)
    return;

  if(tda->tda_mux_current != NULL)
    tdmi_stop(tda->tda_mux_current);

  tda->tda_mux_current = tdmi;

  if(maylog)
    syslog(LOG_DEBUG, "\"%s\" tuning to mux \"%s\"", 
	   tda->tda_path, tdmi->tdmi_mux->tdm_title);

  /* Add tables which will be activated once the tuning is completed */

  dvb_table_add_default(tdmi);

  /* Send command to the thread */

  c = malloc(sizeof(dvb_fe_cmd_t));
  c->tdmi = tdmi;
  pthread_mutex_lock(&tda->tda_lock);
  TAILQ_INSERT_TAIL(&tda->tda_fe_cmd_queue, c, link);
  pthread_cond_signal(&tda->tda_cond);
  pthread_mutex_unlock(&tda->tda_lock);
}
