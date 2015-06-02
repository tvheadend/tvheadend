/*
 *  Tvheadend
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

#ifndef RTCP_H
#define	RTCP_H

#include "iptv_private.h"
#include "udp.h"

/*
 * Current protocol version.
 */
#define RTP_VERSION    2

#define RTP_SEQ_MOD (1<<16)
#define RTP_MAX_SDES 255      /* maximum text length for SDES */

typedef enum
{
  RTCP_SR   = 200,
  RTCP_RR   = 201,
  RTCP_SDES = 202,
  RTCP_BYE  = 203,
  RTCP_APP  = 204
} rtcp_type_t;

typedef enum
{
  RTCP_SDES_END   = 0,
  RTCP_SDES_CNAME = 1,
  RTCP_SDES_NAME  = 2,
  RTCP_SDES_EMAIL = 3,
  RTCP_SDES_PHONE = 4,
  RTCP_SDES_LOC   = 5,
  RTCP_SDES_TOOL  = 6,
  RTCP_SDES_NOTE  = 7,
  RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/*
 * RTCP common header word
 */
typedef struct
{
  unsigned int version : 2;   /* protocol version */
  unsigned int p : 1;         /* padding flag */
  unsigned int count : 5;     /* varies by packet type */
  unsigned int pt : 8;        /* RTCP packet type */
  uint16_t length;           /* pkt len in words, w/o this word */
} rtcp_common_t;


/*
 * Big-endian mask for version, padding bit and packet type pair
 */
#define RTCP_VALID_MASK (0xc000 | 0x2000 | 0xfe)
#define RTCP_VALID_VALUE ((RTP_VERSION << 14) | RTCP_SR)

/*
 * Reception report block
 */
typedef struct
{
  uint32_t ssrc;             /* data source being reported */
  unsigned int fraction : 8;  /* fraction lost since last SR/RR */

  int lost : 24;              /* cumul. no. pkts lost (signed!) */
  uint32_t last_seq;         /* extended last seq. no. received */
  uint32_t jitter;           /* interarrival jitter */
  uint32_t lsr;              /* last SR packet from this source */
  uint32_t dlsr;             /* delay since last SR packet */
} rtcp_rr_t;

/*
 * SDES item
 */
typedef struct
{
  u_int8_t type;              /* type of item (rtcp_sdes_type_t) */
  u_int8_t length;            /* length of item (in octets) */
  char data[1];             /* text, not null-terminated */
} rtcp_sdes_item_t;

typedef struct rtcp_sdes
{
  uint32_t src;      /* first SSRC/CSRC */
  rtcp_sdes_item_t item[1]; /* list of SDES items */
} rtcp_sdes_t;

/*
 * One RTCP packet
 */
typedef struct
{
  rtcp_common_t common;     /* common header */

  union
  {

    /* sender report (SR) */
    struct
    {
      uint32_t ssrc;     /* sender generating this report */
      uint32_t ntp_sec;  /* NTP timestamp */
      uint32_t ntp_frac;
      uint32_t rtp_ts;   /* RTP timestamp */
      uint32_t psent;    /* packets sent */
      uint32_t osent;    /* octets sent */
      rtcp_rr_t rr[1];  /* variable-length list */
    } sr;

    /* reception report (RR) */
    struct
    {
      uint32_t ssrc;     /* receiver generating this report */
      rtcp_rr_t rr[1];  /* variable-length list */
    } rr;

    /* source description (SDES) */
    rtcp_sdes_t sdes;

    /* BYE */
    struct
    {
      uint32_t src[1];   /* list of sources */
      /* can't express trailing text for reason */
    } bye;
  } r;
} rtcp_t;

typedef struct iptv_rtcp_info {
  /* Last transmitted packet timestamp */
  time_t last_ts;
  /* Next scheduled packet sending timestamp */
  time_t next_ts;
  
  double average_packet_size;
  
  int members;
  int senders;
  
  uint16_t last_received_sequence;
  uint16_t sequence_cycle;
  
  /* Connection to the RTCP remote. Initialized by the RTSP code. */
  udp_connection_t *connection;
  
  uint32_t source_ssrc;
  uint32_t my_ssrc;
} iptv_rtcp_info_t;

/*
 Init rtcp_info field of the rtsp_info.
 Return 0 if everything was OK.
 rtcp_destroy must be called when rtsp_info is destroyed.
 */
int rtcp_init(iptv_rtcp_info_t *info);

/*
 Destroy rtcp_info field of rtsp_info.
 */
int rtcp_destroy(iptv_rtcp_info_t *info);

/*
 Update RTCP informations.
 It can also send a RTCP RR packet if the timer has expired.
 */
int rtcp_receiver_update(iptv_rtcp_info_t *info, uint8_t *rtp_packet);
#endif	/* IPTV_RTCP_H */

