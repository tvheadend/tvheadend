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

#include "input/mpegts.h"

#include <linux/dvb/version.h>

#define DVB_VER_INT(maj,min) (((maj) << 16) + (min))

#define DVB_VER_ATLEAST(maj, min) \
 (DVB_VER_INT(DVB_API_VERSION,  DVB_API_VERSION_MINOR) >= DVB_VER_INT(maj, min))

/* Max allowed frequency variation for something to be considered the same */
#define LINUXDVB_FREQ_TOL 2000

typedef struct linuxdvb_hardware    linuxdvb_hardware_t;
typedef struct linuxdvb_adapter     linuxdvb_adapter_t;
typedef struct linuxdvb_frontend    linuxdvb_frontend_t;
typedef struct linuxdvb_satconf     linuxdvb_satconf_t;
typedef struct linuxdvb_satconf_ele linuxdvb_satconf_ele_t;
typedef struct linuxdvb_diseqc      linuxdvb_diseqc_t;
typedef struct linuxdvb_lnb         linuxdvb_lnb_t;
typedef struct linuxdvb_network     linuxdvb_network_t;
typedef struct linuxdvb_mux         linuxdvb_mux_t;
typedef struct linuxdvb_en50494     linuxdvb_en50494_t;

typedef LIST_HEAD(,linuxdvb_hardware) linuxdvb_hardware_list_t;
typedef TAILQ_HEAD(linuxdvb_satconf_ele_list,linuxdvb_satconf_ele) linuxdvb_satconf_ele_list_t;

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
  char                      lfe_name[128];
  char                     *lfe_fe_path;
  char                     *lfe_dmx_path;
  char                     *lfe_dvr_path;

  /*
   * Reception
   */
  int                       lfe_fe_fd;
  pthread_t                 lfe_dvr_thread;
  th_pipe_t                 lfe_dvr_pipe;
  pthread_mutex_t           lfe_dvr_lock;
  pthread_cond_t            lfe_dvr_cond;
 
  /*
   * Tuning
   */
  int                       lfe_locked;
  int                       lfe_status;
  time_t                    lfe_monitor;
  gtimer_t                  lfe_monitor_timer;

  /*
   * Configuration
   */
  int                       lfe_fullmux;
  int                       lfe_noclosefe;

  /*
   * Satconf (DVB-S only)
   */
  linuxdvb_satconf_t       *lfe_satconf;
};

struct linuxdvb_satconf
{
  idnode_t              ls_id;
  const char           *ls_type;

  /*
   * MPEG-TS hooks
   */
  mpegts_input_t        *ls_frontend; ///< Frontend we're proxying for
  mpegts_mux_instance_t *ls_mmi;      ///< Used within delay diseqc handler

  /*
   * Diseqc handling
   */
  gtimer_t               ls_diseqc_timer;
  int                    ls_diseqc_idx;
  int                    ls_diseqc_repeats;

  /*
   * LNB settings
   */
  int                    ls_lnb_poweroff;
  
  /*
   * Satconf elements
   */
  linuxdvb_satconf_ele_list_t ls_elements;
};

/*
 * Internal wrapper for a satconf entry
 *
 * Note: this is a bit cumbersome, it comes from how I first did the satconf
 *       and was subsequently bullied (by amet) into changing it (probably
 *       for the better, just don't tell him, no danger he'll read this!)
 *
 *       maybe one day I'll do it again properly
 */
struct linuxdvb_satconf_ele
{
  mpegts_input_t; // This acts as proxy for the frontend

  /*
   * Parent
   */
  linuxdvb_satconf_t               *ls_parent;
  TAILQ_ENTRY(linuxdvb_satconf_ele)  ls_link;

  /*
   * Diseqc kit
   */
  linuxdvb_lnb_t        *ls_lnb;
  linuxdvb_diseqc_t     *ls_switch;
  linuxdvb_diseqc_t     *ls_rotor;
  linuxdvb_diseqc_t     *ls_en50494;
};

struct linuxdvb_diseqc
{
  idnode_t              ld_id;
  const char           *ld_type;
  linuxdvb_satconf_ele_t   *ld_satconf;
  int (*ld_grace) (linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm);
  int (*ld_tune)  (linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm,
                   linuxdvb_satconf_ele_t *ls, int fd);
};

struct linuxdvb_lnb
{
  linuxdvb_diseqc_t;
  uint32_t  (*lnb_freq)(linuxdvb_lnb_t*, linuxdvb_mux_t*);
  int       (*lnb_band)(linuxdvb_lnb_t*, linuxdvb_mux_t*);
  int       (*lnb_pol) (linuxdvb_lnb_t*, linuxdvb_mux_t*);
};

struct linuxdvb_en50494
{
  linuxdvb_diseqc_t;

  /* en50494 configuration*/
  uint16_t  le_position;  /* satelitte A(0) or B(1) */
  uint16_t  le_frequency; /* user band frequency in MHz */
  uint16_t  le_id;        /* user band id 0-7 */
  uint16_t  le_pin;       /* 0-255 or LINUXDVB_EN50494_NOPIN */

  /* runtime */
  uint32_t  le_tune_freq; /* the real frequency to tune to */
};

/*
 * Methods
 */
  
#define LINUXDVB_SUBSYS_FE  0x01
#define LINUXDVB_SUBSYS_DVR 0x02

void linuxdvb_adapter_init ( void );

void linuxdvb_adapter_done ( void );

void linuxdvb_adapter_save ( linuxdvb_adapter_t *la );

int  linuxdvb_adapter_is_free        ( linuxdvb_adapter_t *la );
int  linuxdvb_adapter_current_weight ( linuxdvb_adapter_t *la );

linuxdvb_frontend_t *
linuxdvb_frontend_create
  ( htsmsg_t *conf, linuxdvb_adapter_t *la, int number,
    const char *fe_path, const char *dmx_path, const char *dvr_path,
    dvb_fe_type_t type, const char *name );

void linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *m );

void linuxdvb_frontend_delete ( linuxdvb_frontend_t *lfe );

void linuxdvb_frontend_add_network
  ( linuxdvb_frontend_t *lfe, linuxdvb_network_t *net );

int linuxdvb_frontend_tune0
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq );
int linuxdvb_frontend_tune1
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq );

/*
 * Network
 */

struct linuxdvb_network
{
  mpegts_network_t;

  /*
   * Network type
   */
  dvb_fe_type_t ln_type;
};

void linuxdvb_network_init ( void );
void linuxdvb_network_done ( void );
linuxdvb_network_t *linuxdvb_network_find_by_uuid(const char *uuid);

linuxdvb_network_t *linuxdvb_network_create0
  ( const char *uuid, const idclass_t *idc, htsmsg_t *conf );

struct linuxdvb_mux
{
  mpegts_mux_t;

  /*
   * Tuning information
   */
  dvb_mux_conf_t lm_tuning;
};

linuxdvb_mux_t *linuxdvb_mux_create0
  (linuxdvb_network_t *ln, uint16_t onid, uint16_t tsid,
   const dvb_mux_conf_t *dmc, const char *uuid, htsmsg_t *conf);

#define linuxdvb_mux_create1(n, u, c)\
  linuxdvb_mux_create0(n, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,\
                       NULL, u, c)

/*
 * Service
 */
mpegts_service_t *linuxdvb_service_create0
  (linuxdvb_mux_t *lm, uint16_t sid, uint16_t pmt_pid,
   const char *uuid, htsmsg_t *conf);

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
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int u, int c );
linuxdvb_diseqc_t *linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls );
linuxdvb_diseqc_t *linuxdvb_en50494_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls, int port );

void linuxdvb_lnb_destroy     ( linuxdvb_lnb_t    *lnb );
void linuxdvb_switch_destroy  ( linuxdvb_diseqc_t *ld );
void linuxdvb_rotor_destroy   ( linuxdvb_diseqc_t *ld );
void linuxdvb_en50494_destroy ( linuxdvb_diseqc_t *ld );

htsmsg_t *linuxdvb_lnb_list     ( void *o );
htsmsg_t *linuxdvb_switch_list  ( void *o );
htsmsg_t *linuxdvb_rotor_list   ( void *o );
htsmsg_t *linuxdvb_en50494_list ( void *o );

htsmsg_t *linuxdvb_en50494_id_list ( void *o );
htsmsg_t *linuxdvb_en50494_pin_list ( void *o );

void linuxdvb_en50494_init (void);

int
linuxdvb_diseqc_send
  (int fd, uint8_t framing, uint8_t addr, uint8_t cmd, uint8_t len, ...);
int linuxdvb_diseqc_set_volt (int fd, int volt);

/*
 * Satconf
 */
void linuxdvb_satconf_save ( linuxdvb_satconf_t *ls, htsmsg_t *m );

linuxdvb_satconf_ele_t *linuxdvb_satconf_ele_create0
  (const char *uuid, htsmsg_t *conf, linuxdvb_satconf_t *ls);

void linuxdvb_satconf_ele_destroy ( linuxdvb_satconf_ele_t *ls );

htsmsg_t *linuxdvb_satconf_type_list ( void *o );

linuxdvb_satconf_t *linuxdvb_satconf_create
  ( linuxdvb_frontend_t *lfe,
    const char *type, const char *uuid, htsmsg_t *conf );

void linuxdvb_satconf_delete ( linuxdvb_satconf_t *ls, int delconf );

#endif /* __TVH_LINUXDVB_PRIVATE_H__ */
