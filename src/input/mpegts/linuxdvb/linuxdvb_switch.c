/*
 *  Tvheadend - Linux DVB DiseqC switch
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

typedef struct linuxdvb_switch
{
  linuxdvb_diseqc_t;

  /* Port settings */
  int ls_toneburst;
  int ls_committed;
  int ls_uncomitted;

} linuxdvb_switch_t;

static htsmsg_t *
linuxdvb_switch_class_committed_list ( void *o )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "AA");
  htsmsg_add_str(m, NULL, "AB");
  htsmsg_add_str(m, NULL, "BA");
  htsmsg_add_str(m, NULL, "BB");
  return m;
}

static htsmsg_t *
linuxdvb_switch_class_toneburst_list ( void *o )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "A");
  htsmsg_add_str(m, NULL, "B");
  return m;
}

const idclass_t linuxdvb_switch_class =
{
  .ic_class       = "linuxdvb_switch",
  .ic_caption     = "DiseqC switch",
  .ic_properties  = (const property_t[]) {
    {
      .type   = PT_INT,
      .id     = "committed",
      .name   = "Committed",
      .off    = offsetof(linuxdvb_switch_t, ls_committed),
      .list   = linuxdvb_switch_class_committed_list
    },
    {
      .type   = PT_INT,
      .id     = "uncommitted",
      .name   = "Uncommitted",
      .off    = offsetof(linuxdvb_switch_t, ls_uncomitted),
    },
    {
      .type   = PT_INT,
      .id     = "Tone Burst",
      .name   = "toneburst",
      .off    = offsetof(linuxdvb_switch_t, ls_toneburst),
      .list   = linuxdvb_switch_class_toneburst_list
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_switch_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, int fd )
{
  linuxdvb_switch_t *ls = (linuxdvb_switch_t*)ld;
  
  // TODO: retransmit
  // TODO: build into above protocol
  
  /* Uncommitted */
  if (ls->ls_uncomitted) {
    int s = 0xF0 | (ls->ls_uncomitted - 1);
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x10, 0x39, 1, s))
      return -1;
    usleep(15000);
  }

  /* Committed */
  if (ls->ls_committed) {
    int s = 0xF0 | (ls->ls_committed - 1);
    if (linuxdvb_diseqc_send(fd, 0xE0, 0x10, 0x38, 1, s))
      return -1;
  }

  /* Tone burst */
  if (ls->ls_toneburst) {
    tvhtrace("linuxdvb", "toneburst %s", ls->ls_toneburst == 2 ? "on" : "off");
    if (ioctl(fd, FE_SET_TONE, ls->ls_toneburst ? SEC_TONE_ON : SEC_TONE_OFF)) {
      tvherror("linuxdvb", "failed to set toneburst (e=%s)", strerror(errno));
      return -1;
    }
  }

  return 0;
}

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

linuxdvb_diseqc_t *
linuxdvb_switch_create0
  ( const char *name, htsmsg_t *conf )
{
  linuxdvb_diseqc_t *ld
    = linuxdvb_diseqc_create(linuxdvb_switch, NULL, conf);
  if (ld) {
    ld->ld_tune = linuxdvb_switch_tune;
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
