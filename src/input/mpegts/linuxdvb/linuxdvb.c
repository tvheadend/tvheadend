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
#include "linuxdvb_private.h"

#include <sys/types.h>
#include <dirent.h>

void linuxdvb_init ( int adapter_mask )
{
  /* Scan for hardware */
  int a;
  DIR *dp = opendir("/dev/dvb");
  struct dirent *de;
  while ((de = readdir(dp))) {
    if (sscanf(de->d_name, "adapter%d", &a) != 1) continue;
    if (a & adapter_mask)
      linuxdvb_adapter_create(a);
  }

  /* Load configuration */
  // TODO
}
