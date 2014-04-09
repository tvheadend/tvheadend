/*
 *  Tvheadend - SAT-IP DVB private data
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_SATIP_PRIVATE_H__
#define __TVH_SATIP_PRIVATE_H__

#include "input.h"
#include "htsbuf.h"
#include "udp.h"
#include "satip.h"

#define SATIP_BUF_SIZE    (4000*188)

typedef struct satip_device_info satip_device_info_t;
typedef struct satip_device      satip_device_t;
typedef struct satip_frontend    satip_frontend_t;
typedef struct satip_satconf     satip_satconf_t;

struct satip_device_info
{
  char *myaddr;       /* IP address of this host received data from the SAT>IP device */
  char *addr;         /* IP address */
  char *uuid;
  char *bootid;
  char *configid;
  char *deviceid;
  char *location;     /*< URL of the XML file */
  char *server;
  char *friendlyname;
  char *manufacturer;
  char *manufacturerURL;
  char *modeldesc;
  char *modelname;
  char *modelnum;
  char *serialnum;
  char *presentation;
  char *tunercfg;     /*< XML urn:ses-com:satipX_SATIPCAP contents */
};

struct satip_device
{
  tvh_hardware_t;

  /*
   * Adapter info
   */
  satip_device_info_t        sd_info;

  /*
   * Frontends
   */
  TAILQ_HEAD(,satip_frontend) sd_frontends;

  /*
   * RTSP
   */
  int                        sd_fullmux_ok;
  int                        sd_pids_max;
  int                        sd_pids_len;
  int                        sd_pids_deladd;
  int                        sd_sig_scale;

  int                        sd_rtsp_running;
  pthread_t                  sd_rtsp_tid;
  pthread_mutex_t            sd_rtsp_lock;
  pthread_cond_t             sd_rtsp_cond;
  TAILQ_HEAD(,satip_rtsp_request) sd_rtsp_queue;
  time_t                     sd_rtsp_ping;
  gtimer_t                   sd_rtsp_shutdown;
};

struct satip_frontend
{
  mpegts_input_t;

  /*
   * Device
   */
  satip_device_t            *sf_device;
  TAILQ_ENTRY(satip_frontend) sf_link;

  /*
   * Frontend info
   */
  int                        sf_number;
  dvb_fe_type_t              sf_type;
  int                        sf_type_t2;
  int                        sf_udp_rtp_port;
  int                        sf_fullmux;

  /*
   * Reception
   */
  pthread_t                  sf_dvr_thread;
  th_pipe_t                  sf_dvr_pipe;
  pthread_mutex_t            sf_dvr_lock;
  pthread_cond_t             sf_dvr_cond;
  uint16_t                  *sf_pids;
  uint16_t                  *sf_pids_tuned;
  int                        sf_pids_any;
  int                        sf_pids_any_tuned;
  int                        sf_pids_size;
  int                        sf_pids_count;
  int                        sf_pids_tcount;     /*< tuned count */
  int                        sf_running;
  int                        sf_position;
  udp_connection_t          *sf_rtp;
  udp_connection_t          *sf_rtcp;
  int                        sf_rtp_port;
  mpegts_mux_instance_t     *sf_mmi;
  signal_state_t             sf_status;
  gtimer_t                   sf_monitor_timer;
 
  /*
   * Configuration
   */
  int                        sf_positions;
  TAILQ_HEAD(,satip_satconf) sf_satconf;
};

struct satip_satconf
{

  idnode_t                   sfc_id;
  /*
   * Parent
   */
  satip_frontend_t          *sfc_lfe;
  TAILQ_ENTRY(satip_satconf) sfc_link; 

  /*
   * Config
   */
  int                        sfc_enabled;
  int                        sfc_position;
  int                        sfc_priority;
  char                      *sfc_name;

  /*
   * Assigned networks to this SAT configuration
   */
  idnode_set_t              *sfc_networks;
};

/*
 * Methods
 */
  
void satip_device_init ( void );

void satip_device_done ( void );

void satip_device_save ( satip_device_t *sd );

void satip_device_destroy ( satip_device_t *sd );

satip_frontend_t *
satip_frontend_create
  ( htsmsg_t *conf, satip_device_t *sd, dvb_fe_type_t type, int t2, int num );

void satip_frontend_save ( satip_frontend_t *lfe, htsmsg_t *m );

void satip_frontend_delete ( satip_frontend_t *lfe );

/*
 * SAT>IP Satconf configuration
 */
void satip_satconf_save ( satip_frontend_t *lfe, htsmsg_t *m );

void satip_satconf_destroy ( satip_frontend_t *lfe );

void satip_satconf_create
  ( satip_frontend_t *lfe, htsmsg_t *conf );

void satip_satconf_updated_positions
  ( satip_frontend_t *lfe );

int satip_satconf_get_priority
  ( satip_frontend_t *lfe, mpegts_mux_t *mm );

int satip_satconf_get_position
  ( satip_frontend_t *lfe, mpegts_mux_t *mm );

/*
 * RTSP part
 */

typedef enum {
  SATIP_RTSP_CMD_NONE,
  SATIP_RTSP_CMD_OPTIONS,
  SATIP_RTSP_CMD_SETUP,
  SATIP_RTSP_CMD_PLAY,
  SATIP_RTSP_CMD_TEARDOWN,
  SATIP_RTSP_CMD_DESCRIBE
} satip_rtsp_cmd_t;

typedef struct satip_rtsp_connection {
  /* decoded answer */
  int              cseq;
  int              code;
  char            *header;
  char            *data;
  /* state variables */
  int              sending;
  satip_rtsp_cmd_t cmd;
  int              port;
  int              client_port;
  int              timeout;
  char            *session;
  uint64_t         stream_id;
  /* internal data */
  satip_device_t  *device;
  int              fd;
  char             rbuf[4096];
  size_t           rsize;
  char            *wbuf;
  size_t           wpos;
  size_t           wsize;
  htsbuf_queue_t   wq2;
  satip_rtsp_cmd_t wq2_cmd;
  int              wq2_loaded;
  time_t           ping_time;
} satip_rtsp_connection_t;

satip_rtsp_connection_t *
satip_rtsp_connection( satip_device_t *sd );

void
satip_rtsp_connection_close( satip_rtsp_connection_t *conn );

int
satip_rtsp_send_partial( satip_rtsp_connection_t *conn );

int 
satip_rtsp_send( satip_rtsp_connection_t *conn, htsbuf_queue_t *q,
                 satip_rtsp_cmd_t cmd );

int
satip_rtsp_receive( satip_rtsp_connection_t *conn );

int
satip_rtsp_options_decode( satip_rtsp_connection_t *conn );

void
satip_rtsp_options( satip_rtsp_connection_t *conn );

int
satip_rtsp_setup_decode( satip_rtsp_connection_t *conn );

int
satip_rtsp_setup( satip_rtsp_connection_t *conn,
                  int src, int fe, int udp_port,
                  const dvb_mux_conf_t *dmc,
                  int connection_close );

int
satip_rtsp_play( satip_rtsp_connection_t *sd, const char *pids,
                 const char *addpids, const char *delpids );

int
satip_rtsp_teardown( satip_rtsp_connection_t *conn );

#endif /* __TVH_SATIP_PRIVATE_H__ */
