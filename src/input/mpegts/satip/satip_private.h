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

#include "tvheadend.h"
#include "input.h"
#include "htsbuf.h"
#include "udp.h"
#include "http.h"
#include "satip.h"

#define SATIP_BUF_SIZE    (4000*188)

typedef struct satip_device_info satip_device_info_t;
typedef struct satip_device      satip_device_t;
typedef struct satip_tune_req    satip_tune_req_t;
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
  int rtsp_port;
  int srcs;
};

struct satip_device
{
  tvh_hardware_t;

  mtimer_t                   sd_destroy_timer;
  int                        sd_inload;
  int                        sd_nosave;

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
  char                      *sd_bindaddr;
  int                        sd_tcp_mode;
  int                        sd_fast_switch;
  int                        sd_fullmux_ok;
  int                        sd_pids_max;
  int                        sd_pids_len;
  int                        sd_pids_deladd;
  int                        sd_fe;
  int                        sd_sig_scale;
  char                      *sd_tunercfg;
  int                        sd_pids21;
  int                        sd_pilot_on;
  int                        sd_no_univ_lnb;
  int                        sd_can_weight;
  int                        sd_dbus_allow;
  int                        sd_skip_ts;
  int                        sd_disable_workarounds;
  int                        sd_wake_ref;
  pthread_mutex_t            sd_tune_mutex;
  TAILQ_HEAD(,satip_frontend)sd_serialize_queue;
};

struct satip_tune_req {
  mpegts_mux_instance_t     *sf_mmi;

  mpegts_apids_t             sf_pids;
  mpegts_apids_t             sf_pids_tuned;

  int                        sf_weight;
  int                        sf_weight_tuned;

  int                        sf_netlimit;
  int                        sf_netgroup;
  int                        sf_netposhash;
};

struct satip_udppkt {
  TAILQ_ENTRY(satip_udppkt)  up_link;
  uint8_t                   *up_data;
  uint16_t                   up_data_len;
  uint16_t                   up_data_seq;
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
  int                        sf_type_v2;
  char                      *sf_type_override;
  int                        sf_master;
  int                        sf_udp_rtp_port;
  int                        sf_transport_mode;
  int                        sf_play2;
  int                        sf_tdelay;
  int                        sf_grace_period;
  int                        sf_teardown_delay;
  int                        sf_pass_weight;
  int                        sf_specinv;
  int                        sf_delsys;
  char                      *sf_tuner_bindaddr;

  /*
   * Reception
   */
  pthread_t                  sf_dvr_thread;
  th_pipe_t                  sf_dvr_pipe;
  pthread_mutex_t            sf_dvr_lock;
  int                        sf_thread;
  int                        sf_running;
  int                        sf_tables;
  int                        sf_atsc_c;
  int                        sf_position;
  signal_state_t             sf_status;
  mtimer_t                   sf_monitor_timer;
  uint64_t                   sf_last_tune;
  satip_tune_req_t          *sf_req;
  satip_tune_req_t          *sf_req_thread;
  sbuf_t                     sf_sbuf;
  int                        sf_skip_ts;
  const char *               sf_display_name;
  uint32_t                   sf_seq;
  dvb_mux_t                 *sf_curmux;
  time_t                     sf_last_data_tstamp;
  int                        sf_netlimit;
  int                        sf_netgroup;
  int                        sf_netposhash;
  TAILQ_HEAD(,satip_udppkt)  sf_udp_packets;
  int                        sf_udp_packets_count;
 
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
  int                        sfc_grace;
  char                      *sfc_name;
  int                        sfc_network_limit;
  int                        sfc_network_group;

  /*
   * Assigned networks to this SAT configuration
   */
  idnode_set_t              *sfc_networks;
};

/*
 * Classes
 */
extern const idclass_t satip_frontend_class;

extern const idclass_t satip_frontend_dvbt_class;
extern const idclass_t satip_frontend_dvbs_class;
extern const idclass_t satip_frontend_dvbs_slave_class;
extern const idclass_t satip_frontend_atsc_t_class;
extern const idclass_t satip_frontend_atsc_c_class;

extern const idclass_t satip_satconf_class;

/*
 * Methods
 */
  
void satip_device_init ( void );

void satip_device_done ( void );

static inline void satip_device_changed ( satip_device_t *sd )
  { idnode_changed(&sd->th_id); }

void satip_device_destroy ( satip_device_t *sd );

void satip_device_destroy_later( satip_device_t *sd, int after_ms );

char *satip_device_nicename ( satip_device_t *sd, char *buf, int len );

int satip_frontend_match_satcfg
  ( satip_frontend_t *lfe2, mpegts_mux_t *mm2, int flags, int weight );

satip_frontend_t *
satip_frontend_create
  ( htsmsg_t *conf, satip_device_t *sd, dvb_fe_type_t type, int v2, int num );

void satip_frontend_save ( satip_frontend_t *lfe, htsmsg_t *m );

void satip_frontend_delete ( satip_frontend_t *lfe );

/*
 * SAT>IP Satconf configuration
 */
void satip_satconf_save ( satip_frontend_t *lfe, htsmsg_t *m );

void satip_satconf_destroy ( satip_frontend_t *lfe );

void satip_satconf_create
  ( satip_frontend_t *lfe, htsmsg_t *conf, int def_positions );

void satip_satconf_updated_positions
  ( satip_frontend_t *lfe );

int satip_satconf_get_priority
  ( satip_frontend_t *lfe, mpegts_mux_t *mm );

int satip_satconf_get_grace
  ( satip_frontend_t *lfe, mpegts_mux_t *mm );

satip_satconf_t *satip_satconf_get_position
  ( satip_frontend_t *lfe, mpegts_mux_t *mm, int *hash,
    int check, int flags, int weight );

/*
 * RTSP part
 */

#define SATIP_SETUP_TCP      (1<<0)
#define SATIP_SETUP_PLAY     (1<<1)
#define SATIP_SETUP_PILOT_ON (1<<2)
#define SATIP_SETUP_PIDS21   (1<<3)
#define SATIP_SETUP_FE       (1<<4)
#define SATIP_SETUP_SPECINV0 (1<<5)
#define SATIP_SETUP_SPECINV1 (1<<6)

int
satip_rtsp_setup( http_client_t *hc,
                  int src, int fe, int udp_port,
                  const dvb_mux_conf_t *dmc,
                  int flags, int weight );

int
satip_rtsp_play( http_client_t *hc, const char *pids,
                 const char *addpids, const char *delpids,
                 int max_pids_len, int weight );

#endif /* __TVH_SATIP_PRIVATE_H__ */
