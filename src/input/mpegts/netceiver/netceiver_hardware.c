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
 * NetCeiver Hardware
 */

typedef struct netceiver_hardware {
  tvh_hardware_t;

  htsmsg_t *nchw_interfaces;
} netceiver_hardware_t;

static idnode_set_t *netceiver_hardware_class_get_childs(idnode_t *in)
{
  return idnode_find_all(&netceiver_frontend_class, NULL);
}

static void netceiver_hardware_class_get_title(idnode_t *in, const char *lang, char *dst, size_t dstsize)
{
  snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, N_("NetCeiver")));
}

static htsmsg_t *netceiver_hardware_class_save(idnode_t *self, char *filename, size_t fsize)
{
  htsmsg_t *c;

  c = htsmsg_create_map();
  idnode_save(self, c);
  snprintf(filename, fsize, "input/dvb/netceiver/config");
  return c;
}

static const void *netceiver_hardware_class_interfaces_get(void *obj)
{
  netceiver_hardware_t *nchw = obj;

  if (!nchw->nchw_interfaces)
    nchw->nchw_interfaces = htsmsg_create_list();

  return htsmsg_copy(nchw->nchw_interfaces);
}

static int netceiver_hardware_class_interfaces_set(void *obj, const void *p)
{
  netceiver_hardware_t *nchw = obj;
  htsmsg_t *msg = (htsmsg_t *) p;
  htsmsg_t *add, *del;

  msg = htsmsg_copy(msg);

  htsmsg_diff(nchw->nchw_interfaces, msg, &add, &del);

  if (del) {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, del) {
      const char *interface = htsmsg_field_get_str(f);
      netceiver_discovery_remove_interface(interface);
    }
    htsmsg_destroy(del);
  }

  if (add) {
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, add) {
      const char *interface = htsmsg_field_get_str(f);
      netceiver_discovery_add_interface(interface);
    }
    htsmsg_destroy(add);
  }

  htsmsg_destroy(nchw->nchw_interfaces);
  nchw->nchw_interfaces = msg;

  return add || del;
}

static char *netceiver_hardware_class_interfaces_rend(void *obj, const char *lang)
{
  netceiver_hardware_t *nchw = obj;

  return htsmsg_list_2_csv(nchw->nchw_interfaces, ',', 1);
}

static const idclass_t netceiver_hardware_class = {
  .ic_class      = "netceiver_hardware",
  .ic_caption    = N_("NetCeiver"),
  .ic_get_title  = netceiver_hardware_class_get_title,
  .ic_get_childs = netceiver_hardware_class_get_childs,
  .ic_save       = netceiver_hardware_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type   = PT_STR,
      .id     = "interfaces",
      .name   = N_("Interfaces"),
      .islist = 1,
      .set    = netceiver_hardware_class_interfaces_set,
      .get    = netceiver_hardware_class_interfaces_get,
      .list   = network_interfaces_enum,
      .rend   = netceiver_hardware_class_interfaces_rend,
    },
    {},
  },
};

static netceiver_hardware_t netceiver_hardware;

void netceiver_hardware_init(void)
{
  htsmsg_t *conf;

  idclass_register(&netceiver_hardware_class);

  tvhdebug(LS_NETCEIVER, "loading NetCeiver configuration");
  conf = hts_settings_load("input/dvb/netceiver/config");
  tvh_hardware_create0((tvh_hardware_t *) &netceiver_hardware, &netceiver_hardware_class, NULL, conf);
  htsmsg_destroy(conf);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
