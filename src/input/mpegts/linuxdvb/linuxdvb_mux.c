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

extern const idclass_t mpegts_mux_instance_class;

static void
linuxdvb_mux_create_instances ( mpegts_mux_t *mm )
{
  mpegts_input_t *mi;
  mpegts_mux_instance_t *mmi;
  LIST_FOREACH(mi, &mm->mm_network->mn_inputs, mi_network_link) {
    LIST_FOREACH(mmi, &mi->mi_mux_instances, mmi_input_link)
      if (mmi->mmi_mux == mm) break;
    if (!mmi)
      mmi = mpegts_mux_instance_create(mpegts_mux_instance, NULL, mi, mm);
    // TODO: we might eventually want to keep history!
  }
}

static void
linuxdvb_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}

static void
linuxdvb_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
}

static const char *
dvb_mux_conf_load ( fe_type_t type, dvb_mux_conf_t *dmc, htsmsg_t *m )
{
  //const char *s;
  dmc->dmc_fe_params.inversion = INVERSION_AUTO;
  htsmsg_get_u32(m, "frequency", &dmc->dmc_fe_params.frequency);

#if 0
  switch(tda->tda_type) {
  case FE_OFDM:
    s = htsmsg_get_str(m, "bandwidth");
    if(s == NULL || (r = str2val(s, bwtab)) < 0)
      return "Invalid bandwidth";
    dmc->dmc_fe_params.u.ofdm.bandwidth = r;

    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid QAM constellation";
    dmc->dmc_fe_params.u.ofdm.constellation = r;

    s = htsmsg_get_str(m, "transmission_mode");
    if(s == NULL || (r = str2val(s, modetab)) < 0)
      return "Invalid transmission mode";
    dmc->dmc_fe_params.u.ofdm.transmission_mode = r;

    s = htsmsg_get_str(m, "guard_interval");
    if(s == NULL || (r = str2val(s, guardtab)) < 0)
      return "Invalid guard interval";
    dmc->dmc_fe_params.u.ofdm.guard_interval = r;

    s = htsmsg_get_str(m, "hierarchy");
    if(s == NULL || (r = str2val(s, hiertab)) < 0)
      return "Invalid heirarchy information";
    dmc->dmc_fe_params.u.ofdm.hierarchy_information = r;

    s = htsmsg_get_str(m, "fec_hi");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid hi-FEC";
    dmc->dmc_fe_params.u.ofdm.code_rate_HP = r;

    s = htsmsg_get_str(m, "fec_lo");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid lo-FEC";
    dmc->dmc_fe_params.u.ofdm.code_rate_LP = r;
    break;
  }
#endif
  return "Not yet supported";
}

linuxdvb_mux_t *
linuxdvb_mux_create0
  ( linuxdvb_network_t *ln, const char *uuid, htsmsg_t *conf )
{
  uint32_t u32;
  const char *str;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
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
  lm->mm_config_save      = linuxdvb_mux_config_save;
  lm->mm_open_table       = linuxdvb_mux_open_table;
  lm->mm_close_table      = linuxdvb_mux_close_table;
  lm->mm_create_instances = linuxdvb_mux_create_instances;

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

  /* Tuning info */
  if ((e = htsmsg_get_map(conf, "tuning")))
    (void)dvb_mux_conf_load(ln->ln_type, &lm->lm_tuning, e);

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
