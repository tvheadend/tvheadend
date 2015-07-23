/*
 *  MPEG TS Program Specific Information code
 *  Copyright (C) 2007 - 2010 Andreas Öman
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
#include "dvb.h"
#include "tsdemux.h"
#include "parsers.h"
#include "lang_codes.h"
#include "service.h"
#include "dvb_charset.h"
#include "bouquet.h"
#include "fastscan.h"
#include "descrambler/caid.h"
#include "descrambler/dvbcam.h"
#include "tvhtime.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define PRIV_FSAT (('F' << 24) | ('S' << 16) | ('A' << 8) | 'T')

#define DVB_FASTSCAN_MUXES (8192/UUID_HEX_SIZE)

typedef struct dvb_freesat_svc {
  TAILQ_ENTRY(dvb_freesat_svc) link;
  TAILQ_ENTRY(dvb_freesat_svc) region_link;
  uint16_t sid;
  uint16_t regionid;
  uint16_t lcn;
  mpegts_service_t *svc;
} dvb_freesat_svc_t;

typedef struct dvb_freesat_region {
  LIST_ENTRY(dvb_freesat_region) link;
  TAILQ_HEAD(,dvb_freesat_svc) services;
  uint16_t regionid;
  char name[52];
  bouquet_t *bouquet;
} dvb_freesat_region_t;

typedef struct dvb_bat_svc {
  TAILQ_ENTRY(dvb_bat_svc) link;
  mpegts_service_t *svc;
  uint8_t lcn_dtag;
  uint32_t lcn;
  dvb_freesat_svc_t *fallback;
  int used;
} dvb_bat_svc_t;

typedef struct dvb_bat_id {
  LIST_ENTRY(dvb_bat_id) link;
  uint32_t complete:1;
  uint32_t freesat:1;
  uint32_t bskyb:1;
  uint16_t nbid;
  char name[32];
  mpegts_mux_t *mm;
  TAILQ_HEAD(,dvb_bat_svc)       services;
  TAILQ_HEAD(,dvb_freesat_svc)   fservices;
  LIST_HEAD(,dvb_freesat_region) fregions;
} dvb_bat_id_t;

typedef struct dvb_bat {
  int complete;
  LIST_HEAD(,dvb_bat_id)          bats;
} dvb_bat_t;

int dvb_bouquets_parse = 1;

static int
psi_parse_pmt(mpegts_mux_t *mux, mpegts_service_t *t, const uint8_t *ptr, int len);

static inline int
mpegts_mux_alive(mpegts_mux_t *mm)
{
  /*
   * Return, if mux seems to be alive for updating.
   */
  return !LIST_EMPTY(&mm->mm_services) && mm->mm_scan_result != MM_SCAN_FAIL;
}

static void
dvb_bouquet_comment ( bouquet_t *bq, mpegts_mux_t *mm )
{
  char comment[128];

  if (bq->bq_comment && bq->bq_comment[0])
    return;
  free(bq->bq_comment);
  mpegts_mux_nice_name(mm, comment, sizeof(comment));
  bq->bq_comment = strdup(comment);
  bq->bq_saveflag = 1;
}

#if ENABLE_MPEGTS_DVB
static mpegts_mux_t *
dvb_fs_mux_find ( mpegts_mux_t *mm, uint16_t onid, uint16_t tsid )
{
  mpegts_mux_t *mux;
  char *s;
  int i;

  if (mm->mm_fastscan_muxes == NULL)
    return NULL;
  for (i = 0; i < DVB_FASTSCAN_MUXES * UUID_HEX_SIZE; i += UUID_HEX_SIZE) {
    s = mm->mm_fastscan_muxes + i;
    if (s[0] == '\0')
      return NULL;
    mux = mpegts_mux_find(s);
    if (mux && mux->mm_onid == onid && mux->mm_tsid == tsid)
      return mux;
  }
  return NULL;
}

static void
dvb_fs_mux_add ( mpegts_table_t *mt, mpegts_mux_t *mm, mpegts_mux_t *mux )
{
  const char *uuid;
  char *s;
  int i;

  uuid = idnode_uuid_as_str(&mux->mm_id);
  if (mm->mm_fastscan_muxes == NULL)
    mm->mm_fastscan_muxes = calloc(DVB_FASTSCAN_MUXES, UUID_HEX_SIZE);
  for (i = 0; i < DVB_FASTSCAN_MUXES * UUID_HEX_SIZE; i += UUID_HEX_SIZE) {
    s = mm->mm_fastscan_muxes + i;
    if (s[0] == '\0')
      break;
    if (strcmp(s, uuid) == 0)
      return;
  }
  for (i = 0; i < DVB_FASTSCAN_MUXES * UUID_HEX_SIZE; i += UUID_HEX_SIZE) {
    s = mm->mm_fastscan_muxes + i;
    if (s[0] == '\0') {
      strcpy(s, uuid);
      return;
    }
  }
  tvherror(mt->mt_name, "fastscan mux count overflow");
}
#endif

/* **************************************************************************
 * Descriptors
 * *************************************************************************/

#if ENABLE_MPEGTS_DVB

/**
 * Tables for delivery descriptor parsing
 */
static const dvb_fe_code_rate_t fec_tab [16] = {
  DVB_FEC_AUTO,   DVB_FEC_1_2,   DVB_FEC_2_3,   DVB_FEC_3_4,
  DVB_FEC_5_6,    DVB_FEC_7_8,   DVB_FEC_8_9,   DVB_FEC_3_5,
  DVB_FEC_4_5,    DVB_FEC_9_10,  DVB_FEC_NONE,  DVB_FEC_NONE,
  DVB_FEC_NONE,   DVB_FEC_NONE,  DVB_FEC_NONE,  DVB_FEC_NONE
};

/*
 * Satellite delivery descriptor
 */
static mpegts_mux_t *
dvb_desc_sat_del
  (mpegts_table_t *mt, mpegts_mux_t *mm,
   uint16_t onid, uint16_t tsid,
   const uint8_t *ptr, int len, int force )
{
  int frequency, symrate;
  dvb_mux_conf_t dmc;
  char buf[128];

  /* Not enough data */
  if(len < 11) return NULL;

  /* Extract data */
  frequency = 
    (bcdtoint(ptr[0]) * 1000000 + bcdtoint(ptr[1]) * 10000 + 
     bcdtoint(ptr[2]) * 100     + bcdtoint(ptr[3])) * 10;
  symrate =
    bcdtoint(ptr[7]) * 100000 + bcdtoint(ptr[8]) * 1000 + 
    bcdtoint(ptr[9]) * 10     + (ptr[10] >> 4);
  if (!frequency) {
    tvhlog(LOG_WARNING, mt->mt_name, "dvb-s frequency error");
    return NULL;
  }
  if (!symrate) {
    tvhlog(LOG_WARNING, mt->mt_name, "dvb-s symbol rate error");
    return NULL;
  }

  dvb_mux_conf_init(&dmc, (ptr[6] & 0x4) ? DVB_SYS_DVBS2 : DVB_SYS_DVBS);

  dmc.dmc_fe_freq                = frequency;
  dmc.u.dmc_fe_qpsk.orbital_pos  = bcdtoint(ptr[4]) * 100 + bcdtoint(ptr[5]);
  if ((ptr[6] & 0x80) == 0)
    dmc.u.dmc_fe_qpsk.orbital_pos *= -1;
  dmc.u.dmc_fe_qpsk.polarisation = (ptr[6] >> 5) & 0x03;

  dmc.u.dmc_fe_qpsk.symbol_rate  = symrate * 100;
  dmc.u.dmc_fe_qpsk.fec_inner    = fec_tab[ptr[10] & 0x0f];
  
  static int mtab[4] = {
    DVB_MOD_NONE, DVB_MOD_QPSK, DVB_MOD_PSK_8, DVB_MOD_QAM_16
  };
  static int rtab[4] = {
    DVB_ROLLOFF_35, DVB_ROLLOFF_25, DVB_ROLLOFF_20, DVB_ROLLOFF_AUTO
  };
  dmc.dmc_fe_modulation = mtab[ptr[6] & 0x3];
  if (dmc.dmc_fe_modulation != DVB_MOD_NONE &&
      dmc.dmc_fe_modulation != DVB_MOD_QPSK)
    /* standard DVB-S allows only QPSK */
    /* on 13.0E, there are (ptr[6] & 4) == 0 muxes with 8PSK and DVB-S2 */
    dmc.dmc_fe_delsys = DVB_SYS_DVBS2;
  dmc.dmc_fe_rolloff    = rtab[(ptr[6] >> 3) & 0x3];
  if (dmc.dmc_fe_delsys == DVB_SYS_DVBS &&
      dmc.dmc_fe_rolloff != DVB_ROLLOFF_35) {
    tvhwarn(mt->mt_name, "dvb-s rolloff error");
    return NULL;
  }

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug(mt->mt_name, "    %s", buf);

  /* Create */
  return mm->mm_network->mn_create_mux(mm->mm_network, mm, onid, tsid, &dmc, force);
}

/*
 * Cable delivery descriptor
 */
static mpegts_mux_t *
dvb_desc_cable_del
  (mpegts_table_t *mt, mpegts_mux_t *mm,
   uint16_t onid, uint16_t tsid,
   const uint8_t *ptr, int len )
{
  int frequency, symrate;
  dvb_mux_conf_t dmc;
  char buf[128];

  static const dvb_fe_modulation_t qtab [6] = {
    DVB_MOD_QAM_AUTO, DVB_MOD_QAM_16, DVB_MOD_QAM_32, DVB_MOD_QAM_64,
    DVB_MOD_QAM_128,  DVB_MOD_QAM_256
  };

  /* Not enough data */
  if(len < 11) return NULL;

  /* Extract data */
  frequency  =
    bcdtoint(ptr[0]) * 1000000 + bcdtoint(ptr[1]) * 10000 + 
    bcdtoint(ptr[2]) * 100     + bcdtoint(ptr[3]);
  symrate    =
    bcdtoint(ptr[7]) * 100000 + bcdtoint(ptr[8]) * 1000 + 
    bcdtoint(ptr[9]) * 10     + (ptr[10] >> 4);
  if (!frequency) {
    tvhwarn(mt->mt_name, "dvb-c frequency error");
    return NULL;
  }
  if (!symrate) {
    tvhwarn(mt->mt_name, "dvb-c symbol rate error");
    return NULL;
  }

  dvb_mux_conf_init(&dmc, DVB_SYS_DVBC_ANNEX_A);

  dmc.dmc_fe_freq            = frequency * 100;

  dmc.u.dmc_fe_qam.symbol_rate  = symrate * 100;
  if((ptr[6] & 0x0f) >= sizeof(qtab))
    dmc.dmc_fe_modulation    = DVB_MOD_QAM_AUTO;
  else
    dmc.dmc_fe_modulation    = qtab[ptr[6] & 0x0f];
  dmc.u.dmc_fe_qam.fec_inner = fec_tab[ptr[10] & 0x07];

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug(mt->mt_name, "    %s", buf);

  /* Create */
  return mm->mm_network->mn_create_mux(mm->mm_network, mm, onid, tsid, &dmc, 0);
}

/*
 * Terrestrial delivery descriptor
 */
static mpegts_mux_t *
dvb_desc_terr_del
  (mpegts_table_t *mt, mpegts_mux_t *mm,
   uint16_t onid, uint16_t tsid,
   const uint8_t *ptr, int len )
{
  static const dvb_fe_bandwidth_t btab [8] = {
    DVB_BANDWIDTH_8_MHZ, DVB_BANDWIDTH_7_MHZ,
    DVB_BANDWIDTH_6_MHZ, DVB_BANDWIDTH_AUTO,
    DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_AUTO,
    DVB_BANDWIDTH_AUTO,  DVB_BANDWIDTH_AUTO
  };
  static const dvb_fe_modulation_t ctab [4] = {
    DVB_MOD_QPSK, DVB_MOD_QAM_16, DVB_MOD_QAM_64, DVB_MOD_QAM_AUTO
  };
  static const dvb_fe_guard_interval_t gtab [4] = {
    DVB_GUARD_INTERVAL_1_32, DVB_GUARD_INTERVAL_1_16,
    DVB_GUARD_INTERVAL_1_8,  DVB_GUARD_INTERVAL_1_4
  };
  static const dvb_fe_transmit_mode_t ttab [4] = {
    DVB_TRANSMISSION_MODE_2K,
    DVB_TRANSMISSION_MODE_8K,
    DVB_TRANSMISSION_MODE_4K,
    DVB_TRANSMISSION_MODE_AUTO
  };
  static const dvb_fe_hierarchy_t htab [8] = {
    DVB_HIERARCHY_NONE, DVB_HIERARCHY_1, DVB_HIERARCHY_2, DVB_HIERARCHY_4,
    DVB_HIERARCHY_NONE, DVB_HIERARCHY_1, DVB_HIERARCHY_2, DVB_HIERARCHY_4
  };

  int frequency;
  dvb_mux_conf_t dmc;
  char buf[128];

  /* Not enough data */
  if (len < 11) return NULL;

  /* Extract data */
  frequency     = ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]);
  if (!frequency) {
    tvhwarn(mt->mt_name, "dvb-t frequency error");
    return NULL;
  }

  dvb_mux_conf_init(&dmc, DVB_SYS_DVBT);

  dmc.dmc_fe_freq                         = frequency * 10;
  dmc.u.dmc_fe_ofdm.bandwidth             = btab[(ptr[4] >> 5) & 0x7];
  dmc.dmc_fe_modulation                   = ctab[(ptr[5] >> 6) & 0x3];
  dmc.u.dmc_fe_ofdm.hierarchy_information = htab[(ptr[5] >> 3) & 0x3];
  dmc.u.dmc_fe_ofdm.code_rate_HP          = fec_tab[(ptr[5] + 1) & 0x7];
  dmc.u.dmc_fe_ofdm.code_rate_LP          = fec_tab[((ptr[6] + 1) >> 5) & 0x7];
  dmc.u.dmc_fe_ofdm.guard_interval        = gtab[(ptr[6] >> 3) & 0x3];
  dmc.u.dmc_fe_ofdm.transmission_mode     = ttab[(ptr[6] >> 1) & 0x3];

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug(mt->mt_name, "    %s", buf);
  
  /* Create */
  return mm->mm_network->mn_create_mux(mm->mm_network, mm, onid, tsid, &dmc, 0);
}
 
#endif /* ENABLE_MPEGTS_DVB */

static int
dvb_desc_service
  ( const uint8_t *ptr, int len, int *stype,
    char *sprov, size_t sprov_len,
    char *sname, size_t sname_len, const char *charset )
{
  int r;
  size_t l;
  char *str;

  if (len < 2)
    return -1;

  /* Type */
  *stype = *ptr++;

  /* Provider */
  if ((r = dvb_get_string_with_len(sprov, sprov_len, ptr, len, charset, NULL)) < 0)
    return -1;

  /* Name */
  if (dvb_get_string_with_len(sname, sname_len, ptr+r, len-r, charset, NULL) < 0)
    return -1;

  /* Cleanup name */
  str = sname;
  while (*str && *str <= 32)
    str++;
  memmove(sname, str, sname_len); // Note: could avoid this copy by passing an output ptr
  l   = strlen(str);
  while (l > 1 && str[l-1] <= 32) {
    str[l-1] = 0;
    l--;
  }

  return 0;
}

static dvb_bat_svc_t *
dvb_bat_find_service( dvb_bat_id_t *bi, mpegts_service_t *s,
                      uint8_t lcn_dtag, uint32_t lcn )
{
  dvb_bat_svc_t *bs;

  assert(bi);
  TAILQ_FOREACH(bs, &bi->services, link)
    if (bs->svc == s)
      break;
  if (!bs) {
    bs = calloc(1, sizeof(*bs));
    bs->svc = s;
    TAILQ_INSERT_TAIL(&bi->services, bs, link);
  }
  if (lcn != UINT_MAX && !bs->lcn_dtag) {
    bs->lcn_dtag = lcn_dtag;
    bs->lcn = lcn;
  }
  return bs;
}

static int
dvb_desc_service_list
  ( const char *dstr, const uint8_t *ptr, int len, mpegts_mux_t *mm,
    dvb_bat_id_t *bi )
{
  uint16_t stype, sid;
  mpegts_service_t *s;
  int i;

  for (i = 0; i < len; i += 3) {
    sid   = (ptr[i] << 8) | ptr[i+1];
    stype = ptr[i+2];
    tvhdebug(dstr, "    service %04X (%d) type %02X (%d)", sid, sid, stype, stype);
    if (mm) {
      int save = 0;
      s = mpegts_service_find(mm, sid, 0, 1, &save);
      if (bi)
        dvb_bat_find_service(bi, s, 0, UINT_MAX);
      if (save)
        s->s_config_save((service_t*)s);
    }
  }
  return 0;
}

static int
dvb_desc_local_channel
  ( const char *dstr, const uint8_t *ptr, int len,
    uint8_t dtag, mpegts_mux_t *mm, dvb_bat_id_t *bi, int prefer )
{
  int save = 0;
  uint16_t sid, lcn;
  mpegts_service_t *s;

  if ((len % 4) != 0)
    return 0;

  while(len >= 4) {
    sid = (ptr[0] << 8) | ptr[1];
    lcn = ((ptr[2] & 3) << 8) | ptr[3];
    tvhdebug(dstr, "    sid %d lcn %d", sid, lcn);
    if (sid && lcn && mm) {
      s = mpegts_service_find(mm, sid, 0, 0, &save);
      if (s) {
        if (bi) {
          dvb_bat_find_service(bi, s, dtag, lcn);
        } else if ((!s->s_dvb_channel_dtag ||
                    s->s_dvb_channel_dtag == dtag || prefer) &&
                    s->s_dvb_channel_num != lcn) {
          s->s_dvb_channel_dtag = dtag;
          s->s_dvb_channel_num = lcn;
          s->s_config_save((service_t*)s);
          service_refresh_channel((service_t*)s);
        }
      }
    }
    ptr += 4;
    len -= 4;
  }
  return 0;
}

#if ENABLE_MPEGTS_DVB

/*
 * UK FreeSat
 */

static void
dvb_freesat_local_channels
  ( dvb_bat_id_t *bi, const char *dstr, const uint8_t *ptr, int len )
{
  uint16_t sid, lcn, regionid;
  uint16_t unk;
  dvb_freesat_svc_t *fs;
  int len2;

  while (len > 4) {
    sid = (ptr[0] << 8) | ptr[1];
    unk = (ptr[2] << 8) | ptr[3];
    len2 = ptr[4];
    ptr += 5;
    len -= 5;
    if (len2 > len)
      break;
    tvhtrace(dstr, "      sid %04X (%d) uknown %04X (%d)", sid, sid, unk, unk);
    while (len2 > 3) {
      lcn = ((ptr[0] & 0x0f) << 8) | ptr[1];
      regionid = (ptr[2] << 8) | ptr[3];
      tvhtrace(dstr, "        lcn %d region %d", lcn, regionid);
      
      TAILQ_FOREACH(fs, &bi->fservices, link)
        if (fs->sid == sid && fs->regionid == regionid)
          break;
      if (!fs) {
        fs = calloc(1, sizeof(*fs));
        fs->sid = sid;
        fs->regionid = regionid;
        fs->lcn = lcn;
        TAILQ_INSERT_TAIL(&bi->fservices, fs, link);
      }

      ptr += 4;
      len -= 4;
      len2 -= 4;
    }
  }
}

static void
dvb_freesat_regions
  ( dvb_bat_id_t *bi, const char *dstr, const uint8_t *ptr, int len )
{
  uint16_t id;
  char name[32];
  dvb_freesat_region_t *fr;
  int r;

  if (!bi)
    return;

  while (len > 5) {
    id = (ptr[0] << 8) | ptr[1];
    /* language: ptr[2-4]: 'eng' */
    if ((r = dvb_get_string_with_len(name, sizeof(name), ptr + 5, len - 5, NULL, NULL)) < 0)
      break;
    tvhtrace(dstr, "    region %u - '%s'", id, name);

    LIST_FOREACH(fr, &bi->fregions, link)
      if (fr->regionid == id)
        break;
    if (!fr) {
      fr = calloc(1, sizeof(*fr));
      fr->regionid = id;
      strncpy(fr->name, name, sizeof(fr->name)-1);
      fr->name[sizeof(fr->name)-1] = '\0';
      TAILQ_INIT(&fr->services);
      LIST_INSERT_HEAD(&bi->fregions, fr, link);
    }

    ptr += 5 + r;
    len -= 5 + r;
  }
}

static int
dvb_freesat_add_service
  ( dvb_bat_id_t *bi, dvb_freesat_region_t *fr, mpegts_service_t *s, uint32_t lcn )
{
  char name[96], src[64];
  if (!fr->bouquet) {
    strcpy(name, "???");
    if (idnode_is_instance(&bi->mm->mm_id, &dvb_mux_dvbs_class))
      dvb_sat_position_to_str(((dvb_mux_t *)bi->mm)->lm_tuning.u.dmc_fe_qpsk.orbital_pos,
                              name, sizeof(name));
    snprintf(src, sizeof(src), "dvb-%s://dvbs,%s,%04X,%u",
             bi->freesat ? "freesat" : "bskyb", name, bi->nbid, fr->regionid);
    snprintf(name, sizeof(name), "%s: %s", bi->name, fr->name);
    fr->bouquet = bouquet_find_by_source(name, src, 1);
  }
  bouquet_add_service(fr->bouquet, (service_t *)s, (int64_t)lcn * CHANNEL_SPLIT, 0);
  return fr->bouquet->bq_enabled;
}

static void
dvb_freesat_completed
  ( dvb_bat_t *b, dvb_bat_id_t *bi, const char *dstr )
{
  dvb_bat_svc_t *bs;
  dvb_freesat_svc_t *fs;
  dvb_freesat_region_t *fr;
  uint16_t sid;
  uint32_t total = 0, regions = 0, uregions = 0;

  tvhtrace(dstr, "completed %s [%04X] bouquets '%s'",
           bi->freesat ? "freesat" : "bskyb", bi->nbid, bi->name);

  /* Find all "fallback" services and region specific */
  TAILQ_FOREACH(bs, &bi->services, link) {
    total++;
    sid = bs->svc->s_dvb_service_id;
    TAILQ_FOREACH(fs, &bi->fservices, link)
      if (fs->sid == sid) {
        fs->svc = bs->svc;
        if (fs->regionid == 0 || fs->regionid == 0xffff) {
          if (fs->regionid != 0 || !bs->fallback)
            bs->fallback = fs;
          continue;
        }
        LIST_FOREACH(fr, &bi->fregions, link)
          if (fr->regionid == fs->regionid)
            break;
        if (!fr)
          tvhtrace(dstr, "cannot find freesat region id %u", fs->regionid);
        else
          TAILQ_INSERT_TAIL(&fr->services, fs, region_link);
      }
  }

  /* create bouquets, one per region */
  LIST_FOREACH(fr, &bi->fregions, link) {
    regions++;
    if (TAILQ_EMPTY(&fr->services)) continue;
    uregions++;
    TAILQ_FOREACH(fs, &fr->services, region_link) {
      if (!dvb_freesat_add_service(bi, fr, fs->svc, fs->lcn)) break;
      TAILQ_FOREACH(bs, &bi->services, link)
        if (bs->fallback && fs->lcn == bs->fallback->lcn)
          bs->used = 1;
    }
    if (fs) continue;
    TAILQ_FOREACH(bs, &bi->services, link) {
      if (bs->used) {
        bs->used = 0;
        continue;
      }
      TAILQ_FOREACH(fs, &fr->services, region_link)
        if (fs->svc == bs->svc)
          break;
      if (fs) continue;
      if ((fs = bs->fallback) != NULL) {
        if (!dvb_freesat_add_service(bi, fr, bs->svc, fs->lcn)) break;
      } else {
        if (!dvb_freesat_add_service(bi, fr, bs->svc, 0)) break;
      }
    }
  }

  tvhtrace(dstr, "completed %s [%04X] bouquets '%s' total %u regions %u (%u)",
           bi->freesat ? "freesat" : "bskyb", bi->nbid, bi->name,
           total, regions, uregions);

  /* Remove all services associated to region, notify the completed status */
  LIST_FOREACH(fr, &bi->fregions, link) {
    TAILQ_INIT(&fr->services);
    if (fr->bouquet) {
      dvb_bouquet_comment(fr->bouquet, bi->mm);
      bouquet_completed(fr->bouquet, total);
      fr->bouquet = NULL;
    }
  }

  /* Clear all "fallback/default" services */
  TAILQ_FOREACH(bs, &bi->services, link)
    bs->fallback = NULL;

  tvhtrace(dstr, "completed %s [%04X] bouquets '%s' update finished",
           bi->freesat ? "freesat" : "bskyb", bi->nbid, bi->name);

}

/*
 * UK BSkyB
 */

static struct strtab bskyb_regions[] = {
  { "Atherstone",                  19 },
  { "Border England",              12 },
  { "Border Scotland",             36 },
  { "Brighton",                    65 },
  { "Central Midlands",             3 },
  { "Channel Isles",               34 },
  { "Dundee",                      39 },
  { "East Midlands",               20 },
  { "Essex",                        2 },
  { "Gloucester",                  24 },
  { "Grampian",                    35 },
  { "Granada",                      7 },
  { "Henley On Thames",            70 },
  { "HTV Wales",                   43 },
  { "HTV West",                     4 },
  { "HTV West / Thames Valley",    63 },
  { "Humber",                      29 },
  { "London",                       1 },
  { "London / Essex",              18 },
  { "London / Thames Valley",      66 },
  { "London Kent",                 64 },
  { "Meridian East",               11 },
  { "Meridian North",              68 },
  { "Meridian South",               5 },
  { "Meridian South East",         10 },
  { "Merseyside",                  45 },
  { "Norfolk",                     21 },
  { "North East Midlands",         62 },
  { "North West Yorkshire",         8 },
  { "North Yorkshire",             26 },
  { "Northern Ireland",            33 },
  { "Oxford",                      71 },
  { "Republic of Ireland",         50 },
  { "Ridge Hill",                  41 },
  { "Scarborough",                 61 },
  { "Scottish East",               37 },
  { "Scottish West",               38 },
  { "Sheffield",                   60 },
  { "South Lakeland",              28 },
  { "South Yorkshire",             72 },
  { "Tees",                        69 },
  { "Thames Valley",                9 },
  { "Tring",                       27 },
  { "Tyne",                        13 },
  { "West Anglia",                 25 },
  { "West Dorset",                 67 },
  { "Westcountry",                  6 },
};

static void
dvb_bskyb_local_channels
  ( dvb_bat_id_t *bi, const char *dstr,
    const uint8_t *ptr, int len, mpegts_mux_t *mm )
{
  uint16_t sid, lcn, regionid;
  uint16_t unk, stype;
  dvb_freesat_region_t *fr;
  dvb_freesat_svc_t *fs;
  dvb_bat_svc_t *bs;
  mpegts_service_t *s;
  const char *str;
  char buf[16];

  if (len < 2)
    return;

  regionid = (ptr[1] != 0xff) ? ptr[1] : 0xffff;

#if 0
  if (regionid != 0xffff && regionid != 0 && regionid != 1) {
    if ((str = getenv("TVHEADEND_BSKYB_REGIONID")) != NULL) {
      if (regionid != atoi(str))
        return;
    } else {
      return;
    }
  }
#endif

  len -= 2;
  ptr += 2;

  tvhtrace(dstr, "      region id %04X (%d) unknown %02X (%d)",
           regionid, regionid, ptr[0], ptr[0]);

  while (len > 8) {
    sid = (ptr[0] << 8) | ptr[1];
    lcn = (ptr[5] << 8) | ptr[6];
    stype = ptr[2];
    unk = (ptr[3] << 8) | ptr[4];
    ptr += 9;
    len -= 9;

    tvhtrace(dstr, "      sid %04X (%d) type %02X (%d) lcn %d unknown %04X (%d)",
             sid, sid, stype, stype, lcn, unk, unk);

    TAILQ_FOREACH(fs, &bi->fservices, link)
      if (fs->sid == sid && fs->regionid == regionid)
        break;
    if (!fs) {
      fs = calloc(1, sizeof(*fs));
      fs->sid = sid;
      fs->regionid = regionid;
      fs->lcn = lcn != 0xffff ? lcn : 0;
      TAILQ_INSERT_TAIL(&bi->fservices, fs, link);
    }

    TAILQ_FOREACH(bs, &bi->services, link)
      if (bs->svc->s_dvb_service_id == sid)
        break;
    if (mm && !bs) {
      s = mpegts_service_find(mm, sid, 0, 0, NULL);
      if (s) {
        bs = calloc(1, sizeof(*bs));
        bs->svc = s;
        TAILQ_INSERT_TAIL(&bi->services, bs, link);
      }
    }

    if (regionid && regionid != 0xffff) {
      LIST_FOREACH(fr, &bi->fregions, link)
        if (fr->regionid == regionid)
          break;
      if (!fr) {
        fr = calloc(1, sizeof(*fr));
        fr->regionid = regionid;
        /* Note: Poland provider on 13E uses also this bouquet format */
        if (bi->nbid < 0x1000 || bi->nbid > 0x1010 ||
           (str = val2str(regionid, bskyb_regions)) == NULL) {
          snprintf(buf, sizeof(buf), "Region %d", regionid);
          str = buf;
        }
        strncpy(fr->name, str, sizeof(fr->name)-1);
        fr->name[sizeof(fr->name)-1] = '\0';
        TAILQ_INIT(&fr->services);
        LIST_INSERT_HEAD(&bi->fregions, fr, link);
      }
    }
  }
}

#endif /* ENABLE_MPEGTS_DVB */

/*
 * PAT processing
 */
int
dvb_pat_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  uint16_t sid, pid, tsid;
  uint16_t nit_pid = 0;
  mpegts_mux_t             *mm  = mt->mt_mux;
  mpegts_psi_table_state_t *st  = NULL;
  mpegts_service_t *s;

  /* Begin */
  if (tableid != 0) return -1;
  tsid = (ptr[0] << 8) | ptr[1];
  r    = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                         tableid, tsid, 5, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* Multiplex */
  tvhdebug("pat", "tsid %04X (%d)", tsid, tsid);
  mpegts_mux_set_tsid(mm, tsid, 1);
  
  /* Process each programme */
  ptr += 5;
  len -= 5;
  while(len >= 4) {
    sid = ptr[0]         << 8 | ptr[1];
    pid = (ptr[2] & 0x1f) << 8 | ptr[3];

    /* NIT PID */
    if (sid == 0) {
      if (pid) {
        nit_pid = pid;
        tvhdebug("pat", "  nit on pid %04X (%d)", pid, pid);
      }

    /* Service */
    } else if (pid) {
      tvhdebug("pat", "  sid %04X (%d) on pid %04X (%d)", sid, sid, pid, pid);
      int save = 0;
      if ((s = mpegts_service_find(mm, sid, pid, 1, &save))) {
        if (!s->s_enabled && s->s_auto == SERVICE_AUTO_PAT_MISSING) {
          tvhinfo("mpegts", "enabling service %s [sid %04X/%d] (found in PAT)",
                  s->s_nicename, s->s_dvb_service_id, s->s_dvb_service_id);
          service_set_enabled((service_t *)s, 1, SERVICE_AUTO_NORMAL);
        }
        s->s_dvb_check_seen = dispatch_clock;
        mpegts_table_add(mm, DVB_PMT_BASE, DVB_PMT_MASK, dvb_pmt_callback,
                         NULL, "pmt",
                         MT_CRC | MT_QUICKREQ | MT_ONESHOT | MT_SCANSUBS,
                         pid, MPS_WEIGHT_PMT_SCAN);

        if (save)
          service_request_save((service_t*)s, 1);
      }
    }

    /* Next */
    ptr += 4;
    len -= 4;
  }
  
  /* Install NIT handler */
  if (nit_pid)
    mpegts_table_add(mm, DVB_NIT_BASE, DVB_NIT_MASK, dvb_nit_callback,
                     NULL, "nit", MT_QUICKREQ | MT_CRC, nit_pid,
                     MPS_WEIGHT_NIT);

  /* End */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/*
 * CAT processing
 */
int
dvb_cat_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  uint8_t dtag, dlen;
  uint16_t pid; 
  uintptr_t caid;
  mpegts_mux_t             *mm  = mt->mt_mux;
  mpegts_psi_table_state_t *st  = NULL;

  /* Start */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, 0, 5, &st, &sect, &last, &ver);
  if (r != 1) return r;
  ptr += 5;
  len -= 5;

  /* Send CAT data for descramblers */
  descrambler_cat_data(mm, ptr, len);

  while(len > 2) {
    dtag = *ptr++;
    dlen = *ptr++;
    len -= 2;

    switch(dtag) {
      case DVB_DESC_CA:
        if (len >= 4 && dlen >= 4) {
          caid = ( ptr[0]         << 8) | ptr[1];
          pid  = ((ptr[2] & 0x1f) << 8) | ptr[3];
          tvhdebug("cat", "  caid %04X (%d) pid %04X (%d)",
                   (uint16_t)caid, (uint16_t)caid, pid, pid);
        }
        break;
      default:
        break;
    }

    ptr += dlen;
    len -= dlen;
  }

  /* Finish */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/*
 * PMT processing
 */

int
dvb_pmt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  uint16_t sid;
  mpegts_mux_t *mm = mt->mt_mux;
  mpegts_service_t *s;
  mpegts_psi_table_state_t *st  = NULL;

  /* Start */
  sid = ptr[0] << 8 | ptr[1];
  r   = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                        tableid, sid, 9, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* Find service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    if (s->s_dvb_service_id == sid) break;
  if (!s) return -1;

  /* Process */
  tvhdebug("pmt", "sid %04X (%d)", sid, sid);
  pthread_mutex_lock(&s->s_stream_mutex);
  r = psi_parse_pmt(mt->mt_mux, s, ptr, len);
  pthread_mutex_unlock(&s->s_stream_mutex);
  if (r)
    service_restart((service_t*)s);

#if ENABLE_LINUXDVB_CA
  /* DVBCAM requires full pmt data including header and crc */
  dvbcam_pmt_data(s, ptr - 3, len + 3 + 4);
#endif

  /* Finish */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/*
 * NIT/BAT processing (because its near identical)
 */

static void
dvb_bat_destroy_lists( mpegts_table_t *mt )
{
  dvb_bat_t *b = mt->mt_bat;
  dvb_bat_id_t *bi;
  dvb_bat_svc_t *bs;
  dvb_freesat_region_t *fr;
  dvb_freesat_svc_t *fs;

  while ((bi = LIST_FIRST(&b->bats)) != NULL) {
    while ((bs = TAILQ_FIRST(&bi->services)) != NULL) {
      TAILQ_REMOVE(&bi->services, bs, link);
      free(bs);
    }
    while ((fs = TAILQ_FIRST(&bi->fservices)) != NULL) {
      TAILQ_REMOVE(&bi->fservices, fs, link);
      free(fs);
    }
    while ((fr = LIST_FIRST(&bi->fregions)) != NULL) {
      LIST_REMOVE(fr, link);
      free(fr);
    }
    LIST_REMOVE(bi, link);
    free(bi);
  }
}

void
dvb_bat_destroy( mpegts_table_t *mt )
{
  dvb_bat_destroy_lists(mt);
  free(mt->mt_bat);
  mt->mt_bat = NULL;
}

static void
dvb_bat_completed
  ( dvb_bat_t *b, const char *dstr, int tableid, int tsid, int nbid,
    mpegts_mux_t *mux, bouquet_t *bq_alt )
{
  dvb_bat_id_t *bi;
  dvb_bat_svc_t *bs;
  uint32_t services_count;
  bouquet_t *bq;

  b->complete = 1;

  LIST_FOREACH(bi, &b->bats, link) {

    if (bi->nbid != nbid) {
      if (!bi->complete)
        b->complete = 0;
      continue;
    }

    if (bi->freesat || bi->bskyb) {
#if ENABLE_MPEGTS_DVB
      dvb_freesat_completed(b, bi, dstr);
#endif
      goto complete;
    }

    bq = NULL;

#if ENABLE_MPEGTS_DVB
    if (tableid == 0x4A /* BAT */) {
      char src[64] = "";
      if (idnode_is_instance(&mux->mm_id, &dvb_mux_dvbs_class)) {
        dvb_mux_conf_t *mc = &((dvb_mux_t *)mux)->lm_tuning;
        if (mc->u.dmc_fe_qpsk.orbital_pos != INT_MAX) {
          char buf[16];
          dvb_sat_position_to_str(mc->u.dmc_fe_qpsk.orbital_pos, buf, sizeof(buf));
          snprintf(src, sizeof(src), "dvb-bouquet://dvbs,%s,%04X,%04X", buf, tsid, bi->nbid);
        }
      } else if (idnode_is_instance(&mux->mm_id, &dvb_mux_dvbt_class)) {
        snprintf(src, sizeof(src), "dvb-bouquet://dvbt,%04X,%04X", tsid, bi->nbid);
      } else if (idnode_is_instance(&mux->mm_id, &dvb_mux_dvbc_class)) {
        snprintf(src, sizeof(src), "dvb-bouquet://dvbc,%04X,%04X", tsid, bi->nbid);
      }
      if (src[0])
        bq = bouquet_find_by_source(bi->name, src, !TAILQ_EMPTY(&bi->services));
    } else if (tableid == DVB_FASTSCAN_NIT_BASE) {
      bq = bq_alt;
    }
#endif

    if (!bq) continue;

    dvb_bouquet_comment(bq, bi->mm);

    services_count = 0;
    TAILQ_FOREACH(bs, &bi->services, link) {
      services_count++;
      if (bq->bq_enabled)
        bouquet_add_service(bq, (service_t *)bs->svc,
                            (int64_t)bs->lcn * CHANNEL_SPLIT, 0);
    }

    bouquet_completed(bq, services_count);

complete:
    bi->complete = 1;
  }
}

static int
dvb_nit_mux
  (mpegts_table_t *mt, mpegts_mux_t *mux, mpegts_mux_t *mm,
   uint16_t onid, uint16_t tsid,
   const uint8_t *lptr, int llen, uint8_t tableid,
   dvb_bat_id_t *bi, int discovery)
{
  uint32_t priv = 0;
  uint8_t dtag;
  int dllen, dlen;
  const uint8_t *dlptr, *dptr, *lptr_orig = lptr;
  const char *charset;
  char buf[128], dauth[256];

  if (mux && !mux->mm_enabled)
    bi = NULL;

  charset = dvb_charset_find(mux ? mux->mm_network : mm->mm_network, mux, NULL);

  if (mux)
    mpegts_mux_nice_name(mux, buf, sizeof(buf));
  else
    strcpy(buf, "<none>");
  tvhdebug(mt->mt_name, "  onid %04X (%d) tsid %04X (%d) mux %s%s",
           onid, onid, tsid, tsid, buf, discovery ? " (discovery)" : "");

  DVB_DESC_FOREACH(lptr, llen, 4, dlptr, dllen, dtag, dlen, dptr) {
    tvhtrace(mt->mt_name, "    dtag %02X dlen %d", dtag, dlen);

#if ENABLE_MPEGTS_DVB
    /* Limit delivery descriptiors only in the discovery phase */
    if (discovery &&
        dtag != DVB_DESC_SAT_DEL &&
        dtag != DVB_DESC_CABLE_DEL &&
        dtag != DVB_DESC_TERR_DEL)
      continue;
#endif

    /* User-defined */
    if (mt->mt_mux_cb && mux) {
      int i = 0;
      while (mt->mt_mux_cb[i].cb) {
        if (mt->mt_mux_cb[i].tag == dtag)
          break;
        i++;
        }
      if (mt->mt_mux_cb[i].cb) {
        if (mt->mt_mux_cb[i].cb(mt, mux, bi ? bi->nbid : 0, dtag, dptr, dlen))
          return -1;
        dtag = 0;
      }
    }

    /* Pre-defined */
    switch (dtag) {
    default:
    case 0:
      break;

    /* nit only */
#if ENABLE_MPEGTS_DVB
    case DVB_DESC_SAT_DEL:
    case DVB_DESC_CABLE_DEL:
    case DVB_DESC_TERR_DEL:
      if (discovery) {
        if (dtag == DVB_DESC_SAT_DEL)
          mux = dvb_desc_sat_del(mt, mm, onid, tsid, dptr, dlen,
                                 tableid == DVB_FASTSCAN_NIT_BASE);
        else if (dtag == DVB_DESC_CABLE_DEL)
          mux = dvb_desc_cable_del(mt, mm, onid, tsid, dptr, dlen);
        else
          mux = dvb_desc_terr_del(mt, mm, onid, tsid, dptr, dlen);
        if (mux) {
          mpegts_mux_set_onid(mux, onid);
          mpegts_mux_set_tsid(mux, tsid, 0);
          if (tableid == DVB_FASTSCAN_NIT_BASE)
            dvb_fs_mux_add(mt, mm, mux);
        }
      }
      break;
#endif

    /* Both */
    case DVB_DESC_DEF_AUTHORITY:
      if (dvb_get_string(dauth, sizeof(dauth), dptr, dlen, charset, NULL))
        return -1;
      tvhdebug(mt->mt_name, "    default auth [%s]", dauth);
      if (mux && *dauth)
        mpegts_mux_set_crid_authority(mux, dauth);
      break;
    case DVB_DESC_SERVICE_LIST:
      if (dvb_desc_service_list(mt->mt_name, dptr, dlen, mux, bi))
        return -1;
      break;
    case DVB_DESC_PRIVATE_DATA:
      if (dlen == 4) {
        priv = (dptr[0] << 24) | (dptr[1] << 16) | (dptr[2] << 8) | dptr[3];
        tvhtrace(mt->mt_name, "      private %08X", priv);
      }
      break;
    case 0x81:
      if (priv == 0) goto lcn;
      break;
    case 0x82:
      if (priv == 0) goto lcn;
      break;
    case 0x83:
      if (priv == 0 || priv == 0x28 || priv == 0x29 || priv == 0xa5 ||
          priv == 0x233A) goto lcn;
      break;
    case 0x86:
      if (priv == 0) goto lcn;
      break;
    case 0x88:
      if (priv == 0x28) {
        /* HD simulcast */
        if (dvb_desc_local_channel(mt->mt_name, dptr, dlen, dtag, mux, bi, 1))
          return -1;
      }
      break;
    case 0x93:
      if (priv == 0 || priv == 0x362275)
      /* fall thru */
lcn:
      if (dvb_desc_local_channel(mt->mt_name, dptr, dlen, dtag, mux, bi, 0))
        return -1;
      break;
    case DVB_DESC_FREESAT_LCN:
#if ENABLE_MPEGTS_DVB
      if (bi && tableid == 0x4A && priv == PRIV_FSAT) {
        dvb_freesat_local_channels(bi, mt->mt_name, dptr, dlen);
        bi->freesat = 1;
      }
#endif
      break;
    case DVB_DESC_BSKYB_LCN:
#if ENABLE_MPEGTS_DVB
      if (bi && tableid == 0x4A && priv == 2) {
        dvb_bskyb_local_channels(bi, mt->mt_name, dptr, dlen, mux);
        bi->bskyb = 1;
      }
#endif
      break;
    }
  }

  return lptr - lptr_orig;
}

int
dvb_nit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int save = 0;
  int r, sect, last, ver;
  uint32_t priv = 0;
  uint8_t  dtag;
  int llen, dlen;
  const uint8_t *lptr, *dptr;
  uint16_t nbid = 0, onid, tsid;
  mpegts_mux_t     *mm = mt->mt_mux, *mux;
  mpegts_network_t *mn = mm->mm_network;
  char name[256];
  mpegts_psi_table_state_t *st  = NULL;
  const char *charset;
  bouquet_t *bq = NULL;
  dvb_bat_t *b = NULL;
  dvb_bat_id_t *bi = NULL;

  /* Net/Bat ID */
  nbid = (ptr[0] << 8) | ptr[1];

  /* Begin */
  if (tableid != 0x40 && tableid != 0x41 && tableid != 0x4A &&
      tableid != DVB_FASTSCAN_NIT_BASE)
    return -1;

  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, nbid, 7, &st, &sect, &last, &ver);
  if (r == 0) {
    if (tableid == 0x4A || tableid == DVB_FASTSCAN_NIT_BASE) {
      if ((b = mt->mt_bat) != NULL) {
        if (!b->complete) {
          dvb_bat_completed(b, mt->mt_name, tableid, mm->mm_tsid, nbid,
                            mm, mt->mt_opaque);
          mt->mt_working -= st->working;
          st->working = 0;
        }
        if (b->complete)
          dvb_bat_destroy_lists(mt);
      }
    }
  }
  if (r != 1) return r;

  /* NIT */
  if (tableid != 0x4A && tableid != DVB_FASTSCAN_NIT_BASE) {

    /* Specific NID */
    if (mn->mn_nid) {
      if (mn->mn_nid != nbid) {
        return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
      }
  
    /* Only use "this" network */
    } else if (tableid != 0x40) {
      return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
    }
  }

  /* BAT ID lookup */
  if (dvb_bouquets_parse &&
      (tableid == 0x4A || tableid == DVB_FASTSCAN_NIT_BASE)) {
    if ((b = mt->mt_bat) == NULL) {
      b = calloc(1, sizeof(*b));
      mt->mt_bat = b;
    }
    LIST_FOREACH(bi, &b->bats, link)
      if (bi->nbid == nbid)
        break;
    if (!bi) {
      bi = calloc(1, sizeof(*bi));
      bi->nbid = nbid;
      TAILQ_INIT(&bi->services);
      TAILQ_INIT(&bi->fservices);
      LIST_INSERT_HEAD(&b->bats, bi, link);
      bi->mm = mm;
    }
    if (!st->working) {
      st->working = 1;
      mt->mt_working++;
      mt->mt_flags |= MT_FASTSWITCH;
    }
  }

  /* Network Descriptors */
  *name   = 0;
  charset = dvb_charset_find(mn, NULL, NULL);
  DVB_DESC_FOREACH(ptr, len, 5, lptr, llen, dtag, dlen, dptr) {
    tvhtrace(mt->mt_name, "  dtag %02X dlen %d", dtag, dlen);

    switch (dtag) {
      case DVB_DESC_BOUQUET_NAME:
      case DVB_DESC_NETWORK_NAME:
        if (dvb_get_string(name, sizeof(name), dptr, dlen, charset, NULL))
          return -1;
        break;
      case DVB_DESC_MULTI_NETWORK_NAME:
        // TODO: implement this?
        break;
      case DVB_DESC_PRIVATE_DATA:
        if (tableid == 0x4A && dlen == 4) {
          priv = (dptr[0] << 24) | (dptr[1] << 16) | (dptr[2] << 8) | dptr[3];
          tvhtrace(mt->mt_name, "    private %08X", priv);
        }
        break;
      case DVB_DESC_FREESAT_REGIONS:
#if ENABLE_MPEGTS_DVB
        if (priv == PRIV_FSAT)
          dvb_freesat_regions(bi, mt->mt_name, dptr, dlen);
#endif
        break;
    }
  }

  /* Fastscan */
  if (tableid == DVB_FASTSCAN_NIT_BASE) {
    tvhdebug(mt->mt_name, "fastscan %04X (%d) [%s]", nbid, nbid, name);
    bq = mt->mt_opaque;
    dvb_bouquet_comment(bq, mm);

  /* BAT */
  } else if (tableid == 0x4A) {
    tvhdebug(mt->mt_name, "bouquet %04X (%d) [%s]", nbid, nbid, name);
    if (bi && *name) {
      strncpy(bi->name, name, sizeof(bi->name)-1);
      bi->name[sizeof(bi->name)-1] = '\0';
    }

  /* NIT */
  } else {
    tvhdebug(mt->mt_name, "network %04X (%d) [%s]", nbid, nbid, name);
    save |= mpegts_network_set_network_name(mn, name);
    if (save)
      mn->mn_config_save(mn);
  }

  /* Transport length */
  DVB_LOOP_FOREACH(ptr, len, 0, lptr, llen, 6) {
    tsid  = (lptr[0] << 8) | lptr[1];
    onid  = (lptr[2] << 8) | lptr[3];

#if ENABLE_MPEGTS_DVB
    /* Create new muxes (auto-discovery) */
    r = dvb_nit_mux(mt, NULL, mm, onid, tsid, lptr, llen, tableid, bi, 1);
    if (r < 0)
      return r;
#endif

    /* Find existing mux */
#if ENABLE_MPEGTS_DVB
    if (tableid == DVB_FASTSCAN_NIT_BASE) {
      mux = dvb_fs_mux_find(mm, onid, tsid);
      if (mux && (mm == mux || mpegts_mux_alive(mux))) {
        r = dvb_nit_mux(mt, mux, mm, onid, tsid, lptr, llen, tableid, bi, 0);
        if (r < 0)
          return r;
      }
    } else
#endif
    LIST_FOREACH(mux, &mn->mn_muxes, mm_network_link)
      if (mux->mm_onid == onid && mux->mm_tsid == tsid &&
          (mm == mux || mpegts_mux_alive(mux))) {
        r = dvb_nit_mux(mt, mux, mm, onid, tsid, lptr, llen, tableid, bi, 0);
        if (r < 0)
          return r;
      }
      
    lptr += r;
    llen -= r;
  }

  /* End */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/**
 * DVB SDT (Service Description Table)
 */
static int
dvb_sdt_mux
  (mpegts_table_t *mt, mpegts_mux_t *mm, mpegts_mux_t *mm_orig,
   const uint8_t *ptr, int len, uint8_t tableid)
{
  uint32_t priv = 0;
  uint8_t dtag;
  const uint8_t *lptr, *dptr;
  int llen, dlen;
  mpegts_network_t *mn = mm->mm_network;
  char buf[128];

  mpegts_mux_nice_name(mm, buf, sizeof(buf));

  tvhdebug("sdt", "mux %s", buf);

  /* Service loop */
  while(len >= 5) {
    mpegts_service_t *s;
    int master = 0, save = 0, save2 = 0;
    uint16_t service_id                = ptr[0] << 8 | ptr[1];
    int      free_ca_mode              = (ptr[3] >> 4) & 0x1;
    int      stype = 0;
    char     sprov[256], sname[256], sauth[256];
    int      running_status            = (ptr[3] >> 5) & 0x7;
    const char *charset;
    *sprov = *sname = *sauth = 0;
    tvhdebug("sdt", "  sid %04X (%d) running %d free_ca %d",
             service_id, service_id, running_status, free_ca_mode);

    /* Initialise the loop */
    DVB_LOOP_INIT(ptr, len, 3, lptr, llen);
  
    /* Find service */
    s       = mpegts_service_find(mm, service_id, 0, 1, &save);
    charset = dvb_charset_find(mn, mm, s);

    if (s) {
      if (!s->s_enabled && s->s_auto == SERVICE_AUTO_PAT_MISSING) {
        tvhinfo("mpegts", "enabling service %s [sid %04X/%d] (found in SDT)",
                s->s_nicename, s->s_dvb_service_id, s->s_dvb_service_id);
        service_set_enabled((service_t *)s, 1, SERVICE_AUTO_NORMAL);
      }
      s->s_dvb_check_seen = dispatch_clock;
    }

    /* Descriptor loop */
    DVB_DESC_EACH(lptr, llen, dtag, dlen, dptr) {
      tvhtrace("sdt", "    dtag %02X dlen %d", dtag, dlen);
      switch (dtag) {
        case DVB_DESC_SERVICE:
          if (dvb_desc_service(dptr, dlen, &stype, sprov,
                                sizeof(sprov), sname, sizeof(sname), charset))
            return -1;
          break;
        case DVB_DESC_DEF_AUTHORITY:
          if (dvb_get_string(sauth, sizeof(sauth), dptr, dlen, charset, NULL))
            return -1;
          break;
        case DVB_DESC_PRIVATE_DATA:
          if (dlen == 4) {
            priv = (dptr[0] << 24) | (dptr[1] << 16) | (dptr[2] << 8) | dptr[3];
            tvhtrace(mt->mt_name, "  private %08X", priv);
          }
          break;
        case DVB_DESC_BSKYB_NVOD:
          if (priv == 2)
            if (dvb_get_string(sname, sizeof(sname), dptr, dlen, charset, NULL))
              return -1;
          break;
      }
    }

    tvhtrace("sdt", "  type %02X (%d) name [%s] provider [%s] def_auth [%s]",
             stype, stype, sname, sprov, sauth);
    if (!s) continue;

    /* Update service type */
    if (stype && s->s_dvb_servicetype != stype) {
      int r;
      s->s_dvb_servicetype = stype;
      save = 1;
      tvhtrace("sdt", "    type changed / old %02X (%i)",
               s->s_dvb_servicetype, s->s_dvb_servicetype);

      /* Set tvh service type */
      if ((r = dvb_servicetype_lookup(stype)) != -1)
        s->s_servicetype = r;
    }
    
    /* Check if this is master 
     * Some networks appear to provide diff service names on diff transponders
     */
    if (tableid == 0x42 || mm == mm_orig)
      master = 1;
    
    /* Update CRID authority */
    if (*sauth && strcmp(s->s_dvb_cridauth ?: "", sauth)) {
      tvh_str_update(&s->s_dvb_cridauth, sauth);
      save = 1;
      tvhtrace("sdt", "    cridauth changed");
    }

    /* Update name */
    if (*sname && strcmp(s->s_dvb_svcname ?: "", sname)) {
      if (!s->s_dvb_svcname || master) {
        tvh_str_update(&s->s_dvb_svcname, sname);
        save2 = 1;
        tvhtrace("sdt", "    name changed");
      }
    }
    
    /* Update provider */
    if (*sprov && strcmp(s->s_dvb_provider ?: "", sprov)) {
      if (!s->s_dvb_provider || master) {
        tvh_str_update(&s->s_dvb_provider, sprov);
        save2 = 1;
        tvhtrace("sdt", "    provider changed");
      }
    }

    /* Update nice name */
    if (save2) {
      pthread_mutex_lock(&s->s_stream_mutex);
      service_make_nicename((service_t*)s);
      pthread_mutex_unlock(&s->s_stream_mutex);
      tvhdebug("sdt", "  nicename %s", s->s_nicename);
      save = 1;
    }

    /* Save details */
    if (save) {
      idnode_changed(&s->s_id);
      service_refresh_channel((service_t*)s);
    }
  }

  return 0;
}

int
dvb_sdt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver, extraid;
  uint16_t onid, tsid;
  mpegts_mux_t     *mm = mt->mt_mux, *mm_orig = mm;
  mpegts_network_t *mn = mm->mm_network;
  mpegts_psi_table_state_t *st  = NULL;

  /* Begin */
  tsid    = ptr[0] << 8 | ptr[1];
  onid    = ptr[5] << 8 | ptr[6];
  extraid = ((int)onid) << 16 | tsid;
  if (tableid != 0x42 && tableid != 0x46) return -1;
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, extraid, 8, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* ID */
  tvhdebug("sdt", "onid %04X (%d) tsid %04X (%d)", onid, onid, tsid, tsid);

  /* Service descriptors */
  len -= 8;
  ptr += 8;

  /* Find Transport Stream */
  if (tableid == 0x42) {
    mpegts_mux_set_onid(mm, onid);
    mpegts_mux_set_tsid(mm, tsid, 1);
    r = dvb_sdt_mux(mt, mm, mm, ptr, len, tableid);
    if (r)
      return r;
  } else {
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
      if (mm->mm_onid == onid && mm->mm_tsid == tsid &&
          (mm == mm_orig || mpegts_mux_alive(mm))) {
        r = dvb_sdt_mux(mt, mm, mm_orig, ptr, len, tableid);
        if (r)
          return r;
      }
  }

  /* Done */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/*
 * ATSC VCT processing
 */
  /* Done */
int
atsc_vct_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int i, r, sect, last, ver, extraid, save, dlen;
  int maj, min, count;
  uint16_t tsid, sid, type;
  char chname[256];
  mpegts_mux_t     *mm = mt->mt_mux, *mm_orig = mm;
  mpegts_network_t *mn = mm->mm_network;
  mpegts_service_t *s;
  mpegts_psi_table_state_t *st  = NULL;

  /* Validate */
  if (tableid != 0xc8 && tableid != 0xc9) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, extraid, 7, &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhdebug("vct", "tsid %04X (%d)", tsid, tsid);

  /* # channels */
  count = ptr[6];
  tvhdebug("vct", "channel count %d", count);
  ptr  += 7;
  len  -= 7;
  for (i = 0; i < count && len >= 32; i++) {
    dlen = ((ptr[30] & 0x3) << 8) | ptr[31];
    if (dlen + 32 > len) return -1;

    /* Extract fields */
    atsc_utf16_to_utf8(ptr, 7, chname, sizeof(chname));
    maj  = (ptr[14] & 0xF) << 6 | ptr[15] >> 2;
    min  = (ptr[15] & 0x3) << 2 | ptr[16];
    tsid = (ptr[22]) << 8 | ptr[23];
    sid  = (ptr[24]) << 8 | ptr[25];
    type = ptr[27] & 0x3f;
    tvhdebug("vct", "tsid   %04X (%d)", tsid, tsid);
    tvhdebug("vct", "sid    %04X (%d)", sid, sid);
    tvhdebug("vct", "chname %s",    chname);
    tvhdebug("vct", "chnum  %d.%d", maj, min);
    tvhdebug("vct", "type   %02X (%d)", type, type);

    /* Skip */
    if (type > 3)
      goto next;

    /* Find mux */
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
      if (mm->mm_tsid == tsid && (mm == mm_orig || mpegts_mux_alive(mm))) {
        /* Find the service */
        if (!(s = mpegts_service_find(mm, sid, 0, 1, &save)))
          continue;

        /* Update */
        if (strcmp(s->s_dvb_svcname ?: "", chname)) {
          tvh_str_set(&s->s_dvb_svcname, chname);
          save = 1;
        }
        if (s->s_dvb_channel_num != maj || s->s_dvb_channel_minor != min) {
          s->s_dvb_channel_num = maj;
          s->s_dvb_channel_minor = min;
          save = 1;
        }
        /* Save */
        if (save)
          s->s_config_save((service_t*)s);
     }

    /* Move on */
next:
    ptr += dlen + 32;
    len -= dlen + 32;
  }

  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}


/*
 * DVB BAT processing
 */
int
dvb_bat_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  return dvb_nit_callback(mt, ptr, len, tableid);
}

#if ENABLE_MPEGTS_DVB
static int
dvb_fs_sdt_mux
  ( mpegts_table_t *mt, mpegts_mux_t *mm, mpegts_psi_table_state_t *st,
    const uint8_t *ptr, int len, int discovery )
{
  uint16_t onid, tsid, service_id;
  uint8_t dtag;
  int llen, dlen;
  const uint8_t *lptr, *dptr;
  mpegts_network_t *mn;
  mpegts_mux_t *mux;

  while (len > 0) {
    const char *charset;
    char sprov[256] = "", sname[256] = "";
    mpegts_service_t *s;
    int stype = 0, save = 0;

    onid = (ptr[0] << 8) | ptr[1];
    tsid = (ptr[2] << 8) | ptr[3];
    service_id = (ptr[4] << 8) | ptr[5];
    /* (ptr[6] << 8) | ptr[7]   - video pid */
    /* (ptr[7] << 8) | ptr[8]   - audio pid */
    /* (ptr[9] << 8) | ptr[10]  - video ecm pid */
    /* (ptr[10] << 8) | ptr[12] - audio ecm pid */
    /* (ptr[14] << 8) | ptr[15] - pcr pid */

    /* Initialise the loop */
    DVB_LOOP_INIT(ptr, len, 16, lptr, llen);

    if (discovery) {
      /* Descriptor loop */
      DVB_DESC_EACH(lptr, llen, dtag, dlen, dptr) {
        switch (dtag) {
          case DVB_DESC_SAT_DEL:
            tvhtrace(mt->mt_name, "    dtag %02X dlen %d (discovery) onid %04X (%d) tsid %04X (%d)",
                     dtag, dlen, onid, onid, tsid, tsid);
            mux = dvb_desc_sat_del(mt, mm, onid, tsid, dptr, dlen, 1);
            if (mux) {
              mpegts_mux_set_onid(mux, onid);
              mpegts_mux_set_tsid(mux, tsid, 0);
              dvb_fs_mux_add(mt, mm, mux);
            }
            break;
        }
      }
      continue;
    }

    tvhdebug(mt->mt_name, "  service %04X (%d) onid %04X (%d) tsid %04X (%d)",
             service_id, service_id, onid, onid, tsid, tsid);

    /* Find existing mux */
    mux = dvb_fs_mux_find(mm, onid, tsid);
    if (mux == NULL) {
      tvhtrace(mt->mt_name, "    mux not found");
      continue;
    }
    mn = mux->mm_network;

    /* Find service */
    s       = mpegts_service_find(mux, service_id, 0, 1, &save);
    charset = dvb_charset_find(mn, mux, s);

    /* Descriptor loop */
    DVB_DESC_EACH(lptr, llen, dtag, dlen, dptr) {
      tvhtrace(mt->mt_name, "    dtag %02X dlen %d", dtag, dlen);
      switch (dtag) {
        case DVB_DESC_SERVICE:
          if (dvb_desc_service(dptr, dlen, &stype, sprov,
                               sizeof(sprov), sname, sizeof(sname), charset))
            return -1;
          break;
      }
    }

    tvhtrace(mt->mt_name, "    type %d name [%s] provider [%s]",
             stype, sname, sprov);

    /* Update service type */
    if (stype && !s->s_dvb_servicetype) {
      int r;
      s->s_dvb_servicetype = stype;
      save = 1;
      tvhtrace(mt->mt_name, "    type changed");

      /* Set tvh service type */
      if ((r = dvb_servicetype_lookup(stype)) != -1)
        s->s_servicetype = r;
    }

    /* Update name */
    if (*sname && strcmp(s->s_dvb_svcname ?: "", sname)) {
      if (!s->s_dvb_svcname) {
        tvh_str_update(&s->s_dvb_svcname, sname);
        save = 1;
        tvhtrace(mt->mt_name, "    name changed");
      }
    }

    /* Update provider */
    if (*sprov && strcmp(s->s_dvb_provider ?: "", sprov)) {
      if (!s->s_dvb_provider) {
        tvh_str_update(&s->s_dvb_provider, sprov);
        save = 1;
        tvhtrace(mt->mt_name, "    provider changed");
      }
    }

    if (save) {
      /* Update nice name */
      pthread_mutex_lock(&s->s_stream_mutex);
      service_make_nicename((service_t*)s);
      pthread_mutex_unlock(&s->s_stream_mutex);
      tvhdebug(mt->mt_name, "  nicename %s", s->s_nicename);
      /* Save changes */
      idnode_changed(&s->s_id);
      service_refresh_channel((service_t*)s);
    }

    if (!st->working) {
      st->working = 1;
      mt->mt_working++;
      mt->mt_flags |= MT_FASTSWITCH;
    }
  }

  return 0;
}


/*
 * DVB fastscan table processing
 */
int
dvb_fs_sdt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  uint16_t nbid;
  mpegts_mux_t *mm = mt->mt_mux;
  mpegts_psi_table_state_t *st  = NULL;

  /* Fastscan ID */
  nbid = (ptr[0] << 8) | ptr[1];

  /* Begin */
  if (tableid != 0xBD)
    return -1;
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, nbid, 7, &st, &sect, &last, &ver);
  if (r == 0) {
    mt->mt_working -= st->working;
    st->working = 0;
  }
  if (r != 1) return r;
  if (len < 5) return -1;
  ptr += 5;
  len -= 5;

  dvb_fs_sdt_mux(mt, mm, st, ptr, len, 1);
  dvb_fs_sdt_mux(mt, mm, st, ptr, len, 0);

  /* End */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}
#endif

/**
 * PMT update reason flags
 */
#define PMT_UPDATE_PCR                0x1
#define PMT_UPDATE_NEW_STREAM         0x2
#define PMT_UPDATE_LANGUAGE           0x4
#define PMT_UPDATE_AUDIO_TYPE         0x8
#define PMT_UPDATE_FRAME_DURATION     0x10
#define PMT_UPDATE_COMPOSITION_ID     0x20
#define PMT_UPDATE_ANCILLARY_ID       0x40
#define PMT_UPDATE_STREAM_DELETED     0x80
#define PMT_UPDATE_NEW_CA_STREAM      0x100
#define PMT_UPDATE_NEW_CAID           0x200
#define PMT_UPDATE_CA_PROVIDER_CHANGE 0x400
#define PMT_UPDATE_PARENT_PID         0x800
#define PMT_UPDATE_CAID_DELETED       0x1000
#define PMT_REORDERED                 0x2000

/**
 * Add a CA descriptor
 */
static int
psi_desc_add_ca
  (mpegts_service_t *t, uint16_t caid, uint32_t provid, uint16_t pid)
{
  elementary_stream_t *st;
  caid_t *c;
  int r = 0;

  tvhdebug("pmt", "  caid %04X (%s) provider %08X pid %04X",
           caid, caid2name(caid), provid, pid);

  if((st = service_stream_find((service_t*)t, pid)) == NULL) {
    st = service_stream_create((service_t*)t, pid, SCT_CA);
    r |= PMT_UPDATE_NEW_CA_STREAM;
  }

  st->es_delete_me = 0;

  st->es_position = 0x40000;

  LIST_FOREACH(c, &st->es_caids, link) {
    if(c->caid == caid) {
      c->pid = pid;

      if(c->providerid != provid) {
        c->providerid = provid;
        r |= PMT_UPDATE_CA_PROVIDER_CHANGE;
      }
      return r;
    }
  }

  c = malloc(sizeof(caid_t));

  c->caid = caid;
  c->providerid = provid;
  c->use = 1;
  c->pid = pid;
  LIST_INSERT_HEAD(&st->es_caids, c, link);
  r |= PMT_UPDATE_NEW_CAID;
  return r;
}

/**
 * Parser for CA descriptors
 */
static int 
psi_desc_ca(mpegts_service_t *t, const uint8_t *buffer, int size)
{
  int r = 0;
  int i;
  uint32_t provid = 0;
  uint16_t caid = (buffer[0] << 8) | buffer[1];
  uint16_t pid = ((buffer[2]&0x1F) << 8) | buffer[3];

  switch (caid & 0xFF00) {
  case 0x0100: // SECA/Mediaguard
    provid = (buffer[4] << 8) | buffer[5];

    //Add extra providers, if any
    for (i = 17; i < size; i += 15){
      uint16_t xpid = ((buffer[i]&0x1F) << 8) | buffer[i + 1];
      uint16_t xprovid = (buffer[i + 2] << 8) | buffer[i + 3];

      r |= psi_desc_add_ca(t, caid, xprovid, xpid);
    }
    break;
  case 0x0500:// Viaccess
    for (i = 4; i < size;) {
      unsigned char nano = buffer[i++];
      unsigned char nanolen = buffer[i++];

      if (nano == 0x14) {
        provid = (buffer[i] << 16) | (buffer[i + 1] << 8) | (buffer[i + 2] & 0xf0);
        break;
      }

      i += nanolen;
    }
    break;
  case 0x4a00:
    if (caid == 0x4ad2)//streamguard
       provid=0;
    if (caid != 0x4aee && caid != 0x4ad2) { // Bulcrypt
       provid = size < 4 ? 0 : buffer[4];
    }
    break;
  default:
    provid = 0;
    break;
  }

  r |= psi_desc_add_ca(t, caid, provid, pid);

  return r;
}

/**
 * Parser for teletext descriptor
 */
static int
psi_desc_teletext(mpegts_service_t *t, const uint8_t *ptr, int size,
      int parent_pid, int *position)
{
  int r = 0;
  const char *lang;
  elementary_stream_t *st;

  while(size >= 5) {
    int page = (ptr[3] & 0x7 ?: 8) * 100 + (ptr[4] >> 4) * 10 + (ptr[4] & 0xf);
    int type = ptr[3] >> 3;

    if(type == 2 || type == 5) {
      // 2 = subtitle page, 5 = subtitle page [hearing impaired]

      // We put the teletext subtitle driven streams on a list of pids
      // higher than normal MPEG TS (0x2000 ++)
      int pid = DVB_TELETEXT_BASE + page;
    
      if((st = service_stream_find((service_t*)t, pid)) == NULL) {
        r |= PMT_UPDATE_NEW_STREAM;
        st = service_stream_create((service_t*)t, pid, SCT_TEXTSUB);
        st->es_delete_me = 1;
      }

  
      lang = lang_code_get2((const char*)ptr, 3);
      if(memcmp(st->es_lang,lang,3)) {
        r |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
      }

      if(st->es_parent_pid != parent_pid) {
        r |= PMT_UPDATE_PARENT_PID;
        st->es_parent_pid = parent_pid;
      }

      // Check es_delete_me so we only compute position once per PMT update
      if(st->es_position != *position && st->es_delete_me) {
        st->es_position = *position;
        r |= PMT_REORDERED;
      }
      st->es_delete_me = 0;
      (*position)++;
    }
    ptr += 5;
    size -= 5;
  }
  return r;
}

/** 
 * PMT parser, from ISO 13818-1 and ETSI EN 300 468
 */
static int
psi_parse_pmt
  (mpegts_mux_t *mux, mpegts_service_t *t, const uint8_t *ptr, int len)
{
  int ret = 0;
  uint16_t pcr_pid, pid;
  uint8_t estype;
  int dllen;
  uint8_t dtag, dlen;
  streaming_component_type_t hts_stream_type;
  elementary_stream_t *st, *next;
  int update = 0;
  int composition_id;
  int ancillary_id;
  int version;
  int position;
  int tt_position;
  const char *lang;
  uint8_t audio_type;

  caid_t *c, *cn;

  lock_assert(&t->s_stream_mutex);

  version = ptr[2] >> 1 & 0x1f;
  pcr_pid = (ptr[5] & 0x1f) << 8 | ptr[6];
  dllen   = (ptr[7] & 0xf) << 8 | ptr[8];
  
  if(t->s_pcr_pid != pcr_pid) {
    t->s_pcr_pid = pcr_pid;
    update |= PMT_UPDATE_PCR;
  }
  tvhdebug("pmt", "  pcr_pid %04X", pcr_pid);

  ptr += 9;
  len -= 9;

  /* Mark all streams for deletion */
  TAILQ_FOREACH(st, &t->s_components, es_link) {
    st->es_delete_me = 1;

    LIST_FOREACH(c, &st->es_caids, link)
      c->pid = CAID_REMOVE_ME;
  }

  // Common descriptors
  while(dllen > 1) {
    dtag = ptr[0];
    dlen = ptr[1];

    tvhlog_hexdump("pmt", ptr, dlen + 2);
    len -= 2; ptr += 2; dllen -= 2; 
    if(dlen > len)
      break;

    switch(dtag) {
    case DVB_DESC_CA:
      update |= psi_desc_ca(t, ptr, dlen);
      break;

    default:
      break;
    }
    len -= dlen; ptr += dlen; dllen -= dlen;
  }

  while(len >= 5) {
    estype  = ptr[0];
    pid     = (ptr[1] & 0x1f) << 8 | ptr[2];
    dllen   = (ptr[3] & 0xf) << 8 | ptr[4];
    tvhdebug("pmt", "  pid %04X estype %d", pid, estype);
    tvhlog_hexdump("pmt", ptr, 5);

    ptr += 5;
    len -= 5;

    hts_stream_type = SCT_UNKNOWN;
    composition_id = -1;
    ancillary_id = -1;
    position = 0;
    tt_position = 1000;
    lang = NULL;
    audio_type = 0;

    switch(estype) {
    case 0x01:
    case 0x02:
    case 0x80: // 0x80 is DigiCipher II (North American cable) encrypted MPEG-2
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
      hts_stream_type = SCT_MPEG2AUDIO;
      break;

    case 0x06:
      /* 0x06 is Chinese Cable TV AC-3 audio track */
      /* but mark it so only when no more descriptors exist */
      if (dllen > 1 || !mux || mux->mm_pmt_ac3 != MM_AC3_PMT_06)
        break;
      /* fall through to SCT_AC3 */
    case 0x81:
      hts_stream_type = SCT_AC3;
      break;
    
    case 0x0f:
      hts_stream_type = SCT_MP4A;
      break;

    case 0x11:
      hts_stream_type = SCT_AAC;
      break;

    case 0x1b:
      hts_stream_type = SCT_H264;
      break;

    case 0x24:
      hts_stream_type = SCT_HEVC;
      break;

    default:
      break;
    }

    while(dllen > 1) {
      dtag = ptr[0];
      dlen = ptr[1];

      tvhlog_hexdump("pmt", ptr, dlen + 2);
      len -= 2; ptr += 2; dllen -= 2; 
      if(dlen > len)
        break;

      switch(dtag) {
      case DVB_DESC_CA:
        update |= psi_desc_ca(t, ptr, dlen);
        break;

      case DVB_DESC_REGISTRATION:
        if(mux->mm_pmt_ac3 != MM_AC3_PMT_N05 && dlen == 4 &&
           ptr[0] == 'A' && ptr[1] == 'C' && ptr[2] == '-' &&  ptr[3] == '3')
          hts_stream_type = SCT_AC3;
        /* seen also these formats: */
        /* LU-A, ADV1 */
        break;

      case DVB_DESC_LANGUAGE:
        lang = lang_code_get2((const char*)ptr, 3);
        audio_type = ptr[3];
        break;

      case DVB_DESC_TELETEXT:
        if(estype == 0x06)
          hts_stream_type = SCT_TELETEXT;
  
        update |= psi_desc_teletext(t, ptr, dlen, pid, &tt_position);
        break;

      case DVB_DESC_AC3:
        if(estype == 0x06 || estype == 0x81)
          hts_stream_type = SCT_AC3;
        break;

      case DVB_DESC_AAC:
        if(estype == 0x0f)
          hts_stream_type = SCT_MP4A;
        else if(estype == 0x11)
          hts_stream_type = SCT_AAC;
        break;

      case DVB_DESC_SUBTITLE:
        if(dlen < 8)
          break;

        lang = lang_code_get2((const char*)ptr, 3);
        composition_id = ptr[4] << 8 | ptr[5];
        ancillary_id   = ptr[6] << 8 | ptr[7];
        hts_stream_type = SCT_DVBSUB;
        break;

      case DVB_DESC_EAC3:
        if(estype == 0x06 || estype == 0x81)
          hts_stream_type = SCT_EAC3;
        break;

      default:
        break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }
    
    if(hts_stream_type != SCT_UNKNOWN) {

      if((st = service_stream_find((service_t*)t, pid)) == NULL) {
        update |= PMT_UPDATE_NEW_STREAM;
        st = service_stream_create((service_t*)t, pid, hts_stream_type);
      }

      st->es_type = hts_stream_type;

      st->es_delete_me = 0;

      tvhdebug("pmt", "  type %s position %d",
               streaming_component_type2txt(st->es_type), position);
      if (lang)
        tvhdebug("pmt", "  language %s", lang);
      if (composition_id != -1)
        tvhdebug("pmt", "  composition_id %d", composition_id);
      if (ancillary_id != -1)
        tvhdebug("pmt", "  ancillary_id %d", ancillary_id);

      if(st->es_position != position) {
        update |= PMT_REORDERED;
        st->es_position = position;
      }

      if(lang && memcmp(st->es_lang, lang, 3)) {
        update |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
      }

      if(st->es_audio_type != audio_type) {
        update |= PMT_UPDATE_AUDIO_TYPE;
        st->es_audio_type = audio_type;
      }

      if(composition_id != -1 && st->es_composition_id != composition_id) {
        st->es_composition_id = composition_id;
        update |= PMT_UPDATE_COMPOSITION_ID;
      }

      if(ancillary_id != -1 && st->es_ancillary_id != ancillary_id) {
        st->es_ancillary_id = ancillary_id;
        update |= PMT_UPDATE_ANCILLARY_ID;
      }
    }
    position++;
  }

  /* Scan again to see if any streams should be deleted */
  for(st = TAILQ_FIRST(&t->s_components); st != NULL; st = next) {
    next = TAILQ_NEXT(st, es_link);

    for(c = LIST_FIRST(&st->es_caids); c != NULL; c = cn) {
      cn = LIST_NEXT(c, link);
      if (c->pid == CAID_REMOVE_ME) {
        LIST_REMOVE(c, link);
        free(c);
        update |= PMT_UPDATE_CAID_DELETED;
      }
    }


    if(st->es_delete_me) {
      service_stream_destroy((service_t*)t, st);
      update |= PMT_UPDATE_STREAM_DELETED;
    }
  }

  if(update & PMT_REORDERED)
    sort_elementary_streams((service_t*)t);

  if(update) {
    tvhdebug("pmt", "Service \"%s\" PMT (version %d) updated"
     "%s%s%s%s%s%s%s%s%s%s%s%s%s",
     service_nicename((service_t*)t), version,
     update&PMT_UPDATE_PCR               ? ", PCR PID changed":"",
     update&PMT_UPDATE_NEW_STREAM        ? ", New elementary stream":"",
     update&PMT_UPDATE_LANGUAGE          ? ", Language changed":"",
     update&PMT_UPDATE_FRAME_DURATION    ? ", Frame duration changed":"",
     update&PMT_UPDATE_COMPOSITION_ID    ? ", Composition ID changed":"",
     update&PMT_UPDATE_ANCILLARY_ID      ? ", Ancillary ID changed":"",
     update&PMT_UPDATE_STREAM_DELETED    ? ", Stream deleted":"",
     update&PMT_UPDATE_NEW_CA_STREAM     ? ", New CA stream":"",
     update&PMT_UPDATE_NEW_CAID          ? ", New CAID":"",
     update&PMT_UPDATE_CA_PROVIDER_CHANGE? ", CA provider changed":"",
     update&PMT_UPDATE_PARENT_PID        ? ", Parent PID changed":"",
     update&PMT_UPDATE_CAID_DELETED      ? ", CAID deleted":"",
     update&PMT_REORDERED                ? ", PIDs reordered":"");
    
    service_request_save((service_t*)t, 1);

    // Only restart if something that our clients worry about did change
    if(update & ~(PMT_UPDATE_NEW_CA_STREAM |
      PMT_UPDATE_NEW_CAID |
      PMT_UPDATE_CA_PROVIDER_CHANGE | 
      PMT_UPDATE_CAID_DELETED)) {
      if(t->s_status == SERVICE_RUNNING)
        ret = 1;
    }
    
    // notify descrambler that we found another CAIDs
    if (update & PMT_UPDATE_NEW_CAID)
      descrambler_caid_changed((service_t *)t);
  }
  return ret;
}

/**
 * TDT parser, from ISO 13818-1 and ETSI EN 300 468
 */

static void dvb_time_update(const uint8_t *ptr, const char *srcname)
{
  static time_t dvb_last_update = 0;
  time_t t;
  if (dvb_last_update + 1800 < dispatch_clock) {
    t = dvb_convert_date(ptr, 0);
    tvhtime_update(t, srcname);
    dvb_last_update = dispatch_clock;
  }
}

int
dvb_tdt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  if (len == 5) {
    dvb_time_update(ptr, "TDT");
    mt->mt_incomplete = 0;
    mt->mt_complete = 1;
    mt->mt_finished = 1;
  }
  return 0;
}

/**
 * TOT parser, from ISO 13818-1 and ETSI EN 300 468
 */
int
dvb_tot_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  if (len >= 5) {
    dvb_time_update(ptr, "TOT");
    mt->mt_incomplete = 0;
    mt->mt_complete = 1;
    mt->mt_finished = 1;
  }
  return 0;
}

/*
 * Install default table sets
 */

static void
psi_tables_default ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_PAT_BASE, DVB_PAT_MASK, dvb_pat_callback,
                   NULL, "pat", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_PAT_PID, MPS_WEIGHT_PAT);
  mpegts_table_add(mm, DVB_CAT_BASE, DVB_CAT_MASK, dvb_cat_callback,
                   NULL, "cat", MT_QUICKREQ | MT_CRC, DVB_CAT_PID,
                   MPS_WEIGHT_CAT);
}

#if ENABLE_MPEGTS_DVB
static void
psi_tables_dvb_fastscan( void *aux, bouquet_t *bq, const char *name, int pid )
{
  tvhtrace("fastscan", "adding table %04X (%i) for '%s'", pid, pid, name);
  mpegts_table_add(aux, DVB_FASTSCAN_NIT_BASE, DVB_FASTSCAN_MASK,
                   dvb_nit_callback, bq, "fs_nit", MT_CRC, pid,
                   MPS_WEIGHT_NIT2);
  mpegts_table_add(aux, DVB_FASTSCAN_SDT_BASE, DVB_FASTSCAN_MASK,
                   dvb_fs_sdt_callback, NULL, "fs_sdt", MT_CRC, pid,
                   MPS_WEIGHT_SDT2);
}
#endif

static void
psi_tables_dvb ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_NIT_BASE, DVB_NIT_MASK, dvb_nit_callback,
                   NULL, "nit", MT_QUICKREQ | MT_CRC, DVB_NIT_PID,
                   MPS_WEIGHT_NIT);
  mpegts_table_add(mm, DVB_SDT_BASE, DVB_SDT_MASK, dvb_sdt_callback,
                   NULL, "sdt", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_SDT_PID, MPS_WEIGHT_SDT);
  mpegts_table_add(mm, DVB_BAT_BASE, DVB_BAT_MASK, dvb_bat_callback,
                   NULL, "bat", MT_CRC, DVB_BAT_PID, MPS_WEIGHT_BAT);
  if (tvhtime_update_enabled) {
    mpegts_table_add(mm, DVB_TDT_BASE, DVB_TDT_MASK, dvb_tdt_callback,
                     NULL, "tdt", MT_ONESHOT | MT_QUICKREQ | MT_RECORD,
                     DVB_TDT_PID, MPS_WEIGHT_TDT);
    mpegts_table_add(mm, DVB_TOT_BASE, DVB_TOT_MASK, dvb_tot_callback,
                     NULL, "tot", MT_ONESHOT | MT_QUICKREQ | MT_CRC | MT_RECORD,
                     DVB_TDT_PID, MPS_WEIGHT_TDT);
  }
#if ENABLE_MPEGTS_DVB
  if (idnode_is_instance(&mm->mm_id, &dvb_mux_dvbs_class)) {
    dvb_mux_conf_t *mc = &((dvb_mux_t *)mm)->lm_tuning;
    if (mc->dmc_fe_type == DVB_TYPE_S)
      dvb_fastscan_each(mm, mc->u.dmc_fe_qpsk.orbital_pos,
                        mc->dmc_fe_freq, mc->u.dmc_fe_qpsk.polarisation, psi_tables_dvb_fastscan);
  }
#endif
}

static void
psi_tables_atsc_c ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_VCT_C_BASE, DVB_VCT_MASK, atsc_vct_callback,
                   NULL, "vct", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_VCT_PID, MPS_WEIGHT_VCT);
}

static void
psi_tables_atsc_t ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_VCT_T_BASE, DVB_VCT_MASK, atsc_vct_callback,
                   NULL, "vct", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_VCT_PID, MPS_WEIGHT_VCT);
}

void
psi_tables_install ( mpegts_input_t *mi, mpegts_mux_t *mm,
                     dvb_fe_delivery_system_t delsys)
{
  if (mi == NULL || mm == NULL)
    return;

  psi_tables_default(mm);

  switch (delsys) {
  case DVB_SYS_DVBC_ANNEX_A:
  case DVB_SYS_DVBC_ANNEX_C:
  case DVB_SYS_DVBT:
  case DVB_SYS_DVBT2:
  case DVB_SYS_DVBS:
  case DVB_SYS_DVBS2:
  case DVB_SYS_ISDBS:
    psi_tables_dvb(mm);
    break;
  case DVB_SYS_TURBO:
  case DVB_SYS_ATSC:
  case DVB_SYS_ATSCMH:
    psi_tables_atsc_t(mm);
    break;
  case DVB_SYS_DVBC_ANNEX_B:
    psi_tables_atsc_c(mm);
    break;
  case DVB_SYS_NONE:
  case DVB_SYS_DVBH:
  case DVB_SYS_ISDBT:
  case DVB_SYS_ISDBC:
  case DVB_SYS_DTMB:
  case DVB_SYS_CMMB:
  case DVB_SYS_DSS:
  case DVB_SYS_DAB:
    break;
  case DVB_SYS_ATSC_ALL:
    psi_tables_atsc_c(mm);
    psi_tables_atsc_t(mm);
    break;
  case DVB_SYS_UNKNOWN:
    psi_tables_dvb(mm);
    psi_tables_atsc_c(mm);
    psi_tables_atsc_t(mm);
    break;
    break;
  }

  mpegts_mux_update_pids(mm);
}
