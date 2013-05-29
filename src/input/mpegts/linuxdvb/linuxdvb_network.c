/*
 *  Tvheadend - Linux DVB Network
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
#include "settings.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

extern const idclass_t mpegts_network_class;
const idclass_t linuxdvb_network_class =
{
  .ic_super      = &mpegts_network_class,
  .ic_class      = "linuxdvb_network",
  .ic_caption    = "LinuxDVB Network",
  .ic_properties = (const property_t[]){
#if 0
    { PROPDEF2("type", "Network Type",
               PT_STR, linuxdvb_network_t, ln_type, 1),
      .get_str },
#endif
    {}
  }
};

static mpegts_mux_t *
linuxdvb_network_create_mux
  ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid, dvb_mux_conf_t *conf )
{
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mm->mm_network;
  return (mpegts_mux_t*)linuxdvb_mux_create0(ln, onid, tsid, conf, NULL);
}

static mpegts_service_t *
linuxdvb_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return NULL;
}

static void
linuxdvb_network_config_save ( mpegts_network_t *mn )
{
}

static linuxdvb_network_t *
linuxdvb_network_create0
  ( const char *uuid, htsmsg_t *conf )
{
  uint32_t u32;
  const char *str;
  linuxdvb_network_t *ln;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Create */
  if (!(ln = mpegts_network_create(linuxdvb_network, uuid, NULL)))
    return NULL;
  
  /* Callbacks */
  ln->mn_create_mux     = linuxdvb_network_create_mux;
  ln->mn_create_service = linuxdvb_network_create_service;
  ln->mn_config_save    = linuxdvb_network_config_save;

  /* No config */
  if (!conf)
    return ln;

  /* Load configuration */
  if ((str = htsmsg_get_str(conf, "type")))
    ln->ln_type = dvb_str2type(str);
  if ((str = htsmsg_get_str(conf, "name")))
    ln->mn_network_name = strdup(str);
  if (!htsmsg_get_u32(conf, "nid", &u32))
    ln->mn_nid          = u32;

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/linuxdvb/networks/%s/muxes", uuid))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      (void)linuxdvb_mux_create1(ln, f->hmf_name, e);
    }
  }

  return ln;
}

linuxdvb_network_t*
linuxdvb_network_find_by_uuid(const char *uuid)
{
  idnode_t *in = idnode_find(uuid, &linuxdvb_network_class);
  return (linuxdvb_network_t*)in;
}

void linuxdvb_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if (!(c = hts_settings_load_r(1, "input/linuxdvb/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    (void)linuxdvb_network_create0(f->hmf_name, e);
  }
  htsmsg_destroy(c);
}
