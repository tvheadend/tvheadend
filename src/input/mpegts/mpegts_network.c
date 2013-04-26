/*
 *  Tvheadend - MPEGTS input source
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

#include "input/mpegts.h"

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = "MPEGTS Network",
  .ic_properties = (const property_t[]){
  }
};


mpegts_network_t *
mpegts_network_create0
  ( const char *uuid, const char *netname )
{
  mpegts_network_t *mn = idnode_create(mpegts_network, uuid);
  mn->mn_network_name = strdup(netname);
  TAILQ_INIT(&mn->mn_initial_scan_pending_queue);
  TAILQ_INIT(&mn->mn_initial_scan_current_queue);
  return mn;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
