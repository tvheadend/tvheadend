/*
 *  Tvheadend - Linux DVB Service
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

static void
linuxdvb_service_config_save ( service_t *t )
{
  htsmsg_t *c = htsmsg_create_map();
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_service_save(s, c);
  hts_settings_save(c, "input/linuxdvb/networks/%s/muxes/%s/services/%s",
                    idnode_uuid_as_str(&s->s_dvb_mux->mm_network->mn_id),
                    idnode_uuid_as_str(&s->s_dvb_mux->mm_id),
                    idnode_uuid_as_str(&s->s_id));
  htsmsg_destroy(c);
}

mpegts_service_t *
linuxdvb_service_create0
  ( linuxdvb_mux_t *mm, uint16_t sid, uint16_t pmt_pid,
    const char *uuid, htsmsg_t *conf )
{
  extern const idclass_t mpegts_service_class;
  mpegts_service_t *s
    = mpegts_service_create1(uuid, (mpegts_mux_t*)mm, sid, pmt_pid, conf);
  if (s)
    s->s_config_save = linuxdvb_service_config_save;
  return s;
}
