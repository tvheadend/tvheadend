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

#ifndef DEVICE_H__
#define DEVICE_H__

#include <stdlib.h>
#include "queue.h"

struct htsmsg;

LIST_HEAD(device_list, device);

typedef struct device {
  LIST_ENTRY(device) d_link;
  const struct deviceclass *d_class;  // is NULL if device is not mapped to HW
  int d_id;
  char *d_name;

  char *d_path;
  char *d_busid;
  char *d_devicename;
  char *d_type;

  int d_enabled;

  enum {
    HOSTCONNECTION_UNKNOWN,
    HOSTCONNECTION_USB12,
    HOSTCONNECTION_USB480,
    HOSTCONNECTION_PCI,
  } d_hostconnection;

  struct htsmsg *d_config;

} device_t;


typedef struct deviceclass {
  size_t dc_size;

  void (*dc_start)(device_t *d, struct htsmsg *config);

  struct htsmsg *(*dc_get_conf)(device_t *d);

} deviceclass_t;

void *device_add(const char *path, const char *busid, const char *devicename,
		 const char *type, deviceclass_t *dc);

void device_save(device_t *d);

void device_save_all(void);

void device_init(void);

void device_map(void);

#endif // DEVICE_H__
