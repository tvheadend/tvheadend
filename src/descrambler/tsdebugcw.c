/*
 *  tvheadend, tsdebug code word interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <ctype.h>
#include "tvheadend.h"
#include "caclient.h"
#include "service.h"
#include "input.h"

/**
 *
 */
typedef struct tsdebugcw_service {
  th_descrambler_t;

  int        tdcw_type;
  uint16_t   tdcw_pid;
  uint8_t    tdcw_key_even[16];   /* DES or AES key */
  uint8_t    tdcw_key_odd [16];   /* DES or AES key */

} tsdebugcw_service_t;

typedef struct tsdebugcw_request {
  TAILQ_ENTRY(tsdebugcw_request) link;
  tsdebugcw_service_t *ct;
} tsdebugcw_request_t;

tvh_mutex_t tsdebugcw_mutex;
TAILQ_HEAD(,tsdebugcw_request) tsdebugcw_requests;

/*
 *
 */
static int
tsdebugcw_ecm_reset(th_descrambler_t *th)
{
  return 1;
}

/**
 * s_stream_mutex is held
 */
static void
tsdebugcw_service_destroy(th_descrambler_t *td)
{
  tsdebugcw_service_t *ct = (tsdebugcw_service_t *)td;
  tsdebugcw_request_t *ctr, *ctrnext;

  tvh_mutex_lock(&tsdebugcw_mutex);
  for (ctr = TAILQ_FIRST(&tsdebugcw_requests); ctr; ctr = ctrnext) {
    ctrnext = TAILQ_NEXT(ctr, link);
    if (ctr->ct == ct) {
      TAILQ_REMOVE(&tsdebugcw_requests, ctr, link);
      free(ctr);
    }
  }
  tvh_mutex_unlock(&tsdebugcw_mutex);

  LIST_REMOVE(td, td_service_link);
  free(ct->td_nicename);
  free(ct);
}

/**
 * global_lock is held. Not that we care about that, but either way, it is.
 */
void
tsdebugcw_service_start(service_t *t)
{
  tsdebugcw_service_t *ct;
  th_descrambler_t *td;
  char buf[128];

  extern const idclass_t mpegts_service_class;
  if (!idnode_is_instance(&t->s_id, &mpegts_service_class))
    return;

  LIST_FOREACH(td, &t->s_descramblers, td_service_link)
    if (td->td_stop == tsdebugcw_service_destroy)
      break;
  if (td)
    return;

  ct                   = calloc(1, sizeof(tsdebugcw_service_t));
  td                   = (th_descrambler_t *)ct;
  snprintf(buf, sizeof(buf), "tsdebugcw");
  td->td_nicename      = strdup(buf);
  td->td_service       = t;
  td->td_stop          = tsdebugcw_service_destroy;
  td->td_ecm_reset     = tsdebugcw_ecm_reset;
  tvh_mutex_lock(&t->s_stream_mutex);
  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);
  descrambler_change_keystate((th_descrambler_t *)td, DS_READY, 0);
  tvh_mutex_unlock(&t->s_stream_mutex);
}

/*
 *
 */
void
tsdebugcw_new_keys(service_t *t, int type, uint16_t pid, uint8_t *odd, uint8_t *even)
{
  static char empty[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  th_descrambler_t *td;
  tsdebugcw_service_t *ct;
  tsdebugcw_request_t *ctr;
  int keylen = DESCRAMBLER_KEY_SIZE(type);

  LIST_FOREACH(td, &t->s_descramblers, td_service_link)
    if (td->td_stop == tsdebugcw_service_destroy)
      break;
  if (!td)
    return;
  ct = (tsdebugcw_service_t *)td;
  ct->tdcw_type = type;
  ct->tdcw_pid = pid;
  if (odd && memcmp(empty, odd, keylen))
    memcpy(ct->tdcw_key_odd, odd, keylen);
  if (even && memcmp(empty, even, keylen))
    memcpy(ct->tdcw_key_even, even, keylen);
  ctr = malloc(sizeof(*ctr));
  ctr->ct = ct;
  tvh_mutex_lock(&tsdebugcw_mutex);
  TAILQ_INSERT_TAIL(&tsdebugcw_requests, ctr, link);
  tvh_mutex_unlock(&tsdebugcw_mutex);
}

/*
 *
 */
void
tsdebugcw_go(void)
{
  tsdebugcw_request_t *ctr;
  tsdebugcw_service_t *ct;

  while (1) {
    tvh_mutex_lock(&tsdebugcw_mutex);
    ctr = TAILQ_FIRST(&tsdebugcw_requests);
    if (ctr)
      TAILQ_REMOVE(&tsdebugcw_requests, ctr, link);
    tvh_mutex_unlock(&tsdebugcw_mutex);
    if (!ctr) break;
    ct = ctr->ct;
    descrambler_keys((th_descrambler_t *)ct,
                     ct->tdcw_type, ct->tdcw_pid,
                     ct->tdcw_key_odd, ct->tdcw_key_even);
    free(ctr);
  }
}

/*
 *
 */
void
tsdebugcw_init(void)
{
  tvh_mutex_init(&tsdebugcw_mutex, NULL);
  TAILQ_INIT(&tsdebugcw_requests);
}
