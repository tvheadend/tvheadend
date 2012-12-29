/*
 *  RTSP IPTV Input
 *  Copyright (C) 2012 Adrien CLERC
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

#ifndef IPTV_INPUT_RTSP_H
#define	IPTV_INPUT_RTSP_H

#include <curl/curl.h>

/*
 Main RTSP storage that'll go in service.
 */
typedef struct iptv_rtsp_info {
  /*
   The cURL handle. Opened once at service start and close at service stop.
   */
  CURL *curl;
  
  /*
   The RTSP uri, taken from the configuration.
   */
  const char *uri;
  
  /*
   Set to true if fully initialized. Used for internal state, and to know what to destroy.
   */
  int is_initialized;
  
  /*
   Our bound address for receiving RTSP packets.
   */
  struct addrinfo *client_addr;
  
  /*
   The even client port (for RTCP, use this port + 1).
   */
  int client_port;
  
  /*
   The even server port (for RTCP, use this port + 1).
   */
  int server_port;
  
  /*
   RTCP container.
   */
  struct iptv_rtcp_info *rtcp_info;
} iptv_rtsp_info_t;

/*
 Called when the RTSP stream should start.
 The file descriptor fd will be filled with the socket number that will receive RTSP packets.
 */
iptv_rtsp_info_t *iptv_rtsp_start(const char *uri, int *fd);

/*
 Stop RTSP stream. Everything is destroyed after that.
 */
void iptv_rtsp_stop(iptv_rtsp_info_t *);

/*
 Should be called at program startup.
 */
int iptv_init_rtsp(void);
#endif	/* IPTV_RTSP_INPUT_H */

