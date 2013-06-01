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

typedef struct linuxdvb_hardware linuxdvb_hardware_t;
typedef struct linuxdvb_device   linuxdvb_device_t;
typedef struct linuxdvb_adapter  linuxdvb_adapter_t;
typedef struct linuxdvb_frontend linuxdvb_frontend_t;
typedef struct linuxdvb_network  linuxdvb_network_t;
typedef struct linuxdvb_mux      linuxdvb_mux_t;

typedef LIST_HEAD(,linuxdvb_hardware) linuxdvb_hardware_list_t;

typedef struct device_info
{
  char     *di_id;
  char     di_path[128];
  enum {
    BUS_NONE = 0,
    BUS_PCI,
    BUS_USB1,
    BUS_USB2,
    BUS_USB3
  }        di_bus;
  uint32_t di_dev;
  int      di_min_adapter;
} device_info_t;

struct linuxdvb_hardware
{
  mpegts_input_t; // Note: this is redundant in many of the instances
                  //       but we can't do multiple-inheritance and this
                  //       keeps some of the code clean

  /*
   * Parent/Child links
   */
  linuxdvb_hardware_t          *lh_parent;
  LIST_ENTRY(linuxdvb_hardware) lh_parent_link;
  linuxdvb_hardware_list_t      lh_children;

  /*
   * Device info
   */
  char                         *lh_displayname;
};

extern const idclass_t linuxdvb_hardware_class;

idnode_t **
linuxdvb_hardware_enumerate ( linuxdvb_hardware_list_t *list );
void linuxdvb_hardware_save ( linuxdvb_hardware_t *lh, htsmsg_t *m );
void linuxdvb_hardware_load ( linuxdvb_hardware_t *lh, htsmsg_t *m );


struct linuxdvb_device
{
  linuxdvb_hardware_t;

  /*
   * Device info
   */
  device_info_t               ld_devid;
};

void linuxdvb_device_init ( int adapter_mask );
void linuxdvb_device_save ( linuxdvb_device_t *ld );

linuxdvb_device_t *linuxdvb_device_create0
  (const char *uuid, htsmsg_t *conf);

linuxdvb_device_t * linuxdvb_device_find_by_adapter ( int a );

struct linuxdvb_adapter
{
  linuxdvb_hardware_t;

  /*
   * Adapter info
   */
  char    *la_rootpath;
  uint32_t la_number;
};

#define LINUXDVB_SUBSYS_FE  0x01
#define LINUXDVB_SUBSYS_DVR 0x02

void linuxdvb_adapter_save ( linuxdvb_adapter_t *la, htsmsg_t *m );

linuxdvb_adapter_t *linuxdvb_adapter_create0
  ( linuxdvb_device_t *ld, const char *uuid, htsmsg_t *conf );

linuxdvb_adapter_t *linuxdvb_adapter_added (int a);

int  linuxdvb_adapter_is_free        ( linuxdvb_adapter_t *la );
int  linuxdvb_adapter_current_weight ( linuxdvb_adapter_t *la );

linuxdvb_adapter_t *linuxdvb_adapter_find_by_hwid ( const char *hwid );
linuxdvb_adapter_t *linuxdvb_adapter_find_by_path ( const char *path );

struct linuxdvb_frontend
{
  linuxdvb_hardware_t;

  /*
   * Frontend info
   */
  int                       lfe_number;
  struct dvb_frontend_info  lfe_info;
  char                     *lfe_fe_path;
  char                     *lfe_dmx_path;
  char                     *lfe_dvr_path;

  /*
   * Reception
   */
  int                       lfe_fe_fd;
  int                       lfe_dvr_fd;
  pthread_t                 lfe_dvr_thread;
  th_pipe_t                 lfe_dvr_pipe;
 
  /*
   * Tuning
   */
  time_t                    lfe_monitor;
  gtimer_t                  lfe_monitor_timer;
};

linuxdvb_frontend_t *
linuxdvb_frontend_create0 
  ( linuxdvb_adapter_t *la, const char *uuid, htsmsg_t *conf, fe_type_t type ); 

void linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *m );

linuxdvb_frontend_t *
linuxdvb_frontend_added
  ( linuxdvb_adapter_t *la, int fe_num,
    const char *fe_path, const char *dmx_path, const char *dvr_path,
    const struct dvb_frontend_info *fe_info );
void linuxdvb_frontend_add_network
  ( linuxdvb_frontend_t *lfe, linuxdvb_network_t *net );

struct linuxdvb_network
{
  mpegts_network_t;

  /*
   * Network type
   */
  fe_type_t ln_type;
};

void linuxdvb_network_init ( void );
linuxdvb_network_t *linuxdvb_network_find_by_uuid(const char *uuid);

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

#endif /* __TVH_LINUXDVB_PRIVATE_H__ */
