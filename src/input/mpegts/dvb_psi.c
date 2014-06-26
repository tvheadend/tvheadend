/*
 *  MPEG TS Program Specific Information code
 *  Copyright (C) 2007 - 2010 Andreas �man
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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

SKEL_DECLARE(mpegts_table_state_skel, struct mpegts_table_state);

static int
psi_parse_pmt(mpegts_service_t *t, const uint8_t *ptr, int len);

/* **************************************************************************
 * Lookup tables
 * *************************************************************************/

static const int dvb_servicetype_map[][2] = {
  { 0x01, ST_SDTV  }, /* SDTV (MPEG2) */
  { 0x02, ST_RADIO }, 
  { 0x11, ST_HDTV  }, /* HDTV (MPEG2) */
  { 0x16, ST_SDTV  }, /* Advanced codec SDTV */
  { 0x19, ST_HDTV  }, /* Advanced codec HDTV */
  { 0x80, ST_SDTV  }, /* NET POA - Cabo SDTV */
  { 0x91, ST_HDTV  }, /* Bell TV HDTV */
  { 0x96, ST_SDTV  }, /* Bell TV SDTV */
  { 0xA0, ST_HDTV  }, /* Bell TV tiered HDTV */
  { 0xA4, ST_HDTV  }, /* DN HDTV */
  { 0xA6, ST_HDTV  }, /* Bell TV tiered HDTV */
  { 0xA8, ST_SDTV  }, /* DN advanced SDTV */
  { 0xD3, ST_SDTV  }, /* SKY TV SDTV */
};

int
dvb_servicetype_lookup ( int t )
{
  int i;
  for (i = 0; i < ARRAY_SIZE(dvb_servicetype_map); i++) {
    if (dvb_servicetype_map[i][0] == t)
      return dvb_servicetype_map[i][1];
  }
  return -1;
}

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
  (mpegts_mux_t *mm, uint16_t onid, uint16_t tsid,
   const uint8_t *ptr, int len )
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
    tvhlog(LOG_WARNING, "nit", "dvb-s frequency error");
    return NULL;
  }
  if (!symrate) {
    tvhlog(LOG_WARNING, "nit", "dvb-s symbol rate error");
    return NULL;
  }

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_type                = DVB_TYPE_S;
  dmc.dmc_fe_pilot               = DVB_PILOT_AUTO;
  dmc.dmc_fe_inversion           = DVB_INVERSION_AUTO;
  dmc.dmc_fe_freq                = frequency;
  dmc.u.dmc_fe_qpsk.orbital_pos  = bcdtoint(ptr[4]) * 100 + bcdtoint(ptr[5]);
  dmc.u.dmc_fe_qpsk.orbital_dir  = (ptr[6] & 0x80) ? 'E' : 'W';
  dmc.u.dmc_fe_qpsk.polarisation = (ptr[6] >> 5) & 0x03;

  dmc.u.dmc_fe_qpsk.symbol_rate  = symrate * 100;
  dmc.u.dmc_fe_qpsk.fec_inner    = fec_tab[ptr[10] & 0x0f];
  
  static int mtab[4] = {
    DVB_MOD_NONE, DVB_MOD_QPSK, DVB_MOD_PSK_8, DVB_MOD_QAM_16
  };
  static int rtab[4] = {
    DVB_ROLLOFF_35, DVB_ROLLOFF_25, DVB_ROLLOFF_20, DVB_ROLLOFF_AUTO
  };
  dmc.dmc_fe_delsys     = (ptr[6] & 0x4) ? DVB_SYS_DVBS2 : DVB_SYS_DVBS;
  dmc.dmc_fe_modulation = mtab[ptr[6] & 0x3];
  dmc.dmc_fe_rolloff    = rtab[(ptr[6] >> 3) & 0x3];
  if (dmc.dmc_fe_delsys == DVB_SYS_DVBS &&
      dmc.dmc_fe_rolloff != DVB_ROLLOFF_35) {
    tvhwarn("nit", "dvb-s rolloff error");
    return NULL;
  }

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug("nit", "    %s", buf);

  /* Create */
  return mm->mm_network->mn_create_mux(mm, onid, tsid, &dmc);
}

/*
 * Cable delivery descriptor
 */
static mpegts_mux_t *
dvb_desc_cable_del
  (mpegts_mux_t *mm, uint16_t onid, uint16_t tsid,
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
    tvhwarn("nit", "dvb-c frequency error");
    return NULL;
  }
  if (!symrate) {
    tvhwarn("nit", "dvb-c symbol rate error");
    return NULL;
  }

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_type            = DVB_TYPE_C;
  dmc.dmc_fe_delsys          = DVB_SYS_DVBC_ANNEX_A;
  dmc.dmc_fe_inversion       = DVB_INVERSION_AUTO;
  dmc.dmc_fe_freq            = frequency * 100;

  dmc.u.dmc_fe_qam.symbol_rate  = symrate * 100;
  if((ptr[6] & 0x0f) >= sizeof(qtab))
    dmc.dmc_fe_modulation    = DVB_MOD_QAM_AUTO;
  else
    dmc.dmc_fe_modulation    = qtab[ptr[6] & 0x0f];
  dmc.u.dmc_fe_qam.fec_inner = fec_tab[ptr[10] & 0x07];

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug("nit", "    %s", buf);

  /* Create */
  return mm->mm_network->mn_create_mux(mm, onid, tsid, &dmc);
}

/*
 * Terrestrial delivery descriptor
 */
static mpegts_mux_t *
dvb_desc_terr_del
  (mpegts_mux_t *mm, uint16_t onid, uint16_t tsid,
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
    tvhwarn("nit", "dvb-c frequency error");
    return NULL;
  }

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_type             = DVB_TYPE_T;
  dmc.dmc_fe_delsys           = DVB_SYS_DVBT;
  dmc.dmc_fe_inversion        = DVB_INVERSION_AUTO;
  dmc.dmc_fe_freq             = frequency * 10;

  dmc.u.dmc_fe_ofdm.bandwidth             = btab[(ptr[4] >> 5) & 0x7];
  dmc.dmc_fe_modulation                   = ctab[(ptr[5] >> 6) & 0x3];
  dmc.u.dmc_fe_ofdm.hierarchy_information = htab[(ptr[5] >> 3) & 0x3];
  dmc.u.dmc_fe_ofdm.code_rate_HP          = fec_tab[(ptr[5] + 1) & 0x7];
  dmc.u.dmc_fe_ofdm.code_rate_LP          = fec_tab[((ptr[6] + 1) >> 5) & 0x7];
  dmc.u.dmc_fe_ofdm.guard_interval        = gtab[(ptr[6] >> 3) & 0x3];
  dmc.u.dmc_fe_ofdm.transmission_mode     = ttab[(ptr[6] >> 1) & 0x3];

  /* Debug */
  dvb_mux_conf_str(&dmc, buf, sizeof(buf));
  tvhdebug("nit", "    %s", buf);
  
  /* Create */
  return mm->mm_network->mn_create_mux(mm, onid, tsid, &dmc);
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

static int
dvb_desc_service_list
  ( const char *dstr, const uint8_t *ptr, int len, mpegts_mux_t *mm )
{
  uint16_t stype, sid;
  int i;
  mpegts_service_t *s;
  for (i = 0; i < len; i += 3) {
    sid   = (ptr[i] << 8) | ptr[i+1];
    stype = ptr[i+2];
    tvhdebug(dstr, "    service %04X (%d) type %d", sid, sid, stype);
    if (mm) {
      int save = 0;
      s = mpegts_service_find(mm, sid, 0, 1, &save);
      if (save)
        s->s_config_save((service_t*)s);
    }
  }
  return 0;
}

static int
dvb_desc_local_channel
  ( const char *dstr, const uint8_t *ptr, int len, mpegts_mux_t *mm )
{
  int save = 0;
  uint16_t sid, lcn;

  while(len >= 4) {
    sid = (ptr[0] << 8) | ptr[1];
    lcn = ((ptr[2] & 3) << 8) | ptr[3];
    tvhdebug(dstr, "    sid %d lcn %d", sid, lcn);
    if (lcn && mm) {
      mpegts_service_t *s = mpegts_service_find(mm, sid, 0, 0, &save);
      if (s) {
        if (s->s_dvb_channel_num != lcn) {
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



/* **************************************************************************
 * Tables
 * *************************************************************************/

static int sect_cmp
  ( struct mpegts_table_state *a, struct mpegts_table_state *b )
{
  if (a->tableid != b->tableid)
    return a->tableid - b->tableid;
  if (a->extraid < b->extraid)
    return -1;
  if (a->extraid > b->extraid)
    return 1;
  return 0;
}

static void
mpegts_table_state_reset
  ( mpegts_table_t *mt, mpegts_table_state_t *st, int last )
{
  int i;
  if (st->complete == 2) {
    mt->mt_complete--;
    mt->mt_incomplete++;
  }
  mt->mt_finished = 0;
  st->complete = 0;
  memset(st->sections, 0, sizeof(st->sections));
  for (i = 0; i < last / 32; i++)
    st->sections[i] = 0xFFFFFFFF;
  st->sections[last / 32] = 0xFFFFFFFF << (31 - (last % 32));
}

static struct mpegts_table_state *
mpegts_table_state_find
  ( mpegts_table_t *mt, int tableid, uint64_t extraid, int last )
{
  struct mpegts_table_state *st;

  /* Find state */
  SKEL_ALLOC(mpegts_table_state_skel);
  mpegts_table_state_skel->tableid = tableid;
  mpegts_table_state_skel->extraid = extraid;
  st = RB_INSERT_SORTED(&mt->mt_state, mpegts_table_state_skel, link, sect_cmp);
  if (!st) {
    st   = mpegts_table_state_skel;
    SKEL_USED(mpegts_table_state_skel);
    mt->mt_incomplete++;
    mpegts_table_state_reset(mt, st, last);
  }
  return st;
}

/*
 * End table
 */
static int
dvb_table_complete
  (mpegts_table_t *mt)
{
  if (mt->mt_incomplete || !mt->mt_complete) {
    int total = 0;
    mpegts_table_state_t *st;
    RB_FOREACH(st, &mt->mt_state, link)
      total++;
    tvhtrace(mt->mt_name, "incomplete %d complete %d total %d",
             mt->mt_incomplete, mt->mt_complete, total);
    return 2;
  }
  if (!mt->mt_finished)
    tvhdebug(mt->mt_name, "completed pid %d table %08X / %08x", mt->mt_pid, mt->mt_table, mt->mt_mask);
  mt->mt_finished = 1;
  return 0;
}

int
dvb_table_end
  (mpegts_table_t *mt, mpegts_table_state_t *st, int sect)
{
  int sa, sb;
  uint32_t rem;
  if (st) {
    assert(sect >= 0 && sect <= 255);
    sa = sect / 32;
    sb = sect % 32;
    st->sections[sa] &= ~(0x1 << (31 - sb));
    if (!st->sections[sa]) {
      rem = 0;
      for (sa = 0; sa < 8; sa++)
        rem |= st->sections[sa];
      if (rem) return 1;
      tvhtrace(mt->mt_name, "  tableid %02X extraid %016" PRIx64 " completed",
               st->tableid, st->extraid);
      st->complete = 1;
      mt->mt_incomplete--;
      return dvb_table_complete(mt);
    }
  }
  return 2;
}

/*
 * Begin table
 */
int
dvb_table_begin
  (mpegts_table_t *mt, const uint8_t *ptr, int len,
   int tableid, uint64_t extraid, int minlen,
   mpegts_table_state_t **ret, int *sect, int *last, int *ver)
{
  mpegts_table_state_t *st;
  uint32_t sa, sb;

  /* Not long enough */
  if (len < minlen) {
    tvhdebug(mt->mt_name, "invalid table length %d min %d", len, minlen);
    return -1;
  }

  /* Ignore next */
  if((ptr[2] & 1) == 0)
    return -1;

  tvhtrace(mt->mt_name, "pid %02X tableid %02X extraid %016" PRIx64 " len %d",
           mt->mt_pid, tableid, extraid, len);

  /* Section info */
  if (sect && ret) {
    *sect = ptr[3];
    *last = ptr[4];
    *ver  = (ptr[2] >> 1) & 0x1F;
    tvhtrace(mt->mt_name, "  section %d last %d ver %d", *sect, *last, *ver);
    *ret = st = mpegts_table_state_find(mt, tableid, extraid, *last);

    /* New version */
    if (st->complete &&
        st->version != *ver) {
      tvhtrace(mt->mt_name, "  new version, restart");
      mpegts_table_state_reset(mt, st, *last);
    }
    st->version = *ver;

    /* Complete? */
    if (st->complete) {
      tvhtrace(mt->mt_name, "  skip, already complete");
      if (st->complete == 1) {
        st->complete = 2;
        mt->mt_complete++;
        return dvb_table_complete(mt);
      }
      return -1;
    }

    /* Already seen? */
    sa = *sect / 32;
    sb = *sect % 32;
    if (!(st->sections[sa] & (0x1 << (31 - sb)))) {
      tvhtrace(mt->mt_name, "  skip, already seen");
      return -1;
    }
  }

  tvhlog_hexdump(mt->mt_name, ptr, len);

  return 1;
}

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
  mpegts_mux_t          *mm  = mt->mt_mux;
  mpegts_table_state_t  *st  = NULL;
  mpegts_service_t *s;

  /* Begin */
  if (tableid != 0) return -1;
  tsid = (ptr[0] << 8) | ptr[1];
  r    = dvb_table_begin(mt, ptr, len, tableid, tsid, 5,
                         &st, &sect, &last, &ver);
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
        mpegts_table_add(mm, DVB_PMT_BASE, DVB_PMT_MASK, dvb_pmt_callback,
                         NULL, "pmt", MT_CRC | MT_QUICKREQ | MT_SCANSUBS,
                         pid);

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
                     NULL, "nit", MT_QUICKREQ | MT_CRC, nit_pid);

  /* End */
  return dvb_table_end(mt, st, sect);
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
  mpegts_mux_t          *mm  = mt->mt_mux;
  mpegts_table_state_t  *st  = NULL;

  /* Start */
  r = dvb_table_begin(mt, ptr, len, tableid, 0, 5, &st, &sect, &last, &ver);
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
  return dvb_table_end(mt, st, sect);
}

/*
 * PMT processing
 */

int
dvb_pmt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver, had_components;
  uint16_t sid;
  mpegts_mux_t *mm = mt->mt_mux;
  mpegts_service_t *s;
  mpegts_table_state_t  *st  = NULL;

  /* Start */
  sid = ptr[0] << 8 | ptr[1];
  r   = dvb_table_begin(mt, ptr, len, tableid, sid, 9, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* Find service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    if (s->s_dvb_service_id == sid) break;
  if (!s) return -1;

  /* Process */
  tvhdebug("pmt", "sid %04X (%d)", sid, sid);
  pthread_mutex_lock(&s->s_stream_mutex);
  had_components = !!TAILQ_FIRST(&s->s_components);
  r = psi_parse_pmt(s, ptr, len);
  pthread_mutex_unlock(&s->s_stream_mutex);
  if (r)
    service_restart((service_t*)s, had_components);

  /* Finish */
  return dvb_table_end(mt, st, sect);
}

/*
 * NIT/BAT processing (because its near identical)
 */
int
dvb_nit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int save = 0;
  int r, sect, last, ver;
  uint8_t  dtag;
  int llen, dllen, dlen;
  const uint8_t *lptr, *dlptr, *dptr;
  uint16_t nbid = 0, onid, tsid;
  mpegts_mux_t     *mm = mt->mt_mux, *mux;
  mpegts_network_t *mn = mm->mm_network;
  char name[256], dauth[256];
  mpegts_table_state_t  *st  = NULL;
  const char *charset;

  /* Net/Bat ID */
  nbid = (ptr[0] << 8) | ptr[1];

  /* Begin */
  if (tableid != 0x40 && tableid != 0x41 && tableid != 0x4A) return -1;
  r = dvb_table_begin(mt, ptr, len, tableid, nbid, 7, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* NIT */
  if (tableid != 0x4A) {

    /* Specific NID */
    if (mn->mn_nid) {
      if (mn->mn_nid != nbid) {
        return dvb_table_end(mt, st, sect);
      }
  
    /* Only use "this" network */
    } else if (tableid != 0x40) {
      return dvb_table_end(mt, st, sect);
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
    }
  }

  /* BAT */
  if (tableid == 0x4A) {
    tvhdebug(mt->mt_name, "bouquet %04X (%d) [%s]", nbid, nbid, name);

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
    tvhdebug(mt->mt_name, "  onid %04X (%d) tsid %04X (%d)", onid, onid, tsid, tsid);

    /* Find existing mux */
    LIST_FOREACH(mux, &mn->mn_muxes, mm_network_link)
      if (mux->mm_onid == onid && mux->mm_tsid == tsid)
        break;
    charset = dvb_charset_find(mn, mux, NULL);

    DVB_DESC_FOREACH(lptr, llen, 4, dlptr, dllen, dtag, dlen, dptr) {
      tvhtrace(mt->mt_name, "    dtag %02X dlen %d", dtag, dlen);

      /* User-defined */
      if (mt->mt_mux_cb && mux) {
        int i = 0;
        while (mt->mt_mux_cb[i].cb) {
          if (mt->mt_mux_cb[i].tag == dtag)
            break;
          i++;
        }
        if (mt->mt_mux_cb[i].cb) {
          if (mt->mt_mux_cb[i].cb(mt, mux, dtag, dptr, dlen))
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
          if (dtag == DVB_DESC_SAT_DEL)
            mux = dvb_desc_sat_del(mm, onid, tsid, dptr, dlen);
          else if (dtag == DVB_DESC_CABLE_DEL)
            mux = dvb_desc_cable_del(mm, onid, tsid, dptr, dlen);
          else
            mux = dvb_desc_terr_del(mm, onid, tsid, dptr, dlen);
          if (mux) {
            mpegts_mux_set_onid(mux, onid);
            mpegts_mux_set_tsid(mux, tsid, 0);
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
        case DVB_DESC_LOCAL_CHAN:
          if (dvb_desc_local_channel(mt->mt_name, dptr, dlen, mux))
            return -1;
          break;
        case DVB_DESC_SERVICE_LIST:
          if (dvb_desc_service_list(mt->mt_name, dptr, dlen, mux))
            return -1;
          break;
      }
    }
  }

  /* End */
  return dvb_table_end(mt, st, sect);
}

/**
 * DVB SDT (Service Description Table)
 */
int
dvb_sdt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver, extraid;
  uint16_t onid, tsid;
  uint8_t dtag;
  int llen, dlen;
  const uint8_t *lptr, *dptr;
  mpegts_mux_t     *mm = mt->mt_mux;
  mpegts_network_t *mn = mm->mm_network;
  mpegts_table_state_t  *st  = NULL;

  /* Begin */
  tsid    = ptr[0] << 8 | ptr[1];
  onid    = ptr[5] << 8 | ptr[6];
  extraid = ((int)onid) << 16 | tsid;
  if (tableid != 0x42 && tableid != 0x46) return -1;
  r = dvb_table_begin(mt, ptr, len, tableid, extraid, 8, &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* ID */
  tvhdebug("sdt", "onid %04X (%d) tsid %04X (%d)", onid, onid, tsid, tsid);

  /* Find Transport Stream */
  if (tableid == 0x42) {
    mpegts_mux_set_onid(mm, onid);
    mpegts_mux_set_tsid(mm, tsid, 1);
  } else {
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
      if (mm->mm_onid == onid && mm->mm_tsid == tsid)
        break;
    goto done;
  }

  /* Service loop */
  len -= 8;
  ptr += 8;
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
      }
    }

    tvhtrace("sdt", "  type %d name [%s] provider [%s] def_auth [%s]",
             stype, sname, sprov, sauth);
    if (!s) continue;

    /* Update service type */
    if (stype && s->s_dvb_servicetype != stype) {
      int r;
      s->s_dvb_servicetype = stype;
      save = 1;
      tvhtrace("sdt", "    type changed");

      /* Set tvh service type */
      if ((r = dvb_servicetype_lookup(stype)) != -1)
        s->s_servicetype = r;
    }
    
    /* Check if this is master 
     * Some networks appear to provide diff service names on diff transponders
     */
    if (tableid == 0x42)
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
      idnode_updated(&s->s_id);
      s->s_config_save((service_t*)s);
      service_refresh_channel((service_t*)s);
    }
  }

  /* Done */
done:
  return dvb_table_end(mt, st, sect);
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
  mpegts_mux_t     *mm = mt->mt_mux;
  mpegts_network_t *mn = mm->mm_network;
  mpegts_service_t *s;
  mpegts_table_state_t  *st  = NULL;

  /* Validate */
  if (tableid != 0xc8 && tableid != 0xc9) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin(mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
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
      if (mm->mm_tsid == tsid)
        break;
    if (!mm) goto next;

    /* Find the service */
    if (!(s = mpegts_service_find(mm, sid, 0, 1, &save)))
      goto next;

    /* Update */
    if (strcmp(s->s_dvb_svcname ?: "", chname)) {
      tvh_str_set(&s->s_dvb_svcname, chname);
      save = 1;
    }
    if (s->s_dvb_channel_num != maj) {
      // TODO: ATSC channel numbering is plain weird!
      //       could shift the major (*100 or something) and append
      //       minor, but that'll probably confuse people, as will this!
      s->s_dvb_channel_num = maj;
      save = 1;
    }

    /* Save */
    if (save)
      s->s_config_save((service_t*)s);

    /* Move on */
next:
    ptr += dlen + 32;
    len -= dlen + 32;
  }

  return dvb_table_end(mt, st, sect);
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
           caid, descrambler_caid2name(caid), provid, pid);

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
  case 0x4a00://DRECrypt
    if (caid != 0x4aee) { // Bulcrypt
      provid = size < 4 ? 0 : buffer[4];
      break;
    }
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
  (mpegts_service_t *t, const uint8_t *ptr, int len)
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
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
      hts_stream_type = SCT_MPEG2AUDIO;
      break;

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

    default:
      break;
    }

    while(dllen > 1) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 
      if(dlen > len)
        break;

      switch(dtag) {
      case DVB_DESC_CA:
        update |= psi_desc_ca(t, ptr, dlen);
        break;

      case DVB_DESC_REGISTRATION:
        if(dlen == 4 && 
           ptr[0] == 'A' && ptr[1] == 'C' && ptr[2] == '-' &&  ptr[3] == '3')
          hts_stream_type = SCT_AC3;
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
    
    service_request_save((service_t*)t, 0);

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

/*
 * Install default table sets
 */

void
psi_tables_default ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_PAT_BASE, DVB_PAT_MASK, dvb_pat_callback,
                   NULL, "pat", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_PAT_PID);
  mpegts_table_add(mm, DVB_CAT_BASE, DVB_CAT_MASK, dvb_cat_callback,
                   NULL, "cat", MT_QUICKREQ | MT_CRC, DVB_CAT_PID);
}

void
psi_tables_dvb ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_NIT_BASE, DVB_NIT_MASK, dvb_nit_callback,
                   NULL, "nit", MT_QUICKREQ | MT_CRC, DVB_NIT_PID);
  mpegts_table_add(mm, DVB_SDT_BASE, DVB_SDT_MASK, dvb_sdt_callback,
                   NULL, "sdt", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_SDT_PID);
  mpegts_table_add(mm, DVB_BAT_BASE, DVB_BAT_MASK, dvb_bat_callback,
                   NULL, "bat", MT_CRC, DVB_BAT_PID);
}

void
psi_tables_atsc_c ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_VCT_C_BASE, DVB_VCT_MASK, atsc_vct_callback,
                   NULL, "vct", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_VCT_PID);
}

void
psi_tables_atsc_t ( mpegts_mux_t *mm )
{
  mpegts_table_add(mm, DVB_VCT_T_BASE, DVB_VCT_MASK, atsc_vct_callback,
                   NULL, "vct", MT_QUICKREQ | MT_CRC | MT_RECORD,
                   DVB_VCT_PID);
}
