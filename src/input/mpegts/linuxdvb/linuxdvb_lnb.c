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
linuxdvb_lnb_class_get_title ( idnode_t *o, const char *lang )
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
  .ic_caption     = N_("LNB"),
  .ic_get_title   = linuxdvb_lnb_class_get_title,
};

/* **************************************************************************
 * Control functions
 * *************************************************************************/

/*
 * Standard freq switched LNB
 */

static uint32_t
linuxdvb_lnb_standard_freq 
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  int32_t                f = (int32_t)lm->lm_tuning.dmc_fe_freq;
  if (lnb->lnb_switch && f > lnb->lnb_switch)
    f -= lnb->lnb_high;
  else
    f -= lnb->lnb_low;
  return (uint32_t)abs(f);
}

static int
linuxdvb_lnb_bandstack_match
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm1, dvb_mux_t *lm2 )
{
  /* everything is in one cable */
  return 1;
}

static int
linuxdvb_lnb_standard_match
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm1, dvb_mux_t *lm2 )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  dvb_mux_conf_t      *dmc1 = &lm1->lm_tuning;
  dvb_mux_conf_t      *dmc2 = &lm2->lm_tuning;

  if (lnb->lnb_switch) {
    uint32_t hi1 = dmc1->dmc_fe_freq > lnb->lnb_switch;
    uint32_t hi2 = dmc2->dmc_fe_freq > lnb->lnb_switch;
    if (hi1 != hi2)
      return 0;
  }
  if (dmc1->u.dmc_fe_qpsk.polarisation != dmc2->u.dmc_fe_qpsk.polarisation)
    return 0;
  return 1;
}

static int
linuxdvb_lnb_standard_band
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  uint32_t               f = lm->lm_tuning.dmc_fe_freq;
  return (lnb->lnb_switch && f > lnb->lnb_switch);
}

static int
linuxdvb_lnb_standard_pol
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  return dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_HORIZONTAL ||
         dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_CIRCULAR_LEFT;
}

static int
linuxdvb_lnb_inverted_pol
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  return !linuxdvb_lnb_standard_pol(l, lm);
}

static int 
linuxdvb_lnb_standard_tune
  ( linuxdvb_diseqc_t *ld, dvb_mux_t *lm,
    linuxdvb_satconf_t *lsp, linuxdvb_satconf_ele_t *ls,
    int vol, int pol, int band, int freq )
{
  return linuxdvb_diseqc_set_volt(ls->lse_parent, vol);
}

/*
 * Bandstacked polarity switch LNB
 */

static uint32_t
linuxdvb_lnb_bandstack_freq
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  linuxdvb_lnb_conf_t *lnb = (linuxdvb_lnb_conf_t*)l;
  int32_t                f = (int32_t)lm->lm_tuning.dmc_fe_freq;
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  int pol = dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_HORIZONTAL ||
            dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_CIRCULAR_LEFT;
  if (pol)
    f -= lnb->lnb_high;
  else
    f -= lnb->lnb_low;
  return (uint32_t)abs(f);
}

static int
linuxdvb_lnb_bandstack_band
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  dvb_mux_conf_t      *dmc = &lm->lm_tuning;
  int pol = dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_HORIZONTAL ||
            dmc->u.dmc_fe_qpsk.polarisation == DVB_POLARISATION_CIRCULAR_LEFT;
  return pol;
}

static int
linuxdvb_lnb_bandstack_pol
  ( linuxdvb_lnb_t *l, dvb_mux_t *lm )
{
  return 0;
}

/* **************************************************************************
 * Static list
 * *************************************************************************/

struct linuxdvb_lnb_conf linuxdvb_lnb_all[] = {
  {
    { {
      .ld_type    = "Universal",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    =  9750000,
    .lnb_high   = 10600000,
    .lnb_switch = 11700000,
  },
  {
    { {
      .ld_type    = "Standard",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 10000000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "Enhanced",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    =  9750000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "C-Band",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    =  5150000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "C-Band (bandstack)",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_bandstack_freq,
      .lnb_match  = linuxdvb_lnb_bandstack_match,
      .lnb_band   = linuxdvb_lnb_bandstack_band,
      .lnb_pol    = linuxdvb_lnb_bandstack_pol,
    },
    .lnb_low    =  5150000,
    .lnb_high   =  5750000,
    .lnb_switch =  0,
  },
  {
    { {
      .ld_type    = "Ku 10750",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 10750000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "Ku 10750 (Hi-Band)",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 10750000,
    .lnb_high   = 10750000,
    .lnb_switch = 10750000,
  },
  {
    { {
      .ld_type    = "Ku 10750 (Hi-Band, Inverted-Polar.)",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_inverted_pol,
    },
    .lnb_low    = 10750000,
    .lnb_high   = 10750000,
    .lnb_switch = 10750000,
  },
  {
    { {
      .ld_type    = "Ku 11300",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 11300000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "Ku 11300 (Hi-Band)",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 11300000,
    .lnb_high   = 11300000,
    .lnb_switch = 11300000,
  },
  {
     { {
       .ld_type    = "C 5150/Ku 11750 (22khz switch)",
       .ld_tune    = linuxdvb_lnb_standard_tune,
       },
       .lnb_freq   = linuxdvb_lnb_standard_freq,
       .lnb_band   = linuxdvb_lnb_standard_band,
       .lnb_pol    = linuxdvb_lnb_standard_pol,
     },
     .lnb_low    =  5150000,
     .lnb_high   = 10750000,
     .lnb_switch = 11700000,
  },
  {
    { {
      .ld_type    = "DBS",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 11250000,
    .lnb_high   = 0,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "DBS Bandstack",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_bandstack_freq,
      .lnb_match  = linuxdvb_lnb_bandstack_match,
      .lnb_band   = linuxdvb_lnb_bandstack_band,
      .lnb_pol    = linuxdvb_lnb_bandstack_pol,
    },
    .lnb_low    = 11250000,
    .lnb_high   = 14350000,
    .lnb_switch = 0,
  },
  {
    { {
      .ld_type    = "Ku 10700 (Australia)",
      .ld_tune    = linuxdvb_lnb_standard_tune,
      },
      .lnb_freq   = linuxdvb_lnb_standard_freq,
      .lnb_match  = linuxdvb_lnb_standard_match,
      .lnb_band   = linuxdvb_lnb_standard_band,
      .lnb_pol    = linuxdvb_lnb_standard_pol,
    },
    .lnb_low    = 10700000,
    .lnb_high   = 10700000,
    .lnb_switch = 11800000,
  },
};

/* **************************************************************************
 * Create / Config
 * *************************************************************************/

htsmsg_t *
linuxdvb_lnb_list ( void *o, const char *lang )
{
  int i;
  htsmsg_t *m = htsmsg_create_list();
  for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_all); i++)
    htsmsg_add_str(m, NULL, linuxdvb_lnb_all[i].ld_type);
  return m;
}

linuxdvb_lnb_t *
linuxdvb_lnb_create0
  ( const char *name, htsmsg_t *conf, linuxdvb_satconf_ele_t *ls )
{
  int i;

  /* Setup static entries */
  for (i = 0; i < ARRAY_SIZE(linuxdvb_lnb_all); i++)
    if (!linuxdvb_lnb_all[i].ld_id.in_class)
      idnode_insert(&linuxdvb_lnb_all[i].ld_id, NULL, &linuxdvb_lnb_class, 0);

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
