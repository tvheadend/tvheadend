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
  int pid;
  th_transport_t *t;

  while(r >= 188) {
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    LIST_FOREACH(t, &tda->tda_transports, tht_adapter_link)
      transport_recv_tsb(t, pid, tsb);
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
    dvb_stop_feed(t);
}



/*
 *
 */
void
dvb_stop_feed(th_transport_t *t)
{
  th_pid_t *tp;

  t->tht_dvb_adapter = NULL;
  LIST_REMOVE(t, tht_adapter_link);
  LIST_FOREACH(tp, &t->tht_pids, tp_link) {
    close(tp->tp_demuxer_fd);
    tp->tp_demuxer_fd = -1;
  }
  t->tht_status = TRANSPORT_IDLE;
  transport_flush_subscribers(t);
}


/*
 *
 */
int
dvb_start_feed(th_transport_t *t, unsigned int weight)
{
  th_dvb_adapter_t *tda;
  struct dmx_pes_filter_params dmx_param;
  th_pid_t *tp;
  int w, fd, pid;

  th_dvb_mux_instance_t *tdmi, *cand = NULL;
  th_dvb_mux_t *mux = t->tht_dvb_mux;

  LIST_FOREACH(tdmi, &mux->tdm_instances, tdmi_mux_link) {

    if(tdmi->tdmi_status != NULL)
      continue; /* no lock */

    if(tdmi->tdmi_fec_err_per_sec > DVB_FEC_ERROR_LIMIT)
      continue; /* too much errors to even consider */

    if(tdmi->tdmi_state == TDMI_RUNNING)
      goto gotmux;

    w = transport_compute_weight(&tdmi->tdmi_adapter->tda_transports);
    if(w < weight && cand == NULL)
      cand = tdmi;
  }

  if(cand == NULL)
    return 1;

  tdmi = cand;

  dvb_adapter_clean(tdmi->tdmi_adapter);
  
 gotmux:
  tda = tdmi->tdmi_adapter;

  LIST_FOREACH(tp, &t->tht_pids, tp_link) {
    
    fd = open(tda->tda_demux_path, O_RDWR);
    
    pid = tp->tp_pid;
    tp->tp_cc_valid = 0;

    if(fd == -1) {
      tp->tp_demuxer_fd = -1;
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

    tp->tp_demuxer_fd = fd;
  }

  LIST_INSERT_HEAD(&tda->tda_transports, t, tht_adapter_link);
  t->tht_dvb_adapter = tda;
  t->tht_status = TRANSPORT_RUNNING;
  
  dvb_tune_tdmi(tdmi, 1, TDMI_RUNNING);
  return 0;
}


/* 
 *
 */

int
dvb_configure_transport(th_transport_t *t, const char *muxname)
{
  th_dvb_mux_t *tdm;

  LIST_FOREACH(tdm, &dvb_muxes, tdm_global_link)
    if(!strcmp(tdm->tdm_name, muxname))
      break;

  if(tdm == NULL)
    return -1;

  t->tht_type = TRANSPORT_DVB;
  t->tht_dvb_mux = tdm;
  t->tht_name = strdup(tdm->tdm_title);
  return 0;
}






