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
#define dvb_mux_class_R(c, f, l, ...)\
static const void * \
dvb_mux_##c##_class_##l##_get (void *o)\
{\
  static const char *s;\
  dvb_mux_t *lm = o;\
  s = dvb_##l##2str(lm->lm_tuning.dmc_fe_##f);\
  return &s;\
}\
static int \
dvb_mux_##c##_class_##l##_set (void *o, const void *v)\
{\
  dvb_mux_t *lm = o;\
  lm->lm_tuning.dmc_fe_##f = dvb_str2##l ((const char*)v);\
  return 1;\
}\
static htsmsg_t *\
dvb_mux_##c##_class_##l##_enum (void *o, const char *lang)\
{\
  static const int     t[] = { __VA_ARGS__ };\
  int i;\
  htsmsg_t *m = htsmsg_create_list();\
  for (i = 0; i < ARRAY_SIZE(t); i++)\
    htsmsg_add_str(m, NULL, tvh_gettext_lang(lang, dvb_##l##2str(t[i])));\
  return m;\
}
#define dvb_mux_class_X(c, f, p, l, ...)\
static const void * \
dvb_mux_##c##_class_##l##_get (void *o)\
{\
  static const char *s;\
  dvb_mux_t *lm = o;\
  s = dvb_##l##2str(lm->lm_tuning.u.dmc_fe_##f.p);\
  return &s;\
}\
static int \
dvb_mux_##c##_class_##l##_set (void *o, const void *v)\
{\
  dvb_mux_t *lm = o;\
  lm->lm_tuning.u.dmc_fe_##f.p = dvb_str2##l ((const char*)v);\
  return 1;\
}\
static htsmsg_t *\
dvb_mux_##c##_class_##l##_enum (void *o, const char *lang)\
{\
  static const int     t[] = { __VA_ARGS__ };\
  int i;\
  htsmsg_t *m = htsmsg_create_list(), *e;\
  for (i = 0; i < ARRAY_SIZE(t); i++) {\
    e = htsmsg_create_map(); \
    htsmsg_add_str(e, "key", dvb_##l##2str(t[i]));\
    htsmsg_add_str(e, "val", tvh_gettext_lang(lang, dvb_##l##2str(t[i])));\
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
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_delsys2str(lm->lm_tuning.dmc_fe_delsys);
  return &s;
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
  .ic_caption    = N_("Linux DVB multiplex"),
  .ic_properties = (const property_t[]){
    {}
  }
};

/*
 * DVB-T
 */

dvb_mux_class_X(dvbt, ofdm, bandwidth,             bw,
                     DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_10_MHZ,
                     DVB_BANDWIDTH_8_MHZ, DVB_BANDWIDTH_7_MHZ,
                     DVB_BANDWIDTH_6_MHZ, DVB_BANDWIDTH_5_MHZ,
                     DVB_BANDWIDTH_1_712_MHZ);
dvb_mux_class_R(dvbt, modulation,                  qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(dvbt, ofdm, transmission_mode,     mode,
                     DVB_TRANSMISSION_MODE_AUTO, DVB_TRANSMISSION_MODE_32K,
                     DVB_TRANSMISSION_MODE_16K, DVB_TRANSMISSION_MODE_8K,
                     DVB_TRANSMISSION_MODE_2K, DVB_TRANSMISSION_MODE_1K);
dvb_mux_class_X(dvbt, ofdm, guard_interval,        guard,
                     DVB_GUARD_INTERVAL_AUTO, DVB_GUARD_INTERVAL_1_32,
                     DVB_GUARD_INTERVAL_1_16, DVB_GUARD_INTERVAL_1_8,
                     DVB_GUARD_INTERVAL_1_4, DVB_GUARD_INTERVAL_1_128,
                     DVB_GUARD_INTERVAL_19_128, DVB_GUARD_INTERVAL_19_256);
dvb_mux_class_X(dvbt, ofdm, hierarchy_information, hier,
                     DVB_HIERARCHY_AUTO, DVB_HIERARCHY_NONE,
                     DVB_HIERARCHY_1, DVB_HIERARCHY_2, DVB_HIERARCHY_4);
dvb_mux_class_X(dvbt, ofdm, code_rate_HP,          fechi,
                     DVB_FEC_AUTO, DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4,
                     DVB_FEC_3_5,  DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8);
dvb_mux_class_X(dvbt, ofdm, code_rate_LP,          feclo,
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
  .ic_caption    = N_("Linux DVB-T multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbt, delsys, "DVBT"),
      .desc     = N_("Select the delivery system the mux uses. "
                     "If you have a DVB-T tuner you must select DVB-T "
                     "here."),
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
      .desc     = N_("Select the bandwidth the mux uses. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dvbt, qam, N_("AUTO")),
      .desc     = N_("Select the COFDM modulation used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("transmission_mode", N_("Transmission mode"), dvbt, mode, N_("AUTO")),
      .desc     = N_("Select the transmission/OFDM mode used by the mux. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("guard_interval", N_("Guard interval"), dvbt, guard, N_("AUTO")),
      .desc     = N_("Select the guard interval used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("hierarchy", N_("Hierarchy"), dvbt, hier, N_("AUTO")),
      .desc     = N_("Select the Hierarchical modulation used by this mux. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_hi", N_("FEC high"), dvbt, fechi, N_("AUTO")),
      .desc     = N_("Select the forward error correction high value. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_lo", N_("FEC low"), dvbt, feclo, N_("AUTO")),
      .desc     = N_("Select the forward error correction low value. "
                     "Most people will not need to change this setting."),
    },
    {
      .type     = PT_INT,
      .id       = "plp_id",
      .name     = N_("PLP ID"),
      .desc     = N_("Select the physical layer pipe ID. "
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

dvb_mux_class_R(dvbc, modulation,                 qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QAM_16, DVB_MOD_QAM_32,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_128, DVB_MOD_QAM_256);
dvb_mux_class_X(dvbc, qam, fec_inner,             fec,
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
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DVBC_ANNEX_C));
  return list;
}

const idclass_t dvb_mux_dvbc_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dvbc",
  .ic_caption    = N_("Linux DVB-C multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbc, delsys, "DVB-C"),
      .desc     = N_("Select the delivery system used by your cable provider."),
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
      .desc     = N_("Select the quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("Select the forward error correction used on the mux."),
    },
    {}
  }
};

dvb_mux_class_X(dvbs, qpsk, fec_inner,             fec,
                     DVB_FEC_AUTO, DVB_FEC_NONE,
                     DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4, DVB_FEC_3_5,
                     DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8, DVB_FEC_8_9,
                     DVB_FEC_9_10);

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

static const void *
dvb_mux_dvbs_class_polarity_get (void *o)
{
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_pol2str(lm->lm_tuning.u.dmc_fe_qpsk.polarisation);
  return &s;
}

static int
dvb_mux_dvbs_class_polarity_set (void *o, const void *s)
{
  dvb_mux_t *lm = o;
  lm->lm_tuning.u.dmc_fe_qpsk.polarisation = dvb_str2pol((const char*)s);
  return 1;
}

static htsmsg_t *
dvb_mux_dvbs_class_polarity_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_pol2str(DVB_POLARISATION_VERTICAL));
  htsmsg_add_str(list, NULL, dvb_pol2str(DVB_POLARISATION_HORIZONTAL));
  htsmsg_add_str(list, NULL, dvb_pol2str(DVB_POLARISATION_CIRCULAR_LEFT));
  htsmsg_add_str(list, NULL, dvb_pol2str(DVB_POLARISATION_CIRCULAR_RIGHT));
  return list;
}

static const void *
dvb_mux_dvbs_class_modulation_get ( void *o )
{
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_qam2str(lm->lm_tuning.dmc_fe_modulation);
  return &s;
}

static int
dvb_mux_dvbs_class_modulation_set (void *o, const void *s)
{
  int mod = dvb_str2qam(s);
  dvb_mux_t *lm = o;
  if (mod != lm->lm_tuning.dmc_fe_modulation) {
    lm->lm_tuning.dmc_fe_modulation = mod;
    return 1;
  }
  return 0;
}

static htsmsg_t *
dvb_mux_dvbs_class_modulation_list ( void *o, const char *lang )
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, tvh_gettext_lang(lang, dvb_qam2str(DVB_MOD_AUTO)));
  htsmsg_add_str(list, NULL, dvb_qam2str(DVB_MOD_QPSK));
  htsmsg_add_str(list, NULL, dvb_qam2str(DVB_MOD_QAM_16));
  htsmsg_add_str(list, NULL, dvb_qam2str(DVB_MOD_PSK_8));
  htsmsg_add_str(list, NULL, dvb_qam2str(DVB_MOD_APSK_16));
  htsmsg_add_str(list, NULL, dvb_qam2str(DVB_MOD_APSK_32));
  return list;
}

static const void *
dvb_mux_dvbs_class_rolloff_get ( void *o )
{
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_rolloff2str(lm->lm_tuning.dmc_fe_rolloff);
  return &s;
}

static int
dvb_mux_dvbs_class_rolloff_set ( void *o, const void *s )
{
  dvb_mux_t *lm = o;
  lm->lm_tuning.dmc_fe_rolloff = dvb_str2rolloff(s);
  return 1;
}

static htsmsg_t *
dvb_mux_dvbs_class_rolloff_list ( void *o, const char *lang )
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_rolloff2str(DVB_ROLLOFF_35));
  // Note: this is a bit naff, as the below values are only relevant
  //       to S2 muxes, but currently have no way to model that
  htsmsg_add_str(list, NULL, dvb_rolloff2str(DVB_ROLLOFF_20));
  htsmsg_add_str(list, NULL, dvb_rolloff2str(DVB_ROLLOFF_25));
  htsmsg_add_str(list, NULL, tvh_gettext_lang(lang, dvb_rolloff2str(DVB_ROLLOFF_AUTO)));
  return list;
}

static const void *
dvb_mux_dvbs_class_pilot_get ( void *o )
{
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_pilot2str(lm->lm_tuning.dmc_fe_pilot);
  return &s;
}

static int
dvb_mux_dvbs_class_pilot_set ( void *o, const void *s )
{
  dvb_mux_t *lm = o;
  lm->lm_tuning.dmc_fe_pilot = dvb_str2pilot(s);
  return 1;
}
static htsmsg_t *
dvb_mux_dvbs_class_pilot_list ( void *o, const char *lang )
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, tvh_gettext_lang(lang, dvb_pilot2str(DVB_PILOT_AUTO)));
  htsmsg_add_str(list, NULL, dvb_pilot2str(DVB_PILOT_ON));
  htsmsg_add_str(list, NULL, dvb_pilot2str(DVB_PILOT_OFF));
  return list;
}

static const void *
dvb_mux_dvbs_class_pls_mode_get ( void *o )
{
  static const char *s;
  dvb_mux_t *lm = o;
  s = dvb_plsmode2str(lm->lm_tuning.dmc_fe_pls_mode);
  return &s;
}

static int
dvb_mux_dvbs_class_pls_mode_set ( void *o, const void *s )
{
  dvb_mux_t *lm = o;
  lm->lm_tuning.dmc_fe_pls_mode = dvb_str2plsmode(s);
  return 1;
}

static htsmsg_t *
dvb_mux_dvbs_class_pls_mode_list ( void *o, const char *lang )
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_plsmode2str(DVB_PLS_ROOT));
  htsmsg_add_str(list, NULL, dvb_plsmode2str(DVB_PLS_GOLD));
  htsmsg_add_str(list, NULL, dvb_plsmode2str(DVB_PLS_COMBO));
  return list;
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
  .ic_caption    = N_("Linux DVB-S multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbs, delsys, "DVBS"),
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
      .type     = PT_U32,
      .id       = "symbolrate",
      .name     = N_("Symbol rate (Sym/s)"),
      .desc     = N_("The symbol rate."),
      .off      = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_qpsk.symbol_rate),
      .set      = dvb_mux_dvbs_class_symbol_rate_set,
    },
    {
      MUX_PROP_STR("polarisation", N_("Polarization"), dvbs, polarity, NULL)
    },
    {
      .type     = PT_STR,
      .id       = "modulation",
      .name     = N_("Modulation"),
      .desc     = N_("The modulation used on the mux."),
      .set      = dvb_mux_dvbs_class_modulation_set,
      .get      = dvb_mux_dvbs_class_modulation_get,
      .list     = dvb_mux_dvbs_class_modulation_list,
      .def.s    = "AUTO",
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbs, fec, "AUTO")
    },
    {
      .type     = PT_STR,
      .id       = "rolloff",
      .name     = N_("Rolloff"),
      .desc     = N_("The mux rolloff. Leave as AUTO unless you know the "
                     "exact rolloff for this mux."),
      .set      = dvb_mux_dvbs_class_rolloff_set,
      .get      = dvb_mux_dvbs_class_rolloff_get,
      .list     = dvb_mux_dvbs_class_rolloff_list,
      .def.s    = "AUTO"
    },
    {
      .type     = PT_STR,
      .id       = "pilot",
      .name     = N_("Pilot"),
      .desc     = N_("Use pilot on this mux. AUTO is the recommended "
                     "value."),
      .opts     = PO_ADVANCED,
      .set      = dvb_mux_dvbs_class_pilot_set,
      .get      = dvb_mux_dvbs_class_pilot_get,
      .list     = dvb_mux_dvbs_class_pilot_list,
    },
    {
      .type     = PT_INT,
      .id       = "stream_id",
      .name     = N_("ISI (Stream ID)"),
      .desc     = N_("The stream ID used for this mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_stream_id),
      .def.i	= DVB_NO_STREAM_ID_FILTER,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "pls_mode",
      .name     = N_("PLS mode"),
      .set      = dvb_mux_dvbs_class_pls_mode_set,
      .get      = dvb_mux_dvbs_class_pls_mode_get,
      .list     = dvb_mux_dvbs_class_pls_mode_list,
      .def.s    = "ROOT",
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "pls_code",
      .name     = N_("PLS code"),
      .desc     = N_("Enter the Physical Layer Scrambling (PLS) code "
                     "used on this mux."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_pls_code),
      .def.u32	= 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "orbital",
      .name     = N_("Orbital position"),
      .desc     = N_("The orbital position of the satellite this mux is on."),
      .set      = dvb_mux_dvbs_class_orbital_set,
      .get      = dvb_mux_dvbs_class_orbital_get,
      .opts     = PO_ADVANCED | PO_RDONLY
    },
    {}
  }
};

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

dvb_mux_class_R(atsc_t, modulation, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QAM_256, DVB_MOD_VSB_8);

const idclass_t dvb_mux_atsc_t_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_atsc_t",
  .ic_caption    = N_("Linux ATSC-T multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), atsc_t, delsys, "ATSC-T"),
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
      MUX_PROP_STR("modulation", N_("Modulation"), atsc_t, qam, N_("AUTO"))
    },
    {}
  }
};

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
  .ic_caption    = N_("Linux ATSC-C multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), atsc_c, delsys, "ATSC-C"),
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
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("AUTO"))
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO"))
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
dvb_mux_config_save ( mpegts_mux_t *mm )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  hts_settings_save(c, "input/dvb/networks/%s/muxes/%s/config",
                    idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
                    idnode_uuid_as_str(&mm->mm_id, ubuf2));
  htsmsg_destroy(c);
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
    if (mi->mi_is_enabled(mi, mm, 0))
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
  htsmsg_t *c, *e;
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
  } else {
    tvherror("dvb", "unknown FE type %d", ln->ln_type);
    return NULL;
  }

  /* Create */
  mm = calloc(1, sizeof(dvb_mux_t));
  lm = (dvb_mux_t*)mm;

  /* Defaults */
  dvb_mux_conf_init(&lm->lm_tuning, delsys);

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
  c = hts_settings_load_r(1, "input/dvb/networks/%s/muxes/%s/services",
                         idnode_uuid_as_str(&ln->mn_id, ubuf1),
                         idnode_uuid_as_str(&mm->mm_id, ubuf2));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      mpegts_service_create1(f->hmf_name, (mpegts_mux_t *)lm, 0, 0, e);
    }
    htsmsg_destroy(c);
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
