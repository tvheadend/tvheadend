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

#include "tvhead.h"
#include "dispatch.h"
#include "input_dvb.h"
#include "output_client.h"
#include "channels.h"
#include "transports.h"
#include "teletext.h"

static struct th_dvb_adapter_list adapters;

static void dvr_fd_callback(int events, void *opaque, int fd);

static void dvb_datetime_add_demux(th_dvb_adapter_t *tda);

static void
dvb_add_adapter(const char *path)
{
  char fname[256];
  int fe;
  int dvr;
  th_dvb_adapter_t *tda;

  snprintf(fname, sizeof(fname), "%s/frontend0", path);
  
  fe = open(fname, O_RDWR | O_NONBLOCK);
  if(fe == -1)
    return;
    
  tda = calloc(1, sizeof(th_dvb_adapter_t));
  tda->tda_path = strdup(path);
  tda->tda_demux_path = malloc(256);
  snprintf(tda->tda_demux_path, 256, "%s/demux0", path);
  tda->tda_dvr_path = malloc(256);
  snprintf(tda->tda_dvr_path, 256, "%s/dvr0", path);


  dvr = open(tda->tda_dvr_path, O_RDONLY | O_NONBLOCK);
  if(dvr == -1) {
    fprintf(stderr, "%s: unable to open dvr\n", fname);
    free(tda);
    return;
  }


  tda->tda_fe_fd = fe;
  tda->tda_dvr_fd = dvr;

  if(ioctl(tda->tda_fe_fd, FE_GET_INFO, &tda->tda_fe_info)) {
    fprintf(stderr, "%s: Unable to query adapter\n", fname);
    close(fe);
    close(dvr);
    free(tda);
    return;
  }

  syslog(LOG_INFO, "Adding adapter %s (%s)", tda->tda_fe_info.name, path);

  tda->tda_name = strdup(tda->tda_fe_info.name);
  LIST_INSERT_HEAD(&adapters, tda, tda_link);
  dispatch_addfd(dvr, dvr_fd_callback, tda, DISPATCH_READ);

  dvb_datetime_add_demux(tda);
}




void
dvb_add_adapters(void)
{
  char path[200];
  int i;

  for(i = 0; i < 32; i++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d", i);
    dvb_add_adapter(path);
  }
}

#if 0
static void *
dvb_fe_loop(void *aux)
{
  th_dvb_adapter_t *tda = aux;
  struct timespec ts;

  while(1) {

    if(tda->tda_reconfigure == 0) {

      ts.tv_sec = time(NULL) + 1;
      ts.tv_nsec = 0;

      pthread_cond_timedwait(&tda->tda_cond, &tda->tda_mutex, &ts);
    }

    if(ioctl(tda->tda_fe_fd, FE_READ_STATUS, &tda->tda_fe_status) < 0)
      tda->tda_fe_status  = 0;

    if(ioctl(tda->tda_fe_fd, FE_READ_SIGNAL_STRENGTH, &tda->tda_signal) < 0)
      tda->tda_signal = 0;

    if(ioctl(tda->tda_fe_fd, FE_READ_SNR, &tda->tda_snr) < 0)
      tda->tda_snr = 0;

    if(ioctl(tda->tda_fe_fd, FE_READ_BER, &tda->tda_ber) < 0)
      tda->tda_ber = 0;

    if(ioctl(tda->tda_fe_fd, FE_READ_UNCORRECTED_BLOCKS, 
	     &tda->tda_uncorrected_blocks) < 0)
      tda->tda_uncorrected_blocks = 0;

    if(tda->tda_reconfigure != 0) {
      
      if(ioctl(tda->tda_fe_fd, FE_SET_FRONTEND, &tda->tda_fe_params) < 0) {
	perror("ioctl");
      }
      sleep(1);
      tda->tda_reconfigure = 0;
    }
  }
}
#endif



static void
dvb_dvr_process_packets(th_dvb_adapter_t *tda, uint8_t *tsb, int r)
{
  int pid;
  th_transport_t *t;

  while(r > 0) {
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    LIST_FOREACH(t, &tda->tda_transports, tht_adapter_link)
      transport_recv_tsb(t, pid, tsb);
    r -= 188;
    tsb += 188;
  }
}


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



void
dvb_stop_feed(th_transport_t *t)
{
  int i;

  t->tht_dvb_adapter = NULL;
  LIST_REMOVE(t, tht_adapter_link);
  for(i = 0; i < t->tht_npids; i++) {
    close(t->tht_pids[i].demuxer_fd);
    t->tht_pids[i].demuxer_fd = -1;
  }
  t->tht_status = TRANSPORT_IDLE;
  transport_flush_subscribers(t);
}

static void
dvb_adapter_clean(th_dvb_adapter_t *tda)
{
  th_transport_t *t;
  
  while((t = LIST_FIRST(&tda->tda_transports)) != NULL)
    dvb_stop_feed(t);
}


int
dvb_start_feed(th_transport_t *t, unsigned int weight)
{
  struct dvb_frontend_parameters *params;
  th_dvb_adapter_t *tda, *cand = NULL;
  struct dmx_pes_filter_params dmx_param;
  int w, i, fd, pid;

  params = &t->tht_dvb_fe_params;
  
  LIST_FOREACH(tda, &adapters, tda_link) {
    w = transport_compute_weight(&tda->tda_transports);
    if(w < weight)
      cand = tda;

    if(!memcmp(params, &tda->tda_fe_params, 
	       sizeof(struct dvb_frontend_parameters)))
      break;
  }

  if(tda == NULL) {
    if(cand == NULL)
      return 1;

    dvb_adapter_clean(cand);
    tda = cand;
  }

  for(i = 0; i < t->tht_npids; i++) {
    
    fd = open(tda->tda_demux_path, O_RDWR);
    pid = t->tht_pids[i].pid;
    t->tht_pids[i].cc_valid = 0;

    if(fd == -1) {
      fprintf(stderr, "Unable to open demux for pid %d\n", pid);
      return -1;
    }

    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid = pid;
    dmx_param.input = DMX_IN_FRONTEND;
    dmx_param.output = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags = DMX_IMMEDIATE_START;

    if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
      syslog(LOG_ERR, "%s (%s) -- Cannot demux pid %d -- %s",
	     tda->tda_name, tda->tda_path, pid, strerror(errno));
    }

    t->tht_pids[i].demuxer_fd = fd;
  }

  LIST_INSERT_HEAD(&tda->tda_transports, t, tht_adapter_link);
  t->tht_dvb_adapter = tda;
  t->tht_status = TRANSPORT_RUNNING;

  if(!memcmp(params, &tda->tda_fe_params, 
	     sizeof(struct dvb_frontend_parameters)))
    return 0;

  memcpy(&tda->tda_fe_params, params, sizeof(struct dvb_frontend_parameters));
  
  syslog(LOG_DEBUG, "%s (%s) tuning to transport \"%s\"", 
	 tda->tda_name, tda->tda_path, t->tht_name);

  i = ioctl(tda->tda_fe_fd, FE_SET_FRONTEND, &tda->tda_fe_params);
  if(i != 0) {
    syslog(LOG_ERR, "%s (%s) tuning to transport \"%s\""
	   " -- Front configuration failed -- %s",
	   tda->tda_name, tda->tda_path, t->tht_name,
	   strerror(errno));
  }
  return 0;
}




/* 
 *
 */


int
dvb_configure_transport(th_transport_t *t, const char *muxname)
{
  struct dvb_frontend_parameters *fr_param;
  config_entry_t *ce;
  char buf[100];
 
  if((ce = find_mux_config("dvbmux", muxname)) == NULL)
    return -1;

  t->tht_type = TRANSPORT_DVB;

  fr_param = &t->tht_dvb_fe_params;

  fr_param->inversion = INVERSION_AUTO;
  fr_param->u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
  fr_param->u.ofdm.code_rate_HP = FEC_2_3;
  fr_param->u.ofdm.code_rate_LP = FEC_1_2;
  fr_param->u.ofdm.constellation = QAM_64;
  fr_param->u.ofdm.transmission_mode = TRANSMISSION_MODE_8K;
  fr_param->u.ofdm.guard_interval = GUARD_INTERVAL_1_8;
  fr_param->u.ofdm.hierarchy_information = HIERARCHY_NONE;
 
  fr_param->frequency = 
    atoi(config_get_str_sub(&ce->ce_sub, "frequency", "0"));

  snprintf(buf, sizeof(buf), "DVB-T: %s (%.1f MHz)", muxname, 
	   (float)fr_param->frequency / 1000000.0f);
  t->tht_name = strdup(buf);

  return 0;
}


/*
 * DVB time and date functions
 */

#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

static time_t
convert_date(uint8_t *dvb_buf)
{
  int i;
  int year, month, day, hour, min, sec;
  long int mjd;
  struct tm dvb_time;

  mjd = (dvb_buf[0] & 0xff) << 8;
  mjd += (dvb_buf[1] & 0xff);
  hour = bcdtoint(dvb_buf[2] & 0xff);
  min = bcdtoint(dvb_buf[3] & 0xff);
  sec = bcdtoint(dvb_buf[4] & 0xff);
  /*
   * Use the routine specified in ETSI EN 300 468 V1.4.1,
   * "Specification for Service Information in Digital Video Broadcasting"
   * to convert from Modified Julian Date to Year, Month, Day.
   */
  year = (int) ((mjd - 15078.2) / 365.25);
  month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
  day = mjd - 14956 - (int) (year * 365.25) - (int) (month * 30.6001);
  if (month == 14 || month == 15)
    i = 1;
  else
    i = 0;
  year += i;
  month = month - 1 - i * 12;

  dvb_time.tm_sec = sec;
  dvb_time.tm_min = min;
  dvb_time.tm_hour = hour;
  dvb_time.tm_mday = day;
  dvb_time.tm_mon = month - 1;
  dvb_time.tm_year = year;
  dvb_time.tm_isdst = -1;
  dvb_time.tm_wday = 0;
  dvb_time.tm_yday = 0;
  return (timegm(&dvb_time));
}


static void
dvb_datetime_callback(int events, void *opaque, int fd)
{
  th_dvb_adapter_t *tda = opaque;
  uint8_t sec[4096];
  int seclen, r;
  time_t t;

  if(!(events & DISPATCH_READ))
    return;

  r = read(fd, sec, sizeof(sec));

  seclen = ((sec[1] & 0x0f) << 8) | (sec[2] & 0xff);
  if(r == seclen + 3) {
    t = convert_date(&sec[3]);
    tda->tda_time = t;
  }
}


static void
dvb_datetime_add_demux(th_dvb_adapter_t *tda)
{
  struct dmx_sct_filter_params fparams;
  int fd;

  fd = open(tda->tda_demux_path, O_RDWR);
  if(fd == -1)
    return;

  memset(&fparams, 0, sizeof(fparams));
  fparams.pid = 0x14;
  fparams.timeout = 0;
  fparams.flags = DMX_IMMEDIATE_START;
  fparams.filter.filter[0] = 0x70;
  fparams.filter.mask[0] = 0xff;
  
  if(ioctl(fd, DMX_SET_FILTER, &fparams) < 0) {
    close(fd);
    return;
  }

  dispatch_addfd(fd, dvb_datetime_callback, tda, DISPATCH_READ);
}
