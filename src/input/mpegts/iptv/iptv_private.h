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

#include "input/mpegts.h"
#include "input/mpegts/iptv.h"
#include "htsbuf.h"
#include "url.h"

#define IPTV_PKT_SIZE (300*188)

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

  // Return number of available bytes, with optional offset
  // from start of mux buffer (useful for things with wrapper
  // around TS)
  ssize_t (*read)  ( iptv_mux_t *im, size_t *off );
  
  RB_ENTRY(iptv_handler) link;
};

void iptv_handler_register ( iptv_handler_t *ih, int num );

struct iptv_input
{
  mpegts_input_t;
};

void iptv_input_mux_started ( iptv_mux_t *im );
void iptv_input_recv_packets ( iptv_mux_t *im, size_t off, size_t len );

struct iptv_network
{
  mpegts_network_t;

  int in_bps;
  int in_bw_limited;

  uint32_t in_max_streams;
  uint32_t in_max_bandwidth;
};

struct iptv_mux
{
  mpegts_mux_t;

  int                   mm_iptv_fd;
  char                 *mm_iptv_url;
  char                 *mm_iptv_interface;

  int                   mm_iptv_atsc;

  uint8_t              *mm_iptv_tsb;
  int                   mm_iptv_pos;

  iptv_handler_t       *im_handler;

  void                 *im_data;
};

iptv_mux_t* iptv_mux_create ( const char *uuid, htsmsg_t *conf );

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
