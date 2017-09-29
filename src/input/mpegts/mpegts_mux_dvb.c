/*
 *  Tvheadend - DVB Multiplex
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
#include "mpegts_dvb.h"
#include "queue.h"
#include "settings.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
dvb_mux_delete ( mpegts_mux_t *mm, int delconf );

extern const idclass_t mpegts_mux_class;

/*
 * Generic
 */

/* Macro to define mux class str get/set */
#define dvb_mux_class_R(c, f, l, t, ...)\
static const void * \
dvb_mux_##c##_class_##l##_get (void *o)\
{\
  dvb_mux_t *lm = o;\
  prop_ptr = dvb_##t##2str(lm->lm_tuning.dmc_fe_##f);\
  return &prop_ptr;\
}\
static int \
dvb_mux_##c##_class_##l##_set (void *o, const void *v)\
{\
  dvb_mux_t *lm = o;\
  lm->lm_tuning.dmc_fe_##f = dvb_str2##t ((const char*)v);\
  return 1;\
}\
static htsmsg_t *\
dvb_mux_##c##_class_##l##_enum (void *o, const char *lang)\
{\
  static const int     t[] = { __VA_ARGS__ };\
  int i;\
  htsmsg_t *m = htsmsg_create_list(), *e;\
  for (i = 0; i < ARRAY_SIZE(t); i++) {\
    e = htsmsg_create_key_val(dvb_##t##2str(t[i]), tvh_gettext_lang(lang, dvb_##t##2str(t[i]))); \
    htsmsg_add_msg(m, NULL, e);\
  }\
  return m;\
}
#define dvb_mux_class_X(c, f, p, l, t, ...)\
static const void * \
dvb_mux_##c##_class_##l##_get (void *o)\
{\
  dvb_mux_t *lm = o;\
  prop_ptr = dvb_##t##2str(lm->lm_tuning.u.dmc_fe_##f.p);\
  return &prop_ptr;\
}\
static int \
dvb_mux_##c##_class_##l##_set (void *o, const void *v)\
{\
  dvb_mux_t *lm = o;\
  lm->lm_tuning.u.dmc_fe_##f.p = dvb_str2##t ((const char*)v);\
  return 1;\
}\
static htsmsg_t *\
dvb_mux_##c##_class_##l##_enum (void *o, const char *lang)\
{\
  static const int     t[] = { __VA_ARGS__ };\
  int i;\
  htsmsg_t *m = htsmsg_create_list(), *e;\
  for (i = 0; i < ARRAY_SIZE(t); i++) {\
    e = htsmsg_create_key_val(dvb_##t##2str(t[i]), tvh_gettext_lang(lang, dvb_##t##2str(t[i]))); \
    htsmsg_add_msg(m, NULL, e);\
  }\
  return m;\
}
#define MUX_PROP_STR(_id, _name, t, l, d)\
  .type  = PT_STR,\
  .id    = _id,\
  .name  = _name,\
  .get   = dvb_mux_##t##_class_##l##_get,\
  .set   = dvb_mux_##t##_class_##l##_set,\
  .list  = dvb_mux_##t##_class_##l##_enum,\
  .def.s = d

static const void *
dvb_mux_class_delsys_get (void *o)
{
  dvb_mux_t *lm = o;
  prop_ptr = dvb_delsys2str(lm->lm_tuning.dmc_fe_delsys);
  return &prop_ptr;
}

static int
dvb_mux_class_delsys_set (void *o, const void *v)
{
  const char *s = v;
  int delsys = dvb_str2delsys(s);
  dvb_mux_t *lm = o;
  if (delsys != lm->lm_tuning.dmc_fe_delsys) {
    lm->lm_tuning.dmc_fe_delsys = dvb_str2delsys(s);
    return 1;
  }
  return 0;
}

const idclass_t dvb_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "dvb_mux",
  .ic_caption    = N_("DVB multiplex"),
  .ic_properties = (const property_t[]){
    {}
  }
};

/*
 * DVB-T
 */

dvb_mux_class_X(dvbt, ofdm, bandwidth, bw, bw,
                     DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_10_MHZ,
                     DVB_BANDWIDTH_8_MHZ, DVB_BANDWIDTH_7_MHZ,
                     DVB_BANDWIDTH_6_MHZ, DVB_BANDWIDTH_5_MHZ,
                     DVB_BANDWIDTH_1_712_MHZ);
dvb_mux_class_R(dvbt, modulation, qam, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(dvbt, ofdm, transmission_mode, mode, mode,
                     DVB_TRANSMISSION_MODE_AUTO, DVB_TRANSMISSION_MODE_32K,
                     DVB_TRANSMISSION_MODE_16K, DVB_TRANSMISSION_MODE_8K,
                     DVB_TRANSMISSION_MODE_2K, DVB_TRANSMISSION_MODE_1K);
dvb_mux_class_X(dvbt, ofdm, guard_interval, guard, guard,
                     DVB_GUARD_INTERVAL_AUTO, DVB_GUARD_INTERVAL_1_32,
                     DVB_GUARD_INTERVAL_1_16, DVB_GUARD_INTERVAL_1_8,
                     DVB_GUARD_INTERVAL_1_4, DVB_GUARD_INTERVAL_1_128,
                     DVB_GUARD_INTERVAL_19_128, DVB_GUARD_INTERVAL_19_256);
dvb_mux_class_X(dvbt, ofdm, hierarchy_information, hier, hier,
                     DVB_HIERARCHY_AUTO, DVB_HIERARCHY_NONE,
                     DVB_HIERARCHY_1, DVB_HIERARCHY_2, DVB_HIERARCHY_4);
dvb_mux_class_X(dvbt, ofdm, code_rate_HP, fechi, fechi,
                     DVB_FEC_AUTO, DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4,
                     DVB_FEC_3_5,  DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8);
dvb_mux_class_X(dvbt, ofdm, code_rate_LP, feclo, feclo,
                     DVB_FEC_AUTO, DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4,
                     DVB_FEC_3_5,  DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8);

#define dvb_mux_dvbt_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_dvbt_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_dvbt_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBT));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBT2));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_TURBO));
  return list;
}

static int
dvb_mux_dvbt_class_frequency_set ( void *o, const void *v )
{
  dvb_mux_t *lm = o;
  uint32_t val = *(uint32_t *)v;

  if (val < 1000)
    val *= 1000000;
  else if (val < 1000000)
    val *= 1000;

  if (val != lm->lm_tuning.dmc_fe_freq) {
    lm->lm_tuning.dmc_fe_freq = val;
    return 1;
  }
  return 0;
}

const idclass_t dvb_mux_dvbt_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dvbt",
  .ic_caption    = N_("DVB-T multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbt, delsys, "DVBT"),
      .desc     = N_("The delivery system the mux uses. "
                     "Make sure that your tuner supports the delivery "
                     "system selected here."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      MUX_PROP_STR("bandwidth", N_("Bandwidth"), dvbt, bw, N_("AUTO")),
      .desc     = N_("The bandwidth the mux uses. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dvbt, qam, N_("AUTO")),
      .desc     = N_("The COFDM modulation used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("transmission_mode", N_("Transmission mode"), dvbt, mode, N_("AUTO")),
      .desc     = N_("The transmission/OFDM mode used by the mux. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("guard_interval", N_("Guard interval"), dvbt, guard, N_("AUTO")),
      .desc     = N_("The guard interval used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("hierarchy", N_("Hierarchy"), dvbt, hier, N_("AUTO")),
      .desc     = N_("The hierarchical modulation used by the mux. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_hi", N_("FEC high"), dvbt, fechi, N_("AUTO")),
      .desc     = N_("The forward error correction high value. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_lo", N_("FEC low"), dvbt, feclo, N_("AUTO")),
      .desc     = N_("The forward error correction low value. "
                     "Most people will not need to change this setting."),
    },
    {
      .type     = PT_INT,
      .id       = "plp_id",
      .name     = N_("PLP ID"),
      .desc     = N_("The physical layer pipe ID. "
                     "Most people will not need to change this setting."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_stream_id),
      .def.i	= DVB_NO_STREAM_ID_FILTER,
    },
    {}
  }
};

/*
 * DVB-C
 */

dvb_mux_class_R(dvbc, modulation, qam, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QAM_16, DVB_MOD_QAM_32,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_128, DVB_MOD_QAM_256);
dvb_mux_class_X(dvbc, qam, fec_inner, fec, fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_4_5,
                     DVB_FEC_5_6, DVB_FEC_8_9, DVB_FEC_9_10);

#define dvb_mux_dvbc_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_dvbc_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_dvbc_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBC_ANNEX_A));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBC_ANNEX_B));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBC_ANNEX_C));
  return list;
}

const idclass_t dvb_mux_dvbc_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dvbc",
  .ic_caption    = N_("DVB-C multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbc, delsys, "DVB-C"),
      .desc     = N_("The delivery system used by your cable provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = N_("Symbol rate (Sym/s)"),
      .desc     = N_("The symbol rate used on the mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_qam.symbol_rate),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("AUTO")),
      .desc     = N_("The quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("The forward error correction used on the mux."),
    },
    {}
  }
};

/*
 * DVB-S
 */

dvb_mux_class_X(dvbs, qpsk, fec_inner, fec, fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_3_5,
                     DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8, DVB_FEC_8_9,
                     DVB_FEC_9_10);
dvb_mux_class_R(dvbs, modulation, qam, qam,
                     DVB_MOD_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_PSK_8, DVB_MOD_APSK_16, DVB_MOD_APSK_32);
dvb_mux_class_R(dvbs, rolloff, rolloff, rolloff,
                     DVB_ROLLOFF_35, DVB_ROLLOFF_20, DVB_ROLLOFF_25);
dvb_mux_class_R(dvbs, pilot, pilot, pilot,
                     DVB_PILOT_AUTO, DVB_PILOT_ON, DVB_PILOT_OFF);
dvb_mux_class_X(dvbs, qpsk, polarisation, polarisation, pol,
                     DVB_POLARISATION_VERTICAL, DVB_POLARISATION_HORIZONTAL,
                     DVB_POLARISATION_CIRCULAR_LEFT, DVB_POLARISATION_CIRCULAR_RIGHT);
dvb_mux_class_R(dvbs, pls_mode, pls_mode, plsmode,
                     DVB_PLS_ROOT, DVB_PLS_GOLD, DVB_PLS_COMBO);

static int
dvb_mux_dvbs_class_frequency_set ( void *o, const void *v )
{
  dvb_mux_t *lm = o;
  uint32_t val = *(uint32_t *)v;

  if (val < 100000)
    val *= 1000;

  if (val != lm->lm_tuning.dmc_fe_freq) {
    lm->lm_tuning.dmc_fe_freq = val;
    return 1;
  }
  return 0;
}

static int
dvb_mux_dvbs_class_symbol_rate_set ( void *o, const void *v )
{
  dvb_mux_t *lm = o;
  uint32_t val = *(uint32_t *)v;

  if (val < 100000)
    val *= 1000;

  if (val != lm->lm_tuning.u.dmc_fe_qpsk.symbol_rate) {
    lm->lm_tuning.u.dmc_fe_qpsk.symbol_rate = val;
    return 1;
  }
  return 0;
}

#define dvb_mux_dvbs_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_dvbs_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_dvbs_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBS));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBS2));
  return list;
}

static const void *
dvb_mux_dvbs_class_orbital_get ( void *o )
{
  static char buf[16], *s = buf;
  dvb_mux_t *lm = o;
  if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos == INT_MAX)
    buf[0] = '\0';
  else
    dvb_sat_position_to_str(lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos, buf, sizeof(buf));
  return &s;
}

static int
dvb_mux_dvbs_class_orbital_set ( void *o, const void *s )
{
  dvb_mux_t *lm = o;
  int pos;

  pos = dvb_sat_position_from_str((const char *)s);

  if (pos != lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos) {
    lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos = pos;
    return 1;
  }
  return 0;
}

const idclass_t dvb_mux_dvbs_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dvbs",
  .ic_caption    = N_("DVB-S multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbs, delsys, "DVBS"),
      .desc     = N_("The delivery system used by your provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (kHz)"),
      .desc     = N_("The frequency of the mux/transponder in Hertz."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbs_class_frequency_set,
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = N_("Symbol rate (Sym/s)"),
      .desc     = N_("The symbol rate used on the mux/transponder."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_qpsk.symbol_rate),
      .set      = dvb_mux_dvbs_class_symbol_rate_set,
    },
    {
      MUX_PROP_STR("polarisation", N_("Polarization"), dvbs, polarisation, NULL),
      .desc     = N_("The polarization used on the mux."),
    },
    {
      MUX_PROP_STR("modulation", N_("Modulation"), dvbs, qam, NULL),
      .desc     = N_("The modulation used on the mux."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbs, fec, "AUTO"),
      .desc     = N_("The forward error correction. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("rolloff", N_("Rolloff"), dvbs, rolloff, "AUTO"),
      .desc     = N_("The rolloff used on the mux."),
      .opts     = PO_ADVANCED,
    },
    {
      MUX_PROP_STR("pilot", N_("Pilot"), dvbs, pilot, "AUTO"),
      .desc     = N_("Enable/disable pilot tone."),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "stream_id",
      .name     = N_("ISI (Stream ID)"),
      .desc     = N_("The stream ID used for the mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_stream_id),
      .def.i	= DVB_NO_STREAM_ID_FILTER,
      .opts     = PO_EXPERT
    },
    {
      MUX_PROP_STR("pls_mode", N_("PLS mode"), dvbs, pls_mode, "ROOT"),
      .desc     = N_("The Physical Layer Scrambling (PLS) mode "
                     "used on the mux."),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_U32,
      .id       = "pls_code",
      .name     = N_("PLS code"),
      .desc     = N_("The Physical Layer Scrambling (PLS) code "
                     "used on the mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_pls_code),
      .def.u32	= 1,
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_STR,
      .id       = "orbital",
      .name     = N_("Orbital position"),
      .desc     = N_("The orbital position of the satellite the mux is on."),
      .set      = dvb_mux_dvbs_class_orbital_set,
      .get      = dvb_mux_dvbs_class_orbital_get,
      .opts     = PO_ADVANCED | PO_RDONLY
    },
    {}
  }
};

/*
 * ATSC-T
 */

#define dvb_mux_atsc_t_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_atsc_t_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_atsc_t_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_ATSC));
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_ATSCMH));
  return list;
}

dvb_mux_class_R(atsc_t, modulation, qam, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QAM_256, DVB_MOD_VSB_8);

const idclass_t dvb_mux_atsc_t_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_atsc_t",
  .ic_caption    = N_("ATSC-T multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), atsc_t, delsys, "ATSC-T"),
      .desc     = N_("The delivery system used by your provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      MUX_PROP_STR("modulation", N_("Modulation"), atsc_t, qam, N_("AUTO")),
      .desc     = N_("The modulation used on the mux."),
    },
    {}
  }
};

/*
 * ATSC-C
 */

#define dvb_mux_atsc_c_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_atsc_c_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_atsc_c_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBC_ANNEX_B));
  return list;
}

const idclass_t dvb_mux_atsc_c_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_atsc_c",
  .ic_caption    = N_("ATSC-C multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), atsc_c, delsys, "ATSC-C"),
      .desc     = N_("The delivery system used by your provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = N_("Symbol rate (Sym/s)"),
      .desc     = N_("The symbol rate."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_qam.symbol_rate),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("AUTO")),
      .desc     = N_("The quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("The forward error correction used on the mux."),
    },
    {}
  }
};

/*
 * ISDB-T
 */

#define dvb_mux_isdb_t_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_isdb_t_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_isdb_t_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_ISDBT));
  return list;
}

dvb_mux_class_X(isdb_t, isdbt, bandwidth, bw, bw,
                     DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_10_MHZ,
                     DVB_BANDWIDTH_8_MHZ, DVB_BANDWIDTH_7_MHZ,
                     DVB_BANDWIDTH_6_MHZ, DVB_BANDWIDTH_5_MHZ,
                     DVB_BANDWIDTH_1_712_MHZ);
dvb_mux_class_X(isdb_t, isdbt, guard_interval, guard, guard,
                     DVB_GUARD_INTERVAL_AUTO, DVB_GUARD_INTERVAL_1_32,
                     DVB_GUARD_INTERVAL_1_16, DVB_GUARD_INTERVAL_1_8,
                     DVB_GUARD_INTERVAL_1_4, DVB_GUARD_INTERVAL_1_128,
                     DVB_GUARD_INTERVAL_19_128, DVB_GUARD_INTERVAL_19_256);
dvb_mux_class_X(isdb_t, isdbt, layers[0].modulation, isdbt_mod_a, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(isdb_t, isdbt, layers[0].fec, isdbt_fec_a, fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_4_5,
                     DVB_FEC_5_6, DVB_FEC_8_9, DVB_FEC_9_10);
dvb_mux_class_X(isdb_t, isdbt, layers[1].modulation, isdbt_mod_b, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(isdb_t, isdbt, layers[1].fec, isdbt_fec_b, fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_4_5,
                     DVB_FEC_5_6, DVB_FEC_8_9, DVB_FEC_9_10);
dvb_mux_class_X(isdb_t, isdbt, layers[2].modulation, isdbt_mod_c, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(isdb_t, isdbt, layers[2].fec, isdbt_fec_c, fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_4_5,
                     DVB_FEC_5_6, DVB_FEC_8_9, DVB_FEC_9_10);

const idclass_t dvb_mux_isdb_t_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_isdbt",
  .ic_caption    = N_("ISDB-T multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), isdb_t, delsys, "ISDB-T"),
      .desc     = N_("The delivery system used by your provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      MUX_PROP_STR("bandwidth", N_("Bandwidth"), isdb_t, bw, N_("AUTO")),
      .desc     = N_("The bandwidth the mux uses. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("guard_interval", N_("Guard interval"), isdb_t, guard, N_("AUTO")),
      .desc     = N_("The guard interval used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    /* Layer A */
    {
      MUX_PROP_STR("layera_fec", N_("Layer A: FEC"), isdb_t, isdbt_fec_a, N_("AUTO")),
      .desc     = N_("The layer A forward error correction."),
    },
    {
      MUX_PROP_STR("layera_mod", N_("Layer A: Constellation"), isdb_t, isdbt_mod_a, N_("AUTO")),
      .desc     = N_("The layer A constellation."),
    },
    {
      .type     = PT_U32,
      .id       = "layera_segcnt",
      .name     = N_("Layer A: Segment count"),
      .desc     = N_("The layer A segment count."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[0].segment_count),
    },
    {
      .type     = PT_U32,
      .id       = "layera_timint",
      .name     = N_("Layer A: Time interleaving"),
      .desc     = N_("The layer A time interleaving."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[0].time_interleaving),
    },
    /* Layer B */
    {
      MUX_PROP_STR("layerb_fec", N_("Layer B: FEC"), isdb_t, isdbt_fec_b, N_("AUTO")),
      .desc     = N_("The layer B forward error correction."),
    },
    {
      MUX_PROP_STR("layerb_mod", N_("Layer B: Constellation"), isdb_t, isdbt_mod_b, N_("AUTO")),
      .desc     = N_("The layer B constellation."),
    },
    {
      .type     = PT_U32,
      .id       = "layerb_segcnt",
      .name     = N_("Layer B: Segment count"),
      .desc     = N_("The layer B segment count."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[1].segment_count),
    },
    {
      .type     = PT_U32,
      .id       = "layerb_timint",
      .name     = N_("Layer B: Time interleaving"),
      .desc     = N_("The layer B time interleaving."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[1].time_interleaving),
    },
    /* Layer C */
    {
      MUX_PROP_STR("layerc_fec", N_("Layer C: FEC"), isdb_t, isdbt_fec_c, N_("AUTO")),
      .desc     = N_("The layer C forward error correction."),
    },
    {
      MUX_PROP_STR("layerc_mod", N_("Layer C: Constellation"), isdb_t, isdbt_mod_c, N_("AUTO")),
      .desc     = N_("The layer C constellation."),
    },
    {
      .type     = PT_U32,
      .id       = "layerc_segcnt",
      .name     = N_("Layer C: Segment count"),
      .desc     = N_("The layer C segment count."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[2].segment_count),
    },
    {
      .type     = PT_U32,
      .id       = "layerc_timint",
      .name     = N_("Layer C: Time interleaving"),
      .desc     = N_("The layer C time interleaving."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_isdbt.layers[2].time_interleaving),
    },
    {}
  }
};

/*
 * ISDB-C
 */

const idclass_t dvb_mux_isdb_c_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_isdbc",
  .ic_caption    = N_("ISDB-C multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbc, delsys, "DVB-C"),
      .desc     = N_("The delivery system used by your cable provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = N_("Symbol rate (Sym/s)"),
      .desc     = N_("The symbol rate."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_qam.symbol_rate),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("AUTO")),
      .desc     = N_("The quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("The forward error correction used on the mux."),
    },
    {}
  }
};

/*
 * ISDB-S
 */

#define dvb_mux_isdb_s_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_isdb_s_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_isdb_s_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_ISDBS));
  return list;
}

const idclass_t dvb_mux_isdb_s_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_isdbs",
  .ic_caption    = N_("ISDB-S multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), isdb_s, delsys, "ISDBS"),
      .desc     = N_("The delivery system used by your provider."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (kHz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbs_class_frequency_set,
    },
    {
      .type     = PT_INT,
      .id       = "stream_id",
      .name     = N_("Stream ID"),
      .desc     = N_("The stream ID used for the mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_stream_id),
      .def.i	= DVB_NO_STREAM_ID_FILTER,
      .opts     = PO_ADVANCED
    },
    {}
  }
};

/*
 * DAB
 */

#define dvb_mux_dab_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_dab_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_dab_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DAB));
  return list;
}

const idclass_t dvb_mux_dab_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dab",
  .ic_caption    = N_("DAB multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dab, delsys, "DAB"),
      .desc     = N_("The delivery system used by the mux."),
    },
    {
      .type     = PT_U32,
      .id       = "frequency",
      .name     = N_("Frequency (Hz)"),
      .desc     = N_("The frequency of the mux (in Hertz)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .set      = dvb_mux_dvbt_class_frequency_set,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static htsmsg_t *
dvb_mux_config_save ( mpegts_mux_t *mm, char *filename, size_t fsize )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  if (filename == NULL) {
    mpegts_mux_save(mm, c, 1);
  } else {
    mpegts_mux_save(mm, c, 0);
    snprintf(filename, fsize, "input/dvb/networks/%s/muxes/%s",
             idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
             idnode_uuid_as_str(&mm->mm_id, ubuf2));
  }
  return c;
}

static void
dvb_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  dvb_mux_t *lm = (dvb_mux_t*)mm;
  dvb_network_t *ln = (dvb_network_t*)mm->mm_network;
  uint32_t freq = lm->lm_tuning.dmc_fe_freq, freq2;
  char extra[8], buf2[5], *p;
  if (ln->ln_type == DVB_TYPE_S) {
    const char *s = dvb_pol2str(lm->lm_tuning.u.dmc_fe_qpsk.polarisation);
    if (s) extra[0] = *s;
    extra[1] = '\0';
  } else {
    freq /= 1000;
    strcpy(extra, "MHz");
  }
  freq2 = freq % 1000;
  freq /= 1000;
  snprintf(buf2, sizeof(buf2), "%03d", freq2);
  p = buf2 + 2;
  while (freq2 && (freq2 % 10) == 0) {
    freq2 /= 10;
    *(p--) = '\0';
  }
  if (freq2)
    snprintf(buf, len, "%d.%s%s", freq, buf2, extra);
  else
    snprintf(buf, len, "%d%s", freq, extra);
}

static void
dvb_mux_create_instances ( mpegts_mux_t *mm )
{
  mpegts_network_link_t *mnl;
  LIST_FOREACH(mnl, &mm->mm_network->mn_inputs, mnl_mn_link) {
    mpegts_input_t *mi = mnl->mnl_input;
    if (mi->mi_is_enabled(mi, mm, 0, -1) != MI_IS_ENABLED_NEVER)
      mi->mi_create_mux_instance(mi, mm);
  }
}

static void
dvb_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Remove config */
  if (delconf)
    hts_settings_remove("input/dvb/networks/%s/muxes/%s",
                      idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
                      idnode_uuid_as_str(&mm->mm_id, ubuf2));

  /* Delete the mux */
  mpegts_mux_delete(mm, delconf);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

dvb_mux_t *
dvb_mux_create0
  ( dvb_network_t *ln,
    uint16_t onid, uint16_t tsid, const dvb_mux_conf_t *dmc,
    const char *uuid, htsmsg_t *conf )
{
  const idclass_t *idc;
  mpegts_mux_t *mm;
  dvb_mux_t *lm;
  htsmsg_t *c, *c2, *e;
  htsmsg_field_t *f;
  dvb_fe_delivery_system_t delsys;
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Class */
  if (ln->ln_type == DVB_TYPE_S) {
    idc = &dvb_mux_dvbs_class;
    delsys = DVB_SYS_DVBS;
  } else if (ln->ln_type == DVB_TYPE_C) {
    idc = &dvb_mux_dvbc_class;
    delsys = DVB_SYS_DVBC_ANNEX_A;
  } else if (ln->ln_type == DVB_TYPE_T) {
    idc = &dvb_mux_dvbt_class;
    delsys = DVB_SYS_DVBT;
  } else if (ln->ln_type == DVB_TYPE_ATSC_T) {
    idc = &dvb_mux_atsc_t_class;
    delsys = DVB_SYS_ATSC;
  } else if (ln->ln_type == DVB_TYPE_ATSC_C) {
    idc = &dvb_mux_atsc_c_class;
    delsys = DVB_SYS_DVBC_ANNEX_B;
  } else if (ln->ln_type == DVB_TYPE_ISDB_T) {
    idc = &dvb_mux_isdb_t_class;
    delsys = DVB_SYS_ISDBT;
  } else if (ln->ln_type == DVB_TYPE_ISDB_C) {
    idc = &dvb_mux_isdb_c_class;
    delsys = DVB_SYS_ISDBC;
  } else if (ln->ln_type == DVB_TYPE_ISDB_S) {
    idc = &dvb_mux_isdb_s_class;
    delsys = DVB_SYS_ISDBS;
  } else if (ln->ln_type == DVB_TYPE_DAB) {
    idc = &dvb_mux_dab_class;
    delsys = DVB_SYS_DAB;
  } else {
    tvherror(LS_DVB, "unknown FE type %d", ln->ln_type);
    return NULL;
  }

  /* Create */
  mm = calloc(1, sizeof(dvb_mux_t));
  lm = (dvb_mux_t*)mm;

  /* Defaults */
  dvb_mux_conf_init((mpegts_network_t*)ln, &lm->lm_tuning, delsys);

  /* Parent init and load config */
  if (!(mm = mpegts_mux_create0(mm, idc, uuid,
                                (mpegts_network_t*)ln, onid, tsid, conf))) {
    free(mm);
    return NULL;
  }

  /* Tuning */
  if (dmc)
    memcpy(&lm->lm_tuning, dmc, sizeof(dvb_mux_conf_t));
  lm->lm_tuning.dmc_fe_type = ln->ln_type;

  /* Callbacks */
  lm->mm_delete           = dvb_mux_delete;
  lm->mm_display_name     = dvb_mux_display_name;
  lm->mm_config_save      = dvb_mux_config_save;
  lm->mm_create_instances = dvb_mux_create_instances;

  /* No config */
  if (!conf) return lm;

  /* Services */
  c2 = NULL;
  c = htsmsg_get_map(conf, "services");
  if (c == NULL)
    c = c2 = hts_settings_load_r(1, "input/dvb/networks/%s/muxes/%s/services",
                                 idnode_uuid_as_str(&ln->mn_id, ubuf1),
                                 idnode_uuid_as_str(&mm->mm_id, ubuf2));

  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      mpegts_service_create1(f->hmf_name, (mpegts_mux_t *)lm, 0, 0, e);
    }
    htsmsg_destroy(c2);
  }

  if (ln->ln_type == DVB_TYPE_S) {
    if (ln->mn_satpos == INT_MAX) {
      /* Update the satellite position for the network settings */
      if (lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos != INT_MAX)
        ln->mn_satpos = lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos;
    }
    else {
      /* Update the satellite position for the mux setting */
      lm->lm_tuning.u.dmc_fe_qpsk.orbital_pos = ln->mn_satpos;
    }
  }

  return lm;
}
