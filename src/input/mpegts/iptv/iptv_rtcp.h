/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adrien CLERC
 *
 * Tvheadend
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
  RTCP_APP  = 204,
  RTCP_GF   = 205
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
} rtcp_header_t;


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
} rtcp_rr_block_t;



/*
 * Generic RTC Feedback block
 */
typedef struct
{
  uint32_t my_ssrc;          /* not used */
  uint32_t ssrc;             /* data source being reported */
  uint16_t pid;              /* first missing frame id */
  uint16_t blp;              /* missing frame count */
} rtcp_gf_t;

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
 * reception report (RR)
 */
typedef struct
{
  uint32_t ssrc;     /* receiver generating this report */
  rtcp_rr_block_t rr[1];  /* variable-length list */
} rtcp_rr_t;

/*
 Init rtcp_info field of the rtsp_info.
 Return 0 if everything was OK.
 rtcp_destroy must be called when rtsp_info is destroyed.
 */
int rtcp_init(rtcp_t *info);

/*
 Destroy rtcp_info field of rtsp_info.
 */
int rtcp_destroy(rtcp_t *info);

/*
 Update RTCP informations.
 It can also send a RTCP RR packet if the timer has expired.
 */
int rtcp_receiver_update(rtcp_t *info, uint8_t *rtp_packet);
ssize_t rtcp_send_nak(rtcp_t *rtcp_info, uint32_t ssrc, uint16_t seqn, uint16_t len);
int rtcp_connect(rtcp_t * info, char *url, char *host, int port, char *interface, char *nicename);
#endif	/* IPTV_RTCP_H */
