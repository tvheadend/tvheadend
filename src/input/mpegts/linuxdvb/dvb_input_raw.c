/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2012 Andreas Öman
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

/**
 * DVB input from a raw transport stream
 */

#include <assert.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <sys/epoll.h>

#include "tvheadend.h"
#include "dvb.h"
#include "service.h"

/**
 * Install filters for a service
 *
 * global_lock must be held
 */
static void
open_service(th_dvb_adapter_t *tda, service_t *s)
{
  // NOP -- We receive all PIDs anyway
}


/**
 * Remove filters for a service
 *
 * global_lock must be held
 */
static void
close_service(th_dvb_adapter_t *tda, service_t *s)
{
  // NOP -- open_service() is a NOP
}


/**
 *
 */
static void
open_table(dvb_mux_t *dm, th_dvb_table_t *tdt)
{
  th_dvb_mux_instance_t *tdmi = dm->dm_current_tdmi;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  assert(tdt->tdt_pid < 0x2000);
  tda->tda_table_filter[tdt->tdt_pid] = 1;
}


/**
 *
 */
static void
close_table(dvb_mux_t *dm, th_dvb_table_t *tdt)
{
  th_dvb_mux_instance_t *tdmi = dm->dm_current_tdmi;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  assert(tdt->tdt_pid < 0x2000);
  tda->tda_table_filter[tdt->tdt_pid] = 0;
}


/**
 *
 */
static void
got_section(const uint8_t *data, size_t len, void *opaque)
{
  th_dvb_table_t *tdt = opaque;
  dvb_table_dispatch((uint8_t *)data, len, tdt);
}



/**
 * All the tables can be destroyed from any of the callbacks
 * so we need to be a bit careful here
 */
static void
dvb_table_raw_dispatch(dvb_mux_t *dm, const dvb_table_feed_t *dtf)
{
  int pid = (dtf->dtf_tsb[1] & 0x1f) << 8 | dtf->dtf_tsb[2];
  th_dvb_table_t *vec[dm->dm_num_tables], *tdt;
  int i = 0;
  LIST_FOREACH(tdt, &dm->dm_tables, tdt_link) {
    vec[i++] = tdt;
    tdt->tdt_refcount++;
  }
  assert(i == dm->dm_num_tables);
  int len = dm->dm_num_tables; // can change during callbacks
  for(i = 0; i < len; i++) {
    tdt = vec[i];
    if(!tdt->tdt_destroyed) {
      if(tdt->tdt_pid == pid)
	psi_section_reassemble(&tdt->tdt_sect, dtf->dtf_tsb,
			       0, got_section, tdt);
    }
    dvb_table_release(tdt);
  }
}

/**
 *
 */
static void *
dvb_table_input(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  th_dvb_mux_instance_t *tdmi;
  dvb_table_feed_t *dtf;

  while(1) {

    pthread_mutex_lock(&tda->tda_delivery_mutex);
  
    while((dtf = TAILQ_FIRST(&tda->tda_table_feed)) == NULL)
      pthread_cond_wait(&tda->tda_table_feed_cond, &tda->tda_delivery_mutex);
    TAILQ_REMOVE(&tda->tda_table_feed, dtf, dtf_link);

    pthread_mutex_unlock(&tda->tda_delivery_mutex);

    pthread_mutex_lock(&global_lock);

    if((tdmi = tda->tda_current_tdmi) != NULL)
      dvb_table_raw_dispatch(tdmi->tdmi_mux, dtf);

    pthread_mutex_unlock(&global_lock);
    free(dtf);
  }    
  return NULL;
}


/**
 *
 */
void
dvb_input_raw_setup(th_dvb_adapter_t *tda)
{
  tda->tda_rawmode = 1;
  tda->tda_open_service  = open_service;
  tda->tda_close_service = close_service;
  tda->tda_open_table    = open_table;
  tda->tda_close_table   = close_table;

  TAILQ_INIT(&tda->tda_table_feed);
  pthread_cond_init(&tda->tda_table_feed_cond, NULL);

  pthread_t ptid;
  pthread_create(&ptid, NULL, dvb_table_input, tda);

}

