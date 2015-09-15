/*
 *  API - Timeshift related calls
 *
 *  Copyright (C) 2015 Jaroslav Kysela
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; withm even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "timeshift.h"

void
api_timeshift_init ( void )
{
  static api_hook_t ah[] = {
    { "timeshift/config/load", ACCESS_ADMIN, api_idnode_load_simple, &timeshift_conf },
    { "timeshift/config/save", ACCESS_ADMIN, api_idnode_save_simple, &timeshift_conf },
    { NULL },
  };

  api_register_all(ah);
}
