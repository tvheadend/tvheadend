/*
 *  RTSP IPTV Input
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

#ifndef IPTV_INPUT_RTSP_H
#define	IPTV_INPUT_RTSP_H

#include <curl/curl.h>


typedef struct iptv_rtsp_info {
  CURL *curl;
  const char *uri;
  int is_initialized;
  struct addrinfo *client_addr;
  int client_port;
  int server_port;
  struct iptv_rtcp_info *rtcp_info;
} iptv_rtsp_info_t;

iptv_rtsp_info_t *iptv_rtsp_start(const char *uri, int *fd);

void iptv_rtsp_stop(iptv_rtsp_info_t *);

int iptv_init_rtsp(void);
#endif	/* IPTV_RTSP_INPUT_H */

