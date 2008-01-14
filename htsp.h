/*
 *  tvheadend, HTSP interface
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

#ifndef HTSP_H_
#define HTSP_H_

#include <libhts/htsmsg_binary.h>

#include "rpc.h"
#include "tcp.h"

typedef struct htsp {
  tcp_session_t htsp_tcp_session; /* Must be first */

  LIST_ENTRY(htsp) htsp_global_link;

  int htsp_bufsize;
  int htsp_bufptr;
  int htsp_msglen;
  uint8_t *htsp_buf;

  rpc_session_t htsp_rpc;

  int htsp_async_init_sent;

} htsp_t;

void htsp_start(int port);

int htsp_send_msg(htsp_t *htsp, htsmsg_t *m, int media);

void htsp_async_channel_update(th_channel_t *ch);

#endif /* HTSP_H_ */
