/*
 *  tvheadend, RPC interface
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

#ifndef RPC_H_
#define RPC_H_

#include <libhts/htsmsg.h>

typedef struct rpc_session {

  const char *rs_logname;

  uint32_t rs_seq;

  unsigned int rs_authlevel;
  unsigned int rs_maxweight;
  unsigned int rs_is_async;

  struct th_subscription_list rs_subscriptions;

} rpc_session_t;


typedef struct rpc_cmd {
  const char *rc_name;
  htsmsg_t *(*rc_func)(rpc_session_t *ses, htsmsg_t *in, void *opaque);
  int rc_authlevel;
} rpc_cmd_t;



void rpc_init(rpc_session_t *ses, const char *logname);

void rpc_deinit(rpc_session_t *ses);

htsmsg_t *rpc_dispatch(rpc_session_t *ses, htsmsg_t *in, rpc_cmd_t *cmds,
		       void *opaque);

htsmsg_t *rpc_ok(rpc_session_t *ses);
htsmsg_t *rpc_error(rpc_session_t *ses, const char *err);


#endif /* RPC_H_ */
