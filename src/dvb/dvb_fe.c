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
#include "service.h"
#include "streaming.h"

#include "epggrab.h"

/**
 * Return uncorrected block (since last read)
 *
 * Some adapters report delta themselfs, some return ever increasing value
 * we need to deal with that ourselfs
 */
static int
dvb_fe_get_unc(th_dvb_adapter_t *tda)
{
  uint32_t fec;
  int d, r;

  if(ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &fec))
    return 0; // read failed, just say 0

  if(tda->tda_unc_is_delta)
    return fec;

  d = (int)fec - (int)tda->tda_last_fec;

  if(d < 0) {
    tda->tda_unc_is_delta = 1;
    tvhlog(LOG_DEBUG, "dvb",
	   "%s: FE_READ_UNCORRECTED_BLOCKS returns delta updates (delta=%d)",
	   tda->tda_displayname, d);
    return fec;
  }
  
  r = fec - tda->tda_last_fec;

  tda->tda_last_fec = fec;
  return r;
}



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
  int status, v, vv, i, fec, q;
  th_dvb_mux_instance_t *tdmi = tda->tda_mux_current;
  char buf[50];
  signal_status_t sigstat;
  streaming_message_t sm;
  struct service *t;

  int store = 0;
  int notify = 0;

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
      dvb_fe_get_unc(tda);
    } else {
      tda->tda_fe_monitor_hold--;
      return;
    }
  }

  if(status == -1) {
    /* Read FEC counter (delta) */

    fec = dvb_fe_get_unc(tda);

    if(tdmi->tdmi_unc != fec) {
      tdmi->tdmi_unc = fec;
      notify = 1;
    }

    tdmi->tdmi_fec_err_histogram[tdmi->tdmi_fec_err_ptr++] = fec;
    if(tdmi->tdmi_fec_err_ptr == TDMI_FEC_ERR_HISTOGRAM_SIZE)
      tdmi->tdmi_fec_err_ptr = 0;

    v = vv = 0;
    for(i = 0; i < TDMI_FEC_ERR_HISTOGRAM_SIZE; i++) {
      if(tdmi->tdmi_fec_err_histogram[i] > DVB_FEC_ERROR_LIMIT)
	v++;
      vv += tdmi->tdmi_fec_err_histogram[i];
    }

    float avg = (float)vv / TDMI_FEC_ERR_HISTOGRAM_SIZE;

    if(tdmi->tdmi_unc_avg != avg) {
      tdmi->tdmi_unc_avg = avg;
      notify = 1;
    }

    if(v == 0) {
      status = TDMI_FE_OK;
    } else if(v == 1) {
      status = TDMI_FE_BURSTY_FEC;
    } else {
      status = TDMI_FE_CONSTANT_FEC;
    }

    int v;
    /* bit error rate */
    if(ioctl(tda->tda_fe_fd, FE_READ_BER, &v) != -1 && v != tdmi->tdmi_ber) {
      tdmi->tdmi_ber = v;
      notify = 1;
    }

    /* signal strength */
    if(ioctl(tda->tda_fe_fd, FE_READ_SIGNAL_STRENGTH, &v) != -1 && v != tdmi->tdmi_signal) {
      tdmi->tdmi_signal = v;
      notify = 1;
    }

    /* signal/noise ratio */
    if(tda->tda_snr_valid) {
      if(ioctl(tda->tda_fe_fd, FE_READ_SNR, &v) != -1) {
        float snr = v / 10.0;
        if(tdmi->tdmi_snr != snr) {
          tdmi->tdmi_snr = snr;
          notify = 1;
        }
      }
    }
  }

  if(status != tdmi->tdmi_fe_status) {
    tdmi->tdmi_fe_status = status;

    dvb_mux_nicename(buf, sizeof(buf), tdmi);
    tvhlog(LOG_DEBUG,
	   "dvb", "\"%s\" on adapter \"%s\", status changed to %s",
	   buf, tda->tda_displayname, dvb_mux_status(tdmi));
    store = 1;
    notify = 1;
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
      store = 1;
      notify = 1;
    }
  }

  if(notify) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "id", tdmi->tdmi_identifier);
    htsmsg_add_u32(m, "quality", tdmi->tdmi_quality);
    htsmsg_add_u32(m, "signal", tdmi->tdmi_signal);

    if(tda->tda_snr_valid)
      htsmsg_add_dbl(m, "snr", tdmi->tdmi_snr);
    htsmsg_add_u32(m, "ber", tdmi->tdmi_ber);
    htsmsg_add_u32(m, "unc", tdmi->tdmi_unc);
    notify_by_msg("dvbMux", m);

    m = htsmsg_create_map();
    htsmsg_add_str(m, "identifier", tda->tda_identifier);
    htsmsg_add_u32(m, "signal", MIN(tdmi->tdmi_signal * 100 / 65535, 100));
    if(tda->tda_snr_valid)
      htsmsg_add_dbl(m, "snr", tdmi->tdmi_snr);
    htsmsg_add_u32(m, "ber", tdmi->tdmi_ber);
    htsmsg_add_u32(m, "unc", tdmi->tdmi_unc);
    htsmsg_add_dbl(m, "uncavg", tdmi->tdmi_unc_avg);
    notify_by_msg("tvAdapter", m);
  }

  if(store)
    dvb_mux_save(tdmi);

  /* Streaming message */
  sigstat.status_text = dvb_mux_status(tdmi);
  sigstat.snr         = tdmi->tdmi_snr;
  sigstat.signal      = tdmi->tdmi_signal;
  sigstat.ber         = tdmi->tdmi_ber;
  sigstat.unc         = tdmi->tdmi_unc;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;
  LIST_FOREACH(t, &tda->tda_transports, s_active_link)
    if(t->s_dvb_mux_instance == tda->tda_mux_current && t->s_status == SERVICE_RUNNING ) {
      pthread_mutex_lock(&t->s_stream_mutex);
      streaming_pad_deliver(&t->s_streaming_pad, &sm);
      pthread_mutex_unlock(&t->s_stream_mutex);
    }

}


/**
 * Stop the given TDMI
 */
void
dvb_fe_stop(th_dvb_mux_instance_t *tdmi, int retune)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  lock_assert(&global_lock);

  assert(tdmi == tda->tda_mux_current);
  tda->tda_mux_current = NULL;

  if(tdmi->tdmi_table_initial) {
    tdmi->tdmi_table_initial = 0;
    tda->tda_initial_num_mux--;
    dvb_mux_save(tdmi);
  }

  dvb_table_flush_all(tdmi);

  assert(tdmi->tdmi_scan_queue == NULL);

  if(tdmi->tdmi_enabled) {
    dvb_mux_add_to_scan_queue(tdmi);
  }
  
  epggrab_mux_stop(tdmi, 0);
  
  time(&tdmi->tdmi_lost_adapter);

  if (!retune) {
    gtimer_disarm(&tda->tda_fe_monitor_timer);
    dvb_adapter_stop(tda);
  }
}

#if DVB_API_VERSION >= 5

static int check_frontend (int fe_fd, int dvr, int human_readable) {
  (void)dvr;
  fe_status_t status;
  uint16_t snr, signal;
  uint32_t ber;
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

    if (human_readable) {
      printf ("status %02x | signal %3u%% | snr %3u%% | ber %d | ",
          status, (signal * 100) / 0xffff, (snr * 100) / 0xffff, ber);
    } else {
      printf ("status %02x | signal %04x | snr %04x | ber %08x | ",
          status, signal, snr, ber);
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
    dvb_fe_stop(tda->tda_mux_current, 1);
  else
    dvb_adapter_start(tda);
      
  if(tda->tda_type == FE_QPSK) {
	
    /* DVB-S */
    dvb_satconf_t *sc;
    int port, lowfreq, hifreq, switchfreq, hiband, pol, dbsbs;

    lowfreq = 9750000;
    hifreq = 10600000;
    switchfreq = 11700000;
    port = 0;
    dbsbs = 0;

    if((sc = tdmi->tdmi_conf.dmc_satconf) != NULL) {
      port = sc->sc_port;

      if(sc->sc_lnb != NULL)
      	dvb_lnb_get_frequencies(sc->sc_lnb, &lowfreq, &hifreq, &switchfreq);
      if(!strcmp(sc->sc_id ?: "", "DBS Bandstacked"))
        dbsbs = 1;
    }

    if(dbsbs) {
      hiband = 0;
      if(tdmi->tdmi_conf.dmc_polarisation == POLARISATION_HORIZONTAL ||
         tdmi->tdmi_conf.dmc_polarisation == POLARISATION_CIRCULAR_LEFT)
        p->frequency = abs(p->frequency - hifreq);
      else
        p->frequency = abs(p->frequency - lowfreq);
      pol = POLARISATION_CIRCULAR_LEFT;
    } else {
      hiband = switchfreq && p->frequency > switchfreq;
      pol = tdmi->tdmi_conf.dmc_polarisation;
      if(hiband)
        p->frequency = abs(p->frequency - hifreq);
      else
        p->frequency = abs(p->frequency - lowfreq);
    }
 
    if ((r = diseqc_setup(tda->tda_fe_fd, port,
                          pol == POLARISATION_HORIZONTAL ||
                          pol == POLARISATION_CIRCULAR_LEFT,
                          hiband, tda->tda_diseqc_version,
                          tda->tda_diseqc_repeats)) != 0)
      tvhlog(LOG_ERR, "dvb", "diseqc setup failed %d\n", r);
    }

  dvb_mux_nicename(buf, sizeof(buf), tdmi);

  tda->tda_fe_monitor_hold = 4;


#if DVB_API_VERSION >= 5
  if (tda->tda_type == FE_QPSK) {
    tvhlog(LOG_DEBUG, "dvb", "\"%s\" tuning via s2api to \"%s\" (%d, %d Baud, "
	    "%s, %s, %s) for %s", tda->tda_rootpath, buf, p->frequency, p->u.qpsk.symbol_rate, 
      dvb_mux_fec2str(p->u.qpsk.fec_inner), dvb_mux_delsys2str(dmc.dmc_fe_delsys), 
      dvb_mux_qam2str(dmc.dmc_fe_modulation), reason);
  
    r = dvb_fe_tune_s2(tdmi, &dmc);
  } else
#endif
  {
    tvhlog(LOG_DEBUG, "dvb", "\"%s\" tuning to \"%s\" (%s)", tda->tda_rootpath, buf, reason);
    r = ioctl(tda->tda_fe_fd, FE_SET_FRONTEND, p);
  }

  if(r != 0) {
    tvhlog(LOG_ERR, "dvb", "\"%s\" tuning to \"%s\""
     " -- Front configuration failed -- %s, frequency: %u",
     tda->tda_rootpath, buf, strerror(errno), p->frequency);

    /* Remove from initial scan set */
    if(tdmi->tdmi_table_initial) {
      tdmi->tdmi_table_initial = 0;
      tda->tda_initial_num_mux--;
    }

    /* Mark as bad */
    dvb_mux_set_enable(tdmi, 0);
    return SM_CODE_TUNING_FAILED;
  }   

  tda->tda_mux_current = tdmi;

  gtimer_arm(&tda->tda_fe_monitor_timer, dvb_fe_monitor, tda, 1);


  dvb_table_add_default(tdmi);
  epggrab_mux_start(tdmi);

  dvb_adapter_notify(tda);
  return 0;
}
