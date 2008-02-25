/*
 *  tvheadend, XBMSP interface
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef XBMSP_H_
#define XBMSP_H_

#include "tcp.h"
#include "ffmuxer.h"

/**
 * Server source messages
 */
#define XBMSP_PACKET_OK                         1
#define XBMSP_PACKET_ERROR                      2
#define XBMSP_PACKET_HANDLE                     3
#define XBMSP_PACKET_FILE_DATA                  4
#define XBMSP_PACKET_FILE_CONTENTS              5
#define XBMSP_PACKET_AUTHENTICATION_CONTINUE    6


/**
 * Client source messages
 */
#define XBMSP_PACKET_NULL                      10
#define XBMSP_PACKET_SETCWD                    11
#define XBMSP_PACKET_FILELIST_OPEN             12
#define XBMSP_PACKET_FILELIST_READ             13
#define XBMSP_PACKET_FILE_INFO                 14
#define XBMSP_PACKET_FILE_OPEN                 15
#define XBMSP_PACKET_FILE_READ                 16
#define XBMSP_PACKET_FILE_SEEK                 17
#define XBMSP_PACKET_CLOSE                     18
#define XBMSP_PACKET_CLOSE_ALL                 19
#define XBMSP_PACKET_SET_CONFIGURATION_OPTION  20
#define XBMSP_PACKET_AUTHENTICATION_INIT       21
#define XBMSP_PACKET_AUTHENTICATE              22
#define XBMSP_PACKET_UPCWD                     23


/**
 * Discovery messages
 */
#define XBMSP_PACKET_SERVER_DISCOVERY_QUERY    90
#define XBMSP_PACKET_SERVER_DISCOVERY_REPLY    91

/**
 * Error codes
 */
#define XBMSP_ERROR_OK                        0 /* Reserved */
#define XBMSP_ERROR_FAILURE                   1
#define XBMSP_ERROR_UNSUPPORTED               2
#define XBMSP_ERROR_NO_SUCH_FILE              3
#define XBMSP_ERROR_INVALID_FILE              4
#define XBMSP_ERROR_INVALID_HANDLE            5
#define XBMSP_ERROR_OPEN_FAILED               6
#define XBMSP_ERROR_TOO_MANY_OPEN_FILES       7
#define XBMSP_ERROR_TOO_LONG_READ             8
#define XBMSP_ERROR_ILLEGAL_SEEK              9
#define XBMSP_ERROR_OPTION_IS_READ_ONLY       10
#define XBMSP_ERROR_INVALID_OPTION_VALUE      11
#define XBMSP_ERROR_AUTHENTICATION_NEEDED     12
#define XBMSP_ERROR_AUTHENTICATION_FAILED     13

LIST_HEAD(xbmsp_dirhandle_list,    xbmsp_dirhandle);
LIST_HEAD(xbmsp_subscription_list, xbmsp_subscription);
TAILQ_HEAD(xbmsp_direntry_queue,   xbmsp_direntry);

/** 
 * Represents an directory entry (a file or a dir)
 */
typedef struct xbmsp_direntry {
  TAILQ_ENTRY(xbmsp_direntry) xde_link;

  const char *xde_filename;
  const char *xde_xmlmeta;

} xbmsp_direntry_t;


/** 
 * Represents an opened directory handle.
 *
 * These are created and populated upon XBMSP_PACKET_FILELIST_OPEN and
 * subsequently consumed by XBMSP_PACKET_FILELIST_READ
 */
typedef struct xbmsp_dirhandle {
  LIST_ENTRY(xbmsp_dirhandle) xdh_link;
  uint32_t xdh_handle;
  struct xbmsp_direntry_queue xdh_entries;
} xbmsp_dirhandle_t;

/** 
 * Represents an file (or stream) entry
 */
typedef struct xbmsp_subscription {
  LIST_ENTRY(xbmsp_subscription) xs_link;
  uint32_t xs_handle;
  th_subscription_t *xs_subscription;
  struct xbmsp *xs_xbmsp;

  th_ffmuxer_t xs_tffm;
  tffm_fifo_t *xs_fifo;  /* output fifo */

  size_t xs_pending_read_size;
  uint32_t xs_pending_read_msgid;

} xbmsp_subscrption_t;



/**
 * Context for an XBMSP session
 */
typedef struct xbmsp {
  tcp_session_t xbmsp_tcp_session; /* Must be first */

  LIST_ENTRY(xbmsp) xbmsp_global_link;

  enum {
    XBMSP_STATE_CLIENT_IDENTIFY = 0,
    XBMSP_STATE_1_0,
  } xbmsp_state;

  uint8_t *xbmsp_buf;
  int xbmsp_bufptr;
  int xbmsp_bufsize;
  int xbmsp_msglen;


  const char *xbmsp_username;
  struct config_head *xbmsp_user_config; 
  int xbmsp_authenticated;              /* set if authenticated */
  uint32_t xbmsp_handle_tally;

  char *xbmsp_wd;         /* Working directory */

  struct xbmsp_dirhandle_list xbmsp_dirhandles;

  struct xbmsp_subscription_list xbmsp_subscriptions;

  char xbmsp_logname[100];

} xbmsp_t;

void xbmsp_start(int port);

#endif /* XBMSP_H_ */
