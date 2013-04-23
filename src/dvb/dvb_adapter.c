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

#define _GNU_SOURCE
#include <pthread.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <dirent.h>
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
#include "atomic.h"

static void *dvb_adapter_input_dvr(void *aux);

/**
 *
 */
void
dvb_adapter_start ( th_dvb_adapter_t *tda, int opt )
{
  if(tda->tda_enabled == 0) {
    tvhlog(LOG_INFO, "dvb", "Adapter \"%s\" cannot be started - it's disabled", tda->tda_displayname);
    return;
  }
  
  /* Default to ALL */
  if (!opt)
    opt = TDA_OPT_ALL;

  /* Open front end */
  if ((opt & TDA_OPT_FE) && (tda->tda_fe_fd == -1)) {
    tda->tda_fe_fd = tvh_open(tda->tda_fe_path, O_RDWR | O_NONBLOCK, 0);
    if (tda->tda_fe_fd == -1) return;
    tvhlog(LOG_DEBUG, "dvb", "%s opened frontend %s", tda->tda_rootpath, tda->tda_fe_path);
  }

  /* Start DVR thread */
  if ((opt & TDA_OPT_DVR) && (tda->tda_dvr_pipe.rd == -1)) {
    int err = tvh_pipe(O_NONBLOCK, &tda->tda_dvr_pipe);
    assert(err != -1);
    pthread_create(&tda->tda_dvr_thread, NULL, dvb_adapter_input_dvr, tda);
    tvhlog(LOG_DEBUG, "dvb", "%s started dvr thread", tda->tda_rootpath);
  }
}

void
dvb_adapter_stop ( th_dvb_adapter_t *tda, int opt )
{
  assert(tda->tda_current_tdmi == NULL);

  /* Poweroff */
  if (opt & TDA_OPT_PWR)
    dvb_adapter_poweroff(tda);

  /* Stop DVR thread */
  if ((opt & TDA_OPT_DVR) && (tda->tda_dvr_pipe.rd != -1)) {
    tvhlog(LOG_DEBUG, "dvb", "%s stopping thread", tda->tda_rootpath);
    int err = tvh_write(tda->tda_dvr_pipe.wr, "", 1);
    assert(!err);
    pthread_join(tda->tda_dvr_thread, NULL);
    close(tda->tda_dvr_pipe.rd);
    close(tda->tda_dvr_pipe.wr);
    tda->tda_dvr_pipe.rd = -1;
    tvhlog(LOG_DEBUG, "dvb", "%s stopped thread", tda->tda_rootpath);
  }

  dvb_adapter_notify(tda);

  /* Don't close FE */
  if (!tda->tda_idleclose && tda->tda_enabled) return;

  /* Close front end */
  if ((opt & TDA_OPT_FE) && (tda->tda_fe_fd != -1)) {
    tvhlog(LOG_DEBUG, "dvb", "%s closing frontend", tda->tda_rootpath);
    close(tda->tda_fe_fd);
    tda->tda_fe_fd = -1;
  }
}

/**
 * Install RAW PES filter
 */
static int
dvb_adapter_raw_filter(th_dvb_adapter_t *tda)
{
  int dmx = -1;
  struct dmx_pes_filter_params dmx_param;

  dmx = tvh_open(tda->tda_demux_path, O_RDWR, 0);
  if(dmx == -1) {
    tvhlog(LOG_ALERT, "dvb", "Unable to open %s -- %s",
           tda->tda_demux_path, strerror(errno));
    return -1;
  }

  memset(&dmx_param, 0, sizeof(dmx_param));
  dmx_param.pid      = 0x2000;
  dmx_param.input    = DMX_IN_FRONTEND;
  dmx_param.output   = DMX_OUT_TS_TAP;
  dmx_param.pes_type = DMX_PES_OTHER;
  dmx_param.flags    = DMX_IMMEDIATE_START;

  if(ioctl(dmx, DMX_SET_PES_FILTER, &dmx_param) == -1) {
    tvhlog(LOG_ERR, "dvb",
    "Unable to configure demuxer \"%s\" for all PIDs -- %s",
    tda->tda_demux_path, strerror(errno));
    close(dmx);
    return -1;
  }

  return dmx;
}

/**
 *
 */
static void *
dvb_adapter_input_dvr(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  int fd = -1, i, r, c, efd, nfds, dmx = -1;
  uint8_t tsb[188 * 10];
  service_t *t;
  struct epoll_event ev;
  int delay = 10;

  /* Install RAW demux */
  if (tda->tda_rawmode) {
    if ((dmx = dvb_adapter_raw_filter(tda)) == -1) {
      tvhlog(LOG_ALERT, "dvb", "Unable to install raw mux filter");
      return NULL;
    }
  }

  /* Open DVR */
  if ((fd = tvh_open(tda->tda_dvr_path, O_RDONLY | O_NONBLOCK, 0)) == -1) {
    close(dmx);
    return NULL;
  }

  /* Create poll */
  efd = epoll_create(2);
  memset(&ev, 0, sizeof(ev));
  ev.events  = EPOLLIN;
  ev.data.fd = tda->tda_dvr_pipe.rd;
  epoll_ctl(efd, EPOLL_CTL_ADD, tda->tda_dvr_pipe.rd, &ev);
  ev.data.fd = fd;
  epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);

  r = i = 0;
  while(1) {

    /* Wait for input */
    nfds = epoll_wait(efd, &ev, 1, delay);

    /* No data */
    if (nfds < 1) continue;

    /* Exit */
    if (ev.data.fd != fd) break;

    /* Read data */
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
    atomic_add(&tda->tda_bytes, c);

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
  htsmsg_t *m = htsmsg_create_map();
  int fdiv;

  htsmsg_add_str(m, "identifier", tda->tda_identifier);
  htsmsg_add_str(m, "name", tda->tda_displayname);

  htsmsg_add_str(m, "type", "dvb");

  if(tda->tda_current_tdmi != NULL) {
    th_dvb_mux_instance_t *tdmi = tda->tda_current_tdmi;

    htsmsg_add_str(m, "currentMux",
                   dvb_mux_nicename(tda->tda_current_tdmi->tdmi_mux));

    htsmsg_add_u32(m, "signal", MIN(tdmi->tdmi_signal * 100 / 65535, 100));
    htsmsg_add_u32(m, "snr", tdmi->tdmi_snr);
    htsmsg_add_u32(m, "ber", tdmi->tdmi_ber);
    htsmsg_add_u32(m, "unc", tdmi->tdmi_unc);
    htsmsg_add_u32(m, "uncavg", tdmi->tdmi_unc_avg);
    htsmsg_add_str(m, "reason", tda->tda_tune_reason);
  } else {
    htsmsg_add_str(m, "currentMux", "");
    htsmsg_add_u32(m, "signal", 0);
    htsmsg_add_u32(m, "snr", 0);
    htsmsg_add_u32(m, "ber", 0);
    htsmsg_add_u32(m, "unc", 0);
    htsmsg_add_u32(m, "uncavg", 0);

    if(tda->tda_fe_fd == -1) {
      htsmsg_add_str(m, "reason", "Closed");
    } else {
      htsmsg_add_str(m, "reason", "Idle");
    }
  }

  if(tda->tda_rootpath == NULL)
    return m;

  htsmsg_add_str(m, "path", tda->tda_rootpath);
  htsmsg_add_str(m, "hostconnection", 
		 hostconnection2str(tda->tda_hostconnection));
  htsmsg_add_str(m, "devicename", tda->tda_fe_info->name);

  htsmsg_add_str(m, "deliverySystem", 
		 dvb_adaptertype_to_str(tda->tda_fe_type) ?: "");

  htsmsg_add_u32(m, "satConf", tda->tda_sat);

  fdiv = tda->tda_fe_type == FE_QPSK ? 1 : 1000;

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

  if(!strcmp(which, "delsys")) {
    if(c & FE_CAN_QPSK) {
#if DVB_API_VERSION >= 5
      fe_opts_add(a, "SYS_DVBS",     SYS_DVBS);
      fe_opts_add(a, "SYS_DVBS2",    SYS_DVBS2);
#else
      fe_opts_add(a, "SYS_DVBS",     -1);
#endif
    }
    return a;
  }

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
  if (!tda->tda_poweroff || tda->tda_fe_type != FE_QPSK)
    return;
  diseqc_voltage_off(tda->tda_fe_fd);
  tvhlog(LOG_DEBUG, "dvb", "\"%s\" is off", tda->tda_rootpath);
}
