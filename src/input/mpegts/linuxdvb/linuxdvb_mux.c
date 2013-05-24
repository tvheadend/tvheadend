/*
 *  Tvheadend - Linux DVB Multiplex
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

/*
 * Class definition
 */
extern const idclass_t mpegts_mux_class;
const idclass_t linuxdvb_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "linuxdvb_mux",
  .ic_caption    = "Linux DVB Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_mux_dvbt_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbt",
  .ic_caption    = "Linux DVB-T Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_mux_dvbc_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbc",
  .ic_caption    = "Linux DVB-C Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_mux_dvbs_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbs",
  .ic_caption    = "Linux DVB-S Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_mux_atsc_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_atsc",
  .ic_caption    = "Linux ATSC Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

/*
 * Mux Objects
 */

static void
linuxdvb_mux_config_save ( mpegts_mux_t *mm )
{
}

#if 0
static int
linuxdvb_mux_start ( mpegts_mux_t *mm, const char *reason, int weight )
{
  return SM_CODE_TUNING_FAILED;
}

static void
linuxdvb_mux_stop ( mpegts_mux_t *mm )
{
}
#endif

static void
linuxdvb_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}

static void
linuxdvb_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}


linuxdvb_mux_t *
linuxdvb_mux_create0
  ( linuxdvb_network_t *ln, const char *uuid, htsmsg_t *conf )
{
  //uint32_t u32;
  //const char *str;
  mpegts_mux_t *mm;
  linuxdvb_mux_t *lm;
  const idclass_t *idc;

  /* Class */
  if (ln->ln_type == FE_QPSK)
    idc = &linuxdvb_mux_dvbs_class;
  else if (ln->ln_type == FE_QAM)
    idc = &linuxdvb_mux_dvbc_class;
  else if (ln->ln_type == FE_OFDM)
    idc = &linuxdvb_mux_dvbt_class;
  else if (ln->ln_type == FE_ATSC)
    idc = &linuxdvb_mux_atsc_class;
  else {
    tvhlog(LOG_ERR, "linuxdvb", "unknown FE type %d", ln->ln_type);
    return NULL;
  }

  /* Create */
  if (!(mm = mpegts_mux_create0(calloc(1, sizeof(linuxdvb_mux_t)), idc, uuid,
                                (mpegts_network_t*)ln,
                                MPEGTS_ONID_NONE,
                                MPEGTS_TSID_NONE)))
    return NULL;
  lm = (linuxdvb_mux_t*)mm;
  
  /* Callbacks */
  lm->mm_config_save = linuxdvb_mux_config_save;
#if 0
  lm->mm_start       = linuxdvb_mux_start;
  lm->mm_stop        = linuxdvb_mux_stop;
#endif
  lm->mm_open_table  = linuxdvb_mux_open_table;
  lm->mm_close_table = linuxdvb_mux_close_table;

  /* No config */
  if (!conf)
    return lm;

  /* Config */

  return lm;
}
