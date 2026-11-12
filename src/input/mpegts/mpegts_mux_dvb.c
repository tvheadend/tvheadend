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

/*
 * DAB fields only for their mux type: not in the editor of other muxes, not
 * saved for them (PO_HIDDEN alone only hides a grid column, the forms show
 * it). In the class schema (o == NULL: grid columns, the add form) they are
 * columns hidden by default.
 */
static uint32_t
dvb_mux_class_dab_ip_opts(void *o, uint32_t opts)
{
  const mpegts_mux_t *mm = o;
  (void) opts;
  if (mm == NULL)
    return PO_HIDDEN;
  if (mm->mm_type == MM_TYPE_DAB_MPE)
    return 0;
  return PO_NOUI | PO_NOSAVE;
}

static uint32_t
dvb_mux_class_dab_opts(void *o, uint32_t opts)
{
  const mpegts_mux_t *mm = o;
  if (mm == NULL)
    return opts | PO_HIDDEN;
  if (mpegts_mux_is_dab(mm))
    return opts;
  return opts | PO_NOUI | PO_NOSAVE;
}

static uint32_t
dvb_mux_class_dab_eti_opts(void *o, uint32_t opts)
{
  const mpegts_mux_t *mm = o;
  (void) opts;
  if (mm == NULL)
    return PO_HIDDEN;
  if (mm->mm_type == MM_TYPE_DAB_ETI)
    return 0;
  return PO_NOUI | PO_NOSAVE;
}

/*
 * DAB IP address get/set helpers (string <-> uint32)
 */
static const void *
dvb_mux_class_dab_ip_get(void *o)
{
  static char buf[32];
  static char *s;
  const dvb_mux_t *lm = o;
  uint32_t ip = lm->lm_tuning.dmc_dab_ip;

  s = buf;
  if (ip == 0)
    buf[0] = '\0';
  else
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF, ip & 0xFF);
  return &s;
}

static int
dvb_mux_class_dab_ip_set(void *o, const void *v)
{
  dvb_mux_t *lm = o;
  const char *s = v;
  uint32_t ip = 0;
  unsigned int q[4];

  if (s && *s && sscanf(s, "%u.%u.%u.%u", &q[0], &q[1], &q[2], &q[3]) == 4)
    ip = (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];

  if (ip != lm->lm_tuning.dmc_dab_ip) {
    lm->lm_tuning.dmc_dab_ip = ip;
    return 1;
  }
  return 0;
}

/*
 * Mux type: a sub-mux (DAB) is carried in a PID of its transponder
 */
static htsmsg_t *
dvb_mux_class_type_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("TS"),                       MM_TYPE_TS },
#if ENABLE_DVBDAB
    { N_("DAB-MPE"),                  MM_TYPE_DAB_MPE },
    { N_("DAB-ETI"),                  MM_TYPE_DAB_ETI },
    { N_("DAB-TSNI"),                 MM_TYPE_DAB_TSNI },
#endif
  };
  (void) o;
  return strtab2htsmsg(tab, 1, lang);
}

/* the stream PID only for a sub-mux carried in a PID */
static uint32_t
dvb_mux_class_stream_pid_opts ( void *o, uint32_t opts )
{
  const mpegts_mux_t *mm = o;
  (void) opts;
  if (mm == NULL)
    return PO_HIDDEN;
  if (mm->mm_type == MM_TYPE_DAB_MPE ||
      mm->mm_type == MM_TYPE_DAB_ETI ||
      mm->mm_type == MM_TYPE_DAB_TSNI)
    return 0;
  return PO_NOUI | PO_NOSAVE;
}

/* the transponder of a sub-mux: internal, saved for sub-muxes only */
static uint32_t
dvb_mux_class_parent_opts ( void *o, uint32_t opts )
{
  const mpegts_mux_t *mm = o;
  if (mm == NULL)
    return opts | PO_HIDDEN;
  if (mpegts_mux_is_submux(mm))
    return opts | PO_NOUI;
  return opts | PO_NOUI | PO_NOSAVE;
}

const idclass_t dvb_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "dvb_mux",
  .ic_caption    = N_("DVB multiplex"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "tstype",
      .name     = N_("Type"),
      .desc     = N_("Transport stream type: TS (standard) or DAB."),
      .off      = offsetof(mpegts_mux_t, mm_type),
      .def.i    = MM_TYPE_TS,
      .list     = dvb_mux_class_type_list,
      .opts     = PO_DOC_NLIST
    },
    {
      .type     = PT_U16,
      .id       = "pid",
      .name     = N_("Stream PID"),
      .desc     = N_("PID containing the encapsulated stream (DAB)."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_pid),
      .get_opts = dvb_mux_class_stream_pid_opts,
    },
    {
      .type     = PT_STR,
      .id       = "parent_mux",
      .name     = N_("Transponder"),
      .desc     = N_("The mux a sub-mux is carried in: its tuning follows "
                     "that of the transponder."),
      .off      = offsetof(dvb_mux_t, lm_parent),
      .opts     = PO_RDONLY,
      .get_opts = dvb_mux_class_parent_opts,
    },
    {
      .type     = PT_STR,
      .id       = "dab_ensemble",
      .name     = N_("Ensemble"),
      .desc     = N_("The label of the DAB ensemble, from the scan."),
      .off      = offsetof(dvb_mux_t, lm_dab_ensemble),
      .opts     = PO_RDONLY,
      .get_opts = dvb_mux_class_dab_opts,
    },
    {
      .type     = PT_STR,
      .id       = "dab_ip",
      .name     = N_("DAB IP"),
      .desc     = N_("Multicast IP address for DAB-MPE/GSE."),
      .get      = dvb_mux_class_dab_ip_get,
      .set      = dvb_mux_class_dab_ip_set,
      .get_opts = dvb_mux_class_dab_ip_opts,
    },
    {
      .type     = PT_U16,
      .id       = "dab_port",
      .name     = N_("DAB Port"),
      .desc     = N_("UDP port for DAB-MPE/GSE."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_dab_port),
      .get_opts = dvb_mux_class_dab_ip_opts,
    },
    {
      .type     = PT_INT,
      .id       = "dab_eti_padding",
      .name     = N_("ETI Padding"),
      .desc     = N_("Leading 0xFF bytes in ETI-NA."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_dab_eti_padding),
      .get_opts = dvb_mux_class_dab_eti_opts,
    },
    {
      .type     = PT_INT,
      .id       = "dab_eti_offset",
      .name     = N_("ETI Bit Offset"),
      .desc     = N_("Bit position of E1 sync in ETI-NA."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_dab_eti_bit_offset),
      .get_opts = dvb_mux_class_dab_eti_opts,
    },
    {
      .type     = PT_BOOL,
      .id       = "dab_eti_inverted",
      .name     = N_("ETI Inverted"),
      .desc     = N_("Signal is bit-inverted in ETI-NA."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_dab_eti_inverted),
      .get_opts = dvb_mux_class_dab_eti_opts,
    },
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
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbt, delsys, "DVB-T"),
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
      MUX_PROP_STR("constellation", N_("Constellation"), dvbt, qam, N_("QAM/AUTO")),
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
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("QAM/AUTO")),
      .desc     = N_("The quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("The forward error correction used on the mux."),
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
    {
      .type     = PT_U32,
      .id       = "data_slice",
      .name     = N_("Data slice"),
      .desc     = N_("Data slice code."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_data_slice),
      .def.u32	= 0,
      .opts     = PO_EXPERT
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
                     DVB_FEC_9_10, DVB_FEC_1_3, DVB_FEC_1_4, DVB_FEC_4_15,
                     DVB_FEC_5_9, DVB_FEC_7_9, DVB_FEC_7_15, DVB_FEC_8_15,
                     DVB_FEC_9_20, DVB_FEC_11_15, DVB_FEC_11_20, DVB_FEC_11_45,
                     DVB_FEC_13_18, DVB_FEC_13_45, DVB_FEC_14_45, DVB_FEC_23_36,
                     DVB_FEC_25_36, DVB_FEC_26_45, DVB_FEC_28_45, DVB_FEC_32_45,
                     DVB_FEC_77_90);
dvb_mux_class_R(dvbs, modulation, qam, qam,
                     DVB_MOD_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16, DVB_MOD_QAM_1024,
                     DVB_MOD_QAM_4096, DVB_MOD_PSK_8, DVB_MOD_APSK_16, DVB_MOD_APSK_32,
                     DVB_MOD_APSK_64, DVB_MOD_APSK_8_L, DVB_MOD_APSK_16_L,
                     DVB_MOD_APSK_32_L, DVB_MOD_APSK_64_L);
dvb_mux_class_R(dvbs, rolloff, rolloff, rolloff,
                     DVB_HIERARCHY_AUTO, DVB_ROLLOFF_5, DVB_ROLLOFF_10, DVB_ROLLOFF_15,
                     DVB_ROLLOFF_20, DVB_ROLLOFF_25, DVB_ROLLOFF_35);
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
      MUX_PROP_STR("delsys", N_("Delivery system"), dvbs, delsys, "DVB-S"),
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
    {
      .type     = PT_U32,
      .id       = "dvb_satip_dvbc_freq",
      .name     = N_("SAT>IP DVB-C frequency (Hz)"),
      .off      = offsetof(dvb_mux_t, mm_dvb_satip_dvbc_freq),
      .desc     = N_("For example: 312000000. This frequency is 312Mhz."),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "dvb_satip_dvbt_freq",
      .name     = N_("SAT>IP DVB-T frequency (Hz)"),
      .off      = offsetof(dvb_mux_t, mm_dvb_satip_dvbt_freq),
      .desc     = N_("For example: 312000000. This frequency is 312Mhz."),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "dvb_satip_dvbs_freq",
      .name     = N_("SAT>IP DVB-S frequency (kHz)"),
      .off      = offsetof(dvb_mux_t, mm_dvb_satip_dvbs_freq),
      .desc     = N_("Virtual DVB-S frequency in kHz for SAT>IP clients. "
                     "When set, SAT>IP clients tune to this mux using "
                     "this frequency instead of the real transponder frequency. "
                     "Required for DVB-S2X MIS muxes sharing the same "
                     "physical frequency with different ISI values. "
                     "Example: 10701000 (= 10701 MHz)."),
      .opts     = PO_ADVANCED
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
      MUX_PROP_STR("modulation", N_("Modulation"), atsc_t, qam, N_("QAM/AUTO")),
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
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("QAM/AUTO")),
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
 * CableCARD
 */
const idclass_t dvb_mux_cablecard_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_cablecard",
  .ic_caption    = N_("CableCARD multiplex"),
  .ic_properties = (const property_t[]){
    {
      .type = PT_U32,
      .id   = "vchan",
      .name = N_("Channel"),
      .desc = N_("The channel on the cable provider's network."),
      .off  = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_cablecard.vchannel),
    },
    {
      .type = PT_U32,
      .id   = "frequency",
      .name = N_("Frequency (Hz)"),
      .desc = N_("The frequency of the mux (in Hertz)."),
      .off  = offsetof(dvb_mux_t, lm_tuning.dmc_fe_freq),
      .opts = PO_RDONLY,
    },
    {
      .type = PT_STR,
      .id   = "vchan_name",
      .name = N_("Callsign"),
      .desc = N_("The channel's name or callsign as set by the cable provider."),
      .off  = offsetof(dvb_mux_t, lm_tuning.u.dmc_fe_cablecard.name),
      .opts = PO_RDONLY,
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
      MUX_PROP_STR("layera_mod", N_("Layer A: Constellation"), isdb_t, isdbt_mod_a, N_("QAM/AUTO")),
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
      MUX_PROP_STR("layerb_mod", N_("Layer B: Constellation"), isdb_t, isdbt_mod_b, N_("QAM/AUTO")),
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
      MUX_PROP_STR("layerc_mod", N_("Layer C: Constellation"), isdb_t, isdbt_mod_c, N_("QAM/AUTO")),
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
      MUX_PROP_STR("constellation", N_("Constellation"), dvbc, qam, N_("QAM/AUTO")),
      .desc     = N_("The quadrature amplitude modulation (QAM) used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("fec", N_("FEC"), dvbc, fec, N_("AUTO")),
      .desc     = N_("The forward error correction used on the mux."),
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
    {
      .type     = PT_U32,
      .id       = "data_slice",
      .name     = N_("Data slice"),
      .desc     = N_("Data slice code."),
      .off      = offsetof(dvb_mux_t, lm_tuning.dmc_fe_data_slice),
      .def.u32	= 0,
      .opts     = PO_EXPERT
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
      MUX_PROP_STR("delsys", N_("Delivery system"), isdb_s, delsys, "ISDB-S"),
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
 * DTMB, fixme: review actually used parameters!
 */

dvb_mux_class_X(dtmb, ofdm, bandwidth, bw, bw,
                     DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_10_MHZ,
                     DVB_BANDWIDTH_8_MHZ, DVB_BANDWIDTH_7_MHZ,
                     DVB_BANDWIDTH_6_MHZ, DVB_BANDWIDTH_5_MHZ,
                     DVB_BANDWIDTH_1_712_MHZ);
dvb_mux_class_R(dtmb, modulation, qam, qam,
                     DVB_MOD_QAM_AUTO, DVB_MOD_QPSK, DVB_MOD_QAM_16,
                     DVB_MOD_QAM_64, DVB_MOD_QAM_256);
dvb_mux_class_X(dtmb, ofdm, transmission_mode, mode, mode,
                     DVB_TRANSMISSION_MODE_AUTO, DVB_TRANSMISSION_MODE_32K,
                     DVB_TRANSMISSION_MODE_16K, DVB_TRANSMISSION_MODE_8K,
                     DVB_TRANSMISSION_MODE_2K, DVB_TRANSMISSION_MODE_1K);
dvb_mux_class_X(dtmb, ofdm, guard_interval, guard, guard,
                     DVB_GUARD_INTERVAL_AUTO, DVB_GUARD_INTERVAL_1_32,
                     DVB_GUARD_INTERVAL_1_16, DVB_GUARD_INTERVAL_1_8,
                     DVB_GUARD_INTERVAL_1_4, DVB_GUARD_INTERVAL_1_128,
                     DVB_GUARD_INTERVAL_19_128, DVB_GUARD_INTERVAL_19_256);
dvb_mux_class_X(dtmb, ofdm, hierarchy_information, hier, hier,
                     DVB_HIERARCHY_AUTO, DVB_HIERARCHY_NONE,
                     DVB_HIERARCHY_1, DVB_HIERARCHY_2, DVB_HIERARCHY_4);
dvb_mux_class_X(dtmb, ofdm, code_rate_HP, fechi, fechi,
                     DVB_FEC_AUTO, DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4,
                     DVB_FEC_3_5,  DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8);
dvb_mux_class_X(dtmb, ofdm, code_rate_LP, feclo, feclo,
                     DVB_FEC_AUTO, DVB_FEC_1_2, DVB_FEC_2_3, DVB_FEC_3_4,
                     DVB_FEC_3_5,  DVB_FEC_4_5, DVB_FEC_5_6, DVB_FEC_7_8);

#define dvb_mux_dtmb_class_delsys_get dvb_mux_class_delsys_get
#define dvb_mux_dtmb_class_delsys_set dvb_mux_class_delsys_set

static htsmsg_t *
dvb_mux_dtmb_class_delsys_enum (void *o, const char *lang)
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_add_str(list, NULL, dvb_delsys2str(DVB_SYS_DTMB));
  return list;
}

static int
dvb_mux_dtmb_class_frequency_set ( void *o, const void *v )
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

const idclass_t dvb_mux_dtmb_class =
{
  .ic_super      = &dvb_mux_class,
  .ic_class      = "dvb_mux_dtmb",
  .ic_caption    = N_("DTMB multiplex"),
  .ic_properties = (const property_t[]){
    {
      MUX_PROP_STR("delsys", N_("Delivery system"), dtmb, delsys, "DTMB"),
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
      .set      = dvb_mux_dtmb_class_frequency_set,
    },
    {
      MUX_PROP_STR("bandwidth", N_("Bandwidth"), dtmb, bw, N_("AUTO")),
      .desc     = N_("The bandwidth the mux uses. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("constellation", N_("Constellation"), dtmb, qam, N_("QAM/AUTO")),
      .desc     = N_("The COFDM modulation used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("transmission_mode", N_("Transmission mode"), dtmb, mode, N_("AUTO")),
      .desc     = N_("The transmission/OFDM mode used by the mux. "
                     "If you're not sure of the value leave as AUTO "
                     "but be aware that tuning may fail as some drivers "
                     "do not like the AUTO setting."),
    },
    {
      MUX_PROP_STR("guard_interval", N_("Guard interval"), dtmb, guard, N_("AUTO")),
      .desc     = N_("The guard interval used by the mux. "
                     "If you're not sure of the value leave as AUTO."),
    },
    {
      MUX_PROP_STR("hierarchy", N_("Hierarchy"), dtmb, hier, N_("AUTO")),
      .desc     = N_("The hierarchical modulation used by the mux. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_hi", N_("FEC high"), dtmb, fechi, N_("AUTO")),
      .desc     = N_("The forward error correction high value. "
                     "Most people will not need to change this setting."),
    },
    {
      MUX_PROP_STR("fec_lo", N_("FEC low"), dtmb, feclo, N_("AUTO")),
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

/*
 * Sub-muxes (DAB) are carried in a PID of a transponder and have its
 * tuning. They keep the uuid of the transponder and follow its tuning when
 * it changes (NIT, blind scan, the user): at its save, and when they start.
 * All with global_lock.
 */

/* the same transponder: what a sub-mux copied from it */
static int
dvb_mux_same_transponder ( const dvb_mux_conf_t *a, const dvb_mux_conf_t *b )
{
  if (a->dmc_fe_type != b->dmc_fe_type ||
      a->dmc_fe_freq != b->dmc_fe_freq ||
      a->dmc_fe_stream_id != b->dmc_fe_stream_id)
    return 0;
  if (a->dmc_fe_type == DVB_TYPE_S &&
      a->u.dmc_fe_qpsk.polarisation != b->u.dmc_fe_qpsk.polarisation)
    return 0;
  return 1;
}

uint16_t
dvb_mux_stream_pid ( mpegts_mux_t *mm )
{
  if (!mpegts_mux_is_submux(mm))
    return 0;
  return ((dvb_mux_t *)mm)->lm_tuning.dmc_fe_pid;
}

void
dvb_mux_submux_link ( dvb_mux_t *sm, const dvb_mux_t *parent )
{
  char ubuf[UUID_HEX_SIZE];
  const char *u = idnode_uuid_as_str(&parent->mm_id, ubuf);

  if (sm->lm_parent && !strcmp(sm->lm_parent, u))
    return;
  tvh_str_update(&sm->lm_parent, u);
  idnode_changed(&sm->mm_id);
}

/* the transponder of a sub-mux; a configuration from before the link (or
 * with the transponder gone) takes the one it was created from */
dvb_mux_t *
dvb_mux_submux_parent ( dvb_mux_t *sm )
{
  mpegts_mux_t *mm = (mpegts_mux_t *)sm;
  mpegts_mux_t *p;

  if (sm->lm_parent) {
    p = mpegts_mux_find(sm->lm_parent);
    if (p != NULL && p != mm && p->mm_network == mm->mm_network &&
        !mpegts_mux_is_submux(p) && idnode_is_instance(&p->mm_id, &dvb_mux_class))
      return (dvb_mux_t *)p;
  }
  LIST_FOREACH(p, &mm->mm_network->mn_muxes, mm_network_link)
    if (p != mm && !mpegts_mux_is_submux(p) &&
        dvb_mux_same_transponder(&((dvb_mux_t *)p)->lm_tuning, &sm->lm_tuning)) {
      dvb_mux_submux_link(sm, (dvb_mux_t *)p);
      return (dvb_mux_t *)p;
    }
  return NULL;
}

/* the same tuning, member by member (the struct has padding); the types with
 * more than their delivery system parameters are never taken for the same */
static int
dvb_mux_conf_same ( const dvb_mux_conf_t *a, const dvb_mux_conf_t *b )
{
  if (a->dmc_fe_type != b->dmc_fe_type ||
      a->dmc_fe_delsys != b->dmc_fe_delsys ||
      a->dmc_fe_modulation != b->dmc_fe_modulation ||
      a->dmc_fe_freq != b->dmc_fe_freq ||
      a->dmc_fe_inversion != b->dmc_fe_inversion ||
      a->dmc_fe_rolloff != b->dmc_fe_rolloff ||
      a->dmc_fe_pilot != b->dmc_fe_pilot ||
      a->dmc_fe_stream_id != b->dmc_fe_stream_id ||
      a->dmc_fe_pls_mode != b->dmc_fe_pls_mode ||
      a->dmc_fe_pls_code != b->dmc_fe_pls_code ||
      a->dmc_fe_data_slice != b->dmc_fe_data_slice ||
      a->dmc_fe_pid != b->dmc_fe_pid ||
      a->dmc_dab_ip != b->dmc_dab_ip ||
      a->dmc_dab_port != b->dmc_dab_port ||
      a->dmc_dab_eti_padding != b->dmc_dab_eti_padding ||
      a->dmc_dab_eti_bit_offset != b->dmc_dab_eti_bit_offset ||
      a->dmc_dab_eti_inverted != b->dmc_dab_eti_inverted)
    return 0;
  switch (a->dmc_fe_type) {
  case DVB_TYPE_S:
    return a->u.dmc_fe_qpsk.polarisation == b->u.dmc_fe_qpsk.polarisation &&
           a->u.dmc_fe_qpsk.orbital_pos == b->u.dmc_fe_qpsk.orbital_pos &&
           a->u.dmc_fe_qpsk.symbol_rate == b->u.dmc_fe_qpsk.symbol_rate &&
           a->u.dmc_fe_qpsk.fec_inner == b->u.dmc_fe_qpsk.fec_inner;
  case DVB_TYPE_C:
    return a->u.dmc_fe_qam.symbol_rate == b->u.dmc_fe_qam.symbol_rate &&
           a->u.dmc_fe_qam.fec_inner == b->u.dmc_fe_qam.fec_inner;
  case DVB_TYPE_T:
    return a->u.dmc_fe_ofdm.bandwidth == b->u.dmc_fe_ofdm.bandwidth &&
           a->u.dmc_fe_ofdm.code_rate_HP == b->u.dmc_fe_ofdm.code_rate_HP &&
           a->u.dmc_fe_ofdm.code_rate_LP == b->u.dmc_fe_ofdm.code_rate_LP &&
           a->u.dmc_fe_ofdm.transmission_mode == b->u.dmc_fe_ofdm.transmission_mode &&
           a->u.dmc_fe_ofdm.guard_interval == b->u.dmc_fe_ofdm.guard_interval &&
           a->u.dmc_fe_ofdm.hierarchy_information == b->u.dmc_fe_ofdm.hierarchy_information;
  default:
    return 0;
  }
}

/* the tuning of the transponder, except what makes this a sub-mux; 1 = changed */
int
dvb_mux_submux_sync ( dvb_mux_t *sm )
{
  dvb_mux_t *p = dvb_mux_submux_parent(sm);
  dvb_mux_conf_t dmc;

  if (p == NULL)
    return 0;
  memcpy(&dmc, &p->lm_tuning, sizeof(dmc));
  dmc.dmc_fe_pid = sm->lm_tuning.dmc_fe_pid;
  dmc.dmc_dab_ip = sm->lm_tuning.dmc_dab_ip;
  dmc.dmc_dab_port = sm->lm_tuning.dmc_dab_port;
  dmc.dmc_dab_eti_padding = sm->lm_tuning.dmc_dab_eti_padding;
  dmc.dmc_dab_eti_bit_offset = sm->lm_tuning.dmc_dab_eti_bit_offset;
  dmc.dmc_dab_eti_inverted = sm->lm_tuning.dmc_dab_eti_inverted;
  if (dvb_mux_conf_same(&dmc, &sm->lm_tuning))
    return 0;
  tvhinfo(LS_MPEGTS, "%s: tuning of the transponder %s taken over",
          sm->mm_nicename, p->mm_nicename);
  memcpy(&sm->lm_tuning, &dmc, sizeof(dmc));
  mpegts_mux_update_nice_name((mpegts_mux_t *)sm);
  idnode_changed(&sm->mm_id);
  return 1;
}

/* a sub-mux of the transponder, NULL if it has none */
static mpegts_mux_t *
dvb_mux_first_submux ( dvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t *)lm;
  mpegts_mux_t *p;

  LIST_FOREACH(p, &mm->mm_network->mn_muxes, mm_network_link)
    if (mpegts_mux_is_submux(p) && dvb_mux_submux_parent((dvb_mux_t *)p) == lm)
      return p;
  return NULL;
}

/* the transponder has changed: its sub-muxes follow */
static void
dvb_mux_submux_propagate ( dvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t *)lm;
  mpegts_mux_t *p;

  LIST_FOREACH(p, &mm->mm_network->mn_muxes, mm_network_link)
    if (mpegts_mux_is_submux(p) && dvb_mux_submux_parent((dvb_mux_t *)p) == lm)
      dvb_mux_submux_sync((dvb_mux_t *)p);
}

static htsmsg_t *
dvb_mux_config_save ( mpegts_mux_t *mm, char *filename, size_t fsize )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  if (filename == NULL) {
    mpegts_mux_save(mm, c, 1);
  } else {
    if (!mpegts_mux_is_submux(mm))
      dvb_mux_submux_propagate((dvb_mux_t *)mm);
    mpegts_mux_save(mm, c, 0);
    snprintf(filename, fsize, "input/dvb/networks/%s/muxes/%s",
             idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
             idnode_uuid_as_str(&mm->mm_id, ubuf2));
  }
  /* a TS mux is saved as upstream saves it: without a type (none = TS) */
  if (mm->mm_type == MM_TYPE_TS)
    htsmsg_delete_field(c, "tstype");
  return c;
}

/* "<transponder> <ensemble>[ / PID <n>]" */
static void
dvb_mux_submux_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  dvb_mux_t *lm = (dvb_mux_t*)mm;
  const dvb_mux_conf_t *dmc = &lm->lm_tuning;
  size_t l = strlen(buf);
  uint32_t ip = dmc->dmc_dab_ip;

  if (mm->mm_type == MM_TYPE_TS || l + 1 >= len)
    return;
  buf += l;
  len -= l;
  if (mpegts_mux_is_dab(mm)) {
    /* before the first scan: where the ensemble is */
    if (lm->lm_dab_ensemble && *lm->lm_dab_ensemble) {
      /* the same ensemble can come in several PIDs */
      if (mm->mm_type == MM_TYPE_DAB_ETI || mm->mm_type == MM_TYPE_DAB_TSNI)
        snprintf(buf, len, " %s / PID %u", lm->lm_dab_ensemble, dmc->dmc_fe_pid);
      else
        snprintf(buf, len, " %s", lm->lm_dab_ensemble);
    } else if (mm->mm_type == MM_TYPE_DAB_MPE)
      snprintf(buf, len, " DAB-MPE %u.%u.%u.%u:%u", (ip >> 24) & 0xFF,
               (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, dmc->dmc_dab_port);
    else if (mm->mm_type == MM_TYPE_DAB_ETI)
      snprintf(buf, len, " ETI-NA %u", dmc->dmc_fe_pid);
    else
      snprintf(buf, len, " DAB-TSNI %u", dmc->dmc_fe_pid);
  }
}

static void
dvb_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  dvb_mux_t *lm = (dvb_mux_t*)mm;
  dvb_network_t *ln = (dvb_network_t*)mm->mm_network;
  uint32_t freq = lm->lm_tuning.dmc_fe_freq, freq2;
  char extra[8], buf2[5], *p;

  if (lm->lm_tuning.dmc_fe_type == DVB_TYPE_CABLECARD)
    snprintf(buf, len, "%u", lm->lm_tuning.u.dmc_fe_cablecard.vchannel);
  else {
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

  /* a sub-mux (or DAB) has the tuning of its transponder: add what tells
   * it apart */
  dvb_mux_submux_name(mm, buf, len);
}

#if ENABLE_DVBDAB
/*
 * The label of a DAB ensemble: from the probe, or from the provider of the
 * services in the SDT of the converted TS (libdvbdab) - global_lock
 */
int
dvb_mux_set_dab_ensemble ( mpegts_mux_t *mm, const char *label )
{
  dvb_mux_t *lm = (dvb_mux_t *)mm;

  if (!mpegts_mux_is_dab(mm) || label == NULL || *label == '\0' ||
      !strcmp(lm->lm_dab_ensemble ?: "", label))
    return 0;
  tvh_str_update(&lm->lm_dab_ensemble, label);
  mpegts_mux_update_nice_name(mm);
  idnode_changed(&mm->mm_id);
  return 1;
}
#endif

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

  /* a sub-mux has the tuning of its transponder in its last save */
  if (!delconf && mpegts_mux_is_submux(mm))
    dvb_mux_submux_sync((dvb_mux_t *)mm);

  /* the sub-muxes of a transponder go with it */
  if (!mpegts_mux_is_submux(mm)) {
    mpegts_mux_t *sub;
    while ((sub = dvb_mux_first_submux((dvb_mux_t *)mm)) != NULL)
      sub->mm_delete(sub, delconf);
  }

  /* Delete the mux (the strings stay for its last save, see dvb_mux_free) */
  mpegts_mux_delete(mm, delconf);
}

static void
dvb_mux_free ( mpegts_mux_t *mm )
{
  dvb_mux_t *lm = (dvb_mux_t *)mm;

  free(lm->lm_parent);
  free(lm->lm_dab_ensemble);
  mpegts_mux_free(mm);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

dvb_mux_t *
dvb_mux_create0
  ( dvb_network_t *ln,
    uint32_t onid, uint32_t tsid, const dvb_mux_conf_t *dmc,
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
  } else if (ln->ln_type == DVB_TYPE_CABLECARD) {
    idc = &dvb_mux_cablecard_class;
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
  } else if (ln->ln_type == DVB_TYPE_DTMB) {
    idc = &dvb_mux_dtmb_class;
    delsys = DVB_SYS_DTMB;
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
  lm->mm_free             = dvb_mux_free;
  lm->mm_display_name     = dvb_mux_display_name;
  lm->mm_config_save      = dvb_mux_config_save;

  mpegts_mux_update_nice_name(mm);

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
      mpegts_service_create1(htsmsg_field_name(f), (mpegts_mux_t *)lm, 0, 0, e);
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
