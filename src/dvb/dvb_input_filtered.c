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
 * DVB input using hardware filters
 */

#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

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
  struct dmx_pes_filter_params dmx_param;
  int fd;
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &s->s_components, es_link) {
    if(st->es_pid >= 0x2000)
      continue;

    if(st->es_demuxer_fd != -1)
      continue;

    fd = tvh_open(tda->tda_demux_path, O_RDWR, 0);
    st->es_cc_valid = 0;

    if(fd == -1) {
      st->es_demuxer_fd = -1;
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to open demuxer \"%s\" for pid %d -- %s",
	     s->s_identifier, tda->tda_demux_path, 
	     st->es_pid, strerror(errno));
      continue;
    }

    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid = st->es_pid;
    dmx_param.input = DMX_IN_FRONTEND;
    dmx_param.output = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags = DMX_IMMEDIATE_START;

    if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
      tvhlog(LOG_ERR, "dvb",
	     "\"%s\" unable to configure demuxer \"%s\" for pid %d -- %s",
	     s->s_identifier, tda->tda_demux_path, 
	     st->es_pid, strerror(errno));
      close(fd);
      fd = -1;
    }

    st->es_demuxer_fd = fd;
  }
}


/**
 * Remove filters for a service
 *
 * global_lock must be held
 */
static void
close_service(th_dvb_adapter_t *tda, service_t *s)
{
  elementary_stream_t *es;

  TAILQ_FOREACH(es, &s->s_components, es_link) {
    if(es->es_demuxer_fd != -1) {
      close(es->es_demuxer_fd);
      es->es_demuxer_fd = -1;
    }
  }
}





void
dvb_input_filtered_setup(th_dvb_adapter_t *tda)
{
  tda->tda_open_service  = open_service;
  tda->tda_close_service = close_service;
}

