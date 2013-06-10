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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

extern const idclass_t mpegts_network_class;

static const char *
mpegts_network_class_get_lntype
  ( void * ptr )
{
  return dvb_type2str(((linuxdvb_network_t*)ptr)->ln_type);
}

static void
mpegts_network_class_set_lntype
  ( void *ptr, const char *str )
{ 
  ((linuxdvb_network_t*)ptr)->ln_type = dvb_str2type(str);
}

const idclass_t linuxdvb_network_class =
{
  .ic_super      = &mpegts_network_class,
  .ic_class      = "linuxdvb_network",
  .ic_caption    = "LinuxDVB Network",
  .ic_properties = (const property_t[]){
    { PROPDEF2("type", "Network Type",
               PT_STR, linuxdvb_network_t, ln_type, 1),
      .str_get = mpegts_network_class_get_lntype,
      .str_set = mpegts_network_class_set_lntype },
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
#define LINUXDVB_FREQ_TOL 2000 // TODO: fix this!
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
  hts_settings_save(c, "input/linuxdvb/networks/%s/config",
                    idnode_uuid_as_str(&mn->mn_id));
  htsmsg_destroy(c);
}

static mpegts_mux_t *
linuxdvb_network_create_mux
  ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid, dvb_mux_conf_t *dmc )
{
  // TODO: should we have a mux_find wrapper?
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mm->mm_network;
  mm = linuxdvb_network_find_mux(ln, dmc);
  if (!mm) {
    mm = (mpegts_mux_t*)linuxdvb_mux_create0(ln, onid, tsid, dmc, NULL, NULL);
    if (mm)
      mm->mm_config_save(mm);
  }
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
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mn;
  switch (ln->ln_type) {
    case FE_OFDM:
      return &linuxdvb_mux_dvbt_class;
    case FE_QAM:
      return &linuxdvb_mux_dvbc_class;
    case FE_QPSK:
      return &linuxdvb_mux_dvbs_class;
    case FE_ATSC:
      return &linuxdvb_mux_atsc_class;
    default:
      return NULL;
  }
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
  ( const char *uuid, fe_type_t type, htsmsg_t *conf )
{
  linuxdvb_network_t *ln;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Create */
  if (!(ln = mpegts_network_create(linuxdvb_network, uuid, NULL, conf)))
    return NULL;
  if (type != -1)
    ln->ln_type = type;
  
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
  }

  return ln;
}

void linuxdvb_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if (!(c = hts_settings_load_r(1, "input/linuxdvb/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    (void)linuxdvb_network_create0(f->hmf_name, -1, e);
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
