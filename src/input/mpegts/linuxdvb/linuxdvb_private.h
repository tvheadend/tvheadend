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
typedef struct linuxdvb_adapter  linuxdvb_adapter_t;
typedef struct linuxdvb_frontend linuxdvb_frontend_t;

typedef TAILQ_HEAD(,linuxdvb_hardware) linuxdvb_hardware_queue_t;

struct linuxdvb_hardware
{
  mpegts_input_t; // this is actually redundant in HW

  /* HW tree */
  linuxdvb_hardware_t             *lh_parent;
  TAILQ_ENTRY(linuxdvb_hardware)   lh_parent_link;
  TAILQ_HEAD(,linuxdvb_hardware)   lh_childs;

  /* These replicate the mpegts_input_t callbacks but work down the tree */
  int (*lh_is_free)        (linuxdvb_hardware_t *lh);
  int (*lh_current_weight) (linuxdvb_hardware_t *lh);
};

struct linuxdvb_adapter
{
  linuxdvb_hardware_t;

  /*
   * Hardware info
   */
  char *la_path;
  char *la_dev_id;
  enum {
    BUS_NONE = 0,
    BUS_PCI,
    BUS_USB1,
    BUS_USB2,
    BUS_USB3
  }     la_dev_bus;
};

linuxdvb_adapter_t *linuxdvb_adapter_create (int a);

struct linuxdvb_frontend
{
  linuxdvb_hardware_t;

  /*
   * Frontend info
   */
  struct dvb_frontend_info  lfe_fe_info;
  char                     *lfe_fe_path;
  char                      lfe_dmx_path;
  char                      lfe_dvr_path;
};

linuxdvb_frontend_t *linuxdvb_frontend_create
  ( linuxdvb_adapter_t *la, const char *fe_path,
    const char *dmx_path, const char *dvr_path,
    const struct dvb_frontend_info *fe_info );

#endif /* __TVH_LINUXDVB_PRIVATE_H__ */
