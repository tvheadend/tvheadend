/*
 *  tvheadend - Codec Profiles - shared DRM device enumeration
 *
 *  Copyright (C) 2016 Tvheadend
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

#include "drmdev.h"

#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <linux/types.h>
#include <asm/ioctl.h>
#else
#include <sys/ioccom.h>
#include <sys/types.h>
typedef size_t   __kernel_size_t;
#endif

typedef struct drm_version {
   int version_major;        /**< Major version */
   int version_minor;        /**< Minor version */
   int version_patchlevel;   /**< Patch level */
   __kernel_size_t name_len; /**< Length of name buffer */
   char *name;               /**< Name of driver */
   __kernel_size_t date_len; /**< Length of date buffer */
   char *date;               /**< User-space buffer to hold date */
   __kernel_size_t desc_len; /**< Length of desc buffer */
   char *desc;               /**< User-space buffer to hold desc */
} drm_version_t;

#define DRM_IOCTL_VERSION _IOWR('d', 0x00, struct drm_version)

static int
tvh_probe_drm_device(const char *device, char *name, size_t namelen)
{
    drm_version_t dv;
    char dname[128];
    int fd;

    if ((fd = open(device, O_RDWR)) < 0)
        return -1;
    memset(&dv, 0, sizeof(dv));
    memset(dname, 0, sizeof(dname));
    dv.name = dname;
    dv.name_len = sizeof(dname)-1;
    if (ioctl(fd, DRM_IOCTL_VERSION, &dv) < 0) {
        close(fd);
        return -1;
    }
    snprintf(name, namelen, "%s v%d.%d.%d (%s)",
             dv.name, dv.version_major, dv.version_minor,
             dv.version_patchlevel, device);
    close(fd);
    return 0;
}

htsmsg_t *
tvh_drm_device_list(void *obj, const char *lang)
{
    htsmsg_t *result = htsmsg_create_list();
    char device[PATH_MAX];
    char name[128];
    int i, dev_num;

    for (i = 0; i < 32; i++) {
        dev_num = i + 128;
        snprintf(device, sizeof(device), "/dev/dri/renderD%d", dev_num);
        if (tvh_probe_drm_device(device, name, sizeof(name)) == 0)
            htsmsg_add_msg(result, NULL, htsmsg_create_key_val(device, name));
    }
    for (i = 0; i < 32; i++) {
        // card nodes are numbered from 0 (render nodes from 128)
        dev_num = i;
        snprintf(device, sizeof(device), "/dev/dri/card%d", dev_num);
        if (tvh_probe_drm_device(device, name, sizeof(name)) == 0)
            htsmsg_add_msg(result, NULL, htsmsg_create_key_val(device, name));
    }
    return result;
}
