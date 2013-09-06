/*
 *  Tvheadend - Linux DVB device management
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
#define devinfo_bus2str(p) val2str(p, bustab)
#define devinfo_str2bus(p) str2val(p, bustab)

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
static void
get_min_dvb_adapter ( device_info_t *di )
{
  int mina = -1;
  char path[512];
  DIR *dp;
  struct dirent *de;

  snprintf(path, sizeof(path), "/sys/bus/%s/devices/%s/dvb",
           di->di_bus == BUS_PCI ? "pci" : "usb", di->di_path);

  /* Find minimum adapter number */
  if ((dp = opendir(path))) {
    while ((de = readdir(dp))) {
      int t;
      if ((sscanf(de->d_name, "dvb%d.frontend0", &t)))
        if (mina == -1 || t < mina) mina = t;
    }
  }
  di->di_min_adapter = mina;
}


static void
linuxdvb_device_class_save ( idnode_t *in )
{
  linuxdvb_device_save((linuxdvb_device_t*)in);
}
void linuxdvb_device_save ( linuxdvb_device_t *ld )
{
  htsmsg_t *m, *e, *l;
  linuxdvb_hardware_t *lh;

  m = htsmsg_create_map();

  idnode_save(&ld->ti_id, m);
  
  /* Adapters */
  l = htsmsg_create_map();
  LIST_FOREACH(lh, &ld->lh_children, lh_parent_link) {
    e = htsmsg_create_map();
    linuxdvb_adapter_save((linuxdvb_adapter_t*)lh, e);
    htsmsg_add_msg(l, idnode_uuid_as_str(&lh->ti_id), e);
  }
  htsmsg_add_msg(m, "adapters", l);

  /* Save */
  hts_settings_save(m, "input/linuxdvb/devices/%s",
                    idnode_uuid_as_str(&ld->ti_id));
}

const idclass_t linuxdvb_device_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_device",
  .ic_caption    = "LinuxDVB Device",
  .ic_save       = linuxdvb_device_class_save,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "devid",
      .name     = "Device ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(linuxdvb_device_t, ld_devid.di_id)
    },
    {}
  }
};

static linuxdvb_hardware_list_t linuxdvb_device_all;

idnode_set_t *
linuxdvb_root ( void )
{
  return linuxdvb_hardware_enumerate(&linuxdvb_device_all);
}

linuxdvb_device_t *
linuxdvb_device_create0 ( const char *uuid, htsmsg_t *conf )
{
  linuxdvb_device_t *ld;
  htsmsg_t *e;
  htsmsg_field_t *f;

  /* Create */
  ld = calloc(1, sizeof(linuxdvb_device_t));
  if (idnode_insert(&ld->ti_id, uuid, &linuxdvb_device_class)) {
    free(ld);
    return NULL;
  }
  LIST_INSERT_HEAD(&linuxdvb_device_all, (linuxdvb_hardware_t*)ld, lh_parent_link);

  /* Defaults */
  ld->mi_enabled = 1;

  /* No config */
  if (!conf)
    return ld;

  /* Load config */
  idnode_load(&ld->ti_id, conf);
  get_min_dvb_adapter(&ld->ld_devid);

  /* Adapters */
  if ((conf = htsmsg_get_map(conf, "adapters"))) {
    HTSMSG_FOREACH(f, conf) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      (void)linuxdvb_adapter_create0(ld, f->hmf_name, e);
    }
  }

  return ld;
}

static linuxdvb_device_t *
linuxdvb_device_find_by_hwid ( const char *hwid )
{
  linuxdvb_hardware_t *lh;
  LIST_FOREACH(lh, &linuxdvb_device_all, lh_parent_link) {
    
    if (!strcmp(hwid, ((linuxdvb_device_t*)lh)->ld_devid.di_id ?: ""))
      return (linuxdvb_device_t*)lh;
  }
  return NULL;
}

linuxdvb_device_t *
linuxdvb_device_find_by_adapter ( int a )
{
  linuxdvb_device_t *ld;
  device_info_t dev;

  /* Get device info */
  get_device_info(&dev, a);

  /* Find existing */
  if ((ld = linuxdvb_device_find_by_hwid(dev.di_id))) {
    memcpy(&ld->ld_devid, &dev, sizeof(dev));
    return ld;
  }

  /* Create new */
  if (!(ld = linuxdvb_device_create0(NULL, NULL))) {
    tvhlog(LOG_ERR, "linuxdvb", "failed to create device for adapter%d", a);
    return NULL;
  }

  /* Copy device info */
  memcpy(&ld->ld_devid, &dev, sizeof(dev));
  ld->mi_displayname = strdup(dev.di_id);
  return ld;
}

void linuxdvb_device_init ( int adapter_mask )
{
  int a;
  DIR *dp;
  htsmsg_t *s, *e;
  htsmsg_field_t *f;

  /* Load configuration */
  if ((s = hts_settings_load_r(1, "input/linuxdvb/devices"))) {
    HTSMSG_FOREACH(f, s) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      (void)linuxdvb_device_create0(f->hmf_name, e);
    }
  }

  /* Scan for hardware */
  if ((dp = opendir("/dev/dvb"))) {
    struct dirent *de;
    while ((de = readdir(dp))) {
      if (sscanf(de->d_name, "adapter%d", &a) != 1) continue;
      if ((0x1 << a) & adapter_mask)
        linuxdvb_adapter_added(a);
    }
  }

  // TODO: add udev support for hotplug
}
