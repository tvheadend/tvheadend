/*
 *  IPTV Input
 *
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

#ifndef __IPTV_PRIVATE_H__
#define __IPTV_PRIVATE_H__

#include "input.h"
#include "htsbuf.h"
#include "url.h"
#include "udp.h"

#define IPTV_BUF_SIZE    (300*188)
#define IPTV_PKTS        32
#define IPTV_PKT_PAYLOAD 1472

extern pthread_mutex_t iptv_lock;

typedef struct iptv_input   iptv_input_t;
typedef struct iptv_network iptv_network_t;
typedef struct iptv_mux     iptv_mux_t;
typedef struct iptv_service iptv_service_t;
typedef struct iptv_handler iptv_handler_t;

struct iptv_handler
{
  const char *scheme;
  int     (*start) ( iptv_mux_t *im, const url_t *url );
  void    (*stop)  ( iptv_mux_t *im );
  ssize_t (*read)  ( iptv_mux_t *im );
  
  RB_ENTRY(iptv_handler) link;
};

void iptv_handler_register ( iptv_handler_t *ih, int num );

struct iptv_input
{
  mpegts_input_t;
};

void iptv_input_mux_started ( iptv_mux_t *im );
void iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len );

struct iptv_network
{
  mpegts_network_t;

  int in_bps;
  int in_bw_limited;

  uint32_t in_max_streams;
  uint32_t in_max_bandwidth;
  uint32_t in_max_timeout;
};

iptv_network_t *iptv_network_create0 ( const char *uuid, htsmsg_t *conf );

struct iptv_mux
{
  mpegts_mux_t;

  int                   mm_iptv_fd;
  udp_connection_t     *mm_iptv_connection;
  char                 *mm_iptv_url;
  char                 *mm_iptv_url_sane;
  char                 *mm_iptv_interface;

  int                   mm_iptv_atsc;

  char                 *mm_iptv_muxname;
  char                 *mm_iptv_svcname;

  sbuf_t                mm_iptv_buffer;

  iptv_handler_t       *im_handler;

  void                 *im_data;

};

iptv_mux_t* iptv_mux_create0
  ( iptv_network_t *in, const char *uuid, htsmsg_t *conf );

struct iptv_service
{
  mpegts_service_t;
};

iptv_service_t *iptv_service_create0
  ( iptv_mux_t *im, uint16_t sid, uint16_t pmt_pid,
    const char *uuid, htsmsg_t *conf );

extern iptv_input_t   *iptv_input;
extern iptv_network_t *iptv_network;

void iptv_mux_load_all ( void );

void iptv_http_init    ( void );
void iptv_udp_init     ( void );

#endif /* __IPTV_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
