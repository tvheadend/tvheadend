/*
 *  Tvheadend - Linux DVB LNB config
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

typedef struct linuxdvb_lnb_conf
{
  linuxdvb_lnb_t;

  /* Name/ID */
  const char *lnb_name;

  /* Freq control */
  int lnb_low;
  int lnb_high;
  int lnb_switch;
} linuxdvb_lnb_conf_t;

/* **************************************************************************
 * Control functions
 * *************************************************************************/

/*
 * Standard freq switched LNB
 */

static uint32_t
linuxdvb_lnb_standard_freq 
  ( linuxdvb_lnb_t *l, linuxdvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  uint32_t               f = lm->lm_tuning.dmc_fe_params.frequency;
  if (lnb->lnb_switch && f > lnb->lnb_switch)
    f -= lnb->lnb_high;
  else
    f -= lnb->lnb_low;
  return f;
}

static int 
linuxdvb_lnb_standard_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, int fd )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)ld;
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  int pol = dmc->dmc_fe_polarisation == POLARISATION_HORIZONTAL ||
            dmc->dmc_fe_polarisation == POLARISATION_CIRCULAR_LEFT;
  int hi  = lnb->lnb_switch && (p->frequency > lnb->lnb_switch);
  // TODO: we need to put switch commands into here!
  return diseqc_setup(fd, 0, pol, hi, 0, 0);
}

/*
 * Bandstacked polarity switch LNB
 */

/* **************************************************************************
 * Static list
 * *************************************************************************/

struct linuxdvb_lnb_conf linuxdvb_lnb_list[] = {
  {
    .lnb_name   = "Universal",
    .lnb_low    =  9750000,
    .lnb_high   = 10600000,
    .lnb_switch = 11700000,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  }
};



/* **************************************************************************
 * Create / Config
 * *************************************************************************/

linuxdvb_lnb_t *
linuxdvb_lnb_create0
  ( const char *name, htsmsg_t *conf )
{
  int i;
  for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_list); i++) {
    if (!strcmp(linuxdvb_lnb_list[i].lnb_name, name))
      return (linuxdvb_lnb_t*)&linuxdvb_lnb_list[i];
  }
  return NULL;
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
