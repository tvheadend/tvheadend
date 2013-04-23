/*
 *  DVB for Linux
 *  Copyright (C) 2013 Andreas Öman
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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_support.h"


typedef struct linux_dvb_frontend {
  dvb_hardware_t ldf_dh;
  int ldf_adapterid;
  int ldf_frontend;

  int ldf_fd;  // if -1, frontend is closed

} linux_dvb_frontend_t;




static const idclass_t linux_dvb_hardware_class = {
  .ic_class = "linux_dvb_hardware",
  .ic_get_childs = dvb_hardware_get_childs,
  .ic_get_title = dvb_hardware_get_title,
  .ic_properties = (const property_t[]){
    {
      "name", "Name", PT_STR,
      offsetof(dvb_hardware_t, dh_name),
      .notify = &idnode_notify_title_changed,
    }, {}},
};



static const idclass_t linux_dvb_adapter_class = {
  .ic_class = "linux_dvb_adapter",
  .ic_super = &linux_dvb_hardware_class,
};


static const idclass_t linux_dvb_frontend_class = {
  .ic_class = "linux_dvb_frontend",
  .ic_super = &linux_dvb_hardware_class,
};










static const idclass_t linux_dvbc_frontend_class = {
  .ic_leaf = 1,
  .ic_class = "linux_dvbc_frontend",
  .ic_super = &linux_dvb_frontend_class,
};

static const idclass_t linux_dvbt_frontend_class = {
  .ic_leaf = 1,
  .ic_class = "linux_dvbt_frontend",
  .ic_super = &linux_dvb_frontend_class,
};

static const idclass_t linux_atsc_frontend_class = {
  .ic_leaf = 1,
  .ic_class = "linux_atsc_frontend",
  .ic_super = &linux_dvb_frontend_class,
};

static const idclass_t linux_dvbs_frontend_class = {
  .ic_class = "linux_dvbs_frontend",
  .ic_super = &linux_dvb_frontend_class,
};



/**
 *
 */
static void
linux_dvb_frontend_create(dvb_hardware_t *parent, int adapterid,
                          int frontendid, const struct dvb_frontend_info *dfi)
{
  const idclass_t *class;

  switch(dfi->type) {
  case FE_OFDM:
    class = &linux_dvbt_frontend_class;
    break;
  case FE_QAM:
    class = &linux_dvbc_frontend_class;
    break;
  case FE_QPSK:
    class = &linux_dvbs_frontend_class;
    break;
  case FE_ATSC:
    class = &linux_atsc_frontend_class;
    break;
  default:
    return;
  }

  //  dvb_hardware_t *fe =
    dvb_hardware_create(class,
                        sizeof(linux_dvb_frontend_t), parent, NULL, dfi->name);
}


/**
 *
 */
static void
add_adapter(int aid)
{
  int frontends;
  int demuxers;
  int dvrs;
  int i;
  char path[PATH_MAX];
  dvb_hardware_t *a = NULL;

  for(frontends = 0; frontends < 32; frontends++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d/frontend%d",
             aid, frontends);
    if(access(path, R_OK | W_OK))
      break;
  }

  if(frontends == 0)
    return; // Nothing here

  for(demuxers = 0; demuxers < 32; demuxers++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d/demux%d",
             aid, demuxers);
    if(access(path, R_OK | W_OK))
      break;
  }

  for(dvrs = 0; dvrs < 32; dvrs++) {
    snprintf(path, sizeof(path), "/dev/dvb/adapter%d/dvr%d",
             aid, dvrs);
    if(access(path, R_OK | W_OK))
      break;
  }

  tvhlog(LOG_INFO, "DVB",
         "Linux DVB adapter %d: %d frontends, %d demuxers, %d DVRs",
         aid, frontends, demuxers, dvrs);

  for(i = 0; i < frontends; i++) {

    snprintf(path, sizeof(path), "/dev/dvb/adapter%d/frontend%d", aid, i);

    int fd = tvh_open(path, O_RDWR | O_NONBLOCK, 0);
    if(fd == -1) {
      tvhlog(LOG_ALERT, "DVB",
             "Unable to open %s -- %s", path, strerror(errno));
      continue;
    }

    struct dvb_frontend_info dfi;

    int r = ioctl(fd, FE_GET_INFO, &dfi);
    close(fd);
    if(r) {
      tvhlog(LOG_ALERT, "DVB", "%s: Unable to query adapter", path);
      continue;
    }

    if(a == NULL) {
      char name[512];
      snprintf(name, sizeof(name), "/dev/dvb/adapter%d", aid);

      a = dvb_hardware_create(&linux_dvb_adapter_class, sizeof(dvb_hardware_t),
                              NULL, NULL, name);
    }
    linux_dvb_frontend_create(a, aid, i, &dfi);
  }
}


/**
 *
 */
void
dvb_linux_init(void)
{
  int i;
  for(i = 0 ; i < 32; i++)
    add_adapter(i);
}


