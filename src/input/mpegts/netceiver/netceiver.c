/*
 *  Tvheadend - NetCeiver DVB input
 *
 *  Copyright (C) 2013-2018 Sven Wegener
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

#include "tvheadend.h"
#include "netceiver_private.h"
#include "input.h"
#include "settings.h"

#include <unistd.h>


/*
 * Initialization
 */

void netceiver_init(void)
{
  netceiver_discovery_init();
  netceiver_monitor_init();
  netceiver_frontend_init();
  netceiver_hardware_init();
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
