/*
 *  Elementary stream functions
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

#ifndef PARSERS_H
#define PARSERS_H

#include "tvheadend.h"
#include "sbuf.h"
#include "streaming.h"
#include "subscriptions.h"
#include "packet.h"

typedef struct parser_es parser_es_t;
typedef struct parser parser_t;

struct th_subscription;

typedef void (parse_callback_t)
  (parser_t *t, parser_es_t *st, const uint8_t *data, int len, int start);

/* parser elementary stream */
struct parser_es {
  elementary_stream_t;
  /* Parent */
  parser_t *es_parser;
  /* State */
  parse_callback_t *es_parse_callback;
  sbuf_t    es_buf;
  uint8_t   es_incomplete;
  uint8_t   es_header_mode;
  uint32_t  es_header_offset;
  uint32_t  es_startcond;
  uint32_t  es_startcode;
  uint32_t  es_startcode_offset;
  int       es_parser_state;
  int       es_parser_ptr;
  int       es_meta_change;
  void     *es_priv;          /* Parser private data */
  sbuf_t    es_buf_a;         /* Audio packet reassembly */
  uint8_t  *es_global_data;
  int       es_global_data_len;
  struct th_pkt *es_curpkt;
  struct streaming_message_queue es_backlog;
  tvhlog_limit_t es_pes_log;
  mpegts_psi_table_t es_psi;
  /* Clocks */
  int64_t   es_curpts;
  int64_t   es_curdts;
  int64_t   es_prevdts;
  int64_t   es_nextdts;
  /* Misc */
  char      es_blank;         /* Teletext: last subtitle was blank */
  /* PCR clocks */
  int64_t   es_last_pcr;
  int64_t   es_last_pcr_dts;
  tvhlog_limit_t es_pcr_log;
};

/* parser internal structure */
struct parser {

  streaming_target_t prs_input;

  streaming_target_t *prs_output;

  th_subscription_t *prs_subscription;

  service_t *prs_service;

  /* Elementary streams */
  elementary_set_t prs_components;

  /* Globals */
  uint16_t prs_pcr_pid;

  /* PCR clocks */
  int64_t  prs_current_pcr;
  int64_t  prs_candidate_pcr;
  int64_t  prs_pcr_boundary;
  uint8_t  prs_current_pcr_guess;

  /* Teletext */
  commercial_advice_t prs_tt_commercial_advice;
  time_t prs_tt_clock;   /* Network clock as determined by teletext decoder */

  /* restart_pending log */
  struct streaming_message_queue prs_rstlog;
};

static inline int64_t
getpts(const uint8_t *p)
{
  int a =  p[0];
  int b = (p[1] << 8) | p[2];
  int c = (p[3] << 8) | p[4];

  if ((a & 1) && (b & 1) && (c & 1)) {

    return
      ((int64_t)((a >> 1) & 0x07) << 30) |
      ((int64_t)((b >> 1)       ) << 15) |
      ((int64_t)((c >> 1)       ))
      ;

  } else {
    // Marker bits not present
    return PTS_UNSET;
  }
}

streaming_target_t * parser_create(streaming_target_t *output, struct th_subscription *ts);

void parser_destroy(streaming_target_t *pad);

streaming_target_t * parser_output(streaming_target_t *pad);

void parse_mpeg_ts(parser_t *t, parser_es_t *st, const uint8_t *data,
                   int len, int start, int err);

void parser_enqueue_packet(struct service *t, parser_es_t *st, th_pkt_t *pkt);

void parser_set_stream_vparam(parser_es_t *st, int width, int height, int duration);

extern const unsigned int mpeg2video_framedurations[16];

#endif /* PARSERS_H */
