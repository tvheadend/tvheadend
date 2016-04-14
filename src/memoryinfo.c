/*
 *  Tvheadend - memory info support
 *  Copyright (C) 2016 Jaroslav Kysela
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
#include "idnode.h"
#include "access.h"
#include "memoryinfo.h"

struct memoryinfo_list memoryinfo_entries;

static const char *
service_class_get_title ( idnode_t *self, const char *lang )
{
  return ((memoryinfo_t *)self)->my_name;
}

CLASS_DOC(memoryinfo)

const idclass_t memoryinfo_class = {
  .ic_class      = "memoryinfo",
  .ic_caption    = N_("Memory Information"),
  .ic_event      = "memoryinfo",
  .ic_doc        = tvh_doc_memoryinfo_class,
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_get_title  = service_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("Name of object."),
      .off      = offsetof(memoryinfo_t, my_name),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_S64_ATOMIC,
      .id       = "size",
      .name     = N_("Size"),
      .desc     = N_("Current object size."),
      .off      = offsetof(memoryinfo_t, my_size),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_S64_ATOMIC,
      .id       = "peak_size",
      .name     = N_("Peak size"),
      .desc     = N_("Largest size the object has reached."),
      .off      = offsetof(memoryinfo_t, my_peak_size),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_S64_ATOMIC,
      .id       = "count",
      .name     = N_("Count of objects"),
      .desc     = N_("Current number of objects."),
      .off      = offsetof(memoryinfo_t, my_count),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_S64_ATOMIC,
      .id       = "peak_count",
      .name     = N_("Peak count of objects"),
      .desc     = N_("Highest count of objects."),
      .off      = offsetof(memoryinfo_t, my_peak_count),
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {}
  }
};
