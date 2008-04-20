/*
 *  TV Input - Linux DVB interface - Feed receiver functions
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

#include "tvhead.h"
#include "dispatch.h"
#include "dvb.h"
#include "dvb_dvr.h"
#include "channels.h"
#include "transports.h"
#include "dvb_support.h"
#include "tsdemux.h"

static void dvr_fd_callback(int events, void *opaque, int fd);

int
dvb_dvr_init(th_dvb_adapter_t *tda)
{
  int dvr;

  dvr = open(tda->tda_dvr_path, O_RDONLY | O_NONBLOCK);
  if(dvr == -1) {
    syslog(LOG_ALERT, "%s: unable to open dvr\n", tda->tda_dvr_path);
    return -1;
  }

  dispatch_addfd(dvr, dvr_fd_callback, tda, DISPATCH_READ);


  return 0;
}

/*
 *
 */
static void
dvb_dvr_process_packets(th_dvb_adapter_t *tda, uint8_t *tsb, int r)
{
  th_transport_t *t;

  while(r >= 188) {
    LIST_FOREACH(t, &tda->tda_transports, tht_active_link)
      ts_recv_packet1(t, tsb);
    r -= 188;
    tsb += 188;
  }
}


/*
 *
 */
static void
dvr_fd_callback(int events, void *opaque, int fd)
{
  th_dvb_adapter_t *tda = opaque;
  uint8_t tsb0[188 * 10];
  int r;

  if(!(events & DISPATCH_READ))
    return;

  r = read(fd, tsb0, sizeof(tsb0));
  dvb_dvr_process_packets(tda, tsb0, r);
}



/*
 *
 */
void
dvb_adapter_clean(th_dvb_adapter_t *tda)
{
  th_transport_t *t;
  
  while((t = LIST_FIRST(&tda->tda_transports)) != NULL)
    transport_stop(t, 1);
}



/*
 *
 */
void
dvb_stop_feed(th_transport_t *t)
{
  th_stream_t *st;

  LIST_REMOVE(t, tht_active_link);
  LIST_FOREACH(st, &t->tht_streams, st_link) {
    close(st->st_demuxer_fd);
    st->st_demuxer_fd = -1;
  }
  t->tht_status = TRANSPORT_IDLE;
}


/*
 * Switch the adapter (which is implicitly tied to our transport)
 * to receive the given transport.
 *
 * But we only do this if 'weight' is higher than all of the current
 * transports that is subscribing to the adapter
 */
int
dvb_start_feed(th_transport_t *t, unsigned int weight, int status)
{
  struct dmx_pes_filter_params dmx_param;
  th_stream_t *st;
  int w, fd, pid, i;
  th_dvb_adapter_t *tda = t->tht_dvb_mux_instance->tdmi_adapter;
  th_dvb_mux_instance_t *tdmi = tda->tda_mux_current;

  if(tda->tda_rootpath == NULL)
    return 1; /* hardware not present */

  /* Check if adapter is idle, or already tuned */

  if(tdmi != NULL && tdmi != t->tht_dvb_mux_instance) {

    /* Nope .. */

    if(tdmi->tdmi_status != NULL)
      return 1;  /* no lock on adapter, cant use it */

    for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++)
      if(tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT)
	return 1; /* too many errors for using it */

    w = transport_compute_weight(&tdmi->tdmi_adapter->tda_transports);
    if(w > weight)
      return 1; /* We are outranked by weight, cant use it */

    dvb_adapter_clean(tda);
  }
  tdmi = t->tht_dvb_mux_instance;

  LIST_FOREACH(st, &t->tht_streams, st_link) {
    
    fd = open(tda->tda_demux_path, O_RDWR);
    
    pid = st->st_pid;
    st->st_cc_valid = 0;

    if(fd == -1) {
      st->st_demuxer_fd = -1;
      syslog(LOG_ERR,
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
      syslog(LOG_ERR,
	     "\"%s\" unable to configure demuxer \"%s\" for pid %d -- %s",
	     t->tht_name, tda->tda_demux_path, pid, strerror(errno));
      close(fd);
      fd = -1;
    }

    st->st_demuxer_fd = fd;
  }

  LIST_INSERT_HEAD(&tda->tda_transports, t, tht_active_link);
  t->tht_status = status;
  
  dvb_tune_tdmi(tdmi, 1, TDMI_RUNNING);
  return 0;
}



