/*
 *  Tvheadend - Linux DVB input system
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

#include "tvheadend.h"
#include "input.h"
#include "settings.h"
#include "linuxdvb_private.h"

int linuxdvb_adapter_mask;

void linuxdvb_init ( int adapter_mask )
{
  linuxdvb_adapter_mask = adapter_mask;

  /* Initialise en50494 locks */
  linuxdvb_en50494_init();

  /* Initialsie devices */
  linuxdvb_adapter_init();
}

void linuxdvb_done ( void )
{
  linuxdvb_adapter_done();
}
