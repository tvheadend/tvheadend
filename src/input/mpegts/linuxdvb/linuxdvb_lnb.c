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
#include "settings.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
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

  /* Freq control */
  int lnb_low;
  int lnb_high;
  int lnb_switch;
} linuxdvb_lnb_conf_t;

static const char *
linuxdvb_lnb_class_get_title ( idnode_t *o )
{
  static char buf[256];
  linuxdvb_diseqc_t *ld = (linuxdvb_diseqc_t*)o;
  snprintf(buf, sizeof(buf), "LNB: %s", ld->ld_type);
  return buf;
}

extern const idclass_t linuxdvb_diseqc_class;

const idclass_t linuxdvb_lnb_class =
{
  .ic_super       = &linuxdvb_diseqc_class,
  .ic_class       = "linuxdvb_lnb_basic",
  .ic_caption     = "LNB",
  .ic_get_title   = linuxdvb_lnb_class_get_title,
};

/* **************************************************************************
 * Control functions
 * *************************************************************************/

static int
diseqc_volt_tone ( int fd, int vol, int tone )
{
  /* Set voltage */
  tvhtrace("linuxdvb", "set LNB voltage %dV", vol ? 18 : 13);
  if (ioctl(fd, FE_SET_VOLTAGE, vol ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13)) {
    tvherror("linuxdvb", "failed to set LNB voltage (e=%s)", strerror(errno));
    return -1;
  }
  usleep(15000);

  /* Set tone */
  tvhtrace("linuxdvb", "set LNB tone %s", tone ? "on" : "off");
  if (ioctl(fd, FE_SET_TONE, tone ? SEC_TONE_ON : SEC_TONE_OFF)) {
    tvherror("linuxdvb", "failed to set LNB tone (e=%s)", strerror(errno));
    return -1;
  }

  return 0;
}

/*
 * Standard freq switched LNB
 */

static uint32_t
linuxdvb_lnb_standard_freq 
  ( linuxdvb_lnb_t *l, linuxdvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  int32_t                f = (int32_t)lm->lm_tuning.dmc_fe_params.frequency;
  if (lnb->lnb_switch && f > lnb->lnb_switch)
    f -= lnb->lnb_high;
  else
    f -= lnb->lnb_low;
  return (uint32_t)abs(f);
}

static int
linuxdvb_lnb_standard_band
  ( linuxdvb_lnb_t *l, linuxdvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  uint32_t               f = lm->lm_tuning.dmc_fe_params.frequency;
  return (lnb->lnb_switch && f > lnb->lnb_switch);
}

static int 
linuxdvb_lnb_standard_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, linuxdvb_satconf_t *ls, int fd )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)ld;
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  int pol = dmc->dmc_fe_polarisation == POLARISATION_HORIZONTAL ||
            dmc->dmc_fe_polarisation == POLARISATION_CIRCULAR_LEFT;
  int hi  = lnb->lnb_switch && (p->frequency > lnb->lnb_switch);

  return diseqc_volt_tone(fd, pol, hi);
}

/*
 * Bandstacked polarity switch LNB
 */

static uint32_t
linuxdvb_lnb_bandstack_freq
  ( linuxdvb_lnb_t *l, linuxdvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  int32_t                f = (int32_t)lm->lm_tuning.dmc_fe_params.frequency;
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  int pol = dmc->dmc_fe_polarisation == POLARISATION_HORIZONTAL ||
            dmc->dmc_fe_polarisation == POLARISATION_CIRCULAR_LEFT;
  if (pol)
    f -= lnb->lnb_high;
  else
    f -= lnb->lnb_low;
  return (uint32_t)abs(f);
}

static int
linuxdvb_lnb_bandstack_band
  ( linuxdvb_lnb_t *l, linuxdvb_mux_t *lm )
{
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  int pol = dmc->dmc_fe_polarisation == POLARISATION_HORIZONTAL ||
            dmc->dmc_fe_polarisation == POLARISATION_CIRCULAR_LEFT;
  return pol;
}

static int
linuxdvb_lnb_bandstack_tune
  ( linuxdvb_diseqc_t *ld, linuxdvb_mux_t *lm, linuxdvb_satconf_t *ls, int fd )
{
  return diseqc_volt_tone(fd, 1, 0);
}

/* **************************************************************************
 * Static list
 * *************************************************************************/

struct linuxdvb_lnb_conf linuxdvb_lnb_all[] = {
  {
    .ld_type    = "Universal",
    .lnb_low    =  9750000,
    .lnb_high   = 10600000,
    .lnb_switch = 11700000,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "Standard",
    .lnb_low    = 10000000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "Enhanced",
    .lnb_low    =  9750000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "C-Band",
    .lnb_low    =  5150000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "Circular 10750",
    .lnb_low    = 10750000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "Ku 11300",
    .lnb_low    = 11300000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "DBS",
    .lnb_low    = 11250000,
    .lnb_high   = 0,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_standard_freq,
    .lnb_band   = linuxdvb_lnb_standard_band,
    .ld_tune    = linuxdvb_lnb_standard_tune,
  },
  {
    .ld_type    = "DBS Bandstack",
    .lnb_low    = 11250000,
    .lnb_high   = 14350000,
    .lnb_switch = 0,
    .lnb_freq   = linuxdvb_lnb_bandstack_freq,
    .lnb_band   = linuxdvb_lnb_bandstack_band,
    .ld_tune    = linuxdvb_lnb_bandstack_tune,
  },
};

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

htsmsg_t *
linuxdvb_lnb_list ( void *o )
{
  int i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_all); i++)
    htsmsg_add_str(m, NULL, linuxdvb_lnb_all[i].ld_type);
  return m;
}

linuxdvb_lnb_t *
linuxdvb_lnb_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_t *ls )
{
  int i;

  /* Setup static entries */
  for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_all); i++)
    if (!linuxdvb_lnb_all[i].ld_id.in_class)
      idnode_insert(&linuxdvb_lnb_all[i].ld_id, NULL, &linuxdvb_lnb_class);

  /* Find */
  if (name) {
    for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_all); i++) {
      if (!strcmp(linuxdvb_lnb_all[i].ld_type, name))
        return (linuxdvb_lnb_t*)&linuxdvb_lnb_all[i];
    }
  }
  return (linuxdvb_lnb_t*)linuxdvb_lnb_all; // Universal
}

void
linuxdvb_lnb_destroy ( linuxdvb_lnb_t *lnb )
{
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
