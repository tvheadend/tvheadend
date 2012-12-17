/*
 *  tvheadend, CCW
 *  Copyright (C) 2012
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

#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/un.h>

#include "tvheadend.h"
#include "dvb/dvb.h"
#include "tcp.h"
#include "psi.h"
#include "tsdemux.h"
#include "ffdecsa/FFdecsa.h"
#include "ccw.h"
#include "notify.h"
#include "subscriptions.h"
#include "dtable.h"


/**
 *
 */
TAILQ_HEAD(ccw_queue, ccw);
LIST_HEAD(ccw_service_list, ccw_service);
static struct ccw_queue ccws;
static pthread_cond_t ccw_config_changed;



/**
 *
 */
typedef struct ccw_service {
  th_descrambler_t ct_head;

  service_t *ct_service;

  struct ccw *ct_ccw;

  LIST_ENTRY(ccw_service) ct_link;

  /**
   * Status of the key(s) in ct_keys
   */
  enum {
    CT_UNKNOWN,
    CT_RESOLVED,
    CT_FORBIDDEN
  } ct_keystate;

  /* buffer for keystruct */
  void    *ct_keys;

  /* CSA */
  int      ct_cluster_size;
  uint8_t *ct_tsbcluster;
  int      ct_fill;

  uint8_t ccw_evenkey[8];
  uint8_t ccw_oddkey[8];

} ccw_service_t;


/**
 *
 */
typedef struct ccw {
  pthread_cond_t ccw_cond;

  TAILQ_ENTRY(ccw) ccw_link; /* Linkage protected via global_lock */

  struct ccw_service_list ccw_services;

  uint16_t ccw_caid;  // CAID
  uint16_t ccw_tid;   // Transponder ID
  uint16_t ccw_sid;   // Channel ID
  uint8_t ccw_confedkey[8];  // Key
  char *ccw_comment;
  char *ccw_id;

  int   ccw_enabled;
  int   ccw_running;
  int   ccw_reconfigure;

} ccw_t;


/**
 * global_lock is held
 * s_stream_mutex is held
 */
static void 
ccw_service_destroy(th_descrambler_t *td)
{
  tvhlog(LOG_INFO, "ccw", "Removing CCW key from service");

  ccw_service_t *ct = (ccw_service_t *)td;

  LIST_REMOVE(td, td_service_link);
  LIST_REMOVE(ct, ct_link);

  free_key_struct(ct->ct_keys);
  free(ct->ct_tsbcluster);
  free(ct);
}


/**
 *
 */
static void
ccw_table_input(struct th_descrambler *td, struct service *t,
    struct elementary_stream *st, const uint8_t *data, int len)
{

// CCW work without ECM, so never enter here

  tvhlog(LOG_INFO, "ccw",
            "ECM incoming for service \"%s\"",t->s_svcname);
}


/**
 *
 */
static int
ccw_descramble(th_descrambler_t *td, service_t *t, struct elementary_stream *st,
     const uint8_t *tsb)
{
  ccw_service_t *ct = (ccw_service_t *)td;
  int r, i;
  unsigned char *vec[3];
  uint8_t *t0;

  if(ct->ct_keystate == CT_FORBIDDEN)
    return 1;

  if(ct->ct_keystate != CT_RESOLVED)
    return -1;

  memcpy(ct->ct_tsbcluster + ct->ct_fill * 188, tsb, 188);
  ct->ct_fill++;

  if(ct->ct_fill != ct->ct_cluster_size)
    return 0;

  ct->ct_fill = 0;

  vec[0] = ct->ct_tsbcluster;
  vec[1] = ct->ct_tsbcluster + ct->ct_cluster_size * 188;
  vec[2] = NULL;

  while(1) {
    t0 = vec[0];
    r = decrypt_packets(ct->ct_keys, vec);
    if(r == 0)
      break;
    for(i = 0; i < r; i++) {
      ts_recv_packet2(t, t0);
      t0 += 188;
    }
  }
  return 0;
}


/**
 *
 */
static inline elementary_stream_t *
ccw_find_stream_by_caid(service_t *t, int caid)
{
  elementary_stream_t *st;
  caid_t *c;

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    LIST_FOREACH(c, &st->es_caids, link) {
      if(c->caid == caid)
	return st;
    }
  }
  return NULL;
}



/**
 * Check if our CAID's matches, and if so, link
 *
 * global_lock is held
 */
void
ccw_service_start(service_t *t)
{
  ccw_t *ccw;
  ccw_service_t *ct;
  th_descrambler_t *td;

  lock_assert(&global_lock);

  TAILQ_FOREACH(ccw, &ccws, ccw_link) {
    if(ccw->ccw_caid == 0)
      continue;

    if(ccw_find_stream_by_caid(t, ccw->ccw_caid) == NULL)
      continue;

    if(t->s_dvb_service_id != ccw->ccw_sid)
      continue;

    if(t->s_dvb_mux_instance->tdmi_transport_stream_id != ccw->ccw_tid)
      continue;

    tvhlog(LOG_INFO, "ccw",
      "Starting ccw key for service \"%s\" on tuner %d", 
      t->s_svcname,
      t->s_dvb_mux_instance->tdmi_adapter->tda_adapter_num);


    /* create new ccw service */
    ct                  = calloc(1, sizeof(ccw_service_t));
    ct->ct_cluster_size = get_suggested_cluster_size();
    ct->ct_tsbcluster   = malloc(ct->ct_cluster_size * 188);

    ct->ct_keys       = get_key_struct();
    ct->ct_ccw      = ccw;
    ct->ct_service  = t;

    memcpy(ct->ccw_evenkey,ccw->ccw_confedkey,8);
    memcpy(ct->ccw_oddkey,ccw->ccw_confedkey,8);

    set_even_control_word(ct->ct_keys, ct->ccw_evenkey);
    set_odd_control_word(ct->ct_keys, ct->ccw_oddkey);
    if(ct->ct_keystate != CT_RESOLVED)
        tvhlog(LOG_INFO, "ccw", "Obtained key for service \"%s\"",t->s_svcname);

    tvhlog(LOG_DEBUG, "ccw",
	   "Key for service \"%s\" "
	   "even: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
	   " odd: %02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
	   t->s_svcname,
	   ct->ccw_evenkey[0], ct->ccw_evenkey[1], ct->ccw_evenkey[2], ct->ccw_evenkey[3],
           ct->ccw_evenkey[4], ct->ccw_evenkey[5], ct->ccw_evenkey[6], ct->ccw_evenkey[7],
           ct->ccw_oddkey[0], ct->ccw_oddkey[1], ct->ccw_oddkey[2], ct->ccw_oddkey[3],
           ct->ccw_oddkey[4], ct->ccw_oddkey[5], ct->ccw_oddkey[6],  ct->ccw_oddkey[7]);
    ct->ct_keystate = CT_RESOLVED;

    td = &ct->ct_head;
    td->td_stop       = ccw_service_destroy;
    td->td_table      = ccw_table_input;
    td->td_descramble = ccw_descramble;
    LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

    LIST_INSERT_HEAD(&ccw->ccw_services, ct, ct_link);
  }
}


/**
 *
 */
static void
ccw_destroy(ccw_t *ccw)
{
  lock_assert(&global_lock);
  TAILQ_REMOVE(&ccws, ccw, ccw_link);
  ccw->ccw_running = 0;
  pthread_cond_signal(&ccw->ccw_cond);
}

/**
 *
 */
static ccw_t *
ccw_entry_find(const char *id, int create)
{
  char buf[20];
  ccw_t *ccw;
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(ccw, &ccws, ccw_link)
      if(!strcmp(ccw->ccw_id, id))
  return ccw;
  }
  if(create == 0)
    return NULL;

  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }

  ccw = calloc(1, sizeof(ccw_t));
  pthread_cond_init(&ccw->ccw_cond, NULL);
  ccw->ccw_id      = strdup(id);
  ccw->ccw_running = 1;

  TAILQ_INSERT_TAIL(&ccws, ccw, ccw_link);

  return ccw;
}


/**
 *
 */
static htsmsg_t *
ccw_record_build(ccw_t *ccw)
{
  htsmsg_t *e = htsmsg_create_map();
  char buf[100];

  htsmsg_add_str(e, "id", ccw->ccw_id);
  htsmsg_add_u32(e, "enabled",  !!ccw->ccw_enabled);

  htsmsg_add_u32(e, "caid", ccw->ccw_caid);
  htsmsg_add_u32(e, "tid", ccw->ccw_tid);
  htsmsg_add_u32(e, "sid", ccw->ccw_sid);

  snprintf(buf, sizeof(buf),
	   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	   ccw->ccw_confedkey[0],
	   ccw->ccw_confedkey[1],
	   ccw->ccw_confedkey[2],
	   ccw->ccw_confedkey[3],
	   ccw->ccw_confedkey[4],
	   ccw->ccw_confedkey[5],
	   ccw->ccw_confedkey[6],
	   ccw->ccw_confedkey[7]);

  htsmsg_add_str(e, "key", buf);
  htsmsg_add_str(e, "comment", ccw->ccw_comment ?: "");

  return e;
}


/**
 *
 */
static int
nibble(char c)
{
  switch(c) {
  case '0' ... '9':
    return c - '0';
  case 'a' ... 'f':
    return c - 'a' + 10;
  case 'A' ... 'F':
    return c - 'A' + 10;
  default:
    return 0;
  }
}

/**
 *
 */
static htsmsg_t *
ccw_entry_update(void *opaque, const char *id, htsmsg_t *values, int maycreate)
{
  ccw_t *ccw;
  const char *s;
  uint32_t u32;
  uint8_t key[8];
  int u, l, i;

  if((ccw = ccw_entry_find(id, maycreate)) == NULL)
    return NULL;

  lock_assert(&global_lock);

  if(!htsmsg_get_u32(values, "caid", &u32))
    ccw->ccw_caid = u32;
  if(!htsmsg_get_u32(values, "tid", &u32))
    ccw->ccw_tid = u32;
  if(!htsmsg_get_u32(values, "sid", &u32))
    ccw->ccw_sid = u32;

  if((s = htsmsg_get_str(values, "key")) != NULL) {
    for(i = 0; i < 8; i++) {
      while(*s != 0 && !isxdigit(*s)) s++;
      if(*s == 0)
	break;
      u = nibble(*s++);
      while(*s != 0 && !isxdigit(*s)) s++;
      if(*s == 0)
	break;
      l = nibble(*s++);
      key[i] = (u << 4) | l;
    }
    memcpy(ccw->ccw_confedkey, key, 8);
  }


  if((s = htsmsg_get_str(values, "comment")) != NULL) {
    free(ccw->ccw_comment);
    ccw->ccw_comment = strdup(s);
  }

  if(!htsmsg_get_u32(values, "enabled", &u32)) 
    ccw->ccw_enabled = u32;


  ccw->ccw_reconfigure = 1;

  pthread_cond_signal(&ccw->ccw_cond);

  pthread_cond_broadcast(&ccw_config_changed);

  return ccw_record_build(ccw);
}




/**
 *
 */
static int
ccw_entry_delete(void *opaque, const char *id)
{
  ccw_t *ccw;

  pthread_cond_broadcast(&ccw_config_changed);

  if((ccw = ccw_entry_find(id, 0)) == NULL)
    return -1;
  ccw_destroy(ccw);
  return 0;
}



/**
 *
 */
static htsmsg_t *
ccw_entry_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  ccw_t *ccw;

  TAILQ_FOREACH(ccw, &ccws, ccw_link)
    htsmsg_add_msg(r, NULL, ccw_record_build(ccw));

  return r;
}


/**
 *
 */
static htsmsg_t *
ccw_entry_get(void *opaque, const char *id)
{
  ccw_t *ccw;


  if((ccw = ccw_entry_find(id, 0)) == NULL)
    return NULL;
  return ccw_record_build(ccw);
}


/**
 *
 */
static htsmsg_t *
ccw_entry_create(void *opaque)
{
  pthread_cond_broadcast(&ccw_config_changed);
  return ccw_record_build(ccw_entry_find(NULL, 1));
}


/**
 *
 */
static const dtable_class_t ccw_dtc = {
  .dtc_record_get     = ccw_entry_get,
  .dtc_record_get_all = ccw_entry_get_all,
  .dtc_record_create  = ccw_entry_create,
  .dtc_record_update  = ccw_entry_update,
  .dtc_record_delete  = ccw_entry_delete,
  .dtc_read_access = ACCESS_ADMIN,
  .dtc_write_access = ACCESS_ADMIN,
  .dtc_mutex = &global_lock,
};


/**
 *
 */
void
ccw_init(void)
{
  dtable_t *dt;

  TAILQ_INIT(&ccws);

  pthread_cond_init(&ccw_config_changed, NULL);

  dt = dtable_create(&ccw_dtc, "ccw", NULL);
  dtable_load(dt);

}

