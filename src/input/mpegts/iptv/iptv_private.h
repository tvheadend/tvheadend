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

#define IPTV_KILL_KILL   0
#define IPTV_KILL_TERM   1
#define IPTV_KILL_INT    2
#define IPTV_KILL_HUP    3
#define IPTV_KILL_USR1   4
#define IPTV_KILL_USR2   5

struct bouquet;

extern pthread_mutex_t iptv_lock;

typedef struct iptv_input   iptv_input_t;
typedef struct iptv_network iptv_network_t;
typedef struct iptv_mux     iptv_mux_t;
typedef struct iptv_service iptv_service_t;
typedef struct iptv_handler iptv_handler_t;

struct iptv_handler
{
  const char *scheme;

  uint32_t buffer_limit;

  int     (*start) ( iptv_mux_t *im, const char *raw, const url_t *url );
  void    (*stop)  ( iptv_mux_t *im );
  ssize_t (*read)  ( iptv_mux_t *im );
  void    (*pause) ( iptv_mux_t *im, int pause );
  
  RB_ENTRY(iptv_handler) link;
};

void iptv_handler_register ( iptv_handler_t *ih, int num );

struct iptv_input
{
  mpegts_input_t;
};

int  iptv_input_fd_started ( iptv_mux_t *im );
void iptv_input_mux_started ( iptv_mux_t *im );
int  iptv_input_recv_packets ( iptv_mux_t *im, ssize_t len );
void iptv_input_recv_flush ( iptv_mux_t *im );
void iptv_input_pause_handler ( iptv_mux_t *im, int pause );

struct iptv_network
{
  mpegts_network_t;

  int in_bps;
  int in_bw_limited;

  int in_scan_create;
  int in_priority;
  int in_streaming_priority;
  int in_remove_scrambled_bits;

  uint16_t in_service_id;

  uint32_t in_max_streams;
  uint32_t in_max_bandwidth;
  uint32_t in_max_timeout;

  char    *in_url;
  char    *in_url_sane;
  int      in_bouquet;
  mtimer_t in_bouquet_timer;
  char    *in_ctx_charset;
  int64_t  in_channel_number;
  uint32_t in_refetch_period;
  char    *in_icon_url;
  char    *in_icon_url_sane;
  int      in_ssl_peer_verify;
  char    *in_remove_args;
  int      in_tsid_accept_zero_value;

  void    *in_auto; /* private structure for auto-network */
};

iptv_network_t *iptv_network_create0 ( const char *uuid, htsmsg_t *conf, const idclass_t *idc );

struct iptv_mux
{
  mpegts_mux_t;

  int                   mm_iptv_priority;
  int                   mm_iptv_streaming_priority;
  int                   mm_iptv_fd;
  udp_connection_t     *mm_iptv_connection;
  int                   mm_iptv_fd2;
  udp_connection_t     *mm_iptv_connection2;
  char                 *mm_iptv_url;
  char                 *mm_iptv_url_sane;
  char                 *mm_iptv_url_raw;
  char                 *mm_iptv_interface;

  int                   mm_iptv_substitute;
  int                   mm_iptv_atsc;

  char                 *mm_iptv_muxname;
  char                 *mm_iptv_svcname;
  int64_t               mm_iptv_chnum;
  char                 *mm_iptv_icon;
  char                 *mm_iptv_epgid;

  int                   mm_iptv_respawn;
  time_t                mm_iptv_respawn_last;
  int                   mm_iptv_kill;
  int                   mm_iptv_kill_timeout;
  char                 *mm_iptv_env;
  char                 *mm_iptv_hdr;
  char                 *mm_iptv_tags;
  uint32_t              mm_iptv_satip_dvbt_freq;

  uint32_t              mm_iptv_rtp_seq;

  sbuf_t                mm_iptv_buffer;

  uint32_t              mm_iptv_buffer_limit;

  iptv_handler_t       *im_handler;
  mtimer_t              im_pause_timer;

  int64_t               im_pcr;
  int64_t               im_pcr_start;
  int64_t               im_pcr_end;
  uint16_t              im_pcr_pid;

  void                 *im_data;

  int                   im_delete_flag;
};

iptv_mux_t* iptv_mux_create0
  ( iptv_network_t *in, const char *uuid, htsmsg_t *conf );

struct iptv_service
{
  mpegts_service_t;
  char * s_iptv_svcname;
};

iptv_service_t *iptv_service_create0
  ( iptv_mux_t *im, uint16_t sid, uint16_t pmt_pid,
    const char *uuid, htsmsg_t *conf );

extern const idclass_t iptv_network_class;
extern const idclass_t iptv_auto_network_class;
extern const idclass_t iptv_mux_class;

extern iptv_input_t   *iptv_input;
extern iptv_network_t *iptv_network;


void iptv_bouquet_trigger(iptv_network_t *in, int timeout);
int iptv_url_set ( char **url, char **sane_url, const char *str, int allow_file, int allow_pipe );

void iptv_mux_load_all ( void );

void iptv_auto_network_trigger( iptv_network_t *in );
void iptv_auto_network_init( iptv_network_t *in );
void iptv_auto_network_done( iptv_network_t *in );

void iptv_http_init    ( void );
void iptv_udp_init     ( void );
void iptv_rtsp_init    ( void );
void iptv_pipe_init    ( void );
void iptv_file_init    ( void );

ssize_t iptv_rtp_read ( iptv_mux_t *im, udp_multirecv_t *um,
                        void (*pkt_cb)(iptv_mux_t *im, uint8_t *buf, int len) );

void iptv_input_unpause ( void *aux );

#endif /* __IPTV_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
