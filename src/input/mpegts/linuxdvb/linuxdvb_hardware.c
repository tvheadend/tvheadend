/*
 *  Tvheadend - Linux DVB hardware class
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
#include "queue.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

idnode_t **
linuxdvb_hardware_enumerate ( linuxdvb_hardware_list_t *list )
{
  linuxdvb_hardware_t *lh;
  idnode_t **v;
  int cnt = 1;
  LIST_FOREACH(lh, list, lh_parent_link)
    cnt++;
  v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  LIST_FOREACH(lh, list, lh_parent_link)
    v[cnt++] = &lh->mi_id;
  v[cnt] = NULL;
  return v;
}

static const char *
linuxdvb_hardware_class_get_title ( idnode_t *in )
{
  return ((linuxdvb_hardware_t*)in)->lh_displayname;
}

static idnode_t **
linuxdvb_hardware_class_get_childs ( idnode_t *in )
{
  return linuxdvb_hardware_enumerate(&((linuxdvb_hardware_t*)in)->lh_children);
}

const idclass_t linuxdvb_hardware_class =
{
  .ic_class      = "linuxdvb_hardware",
  .ic_caption    = "LinuxDVB Hardware",
  .ic_get_title  = linuxdvb_hardware_class_get_title,
  .ic_get_childs = linuxdvb_hardware_class_get_childs,
  .ic_properties = (const property_t[]){
    { PROPDEF1("enabled", "Enabled",
               PT_BOOL, linuxdvb_hardware_t, lh_enabled) },
    { PROPDEF1("displayname", "Name",
               PT_STR, linuxdvb_hardware_t, lh_displayname) },
    {}
  }
};
