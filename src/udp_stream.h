/*
 *  tvheadend, UDP stream interface
 *  Copyright (C) 2019 Stephane Duperron
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

#ifndef UDP_STREAM_H_
#define UDP_STREAM_H_

#include "udp.h"
#include "subscriptions.h"
#include "profile.h"

typedef struct udp_stream {
  LIST_ENTRY(udp_stream) us_link;
  char *us_hint;
  char *us_udp_url;
  udp_connection_t *us_uc;
  profile_chain_t us_prch;
  th_subscription_t *us_subscript;
  char *us_content_name;
  pthread_t us_tid;
  int us_running;
  tvh_mutex_t* us_global_lock;
} udp_stream_t;

LIST_HEAD(udp_stream_list, udp_stream);

udp_stream_t *
create_udp_stream ( udp_connection_t *uc, const char *hint );

void
delete_udp_stream (udp_stream_t * ustream);

udp_stream_t *
find_udp_stream_by_hint ( const char *hint );

udp_stream_t *
find_udp_stream_by_url ( const char *url );

int
udp_stream_run (udp_stream_t *us);

int
udp_stream_shutdown (udp_stream_t *us);

#endif /* UDP_STREAM_H_ */
