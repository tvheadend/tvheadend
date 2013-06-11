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

idnode_set_t *
linuxdvb_hardware_enumerate ( linuxdvb_hardware_list_t *list )
{
  linuxdvb_hardware_t *lh;
  idnode_set_t *set = idnode_set_create();
  LIST_FOREACH(lh, list, lh_parent_link)
    idnode_set_add(set, &lh->mi_id, NULL);
  return set;
}

static const char *
linuxdvb_hardware_class_get_title ( idnode_t *in )
{
  return ((linuxdvb_hardware_t*)in)->lh_displayname;
}

static idnode_set_t *
linuxdvb_hardware_class_get_childs ( idnode_t *in )
{
  return linuxdvb_hardware_enumerate(&((linuxdvb_hardware_t*)in)->lh_children);
}

extern const idclass_t mpegts_input_class;
const idclass_t linuxdvb_hardware_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "linuxdvb_hardware",
  .ic_caption    = "LinuxDVB Hardware",
  .ic_get_title  = linuxdvb_hardware_class_get_title,
  .ic_get_childs = linuxdvb_hardware_class_get_childs,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = "Name",
      .off      = offsetof(linuxdvb_hardware_t, lh_displayname),
    },
    {}
  }
};
