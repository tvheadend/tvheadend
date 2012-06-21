/*
 *  Electronic Program Guide - EPG grabber OTA functions
 *  Copyright (C) 2012 Adam Sutton
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

#ifndef __TVH_EPGGRAB_OTA_H__
#define __TVH_EPGGRAB_OTA_H__

#include "queue.h"
#include <sys/time.h>

/*
 * Mux to OTA Grabber link
 */
typedef struct epggrab_ota_mux
{
  LIST_ENTRY(epggrab_ota_mux)       glob_link; ///< Global list of all instances
  LIST_ENTRY(epggrab_ota_mux)       grab_link; ///< Grabber's list link
  LIST_ENTRY(epggrab_ota_mux)       tdmi_link; ///< Mux list link
  struct epggrab_module            *grab;      ///< Grabber module
  struct th_dvb_mux_instance       *tdmi;      ///< Mux instance

  int                               timeout;   ///< Time out if this long
  int                               interval;  ///< Re-grab this often

  enum {
    EPGGRAB_OTA_IDLE,
    EPGGRAB_OTA_STARTED,
    EPGGRAB_OTA_COMPLETED
  }                                 status;    ///< Status of the scan
  time_t                            started;   ///< Time of last start
  time_t                            completed; ///< Time of last completion

} epggrab_ota_mux_t;

/*
 * Register interest (will return existing if already registered)
 */
epggrab_ota_mux_t *epggrab_ota_register
  ( struct epggrab_module *mod, struct th_dvb_mux_instance *tdmi,
    int timeout, int interval );

/*
 * Unregister
 */
void epggrab_ota_unregister
  ( epggrab_ota_mux_t *ota );
void epggrab_ota_unregister_one
  ( struct epggrab_module *mod, struct th_dvb_mux_instance *tdmi );
void epggrab_ota_unregister_all
  ( struct epggrab_module *mod );

/*
 * Status
 */
int  epggrab_ota_begin     ( epggrab_ota_mux_t *ota );
void epggrab_ota_complete  ( epggrab_ota_mux_t *ota );
void epggrab_ota_cancel    ( epggrab_ota_mux_t *ota );
void epggrab_ota_timeout   ( epggrab_ota_mux_t *ota );

#endif /* __TVH_EPGGRAB_OTA_H__ */
