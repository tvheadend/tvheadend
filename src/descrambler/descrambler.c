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

struct caid_tab {
  const char *name;
  uint16_t caid;
  uint16_t mask;
};

static struct caid_tab caidnametab[] = {
  { "Seca",             0x0100, 0xff00 },
  { "CCETT",            0x0200, 0xff00 },
  { "Deutsche Telekom", 0x0300, 0xff00 },
  { "Eurodec",          0x0400, 0xff00 },
  { "Viaccess",         0x0500, 0xff00 },
  { "Irdeto",           0x0600, 0xff00 },
  { "Jerroldgi",        0x0700, 0xff00 },
  { "Matra",            0x0800, 0xff00 },
  { "NDS",              0x0900, 0xff00 },
  { "Nokia",            0x0A00, 0xff00 },
  { "Conax",            0x0B00, 0xff00 },
  { "NTL",              0x0C00, 0xff00 },
  { "CryptoWorks",      0x0D00, 0xff80 },
  { "CryptoWorks ICE",	0x0D80, 0xff80 },
  { "PowerVu",          0x0E00, 0xff00 },
  { "Sony",             0x0F00, 0xff00 },
  { "Tandberg",         0x1000, 0xff00 },
  { "Thompson",         0x1100, 0xff00 },
  { "TV-Com",           0x1200, 0xff00 },
  { "HPT",              0x1300, 0xff00 },
  { "HRT",              0x1400, 0xff00 },
  { "IBM",              0x1500, 0xff00 },
  { "Nera",             0x1600, 0xff00 },
  { "BetaCrypt",        0x1700, 0xff00 },
  { "NagraVision",      0x1800, 0xff00 },
  { "Titan",            0x1900, 0xff00 },
  { "Telefonica",       0x2000, 0xff00 },
  { "Stentor",          0x2100, 0xff00 },
  { "Tadiran Scopus",   0x2200, 0xff00 },
  { "BARCO AS",         0x2300, 0xff00 },
  { "StarGuide",        0x2400, 0xff00 },
  { "Mentor",           0x2500, 0xff00 },
  { "EBU",              0x2600, 0xff00 },
  { "GI",               0x4700, 0xff00 },
  { "Telemann",         0x4800, 0xff00 },
  { "StreamGuard",      0x4ad2, 0xffff },
  { "DRECrypt",         0x4ae0, 0xffff },
  { "DRECrypt2",        0x4ae1, 0xffff },
  { "Bulcrypt",         0x4aee, 0xffff },
  { "TongFang",         0x4b00, 0xff00 },
  { "Griffin",          0x5500, 0xffe0 },
  { "Bulcrypt",         0x5581, 0xffff },
  { "Verimatrix",       0x5601, 0xffff },
  { "DRECrypt",         0x7be0, 0xffff },
  { "DRECrypt2",        0x7be1, 0xffff },
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

/*
 * This routine is called from two places
 * a) start a new service
 * b) restart a running service with possible caid changes
 */
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
  if (t->s_descramble == NULL) {
    t->s_descramble = dr = calloc(1, sizeof(th_descrambler_runtime_t));
    sbuf_init(&dr->dr_buf);
    dr->dr_key_index = 0xff;
    dr->dr_last_descramble = dispatch_clock;
  }
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
      dr->dr_key_valid |= 0x40;
      break;
    }
  for (i = 0; i < 8; i++)
    if (odd[i]) {
      j++;
      tvhcsa_set_key_odd(td->td_csa, odd);
      dr->dr_key_valid |= 0x80;
      break;
    }

  if (j) {
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

static void
descrambler_flush_table_data( service_t *t )
{
  mpegts_service_t *ms = (mpegts_service_t *)t;
  mpegts_mux_t *mux = ms->s_dvb_mux;
  descrambler_table_t *dt;
  descrambler_section_t *ds;

  if (mux == NULL)
    return;
  tvhtrace("descrabler", "flush table data for service \"%s\"", ms->s_dvb_svcname);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table == NULL || dt->table->mt_service != ms)
      continue;
    TAILQ_FOREACH(ds, &dt->sections, link) {
      free(ds->last_data);
      ds->last_data = NULL;
      ds->last_data_len = 0;
    }
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
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
#define KEY_MASK(k) (((k) & 0x40) + 0x40) /* 0x40 (for even) or 0x80 (for odd) */
  th_descrambler_t *td;
  th_descrambler_runtime_t *dr = t->s_descramble;
  int count, failed, off, size, flush_data = 0;
  uint8_t *tsb2, ki;

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
        ki = tsb2[3];
        if ((ki & 0x80) != 0x00) {
          if ((dr->dr_key_valid & KEY_MASK(ki)) == 0) {
            /* key is not valid - just skip from buffer */
            continue;
          }
          if (dr->dr_key_index != (ki & 0x40) &&
              dr->dr_key_start + 2 < dispatch_clock) {
            tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                    (ki & 0x40) ? "odd" : "even",
                                    ((mpegts_service_t *)t)->s_dvb_svcname);
            if (dr->dr_ecm_key_time + 2 < dr->dr_key_start) {
              sbuf_cut(&dr->dr_buf, off);
              if (!td->td_ecm_reset(td))
                goto next;
            }
            key_update(dr, ki);
          }
        }
        tvhcsa_descramble(td->td_csa,
                          (mpegts_service_t *)td->td_service,
                          tsb2);
      }
      sbuf_free(&dr->dr_buf);
      dr->dr_last_descramble = dispatch_clock;
    }
    ki = tsb[3];
    if ((ki & 0x80) != 0x00) {
      if ((dr->dr_key_valid & KEY_MASK(ki)) == 0) {
        limitedlog(&dr->dr_loglimit_key, "descrambler",
                   ((mpegts_service_t *)t)->s_dvb_svcname,
                   (ki & 0x40) ? "odd stream key is not valid" :
                                 "even stream key is not valid");
        continue;
      }
      if (dr->dr_key_index != (ki & 0x40) &&
          dr->dr_key_start + 2 < dispatch_clock) {
        tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                (ki & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        if (dr->dr_ecm_key_time + 2 < dr->dr_key_start) {
          tvhtrace("descrambler", "ECM late (%ld seconds) for service \"%s\"",
                                  dispatch_clock - dr->dr_ecm_key_time,
                                  ((mpegts_service_t *)t)->s_dvb_svcname);
          if (!td->td_ecm_reset(td))
            goto next;
        }
        key_update(dr, ki);
      }
    }
    tvhcsa_descramble(td->td_csa,
                      (mpegts_service_t *)td->td_service,
                      tsb);
    dr->dr_last_descramble = dispatch_clock;
    return 1;
next:
    flush_data = 1;
    continue;
  }
  if (dr->dr_ecm_start) { /* ECM sent */
    ki = tsb[3];
    if ((ki & 0x80) != 0x00) {
      if (dr->dr_key_start == 0) {
        tvhtrace("descrambler", "initial stream key set to %s for service \"%s\"",
                                (ki & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        key_update(dr, tsb[3]);
      } else if (dr->dr_key_index != (ki & 0x40) &&
                 dr->dr_key_start + 2 < dispatch_clock) {
        tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                (ki & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        key_update(dr, ki);
      }
    }
    if (count != failed) {
      /*
       * Fill a temporary buffer until the keys are known to make
       * streaming faster.
       */
      if (dr->dr_buf.sb_ptr >= 3000 * 188) {
        sbuf_cut(&dr->dr_buf, 300 * 188);
        tvhtrace("descrambler", "cannot decode packets for service \"%s\"",
                 ((mpegts_service_t *)t)->s_dvb_svcname);
      }
      sbuf_append(&dr->dr_buf, tsb, 188);
    }
  }
  if (flush_data)
    descrambler_flush_table_data(t);
  if (dr->dr_last_descramble + 25 < dispatch_clock)
    return -1;
  if (count && count == failed)
    return -1;
  return count;
#undef KEY_MASK
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
  tvhtrace("descrambler", "mux %p open pid %04X (%i) (flags 0x%04x)", mux, pid, pid, flags);
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
          tvhtrace("descrambler", "mux %p close pid %04X (%i)", mux, pid, pid);
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
  tvhtrace("descrambler", "mux %p - flush tables", mux);
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
  const char *s = NULL;
  static char __thread buf[20];
  struct caid_tab *tab;
  int i;

  for (i = 0; i < ARRAY_SIZE(caidnametab); i++) {
    tab = &caidnametab[i];
    if ((caid & tab->mask) == tab->caid) {
      s = tab->name;
      break;
    }
  }
  if(s != NULL)
    return s;
  snprintf(buf, sizeof(buf), "0x%x", caid);
  return buf;
}

uint16_t
descrambler_name2caid(const char *s)
{
  int i, r = -1;
  struct caid_tab *tab;

  for (i = 0; i < ARRAY_SIZE(caidnametab); i++) {
    tab = &caidnametab[i];
    if (strcmp(tab->name, s) == 0) {
      r = tab->caid;
      break;
    }
  }

  return (r < 0) ? strtol(s, NULL, 0) : r;
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
