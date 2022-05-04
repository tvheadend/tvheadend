/*
 *  Tvheadend - Linux DVB private data
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_LINUXDVB_PRIVATE_H__
#define __TVH_LINUXDVB_PRIVATE_H__

#include "input.h"

#if ENABLE_LINUXDVB
#include <linux/dvb/version.h>
#endif

#if ENABLE_LINUXDVB_CA
#include "../en50221/en50221.h"
#include <linux/dvb/ca.h>
#endif

#define DVB_VER_INT(maj,min) (((maj) << 16) + (min))

#define DVB_VER_ATLEAST(maj, min) \
 (DVB_VER_INT(DVB_API_VERSION,  DVB_API_VERSION_MINOR) >= DVB_VER_INT(maj, min))

typedef struct linuxdvb_hardware    linuxdvb_hardware_t;
typedef struct linuxdvb_adapter     linuxdvb_adapter_t;
typedef struct linuxdvb_frontend    linuxdvb_frontend_t;
#if ENABLE_LINUXDVB_CA
typedef struct linuxdvb_transport   linuxdvb_transport_t;
typedef struct linuxdvb_ca          linuxdvb_ca_t;
typedef struct linuxdvb_ca_write    linuxdvb_ca_write_t;
#endif
#if ENABLE_DDCI
typedef struct linuxdvb_ddci        linuxdvb_ddci_t;
#endif
typedef struct linuxdvb_satconf     linuxdvb_satconf_t;
typedef struct linuxdvb_satconf_ele linuxdvb_satconf_ele_t;
typedef struct linuxdvb_diseqc      linuxdvb_diseqc_t;
typedef struct linuxdvb_lnb         linuxdvb_lnb_t;
typedef struct linuxdvb_network     linuxdvb_network_t;
typedef struct linuxdvb_en50494     linuxdvb_en50494_t;

typedef LIST_HEAD(,linuxdvb_hardware) linuxdvb_hardware_list_t;
typedef TAILQ_HEAD(linuxdvb_satconf_ele_list,linuxdvb_satconf_ele) linuxdvb_satconf_ele_list_t;
#if ENABLE_LINUXDVB_CA
typedef TAILQ_HEAD(linuxdvb_ca_write_queue,linuxdvb_ca_write) linuxdvb_ca_write_queue_t;
#endif

struct linuxdvb_adapter
{
  tvh_hardware_t;

  /*
   * Adapter info
   */
  char    *la_name;
  char    *la_rootpath;
  int      la_dvb_number;

  /*
   * Frontends
   */
  LIST_HEAD(,linuxdvb_frontend) la_frontends;

  /*
   *  CA devices
   */
#if ENABLE_LINUXDVB_CA
  LIST_HEAD(,linuxdvb_transport) la_ca_transports;
#endif
  /*
  * Functions
  */
  int (*la_is_enabled) ( linuxdvb_adapter_t *la );
};

struct linuxdvb_frontend
{
  mpegts_input_t;

  /*
   * Adapter
   */
  linuxdvb_adapter_t           *lfe_adapter;
  LIST_ENTRY(linuxdvb_frontend) lfe_link;

  /*
   * Frontend info
   */
  int                       lfe_number;
  dvb_fe_type_t             lfe_type;
  char                     *lfe_name;
  char                     *lfe_fe_path;
  char                     *lfe_dmx_path;
  char                     *lfe_dvr_path;
  char                     *lfe_sysfs;

  /*
   * Reception
   */
  int                       lfe_fe_fd;
  pthread_t                 lfe_dvr_thread;
  th_pipe_t                 lfe_dvr_pipe;
  tvh_mutex_t           lfe_dvr_lock;
  tvh_cond_t                lfe_dvr_cond;
  mpegts_apids_t            lfe_pids;
  int                       lfe_pids_max;
  int                       lfe_pids_use_all;
 
  /*
   * Tuning
   */
  int                       lfe_refcount;
  int                       lfe_ready;
  int                       lfe_in_setup;
  int                       lfe_locked;
  int                       lfe_status;
  int                       lfe_status2;
  int                       lfe_ioctls;
  int                       lfe_nodata;
  int                       lfe_freq;
  int64_t                   lfe_monitor;
  mtimer_t                  lfe_monitor_timer;
  tvhlog_limit_t            lfe_status_log;

  /*
   * Configuration
   */
  char                     *lfe_master;
  int                       lfe_powersave;
  int                       lfe_tune_repeats;
  uint32_t                  lfe_skip_bytes;
  uint32_t                  lfe_ibuf_size;
  uint32_t                  lfe_status_period;
  int                       lfe_old_status;
  int                       lfe_lna;
  uint32_t                  lfe_sig_multiplier;
  uint32_t                  lfe_snr_multiplier;

  /*
   * Satconf (DVB-S only)
   */
  linuxdvb_satconf_t       *lfe_satconf;
};

#if ENABLE_LINUXDVB_CA
struct linuxdvb_transport
{
  LIST_ENTRY(linuxdvb_transport)  lcat_link;
  LIST_ENTRY(linuxdvb_transport)  lcat_all_link;
  en50221_transport_t            *lcat_transport;
  LIST_HEAD(, linuxdvb_ca)        lcat_slots;
  char                           *lcat_name;

  /*
   * Adapter
   */
  linuxdvb_adapter_t             *lcat_adapter;
  int                             lcat_number;
  char                           *lcat_ca_path;
  int                             lcat_ca_fd;
  int                             lcat_fatal;
  int                             lcat_enabled;
  int64_t                         lcat_fatal_time;
  int64_t                         lcat_close_time;

#if ENABLE_DDCI
  linuxdvb_ddci_t                *lddci;
#endif
};

struct linuxdvb_ca
{
  idnode_t                  lca_id;

  /*
   * Transport
   */
  LIST_ENTRY(linuxdvb_ca)   lca_link;
  LIST_ENTRY(linuxdvb_ca)   lca_all_link;
  linuxdvb_transport_t     *lca_transport;

  /*
   * CA handling
   */
  int                       lca_enabled;
  int                       lca_phys_layer;
  int                       lca_high_bitrate_mode;
  int                       lca_capmt_query;
  int                       lca_capmt_interval;
  int                       lca_capmt_query_interval;
  int64_t                   lca_capmt_blocked;
  pthread_t                 lca_en50221_thread;
  int                       lca_en50221_thread_running;

  /*
   * EN50221
   */

  /*
   * CA info
   */
  int                       lca_adapnum;
  uint8_t                   lca_slotnum;
  char                     *lca_name;
  int                       lca_state;
  char                     *lca_modulename;
  linuxdvb_ca_write_queue_t lca_write_queue;

  /*
   * CAM module info
   */
  int                       lca_pin_reply;
  char                     *lca_pin_str;
  char                     *lca_pin_match_str;
};
#endif

struct linuxdvb_satconf
{
  idnode_t               ls_id;
  const char            *ls_type;

  /*
   * MPEG-TS hooks
   */
  mpegts_input_t        *ls_frontend; ///< Frontend we're proxying for
  mpegts_mux_instance_t *ls_mmi;      ///< Used within delay diseqc handler

  /*
   * Tuning
   */
  int                    ls_early_tune;

  /*
   * Diseqc handling
   */
  mtimer_t               ls_diseqc_timer;
  int                    ls_diseqc_idx;
  int                    ls_diseqc_repeats;
  int                    ls_diseqc_full;
  int                    ls_switch_rotor;

  /*
   * LNB settings
   */
  int                    ls_lnb_poweroff;
  int                    ls_lnb_highvol;
  uint32_t               ls_max_rotor_move;
  uint32_t               ls_min_rotor_move;
  double                 ls_site_lat;
  double                 ls_site_lon;
  uint32_t               ls_motor_rate;
  int                    ls_site_lat_south;
  int                    ls_site_lon_west;
  int                    ls_site_altitude;
  char                  *ls_rotor_extcmd;
  
  /*
   * Satconf elements
   */
  linuxdvb_satconf_ele_list_t ls_elements;
  linuxdvb_satconf_ele_t *ls_last_switch;
  linuxdvb_diseqc_t     *ls_active_diseqc;
  int                    ls_last_switch_pol;
  int                    ls_last_switch_band;
  int                    ls_last_vol;
  int                    ls_last_toneburst;
  int                    ls_last_tone_off;
  int                    ls_last_orbital_pos;
  int                    ls_last_queued_pos;
};

/*
 * Elementary satconf entry
 */
struct linuxdvb_satconf_ele
{
  idnode_t               lse_id;
  /*
   * Parent
   */
  linuxdvb_satconf_t               *lse_parent;
  TAILQ_ENTRY(linuxdvb_satconf_ele)  lse_link;

  /*
   * Config
   */
  int                    lse_enabled;
  int                    lse_priority;
  char                  *lse_name;

  /*
   * Assigned networks to this SAT configuration
   */
  idnode_set_t          *lse_networks;

  /*
   * Diseqc kit
   */
  linuxdvb_lnb_t        *lse_lnb;
  linuxdvb_diseqc_t     *lse_switch;
  linuxdvb_diseqc_t     *lse_rotor;
  linuxdvb_diseqc_t     *lse_en50494;
};

struct linuxdvb_diseqc
{
  idnode_t              ld_id;
  const char           *ld_type;
  linuxdvb_satconf_ele_t   *ld_satconf;
  int (*ld_grace) (linuxdvb_diseqc_t *ld, dvb_mux_t *lm);
  int (*ld_freq)  (linuxdvb_diseqc_t *ld, dvb_mux_t *lm, int freq);
  int (*ld_match) (linuxdvb_diseqc_t *ld, dvb_mux_t *lm1, dvb_mux_t *lm2);
  int (*ld_start) (linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
                   linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls);
  int (*ld_tune)  (linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
                   linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls,
                   int vol, int pol, int band, int freq);
  int (*ld_post)  (linuxdvb_diseqc_t *ld,
                   linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls);
};

struct linuxdvb_lnb
{
  linuxdvb_diseqc_t;
  uint32_t  (*lnb_freq) (linuxdvb_lnb_t*, dvb_mux_t*);
  int       (*lnb_match)(linuxdvb_lnb_t*, dvb_mux_t*, dvb_mux_t*);
  int       (*lnb_band) (linuxdvb_lnb_t*, dvb_mux_t*);
  int       (*lnb_pol)  (linuxdvb_lnb_t*, dvb_mux_t*);
};

#define UNICABLE_EN50494     50494
#define UNICABLE_EN50607     50607

struct linuxdvb_en50494
{
  linuxdvb_diseqc_t;

  /* en50494 configuration */
  uint16_t  le_position;  /* satelitte A(0) or B(1) */
  uint16_t  le_frequency; /* user band frequency in MHz */
  uint16_t  le_id;        /* user band id 0-7 */
  uint16_t  le_pin;       /* 0-255 or LINUXDVB_EN50494_NOPIN */

  /* runtime */
  uint32_t  le_tune_freq; /* the real frequency to tune to */
};

/*
 * Classes
 */

extern const idclass_t linuxdvb_adapter_class;
extern const idclass_t linuxdvb_ca_class;

extern const idclass_t linuxdvb_frontend_dvbt_class;
extern const idclass_t linuxdvb_frontend_dvbs_class;
extern const idclass_t linuxdvb_frontend_dvbs_slave_class;
extern const idclass_t linuxdvb_frontend_dvbc_class;
extern const idclass_t linuxdvb_frontend_atsc_t_class;
extern const idclass_t linuxdvb_frontend_atsc_c_class;
extern const idclass_t linuxdvb_frontend_isdb_t_class;
extern const idclass_t linuxdvb_frontend_isdb_c_class;
extern const idclass_t linuxdvb_frontend_isdb_s_class;
extern const idclass_t linuxdvb_frontend_dab_class;

extern const idclass_t linuxdvb_lnb_class;
extern const idclass_t linuxdvb_rotor_class;
extern const idclass_t linuxdvb_rotor_gotox_class;
extern const idclass_t linuxdvb_rotor_usals_class;
extern const idclass_t linuxdvb_en50494_class;
extern const idclass_t linuxdvb_switch_class;
extern const idclass_t linuxdvb_diseqc_class;

extern const idclass_t linuxdvb_satconf_class;
extern const idclass_t linuxdvb_satconf_lnbonly_class;
extern const idclass_t linuxdvb_satconf_2port_class;
extern const idclass_t linuxdvb_satconf_4port_class;
extern const idclass_t linuxdvb_satconf_en50494_class;
extern const idclass_t linuxdvb_satconf_advanced_class;
extern const idclass_t linuxdvb_satconf_ele_class;

/*
 * Methods
 */
  
#define LINUXDVB_SUBSYS_FE  0x01
#define LINUXDVB_SUBSYS_DVR 0x02

void linuxdvb_adapter_init ( void );

void linuxdvb_adapter_done ( void );

static inline void linuxdvb_adapter_changed ( linuxdvb_adapter_t *la )
  { idnode_changed(&la->th_id); }

int  linuxdvb_adapter_current_weight ( linuxdvb_adapter_t *la );

const void *linuxdvb_frontend_class_active_get ( void *obj );

linuxdvb_frontend_t *
linuxdvb_frontend_create
  ( htsmsg_t *conf, linuxdvb_adapter_t *la, int number,
    const char *fe_path, const char *dmx_path, const char *dvr_path,
    dvb_fe_type_t type, const char *name );

void linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *m );

void linuxdvb_frontend_destroy ( linuxdvb_frontend_t *lfe );

void linuxdvb_frontend_add_network
  ( linuxdvb_frontend_t *lfe, linuxdvb_network_t *net );

int linuxdvb_frontend_clear
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi );
int linuxdvb_frontend_tune0
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq );
int linuxdvb_frontend_tune1
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq );

int linuxdvb2tvh_delsys ( int delsys );

#if ENABLE_LINUXDVB_CA

linuxdvb_transport_t *
linuxdvb_transport_create( linuxdvb_adapter_t *la, int number, int slots,
                           const char *ca_path, const char *ci_path );
void linuxdvb_transport_destroy( linuxdvb_transport_t *lcat );
void linuxdvb_transport_save( linuxdvb_transport_t *lcat, htsmsg_t *m );

linuxdvb_ca_t * linuxdvb_ca_create
  ( htsmsg_t *conf, linuxdvb_transport_t *lcat, int slotnum );

void linuxdvb_ca_enqueue_capmt
  (linuxdvb_ca_t *lca, const uint8_t *capmt, size_t capmtlen, int descramble);

void linuxdvb_ca_init(void);
void linuxdvb_ca_done(void);

#endif

#if ENABLE_DDCI

linuxdvb_ddci_t *
linuxdvb_ddci_create ( linuxdvb_transport_t *lcat, const char *ci_path);
void
linuxdvb_ddci_destroy ( linuxdvb_ddci_t *lddci );
int
linuxdvb_ddci_open ( linuxdvb_ddci_t *lddci );
void
linuxdvb_ddci_close ( linuxdvb_ddci_t *lddci );
void
linuxdvb_ddci_put ( linuxdvb_ddci_t *lddci, service_t *t,
                    const uint8_t *tsb, int len );
void
linuxdvb_ddci_assign ( linuxdvb_ddci_t *lddci, service_t *t );
void
linuxdvb_ddci_unassign ( linuxdvb_ddci_t *lddci, service_t *t );
/* return 0, if ddci can be assigned to the given service */
int
linuxdvb_ddci_do_not_assign ( linuxdvb_ddci_t *lddci, service_t *t, int multi );
#endif

/*
 * Diseqc gear
 */
linuxdvb_diseqc_t *linuxdvb_diseqc_create0
  ( linuxdvb_diseqc_t *ld, const char *uuid, const idclass_t *idc,
    htsmsg_t *conf, const char *type, linuxdvb_satconf_ele_t *parent );

void linuxdvb_diseqc_destroy ( linuxdvb_diseqc_t *ld );

#define linuxdvb_diseqc_create(_d, _u, _c, _t, _p)\
  linuxdvb_diseqc_create0(calloc(1, sizeof(struct _d)),\
                                 _u, &_d##_class, _c, _t, _p)
  
linuxdvb_lnb_t    *linuxdvb_lnb_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls );
linuxdvb_diseqc_t *linuxdvb_switch_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int c, int u );
linuxdvb_diseqc_t *linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls );

#define UNICABLE_I_NAME       "Unicable I (EN50494)"
#define UNICABLE_II_NAME      "Unicable II (EN50607)"

linuxdvb_diseqc_t *linuxdvb_en50494_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int port );

void linuxdvb_lnb_destroy     ( linuxdvb_lnb_t *lnb );
void linuxdvb_switch_destroy  ( linuxdvb_diseqc_t *ld );
void linuxdvb_rotor_destroy   ( linuxdvb_diseqc_t *ld );
void linuxdvb_en50494_destroy ( linuxdvb_diseqc_t *ld );

htsmsg_t *linuxdvb_lnb_list     ( void *o, const char *lang );
htsmsg_t *linuxdvb_switch_list  ( void *o, const char *lang );
htsmsg_t *linuxdvb_rotor_list   ( void *o, const char *lang );
htsmsg_t *linuxdvb_en50494_list ( void *o, const char *lang );

htsmsg_t *linuxdvb_en50494_id_list  ( void *o, const char *lang );
htsmsg_t *linuxdvb_en50607_id_list  ( void *o, const char *lang );
htsmsg_t *linuxdvb_en50494_pin_list ( void *o, const char *lang );

static inline int linuxdvb_unicable_is_en50607( const char *str )
  { return str && strcmp(str, UNICABLE_II_NAME) == 0; }
static inline int linuxdvb_unicable_is_en50494( const char *str )
  { return !linuxdvb_unicable_is_en50607(str); }

void linuxdvb_en50494_init (void);

int linuxdvb_diseqc_raw_send (int fd, int len, ...);
int
linuxdvb_diseqc_send
  (int fd, uint8_t framing, uint8_t addr, uint8_t cmd, int len, ...);
int linuxdvb_diseqc_set_volt ( linuxdvb_satconf_t *ls, int volt );

/*
 * Satconf
 */
void linuxdvb_satconf_save ( linuxdvb_satconf_t *ls, htsmsg_t *m );

linuxdvb_satconf_ele_t *linuxdvb_satconf_ele_create0
  (const char *uuid, htsmsg_t *conf, linuxdvb_satconf_t *ls);

void linuxdvb_satconf_ele_destroy ( linuxdvb_satconf_ele_t *ls );

htsmsg_t *linuxdvb_satconf_type_list ( void *o, const char *lang );

linuxdvb_satconf_t *linuxdvb_satconf_create
  ( linuxdvb_frontend_t *lfe, htsmsg_t *conf );

void linuxdvb_satconf_destroy ( linuxdvb_satconf_t *ls, int delconf );

int linuxdvb_satconf_get_priority
  ( linuxdvb_satconf_t *ls, mpegts_mux_t *mm );

int linuxdvb_satconf_get_grace
  ( linuxdvb_satconf_t *ls, mpegts_mux_t *mm );

int linuxdvb_satconf_lnb_freq
  ( linuxdvb_satconf_t *ls, mpegts_mux_instance_t *mmi );

void linuxdvb_satconf_post_stop_mux( linuxdvb_satconf_t *ls );

int linuxdvb_satconf_power_save( linuxdvb_satconf_t *ls );

int linuxdvb_satconf_start_mux
  ( linuxdvb_satconf_t *ls, mpegts_mux_instance_t *mmi, int skip_diseqc );

int linuxdvb_satconf_match_mux
  ( linuxdvb_satconf_t *ls, mpegts_mux_t *mm );

int linuxdvb_satconf_start ( linuxdvb_satconf_t *ls, int delay, int vol );

void linuxdvb_satconf_reset ( linuxdvb_satconf_t *ls );

static inline int linuxdvb_satconf_fe_fd ( linuxdvb_satconf_t *ls )
  { return ((linuxdvb_frontend_t *)ls->ls_frontend)->lfe_fe_fd; }

#endif /* __TVH_LINUXDVB_PRIVATE_H__ */
