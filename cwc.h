/*
 *  tvheadend, CWC interface
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef CWC_H_
#define CWC_H_

#include "tcp.h"

TAILQ_HEAD(cwc_queue, cwc);

extern struct cwc_queue cwcs;

typedef struct cwc {
  tcp_session_t cwc_tcp_session; /* Must be first */

  TAILQ_ENTRY(cwc) cwc_link;

  LIST_HEAD(, cwc_transport) cwc_transports;

  enum {
    CWC_STATE_IDLE,
    CWC_STATE_WAIT_LOGIN_KEY,
    CWC_STATE_WAIT_LOGIN_ACK,
    CWC_STATE_WAIT_CARD_DATA,
    CWC_STATE_RUNNING,
    CWC_STATE_HOST_ERROR,
    CWC_STATE_ACCESS_ERROR,
  } cwc_state;
  
  uint16_t cwc_caid;

  uint16_t cwc_seq;

  uint8_t cwc_key[16];

  uint8_t cwc_buf[256];
  int cwc_bufptr;

  /* From configuration */

  uint8_t cwc_confedkey[14];
  char *cwc_username;
  char *cwc_password;
  char *cwc_password_salted;   /* salted version */
  char *cwc_comment;

  dtimer_t cwc_idle_timer;

  dtimer_t cwc_send_ka_timer;

  char *cwc_id;

  const char *cwc_errtxt;

  int cwc_enabled;

} cwc_t;


void cwc_init(void);

void cwc_transport_start(th_transport_t *t);

const char *cwc_add(const char *hostname, const char *porttxt,
		    const char *username, const char *password, 
		    const char *deskey, const char *enabled,
		    int save, int salt);

const char *cwc_status_to_text(struct cwc *cwc);

cwc_t *cwc_find(int id);

void cwc_delete(cwc_t *cwc);

void cwc_set_enable(cwc_t *cwc, int enabled);

#endif /* CWC_H_ */
