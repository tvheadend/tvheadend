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

#include "tvheadend.h"
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
  th_descrambler_runtime_t *dr;

#if ENABLE_CWC
  cwc_service_start(t);
#endif
#if ENABLE_CAPMT
  capmt_service_start(t);
#endif
  t->s_descramble = dr = calloc(1, sizeof(th_descrambler_runtime_t));
  sbuf_init(&dr->dr_buf);
  dr->dr_key_index = 0xff;
  dr->dr_last_descramble = dispatch_clock;
}

void
descrambler_service_stop ( service_t *t )
{
  th_descrambler_t *td;
  th_descrambler_runtime_t *dr = t->s_descramble;

  while ((td = LIST_FIRST(&t->s_descramblers)) != NULL)
    td->td_stop(td);
  if (dr) {
    sbuf_free(&dr->dr_buf);
    free(dr);
  }
  t->s_descramble = NULL;
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

void
descrambler_keys ( th_descrambler_t *td,
                   const uint8_t *even, const uint8_t *odd )
{
  service_t *t = td->td_service;
  th_descrambler_runtime_t *dr;
  th_descrambler_t *td2;
  int i, j = 0;

  if (t == NULL || (dr = t->s_descramble) == NULL) {
    td->td_keystate = DS_FORBIDDEN;
    return;
  }

  pthread_mutex_lock(&t->s_stream_mutex);

  LIST_FOREACH(td2, &t->s_descramblers, td_service_link)
    if (td2 != td && td2->td_keystate == DS_RESOLVED) {
      tvhlog(LOG_DEBUG, "descrambler",
                        "Already has a key from %s for service \"%s\", "
                        "ignoring key from \"%s\"",
                        td2->td_nicename,
                        ((mpegts_service_t *)td2->td_service)->s_dvb_svcname,
                        td->td_nicename);
      td->td_keystate = DS_IDLE;
      goto fin;
    }

  for (i = 0; i < 8; i++)
    if (even[i]) {
      j++;
      tvhcsa_set_key_even(td->td_csa, even);
      break;
    }
  for (i = 0; i < 8; i++)
    if (odd[i]) {
      j++;
      tvhcsa_set_key_odd(td->td_csa, odd);
      break;
    }

  if (j == 2) {
    if (td->td_keystate != DS_RESOLVED)
      tvhlog(LOG_DEBUG, "descrambler",
                        "Obtained keys from %s for service \"%s\"",
                        td->td_nicename,
                        ((mpegts_service_t *)t)->s_dvb_svcname);
    tvhtrace("descrambler", "Obtained keys "
             "%02X%02X%02X%02X%02X%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X"
             " from %s for service \"%s\"",
             even[0], even[1], even[2], even[3], even[4], even[5], even[6], even[7],
             odd[0], odd[1], odd[2], odd[3], odd[4], odd[5], odd[6], odd[7],
             td->td_nicename,
             ((mpegts_service_t *)t)->s_dvb_svcname);
    dr->dr_ecm_key_time = dispatch_clock;
    td->td_keystate = DS_RESOLVED;
  } else {
    tvhlog(LOG_DEBUG, "descrambler",
                      "Empty keys received from %s for service \"%s\"",
                      td->td_nicename,
                      ((mpegts_service_t *)t)->s_dvb_svcname);
  }

fin:
  pthread_mutex_unlock(&t->s_stream_mutex);
}

static inline void
key_update( th_descrambler_runtime_t *dr, uint8_t key )
{
  /* set the even (0) or odd (0x40) key index */
  dr->dr_key_index = key & 0x40;
  dr->dr_key_start = dispatch_clock;
}

int
descrambler_descramble ( service_t *t,
                         elementary_stream_t *st,
                         const uint8_t *tsb )
{
  th_descrambler_t *td;
  th_descrambler_runtime_t *dr = t->s_descramble;
  int count, failed, off, size;
  uint8_t *tsb2;

  lock_assert(&t->s_stream_mutex);

  if (dr == NULL)
    return -1;
  count = failed = 0;
  LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
    count++;
    if (td->td_keystate == DS_FORBIDDEN) {
      failed++;
      continue;
    }
    if (td->td_keystate != DS_RESOLVED)
      continue;
    if (dr->dr_buf.sb_ptr > 0) {
      for (off = 0, size = dr->dr_buf.sb_ptr; off < size; off += 188) {
        tsb2 = dr->dr_buf.sb_data + off;
        if ((tsb2[3] & 0x80) != 0x00 && dr->dr_key_index != (tsb2[3] & 0x40) &&
            dr->dr_key_start + 2 < dispatch_clock) {
          tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                  (tsb2[3] & 0x40) ? "odd" : "even",
                                  ((mpegts_service_t *)t)->s_dvb_svcname);
          if (dr->dr_ecm_key_time + 2 < dr->dr_key_start) {
            sbuf_cut(&dr->dr_buf, off);
            goto forbid;
          }
          key_update(dr, tsb2[3]);
        }
        tvhcsa_descramble(td->td_csa,
                          (mpegts_service_t *)td->td_service,
                          tsb2);
      }
      sbuf_free(&dr->dr_buf);
      dr->dr_last_descramble = dispatch_clock;
    }
    if ((tsb[3] & 0x80) != 0x00 && dr->dr_key_index != (tsb[3] & 0x40) &&
       dr->dr_key_start + 2 < dispatch_clock) {
      tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                              (tsb[3] & 0x40) ? "odd" : "even",
                              ((mpegts_service_t *)t)->s_dvb_svcname);
      if (dr->dr_ecm_key_time + 2 < dr->dr_key_start) {
forbid:
        tvhtrace("descrambler", "ECM late (%ld seconds) for service \"%s\"",
                                dispatch_clock - dr->dr_ecm_key_time,
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        td->td_keystate = DS_IDLE;
        failed++;
        continue;
      }
      key_update(dr, tsb[3]);
    }
    tvhcsa_descramble(td->td_csa,
                      (mpegts_service_t *)td->td_service,
                      tsb);
    dr->dr_last_descramble = dispatch_clock;
    return 1;
  }
  if (dr->dr_ecm_start) { /* ECM sent */
    if ((tsb[3] & 0x80) != 0x00) {
      if (dr->dr_key_start == 0) {
        tvhtrace("descrambler", "initial stream key set to %s for service \"%s\"",
                                (tsb[3] & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        key_update(dr, tsb[3]);
      } else if (dr->dr_key_index != (tsb[3] & 0x40) &&
                 dr->dr_key_start + 2 < dispatch_clock) {
        tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                (tsb[3] & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        key_update(dr, tsb[3]);
      }
    }
    if (count != failed) {
      /*
       * Fill a temporary buffer until the keys are known to make
       * streaming faster.
       */
      if (dr->dr_buf.sb_ptr >= 3000 * 188)
        sbuf_cut(&dr->dr_buf, 300 * 188);
      sbuf_append(&dr->dr_buf, tsb, 188);
    }
  }
  if (dr->dr_last_descramble + 25 < dispatch_clock)
    return -1;
  if (count && count == failed)
    return -1;
  return count;
}

static int
descrambler_table_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  descrambler_table_t *dt = mt->mt_opaque;
  descrambler_section_t *ds;
  th_descrambler_runtime_t *dr;

  pthread_mutex_lock(&mt->mt_mux->mm_descrambler_lock);
  TAILQ_FOREACH(ds, &dt->sections, link)
    if (ds->last_data == NULL || len != ds->last_data_len ||
        memcmp(ds->last_data, ptr, len)) {
      free(ds->last_data);
      ds->last_data = malloc(len);
      if (ds->last_data) {
        memcpy(ds->last_data, ptr, len);
        ds->last_data_len = len;
      } else {
        ds->last_data_len = 0;
      }
      ds->callback(ds->opaque, mt->mt_pid, ptr, len);
      if ((mt->mt_flags & MT_FAST) != 0) { /* ECM */
        mpegts_service_t *t = mt->mt_service;
        if (t) {
          /* The keys are requested from this moment */
          dr = t->s_descramble;
          dr->dr_ecm_start = dispatch_clock;
          tvhtrace("descrambler", "ECM message (len %d, pid %d) for service \"%s\"",
                   len, mt->mt_pid, t->s_dvb_svcname);
        } else
          tvhtrace("descrambler", "Unknown fast table message (len %d, pid %d)",
                   len, mt->mt_pid);
      }
    }
  pthread_mutex_unlock(&mt->mt_mux->mm_descrambler_lock);
  return 0;
}

static int
descrambler_open_pid_( mpegts_mux_t *mux, void *opaque, int pid,
                       descrambler_section_callback_t callback,
                       service_t *service )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;
  int flags;

  if (mux == NULL)
    return 0;
  flags  = pid >> 16;
  pid   &= 0x3fff;
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid == pid) {
      TAILQ_FOREACH(ds, &dt->sections, link) {
        if (ds->opaque == opaque)
          return 0;
      }
    }
  }
  if (!dt) {
    dt = calloc(1, sizeof(*dt));
    TAILQ_INIT(&dt->sections);
    dt->table = mpegts_table_add(mux, 0, 0, descrambler_table_callback,
                                 dt, "descrambler", MT_FULL | flags, pid);
    if (dt->table)
      dt->table->mt_service = (mpegts_service_t *)service;
    TAILQ_INSERT_TAIL(&mux->mm_descrambler_tables, dt, link);
  }
  ds = calloc(1, sizeof(*ds));
  ds->callback    = callback;
  ds->opaque      = opaque;
  TAILQ_INSERT_TAIL(&dt->sections, ds, link);
  tvhtrace("descrambler", "open pid %04X (%i) (flags 0x%04x)", pid, pid, flags);
  return 1;
}

int
descrambler_open_pid( mpegts_mux_t *mux, void *opaque, int pid,
                      descrambler_section_callback_t callback,
                      service_t *service )
{
  int res;

  pthread_mutex_lock(&mux->mm_descrambler_lock);
  res = descrambler_open_pid_(mux, opaque, pid, callback, service);
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  return res;
}

static int
descrambler_close_pid_( mpegts_mux_t *mux, void *opaque, int pid )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;

  if (mux == NULL)
    return 0;
  pid &= 0x3fff;
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid == pid) {
      TAILQ_FOREACH(ds, &dt->sections, link) {
        if (ds->opaque == opaque) {
          TAILQ_REMOVE(&dt->sections, ds, link);
          ds->callback(ds->opaque, -1, NULL, 0);
          free(ds->last_data);
          free(ds);
          if (TAILQ_FIRST(&dt->sections) == NULL) {
            TAILQ_REMOVE(&mux->mm_descrambler_tables, dt, link);
            mpegts_table_destroy(dt->table);
            free(dt);
          }
          tvhtrace("descrambler", "close pid %04X (%i)", pid, pid);
          return 1;
        }
      }
    }
  }
  return 0;
}

int
descrambler_close_pid( mpegts_mux_t *mux, void *opaque, int pid )
{
  int res;

  pthread_mutex_lock(&mux->mm_descrambler_lock);
  res = descrambler_close_pid_(mux, opaque, pid);
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  return res;
}

void
descrambler_flush_tables( mpegts_mux_t *mux )
{
  descrambler_table_t *dt;
  descrambler_section_t *ds;
  descrambler_emm_t *emm;

  if (mux == NULL)
    return;
  tvhtrace("descrambler", "flush tables for %p", mux);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  while ((dt = TAILQ_FIRST(&mux->mm_descrambler_tables)) != NULL) {
    while ((ds = TAILQ_FIRST(&dt->sections)) != NULL) {
      TAILQ_REMOVE(&dt->sections, ds, link);
      ds->callback(ds->opaque, -1, NULL, 0);
      free(ds->last_data);
      free(ds);
    }
    TAILQ_REMOVE(&mux->mm_descrambler_tables, dt, link);
    mpegts_table_destroy(dt->table);
    free(dt);
  }
  while ((emm = TAILQ_FIRST(&mux->mm_descrambler_emms)) != NULL) {
    TAILQ_REMOVE(&mux->mm_descrambler_emms, emm, link);
    free(emm);
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
}

void
descrambler_cat_data( mpegts_mux_t *mux, const uint8_t *data, int len )
{
  descrambler_emm_t *emm;
  uint8_t dtag, dlen;
  uint16_t caid = 0, pid = 0;
  descrambler_section_callback_t callback = NULL;
  void *opaque = NULL;
  TAILQ_HEAD(,descrambler_emm) removing;

  tvhtrace("descrambler", "CAT data (len %d)", len);
  tvhlog_hexdump("descrambler", data, len);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link)
    emm->to_be_removed = 1;
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  while (len > 2) {
    if (len > 2) {
      dtag = *data++;
      dlen = *data++;
      len -= 2;
      if (dtag != DVB_DESC_CA || len < 4 || dlen < 4)
        goto next;
      caid =  (data[0] << 8) | data[1];
      pid  = ((data[2] << 8) | data[3]) & 0x1fff;
      if (pid == 0)
        goto next;
#if ENABLE_CWC
      cwc_caid_update(mux, caid, pid, 1);
#endif
      pthread_mutex_lock(&mux->mm_descrambler_lock);
      TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link)
        if (emm->caid == caid) {
          emm->to_be_removed = 0;
          if (emm->pid == EMM_PID_UNKNOWN) {
            tvhtrace("descrambler", "attach emm caid %04X (%i) pid %04X (%i)", caid, caid, pid, pid);
            emm->pid = pid;
            callback = emm->callback;
            opaque   = emm->opaque;
            break;
          }
        }
      if (emm)
        descrambler_open_pid_(mux, opaque, pid, callback, NULL);
      pthread_mutex_unlock(&mux->mm_descrambler_lock);
next:
      data += dlen;
      len  -= dlen;
    }
  }
  TAILQ_INIT(&removing);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link)
    if (emm->to_be_removed) {
      if (emm->pid != EMM_PID_UNKNOWN) {
        caid = emm->caid;
        pid  = emm->pid;
        tvhtrace("descrambler", "close emm caid %04X (%i) pid %04X (%i)", caid, caid, pid, pid);
        descrambler_close_pid(mux, emm->opaque, pid);
      }
      TAILQ_REMOVE(&mux->mm_descrambler_emms, emm, link);
      TAILQ_INSERT_TAIL(&removing, emm, link);
    }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  while ((emm = TAILQ_FIRST(&removing)) != NULL) {
    if (emm->pid != EMM_PID_UNKNOWN) {
#if ENABLE_CWC
      cwc_caid_update(mux, emm->caid, emm->pid, 0);
#endif
    }
    TAILQ_REMOVE(&removing, emm, link);
    free(emm);
  }
}

int
descrambler_open_emm( mpegts_mux_t *mux, void *opaque, int caid,
                      descrambler_section_callback_t callback )
{
  descrambler_emm_t *emm;
  caid_t *c;
  int pid = EMM_PID_UNKNOWN;

  if (mux == NULL)
    return 0;
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link) {
    if (emm->caid == caid && emm->opaque == opaque) {
      pthread_mutex_unlock(&mux->mm_descrambler_lock);
      return 0;
    }
  }
  emm = calloc(1, sizeof(*emm));
  emm->caid     = caid;
  emm->pid      = EMM_PID_UNKNOWN;
  emm->opaque   = opaque;
  emm->callback = callback;
  LIST_FOREACH(c, &mux->mm_descrambler_caids, link) {
    if (c->caid == caid) {
      emm->pid = pid = c->pid;
      break;
    }
  }
  TAILQ_INSERT_TAIL(&mux->mm_descrambler_emms, emm, link);
  if (pid != EMM_PID_UNKNOWN) {
    tvhtrace("descrambler",
             "attach emm caid %04X (%i) pid %04X (%i) - direct",
             caid, caid, pid, pid);
    descrambler_open_pid_(mux, opaque, pid, callback, NULL);
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  return 1;
}

int
descrambler_close_emm( mpegts_mux_t *mux, void *opaque, int caid )
{
  descrambler_emm_t *emm;
  int pid;

  if (mux == NULL)
    return 0;
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link)
    if (emm->caid == caid && emm->opaque == opaque)
      break;
  if (!emm) {
    pthread_mutex_unlock(&mux->mm_descrambler_lock);
    return 0;
  }
  TAILQ_REMOVE(&mux->mm_descrambler_emms, emm, link);
  pid  = emm->pid;
  if (pid != EMM_PID_UNKNOWN) {
    caid = emm->caid;
    tvhtrace("descrambler", "close emm caid %04X (%i) pid %04X (%i) - direct", caid, caid, pid, pid);
    descrambler_close_pid_(mux, opaque, pid);
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  free(emm);
  return 1;
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
