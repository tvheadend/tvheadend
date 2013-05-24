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
 * DVB Hardware class
 * **************************************************************************/

static idnode_t **
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

/* ***************************************************************************
 * DVB Device
 * **************************************************************************/

/*
 * BUS str table
 */
static struct strtab bustab[] = {
  { "PCI",  BUS_PCI  },
  { "USB1", BUS_USB1 },
  { "USB2", BUS_USB2 },
  { "USB3", BUS_USB3 }
};
static const char*
devinfo_bus2str ( int p )
{
  return val2str(p, bustab);
}

/*
 * Get bus information
 */
static void
get_device_info ( device_info_t *di, int a )
{
  FILE *fp;
  DIR *dp;
  struct dirent *de;
  uint16_t u16;
  int speed;
  char path[512], buf[512];
  ssize_t c;
  int mina = a;

  /* Check for subsystem */
#define DVB_DEV_PATH "/sys/class/dvb/dvb%d.frontend0/device"
  snprintf(path, sizeof(path), DVB_DEV_PATH "/subsystem", a);
  if ((c = readlink(path, buf, sizeof(buf))) != -1) {
    buf[c] = '\0';
    char *bus = basename(buf);
    if (!strcmp(bus, "pci")) {
      di->di_bus = BUS_PCI;
      snprintf(path, sizeof(path), DVB_DEV_PATH "/subsystem_vendor", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "0x%hx", &u16) == 1)
          di->di_dev = u16;
      }
      di->di_dev <<= 16;
      snprintf(path, sizeof(path), DVB_DEV_PATH "/subsystem_device", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "0x%hx", &u16) == 1)
          di->di_dev |= u16;
      }

    } else if (!strcmp(bus, "usb")) {
      di->di_bus = BUS_USB1;
      snprintf(path, sizeof(path), DVB_DEV_PATH "/idVendor", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "%hx", &u16) == 1)
          di->di_dev = u16;
      }
      di->di_dev <<= 16;
      snprintf(path, sizeof(path), DVB_DEV_PATH "/idProduct", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "%hx", &u16) == 1)
          di->di_dev |= u16;
      }
      snprintf(path, sizeof(path), DVB_DEV_PATH "/speed", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "%d", &speed) == 1) {
          if (speed > 480) {
            di->di_bus = BUS_USB3;
          } else if (speed == 480) {
            di->di_bus = BUS_USB2;
          }
        }
        fclose(fp);
      }
    } else {
      tvhlog(LOG_WARNING, "linuxdvb",
             "could not determine host connection for adapter%d", a);
    }
  }

  /* Get Path */
  snprintf(path, sizeof(path), DVB_DEV_PATH, a);
  if ((c = readlink(path, buf, sizeof(buf))) != -1) {
    buf[c] = '\0';
    strcpy(di->di_path, basename(buf));
  }
  
  /* Find minimum adapter number */
  snprintf(path, sizeof(path), DVB_DEV_PATH "/dvb", a);
  if ((dp = opendir(path))) {
    while ((de = readdir(dp))) {
      int t;
      if ((sscanf(de->d_name, "dvb%d.frontend0", &t)))
        if (t < mina) mina = t;
    }
    closedir(dp);
  }
  di->di_min_adapter = mina;

  /* Create ID */
  if (*di->di_path && di->di_dev) {
    snprintf(buf, sizeof(buf), "%s/%s/%04x:%04x",
             devinfo_bus2str(di->di_bus), di->di_path,
             di->di_dev >> 16, di->di_dev & 0xFFFF);
  } else {
    snprintf(buf, sizeof(buf), "/dev/dvb/adapter%d", a);
  }
  di->di_id = strdup(buf);
}

const idclass_t linuxdvb_device_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_device",
  .ic_caption    = "LinuxDVB Device",
  .ic_properties = (const property_t[]){
    { PROPDEF2("devid", "Device ID",
               PT_STR, linuxdvb_device_t, ld_devid.di_id, 1) },
    {}
  }
};

static linuxdvb_hardware_list_t linuxdvb_device_all;

idnode_t **
linuxdvb_root ( void )
{
  return linuxdvb_hardware_enumerate(&linuxdvb_device_all);
}

linuxdvb_device_t *
linuxdvb_device_create0 ( const char *uuid, htsmsg_t *conf )
{
  uint32_t u32;
  const char *str;
  linuxdvb_device_t *ld;

  /* Create */
  ld = calloc(1, sizeof(linuxdvb_device_t));
  if (idnode_insert(&ld->mi_id, uuid, &linuxdvb_device_class)) {
    free(ld);
    return NULL;
  }
  LIST_INSERT_HEAD(&linuxdvb_device_all, (linuxdvb_hardware_t*)ld, lh_parent_link);

  /* No config */
  if (!conf)
    return ld;

  /* Load config */
  if (!htsmsg_get_u32(conf, "enabled", &u32) && u32)
    ld->lh_enabled     = 1;
  if ((str = htsmsg_get_str(conf, "displayname")))
    ld->lh_displayname = strdup(str);
  if ((str = htsmsg_get_str(conf, "devid")))
    strncpy(ld->ld_devid.di_id, str, sizeof(ld->ld_devid.di_id));

  // TODO: adapters
  // TODO: frontends

  return ld;
}

static linuxdvb_device_t *
linuxdvb_device_find_by_hwid ( const char *hwid )
{
  linuxdvb_hardware_t *lh;
  LIST_FOREACH(lh, &linuxdvb_device_all, lh_parent_link)
    if (!strcmp(hwid, ((linuxdvb_device_t*)lh)->ld_devid.di_id))
      return (linuxdvb_device_t*)lh;
  return NULL;
}

static linuxdvb_device_t *
linuxdvb_device_find_by_adapter ( int a )
{
  linuxdvb_device_t *ld;
  device_info_t dev;

  /* Get device info */
  get_device_info(&dev, a);

  /* Find existing */
  if ((ld = linuxdvb_device_find_by_hwid(dev.di_id)))
    return ld;

  /* Create new */
  if (!(ld = linuxdvb_device_create0(NULL, NULL))) {
    tvhlog(LOG_ERR, "linuxdvb", "failed to create device for adapter%d", a);
    return NULL;
  }

  /* Copy device info */
  memcpy(&ld->ld_devid, &dev, sizeof(dev));
  ld->lh_displayname = strdup(dev.di_id);
  return ld;
}

/* ***************************************************************************
 * DVB Adapter
 * **************************************************************************/

const idclass_t linuxdvb_adapter_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_adapter",
  .ic_caption    = "LinuxDVB Adapter",
  .ic_properties = (const property_t[]){
    { PROPDEF2("rootpath", "Device Path",
               PT_STR, linuxdvb_adapter_t, la_rootpath, 1) },
    {}
  }
};

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
static linuxdvb_adapter_t *
linuxdvb_adapter_create0 ( const char *uuid, linuxdvb_device_t *ld, int n )
{
  linuxdvb_adapter_t *la;

  la = calloc(1, sizeof(linuxdvb_adapter_t));
  if (idnode_insert(&la->mi_id, uuid, &linuxdvb_adapter_class)) {
    free(la);
    return NULL;
  }
  la->la_number = n;

  LIST_INSERT_HEAD(&ld->lh_children, (linuxdvb_hardware_t*)la, lh_parent_link);
  la->lh_parent = (linuxdvb_hardware_t*)ld;

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
    if (!(la = linuxdvb_adapter_create0(NULL, ld, a)))
      return NULL;
  }

  /* Update */
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

  return la;
}


