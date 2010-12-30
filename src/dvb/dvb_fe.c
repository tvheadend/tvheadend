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
#include <assert.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_support.h"
#include "diseqc.h"
#include "notify.h"
#include "dvr/dvr.h"
#if 0

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

    /* bit error rate */
    if(ioctl(tda->tda_fe_fd, FE_READ_BER, &tdmi->tdmi_ber) == -1)
      tdmi->tdmi_ber = -2;

    /* signal strength */
    if(ioctl(tda->tda_fe_fd, FE_READ_SIGNAL_STRENGTH, &tdmi->tdmi_signal) == -1)
      tdmi->tdmi_signal = -2;

    /* signal/noise ratio */
    if(ioctl(tda->tda_fe_fd, FE_READ_SNR, &tdmi->tdmi_snr) == -1)
      tdmi->tdmi_snr = -2;
  }

  if(status != tdmi->tdmi_fe_status) {
    tdmi->tdmi_fe_status = status;

    dvb_mux_nicename(buf, sizeof(buf), tdmi);
    tvhlog(LOG_DEBUG, 
	   "dvb", "\"%s\" on adapter \"%s\", status changed to %s",
	   buf, tda->tda_displayname, dvb_mux_status(tdmi));
    update = 1;
  }

  if(status != TDMI_FE_UNKNOWN) {
    if(tda->tda_qmon) {
      q = tdmi->tdmi_quality + (status - TDMI_FE_OK + 1);
      q = MAX(MIN(q, 100), 0);
    } else {
      q = 100;
    }
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

  if(tda->tda_allpids_dmx_fd != -1) {
    close(tda->tda_allpids_dmx_fd);
    tda->tda_allpids_dmx_fd = -1;
  }

  if(tda->tda_dump_fd != -1) {
    close(tda->tda_dump_fd);
    tda->tda_dump_fd = -1;
  }

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
 * Open a dump file which we write the entire mux output to
 */
static void
dvb_adapter_open_dump_file(th_dvb_adapter_t *tda)
{
  struct dmx_pes_filter_params dmx_param;
  char fullname[1000];
  char path[500];
  const char *fname = tda->tda_mux_current->tdmi_identifier;

  int fd = tvh_open(tda->tda_demux_path, O_RDWR, 0);
  if(fd == -1)
    return;

  memset(&dmx_param, 0, sizeof(dmx_param));
  dmx_param.pid = 0x2000;
  dmx_param.input = DMX_IN_FRONTEND;
  dmx_param.output = DMX_OUT_TS_TAP;
  dmx_param.pes_type = DMX_PES_OTHER;
  dmx_param.flags = DMX_IMMEDIATE_START;
  
  if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
    tvhlog(LOG_ERR, "dvb",
	   "\"%s\" unable to configure demuxer \"%s\" for all PIDs -- %s",
	   fname, tda->tda_demux_path, 
	   strerror(errno));
    close(fd);
    return;
  }

  snprintf(path, sizeof(path), "%s/muxdumps", 
      dvr_config_find_by_name_default("")->dvr_storage);

  if(mkdir(path, 0777) && errno != EEXIST) {
    tvhlog(LOG_ERR, "dvb", "\"%s\" unable to create mux dump dir %s -- %s",
	   fname, path, strerror(errno));
    close(fd);
    return;
  }

  int attempt = 1;

  while(1) {
    struct stat st;
    snprintf(fullname, sizeof(fullname), "%s/%s.dump%d.ts",
	     path, fname, attempt);

    if(stat(fullname, &st) == -1)
      break;
    
    attempt++;
  }
  
  int f = open(fullname, O_CREAT | O_TRUNC | O_WRONLY, 0777);

  if(f == -1) {
    tvhlog(LOG_ERR, "dvb", "\"%s\" unable to create mux dump file %s -- %s",
	   fname, fullname, strerror(errno));
    close(fd);
    return;
  }
	   
  tvhlog(LOG_WARNING, "dvb", "\"%s\" writing to mux dump file %s",
	 fname, fullname);

  tda->tda_allpids_dmx_fd = fd;
  tda->tda_dump_fd = f;
}



#if DVB_API_VERSION >= 5

static int check_frontend (int fe_fd, int dvr, int human_readable) {
  (void)dvr;
  fe_status_t status;
  uint16_t snr, signal;
  uint32_t ber, uncorrected_blocks;
  int timeout = 0;

  do {
    if (ioctl(fe_fd, FE_READ_STATUS, &status) == -1)
      perror("FE_READ_STATUS failed");
    /* some frontends might not support all these ioctls, thus we
     * avoid printing errors
     */
    if (ioctl(fe_fd, FE_READ_SIGNAL_STRENGTH, &signal) == -1)
      signal = -2;
    if (ioctl(fe_fd, FE_READ_SNR, &snr) == -1)
      snr = -2;
    if (ioctl(fe_fd, FE_READ_BER, &ber) == -1)
      ber = -2;
    if (ioctl(fe_fd, FE_READ_UNCORRECTED_BLOCKS, &uncorrected_blocks) == -1)
      uncorrected_blocks = -2;

    if (human_readable) {
      printf ("status %02x | signal %3u%% | snr %3u%% | ber %d | unc %d | ",
          status, (signal * 100) / 0xffff, (snr * 100) / 0xffff, ber, uncorrected_blocks);
    } else {
      printf ("status %02x | signal %04x | snr %04x | ber %08x | unc %08x | ",
          status, signal, snr, ber, uncorrected_blocks);
    }
    if (status & FE_HAS_LOCK)
      printf("FE_HAS_LOCK");
    printf("\n");

    if ((status & FE_HAS_LOCK) || (++timeout >= 10))
      break;

    usleep(1000000);
  } while (1);

  return 0;
}

static struct dtv_property clear_p[] = {
  { .cmd = DTV_CLEAR },
};

static struct dtv_properties clear_cmdseq = {
  .num = 1,
  .props = clear_p
};


/**
 *
 */
static int
dvb_fe_tune_s2(th_dvb_mux_instance_t *tdmi, dvb_mux_conf_t *dmc)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  int r;
  
  if ((ioctl(tda->tda_fe_fd, FE_SET_PROPERTY, &clear_cmdseq)) != 0)
    return -1;
  
  struct dvb_frontend_event ev;
  
  /* Support for legacy satellite tune, with the new API. */
  struct dtv_property _dvbs_cmdargs[] = {
    { .cmd = DTV_DELIVERY_SYSTEM, .u.data = dmc->dmc_fe_delsys },
    { .cmd = DTV_FREQUENCY,       .u.data = p->frequency },
    { .cmd = DTV_MODULATION,      .u.data = dmc->dmc_fe_modulation },
    { .cmd = DTV_SYMBOL_RATE,     .u.data = p->u.qpsk.symbol_rate },
    { .cmd = DTV_INNER_FEC,       .u.data = p->u.qpsk.fec_inner },
    { .cmd = DTV_INVERSION,       .u.data = INVERSION_AUTO },
    { .cmd = DTV_ROLLOFF,         .u.data = dmc->dmc_fe_rolloff },
    { .cmd = DTV_PILOT,           .u.data = PILOT_AUTO },
    { .cmd = DTV_TUNE },
  };

  struct dtv_properties _dvbs_cmdseq = {
    .num = 9,
    .props = _dvbs_cmdargs
  };

  /* discard stale QPSK events */
  while (1) {
    if (ioctl(tda->tda_fe_fd, FE_GET_EVENT, &ev) == -1)
      break;
  }

  /* do tuning now */
  r = ioctl(tda->tda_fe_fd, FE_SET_PROPERTY, &_dvbs_cmdseq);

  if(0)
    check_frontend (tda->tda_fe_fd, 0, 1);
  return r;

}

#endif

/**
 *
 */
int
dvb_fe_tune(th_dvb_mux_instance_t *tdmi, const char *reason)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  // copy dmc, cause frequency may be change with FE_QPSK
  dvb_mux_conf_t dmc = tdmi->tdmi_conf;
  dvb_frontend_parameters_t* p = &dmc.dmc_fe_params;
  
  char buf[256];
  int r;
 

  lock_assert(&global_lock);

  if(tda->tda_mux_current == tdmi)
    return 0;
  
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

    hiband = switchfreq && p->frequency > switchfreq;

    pol = tdmi->tdmi_conf.dmc_polarisation;
    if ((r = diseqc_setup(tda->tda_fe_fd,
		 port,
		 pol == POLARISATION_HORIZONTAL ||
		 pol == POLARISATION_CIRCULAR_LEFT,
		 hiband, tda->tda_diseqc_version)) != 0)
      tvhlog(LOG_ERR, "dvb", "diseqc setup failed %d\n", r);
      
    if(hiband)
      p->frequency = abs(p->frequency - hifreq);
    else
      p->frequency = abs(p->frequency - lowfreq);
  }

  dvb_mux_nicename(buf, sizeof(buf), tdmi);

  tda->tda_fe_monitor_hold = 4;


#if DVB_API_VERSION >= 5
  if (tda->tda_type == FE_QPSK) {
    tvhlog(LOG_DEBUG, "dvb", "\"%s\" tuning via s2api to \"%s\" (%d, %d Baud, "
	    "%s, %s, %s)", tda->tda_rootpath, buf, p->frequency, p->u.qpsk.symbol_rate, 
      dvb_mux_fec2str(p->u.qpsk.fec_inner), dvb_mux_delsys2str(dmc.dmc_fe_delsys), 
      dvb_mux_qam2str(dmc.dmc_fe_modulation));
  
    r = dvb_fe_tune_s2(tdmi, &dmc);
  } else
#endif
  {
    tvhlog(LOG_DEBUG, "dvb", "\"%s\" tuning to \"%s\" (%s)", tda->tda_rootpath, buf, reason);
    r = ioctl(tda->tda_fe_fd, FE_SET_FRONTEND, p);
  }

  if(r != 0) {
    tvhlog(LOG_ERR, "dvb", "\"%s\" tuning to \"%s\""
     " -- Front configuration failed -- %s, frequency: %ld",
     tda->tda_rootpath, buf, strerror(errno), p->frequency);
    return SM_CODE_TUNING_FAILED;
  }   

  tda->tda_mux_current = tdmi;

  if(tda->tda_dump_muxes)
    dvb_adapter_open_dump_file(tda);

  gtimer_arm(&tda->tda_fe_monitor_timer, dvb_fe_monitor, tda, 1);

  dvb_table_add_default(tdmi);

  dvb_adapter_notify(tda);
  return 0;
}
#endif
