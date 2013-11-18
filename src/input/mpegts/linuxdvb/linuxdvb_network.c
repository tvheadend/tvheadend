/*
 *  Tvheadend - Linux DVB Network
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
#include "scanfile.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

static mpegts_mux_t *
linuxdvb_network_find_mux
  ( linuxdvb_network_t *ln, dvb_mux_conf_t *dmc );

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

extern const idclass_t mpegts_network_class;

static void
linuxdvb_network_class_delete ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;

  /* remove config */
  hts_settings_remove("input/linuxdvb/networks/%s", 
                      idnode_uuid_as_str(in));

  /* Parent delete */
  mpegts_network_delete(mn);
}

static const void *
linuxdvb_network_class_scanfile_get ( void *o )
{
  static const char *s = NULL;
  return &s;
}
static int
linuxdvb_network_class_scanfile_set ( void *o, const void *s )
{
  dvb_mux_conf_t *dmc;
  scanfile_network_t *sfn;
  mpegts_mux_t *mm;

  /* Find */
  if (!s)
    return 0;
  if (!(sfn = scanfile_find(s)))
    return 0;
  
  /* Create */
  LIST_FOREACH(dmc, &sfn->sfn_muxes, dmc_link) {
    if (!(mm = linuxdvb_network_find_mux(o, dmc))) {
      mm = (mpegts_mux_t*)linuxdvb_mux_create0(o,
                                               MPEGTS_ONID_NONE,
                                               MPEGTS_TSID_NONE,
                                               dmc, NULL, NULL);
      if (mm)
        mm->mm_config_save(mm);
    }
  }
  return 0;
}
static htsmsg_t *
linuxdvb_network_class_scanfile_list ( const char *type )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type", "api");
  htsmsg_add_str(m, "uri", "linuxdvb/scanfile/list");
  e = htsmsg_create_map();
  htsmsg_add_str(e, "type", type);
  htsmsg_add_msg(m, "params", e);
  return m;
}
static htsmsg_t *
linuxdvb_network_dvbt_class_scanfile_list ( void *o )
{
  return linuxdvb_network_class_scanfile_list("dvbt");
}
static htsmsg_t *
linuxdvb_network_dvbc_class_scanfile_list ( void *o )
{
  return linuxdvb_network_class_scanfile_list("dvbc");
}
static htsmsg_t *
linuxdvb_network_dvbs_class_scanfile_list ( void *o )
{
  return linuxdvb_network_class_scanfile_list("dvbs");
}
static htsmsg_t *
linuxdvb_network_atsc_class_scanfile_list ( void *o )
{
  return linuxdvb_network_class_scanfile_list("atsc");
}

const idclass_t linuxdvb_network_class =
{
  .ic_super      = &mpegts_network_class,
  .ic_class      = "linuxdvb_network",
  .ic_caption    = "LinuxDVB Network",
  .ic_delete     = linuxdvb_network_class_delete,
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_network_dvbt_class =
{
  .ic_super      = &linuxdvb_network_class,
  .ic_class      = "linuxdvb_network_dvbt",
  .ic_caption    = "DVB-T Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = linuxdvb_network_class_scanfile_set,
      .get      = linuxdvb_network_class_scanfile_get,
      .list     = linuxdvb_network_dvbt_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t linuxdvb_network_dvbc_class =
{
  .ic_super      = &linuxdvb_network_class,
  .ic_class      = "linuxdvb_network_dvbc",
  .ic_caption    = "DVB-C Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = linuxdvb_network_class_scanfile_set,
      .get      = linuxdvb_network_class_scanfile_get,
      .list     = linuxdvb_network_dvbc_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t linuxdvb_network_dvbs_class =
{
  .ic_super      = &linuxdvb_network_class,
  .ic_class      = "linuxdvb_network_dvbs",
  .ic_caption    = "DVB-S Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = linuxdvb_network_class_scanfile_set,
      .get      = linuxdvb_network_class_scanfile_get,
      .list     = linuxdvb_network_dvbs_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t linuxdvb_network_atsc_class =
{
  .ic_super      = &linuxdvb_network_class,
  .ic_class      = "linuxdvb_network_atsc",
  .ic_caption    = "ATSC Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = linuxdvb_network_class_scanfile_set,
      .get      = linuxdvb_network_class_scanfile_get,
      .list     = linuxdvb_network_atsc_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/* ****************************************************************************
 * Class methods
 * ***************************************************************************/

static mpegts_mux_t *
linuxdvb_network_find_mux
  ( linuxdvb_network_t *ln, dvb_mux_conf_t *dmc )
{
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link) {
    linuxdvb_mux_t *lm = (linuxdvb_mux_t*)mm;
    if (abs(lm->lm_tuning.dmc_fe_params.frequency
            - dmc->dmc_fe_params.frequency) > LINUXDVB_FREQ_TOL) continue;
    if (lm->lm_tuning.dmc_fe_polarisation != dmc->dmc_fe_polarisation) continue;
    break;
  }
  return mm;
}

static void
linuxdvb_network_config_save ( mpegts_network_t *mn )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&mn->mn_id, c);
  htsmsg_add_str(c, "class", mn->mn_id.in_class->ic_class);
  hts_settings_save(c, "input/linuxdvb/networks/%s/config",
                    idnode_uuid_as_str(&mn->mn_id));
  htsmsg_destroy(c);
}

static mpegts_mux_t *
linuxdvb_network_create_mux
  ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid, dvb_mux_conf_t *dmc )
{
  int save = 0;
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mm->mm_network;
  mm = linuxdvb_network_find_mux(ln, dmc);
  if (!mm && ln->mn_autodiscovery) {
    mm   = (mpegts_mux_t*)linuxdvb_mux_create0(ln, onid, tsid, dmc, NULL, NULL);
    save = 1;
  } else if (mm) {
    linuxdvb_mux_t *lm = (linuxdvb_mux_t*)mm;
    dmc->dmc_fe_params.frequency = lm->lm_tuning.dmc_fe_params.frequency;
    // Note: keep original freq, else it can bounce around if diff transponders
    // report it slightly differently.
    // TODO: Note: should we also leave AUTO settings as is?
    if (memcmp(&lm->lm_tuning, dmc, sizeof(lm->lm_tuning))) {
      memcpy(&lm->lm_tuning, dmc, sizeof(lm->lm_tuning));
      save = 1;
    }
  }
  if (save)
      mm->mm_config_save(mm);
  return mm;
}

static mpegts_service_t *
linuxdvb_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return linuxdvb_service_create0((linuxdvb_mux_t*)mm, sid,
                                  pmt_pid, NULL, NULL);
}

static const idclass_t *
linuxdvb_network_mux_class
  ( mpegts_network_t *mn )
{
  extern const idclass_t linuxdvb_mux_dvbt_class;
  extern const idclass_t linuxdvb_mux_dvbc_class;
  extern const idclass_t linuxdvb_mux_dvbs_class;
  extern const idclass_t linuxdvb_mux_atsc_class;
  if (idnode_is_instance(&mn->mn_id, &linuxdvb_network_dvbt_class))
    return &linuxdvb_mux_dvbt_class;
  if (idnode_is_instance(&mn->mn_id, &linuxdvb_network_dvbc_class))
    return &linuxdvb_mux_dvbc_class;
  if (idnode_is_instance(&mn->mn_id, &linuxdvb_network_dvbs_class))
    return &linuxdvb_mux_dvbs_class;
  if (idnode_is_instance(&mn->mn_id, &linuxdvb_network_atsc_class))
    return &linuxdvb_mux_atsc_class;
  return NULL;
}

static mpegts_mux_t *
linuxdvb_network_mux_create2
  ( mpegts_network_t *mn, htsmsg_t *conf )
{
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mn;
  return (mpegts_mux_t*)
    linuxdvb_mux_create0(ln, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,
                         NULL, NULL, conf);
}

/* ****************************************************************************
 * Creation/Config
 * ***************************************************************************/

linuxdvb_network_t *
linuxdvb_network_create0
  ( const char *uuid, const idclass_t *idc, htsmsg_t *conf )
{
  linuxdvb_network_t *ln;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  ln = calloc(1, sizeof(linuxdvb_network_t));
  if (idc == &linuxdvb_network_dvbt_class)
    ln->ln_type = FE_OFDM;
  else if (idc == &linuxdvb_network_dvbc_class)
    ln->ln_type = FE_QAM;
  else if (idc == &linuxdvb_network_dvbs_class)
    ln->ln_type = FE_QPSK;
  else
    ln->ln_type = FE_ATSC;

  /* Create */
  if (!(ln = (linuxdvb_network_t*)mpegts_network_create0((void*)ln,
                                     idc, uuid, NULL, conf)))
    return NULL;
  
  /* Callbacks */
  ln->mn_create_mux     = linuxdvb_network_create_mux;
  ln->mn_create_service = linuxdvb_network_create_service;
  ln->mn_config_save    = linuxdvb_network_config_save;
  ln->mn_mux_class      = linuxdvb_network_mux_class;
  ln->mn_mux_create2    = linuxdvb_network_mux_create2;

  /* No config */
  if (!conf)
    return ln;

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/linuxdvb/networks/%s/muxes", uuid))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      (void)linuxdvb_mux_create1(ln, f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }

  return ln;
}

static mpegts_network_t *
linuxdvb_network_builder
  ( const idclass_t *idc, htsmsg_t *conf )
{
  return (mpegts_network_t*)linuxdvb_network_create0(NULL, idc, conf);
}

void linuxdvb_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  const char *s;
  int i;

  const idclass_t* classes[] = {
    &linuxdvb_network_dvbt_class,
    &linuxdvb_network_dvbc_class,
    &linuxdvb_network_dvbs_class,
    &linuxdvb_network_atsc_class,
  };
  
  /* Register class builders */
  for (i = 0; i < ARRAY_SIZE(classes); i++)
    mpegts_network_register_builder(classes[i], linuxdvb_network_builder);
  
  /* Load settings */
  if (!(c = hts_settings_load_r(1, "input/linuxdvb/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    if (!(s = htsmsg_get_str(e, "class")))  continue;
    for (i = 0; i < ARRAY_SIZE(classes); i++) {
      if(!strcmp(classes[i]->ic_class, s)) {
        (void)linuxdvb_network_create0(f->hmf_name, classes[i], e);
        break;
      }
    }
  }
  htsmsg_destroy(c);
}

/* ****************************************************************************
 * Search
 * ***************************************************************************/

linuxdvb_network_t*
linuxdvb_network_find_by_uuid(const char *uuid)
{
  idnode_t *in = idnode_find(uuid, &linuxdvb_network_class);
  return (linuxdvb_network_t*)in;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
