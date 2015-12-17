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

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

extern const idclass_t mpegts_network_class;

static void
dvb_network_class_delete ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;
  char ubuf[UUID_HEX_SIZE];

  /* remove config */
  hts_settings_remove("input/dvb/networks/%s", 
                      idnode_uuid_as_str(in, ubuf));

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
  dvb_mux_t *mm;

  /* Find */
  if (!s)
    return 0;
  if (!(sfn = scanfile_find(s)))
    return 0;

  /* Set satellite position */
  if (sfn->sfn_satpos != INT_MAX && ln->mn_satpos == INT_MAX)
    ln->mn_satpos = sfn->sfn_satpos;

  /* Create */
  LIST_FOREACH(dmc, &sfn->sfn_muxes, dmc_link) {
    if (!(mm = dvb_network_find_mux(ln, dmc, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE))) {
      mm = dvb_mux_create0(o, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE,
                           dmc, NULL, NULL);
      if (mm)
        mm->mm_config_save((mpegts_mux_t *)mm);
      if (tvhtrace_enabled()) {
        char buf[128];
        dvb_mux_conf_str(dmc, buf, sizeof(buf));
        tvhtrace("scanfile", "mux %p %s added to network %s", mm, buf, ln->mn_network_name);
      }
    } else {
      if (tvhtrace_enabled()) {
        char buf[128];
        dvb_mux_conf_str(dmc, buf, sizeof(buf));
        tvhtrace("scanfile", "mux %p skipped %s in network %s", mm, buf, ln->mn_network_name);
        dvb_mux_conf_str(&((dvb_mux_t *)mm)->lm_tuning, buf, sizeof(buf));
        tvhtrace("scanfile", "mux %p exists %s in network %s", mm, buf, ln->mn_network_name);
      }
    }
  }
  return 0;
}
static htsmsg_t *
dvb_network_class_scanfile_list ( void *o, const char *lang, const char *type )
{
  dvb_network_t *ln = o;
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type", "api");
  htsmsg_add_str(m, "uri", "dvb/scanfile/list");
  htsmsg_add_str(m, "stype", "none");
  e = htsmsg_create_map();
  htsmsg_add_str(e, "type", type);
  if (ln && ln->mn_satpos != INT_MAX)
    htsmsg_add_s32(e, "satpos", ln->mn_satpos);
  htsmsg_add_msg(m, "params", e);
  return m;
}

static htsmsg_t *
dvb_network_dvbt_class_scanfile_list ( void *o, const char *lang )
{
  return dvb_network_class_scanfile_list(o, lang, "dvbt");
}
static htsmsg_t *
dvb_network_dvbc_class_scanfile_list ( void *o, const char *lang )
{
  return dvb_network_class_scanfile_list(o, lang, "dvbc");
}
static htsmsg_t *
dvb_network_dvbs_class_scanfile_list ( void *o, const char *lang )
{
  return dvb_network_class_scanfile_list(o, lang, "dvbs");
}
static htsmsg_t *
dvb_network_atsc_t_class_scanfile_list ( void *o, const char *lang )
{
  return dvb_network_class_scanfile_list(o, lang, "atsc-t");
}

static htsmsg_t *
dvb_network_atsc_c_class_scanfile_list ( void *o, const char *lang )
{
  return dvb_network_class_scanfile_list(o, lang, "atsc-c");
}

static const void *
dvb_network_class_orbital_pos_get ( void *o )
{
  dvb_network_t *ln = o;
  static char buf[16];
  static const char *s;
  s = NULL;
  if (ln->mn_satpos != INT_MAX) {
    dvb_sat_position_to_str(ln->mn_satpos, buf, sizeof(buf));
    s = buf;
  } else
    s = "";
  return &s;
}

static int
dvb_network_class_orbital_pos_set ( void *o, const void *s )
{
  dvb_network_t *ln = o;
  int satpos;

  /* Find */
  if (!s)
    return 0;

  satpos = dvb_sat_position_from_str(s);
  if (satpos != ln->mn_satpos) {
    ln->mn_satpos = satpos;
    return 1;
  }

  return 0;
}

static htsmsg_t *
dvb_network_class_orbital_pos_list ( void *o, const char *lang )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type", "api");
  htsmsg_add_str(m, "uri", "dvb/orbitalpos/list");
  htsmsg_add_str(m, "stype", "none");
  e = htsmsg_create_map();
  htsmsg_add_msg(m, "params", e);
  return m;
}

const idclass_t dvb_network_class =
{
  .ic_super      = &mpegts_network_class,
  .ic_class      = "dvb_network",
  .ic_caption    = N_("LinuxDVB network"),
  .ic_delete     = dvb_network_class_delete,
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t dvb_network_dvbt_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_dvbt",
  .ic_caption    = N_("DVB-T network"),
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = N_("Pre-defined muxes"),
      .desc     = N_("Use a pre-defined list of DVB-T muxes. "
                     "Note: these lists can sometimes be outdated and "
                     "may cause scanning to take longer than usual."),
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
  .ic_caption    = N_("DVB-C network"),
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = N_("Pre-defined muxes"),
      .desc     = N_("Use a pre-defined list of DVB-C muxes. "
                     "Note: these lists can sometimes be outdated and "
                     "may cause scanning to take longer than usual."),
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
  .ic_caption    = N_("DVB-S network"),
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = N_("Pre-defined muxes"),
      .desc     = N_("Use a pre-defined list of DVB-S/S2 muxes. "
                     "Note: these lists can sometimes be outdated and "
                     "may cause scanning to take longer than usual."),
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_dvbs_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "orbital_pos",
      .name     = N_("Orbital position"),
      .desc     = N_("Select the orbital position of the satellite "
                     "your dish is pointing at."),
      .set      = dvb_network_class_orbital_pos_set,
      .get      = dvb_network_class_orbital_pos_get,
      .list     = dvb_network_class_orbital_pos_list,
    },
    {}
  }
};

const idclass_t dvb_network_atsc_t_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_atsc_t",
  .ic_caption    = N_("ATSC-T network"),
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = N_("Pre-defined muxes"),
      .desc     = N_("Use a pre-defined list of ATSC-T muxes. "
                     "Note: these lists can sometimes be outdated and "
                     "may cause scanning to take longer than usual."),
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_atsc_t_class_scanfile_list,
      .opts     = PO_NOSAVE,
    },
    {}
  }
};

const idclass_t dvb_network_atsc_c_class =
{
  .ic_super      = &dvb_network_class,
  .ic_class      = "dvb_network_atsc_c",
  .ic_caption    = N_("ATSC-C network"),
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "scanfile",
      .name     = N_("Pre-defined muxes"),
      .desc     = N_("Use a pre-defined list of ATSC-C muxes. "
                     "Note: these lists can sometimes be outdated and "
                     "may cause scanning to take longer than usual."),
      .set      = dvb_network_class_scanfile_set,
      .get      = dvb_network_class_scanfile_get,
      .list     = dvb_network_atsc_c_class_scanfile_list,
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
  case DVB_TYPE_ATSC_C:
    return deltaU32(lm->lm_tuning.u.dmc_fe_qam.symbol_rate,
               dmc->u.dmc_fe_qam.symbol_rate) > deltar;
  case DVB_TYPE_S:
    return deltaU32(lm->lm_tuning.u.dmc_fe_qpsk.symbol_rate,
               dmc->u.dmc_fe_qpsk.symbol_rate) > deltar;
  case DVB_TYPE_ATSC_T:
    return 0;
  default:
    return 0;
  }
}

static int
dvb_network_check_orbital_pos ( int satpos1, int satpos2 )
{
  if (satpos1 != INT_MAX && satpos2 != INT_MAX) {
    /* 1W and 0.8W */
    if (abs(satpos1 - satpos2) > 2)
      return 1;
  }
  return 0;
}

dvb_mux_t *
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

    /* Also, the system type should match (DVB-S/DVB-S2) */
    if (lm->lm_tuning.dmc_fe_delsys != dmc->dmc_fe_delsys) continue;

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
    if (deltaU32(lm->lm_tuning.dmc_fe_freq, dmc->dmc_fe_freq) > deltaf) continue;

    /* Reject if not same symbol rate (some tolerance due to changes and diff in NIT) */
    if (dvb_network_check_symbol_rate(lm, dmc, deltar)) continue;

    /* DVB-S extra checks */
    if (lm->lm_tuning.dmc_fe_type == DVB_TYPE_S) {

      /* Same polarisation */
      if (lm->lm_tuning.u.dmc_fe_qpsk.polarisation != dmc->u.dmc_fe_qpsk.polarisation) continue;

      /* Same orbital position */
      if (dvb_network_check_orbital_pos(lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos,
                                        dmc->u.dmc_fe_qpsk.orbital_pos)) continue;
    }

    /* Same PLP/ISI */
    if (lm->lm_tuning.dmc_fe_stream_id != dmc->dmc_fe_stream_id) continue;

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
  return (dvb_mux_t *)mm;
}

static void
dvb_network_config_save ( mpegts_network_t *mn )
{
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&mn->mn_id, c);
  htsmsg_add_str(c, "class", mn->mn_id.in_class->ic_class);
  hts_settings_save(c, "input/dvb/networks/%s/config",
                    idnode_uuid_as_str(&mn->mn_id, ubuf));
  htsmsg_destroy(c);
}

const idclass_t *
dvb_network_mux_class
  ( mpegts_network_t *mn )
{
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbt_class))
    return &dvb_mux_dvbt_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbc_class))
    return &dvb_mux_dvbc_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class))
    return &dvb_mux_dvbs_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_atsc_t_class))
    return &dvb_mux_atsc_t_class;
  if (idnode_is_instance(&mn->mn_id, &dvb_network_atsc_c_class))
    return &dvb_mux_atsc_c_class;
  return NULL;
}

#define CBIT_ORBITAL_POS        (1<<0)
#define CBIT_POLARISATION       (1<<1)
#define CBIT_FREQ               (1<<2)
#define CBIT_RATE               (1<<3)
#define CBIT_RATE_HP            (1<<4)
#define CBIT_RATE_LP            (1<<5)
#define CBIT_MODULATION         (1<<6)
#define CBIT_INVERSION          (1<<7)
#define CBIT_ROLLOFF            (1<<8)
#define CBIT_PILOT              (1<<9)
#define CBIT_STREAM_ID          (1<<10)
#define CBIT_BANDWIDTH          (1<<11)
#define CBIT_TRANS_MODE         (1<<12)
#define CBIT_GUARD              (1<<13)
#define CBIT_HIERARCHY          (1<<14)
#define CBIT_PLS_MODE           (1<<15)
#define CBIT_PLS_CODE           (1<<16)
#define CBIT_FEC_INNER          (1<<17)

static mpegts_mux_t *
dvb_network_create_mux
  ( mpegts_network_t *mn, void *origin, uint16_t onid, uint16_t tsid,
    void *p, int force )
{
  int save = 0, satpos;
  dvb_mux_t *mm;
  dvb_network_t *ln;
  dvb_mux_conf_t *dmc = p;
  const idclass_t *cls = dvb_network_mux_class(mn);

  /* when forced - try to look also to another DVB-S networks */
  if (force && cls == &dvb_mux_dvbs_class && dmc->dmc_fe_type == DVB_TYPE_S &&
      dmc->u.dmc_fe_qpsk.orbital_pos != INT_MAX) {
    satpos = dvb_network_get_orbital_pos(mn);
    if (dvb_network_check_orbital_pos(satpos, dmc->u.dmc_fe_qpsk.orbital_pos)) {
      LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
        if (!idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class)) continue;
        satpos = dvb_network_get_orbital_pos(mn);
        if (satpos == INT_MAX) continue;
        if (!dvb_network_check_orbital_pos(satpos, dmc->u.dmc_fe_qpsk.orbital_pos))
          break;
      }
      if (mn == NULL)
        return NULL;
    }
  }

  ln = (dvb_network_t*)mn;
  mm = dvb_network_find_mux(ln, dmc, onid, tsid);
  if (!mm && (ln->mn_autodiscovery || force)) {
    cls = dvb_network_mux_class((mpegts_network_t *)ln);
    save |= cls == &dvb_mux_dvbt_class && dmc->dmc_fe_type == DVB_TYPE_T;
    save |= cls == &dvb_mux_dvbc_class && dmc->dmc_fe_type == DVB_TYPE_C;
    save |= cls == &dvb_mux_dvbs_class && dmc->dmc_fe_type == DVB_TYPE_S;
    save |= cls == &dvb_mux_atsc_t_class && dmc->dmc_fe_type == DVB_TYPE_ATSC_T;
    save |= cls == &dvb_mux_atsc_c_class && dmc->dmc_fe_type == DVB_TYPE_ATSC_C;
    if (save && dmc->dmc_fe_type == DVB_TYPE_S) {
      satpos = dvb_network_get_orbital_pos(mn);
      /* do not allow to mix satellite positions */
      if (dvb_network_check_orbital_pos(satpos, dmc->u.dmc_fe_qpsk.orbital_pos))
        save = 0;
    }
    if (save) {
      mm = dvb_mux_create0(ln, onid, tsid, dmc, NULL, NULL);
      if (tvhtrace_enabled()) {
        char buf[128];
        dvb_mux_conf_str(&((dvb_mux_t *)mm)->lm_tuning, buf, sizeof(buf));
        tvhtrace("mpegts", "mux %p %s onid %i tsid %i added to network %s (autodiscovery)",
                 mm, buf, onid, tsid, mm->mm_network->mn_network_name);
      }
    }
  } else if (mm) {
    dvb_mux_t *lm = (dvb_mux_t*)mm;
    /* the nit tables may be inconsistent (like rolloff ping-pong) */
    /* accept information only from one origin mux */
    if (mm->mm_dmc_origin_expire > dispatch_clock && mm->mm_dmc_origin && mm->mm_dmc_origin != origin)
      goto noop;
    #define COMPARE(x, cbit) ({ \
      int xr = dmc->x != lm->lm_tuning.x; \
      if (xr) { \
        tvhtrace("mpegts", "create mux dmc->" #x " (%li) != lm->lm_tuning." #x \
                 " (%li)", (long)dmc->x, (long)lm->lm_tuning.x); \
        lm->lm_tuning.x = dmc->x; \
      } xr ? cbit : 0; })
    #define COMPAREN(x, cbit) ({ \
      int xr = dmc->x != 0 && dmc->x != 1 && dmc->x != lm->lm_tuning.x; \
      if (xr) { \
        tvhtrace("mpegts", "create mux dmc->" #x " (%li) != lm->lm_tuning." #x \
                 " (%li)", (long)dmc->x, (long)lm->lm_tuning.x); \
        lm->lm_tuning.x = dmc->x; \
      } xr ? cbit : 0; })
    dvb_mux_conf_t tuning_old;
    char buf[128];
    tuning_old = lm->lm_tuning;
    /* Always save the orbital position */
    if (dmc->dmc_fe_type == DVB_TYPE_S) {
      if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos == INT_MAX ||
          dvb_network_check_orbital_pos(lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos,
                                        dmc->u.dmc_fe_qpsk.orbital_pos))
        save |= COMPARE(u.dmc_fe_qpsk.orbital_pos, CBIT_ORBITAL_POS);
    }
    /* Do not change anything else without autodiscovery flag */
    if (!ln->mn_autodiscovery)
      goto save;
    /* Handle big diffs that have been allowed through for DVB-S */
    if (deltaU32(dmc->dmc_fe_freq, lm->lm_tuning.dmc_fe_freq) > 4000) {
      lm->lm_tuning.dmc_fe_freq = dmc->dmc_fe_freq;
      save |= CBIT_FREQ;
    }
    save |= COMPAREN(dmc_fe_modulation, CBIT_MODULATION);
    save |= COMPAREN(dmc_fe_inversion, CBIT_INVERSION);
    save |= COMPAREN(dmc_fe_rolloff, CBIT_ROLLOFF);
    save |= COMPAREN(dmc_fe_pilot, CBIT_PILOT);
    switch (dmc->dmc_fe_type) {
    case DVB_TYPE_T:
      save |= COMPARE(dmc_fe_stream_id, CBIT_STREAM_ID);
      save |= COMPAREN(u.dmc_fe_ofdm.bandwidth, CBIT_BANDWIDTH);
      save |= COMPAREN(u.dmc_fe_ofdm.code_rate_HP, CBIT_RATE_HP);
      save |= COMPAREN(u.dmc_fe_ofdm.code_rate_LP, CBIT_RATE_LP);
      save |= COMPAREN(u.dmc_fe_ofdm.transmission_mode, CBIT_TRANS_MODE);
      save |= COMPAREN(u.dmc_fe_ofdm.guard_interval, CBIT_GUARD);
      save |= COMPAREN(u.dmc_fe_ofdm.hierarchy_information, CBIT_HIERARCHY);
      break;
    case DVB_TYPE_S:
      save |= COMPARE(u.dmc_fe_qpsk.polarisation, CBIT_POLARISATION);
      save |= COMPARE(u.dmc_fe_qpsk.symbol_rate, CBIT_RATE);
      save |= COMPARE(dmc_fe_stream_id, CBIT_STREAM_ID);
      save |= COMPAREN(dmc_fe_pls_mode, CBIT_PLS_MODE);
      save |= COMPAREN(dmc_fe_pls_code, CBIT_PLS_CODE);
      save |= COMPAREN(u.dmc_fe_qpsk.fec_inner, CBIT_FEC_INNER);
      break;
    case DVB_TYPE_C:
      save |= COMPARE(u.dmc_fe_qam.symbol_rate, CBIT_RATE);
      save |= COMPAREN(u.dmc_fe_qam.fec_inner, CBIT_FEC_INNER);
      break;
    default:
      abort();
    }
    #undef COMPARE
    #undef COMPAREN
    /* ignore rolloff only changes (don't save) */
    save &= ~CBIT_ROLLOFF;
    if (save) {
      char muxname[128];
      mpegts_mux_nice_name((mpegts_mux_t *)mm, muxname, sizeof(muxname));
      dvb_mux_conf_str(&tuning_old, buf, sizeof(buf));
      tvhwarn("mpegts", "mux %s changed from %s (%08x)", muxname, buf, save);
      dvb_mux_conf_str(&lm->lm_tuning, buf, sizeof(buf));
      tvhwarn("mpegts", "mux %s changed to   %s (%08x)", muxname, buf, save);
    }
  }
save:
  if (mm && save) {
    mm->mm_dmc_origin        = origin;
    mm->mm_dmc_origin_expire = dispatch_clock + 3600 * 24; /* one day */
    mm->mm_config_save((mpegts_mux_t *)mm);
  }
noop:
  return (mpegts_mux_t *)mm;
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
  const char *s;

  ln = calloc(1, sizeof(dvb_network_t));
  ln->ln_type = dvb_fe_type_by_network_class(idc);
  assert(ln->ln_type != DVB_TYPE_NONE);

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

  /* Set predefined muxes */
  /* Because PO_NOSAVE in idnode_load() this value is not set on load */
  if ((s = htsmsg_get_str(conf, "scanfile")) != NULL)
    dvb_network_class_scanfile_set(ln, s);

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
  &dvb_network_atsc_t_class,
  &dvb_network_atsc_c_class,
};

void dvb_network_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  const char *s;
  int i;

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
      if (strcmp(s, "dvb_network_atsc") == 0)
        s = "dvb_network_atsc_t";
      if(!strcmp(dvb_network_classes[i]->ic_class, s)) {
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

const idclass_t *dvb_network_class_by_fe_type(dvb_fe_type_t type)
{
  if (type == DVB_TYPE_T)
    return &dvb_network_dvbt_class;
  else if (type == DVB_TYPE_C)
    return &dvb_network_dvbc_class;
  else if (type == DVB_TYPE_S)
    return &dvb_network_dvbs_class;
  else if (type == DVB_TYPE_ATSC_T)
    return &dvb_network_atsc_t_class;
  else if (type == DVB_TYPE_ATSC_C)
    return &dvb_network_atsc_c_class;

  return NULL;
}

dvb_fe_type_t dvb_fe_type_by_network_class(const idclass_t *idc)
{
  if (idc == &dvb_network_dvbt_class)
    return DVB_TYPE_T;
  else if (idc == &dvb_network_dvbc_class)
    return DVB_TYPE_C;
  else if (idc == &dvb_network_dvbs_class)
    return DVB_TYPE_S;
  else if (idc == &dvb_network_atsc_t_class)
    return DVB_TYPE_ATSC_T;
  else if (idc == &dvb_network_atsc_c_class)
    return DVB_TYPE_ATSC_C;

  return DVB_TYPE_NONE;
}

idnode_set_t *dvb_network_list_by_fe_type(dvb_fe_type_t type)
{
  const idclass_t *idc = dvb_network_class_by_fe_type(type);

  if (!idc)
    return NULL;

  return idnode_find_all(idc, NULL);
}

int dvb_network_get_orbital_pos(mpegts_network_t *mn)
{
  dvb_network_t *ln = (dvb_network_t *)mn;
  mpegts_mux_t  *mm;
  dvb_mux_t     *lm = NULL;

  if (!ln)
    return INT_MAX;
  if (tvhtrace_enabled()) {
    if (!idnode_is_instance(&mn->mn_id, &dvb_network_dvbs_class)) {
      tvhinfo("mpegts", "wrong dvb_network_get_orbital_pos() call");
      return INT_MAX;
    }
  }
  if (ln->mn_satpos != INT_MAX)
    return ln->mn_satpos;
  LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link) {
    lm = (dvb_mux_t *)mm;
    if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos != INT_MAX)
      break;
  }
  if (mm)
    return lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos;
  return INT_MAX;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
