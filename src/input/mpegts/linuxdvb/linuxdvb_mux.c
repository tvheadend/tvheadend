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

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t mpegts_mux_class;

/*
 * Generic
 */

/* Macro to defien mux class str get/set */
#define linuxdvb_mux_class_X(c, f, p, l, ...)\
static const char * \
linuxdvb_mux_##c##_class_##l##_get (void *o)\
{\
  linuxdvb_mux_t *lm = o;\
  return dvb_##l##2str(lm->lm_tuning.dmc_fe_params.u.f.p);\
}\
static int \
linuxdvb_mux_##c##_class_##l##_set (void *o, const char *s)\
{\
  linuxdvb_mux_t *lm = o;\
  lm->lm_tuning.dmc_fe_params.u.f.p = dvb_str2##l (s);\
  return 1;\
}\
static htsmsg_t *\
linuxdvb_mux_##c##_class_##l##_enum (void *o)\
{\
  static const int     t[] = { __VA_ARGS__ };\
  int i;\
  htsmsg_t *m = htsmsg_create_list();\
  for (i = 0; i < ARRAY_SIZE(t); i++)\
    htsmsg_add_str(m, NULL, dvb_##l##2str(t[i]));\
  return m;\
}
#define MUX_PROP_STR(_id, _name, t, l)\
  .type = PT_STR,\
  .id   = _id,\
  .name = _name,\
  .str_get  = linuxdvb_mux_##t##_class_##l##_get,\
  .str_set  = linuxdvb_mux_##t##_class_##l##_set,\
  .str_enum = linuxdvb_mux_##t##_class_##l##_enum

const idclass_t linuxdvb_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "linuxdvb_mux",
  .ic_caption    = "Linux DVB Multiplex",
  .ic_properties = (const property_t[]){
    {}
  }
};

/*
 * DVB-T
 */

linuxdvb_mux_class_X(dvbt, ofdm, bandwidth,             bw,
                     BANDWIDTH_AUTO
                     , BANDWIDTH_8_MHZ, BANDWIDTH_7_MHZ, BANDWIDTH_6_MHZ
#if DVB_VER_ATLEAST(5,4)
                     , BANDWIDTH_5_MHZ
                     , BANDWIDTH_10_MHZ
                     , BANDWIDTH_1_712_MHZ
#endif
                    );
linuxdvb_mux_class_X(dvbt, ofdm, constellation,         qam,
                     QAM_AUTO, QPSK, QAM_16, QAM_64, QAM_256
                    );
linuxdvb_mux_class_X(dvbt, ofdm, transmission_mode,     mode,
                    TRANSMISSION_MODE_AUTO,
                    TRANSMISSION_MODE_2K, TRANSMISSION_MODE_8K
#if DVB_VER_ATLEAST(5,4)
                    , TRANSMISSION_MODE_1K, TRANSMISSION_MODE_16K
                    , TRANSMISSION_MODE_32K
#endif
                    );
linuxdvb_mux_class_X(dvbt, ofdm, guard_interval,        guard,
                     GUARD_INTERVAL_AUTO, GUARD_INTERVAL_1_4,
                     GUARD_INTERVAL_1_8, GUARD_INTERVAL_1_16,
                     GUARD_INTERVAL_1_32
#if DVB_VER_ATLEAST(5,4)
                     , GUARD_INTERVAL_1_128, GUARD_INTERVAL_19_128
                     , GUARD_INTERVAL_19_256
#endif
                    );
linuxdvb_mux_class_X(dvbt, ofdm, hierarchy_information, hier,
                     HIERARCHY_AUTO, HIERARCHY_NONE,
                     HIERARCHY_1, HIERARCHY_2, HIERARCHY_4
                    );
linuxdvb_mux_class_X(dvbt, ofdm, code_rate_HP,          fechi,
                     FEC_AUTO,
                     FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, FEC_7_8
#if DVB_VER_ATLEAST(5,4)
                     , FEC_3_5
#endif
                    );
linuxdvb_mux_class_X(dvbt, ofdm, code_rate_LP,          feclo,
                     FEC_AUTO,
                     FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, FEC_7_8
#if DVB_VER_ATLEAST(5,4)
                     , FEC_3_5
#endif
                    );

const idclass_t linuxdvb_mux_dvbt_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbt",
  .ic_caption    = "Linux DVB-T Multiplex",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = "Frequency (MHz)",
      .opts     = PO_WRONCE,
      .off      = offsetof(linuxdvb_mux_t, lm_tuning.dmc_fe_params.frequency),
    },
    {
      MUX_PROP_STR("bandwidth", "Bandwidth", dvbt, bw)
    },
    {
      MUX_PROP_STR("constellation", "Constellation", dvbt, qam)
    },
    {
      MUX_PROP_STR("transmission_mode", "Transmission Mode", dvbt, mode)
    },
    {
      MUX_PROP_STR("guard_interval", "Guard Interval", dvbt, guard)
    },
    {
      MUX_PROP_STR("hierarchy", "Hierarchy", dvbt, hier),
    },
    {
      MUX_PROP_STR("fec_hi", "FEC High", dvbt, fechi),
    },
    {
      MUX_PROP_STR("fec_lo", "FEC Low", dvbt, feclo),
    },
    {}
  }
};

/*
 * DVB-C
 */

linuxdvb_mux_class_X(dvbc, qam, modulation,            qam,
                     QAM_AUTO, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256
                    );
linuxdvb_mux_class_X(dvbc, qam, fec_inner,             fec,
                     FEC_AUTO, FEC_NONE,
                     FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, FEC_8_9
#if DVB_VER_ATLEAST(5,4)
                     , FEC_9_10
#endif
                    );
const idclass_t linuxdvb_mux_dvbc_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbc",
  .ic_caption    = "Linux DVB-C Multiplex",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = "Frequency (MHz)",
      .opts     = PO_WRONCE,
      .off      = offsetof(linuxdvb_mux_t, lm_tuning.dmc_fe_params.frequency),
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = "Symbol Rate",
      .opts     = PO_WRONCE,
      .off      = offsetof(linuxdvb_mux_t, lm_tuning.dmc_fe_params.u.qam.symbol_rate),
    },
    {
      MUX_PROP_STR("constellation", "Constellation", dvbc, qam)
    },
    {
      MUX_PROP_STR("fec", "FEC", dvbc, fec)
    },
    {}
  }
};

linuxdvb_mux_class_X(dvbs, qam, modulation,            qam,
                     QAM_AUTO, QPSK, QAM_16
#if DVB_VER_ATLEAST(5,4)
                     , PSK_8, APSK_16, APSK_32
#endif
                    );
linuxdvb_mux_class_X(dvbs, qam, fec_inner,             fec,
                     FEC_AUTO, FEC_NONE,
                     FEC_1_2, FEC_2_3, FEC_3_4, FEC_4_5, FEC_5_6, FEC_7_8,
                     FEC_8_9
#if DVB_VER_ATLEAST(5,4)
                     , FEC_3_5, FEC_9_10
#endif
                    );
static const char *
linuxdvb_mux_dvbs_class_polarity_get (void *o)
{
  linuxdvb_mux_t *lm = o;
  return dvb_pol2str(lm->lm_tuning.dmc_fe_polarisation);
}
static int
linuxdvb_mux_dvbs_class_polarity_set (void *o, const char *s)
{
  linuxdvb_mux_t *lm = o;
  lm->lm_tuning.dmc_fe_polarisation = dvb_str2pol(s);
  return 1;
}
static htsmsg_t *
linuxdvb_mux_dvbs_class_polarity_enum (void *o)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_pol2str(POLARISATION_VERTICAL));
  htsmsg_add_str(list, NULL, dvb_pol2str(POLARISATION_HORIZONTAL));
  htsmsg_add_str(list, NULL, dvb_pol2str(POLARISATION_CIRCULAR_LEFT));
  htsmsg_add_str(list, NULL, dvb_pol2str(POLARISATION_CIRCULAR_RIGHT));
  return list;
}

const idclass_t linuxdvb_mux_dvbs_class =
{
  .ic_super      = &linuxdvb_mux_class,
  .ic_class      = "linuxdvb_mux_dvbs",
  .ic_caption    = "Linux DVB-S Multiplex",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = "Frequency (MHz)",
      .opts     = PO_WRONCE,
      .off      = offsetof(linuxdvb_mux_t, lm_tuning.dmc_fe_params.frequency),
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = "Symbol Rate",
      .opts     = PO_WRONCE,
      .off      = offsetof(linuxdvb_mux_t, lm_tuning.dmc_fe_params.u.qpsk.symbol_rate),
    },
    {
      MUX_PROP_STR("polarisation", "Polarisation", dvbs, polarity)
    },
    {
      MUX_PROP_STR("constellation", "Constellation", dvbs, qam)
    },
    {
      MUX_PROP_STR("fec", "FEC", dvbs, fec)
    },
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

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
linuxdvb_mux_config_save ( mpegts_mux_t *mm )
{
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  hts_settings_save(c, "input/linuxdvb/networks/%s/muxes/%s/config",
                    idnode_uuid_as_str(&mm->mm_network->mn_id),
                    idnode_uuid_as_str(&mm->mm_id));
  htsmsg_destroy(c);
}

static void
linuxdvb_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  linuxdvb_mux_t *lm = (linuxdvb_mux_t*)mm;
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mm->mm_network;
  snprintf(buf, len, "%d %s%s [onid:%04X, tsid:%04X]",
           lm->lm_tuning.dmc_fe_params.frequency,
           (ln->ln_type == FE_QPSK) ? "MHz " : "Hz",
           (ln->ln_type == FE_QPSK) 
             ? dvb_pol2str(lm->lm_tuning.dmc_fe_polarisation)
             : "",
           mm->mm_onid, mm->mm_tsid);
}

static void
linuxdvb_mux_create_instances ( mpegts_mux_t *mm )
{
  mpegts_input_t *mi;
  LIST_FOREACH(mi, &mm->mm_network->mn_inputs, mi_network_link)
    mi->mi_create_mux_instance(mi, mm);
}

static void
linuxdvb_mux_open_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  char buf[256];
  linuxdvb_frontend_t *lfe;
  if (mt->mt_pid >= 0x2000)
    return;
  mm->mm_display_name(mm, buf, sizeof(buf));
  if (!mm->mm_table_filter[mt->mt_pid])
    tvhtrace("mpegts", "%s - opened table %s pid %04X (%d)",
             buf, mt->mt_name, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 1;

  /* Open DMX */
  if (mm->mm_active) {
    lfe       = (linuxdvb_frontend_t*)mm->mm_active->mmi_input;
    mt->mt_fd = lfe->lfe_open_pid(lfe, mt->mt_pid, NULL);
  }
}

static void
linuxdvb_mux_close_table ( mpegts_mux_t *mm, mpegts_table_t *mt )
{
  char buf[256];
  if (mt->mt_pid >= 0x2000)
    return;
  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - closed table %s pid %04X (%d)",
           buf, mt->mt_name, mt->mt_pid, mt->mt_pid);
  mm->mm_table_filter[mt->mt_pid] = 0;
  close(mt->mt_fd);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

linuxdvb_mux_t *
linuxdvb_mux_create0
  ( linuxdvb_network_t *ln,
    uint16_t onid, uint16_t tsid, const dvb_mux_conf_t *dmc,
    const char *uuid, htsmsg_t *conf )
{
  const idclass_t *idc;
  mpegts_mux_t *mm;
  linuxdvb_mux_t *lm;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

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
    tvherror("linuxdvb", "unknown FE type %d", ln->ln_type);
    return NULL;
  }

  /* Create */
  if (!(mm = mpegts_mux_create0(calloc(1, sizeof(linuxdvb_mux_t)), idc, uuid,
                                (mpegts_network_t*)ln, onid, tsid, conf)))
    return NULL;
  lm = (linuxdvb_mux_t*)mm;

  /* Tuning */
  if (dmc)
    memcpy(&lm->lm_tuning, dmc, sizeof(dvb_mux_conf_t));

  /* Callbacks */
  lm->mm_display_name     = linuxdvb_mux_display_name;
  lm->mm_config_save      = linuxdvb_mux_config_save;
  lm->mm_create_instances = linuxdvb_mux_create_instances;
  lm->mm_open_table       = linuxdvb_mux_open_table;
  lm->mm_close_table      = linuxdvb_mux_close_table;
  
  /* No config */
  if (!conf) return lm;

  /* Services */
  c = hts_settings_load_r(1, "input/linuxdvb/networks/%s/muxes/%s/services",
                         idnode_uuid_as_str(&ln->mn_id),
                         idnode_uuid_as_str(&mm->mm_id));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      (void)linuxdvb_service_create0(lm, 0, 0, f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }

  return lm;
}
