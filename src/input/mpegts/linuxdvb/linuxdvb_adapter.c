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
#include "linuxdvb_private.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

/*
 * Get bus information
 */
#if 0
static void
get_host_connection ( linuxdvb_adapter_t *la, int a )
{
  FILE *fp;
  int speed;
  char path[512], buf[512];

  /* Check for subsystem */
#define DVB_DEV_PATH "/sys/class/dvb/dvb%d.frontend0/device"
  snprintf(path, sizeof(path), DVB_DEV_PATH "/subsystem", a);
  if (readlink(tpath, buf, sizeof(buf)) != -1) {
    char *bus = basename(buf);
    if (!strcmp(bus, "pci")) {
      i->bus = BUS_PCI;
    } else if (!strcmp(bus, "usb")) {
      i->bus = BUS_USB1;
      snprintf(path, sizeof(path), DVB_DEV_PATH "/speed", a);
      if ((fp = fopen(path, "r"))) {
        if (fscanf(fp, "%d", &speed) == 1) {
          if (speed > 480) {
            i->bus = USB3;
          } else if (speed == 480) {
            i->bus = USB2;
          }
        }
        fclose(fp);
      }
    } else {
      tvhlog(LOG_WARN, "linuxdvb",
             "could not determine host connection for adapter%d", a);
    }
  }

  /* Get ID */
  snprintf(path, sizeof(path), DVB_DEV_PATH, a);
  if (readlink(tpath, buf, sizeof(buf)) != -1)
    strncpy(i->id, basename(buf), sizeof(i->id));
}
#endif

/*
 * Check is free
 */
static int
linuxdvb_adapter_is_free ( mpegts_input_t *mi )
{
  linuxdvb_hardware_t *lh;
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)mi;

  TAILQ_FOREACH(lh, &la->lh_childs, lh_parent_link)
    if (!lh->lh_is_free(lh))
      return 0;
  return 1;
}

/*
 * Get current weight
 */
static int
linuxdvb_adapter_current_weight ( mpegts_input_t *mi )
{
  int w = 0;
  linuxdvb_hardware_t *lh;
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)mi;

  TAILQ_FOREACH(lh, &la->lh_childs, lh_parent_link)
    w = MAX(w, lh->lh_current_weight(lh));
  return w;
}

/*
 * Load an adapter
 */
linuxdvb_adapter_t *
linuxdvb_adapter_create ( int adapter )
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

    /* Create adapter */
    if (!la) {
      //la = linuxdvb_hardware_create(linuxdvb_adapter_t);
      la->mi_is_free        = linuxdvb_adapter_is_free;
      la->mi_current_weight = linuxdvb_adapter_current_weight;
    }

    /* Create frontend */
    //linuxdvb_frontend_create(la, fe_path, dmx_path, dvr_path, &dfi);
  }

  return la;
}
