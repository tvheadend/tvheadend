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

#ifndef __TVH_DESCRAMBLER_H__
#define __TVH_DESCRAMBLER_H__

#include <stdint.h>
#include <stdlib.h>
#include "queue.h"

struct service;
struct elementary_stream;
struct tvhcsa;
struct mpegts_table;
struct mpegts_mux;

#define DESCRAMBLER_NONE 0
#define DESCRAMBLER_DES  1
#define DESCRAMBLER_AES  2

/**
 * Descrambler superclass
 *
 * Created/Destroyed on per-transport basis upon transport start/stop
 */
typedef struct th_descrambler {
  LIST_ENTRY(th_descrambler) td_service_link;

  char *td_nicename;

  enum {
    DS_UNKNOWN,
    DS_RESOLVED,
    DS_FORBIDDEN,
    DS_IDLE
  } td_keystate;

  struct service *td_service;
  struct tvhcsa  *td_csa;

  void (*td_stop)       (struct th_descrambler *d);
  void (*td_caid_change)(struct th_descrambler *d);
  int  (*td_ecm_reset)  (struct th_descrambler *d);

} th_descrambler_t;

typedef struct th_descrambler_runtime {
  time_t   dr_last_descramble;
  uint32_t dr_key:1;
  uint32_t dr_key_first:1;
  uint8_t  dr_key_index;
  uint8_t  dr_key_valid;
  uint8_t  dr_ecm_valid;
  time_t   dr_key_start;
  time_t   dr_key_timestamp[2];
  time_t   dr_ecm_start;
  time_t   dr_ecm_key_time;
  sbuf_t   dr_buf;
  loglimiter_t dr_loglimit_key;
} th_descrambler_runtime_t;

typedef void (*descrambler_section_callback_t)
  (void *opaque, int pid, const uint8_t *section, int section_len);

/**
 * Track required PIDs
 */
typedef struct descrambler_section {
  TAILQ_ENTRY(descrambler_section) link;
  descrambler_section_callback_t callback;
  void     *opaque;
  uint8_t  *last_data;
  int       last_data_len;
} descrambler_section_t;

typedef struct descrambler_table {
  TAILQ_ENTRY(descrambler_table) link;
  struct mpegts_table *table;
  TAILQ_HEAD(,descrambler_section) sections;
} descrambler_table_t;

/**
 * List of CA ids
 */
#define CAID_REMOVE_ME ((uint16_t)-1)

typedef struct caid {
  LIST_ENTRY(caid) link;

  uint16_t pid;
  uint16_t caid;
  uint32_t providerid;
  uint8_t  use;
  uint8_t  filter;

} caid_t;

/**
 * List of EMM subscribers
 */
#define EMM_PID_UNKNOWN ((uint16_t)-1)

typedef struct descrambler_emm {
  TAILQ_ENTRY(descrambler_emm) link;

  uint16_t caid;
  uint16_t pid;
  unsigned int to_be_removed:1;

  descrambler_section_callback_t callback;
  void *opaque;
} descrambler_emm_t;

/**
 * cards for which emm updates are handled
 */
typedef enum {
  CARD_IRDETO,
  CARD_DRE,
  CARD_CONAX,
  CARD_SECA,
  CARD_VIACCESS,
  CARD_NAGRA,
  CARD_NDS,
  CARD_CRYPTOWORKS,
  CARD_BULCRYPT,
  CARD_UNKNOWN
} card_type_t;

LIST_HEAD(caid_list, caid);

#define DESCRAMBLER_ECM_PID(pid) ((pid) | (MT_FAST << 16))

void descrambler_init          ( void );
void descrambler_done          ( void );
void descrambler_service_start ( struct service *t );
void descrambler_service_stop  ( struct service *t );
void descrambler_caid_changed  ( struct service *t );
void descrambler_keys          ( th_descrambler_t *t, int type,
                                 const uint8_t *even, const uint8_t *odd );
int  descrambler_descramble    ( struct service *t,
                                 struct elementary_stream *st,
                                 const uint8_t *tsb );
int  descrambler_open_pid      ( struct mpegts_mux *mux, void *opaque, int pid,
                                 descrambler_section_callback_t callback,
                                 struct service *service );
int  descrambler_close_pid     ( struct mpegts_mux *mux, void *opaque, int pid );
void descrambler_flush_tables  ( struct mpegts_mux *mux );
void descrambler_cat_data      ( struct mpegts_mux *mux, const uint8_t *data, int len );
int  descrambler_open_emm      ( struct mpegts_mux *mux, void *opaque, int caid,
                                 descrambler_section_callback_t callback );
int  descrambler_close_emm     ( struct mpegts_mux *mux, void *opaque, int caid );

const char *descrambler_caid2name( uint16_t caid );
uint16_t descrambler_name2caid ( const char *str );

card_type_t detect_card_type   ( const uint16_t caid );

#endif /* __TVH_DESCRAMBLER_H__ */

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
