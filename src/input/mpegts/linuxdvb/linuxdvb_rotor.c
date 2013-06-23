/*
 *  Tvheadend - Linux DVB DiseqC Rotor
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
#include "diseqc.h"
#include "settings.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

typedef struct linuxdvb_rotor
{
  linuxdvb_diseqc_t;

  /* Port settings */
  int ls_toneburst;
  int ls_committed;
  int ls_uncomitted;

} linuxdvb_rotor_t;

const idclass_t linuxdvb_rotor_class =
{
  .ic_class       = "linuxdvb_switch",
  .ic_caption     = "DiseqC switch",
  .ic_properties  = (const property_t[]) {
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_rotor_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, int fd )
{
  return 0;
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

linuxdvb_diseqc_t *
linuxdvb_rotor_create0
  ( const char *name, htsmsg_t *conf )
{
  linuxdvb_diseqc_t *ld
    = linuxdvb_diseqc_create(linuxdvb_rotor, NULL, conf);
  if (ld) {
    ld->ld_tune = linuxdvb_rotor_tune;
  }

  return ld;
}

#if 0
void
linuxvb_lnb_destroy ( linuxdvb_lnb_t *lnb )
{
}
#endif

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
