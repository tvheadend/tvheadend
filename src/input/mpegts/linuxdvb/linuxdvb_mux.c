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

static void
linuxdvb_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  size_t c = 0;
  const char *unit = "Hz";
  linuxdvb_mux_t *lm = (linuxdvb_mux_t*)mm;
  c = snprintf(buf+c, len-c, "%d %s [%04X:%04X]",
               lm->lm_tuning.dmc_fe_params.frequency,
               unit, mm->mm_onid, mm->mm_tsid);
}

static void
linuxdvb_mux_create_instances ( mpegts_mux_t *mm )
{
  extern const idclass_t mpegts_mux_instance_class;
  mpegts_input_t *mi;
  mpegts_mux_instance_t *mmi;
  tvhtrace("linuxdvb", "mm %p create instances", mm);
  LIST_FOREACH(mi, &mm->mm_network->mn_inputs, mi_network_link) {
    tvhtrace("linuxdvb", "  checking mi %p", mi);
    LIST_FOREACH(mmi, &mi->mi_mux_instances, mmi_input_link)
      if (mmi->mmi_mux == mm) break;
    if (!mmi) {
      mmi = mpegts_mux_instance_create(mpegts_mux_instance, NULL, mi, mm);
      tvhtrace("linuxdvb", "    created mmi %p", mmi);
    }
    // TODO: we might eventually want to keep history!
  }
}

#if 0
static void
linuxdvb_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}

static void
linuxdvb_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}
#endif

linuxdvb_mux_t *
linuxdvb_mux_create0
  ( linuxdvb_network_t *ln,
    uint16_t onid, uint16_t tsid, const dvb_mux_conf_t *dmc,
    const char *uuid )
{
  const idclass_t *idc;
  mpegts_mux_t *mm;
  linuxdvb_mux_t *lm;

  /* Search for existing */

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
                                (mpegts_network_t*)ln, onid, tsid)))
    return NULL;
  lm = (linuxdvb_mux_t*)mm;
  memcpy(&lm->lm_tuning, dmc, sizeof(dvb_mux_conf_t));

  /* Callbacks */
  lm->mm_display_name     = linuxdvb_mux_display_name;
  lm->mm_config_save      = linuxdvb_mux_config_save;
  lm->mm_create_instances = linuxdvb_mux_create_instances;

  return (linuxdvb_mux_t*)mm;
}

linuxdvb_mux_t *
linuxdvb_mux_create1
  ( linuxdvb_network_t *ln, const char *uuid, htsmsg_t *conf )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  linuxdvb_mux_t *lm;
  dvb_mux_conf_t dmc;

  /* Check tuning */
  memset(&dmc, 0, sizeof(dmc));
  if (conf) {
    if ((str = dvb_mux_conf_load(ln->ln_type, &dmc, conf))) {
      tvhlog(LOG_ERR, "linuxdvb", "failed to load mux config [%s]", str);
      return NULL;
    }
  }

  lm = linuxdvb_mux_create0(ln, MPEGTS_ONID_NONE,
                            MPEGTS_TSID_NONE, &dmc, uuid);
  if (!lm) printf("OH DEAR\n");
  if (!lm) return NULL;
  
  /* No config */
  if (!conf)
    return lm;

  /* Config */
  // TODO: this could go in mpegts_mux
  if (!htsmsg_get_u32(conf, "enabled", &u32) && u32)
    lm->mm_enabled = 1;
  if (!htsmsg_get_u32(conf, "onid", &u32))
    lm->mm_onid = u32;
  if (!htsmsg_get_u32(conf, "tsid", &u32))
    lm->mm_tsid = u32;
  if ((str = htsmsg_get_str(conf, "default_authority")))
    lm->mm_dvb_default_authority = strdup(str);
  memcpy(&lm->lm_tuning, &dmc, sizeof(dmc));

  /* Services */
  if ((c = hts_settings_load_r(1, "input/linuxdvb/networks/%s/muxes/%s/services",
                               "TODO", uuid))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      //(void)linuxdvb_service_create0(lm, f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }

  return lm;
}
