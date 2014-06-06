/*
 *  Tvheadend
 *  Copyright (C) 2013 Andreas Öman
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

#include "descrambler.h"
#include "cwc.h"
#include "capmt.h"
#include "ffdecsa/FFdecsa.h"
#include "input.h"
#include "tvhcsa.h"

static struct strtab caidnametab[] = {
  { "Seca",             0x0100 }, 
  { "CCETT",            0x0200 }, 
  { "Deutsche Telecom", 0x0300 }, 
  { "Eurodec",          0x0400 }, 
  { "Viaccess",         0x0500 }, 
  { "Irdeto",           0x0600 }, 
  { "Irdeto",           0x0602 }, 
  { "Irdeto",           0x0603 },
  { "Irdeto",           0x0604 },
  { "Irdeto",           0x0622 },
  { "Irdeto",           0x0624 },
  { "Irdeto",           0x0648 },
  { "Irdeto",           0x0666 },
  { "Jerroldgi",        0x0700 }, 
  { "Matra",            0x0800 }, 
  { "NDS",              0x0900 },
  { "NDS",              0x0919 },
  { "NDS",              0x091F }, 
  { "NDS",              0x092B },
  { "NDS",              0x09AF }, 
  { "NDS",              0x09C4 },
  { "NDS",              0x0960 },
  { "NDS",              0x0963 }, 
  { "Nokia",            0x0A00 }, 
  { "Conax",            0x0B00 },
  { "Conax",            0x0B01 },
  { "Conax",            0x0B02 }, 
  { "Conax",            0x0BAA },
  { "NTL",              0x0C00 }, 
  { "CryptoWorks",      0x0D00 },
  { "CryptoWorks",      0x0D01 },
  { "CryptoWorks",      0x0D02 },
  { "CryptoWorks",      0x0D03 },
  { "CryptoWorks",      0x0D05 },
  { "CryptoWorks",      0x0D0F },
  { "CryptoWorks",      0x0D70 },
  { "CryptoWorks ICE",	0x0D95 }, 
  { "CryptoWorks ICE",	0x0D96 },
  { "CryptoWorks ICE",	0x0D97 },
  { "PowerVu",          0x0E00 }, 
  { "PowerVu",          0x0E11 }, 
  { "Sony",             0x0F00 }, 
  { "Tandberg",         0x1000 }, 
  { "Thompson",         0x1100 }, 
  { "TV-Com",           0x1200 }, 
  { "HPT",              0x1300 }, 
  { "HRT",              0x1400 }, 
  { "IBM",              0x1500 }, 
  { "Nera",             0x1600 }, 
  { "BetaCrypt",        0x1700 }, 
  { "BetaCrypt",        0x1702 }, 
  { "BetaCrypt",        0x1722 }, 
  { "BetaCrypt",        0x1762 }, 
  { "NagraVision",      0x1800 },
  { "NagraVision",      0x1803 },
  { "NagraVision",      0x1813 },
  { "NagraVision",      0x1810 },
  { "NagraVision",      0x1815 },
  { "NagraVision",      0x1830 },
  { "NagraVision",      0x1833 },
  { "NagraVision",      0x1834 },
  { "NagraVision",      0x183D },
  { "NagraVision",      0x1861 },
  { "Titan",            0x1900 }, 
  { "Telefonica",       0x2000 }, 
  { "Stentor",          0x2100 }, 
  { "Tadiran Scopus",   0x2200 }, 
  { "BARCO AS",         0x2300 }, 
  { "StarGuide",        0x2400 }, 
  { "Mentor",           0x2500 }, 
  { "EBU",              0x2600 }, 
  { "GI",               0x4700 }, 
  { "Telemann",         0x4800 },
  { "DRECrypt",         0x4ae0 },
  { "DRECrypt2",        0x4ae1 },
  { "Bulcrypt",         0x4aee },
  { "Bulcrypt",         0x5581 },
  { "Verimatrix",       0x5601 },
};

void
descrambler_init ( void )
{
#if ENABLE_CWC
  cwc_init();
#endif
#if ENABLE_CAPMT
  capmt_init();
#endif
#if (ENABLE_CWC || ENABLE_CAPMT) && !ENABLE_DVBCSA
  ffdecsa_init();
#endif
}

void
descrambler_done ( void )
{
#if ENABLE_CAPMT
  capmt_done();
#endif
#if ENABLE_CWC
  cwc_done();
#endif
}

void
descrambler_service_start ( service_t *t )
{
#if ENABLE_CWC
  cwc_service_start(t);
#endif
#if ENABLE_CAPMT
  capmt_service_start(t);
#endif
}

void
descrambler_service_stop ( service_t *t )
{
  th_descrambler_t *td;

  while ((td = LIST_FIRST(&t->s_descramblers)) != NULL)
    td->td_stop(td);
}

void
descrambler_caid_changed ( service_t *t )
{
  th_descrambler_t *td;

  LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
    if (td->td_caid_change)
      td->td_caid_change(td);
  }
}

int
descrambler_descramble ( service_t *t,
                         elementary_stream_t *st,
                         const uint8_t *tsb )
{
  th_descrambler_t *td;
  int count, failed;

  LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
    count++;
    if (td->td_keystate == DS_FORBIDDEN) {
      failed++;
      continue;
    }
    if (td->td_keystate != DS_RESOLVED)
      continue;
    tvhcsa_descramble(td->td_csa,
                      (struct mpegts_service *)td->td_service,
                      st, tsb);
    return 1;
  }
  return count == failed ? -1 : 0;
}

static int
descrambler_table_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  descrambler_table_t *dt = mt->mt_opaque;
  descrambler_section_t *ds;

  pthread_mutex_lock(&mt->mt_mux->mm_descrambler_lock);
  TAILQ_FOREACH(ds, &dt->sections, link)
    ds->callback(ds->opaque, mt->mt_pid, ptr, len);
  pthread_mutex_unlock(&mt->mt_mux->mm_descrambler_lock);
  return 0;
}

int
descrambler_open_pid( mpegts_mux_t *mux, void *opaque, int pid,
                      descrambler_section_callback_t callback )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;

  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid == pid) {
      TAILQ_FOREACH(ds, &dt->sections, link) {
        if (ds->opaque == opaque) {
          pthread_mutex_unlock(&mux->mm_descrambler_lock);
          return 0;
        }
      }
    }
  }
  if (!dt) {
    dt = calloc(1, sizeof(*dt));
    TAILQ_INIT(&dt->sections);
    dt->table = mpegts_table_add(mux, 0, 0, descrambler_table_callback,
                                 dt, "descrambler", MT_FULL, pid);
    TAILQ_INSERT_TAIL(&mux->mm_descrambler_tables, dt, link);
  }
  ds = calloc(1, sizeof(*ds));
  ds->callback    = callback;
  ds->opaque      = opaque;
  TAILQ_INSERT_TAIL(&dt->sections, ds, link);
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  return 1;
}

int
descrambler_close_pid( mpegts_mux_t *mux, void *opaque, int pid )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;

  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid == pid) {
      TAILQ_FOREACH(ds, &dt->sections, link) {
        if (ds->opaque == opaque) {
          TAILQ_REMOVE(&dt->sections, ds, link);
          free(ds);
          if (TAILQ_FIRST(&dt->sections) == NULL) {
            TAILQ_REMOVE(&mux->mm_descrambler_tables, dt, link);
            mpegts_table_destroy(dt->table);
            free(dt);
          }
          pthread_mutex_unlock(&mux->mm_descrambler_lock);
          return 1;
        }
      }
    }
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  return 0;
}

void
descrambler_flush_tables( mpegts_mux_t *mux )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;

  pthread_mutex_lock(&mux->mm_descrambler_lock);
  while ((dt = TAILQ_FIRST(&mux->mm_descrambler_tables)) != NULL) {
    while ((ds = TAILQ_FIRST(&dt->sections)) != NULL) {
      TAILQ_REMOVE(&dt->sections, ds, link);
      free(ds);
    }
    TAILQ_REMOVE(&mux->mm_descrambler_tables, dt, link);
    mpegts_table_destroy(dt->table);
    free(dt);
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
}

// TODO: might actually put const char* into caid_t
const char *
descrambler_caid2name(uint16_t caid)
{
  const char *s = val2str(caid, caidnametab);
  static char buf[20];

  if(s != NULL)
    return s;
  snprintf(buf, sizeof(buf), "0x%x", caid);
  return buf;
}

uint16_t
descrambler_name2caid(const char *s)
{
  int i = str2val(s, caidnametab);
  return (i < 0) ? strtol(s, NULL, 0) : i;
}

/**
 * Detects the cam card type
 * If you want to add another card, have a look at
 * http://www.dvbservices.com/identifiers/ca_system_id?page=3
 *
 * based on the equivalent in sasc-ng
 */
card_type_t
detect_card_type(const uint16_t caid)
{
  
  uint8_t c_sys = caid >> 8;
  
  switch(caid) {
    case 0x5581:
    case 0x4aee:
      return CARD_BULCRYPT;
  }
  
  switch(c_sys) {
    case 0x17:
    case 0x06:
      return CARD_IRDETO;
    case 0x05:
      return CARD_VIACCESS;
    case 0x0b:
      return CARD_CONAX;
    case 0x01:
      return CARD_SECA;
    case 0x4a:
      return CARD_DRE;
    case 0x18:
      return CARD_NAGRA;
    case 0x09:
      return CARD_NDS;
    case 0x0d:
      return CARD_CRYPTOWORKS;
    default:
      return CARD_UNKNOWN;
  }
}

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
