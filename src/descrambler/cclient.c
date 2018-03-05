/*
 *  tvheadend, card client interface
 *  Copyright (C) 2007 Andreas Öman
 *            (C) 2017 Jaroslav Kysela
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

#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include "tvheadend.h"
#include "tcp.h"
#include "cclient.h"
#include "tvhpoll.h"

/*
 *
 */
static int
cc_check_empty(const uint8_t *str, int len)
{
  while (len--) {
    if (*str) return 0;
    str++;
  }
  return 1;
}

/**
 *
 */
static void
cc_free_ecm_section(cc_ecm_section_t *es)
{
  LIST_REMOVE(es, es_link);
  free(es->es_data);
  free(es);
}

/**
 *
 */
static void
cc_free_ecm_pid(cc_ecm_pid_t *ep)
{
  cc_ecm_section_t *es;
  while ((es = LIST_FIRST(&ep->ep_sections)) != NULL)
    cc_free_ecm_section(es);
  LIST_REMOVE(ep, ep_link);
  free(ep);
}

/**
 *
 */
static void
cc_service_ecm_pid_free(cc_service_t *ct)
{
  cc_ecm_pid_t *ep;

  while((ep = LIST_FIRST(&ct->cs_ecm_pids)) != NULL)
    cc_free_ecm_pid(ep);
}

/**
 *
 */
static void
cc_free_card(cc_card_data_t *cd)
{
  LIST_REMOVE(cd, cs_card);
  descrambler_close_emm(cd->cs_mux, cd, cd->cs_ra.caid);
  emm_reass_done(&cd->cs_ra);
  free(cd);
}

/**
 *
 */
static void
cc_free_cards(cclient_t *cc)
{
  cc_card_data_t *cd;

  while((cd = LIST_FIRST(&cc->cc_cards)) != NULL)
    cc_free_card(cd);
}

/**
 *
 */
char *
cc_get_card_name(cc_card_data_t *pcard, char *buf, size_t buflen)
{
  snprintf(buf, buflen, "ID:%08x CAID:%04x with %d provider%s",
           pcard->cs_id, pcard->cs_ra.caid, pcard->cs_ra.providers_count,
           pcard->cs_ra.providers_count != 1 ? "s" : "");
  return buf;
}

/**
 *
 */
cc_card_data_t *
cc_new_card(cclient_t *cc, uint16_t caid, uint32_t cardid, uint8_t *ua,
            int pcount, uint8_t **pid, uint8_t **psa)
{
  cc_card_data_t *pcard = NULL;
  emm_provider_t *ep;
  const char *n;
  const uint8_t *id, *sa;
  int i, j, c, allocated = 0;
  char buf[256];

  LIST_FOREACH(pcard, &cc->cc_cards, cs_card)
    if (pcard->cs_ra.caid == caid &&
        pcard->cs_id == cardid)
      break;

  if (pcard == NULL) {
    pcard = calloc(1, sizeof(cc_card_data_t));
    emm_reass_init(&pcard->cs_ra, caid);
    pcard->cs_id = cardid;
    allocated = 1;
  }

  if (ua) {
    memcpy(pcard->cs_ra.ua, ua, 8);
    if (cc_check_empty(ua, 8))
      ua = NULL;
  } else {
    memset(pcard->cs_ra.ua, 0, 8);
  }

  free(pcard->cs_ra.providers);
  pcard->cs_ra.providers_count = pcount;
  pcard->cs_ra.providers = calloc(pcount, sizeof(pcard->cs_ra.providers[0]));

  for (i = 0, ep = pcard->cs_ra.providers; i < pcount; i++, ep++) {
    id = pid[i];
    if (id)
      ep->id = (id[0] << 16) | (id[1] << 8) | id[2];
    if (psa)
      memcpy(ep->sa, psa[i], 8);
  }

  n = caid2name(caid) ?: "Unknown";

  if (ua) {
    tvhinfo(cc->cc_subsys, "%s: Connected as user %s "
            "to a %s-card-%08x [CAID:%04x : %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x] "
            "with %d provider%s",
            cc->cc_name, cc->cc_username, n, cardid, caid,
            ua[0], ua[1], ua[2], ua[3], ua[4], ua[5], ua[6], ua[7],
            pcount, pcount != 1 ? "s" : "");
  } else {
    tvhinfo(cc->cc_subsys, "%s: Connected as user %s "
            "to a %s-card-%08x [CAID:%04x] with %d provider%s",
            cc->cc_name, cc->cc_username, n, cardid, caid,
            pcount, pcount != 1 ? "s" : "");
  }

  buf[0] = '\0';
  for (i = j = c = 0, ep = pcard->cs_ra.providers; i < pcount; i++, ep++) {
    if (psa && !cc_check_empty(ep->sa, 8)) {
      sa = ep->sa;
      if (buf[0]) {
        tvhdebug(cc->cc_subsys, "%s: Providers: ID:%08x CAID:%04X:[%s]",
                 cc->cc_name, cardid, caid, buf);
        buf[0] = '\0';
        c = 0;
      }
      tvhdebug(cc->cc_subsys, "%s: Provider ID #%d: 0x%04x:0x%06x %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
               cc->cc_name, i + 1, caid, ep->id,
               sa[0], sa[1], sa[2], sa[3], sa[4], sa[5], sa[6], sa[7]);
    } else {
      tvh_strlcatf(buf, sizeof(buf), c, "%s0x%06x", c > 0 ? "," : "", ep->id);
      if (++j > 5) {
        tvhdebug(cc->cc_subsys, "%s: Providers: ID:%08x CAID:%04X:[%s]",
                 cc->cc_name, cardid, caid, buf);
        buf[0] = '\0';
        c = j = 0;
      }
    }
  }
  if (j > 0)
    tvhdebug(cc->cc_subsys, "%s: Providers: ID:%08x CAID:%04X:[%s]",
             cc->cc_name, cardid, caid, buf);
  if (allocated)
    LIST_INSERT_HEAD(&cc->cc_cards, pcard, cs_card);
  if (cc->cc_emm && ua) {
    ua = pcard->cs_ra.ua;
    i = ua[0] || ua[1] || ua[2] || ua[3] ||
        ua[4] || ua[5] || ua[6] || ua[7];
    if (i)
      cc_emm_set_allowed(cc, 1);
  }

  pcard->cs_running = 1;
  return pcard;
}

/**
 *
 */
void
cc_remove_card(cclient_t *cc, cc_card_data_t *pcard)
{
  cc_service_t *ct;
  cc_ecm_pid_t *ep, *epn;
  cc_ecm_section_t *es, *esn;
  emm_provider_t *emmp;
  char buf[256];
  int i;

  tvhinfo(cc->cc_subsys, "%s: card %s removed", cc->cc_name,
          cc_get_card_name(pcard, buf, sizeof(buf)));

  /* invalidate all requests */
  LIST_FOREACH(ct, &cc->cc_services, cs_link) {
    for (ep = LIST_FIRST(&ct->cs_ecm_pids); ep; ep = epn) {
      epn = LIST_NEXT(ep, ep_link);
      for (es = LIST_FIRST(&ep->ep_sections); es; es = esn) {
        esn = LIST_NEXT(es, es_link);
        if (es->es_caid == pcard->cs_ra.caid) {
          emmp = pcard->cs_ra.providers;
          for (i = 0; i < pcard->cs_ra.providers_count; i++, emmp++)
            if (emmp->id == es->es_provid) {
              cc_free_ecm_section(es);
              break;
            }
          }
        }
        es = esn;
      }
      if (LIST_EMPTY(&ep->ep_sections))
        cc_free_ecm_pid(ep);
    }

  cc_free_card(pcard);
}

/**
 *
 */
void
cc_remove_card_by_id(cclient_t *cc, uint32_t id)
{
  cc_card_data_t *pcard;

  LIST_FOREACH(pcard, &cc->cc_cards, cs_card)
    if (pcard->cs_id == id) {
      cc_remove_card(cc, pcard);
      break;
    }
}

/**
 *
 */
static int
cc_ecm_reset(th_descrambler_t *th)
{
  cc_service_t *ct = (cc_service_t *)th;
  cclient_t *cc = ct->cs_client;
  cc_ecm_pid_t *ep;
  cc_ecm_section_t *es;

  pthread_mutex_lock(&cc->cc_mutex);
  descrambler_change_keystate(th, DS_READY, 1);
  LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link) {
      es->es_keystate = ES_UNKNOWN;
      free(es->es_data);
      es->es_data = NULL;
      es->es_data_len = 0;
    }
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cc->cc_mutex);
  return 0;
}

/**
 *
 */
static void
cc_ecm_idle(th_descrambler_t *th)
{
  cc_service_t *ct = (cc_service_t *)th;
  cclient_t *cc = ct->cs_client;
  cc_ecm_pid_t *ep;
  cc_ecm_section_t *es;

  pthread_mutex_lock(&cc->cc_mutex);
  LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link)
    LIST_FOREACH(es, &ep->ep_sections, es_link) {
      es->es_keystate = ES_IDLE;
      free(es->es_data);
      es->es_data = NULL;
      es->es_data_len = 0;
    }
  ct->ecm_state = ECM_RESET;
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 *
 */
void
cc_ecm_reply(cc_service_t *ct, cc_ecm_section_t *es,
             int key_type, uint8_t *key_even, uint8_t *key_odd,
             int seq)
{
  mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
  cclient_t *cc = ct->cs_client;
  cc_ecm_pid_t *ep;
  cc_ecm_section_t *es2, es3;
  char chaninfo[128];
  int i;
  int64_t delay = (getfastmonoclock() - es->es_time) / 1000LL; // in ms

  es->es_pending = 0;

  snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", es->es_capid);

  if (key_even == NULL || key_odd == NULL) {

    /* ERROR */
    if (es->es_nok < CC_MAX_NOKS)
      es->es_nok++;

    if(es->es_keystate == ES_FORBIDDEN)
      return; // We already know it's bad

    if (es->es_nok >= CC_MAX_NOKS) {
      tvhdebug(cc->cc_subsys,
               "%s: Too many NOKs[%i] for service \"%s\"%s from %s",
               cc->cc_name, es->es_section, t->s_dvb_svcname, chaninfo, ct->td_nicename);
      es->es_keystate = ES_FORBIDDEN;
      goto forbid;
    }

    if (descrambler_resolved((service_t *)t, (th_descrambler_t *)ct)) {
      tvhdebug(cc->cc_subsys,
              "%s: NOK[%i] from %s: Already has a key for service \"%s\"",
               cc->cc_name, es->es_section, ct->td_nicename, t->s_dvb_svcname);
      es->es_nok = CC_MAX_NOKS; /* do not send more ECM requests */
      es->es_keystate = ES_IDLE;
      if (ct->td_keystate == DS_READY)
        descrambler_change_keystate((th_descrambler_t *)ct, DS_IDLE, 1);
    }

    tvhdebug(cc->cc_subsys,
             "%s: Received NOK[%i] for service \"%s\"%s "
             "(seqno: %d Req delay: %"PRId64" ms)",
             cc->cc_name, es->es_section, t->s_dvb_svcname, chaninfo, seq, delay);

forbid:
    i = 0;
    LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link)
      LIST_FOREACH(es2, &ep->ep_sections, es_link)
        if(es2 && es2 != es && es2->es_nok == 0) {
          if (es2->es_pending)
            return;
          i++;
        }
    if (i && es->es_nok < CC_MAX_NOKS)
      return;
    
    es->es_keystate = ES_FORBIDDEN;
    LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link) {
      LIST_FOREACH(es2, &ep->ep_sections, es_link)
        if (es2->es_keystate == ES_UNKNOWN ||
            es2->es_keystate == ES_RESOLVED)
          break;
      if (es2)
        break;
    }

    if (ep == NULL) { /* !UNKNOWN && !RESOLVED */
      tvherror(cc->cc_subsys,
               "%s: Can not descramble service \"%s\", access denied (seqno: %d "
               "Req delay: %"PRId64" ms) from %s",
               cc->cc_name, t->s_dvb_svcname, seq, delay, ct->td_nicename);
      descrambler_change_keystate((th_descrambler_t *)ct, DS_FORBIDDEN, 1);
      ct->ecm_state = ECM_RESET;
      /* this pid is not valid, force full scan */
      if (t->s_dvb_prefcapid == ct->cs_capid &&
          t->s_dvb_prefcapid_lock == PREFCAPID_OFF)
        t->s_dvb_prefcapid = 0;
    }
    return;

  } else {

    es->es_nok = 0;
    ct->cs_capid = es->es_capid;
    ct->ecm_state = ECM_VALID;

    if(t->s_dvb_prefcapid == 0 ||
       (t->s_dvb_prefcapid != ct->cs_capid &&
        t->s_dvb_prefcapid_lock == PREFCAPID_OFF)) {
      t->s_dvb_prefcapid = ct->cs_capid;
      tvhdebug(cc->cc_subsys, "%s: Saving prefered PID %d for %s",
               cc->cc_name, t->s_dvb_prefcapid, ct->td_nicename);
      service_request_save((service_t*)t);
    }

    tvhdebug(cc->cc_subsys,
             "%s: Received ECM reply%s for service \"%s\" [%d] "
             "(seqno: %d Req delay: %"PRId64" ms)",
             cc->cc_name, chaninfo, t->s_dvb_svcname, es->es_section, seq, delay);

    if(es->es_keystate != ES_RESOLVED)
      tvhdebug(cc->cc_subsys,
               "%s: Obtained DES keys for service \"%s\" in %"PRId64" ms, from %s",
               cc->cc_name, t->s_dvb_svcname, delay, ct->td_nicename);
    es->es_keystate = ES_RESOLVED;
    es->es_resolved = 1;

    es3 = *es;
    pthread_mutex_unlock(&cc->cc_mutex);
    descrambler_keys((th_descrambler_t *)ct, key_type, 0, key_even, key_odd);
    snprintf(chaninfo, sizeof(chaninfo), "%s:%i", cc->cc_hostname, cc->cc_port);
    descrambler_notify((th_descrambler_t *)ct,
                       es3.es_caid, es3.es_provid,
                       caid2name(es3.es_caid),
                       es3.es_capid, delay,
                       1, "", chaninfo, cc->cc_id);
    pthread_mutex_lock(&cc->cc_mutex);
  }
}

/**
 *
 */
cc_ecm_section_t *
cc_find_pending_section(cclient_t *cc, uint32_t seq, cc_service_t **_ct)
{
  cc_service_t *ct;
  cc_ecm_pid_t *ep;
  cc_ecm_section_t *es;

  if (_ct) *_ct = NULL;
  LIST_FOREACH(ct, &cc->cc_services, cs_link)
    LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link)
      LIST_FOREACH(es, &ep->ep_sections, es_link)
        if(es->es_seq == seq) {
          if (es->es_resolved) {
            mpegts_service_t *t = (mpegts_service_t *)ct->td_service;
            tvhdebug(cc->cc_subsys,
                     "%s: Ignore %sECM (PID %d) for service \"%s\" from %s (seq %i)",
                     cc->cc_name,
                     es->es_pending ? "duplicate " : "",
                     ep->ep_capid, t->s_dvb_svcname, ct->td_nicename, es->es_seq);
            if (_ct) *_ct = ct;
            return NULL;
          }
          if (es->es_pending) {
            if (_ct) *_ct = ct;
            return es;
          }
        }
  tvhwarn(cc->cc_subsys, "%s: Got unexpected ECM reply (seqno: %d)",
          cc->cc_name, seq);
  return NULL;
}

/**
 *
 */
static void
cc_invalidate_cards(cclient_t *cc)
{
  cc_card_data_t *cd;

  LIST_FOREACH(cd, &cc->cc_cards, cs_card)
    cd->cs_running = 0;
}

/**
 *
 */
static void
cc_flush_services(cclient_t *cc)
{
  cc_service_t *ct;

  LIST_FOREACH(ct, &cc->cc_services, cs_link)
    descrambler_flush_table_data(ct->td_service);
}

/**
 *
 */
int
cc_read(cclient_t *cc, void *buf, size_t len, int timeout)
{
  int r;

  pthread_mutex_unlock(&cc->cc_mutex);
  r = tcp_read_timeout(cc->cc_fd, buf, len, timeout);
  pthread_mutex_lock(&cc->cc_mutex);

  if (r && tvheadend_is_running())
    tvhwarn(cc->cc_subsys, "%s: read error %d (%s)",
            cc->cc_name, r, strerror(r));

  if (cc_must_break(cc))
    return ECONNABORTED;

  tvhtrace(cc->cc_subsys, "%s: read len %zd", cc->cc_name, len);
  tvhlog_hexdump(cc->cc_subsys, buf, len);
  return r;
}

/**
 *
 */
void
cc_write_message(cclient_t *cc, cc_message_t *msg, int enq)
{
  if (enq) {
    lock_assert(&cc->cc_mutex);
    if (cc->cc_write_running) {
      TAILQ_INSERT_TAIL(&cc->cc_writeq, msg, cm_link);
      tvh_nonblock_write(cc->cc_pipe.wr, "w", 1);
    } else {
      free(msg);
    }
  } else {
    tvhtrace(cc->cc_subsys, "%s: sending message len %u",
             cc->cc_name, msg->cm_len);
    tvhlog_hexdump(cc->cc_subsys, msg->cm_data, msg->cm_len);
    if (tvh_write(cc->cc_fd, msg->cm_data, msg->cm_len))
      tvhinfo(cc->cc_subsys, "%s: write error: %s",
              cc->cc_name, strerror(errno));
    free(msg);
  }
}

/**
 *
 */
static void
cc_session(cclient_t *cc)
{
  tvhpoll_t *poll;
  tvhpoll_event_t ev;
  char buf[16];
  sbuf_t rbuf;
  cc_message_t *cm;
  ssize_t len;
  int64_t mono;
  int r;

  if (cc->cc_init_session(cc))
    return;

  /**
   * Ok, connection good, reset retry delay to zero
   */
  cc->cc_retry_delay = 0;
  cc_flush_services(cc);

  /**
   * We do all requests from now on in a separate thread
   */
  TAILQ_INIT(&cc->cc_writeq);
  cc->cc_write_running = 1;
  sbuf_init(&rbuf);

  /**
   * Mainloop
   */
  poll = tvhpoll_create(1);
  tvhpoll_add1(poll, cc->cc_pipe.rd, TVHPOLL_IN, &cc->cc_pipe);
  tvhpoll_add1(poll, cc->cc_fd, TVHPOLL_IN, &cc->cc_fd);
  mono = mclk() + sec2mono(cc->cc_keepalive_interval);
  while (!cc_must_break(cc)) {
    pthread_mutex_unlock(&cc->cc_mutex);
    r = tvhpoll_wait(poll, &ev, 1, 1000);
    pthread_mutex_lock(&cc->cc_mutex);
    if (r == 0)
      continue;
    if (r < 0 && ERRNO_AGAIN(errno))
      continue;
    if (r < 0)
      break;
    if (ev.ptr == &cc->cc_pipe) {
      len = read(cc->cc_pipe.rd, buf, sizeof(buf));
    } else if (ev.ptr == &cc->cc_fd) {
      sbuf_alloc(&rbuf, 1024);
      len = sbuf_read(&rbuf, cc->cc_fd);
      if (len > 0) {
        tvhtrace(cc->cc_subsys, "%s: read len %zd", cc->cc_name, len);
        tvhlog_hexdump(cc->cc_subsys, rbuf.sb_data + rbuf.sb_ptr - len, len);
        if (cc->cc_read(cc, &rbuf))
          break;
      }
    } else {
      abort();
    }
    if ((cm = TAILQ_FIRST(&cc->cc_writeq)) != NULL) {
      TAILQ_REMOVE(&cc->cc_writeq, cm, cm_link);
      tvhtrace(cc->cc_subsys, "%s: sending queued message len %u",
               cc->cc_name, cm->cm_len);
      tvhlog_hexdump(cc->cc_subsys, cm->cm_data, cm->cm_len);
      if (tvh_nonblock_write(cc->cc_fd, cm->cm_data, cm->cm_len)) {
        free(cm);
        break;
      }
      free(cm);
    }
    if (mono < mclk()) {
      mono = mclk() + sec2mono(cc->cc_keepalive_interval);
      if (cc->cc_keepalive)
        cc->cc_keepalive(cc);
    }
  }
  tvhdebug(cc->cc_subsys, "%s: session exiting", cc->cc_name);
  tvhpoll_destroy(poll);
  sbuf_free(&rbuf);
  shutdown(cc->cc_fd, SHUT_RDWR);
}

/**
 *
 */
static void *
cc_thread(void *aux)
{
  cclient_t *cc = aux;
  int fd, d, r;
  char errbuf[100];
  char name[256];
  char hostname[256];
  int port;
  int attempts = 0;
  int64_t mono;

  pthread_mutex_lock(&cc->cc_mutex);

  while(cc->cc_running) {

    cc_invalidate_cards(cc);
    caclient_set_status((caclient_t *)cc, CACLIENT_STATUS_READY);
    
    snprintf(name, sizeof(name), "%s:%d", cc->cc_hostname, cc->cc_port);
    cc->cc_name = name;

    snprintf(hostname, sizeof(hostname), "%s", cc->cc_hostname);
    port = cc->cc_port;

    tvhinfo(cc->cc_subsys, "%s: Attemping to connect to server", cc->cc_name);

    pthread_mutex_unlock(&cc->cc_mutex);

    fd = tcp_connect(hostname, port, NULL, errbuf, sizeof(errbuf), 10);

    pthread_mutex_lock(&cc->cc_mutex);

    if(fd == -1) {
      attempts++;
      tvhinfo(cc->cc_subsys,
              "%s: Connection failed: %s", cc->cc_name, errbuf);
    } else {

      if(cc->cc_running == 0) {
        close(fd);
        break;
      }

      tvhdebug(cc->cc_subsys, "%s: Connected", cc->cc_name);
      attempts = 0;

      cc->cc_fd = fd;
      cc->cc_reconfigure = 0;

      cc_session(cc);

      cc->cc_fd = -1;
      close(fd);
      tvhinfo(cc->cc_subsys, "%s: Disconnected", cc->cc_name);
    }

    if(cc->cc_running == 0) continue;
    if(attempts == 1 || cc->cc_reconfigure) {
      cc->cc_reconfigure = 0;
      continue; // Retry immediately
    }

    caclient_set_status((caclient_t *)cc, CACLIENT_STATUS_DISCONNECTED);

    d = 3;

    tvhinfo(cc->cc_subsys,
            "%s: Automatic connection attempt in %d seconds",
            cc->cc_name, d-1);

    mono = mclk() + sec2mono(d);
    do {
      r = tvh_cond_timedwait(&cc->cc_cond, &cc->cc_mutex, mono);
      if (r == ETIMEDOUT)
        break;
    } while (ERRNO_AGAIN(r));
  }

  tvhinfo(cc->cc_subsys, "%s: Inactive, thread exit", cc->cc_name);
  cc_free_cards(cc);
  cc->cc_name = NULL;
  pthread_mutex_unlock(&cc->cc_mutex);
  return NULL;
}


/**
 *
 */
static int
verify_provider(cc_card_data_t *pcard, uint32_t providerid)
{
  int i;

  if(providerid == 0)
    return 1;
  
  for(i = 0; i < pcard->cs_ra.providers_count; i++)
    if(providerid == pcard->cs_ra.providers[i].id)
      return 1;
  return 0;
}

/**
 *
 */
void
cc_emm_set_allowed(cclient_t *cc, int emm_allowed)
{
  cc_card_data_t *pcard;

  if (!emm_allowed) {
    tvhinfo(cc->cc_subsys,
            "%s: Will not forward EMMs (not allowed by server)",
            cc->cc_name);
  } else {
    LIST_FOREACH(pcard, &cc->cc_cards, cs_card)
      if (pcard->cs_ra.type != CARD_UNKNOWN) break;
    if (pcard) {
      tvhinfo(cc->cc_subsys, "%s: Will forward EMMs", cc->cc_name);
    } else {
      tvhinfo(cc->cc_subsys,
              "%s: Will not forward EMMs (unsupported CA system)",
              cc->cc_name);
      emm_allowed = 0;
    }
  }
  cc->cc_forward_emm = !!emm_allowed;
}

/**
 *
 */
static void
cc_emm_send(void *aux, const uint8_t *radata, int ralen, void *mux)
{
  cc_card_data_t *pcard = aux;
  cclient_t *cc = pcard->cs_client;

  tvhtrace(cc->cc_subsys, "%s: sending EMM for %04x mux %p",
           cc->cc_name, pcard->cs_ra.caid, mux);
  tvhlog_hexdump(cc->cc_subsys, radata, ralen);
  cc->cc_send_emm(cc, NULL, pcard, 0, radata, ralen);
}

/**
 *
 */
static void
cc_emm(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cc_card_data_t *pcard = opaque;
  cclient_t *cc;
  void *mux;

  if (data == NULL) {  /* end-of-data */
    pcard->cs_mux = NULL;
    return;
  }
  if (pcard->cs_mux == NULL)
    return;
  cc = pcard->cs_client;
  pthread_mutex_lock(&cc->cc_mutex);
  mux = pcard->cs_mux;
  if (pcard->cs_running && cc->cc_forward_emm && cc->cc_write_running) {
    if (cc->cc_emmex) {
      if (cc->cc_emm_mux && cc->cc_emm_mux != mux) {
        if (cc->cc_emm_update_time + sec2mono(25) > mclk())
          goto end_of_job;
      }
      cc->cc_emm_update_time = mclk();
    }
    cc->cc_emm_mux = mux;
    emm_filter(&pcard->cs_ra, data, len, mux, cc_emm_send, pcard);
  }
end_of_job:
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 *
 */
static void
cc_table_input(void *opaque, int pid, const uint8_t *data, int len, int emm)
{
  cc_service_t *ct = opaque;
  elementary_stream_t *st;
  mpegts_service_t *t = (mpegts_service_t*)ct->td_service;
  cclient_t *cc = ct->cs_client;
  int capid, section, ecm;
  cc_ecm_pid_t *ep;
  cc_ecm_section_t *es;
  char chaninfo[32];
  cc_card_data_t *pcard = NULL;
  caid_t *c;
  uint16_t caid;
  uint32_t provid;

  if (data == NULL)
    return;

  if (len > 4096)
    return;

  pthread_mutex_lock(&cc->cc_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);

  if (ct->td_keystate == DS_IDLE)
    goto end;

  if (ct->ecm_state == ECM_RESET) {
    /* clean all */
    cc_service_ecm_pid_free(ct);
    /* move to init state */
    ct->ecm_state = ECM_INIT;
    ct->cs_capid = 0xffff;
    t->s_dvb_prefcapid = 0;
    tvhdebug(cc->cc_subsys, "%s: Reset after unexpected or no reply for service \"%s\"",
             cc->cc_name, t->s_dvb_svcname);
  }

  LIST_FOREACH(ep, &ct->cs_ecm_pids, ep_link)
    if(ep->ep_capid == pid) break;

  if(ep == NULL) {
    if (ct->ecm_state == ECM_INIT) {
      // Validate prefered ECM PID
      tvhdebug(cc->cc_subsys, "%s: ECM state INIT", cc->cc_name);

      if(t->s_dvb_prefcapid_lock != PREFCAPID_OFF) {
        st = service_stream_find((service_t*)t, t->s_dvb_prefcapid);
        if (st && st->es_type == SCT_CA)
          LIST_FOREACH(c, &st->es_caids, link)
            LIST_FOREACH(pcard, &cc->cc_cards, cs_card)
              if(pcard->cs_running &&
                 pcard->cs_ra.caid == c->caid &&
                 verify_provider(pcard, c->providerid))
                goto prefcapid_ok;
        tvhdebug(cc->cc_subsys, "%s: Invalid prefered ECM (PID %d) found for service \"%s\"",
                 cc->cc_name, cc->cc_port, t->s_dvb_svcname);
        t->s_dvb_prefcapid = 0;
      }

prefcapid_ok:
      if(t->s_dvb_prefcapid == pid || t->s_dvb_prefcapid == 0 ||
         t->s_dvb_prefcapid_lock == PREFCAPID_OFF) {
        ep = calloc(1, sizeof(cc_ecm_pid_t));
        ep->ep_capid = pid;
        LIST_INSERT_HEAD(&ct->cs_ecm_pids, ep, ep_link);
        tvhdebug(cc->cc_subsys, "%s: Insert %s ECM (PID %d) for service \"%s\"",
                 cc->cc_name, t->s_dvb_prefcapid ? "preferred" : "new", pid, t->s_dvb_svcname);
      }
    }
    if(ep == NULL)
      goto end;
  }

  st = service_stream_find((service_t *)t, pid);
  if (st) {
    LIST_FOREACH(c, &st->es_caids, link)
      LIST_FOREACH(pcard, &cc->cc_cards, cs_card)
        if(pcard->cs_running &&
           pcard->cs_ra.caid == c->caid &&
           verify_provider(pcard, c->providerid))
          goto found;
  }

  goto end;

found:
  caid = c->caid;
  provid = c->providerid;

  ecm = data[0] == 0x80 || data[0] == 0x81;
  if (pcard->cs_ra.caid == 0x4a30) ecm |= data[0] == 0x50; /* DVN */

  if (ecm) {
    if ((pcard->cs_ra.caid >> 8) == 6) {
      ep->ep_last_section = data[5];
      section = data[4];
    } else if (pcard->cs_ra.caid == 0xe00) {
      ep->ep_last_section = 0;
      section = data[0] & 1;
    } else {
      ep->ep_last_section = 0;
      section = 0;
    }

    capid = pid;
    snprintf(chaninfo, sizeof(chaninfo), " (PID %d)", capid);

    LIST_FOREACH(es, &ep->ep_sections, es_link)
      if (es->es_section == section)
        break;
    if (es == NULL) {
      es = calloc(1, sizeof(cc_ecm_section_t));
      es->es_section = section;
      LIST_INSERT_HEAD(&ep->ep_sections, es, es_link);
    }
    if (es->es_data_len == len && memcmp(es->es_data, data, len) == 0)
      goto end;
    if (es->es_data_len < len) {
      free(es->es_data);
      es->es_data = malloc(len);
    }
    memcpy(es->es_data, data, len);
    es->es_data_len = len;

    if(cc->cc_fd == -1) {
      // New key, but we are not connected (anymore), can not descramble
      descrambler_change_keystate((th_descrambler_t *)ct, DS_READY, 0);
      goto end;
    }

    if (es->es_keystate == ES_FORBIDDEN || es->es_keystate == ES_IDLE)
      goto end;

    es->es_caid = caid;
    es->es_provid = provid;
    es->es_capid = capid;
    es->es_pending = 1;
    es->es_resolved = 0;

    if (ct->cs_capid != 0xffff && ct->cs_capid > 0 &&
       capid > 0 && ct->cs_capid != capid) {
      tvhdebug(cc->cc_subsys, "%s: Filtering ECM (PID %d), using PID %d",
               cc->cc_name, capid, ct->cs_capid);
      goto end;
    }

    if (cc->cc_send_ecm(cc, ct, es, pcard, data, len) == 0) {
      tvhdebug(cc->cc_subsys,
               "%s: Sending ECM%s section=%d/%d for service \"%s\" (seqno: %d)",
               cc->cc_name, chaninfo, section,
               ep->ep_last_section, t->s_dvb_svcname, es->es_seq);
      es->es_time = getfastmonoclock();
    } else {
      es->es_pending = 0;
    }
  } else {
    if (cc->cc_forward_emm && data[0] >= 0x82 && data[0] <= 0x92) {
      tvhtrace(cc->cc_subsys, "%s: sending EMM for %04X:%06X service \"%s\"",
               cc->cc_name, pcard->cs_ra.caid, provid,
               t->s_dvb_svcname);
      tvhlog_hexdump(cc->cc_subsys, data, len);
      emm_filter(&pcard->cs_ra, data, len, t->s_dvb_mux, cc_emm_send, pcard);
    }
  }

end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 * cc_mutex is held
 */
static void
cc_service_destroy0(cclient_t *cc, th_descrambler_t *td)
{
  cc_service_t *ct = (cc_service_t *)td;
  int i;

  for (i = 0; i < ct->cs_epids.count; i++)
    descrambler_close_pid(ct->cs_mux, ct, ct->cs_epids.pids[i].pid);
  mpegts_pid_done(&ct->cs_epids);

  cc_service_ecm_pid_free(ct);

  LIST_REMOVE(td, td_service_link);

  LIST_REMOVE(ct, cs_link);

  free(ct->td_nicename);
  free(ct);

  if (LIST_EMPTY(&cc->cc_services) && cc->cc_no_services)
    cc->cc_no_services(cc);
}

/**
 * cc_mutex is held
 */
static void
cc_service_destroy(th_descrambler_t *td)
{
  cc_service_t *ct = (cc_service_t *)td;
  cclient_t *cc = ct->cs_client;

  pthread_mutex_lock(&cc->cc_mutex);
  cc_service_destroy0(cc, td);
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held. Not that we care about that, but either way, it is.
 */
void
cc_service_start(caclient_t *cac, service_t *t)
{
  cclient_t *cc = (cclient_t *)cac;
  cc_service_t *ct;
  th_descrambler_t *td;
  elementary_stream_t *st;
  caid_t *c;
  cc_card_data_t *pcard;
  char buf[512];
  int i, reuse = 0, prefpid, prefpid_lock, forcecaid;
  mpegts_apids_t epids;

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;

  pthread_mutex_lock(&cc->cc_mutex);
  pthread_mutex_lock(&t->s_stream_mutex);
  LIST_FOREACH(ct, &cc->cc_services, cs_link) {
    if (ct->td_service == t && ct->cs_client == cc)
      break;
  }
  prefpid      = ((mpegts_service_t *)t)->s_dvb_prefcapid;
  prefpid_lock = ((mpegts_service_t *)t)->s_dvb_prefcapid_lock;
  forcecaid    = ((mpegts_service_t *)t)->s_dvb_forcecaid;
  LIST_FOREACH(pcard, &cc->cc_cards, cs_card) {
    if (!pcard->cs_running) continue;
    if (pcard->cs_ra.caid == 0) continue;
    TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
      if (prefpid_lock == PREFCAPID_FORCE && prefpid != st->es_pid)
        continue;
      LIST_FOREACH(c, &st->es_caids, link) {
        if (c->use && c->caid == pcard->cs_ra.caid)
          if (!forcecaid || forcecaid == c->caid)
            break;
      }
      if (c) break;
    }
    if (st) break;
  }
  if (!pcard) {
    if (ct) cc_service_destroy0(cc, (th_descrambler_t*)ct);
    goto end;
  }
  if (ct) {
    reuse = 1;
    for (i = 0; i < ct->cs_epids.count; i++) {
      TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
        if (st->es_pid != ct->cs_epids.pids[i].pid) continue;
        LIST_FOREACH(c, &st->es_caids, link)
          if (c->use && c->caid == pcard->cs_ra.caid)
            break;
        if (c) break;
      }
      if (st == NULL) {
        descrambler_close_pid(ct->cs_mux, ct, ct->cs_epids.pids[i].pid);
        reuse |= 2;
      }
    }
    goto add;
  }

  if (cc->cc_alloc_service) {
    ct = cc->cc_alloc_service(cc);
  } else {
    ct = calloc(1, sizeof(*ct));
  }
  ct->cs_client        = cc;
  ct->cs_capid         = 0xffff;
  ct->cs_mux           = ((mpegts_service_t *)t)->s_dvb_mux;
  ct->ecm_state        = ECM_INIT;

  td                   = (th_descrambler_t *)ct;
  snprintf(buf, sizeof(buf), "%s-%s-%04X",
           cc->cc_id, cc->cc_name, pcard->cs_ra.caid);
  td->td_nicename      = strdup(buf);
  td->td_service       = t;
  td->td_stop          = cc_service_destroy;
  td->td_ecm_reset     = cc_ecm_reset;
  td->td_ecm_idle      = cc_ecm_idle;
  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&cc->cc_services, ct, cs_link);

  descrambler_change_keystate(td, DS_READY, 0);

add:
  i = 0;
  mpegts_pid_init(&epids);
  TAILQ_FOREACH(st, &t->s_filt_components, es_filt_link) {
    LIST_FOREACH(c, &st->es_caids, link)
      if (c->use && c->caid == pcard->cs_ra.caid)
        mpegts_pid_add(&epids, st->es_pid, 0);
  }
  if (mpegts_pid_cmp(&ct->cs_epids, &epids))
    reuse |= 2;
  mpegts_pid_copy(&ct->cs_epids, &epids);
  mpegts_pid_done(&epids);

  for (i = 0; i < ct->cs_epids.count; i++)
    descrambler_open_pid(ct->cs_mux, ct, ct->cs_epids.pids[i].pid,
                         cc_table_input, t);

  if (reuse & 2) {
    ct->cs_capid = 0xffff;
    ct->ecm_state = ECM_INIT;
  }

  if (reuse != 1)
    tvhdebug(cc->cc_subsys, "%s: %s %susing CWC %s:%d",
             cc->cc_name, service_nicename(t), reuse ? "re" : "", cc->cc_hostname, cc->cc_port);

end:
  pthread_mutex_unlock(&t->s_stream_mutex);
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 *
 */
void
cc_free(caclient_t *cac)
{
  cclient_t *cc = (cclient_t *)cac;
  cc_service_t *ct;

  while((ct = LIST_FIRST(&cc->cc_services)) != NULL)
    cc_service_destroy((th_descrambler_t *)ct);

  cc_free_cards(cc);
  free((void *)cc->cc_password);
  free((void *)cc->cc_username);
  free((void *)cc->cc_hostname);
}

/**
 *
 */
void
cc_caid_update(caclient_t *cac, mpegts_mux_t *mux, uint16_t caid, uint16_t pid, int valid)
{
  cclient_t *cc = (cclient_t *)cac;;
  cc_card_data_t *pcard;

  tvhtrace(cc->cc_subsys,
           "%s: caid update event - client %s mux %p caid %04x (%i) pid %04x (%i) valid %i",
           cc->cc_name, cac->cac_name, mux, caid, caid, pid, pid, valid);
  pthread_mutex_lock(&cc->cc_mutex);
  if (valid < 0 || cc->cc_running) {
    LIST_FOREACH(pcard, &cc->cc_cards, cs_card) {
      if (valid < 0 || pcard->cs_ra.caid == caid) {
        if (pcard->cs_mux && pcard->cs_mux != mux) continue;
        if (valid > 0) {
          pcard->cs_client = cc;
          pcard->cs_mux    = mux;
          descrambler_open_emm(mux, pcard, caid, cc_emm);
        } else {
          pcard->cs_mux    = NULL;
          descrambler_close_emm(mux, pcard, caid);
        }
      }
    }
  }
  pthread_mutex_unlock(&cc->cc_mutex);
}

/**
 *
 */
void
cc_conf_changed(caclient_t *cac)
{
  cclient_t *cc = (cclient_t *)cac;
  pthread_t tid;
  cc_message_t *cm;
  char tname[32];

  if (cac->cac_enabled) {
    if (cc->cc_hostname == NULL || cc->cc_hostname[0] == '\0') {
      caclient_set_status(cac, CACLIENT_STATUS_NONE);
      return;
    }
    pthread_mutex_lock(&cc->cc_mutex);
    if (!cc->cc_running) {
      cc->cc_running = 1;
      tvh_pipe(O_NONBLOCK, &cc->cc_pipe);
      snprintf(tname, sizeof(tname), "cc-%s", cc->cc_id);
      tvhthread_create(&cc->cc_tid, NULL, cc_thread, cc, tname);
      pthread_mutex_unlock(&cc->cc_mutex);
      return;
    }
    cc->cc_reconfigure = 1;
    if(cc->cc_fd >= 0)
      shutdown(cc->cc_fd, SHUT_RDWR);
    tvh_cond_signal(&cc->cc_cond, 0);
    pthread_mutex_unlock(&cc->cc_mutex);
  } else {
    if (!cc->cc_running)
      return;
    pthread_mutex_lock(&cc->cc_mutex);
    cc->cc_running = 0;
    tvh_cond_signal(&cc->cc_cond, 0);
    tid = cc->cc_tid;
    if (cc->cc_fd >= 0)
      shutdown(cc->cc_fd, SHUT_RDWR);
    pthread_mutex_unlock(&cc->cc_mutex);
    tvh_write(cc->cc_pipe.wr, "q", 1);
    pthread_kill(tid, SIGHUP);
    pthread_join(tid, NULL);
    tvh_pipe_close(&cc->cc_pipe);
    caclient_set_status(cac, CACLIENT_STATUS_NONE);
    while ((cm = TAILQ_FIRST(&cc->cc_writeq)) != NULL) {
      TAILQ_REMOVE(&cc->cc_writeq, cm, cm_link);
      free(cm);
    }
  }
}

/**
 *
 */
const idclass_t caclient_cc_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_card",
  .ic_caption    = N_("Card client"),
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Login Settings"),
      .number = 2,
    },
    {
      .name   = N_("EMM Settings"),
      .number = 3,
    },
    {
      .name   = N_("Connection Settings"),
      .number = 4,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .desc     = N_("Login username."),
      .off      = offsetof(cclient_t, cc_username),
      .opts     = PO_TRIM,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = N_("Password"),
      .desc     = N_("Login password."),
      .off      = offsetof(cclient_t, cc_password),
      .opts     = PO_PASSWORD,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "hostname",
      .name     = N_("Hostname/IP"),
      .desc     = N_("Hostname (or IP) of the server."),
      .off      = offsetof(cclient_t, cc_hostname),
      .def.s    = "localhost",
      .opts     = PO_TRIM,
      .group    = 2,
    },
    {
      .type     = PT_INT,
      .id       = "port",
      .name     = N_("Port"),
      .desc     = N_("Port to connect to."),
      .off      = offsetof(cclient_t, cc_port),
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "emm",
      .name     = N_("Update card (EMM)"),
      .desc     = N_("Enable/disable offering of Entitlement Management Message updates."),
      .off      = offsetof(cclient_t, cc_emm),
      .def.i    = 1,
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "emmex",
      .name     = N_("Updates from one mux (EMM)"),
      .desc     = N_("Update Entitlement Management Messages from one mux only."),
      .off      = offsetof(cclient_t, cc_emmex),
      .def.i    = 1,
      .group    = 3,
    },
    {
      .type     = PT_INT,
      .id       = "keepalive_interval",
      .name     = N_("Keepalive interval"),
      .desc     = N_("Keepalive interval in seconds"),
      .off      = offsetof(cclient_t, cc_keepalive_interval),
      .def.i    = CC_KEEPALIVE_INTERVAL,
      .group    = 4,
    },
    { }
  }
};
