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
#include "queue.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

/* ***************************************************************************
 * DVB Adapter
 * **************************************************************************/

static void
linuxdvb_adapter_class_save ( idnode_t *in )
{
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  linuxdvb_device_save((linuxdvb_device_t*)la->lh_parent);
}

const idclass_t linuxdvb_adapter_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_adapter",
  .ic_caption    = "LinuxDVB Adapter",
  .ic_save       = linuxdvb_adapter_class_save,
  .ic_properties = (const property_t[]){
    { PROPDEF2("rootpath", "Device Path",
               PT_STR, linuxdvb_adapter_t, la_rootpath, 1) },
    {}
  }
};

/*
 * Save data
 */
void
linuxdvb_adapter_save ( linuxdvb_adapter_t *la, htsmsg_t *m )
{
  htsmsg_t *l;
  linuxdvb_hardware_t *lh;

  linuxdvb_hardware_save((linuxdvb_hardware_t*)la, m);
  htsmsg_add_u32(m, "number", la->la_number);
  if (la->la_rootpath)
    htsmsg_add_str(m, "rootpath", la->la_rootpath);

  /* Frontends */
  l = htsmsg_create_map();
  LIST_FOREACH(lh, &la->lh_children, lh_parent_link) {
    htsmsg_t *e = htsmsg_create_map();
    linuxdvb_frontend_save((linuxdvb_frontend_t*)lh, e);
    htsmsg_add_msg(l, idnode_uuid_as_str(&lh->mi_id), e);
  }
  htsmsg_add_msg(m, "frontends", l);
}

/*
 * Check is free
 */
int
linuxdvb_adapter_is_free ( linuxdvb_adapter_t *la )
{
  return 0;
}

/*
 * Get current weight
 */
int
linuxdvb_adapter_current_weight ( linuxdvb_adapter_t *la )
{
  return 0;
}

/*
 * Create
 */
linuxdvb_adapter_t *
linuxdvb_adapter_create0 
  ( linuxdvb_device_t *ld, const char *uuid, htsmsg_t *conf )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *e;
  htsmsg_field_t *f;
  linuxdvb_adapter_t *la;

  la = calloc(1, sizeof(linuxdvb_adapter_t));
  if (idnode_insert(&la->mi_id, uuid, &linuxdvb_adapter_class)) {
    free(la);
    return NULL;
  }

  LIST_INSERT_HEAD(&ld->lh_children, (linuxdvb_hardware_t*)la, lh_parent_link);
  la->lh_parent = (linuxdvb_hardware_t*)ld;

  /* No conf */
  if (!conf)
    return la;

  linuxdvb_hardware_load((linuxdvb_hardware_t*)la, conf);
  if (!htsmsg_get_u32(conf, "number", &u32))
    la->la_number = u32;
  if ((str = htsmsg_get_str(conf, "rootpath")))
    la->la_rootpath = strdup(str);

  /* Frontends */
  if ((conf = htsmsg_get_map(conf, "frontends"))) {
    HTSMSG_FOREACH(f, conf) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      (void)linuxdvb_frontend_create0(la, f->hmf_name, e, 0);
    }
  }

  return la;
}

/*
 * Find existing adapter/device entry
 */
static linuxdvb_adapter_t *
linuxdvb_adapter_find_by_number ( int adapter )
{
  int a;
  char buf[1024];
  linuxdvb_hardware_t *lh;
  linuxdvb_device_t *ld;
  linuxdvb_adapter_t *la;

  /* Find device */
  if (!(ld = linuxdvb_device_find_by_adapter(adapter)))
    return NULL;

  /* Find existing adapter */ 
  a = adapter - ld->ld_devid.di_min_adapter;
  LIST_FOREACH(lh, &ld->lh_children, lh_parent_link) {
    if (((linuxdvb_adapter_t*)lh)->la_number == a)
      break;
  }
  la = (linuxdvb_adapter_t*)lh;

  /* Create */
  if (!la) {
    if (!(la = linuxdvb_adapter_create0(ld, NULL, NULL)))
      return NULL;
  }

  /* Update */
  la->la_number = a;
  snprintf(buf, sizeof(buf), "/dev/dvb/adapter%d", adapter);
  tvh_str_update(&la->la_rootpath, buf);
  if (!la->lh_displayname)
    la->lh_displayname = strdup(la->la_rootpath);

  return la;
}

/*
 * Load an adapter
 */
linuxdvb_adapter_t *
linuxdvb_adapter_added ( int adapter )
{
  int i, r, fd;
  char fe_path[512], dmx_path[512], dvr_path[512];
  linuxdvb_adapter_t *la = NULL;
  struct dvb_frontend_info dfi;

  /* Process each frontend */
  for (i = 0; i < 32; i++) {
    snprintf(fe_path, sizeof(fe_path), "/dev/dvb/adapter%d/frontend%d", adapter, i);

    /* No access */
    if (access(fe_path, R_OK | W_OK)) continue;

    /* Get frontend info */
    fd = tvh_open(fe_path, O_RDONLY | O_NONBLOCK, 0);
    if (fd == -1) {
      tvhlog(LOG_ERR, "linuxdvb", "unable to open %s", fe_path);
      continue;
    }
    r = ioctl(fd, FE_GET_INFO, &dfi);
    close(fd);
    if(r) {
      tvhlog(LOG_ERR, "linuxdvb", "unable to query %s", fe_path);
      continue;
    }

    /* DVR/DMX (bit of a guess) */
    snprintf(dmx_path, sizeof(dmx_path), "/dev/dvb/adapter%d/demux%d", adapter, i);
    if (access(dmx_path, R_OK | W_OK)) {
      snprintf(dmx_path, sizeof(dmx_path), "/dev/dvb/adapter%d/demux0", adapter);
      if (access(dmx_path, R_OK | W_OK)) continue;
    }

    snprintf(dvr_path, sizeof(dvr_path), "/dev/dvb/adapter%d/dvr%d", adapter, i);
    if (access(dvr_path, R_OK | W_OK)) {
      snprintf(dvr_path, sizeof(dvr_path), "/dev/dvb/adapter%d/dvr0", adapter);
      if (access(dvr_path, R_OK | W_OK)) continue;
    }

    /* Create/Find adapter */
    if (!la) {
      if (!(la = linuxdvb_adapter_find_by_number(adapter))) {
        tvhlog(LOG_ERR, "linuxdvb", "failed to find/create adapter%d", adapter);
        return NULL;
      }
    }

    /* Create frontend */
    tvhlog(LOG_DEBUG, "linuxdvb", "fe_create(%p, %s, %s, %s)",
           la, fe_path, dmx_path, dvr_path);
    linuxdvb_frontend_added(la, i, fe_path, dmx_path, dvr_path, &dfi);
  }

  if (la)
    linuxdvb_device_save((linuxdvb_device_t*)la->lh_parent);

  return la;
}
