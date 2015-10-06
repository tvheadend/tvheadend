/*
 *  Tvheadend
 *  Copyright (C) 2013 Andreas Öman
 *  Copyright (C) 2014,2015 Jaroslav Kysela
 *
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
#include "config.h"
#include "settings.h"
#include "descrambler.h"
#include "caid.h"
#include "caclient.h"
#include "ffdecsa/FFdecsa.h"
#include "input.h"
#include "input/mpegts/tsdemux.h"
#include "dvbcam.h"
#include "streaming.h"

#define MAX_QUICK_ECM_ENTRIES 100

uint16_t *quick_ecm_table = NULL;

void
descrambler_init ( void )
{
  htsmsg_t *c, *e, *q;
  htsmsg_field_t *f;
  const char *s;
  uint32_t caid;
  int idx;

#if (ENABLE_CWC || ENABLE_CAPMT) && !ENABLE_DVBCSA
  ffdecsa_init();
#endif
  caclient_init();
#if ENABLE_LINUXDVB_CA
  dvbcam_init();
#endif

  if ((c = hts_settings_load("descrambler")) != NULL) {
    idx = 0;
    if ((q = htsmsg_get_list(c, "quick_ecm")) != NULL) {
      HTSMSG_FOREACH(f, q) {
        if (!(e = htsmsg_field_get_map(f))) continue;
        if (idx + 1 >= MAX_QUICK_ECM_ENTRIES) continue;
        if ((s = htsmsg_get_str(e, "caid")) == NULL) continue;
        caid = strtol(s, NULL, 16);
        tvhinfo("descrambler", "adding CAID %04X as quick ECM (%s)", caid, htsmsg_get_str(e, "name") ?: "unknown");
        if (!quick_ecm_table)
          quick_ecm_table = malloc(sizeof(uint16_t) * MAX_QUICK_ECM_ENTRIES);
        quick_ecm_table[idx++] = caid;
      }
      if (quick_ecm_table)
        quick_ecm_table[idx] = 0;
    }
    htsmsg_destroy(c);
  }
}

void
descrambler_done ( void )
{
  caclient_done();
  free(quick_ecm_table);
  quick_ecm_table = NULL;
}

/*
 * Decide, if we should work in "quick ECM" mode
 */
static int
descrambler_quick_ecm ( mpegts_service_t *t, int pid )
{
  elementary_stream_t *st;
  caid_t *ca;
  uint16_t *p;

  if (!quick_ecm_table)
    return 0;
  TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
    if (st->es_pid != pid) continue;
    LIST_FOREACH(ca, &st->es_caids, link)
      for (p = quick_ecm_table; *p; p++)
        if (ca->caid == *p)
          return 1;
  }
  return 0;
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
  elementary_stream_t *st;

  if (!((mpegts_service_t *)t)->s_dvb_forcecaid) {

    TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link)
      if (LIST_FIRST(&st->es_caids) == NULL)
        break;

    /* Do not run descrambler on FTA channels */
    if (!st)
      return;

  }

  ((mpegts_service_t *)t)->s_dvb_mux->mm_descrambler_flush = 0;
  if (t->s_descramble == NULL) {
    t->s_descramble = dr = calloc(1, sizeof(th_descrambler_runtime_t));
    sbuf_init(&dr->dr_buf);
    dr->dr_key_index = 0xff;
    tvhcsa_init(&dr->dr_csa);
  }
  caclient_start(t);

#if ENABLE_LINUXDVB_CA
  dvbcam_service_start(t);
#endif
}

void
descrambler_service_stop ( service_t *t )
{
  th_descrambler_t *td;
  th_descrambler_runtime_t *dr = t->s_descramble;

#if ENABLE_LINUXDVB_CA
  dvbcam_service_stop(t);
#endif

  while ((td = LIST_FIRST(&t->s_descramblers)) != NULL)
    td->td_stop(td);
  t->s_descramble = NULL;
  if (dr) {
    tvhcsa_destroy(&dr->dr_csa);
    sbuf_free(&dr->dr_buf);
    free(dr);
  }
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
descrambler_notify( th_descrambler_t *td,
                    uint16_t caid, uint32_t provid,
                    const char *cardsystem, uint16_t pid, uint32_t ecmtime,
                    uint16_t hops, const char *reader, const char *from,
                    const char *protocol )
{
  mpegts_service_t *t = (mpegts_service_t *)td->td_service;
  streaming_message_t *sm;
  descramble_info_t *di;

  tvhlog(LOG_DEBUG, "descrambler", "info - service='%s' caid=%04X(%s) "
                                   "provid=%06X ecmtime=%d hops=%d "
                                   "reader='%s' from='%s' protocol='%s'",
         t->s_dvb_svcname, caid, cardsystem, provid,
         ecmtime, hops, reader, from, protocol);

  sm = streaming_msg_create(SMT_DESCRAMBLE_INFO);
  sm->sm_data = di = calloc(1, sizeof(*di));

  di->pid     = pid;
  di->caid    = caid;
  di->provid  = provid;
  di->ecmtime = ecmtime;
  di->hops    = hops;
  strncpy(di->cardsystem, cardsystem, sizeof(di->cardsystem)-1);
  strncpy(di->reader, reader, sizeof(di->reader)-1);
  strncpy(di->from, from, sizeof(di->protocol)-1);
  strncpy(di->protocol, protocol, sizeof(di->protocol)-1);

  pthread_mutex_lock(&t->s_stream_mutex);
  streaming_pad_deliver(&t->s_streaming_pad, sm);
  pthread_mutex_unlock(&t->s_stream_mutex);
}

int
descrambler_resolved( service_t *t, th_descrambler_t *ignore )
{
  th_descrambler_t *td;

  LIST_FOREACH(td, &t->s_descramblers, td_service_link)
    if (td != ignore && td->td_keystate == DS_RESOLVED)
      return 1;
  return 0;
}

void
descrambler_keys ( th_descrambler_t *td, int type,
                   const uint8_t *even, const uint8_t *odd )
{
  static uint8_t empty[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  service_t *t = td->td_service;
  th_descrambler_runtime_t *dr;
  th_descrambler_t *td2;
  int j = 0;

  if (t == NULL || (dr = t->s_descramble) == NULL) {
    td->td_keystate = DS_FORBIDDEN;
    return;
  }

  if (tvhcsa_set_type(&dr->dr_csa, type) < 0)
    return;

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
      if (td->td_ecm_idle)
        td->td_ecm_idle(td);
      goto fin;
    }

  if (memcmp(empty, even, dr->dr_csa.csa_keylen)) {
    j++;
    memcpy(dr->dr_key_even, even, dr->dr_csa.csa_keylen);
    dr->dr_key_changed |= 1;
    dr->dr_key_valid |= 0x40;
    dr->dr_key_timestamp[0] = dispatch_clock;
  }
  if (memcmp(empty, odd, dr->dr_csa.csa_keylen)) {
    j++;
    memcpy(dr->dr_key_odd, odd, dr->dr_csa.csa_keylen);
    dr->dr_key_changed |= 2;
    dr->dr_key_valid |= 0x80;
    dr->dr_key_timestamp[1] = dispatch_clock;
  }

  if (j) {
    if (td->td_keystate != DS_RESOLVED)
      tvhlog(LOG_DEBUG, "descrambler",
                        "Obtained keys from %s for service \"%s\"",
                        td->td_nicename,
                        ((mpegts_service_t *)t)->s_dvb_svcname);
    if (dr->dr_csa.csa_keylen == 8) {
      tvhtrace("descrambler", "Obtained keys "
               "%02X%02X%02X%02X%02X%02X%02X%02X:%02X%02X%02X%02X%02X%02X%02X%02X"
               " from %s for service \"%s\"",
               even[0], even[1], even[2], even[3], even[4], even[5], even[6], even[7],
               odd[0], odd[1], odd[2], odd[3], odd[4], odd[5], odd[6], odd[7],
               td->td_nicename,
               ((mpegts_service_t *)t)->s_dvb_svcname);
    } else if (dr->dr_csa.csa_keylen == 16) {
      tvhtrace("descrambler", "Obtained keys "
               "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X:"
               "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
               " from %s for service \"%s\"",
               even[0], even[1], even[2], even[3], even[4], even[5], even[6], even[7],
               even[8], even[9], even[10], even[11], even[12], even[13], even[14], even[15],
               odd[0], odd[1], odd[2], odd[3], odd[4], odd[5], odd[6], odd[7],
               odd[8], odd[9], odd[10], odd[11], odd[12], odd[13], odd[14], odd[15],
               td->td_nicename,
               ((mpegts_service_t *)t)->s_dvb_svcname);
    } else {
      tvhtrace("descrambler", "Unknown keys from %s for for service \"%s\"",
               td->td_nicename, ((mpegts_service_t *)t)->s_dvb_svcname);
    }
    dr->dr_ecm_last_key_time = dispatch_clock;
    td->td_keystate = DS_RESOLVED;
  } else {
    tvhlog(LOG_DEBUG, "descrambler",
                      "Empty keys received from %s for service \"%s\"",
                      td->td_nicename,
                      ((mpegts_service_t *)t)->s_dvb_svcname);
  }

fin:
  pthread_mutex_unlock(&t->s_stream_mutex);
#if ENABLE_TSDEBUG
  if (j) {
    tsdebug_packet_t *tp = malloc(sizeof(*tp));
    uint16_t keylen = dr->dr_csa.csa_keylen;
    uint16_t sid = ((mpegts_service_t *)td->td_service)->s_dvb_service_id;
    uint32_t pos = 0, crc;
    mpegts_mux_t *mm = ((mpegts_service_t *)td->td_service)->s_dvb_mux;
    if (!mm->mm_active) {
      free(tp);
      return;
	}
    pthread_mutex_lock(&mm->mm_active->mmi_input->mi_output_lock);
    tp->pos = mm->mm_tsdebug_pos;
    memset(tp->pkt, 0xff, sizeof(tp->pkt));
    tp->pkt[pos++] = 0x47; /* sync byte */
    tp->pkt[pos++] = 0x1f; /* PID MSB */
    tp->pkt[pos++] = 0xff; /* PID LSB */
    tp->pkt[pos++] = 0x00; /* CC */
    memcpy(tp->pkt + pos, "TVHeadendDescramblerKeys", 24);
    pos += 24;
    tp->pkt[pos++] = type & 0xff;
    tp->pkt[pos++] = keylen & 0xff;
    tp->pkt[pos++] = (sid >> 8) & 0xff;
    tp->pkt[pos++] = sid & 0xff;
    memcpy(tp->pkt + pos, even, keylen);
    memcpy(tp->pkt + pos + keylen, odd, keylen);
    pos += 2 * keylen;
    crc = tvh_crc32(tp->pkt, pos, 0x859aa5ba);
    tp->pkt[pos++] = (crc >> 24) & 0xff;
    tp->pkt[pos++] = (crc >> 16) & 0xff;
    tp->pkt[pos++] = (crc >> 8) & 0xff;
    tp->pkt[pos++] = crc & 0xff;
    TAILQ_INSERT_HEAD(&mm->mm_tsdebug_packets, tp, link);
    pthread_mutex_unlock(&mm->mm_active->mmi_input->mi_output_lock);
  }
#endif
}

static void
descrambler_flush_table_data( service_t *t )
{
  mpegts_service_t *ms = (mpegts_service_t *)t;
  mpegts_mux_t *mux = ms->s_dvb_mux;
  descrambler_table_t *dt;
  descrambler_section_t *ds;
  descrambler_ecmsec_t *des;

  if (mux == NULL)
    return;
  tvhtrace("descrambler", "flush table data for service \"%s\"", ms->s_dvb_svcname);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table == NULL || dt->table->mt_service != ms)
      continue;
    TAILQ_FOREACH(ds, &dt->sections, link)
      while ((des = LIST_FIRST(&ds->ecmsecs)) != NULL) {
        LIST_REMOVE(des, link);
        free(des->last_data);
        free(des);
      }
  }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
}

static inline void
key_update( th_descrambler_runtime_t *dr, uint8_t key )
{
  /* set the even (0) or odd (0x40) key index */
  dr->dr_key_index = key & 0x40;
  if (dr->dr_key_start)
    dr->dr_key_start = dispatch_clock;
  else
    /* We don't know the exact start key switch time */
    dr->dr_key_start = dispatch_clock - 60;
}

static inline int
key_valid ( th_descrambler_runtime_t *dr, uint8_t ki )
{
  /* 0x40 (for even) or 0x80 (for odd) */
  uint8_t mask = ((ki & 0x40) + 0x40);
  return dr->dr_key_valid & mask;
}

static inline int
key_late( th_descrambler_runtime_t *dr, uint8_t ki )
{
  uint8_t kidx = (ki & 0x40) >> 6;
  /* required key is older than previous? */
  if (dr->dr_key_timestamp[kidx] < dr->dr_key_timestamp[kidx^1]) {
    /* but don't take in account the keys modified just now */
    if (dr->dr_key_timestamp[kidx^1] + 2 < dispatch_clock)
      return 1;
  }
  /* ECM was sent, but no new key was received */
  return dr->dr_ecm_last_key_time + 2 < dr->dr_key_start &&
         (!dr->dr_quick_ecm || dr->dr_ecm_start[kidx] + 4 < dr->dr_key_start);
}

static inline int
key_started( th_descrambler_runtime_t *dr, uint8_t ki )
{
  uint8_t kidx = (ki & 0x40) >> 6;
  return (int64_t)dispatch_clock - (int64_t)dr->dr_ecm_start[kidx] < 5;
}

static int
ecm_reset( service_t *t, th_descrambler_runtime_t *dr )
{
  th_descrambler_t *td;
  int ret = 0;

  /* reset the reader ECM state */
  LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
    if (!td->td_ecm_reset(td)) {
      dr->dr_key_valid = 0;
      ret = 1;
    }
  }
  return ret;
}

int
descrambler_descramble ( service_t *t,
                         elementary_stream_t *st,
                         const uint8_t *tsb,
                         int len )
{
  th_descrambler_t *td;
  th_descrambler_runtime_t *dr = t->s_descramble;
  int count, failed, resolved, off, len2, len3, flush_data = 0;
  uint32_t dbuflen;
  const uint8_t *tsb2;
  uint8_t ki;

  lock_assert(&t->s_stream_mutex);

  if (dr == NULL) {
    if ((tsb[3] & 0x80) == 0) {
      ts_recv_packet2((mpegts_service_t *)t, tsb, len);
      return 1;
    }
    return -1;
  }

  if (dr->dr_csa.csa_type == DESCRAMBLER_NONE && dr->dr_buf.sb_ptr == 0)
    if ((tsb[3] & 0x80) == 0) {
      ts_recv_packet2((mpegts_service_t *)t, tsb, len);
      return 1;
    }

  count = failed = resolved = 0;
  LIST_FOREACH(td, &t->s_descramblers, td_service_link) {
    count++;
    switch (td->td_keystate) {
    case DS_FORBIDDEN: failed++;   break;
    case DS_RESOLVED : resolved++; break;
    default: break;
    }
  }

  if (resolved) {

    /* update the keys */
    if (dr->dr_key_changed) {
      dr->dr_csa.csa_flush(&dr->dr_csa, (mpegts_service_t *)t);
      if (dr->dr_key_changed & 1)
        tvhcsa_set_key_even(&dr->dr_csa, dr->dr_key_even);
      if (dr->dr_key_changed & 2)
        tvhcsa_set_key_odd(&dr->dr_csa, dr->dr_key_odd);
      dr->dr_key_changed = 0;
    }

    /* process the queued TS packets */
    if (dr->dr_buf.sb_ptr > 0) {
      for (tsb2 = dr->dr_buf.sb_data, len2 = dr->dr_buf.sb_ptr;
           len2 > 0; tsb2 += len3, len2 -= len3) {
        ki = tsb2[3];
        if ((ki & 0x80) != 0x00) {
          if (key_valid(dr, ki) == 0) {
            sbuf_cut(&dr->dr_buf, tsb2 - dr->dr_buf.sb_data);
            goto next;
          }
          if (dr->dr_key_index != (ki & 0x40) &&
              dr->dr_key_start + 2 < dispatch_clock) {
            tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                    (ki & 0x40) ? "odd" : "even",
                                    ((mpegts_service_t *)t)->s_dvb_svcname);
            if (key_late(dr, ki)) {
              if (ecm_reset(t, dr)) {
                sbuf_cut(&dr->dr_buf, tsb2 - dr->dr_buf.sb_data);
                flush_data = 1;
                goto next;
              }
            }
            key_update(dr, ki);
          }
        }
        len3 = mpegts_word_count(tsb2, len2, 0xFF0000C0);
        dr->dr_csa.csa_descramble(&dr->dr_csa, (mpegts_service_t *)t, tsb2, len3);
      }
      if (len2 == 0)
        service_reset_streaming_status_flags(t, TSS_NO_ACCESS);
      sbuf_free(&dr->dr_buf);
    }

    /* check for key change */
    ki = tsb[3];
    if ((ki & 0x80) != 0x00) {
      if (key_valid(dr, ki) == 0) {
        if (!key_started(dr, ki) && tvhlog_limit(&dr->dr_loglimit_key, 10))
          tvhwarn("descrambler", "%s %s",
                   ((mpegts_service_t *)t)->s_dvb_svcname,
                   (ki & 0x40) ? "odd stream key is not valid" :
                                 "even stream key is not valid");
        goto next;
      }
      if (dr->dr_key_index != (ki & 0x40) &&
          dr->dr_key_start + 2 < dispatch_clock) {
        tvhtrace("descrambler", "stream key changed to %s for service \"%s\"",
                                (ki & 0x40) ? "odd" : "even",
                                ((mpegts_service_t *)t)->s_dvb_svcname);
        if (key_late(dr, ki)) {
          tvherror("descrambler", "ECM - key late (%ld seconds) for service \"%s\"",
                                  dispatch_clock - dr->dr_ecm_last_key_time,
                                  ((mpegts_service_t *)t)->s_dvb_svcname);
          if (ecm_reset(t, dr)) {
            flush_data = 1;
            goto next;
          }
        }
        key_update(dr, ki);
      }
    }
    dr->dr_csa.csa_descramble(&dr->dr_csa, (mpegts_service_t *)t, tsb, len);
    service_reset_streaming_status_flags(t, TSS_NO_ACCESS);
    return 1;
  }
next:
  if (dr->dr_ecm_start[0] || dr->dr_ecm_start[1]) { /* ECM sent */
    ki = tsb[3];
    if ((ki & 0x80) != 0x00) {
      if (dr->dr_key_start == 0) {
        /* do not use the first TS packet to decide - it may be wrong */
        if (dr->dr_buf.sb_ptr > 20 * 188) {
          for (off = 0; off < 20 * 188; off += 188)
            if ((dr->dr_buf.sb_data[off + 3] & 0xc0) != (ki & 0xc0))
              break;
          if (off >= 20 * 188) {
            tvhtrace("descrambler", "initial stream key set to %s for service \"%s\"",
                                    (ki & 0x40) ? "odd" : "even",
                                    ((mpegts_service_t *)t)->s_dvb_svcname);
            key_update(dr, ki);
          } else {
            sbuf_cut(&dr->dr_buf, 188);
          }
        }
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
      dbuflen = MAX(300, config.descrambler_buffer);
      if (dr->dr_buf.sb_ptr >= dbuflen * 188) {
        sbuf_cut(&dr->dr_buf, MAX((dbuflen / 10) * 188, len));
        if (dr->dr_last_err + 10 < dispatch_clock) {
          dr->dr_last_err = dispatch_clock;
          tvherror("descrambler", "cannot decode packets for service \"%s\"",
                   ((mpegts_service_t *)t)->s_dvb_svcname);
        } else {
          tvhtrace("descrambler", "cannot decode packets for service \"%s\"",
                   ((mpegts_service_t *)t)->s_dvb_svcname);
        }
      }
      sbuf_append(&dr->dr_buf, tsb, len);
      service_set_streaming_status_flags(t, TSS_NO_ACCESS);
    }
  } else {
    service_set_streaming_status_flags(t, TSS_NO_ACCESS);
  }
  if (flush_data)
    descrambler_flush_table_data(t);
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
  descrambler_ecmsec_t *des;
  th_descrambler_runtime_t *dr;
  int emm = (mt->mt_flags & MT_FAST) == 0;

  if (len < 6)
    return 0;
  pthread_mutex_lock(&mt->mt_mux->mm_descrambler_lock);
  TAILQ_FOREACH(ds, &dt->sections, link) {
    if (!emm) {
      LIST_FOREACH(des, &ds->ecmsecs, link)
        if (des->number == ptr[4])
          break;
    } else {
      des = LIST_FIRST(&ds->ecmsecs);
    }
    if (des == NULL) {
      des = calloc(1, sizeof(*des));
      des->number = emm ? 0 : ptr[4];
      LIST_INSERT_HEAD(&ds->ecmsecs, des, link);
    }
    if (des->last_data == NULL || len != des->last_data_len ||
        memcmp(des->last_data, ptr, len)) {
      free(des->last_data);
      des->last_data = malloc(len);
      if (des->last_data) {
        memcpy(des->last_data, ptr, len);
        des->last_data_len = len;
      } else {
        des->last_data_len = 0;
      }
      ds->callback(ds->opaque, mt->mt_pid, ptr, len, emm);
      if (!emm) { /* ECM */
        mpegts_service_t *t = mt->mt_service;
        if (t) {
          /* The keys are requested from this moment */
          dr = t->s_descramble;
          if (dr) {
            if (!dr->dr_quick_ecm && !ds->quick_ecm_called) {
              ds->quick_ecm_called = 1;
              dr->dr_quick_ecm = descrambler_quick_ecm(mt->mt_service, mt->mt_pid);
              if (dr->dr_quick_ecm)
                tvhdebug("descrambler", "quick ECM enabled for service '%s'",
                         t->s_dvb_svcname);
            }
            if ((ptr[0] & 0xfe) == 0x80) { /* 0x80 = even, 0x81 = odd */
              dr->dr_ecm_start[ptr[0] & 1] = dispatch_clock;
              if (dr->dr_quick_ecm)
                dr->dr_key_valid &= ~(1 << ((ptr[0] & 1) + 6)); /* 0x40 = even, 0x80 = odd */
            }
            tvhtrace("descrambler", "ECM message %02x (section %d, len %d, pid %d) for service \"%s\"",
                     ptr[0], des->number, len, mt->mt_pid, t->s_dvb_svcname);
          }
        } else
          tvhtrace("descrambler", "Unknown fast table message %02x (section %d, len %d, pid %d)",
                   ptr[0], des->number, len, mt->mt_pid);
      } else {
        tvhtrace("descrambler", "EMM message %02x:%02x:%02x:%02x (len %d, pid %d)",
                 ptr[0], ptr[1], ptr[2], ptr[3], len, mt->mt_pid);
      }
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
  if (mux->mm_descrambler_flush)
    return 0;
  flags  = (pid >> 16) & MT_FAST;
  pid   &= 0x1fff;
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid != pid || (dt->table->mt_flags & MT_FAST) != flags)
      continue;
    TAILQ_FOREACH(ds, &dt->sections, link) {
      if (ds->opaque == opaque)
        return 0;
    }
    break;
  }
  if (!dt) {
    dt = calloc(1, sizeof(*dt));
    TAILQ_INIT(&dt->sections);
    dt->table = mpegts_table_add(mux, 0, 0, descrambler_table_callback,
                                 dt, (flags & MT_FAST) ? "ecm" : "emm",
                                 MT_FULL | MT_DEFER | flags, pid,
                                 MPS_WEIGHT_CA);
    if (dt->table)
      dt->table->mt_service = (mpegts_service_t *)service;
    TAILQ_INSERT_TAIL(&mux->mm_descrambler_tables, dt, link);
  }
  ds = calloc(1, sizeof(*ds));
  ds->callback    = callback;
  ds->opaque      = opaque;
  LIST_INIT(&ds->ecmsecs);
  TAILQ_INSERT_TAIL(&dt->sections, ds, link);
  tvhtrace("descrambler", "mux %p open pid %04X (%i) (flags 0x%04x) for %p", mux, pid, pid, flags, opaque);
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
  descrambler_ecmsec_t *des;
  int flags;

  if (mux == NULL)
    return 0;
  flags =  (pid >> 16) & MT_FAST;
  pid   &= 0x1fff;
  TAILQ_FOREACH(dt, &mux->mm_descrambler_tables, link) {
    if (dt->table->mt_pid != pid || (dt->table->mt_flags & MT_FAST) != flags)
      continue;
    TAILQ_FOREACH(ds, &dt->sections, link) {
      if (ds->opaque == opaque) {
        TAILQ_REMOVE(&dt->sections, ds, link);
        ds->callback(ds->opaque, -1, NULL, 0, (flags & MT_FAST) == 0);
        while ((des = LIST_FIRST(&ds->ecmsecs)) != NULL) {
          LIST_REMOVE(des, link);
          free(des->last_data);
          free(des);
        }
        if (TAILQ_FIRST(&dt->sections) == NULL) {
          TAILQ_REMOVE(&mux->mm_descrambler_tables, dt, link);
          mpegts_table_destroy(dt->table);
          free(dt);
        }
        free(ds);
        tvhtrace("descrambler", "mux %p close pid %04X (%i) (flags 0x%04x) for %p", mux, pid, pid, flags, opaque);
        return 1;
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
  descrambler_ecmsec_t *des;
  descrambler_emm_t *emm;

  if (mux == NULL)
    return;
  tvhtrace("descrambler", "mux %p - flush tables", mux);
  caclient_caid_update(mux, 0, 0, -1);
  pthread_mutex_lock(&mux->mm_descrambler_lock);
  mux->mm_descrambler_flush = 1;
  while ((dt = TAILQ_FIRST(&mux->mm_descrambler_tables)) != NULL) {
    while ((ds = TAILQ_FIRST(&dt->sections)) != NULL) {
      TAILQ_REMOVE(&dt->sections, ds, link);
      ds->callback(ds->opaque, -1, NULL, 0, (dt->table->mt_flags & MT_FAST) ? 0 : 1);
      while ((des = LIST_FIRST(&ds->ecmsecs)) != NULL) {
        LIST_REMOVE(des, link);
        free(des->last_data);
        free(des);
      }
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
      caclient_caid_update(mux, caid, pid, 1);
      pthread_mutex_lock(&mux->mm_descrambler_lock);
      TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link)
        if (emm->caid == caid) {
          emm->to_be_removed = 0;
          if (emm->pid == EMM_PID_UNKNOWN) {
            tvhtrace("descrambler", "attach emm caid %04X (%i) pid %04X (%i)", caid, caid, pid, pid);
            emm->pid = pid;
            descrambler_open_pid_(mux, emm->opaque, pid, emm->callback, NULL);
          }
        }
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
        descrambler_close_pid_(mux, emm->opaque, pid);
      }
      TAILQ_REMOVE(&mux->mm_descrambler_emms, emm, link);
      TAILQ_INSERT_TAIL(&removing, emm, link);
    }
  pthread_mutex_unlock(&mux->mm_descrambler_lock);
  while ((emm = TAILQ_FIRST(&removing)) != NULL) {
    if (emm->pid != EMM_PID_UNKNOWN)
      caclient_caid_update(mux, emm->caid, emm->pid, 0);
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
  if (mux->mm_descrambler_flush)
    goto unlock;
  TAILQ_FOREACH(emm, &mux->mm_descrambler_emms, link) {
    if (emm->caid == caid && emm->opaque == opaque) {
unlock:
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

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
