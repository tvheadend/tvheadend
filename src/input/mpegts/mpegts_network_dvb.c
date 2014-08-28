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
#include "queue.h"
#include "settings.h"
#include "mpegts_dvb.h"
#include "linuxdvb/linuxdvb_private.h"
#include "dvb_charset.h"
#include "scanfile.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>

static mpegts_mux_t *
dvb_network_find_mux
  ( dvb_network_t *ln, dvb_mux_conf_t *dmc, uint16_t onid, uint16_t tsid );

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

extern const idclass_t mpegts_network_class;

static void
dvb_network_class_delete ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;

  /* remove config */
  hts_settings_remove("input/dvb/networks/%s", 
                      idnode_uuid_as_str(in));

  /* Parent delete */
  mpegts_network_delete(mn, 1);
}

static const void *
dvb_network_class_scanfile_get ( void *o )
{
  static const char *s = NULL;
  return &s;
}
static int
dvb_network_class_scanfile_set ( void *o, const void *s )
{
  dvb_network_t *ln = o;
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
    if (!(mm = dvb_network_find_mux(ln, dmc, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE))) {
      mm = (mpegts_mux_t*)dvb_mux_create0(o,
                                          MPEGTS_ONID_NONE,
                                          MPEGTS_TSID_NONE,
                                          dmc, NULL, NULL);
      if (mm)
        mm->mm_config_save(mm);
#if ENABLE_TRACE
      char buf[128];
      dvb_mux_conf_str(dmc, buf, sizeof(buf));
      tvhtrace("scanfile", "mux %p %s added to network %s", mm, buf, ln->mn_network_name);
#endif
    } else {
#if ENABLE_TRACE
      char buf[128];
      dvb_mux_conf_str(dmc, buf, sizeof(buf));
      tvhtrace("scanfile", "mux %p skipped %s in network %s", mm, buf, ln->mn_network_name);
      dvb_mux_conf_str(&((dvb_mux_t *)mm)->lm_tuning, buf, sizeof(buf));
      tvhtrace("scanfile", "mux %p exists %s in network %s", mm, buf, ln->mn_network_name);
#endif
    }
  }
  return 0;
}
static htsmsg_t *
dvb_network_class_scanfile_list ( const char *type )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type", "api");
  htsmsg_add_str(m, "uri", "dvb/scanfile/list");
  e = htsmsg_create_map();
  htsmsg_add_str(e, "type", type);
  htsmsg_add_msg(m, "params", e);
  return m;
}
static htsmsg_t *
dvb_network_dvbt_class_scanfile_list ( void *o )
{
  return dvb_network_class_scanfile_list("dvbt");
}
static htsmsg_t *
dvb_network_dvbc_class_scanfile_list ( void *o )
{
  return dvb_network_class_scanfile_list("dvbc");
}
static htsmsg_t *
dvb_network_dvbs_class_scanfile_list ( void *o )
{
  return dvb_network_class_scanfile_list("dvbs");
}
static htsmsg_t *
dvb_network_atsc_class_scanfile_list ( void *o )
{
  return dvb_network_class_scanfile_list("atsc");
}

const idclass_t dvb_network_class =
{
  .ic_super      = &mpegts_network_class,
  .ic_class      = "dvb_network",
  .ic_caption    = "LinuxDVB Network",
  .ic_delete     = dvb_network_class_delete,
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t dvb_network_dvbt_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_dvbt",
  .ic_caption    = "DVB-T Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_dvbt_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t dvb_network_dvbc_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_dvbc",
  .ic_caption    = "DVB-C Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_dvbc_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t dvb_network_dvbs_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_dvbs",
  .ic_caption    = "DVB-S Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_dvbs_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t dvb_network_atsc_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_atsc",
  .ic_caption    = "ATSC Network",
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = "Pre-defined Muxes",
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_atsc_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

/* ****************************************************************************
 * Class methods
 * ***************************************************************************/

static int
dvb_network_check_bandwidth( int bw1, int bw2 )
{
  if (bw1 == DVB_BANDWIDTH_NONE || bw1 == DVB_BANDWIDTH_AUTO ||
      bw2 == DVB_BANDWIDTH_NONE || bw2 == DVB_BANDWIDTH_AUTO)
    return 0;
  return bw1 != bw2;
}

static int
dvb_network_check_symbol_rate( dvb_mux_t *lm, dvb_mux_conf_t *dmc, int deltar )
{
  switch (dmc->dmc_fe_type) {
  case DVB_TYPE_T:
    return dvb_network_check_bandwidth(lm->lm_tuning.u.dmc_fe_ofdm.bandwidth,
                                       dmc->u.dmc_fe_ofdm.bandwidth);
  case DVB_TYPE_C:
    return abs(lm->lm_tuning.u.dmc_fe_qam.symbol_rate -
               dmc->u.dmc_fe_qam.symbol_rate) > deltar;
  case DVB_TYPE_S:
    return abs(lm->lm_tuning.u.dmc_fe_qpsk.symbol_rate -
               dmc->u.dmc_fe_qpsk.symbol_rate) > deltar;
  case DVB_TYPE_ATSC:
    return 0;
  default:
    return 0;
  }
}

static int
dvb_network_check_orbital_pos ( dvb_mux_t *lm, dvb_mux_conf_t *dmc )
{
  if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir &&
      dmc->u.dmc_fe_qpsk.orbital_dir) {
    if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir !=
                 dmc->u.dmc_fe_qpsk.orbital_dir)
      return 1;
    /* 1W and 0.8W */
    if (abs(lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos -
                     dmc->u.dmc_fe_qpsk.orbital_pos) > 2)
      return 1;
  }
  return 0;
}

static mpegts_mux_t *
dvb_network_find_mux
  ( dvb_network_t *ln, dvb_mux_conf_t *dmc, uint16_t onid, uint16_t tsid )
{
  int deltaf, deltar;
  mpegts_mux_t *mm, *mm_alt = NULL;

  LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link) {
    deltaf = 2000; // 2K/MHz
    deltar = 1000;
    dvb_mux_t *lm = (dvb_mux_t*)mm;

    /* Same FE type - this REALLY should match! */
    if (lm->lm_tuning.dmc_fe_type != dmc->dmc_fe_type) continue;

    /* if ONID/TSID are a perfect match (and this is DVB-S, allow greater deltaf) */
    if (lm->lm_tuning.dmc_fe_type == DVB_TYPE_S) {
      deltar = 10000;
      if (onid != MPEGTS_ONID_NONE && tsid != MPEGTS_TSID_NONE)
        deltaf = 16000; // This is slightly crazy, but I have seen 10MHz changes in freq
                        // and remember the ONID and TSID must agree
      else
        deltaf = 4000;
    }

    /* Reject if not same frequency (some tolerance due to changes and diff in NIT) */
    if (abs(lm->lm_tuning.dmc_fe_freq - dmc->dmc_fe_freq) > deltaf) continue;

    /* Reject if not same symbol rate (some tolerance due to changes and diff in NIT) */
    if (dvb_network_check_symbol_rate(lm, dmc, deltar)) continue;

    /* DVB-S extra checks */
    if (lm->lm_tuning.dmc_fe_type == DVB_TYPE_S) {

      /* Same polarisation */
      if (lm->lm_tuning.u.dmc_fe_qpsk.polarisation != dmc->u.dmc_fe_qpsk.polarisation) continue;

      /* Same orbital position */
      if (dvb_network_check_orbital_pos(lm, dmc)) continue;
    }

    mm_alt = mm;

    /* Reject if not same ID */
    if (onid != MPEGTS_ONID_NONE && mm->mm_onid != MPEGTS_ONID_NONE && mm->mm_onid != onid) continue;
    if (tsid != MPEGTS_TSID_NONE && mm->mm_tsid != MPEGTS_TSID_NONE && mm->mm_tsid != tsid) continue;

    break;
  }
  if (!mm && onid != MPEGTS_ONID_NONE && tsid != MPEGTS_TSID_NONE) {
    /* use the mux with closest parameters */
    /* unfortunately, the onid and tsid might DIFFER */
    /* in the NIT table information and real mux feed */
    mm = mm_alt;
  }
  return mm;
}

static void
dvb_network_config_save ( mpegts_network_t *mn )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&mn->mn_id, c);
  htsmsg_add_str(c, "class", mn->mn_id.in_class->ic_class);
  hts_settings_save(c, "input/dvb/networks/%s/config",
                    idnode_uuid_as_str(&mn->mn_id));
  htsmsg_destroy(c);
}

static const idclass_t *
dvb_network_mux_class
  ( mpegts_network_t *mn )
{
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbt_class))
    return &dvb_mux_dvbt_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbc_class))
    return &dvb_mux_dvbc_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class))
    return &dvb_mux_dvbs_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_atsc_class))
    return &dvb_mux_atsc_class;
  return NULL;
}

static mpegts_mux_t *
dvb_network_create_mux
  ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid, void *p )
{
  int save = 0;
  mpegts_mux_t *mmo = mm;
  dvb_network_t *ln = (dvb_network_t*)mm->mm_network;
  dvb_mux_conf_t *dmc = p;

  mm = dvb_network_find_mux(ln, dmc, onid, tsid);
  if (!mm && ln->mn_autodiscovery) {
    const idclass_t *cls;
    cls = dvb_network_mux_class((mpegts_network_t *)ln);
    save |= cls == &dvb_mux_dvbt_class && dmc->dmc_fe_type == DVB_TYPE_T;
    save |= cls == &dvb_mux_dvbc_class && dmc->dmc_fe_type == DVB_TYPE_C;
    save |= cls == &dvb_mux_dvbs_class && dmc->dmc_fe_type == DVB_TYPE_S;
    save |= cls == &dvb_mux_atsc_class && dmc->dmc_fe_type == DVB_TYPE_ATSC;
    if (save && dmc->dmc_fe_type == DVB_TYPE_S) {
      mpegts_mux_t *mm2;
      dvb_mux_t *lm;
      LIST_FOREACH(mm2, &ln->mn_muxes, mm_network_link) {
       lm = (dvb_mux_t *)mm2;
       if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir)
         break;
      }
      /* do not allow to mix sattelite positions */
      if (mm2 && dvb_network_check_orbital_pos(lm, dmc))
        save = 0;
    }
    if (save) {
      mm = (mpegts_mux_t*)dvb_mux_create0(ln, onid, tsid, dmc, NULL, NULL);
#if ENABLE_TRACE
      char buf[128];
      dvb_mux_conf_str(&((dvb_mux_t *)mm)->lm_tuning, buf, sizeof(buf));
      tvhtrace("mpegts", "mux %p %s onid %i tsid %i added to network %s (autodiscovery)",
               mm, buf, onid, tsid, mm->mm_network->mn_network_name);
#endif      
    }
  } else if (mm) {
    dvb_mux_t *lm = (dvb_mux_t*)mm;
    /* the nit tables may be inconsistent (like rolloff ping-pong) */
    /* accept information only from one origin mux */
    if (mm->mm_dmc_origin_expire > dispatch_clock && mm->mm_dmc_origin && mm->mm_dmc_origin != mmo)
      goto noop;
#if ENABLE_TRACE
    #define COMPARE(x) ({ \
      int xr = dmc->x != lm->lm_tuning.x; \
      if (xr) { \
        tvhtrace("mpegts", "create mux dmc->" #x " (%li) != lm->lm_tuning." #x \
                 " (%li)", (long)dmc->x, (long)lm->lm_tuning.x); \
        lm->lm_tuning.x = dmc->x; \
      } xr; })
    #define COMPAREN(x) ({ \
      int xr = dmc->x != 0 && dmc->x != 1 && dmc->x != lm->lm_tuning.x; \
      if (xr) { \
        tvhtrace("mpegts", "create mux dmc->" #x " (%li) != lm->lm_tuning." #x \
                 " (%li)", (long)dmc->x, (long)lm->lm_tuning.x); \
        lm->lm_tuning.x = dmc->x; \
      } xr; })
#else
    #define COMPARE(x) ({ \
      int xr = dmc->x != lm->lm_tuning.x; \
      if (xr) lm->lm_tuning.x = dmc->x; \
      xr; })
    /* note - zero means NONE, one means AUTO */
    #define COMPAREN(x) ({ \
      int xr = dmc->x != 0 && dmc->x != 1 && dmc->x != lm->lm_tuning.x; \
      if (xr) lm->lm_tuning.x = dmc->x; \
      xr; })
#endif
#if ENABLE_TRACE
    dvb_mux_conf_t tuning_old;
    char buf[128];
    tuning_old = lm->lm_tuning;
#endif
    /* Handle big diffs that have been allowed through for DVB-S */
    if (abs(dmc->dmc_fe_freq - lm->lm_tuning.dmc_fe_freq) > 4000) {
      lm->lm_tuning.dmc_fe_freq = dmc->dmc_fe_freq;
      save = 1;
    }
    save |= COMPAREN(dmc_fe_delsys);
    save |= COMPAREN(dmc_fe_modulation);
    save |= COMPAREN(dmc_fe_inversion);
    save |= COMPAREN(dmc_fe_rolloff);
    save |= COMPAREN(dmc_fe_pilot);
    switch (dmc->dmc_fe_type) {
    case DVB_TYPE_T:
      save |= COMPAREN(u.dmc_fe_ofdm.bandwidth);
      save |= COMPAREN(u.dmc_fe_ofdm.code_rate_HP);
      save |= COMPAREN(u.dmc_fe_ofdm.code_rate_LP);
      save |= COMPAREN(u.dmc_fe_ofdm.transmission_mode);
      save |= COMPAREN(u.dmc_fe_ofdm.guard_interval);
      save |= COMPAREN(u.dmc_fe_ofdm.hierarchy_information);
      break;
    case DVB_TYPE_S:
      save |= COMPARE(u.dmc_fe_qpsk.polarisation);
      if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir == 0 ||
          dvb_network_check_orbital_pos(lm, dmc))
        save |= COMPARE(u.dmc_fe_qpsk.orbital_pos);
      save |= COMPARE(u.dmc_fe_qpsk.orbital_dir);
      save |= COMPARE(u.dmc_fe_qpsk.symbol_rate);
      save |= COMPAREN(u.dmc_fe_qpsk.fec_inner);
      break;
    case DVB_TYPE_C:
      save |= COMPARE(u.dmc_fe_qam.symbol_rate);
      save |= COMPAREN(u.dmc_fe_qam.fec_inner);
      break;
    default:
      abort();
    }
    #undef COMPARE
    #undef COMPAREN
#if ENABLE_TRACE
    if (save) {
      dvb_mux_conf_str(&tuning_old, buf, sizeof(buf));
      tvhtrace("mpegts", "mux %p changed from %s in network %s",
               mm, buf, mm->mm_network->mn_network_name);
      dvb_mux_conf_str(&lm->lm_tuning, buf, sizeof(buf));
      tvhtrace("mpegts", "mux %p changed to %s in network %s",
               mm, buf, mm->mm_network->mn_network_name);
    }
#endif
  }
  if (mm) {
    mm->mm_dmc_origin        = mmo;
    mm->mm_dmc_origin_expire = dispatch_clock + 3600 * 24; /* one day */
    if (save)
      mm->mm_config_save(mm);
  }
noop:
  return mm;
}

static mpegts_service_t *
dvb_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return mpegts_service_create1(NULL, mm, sid, pmt_pid, NULL);
}

static mpegts_mux_t *
dvb_network_mux_create2
  ( mpegts_network_t *mn, htsmsg_t *conf )
{
  dvb_network_t *ln = (dvb_network_t*)mn;
  return (mpegts_mux_t*)
    dvb_mux_create0(ln, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,
                    NULL, NULL, conf);
}

/* ****************************************************************************
 * Creation/Config
 * ***************************************************************************/

dvb_network_t *
dvb_network_create0
  ( const char *uuid, const idclass_t *idc, htsmsg_t *conf )
{
  dvb_network_t *ln;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  ln = calloc(1, sizeof(dvb_network_t));
  if (idc == &dvb_network_dvbt_class)
    ln->ln_type = DVB_TYPE_T;
  else if (idc == &dvb_network_dvbc_class)
    ln->ln_type = DVB_TYPE_C;
  else if (idc == &dvb_network_dvbs_class)
    ln->ln_type = DVB_TYPE_S;
  else
    ln->ln_type = DVB_TYPE_ATSC;

  /* Create */
  if (!(ln = (dvb_network_t*)mpegts_network_create0((void*)ln,
                                     idc, uuid, NULL, conf)))
    return NULL;
  
  /* Callbacks */
  ln->mn_create_mux     = dvb_network_create_mux;
  ln->mn_create_service = dvb_network_create_service;
  ln->mn_config_save    = dvb_network_config_save;
  ln->mn_mux_class      = dvb_network_mux_class;
  ln->mn_mux_create2    = dvb_network_mux_create2;

  /* No config */
  if (!conf)
    return ln;

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/dvb/networks/%s/muxes", uuid))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      (void)dvb_mux_create1(ln, f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }

  return ln;
}

static mpegts_network_t *
dvb_network_builder
  ( const idclass_t *idc, htsmsg_t *conf )
{
  return (mpegts_network_t*)dvb_network_create0(NULL, idc, conf);
}

static  const idclass_t* dvb_network_classes[] = {
  &dvb_network_dvbt_class,
  &dvb_network_dvbc_class,
  &dvb_network_dvbs_class,
  &dvb_network_atsc_class,
};

void dvb_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  const char *s;
  int i, move = 0;

  /* Load scan files */
  scanfile_init();

  /* Load list of mux charset global overrides */
  dvb_charset_init();

  /* Register class builders */
  for (i = 0; i < ARRAY_SIZE(dvb_network_classes); i++)
    mpegts_network_register_builder(dvb_network_classes[i],
                                    dvb_network_builder);
  
  /* Load settings */
  if (!(c = hts_settings_load_r(1, "input/dvb/networks")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_get_map_by_field(f)))  continue;
    if (!(e = htsmsg_get_map(e, "config"))) continue;
    if (!(s = htsmsg_get_str(e, "class")))  continue;
    for (i = 0; i < ARRAY_SIZE(dvb_network_classes); i++) {
      if(!strcmp(dvb_network_classes[i]->ic_class, s + (move ? 5 : 0))) {
        dvb_network_create0(f->hmf_name, dvb_network_classes[i], e);
        break;
      }
    }
  }
  htsmsg_destroy(c);
}

void dvb_network_done ( void )
{
  int i;

  pthread_mutex_lock(&global_lock);
  /* Unregister class builders */
  for (i = 0; i < ARRAY_SIZE(dvb_network_classes); i++) {
    mpegts_network_unregister_builder(dvb_network_classes[i]);
    mpegts_network_class_delete(dvb_network_classes[i], 0);
  }
  pthread_mutex_unlock(&global_lock);

  dvb_charset_done();
  scanfile_done();
}

/* ****************************************************************************
 * Search
 * ***************************************************************************/

dvb_network_t*
dvb_network_find_by_uuid(const char *uuid)
{
  idnode_t *in = idnode_find(uuid, &dvb_network_class);
  return (dvb_network_t*)in;
}

int dvb_network_get_orbital_pos
  ( mpegts_network_t *mn, int *pos, char *dir )
{
  dvb_network_t *ln = (dvb_network_t *)mn;
  mpegts_mux_t  *mm;
  dvb_mux_t     *lm = NULL;

  LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link) {
    lm = (dvb_mux_t *)mm;
    if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir)
      break;
  }
  if (mm) {
    *pos = lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos;
    *dir = lm->lm_tuning.u.dmc_fe_qpsk.orbital_dir;
    return 0;
  } else {
    return -1;
  }
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
