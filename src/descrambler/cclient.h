/*
 *  tvheadend, network card client
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

#include "tvheadend.h"
#include "caclient.h"
#include "descrambler.h"
#include "emm_reass.h"
#include "input.h"

/**
 *
 */
#define CC_KEEPALIVE_INTERVAL 30
#define CC_MAX_NOKS           3

/**
 *
 */
typedef struct cc_ecm_section {
  LIST_ENTRY(cc_ecm_section) es_link;

  enum {
    ES_UNKNOWN,
    ES_RESOLVED,
    ES_FORBIDDEN,
    ES_IDLE
  } es_keystate;

  int      es_section;
  uint8_t *es_data;
  uint32_t es_data_len;

  uint32_t es_card_id;
  uint16_t es_capid;
  uint16_t es_caid;
  uint32_t es_provid;

  uint32_t es_seq;
  uint8_t  es_nok;
  uint8_t  es_pending;
  uint8_t  es_resolved;
  int64_t  es_time;  // time request was sent

} cc_ecm_section_t;

/**
 *
 */
typedef struct cc_ecm_pid {
  LIST_ENTRY(cc_ecm_pid) ep_link;

  uint16_t ep_capid;

  int ep_last_section;
  LIST_HEAD(, cc_ecm_section) ep_sections;
} cc_ecm_pid_t;

/**
 *
 */
typedef struct cc_service {
  th_descrambler_t;

  void *cs_client;

  LIST_ENTRY(cc_service) cs_link;

  uint16_t cs_capid;
  mpegts_apids_t cs_epids;

  mpegts_mux_t *cs_mux;

  /**
   * ECM Status
   */
  enum {
    ECM_INIT,
    ECM_VALID,
    ECM_RESET
  } ecm_state;

  LIST_HEAD(, cc_ecm_pid) cs_ecm_pids;

} cc_service_t;

/**
 *
 */
typedef struct cc_message {
  TAILQ_ENTRY(cc_message) cm_link;
  uint32_t cm_len;
  uint8_t  cm_data[0];
} cc_message_t;

/**
 *
 */
typedef struct cc_card_data {
  LIST_ENTRY(cc_card_data) cs_card;
  uint32_t      cs_id;
  emm_reass_t   cs_ra;
  void         *cs_client;
  mpegts_mux_t *cs_mux;
  uint8_t       cs_running;
  union {
    struct {
      uint32_t  cs_remote_id;
      uint8_t   cs_hop;
      uint8_t   cs_reshare;
    } cccam;
  };
} cc_card_data_t;

/**
 *
 */
typedef struct cclient {
  caclient_t;

  int cc_subsys;
  char *cc_name;
  const char *cc_id;

  /* Callbacks */
  void *(*cc_alloc_service)(void *cc);
  int (*cc_init_session)(void *cc);
  int (*cc_read)(void *cc, sbuf_t *sbuf);
  void (*cc_keepalive)(void *cc);
  int (*cc_send_ecm)(void *cc, cc_service_t *ct, cc_ecm_section_t *es,
                     cc_card_data_t *pcard, const uint8_t *data, int len);
  void (*cc_send_emm)(void *cc, cc_service_t *ct, cc_card_data_t *pcard,
                      uint32_t provid, const uint8_t *data, int len);
  void (*cc_no_services)(void *cc);

  /* Connection */
  int cc_fd;
  int cc_keepalive_interval;
  int cc_retry_delay;

  /* Thread */
  pthread_t cc_tid;
  tvh_cond_t cc_cond;
  tvh_mutex_t cc_mutex;
  th_pipe_t cc_pipe;

  /* Database */
  LIST_HEAD(, cc_service) cc_services;
  LIST_HEAD(, cc_card_data) cc_cards;

  /* Writer */
  int cc_seq;
  TAILQ_HEAD(, cc_message) cc_writeq;
  uint8_t cc_write_running;

  /* Emm forwarding */
  int cc_forward_emm;

  /* Emm exclusive update */
  int64_t cc_emm_update_time;
  void *cc_emm_mux;

  /* From configuration */
  char *cc_username;
  char *cc_password;
  char *cc_hostname;
  int cc_port;
  int cc_emm;
  int cc_emmex;

  uint8_t cc_running;
  uint8_t cc_reconfigure;
} cclient_t;

/*
 *
 */
static inline int cc_must_break(cclient_t *cc)
 { return !cc->cc_running || !cc->cac_enabled || cc->cc_reconfigure; }

char *cc_get_card_name(cc_card_data_t *pcard, char *buf, size_t buflen);
cc_card_data_t *cc_new_card
  (cclient_t *cc, uint16_t caid, uint32_t cardid,
   uint8_t *ua, int pcount, uint8_t **pid, uint8_t **psa);
void cc_emm_set_allowed(cclient_t *cc, int emm_allowed);
void cc_remove_card(cclient_t *cc, cc_card_data_t *pcard);
void cc_remove_card_by_id(cclient_t *cc, uint32_t card_id);

void cc_ecm_reply
  (cc_service_t *ct, cc_ecm_section_t *es, int key_type,
   uint8_t *key_even, uint8_t *key_odd, int seq);

cc_ecm_section_t *cc_find_pending_section
  (cclient_t *cc, uint32_t seq, cc_service_t **ct);

void cc_write_message(cclient_t *cc, cc_message_t *msg, int enq);
int cc_read(cclient_t *cc, void *buf, size_t len, int timeout);

void cc_service_start(caclient_t *cac, service_t *t);
void cc_free(caclient_t *cac);
void cc_caid_update(caclient_t *cac, mpegts_mux_t *mux, uint16_t caid, uint32_t provid, uint16_t pid, int valid);
void cc_conf_changed(caclient_t *cac);
