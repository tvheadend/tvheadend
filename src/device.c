/*
 *  Tvheadend
 *  Copyright (C) 2010 Andreas Öman
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "tvheadend.h"
#include "device.h"
#include "htsmsg.h"
#include "settings.h"

static struct device_list conf_devices;
static struct device_list probed_devices;
static struct device_list running_devices;

static int deviceid;

/**
 *
 */  
static const char *
hostconnection2str(int type)
{
  switch(type) {
  case HOSTCONNECTION_USB12:
    return "USB (12 Mbit/s)";
    
  case HOSTCONNECTION_USB480:
    return "USB (480 Mbit/s)";

  case HOSTCONNECTION_PCI:
    return "PCI";
  }
  return "Unknown";
}




/**
 *
 */
static int
readlinefromfile(const char *path, char *buf, size_t buflen)
{
  int fd = open(path, O_RDONLY);
  ssize_t r;

  if(fd == -1)
    return -1;

  r = read(fd, buf, buflen - 1);
  close(fd);
  if(r < 0)
    return -1;

  buf[buflen - 1] = 0;
  return 0;
}


/**
 *
 */
static void
device_destroy(device_t *d)
{
  LIST_REMOVE(d, d_link);
  free(d->d_name);
  free(d->d_path);
  free(d->d_busid);
  free(d->d_devicename);
  free(d->d_type);
  if(d->d_config != NULL)
    htsmsg_destroy(d->d_config);
  free(d);
}


/**
 *
 */
void *
device_add(const char *path, const char *busid, const char *devicename,
	   const char *type, deviceclass_t *dc)
{
  device_t *d;
  char buf[256];
  char speedpath[PATH_MAX];
  char line[64];
  int speed;

  snprintf(speedpath, sizeof(speedpath), "%s/speed", busid);

  if(readlinefromfile(speedpath, line, sizeof(line))) {
    // Unable to read speed, assume it's PCI
    speed = HOSTCONNECTION_PCI;
  } else {
    speed = atoi(line) >= 480 ? HOSTCONNECTION_USB480 : HOSTCONNECTION_USB12;
  }

  tvhlog(LOG_INFO, "DEVICE", "Found device %s (%s)", path, type);
  tvhlog(LOG_INFO, "DEVICE", "  BusID: %s : %s", 
	 busid, hostconnection2str(speed));
  tvhlog(LOG_INFO, "DEVICE", "   Name: %s", devicename);
  snprintf(buf, sizeof(buf), "%s (%s)", devicename, path);

  d = calloc(1, dc->dc_size);
  d->d_id         = ++deviceid;
  d->d_name       = strdup(buf);
  d->d_path       = strdup(path);
  d->d_busid      = strdup(busid);
  d->d_devicename = strdup(devicename);
  d->d_type       = strdup(type);
  d->d_class      = dc;
  d->d_hostconnection = speed;
  LIST_INSERT_HEAD(&probed_devices, d, d_link);
  return d;
}


/**
 *
 */
static void
device_start(device_t *d, htsmsg_t *config)
{
  LIST_REMOVE(d, d_link);
  LIST_INSERT_HEAD(&running_devices, d, d_link);
  d->d_class->dc_start(d, config);
}


/**
 *
 */
static void
device_map0(int (*devcmp)(const device_t *d1, const device_t *d2))
{
  device_t *d, *next, *e;
  for(d = LIST_FIRST(&probed_devices); d != NULL; d = next) {
    next = LIST_NEXT(d, d_link);

    LIST_FOREACH(e, &conf_devices, d_link) {
      if(!devcmp(d, e))
	break;
    }
    if(e == NULL)
      continue;

    device_start(d, e->d_config);
    device_destroy(e);
  }
}


/**
 *
 */
static int
devcmp1(const device_t *d1, const device_t *d2)
{
  return strcmp(d1->d_path,       d2->d_path)
    ||   strcmp(d1->d_busid,      d2->d_busid)
    ||   strcmp(d1->d_devicename, d2->d_devicename)
    ||   strcmp(d1->d_type,       d2->d_type);
}

/**
 *
 */
static int
devcmp2(const device_t *d1, const device_t *d2)
{
  return strcmp(d1->d_busid,      d2->d_busid)
    ||   strcmp(d1->d_devicename, d2->d_devicename)
    ||   strcmp(d1->d_type,       d2->d_type);
}


/**
 *
 */
static int
devcmp3(const device_t *d1, const device_t *d2)
{
  return strcmp(d1->d_devicename, d2->d_devicename)
    ||   strcmp(d1->d_type,       d2->d_type);
}


/**
 *
 */
static int
devcmp4(const device_t *d1, const device_t *d2)
{
  return strcmp(d1->d_busid,      d2->d_busid)
    ||   strcmp(d1->d_type,       d2->d_type);
}


/**
 *
 */
static int
devcmp5(const device_t *d1, const device_t *d2)
{
  return strcmp(d1->d_type,       d2->d_type);
}


/**
 *
 */
void
device_map(void)
{
  device_t *d;

  device_map0(devcmp1);
  device_map0(devcmp2);
  device_map0(devcmp3);
  device_map0(devcmp4);
  device_map0(devcmp5);

  while((d = LIST_FIRST(&probed_devices)) != NULL)
    device_start(d, NULL);
}

/**
 *
 */
static void
device_save_list(htsmsg_t *all, struct device_list *list)
{
  device_t *d;
  htsmsg_t *dm;

  LIST_FOREACH(d, list, d_link) {
    dm = htsmsg_create_map();
    
    htsmsg_add_str(dm, "name", d->d_name);
    htsmsg_add_str(dm, "path", d->d_path);
    htsmsg_add_str(dm, "busid", d->d_busid);
    htsmsg_add_str(dm, "devicename", d->d_devicename);
    htsmsg_add_str(dm, "type", d->d_type);
    htsmsg_add_u32(dm, "enabled", d->d_enabled);
    
    if(d->d_class != NULL && d->d_class->dc_get_conf != NULL) {
      htsmsg_t *priv = d->d_class->dc_get_conf(d);
      if(priv != NULL)
	htsmsg_add_msg(dm, "config", priv);
    } else {
      if(d->d_config != NULL)
	htsmsg_add_msg(dm, "config", htsmsg_copy(d->d_config));
    }
    htsmsg_add_msg(all, NULL, dm);
  }
}


/**
 *
 */
void
device_save_all(void)
{
  htsmsg_t *all = htsmsg_create_list();
  device_save_list(all, &running_devices);
  device_save_list(all, &probed_devices);
  hts_settings_save(all, "devices");
}


/**
 *
 */
void
device_save(device_t *d)
{
  device_save_all();
}


/**
 *
 */
static void
device_load_one(htsmsg_t *m)
{
  device_t *d;
  const char *name       = htsmsg_get_str(m, "name");
  const char *path       = htsmsg_get_str(m, "path");
  const char *busid      = htsmsg_get_str(m, "busid");
  const char *devicename = htsmsg_get_str(m, "devicename");
  const char *type       = htsmsg_get_str(m, "type");
  htsmsg_t *config;

  if(!name || !path || !busid || !devicename || !type)
    return;

  d = calloc(1, sizeof(device_t));
  d->d_id         = ++deviceid;
  d->d_name       = strdup(name);
  d->d_path       = strdup(path);
  d->d_busid      = strdup(busid);
  d->d_devicename = strdup(devicename);
  d->d_type       = strdup(type);

  if((config = htsmsg_get_map(m, "config")) != NULL)
    d->d_config = htsmsg_copy(config);
  LIST_INSERT_HEAD(&conf_devices, d, d_link);
}


/**
 *
 */
void
device_init(void)
{
  htsmsg_t *c, *l = hts_settings_load("devices");
  htsmsg_field_t *f;

  if(l == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) != NULL)
      device_load_one(c);
  }
  htsmsg_destroy(l);
}
