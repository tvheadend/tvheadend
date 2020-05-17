/*
 *  Packet parsing functions - streaming message handler
 *  Copyright (C) 2017 Jaroslav Kysela
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

#include "parsers.h"
#include "../input/mpegts/dvb_psi_hbbtv.h"
#include "packet.h"
#include "streaming.h"
#include "config.h"
#include "htsmsg_binary2.h"

/**
 *
 */
static inline parser_es_t *
parser_find_es(parser_t *prs, int pid)
{
  elementary_stream_t *es;

  TAILQ_FOREACH(es, &prs->prs_components.set_all, es_link)
    if (es->es_pid == pid)
      return (parser_es_t *)es;
  return NULL;
}

/**
 * Extract PCR clocks
 */
static void ts_recv_pcr(parser_t *t, const uint8_t *tsb)
{
  int64_t pcr;
  pcr  =  (uint64_t)tsb[6] << 25;
  pcr |=  (uint64_t)tsb[7] << 17;
  pcr |=  (uint64_t)tsb[8] << 9;
  pcr |=  (uint64_t)tsb[9] << 1;
  pcr |= ((uint64_t)tsb[10] >> 7) & 0x01;
  /* handle the broken info using candidate variable */
  if (t->prs_current_pcr == PTS_UNSET || t->prs_current_pcr_guess ||
      pts_diff(t->prs_current_pcr, pcr) <= (int64_t)t->prs_pcr_boundary ||
      (t->prs_candidate_pcr != PTS_UNSET &&
       pts_diff(t->prs_candidate_pcr, pcr) <= (int64_t)t->prs_pcr_boundary)) {
    if (pcr != t->prs_current_pcr) {
      if (t->prs_current_pcr == PTS_UNSET)
        tvhtrace(LS_PCR, "%s: initial  : %"PRId64, service_nicename(t->prs_service), pcr);
      else
        tvhtrace(LS_PCR, "%s: change   : %"PRId64"%s", service_nicename(t->prs_service), pcr,
                         t->prs_candidate_pcr != PTS_UNSET ? " (candidate)" : "");
      t->prs_current_pcr = pcr;
      t->prs_current_pcr_guess = 0;
    }
    t->prs_candidate_pcr = PTS_UNSET;
  } else {
    tvhtrace(LS_PCR, "%s: candidate: %"PRId64, service_nicename(t->prs_service), pcr);
    t->prs_candidate_pcr = pcr;
  }
}

/*
 * Extract PCR from audio
 */
static void ts_recv_pcr_audio
  (parser_t *t, parser_es_t *st, const uint8_t *buf, int len)
{
  int64_t dts, pts, d;
  int hdr, flags, hlen;

  if (buf[3] != 0xbd && buf[3] != 0xc0)
    return;

  if (len < 9)
    return;

  buf  += 6;
  len  -= 6;
  hdr   = buf[0];
  flags = buf[1];
  hlen  = buf[2];
  buf  += 3;
  len  -= 3;

  if (len < hlen || (hdr & 0xc0) != 0x80)
    return;

  if ((flags & 0xc0) == 0xc0) {
    if (hlen < 10)
      return;

    pts = getpts(buf);
    dts = getpts(buf + 5);

    d = (pts - dts) & PTS_MASK;
    if (d > 180000) // More than two seconds of PTS/DTS delta, probably corrupt
      return;

  } else if ((flags & 0xc0) == 0x80) {
    if (hlen < 5)
      return;

    dts = pts = getpts(buf);
  } else
    return;

  if (st->es_last_pcr == PTS_UNSET) {
    d = pts_diff(st->es_last_pcr_dts, dts);
    goto set;
  }

  if (st->es_last_pcr != t->prs_current_pcr) {
    st->es_last_pcr = t->prs_current_pcr;
    st->es_last_pcr_dts = dts;
    return;
  }

  d = pts_diff(st->es_last_pcr_dts, dts);
  if (d == PTS_UNSET || d < 30000)
    return;
  if (d < 10*90000)
    t->prs_current_pcr += d;

set:
  if (t->prs_current_pcr == PTS_UNSET && d != PTS_UNSET && d < 30000) {
    t->prs_current_pcr = (dts - 2*90000) & PTS_MASK;
    t->prs_current_pcr_guess = 1;
  }

  tvhtrace(LS_PCR, "%s: audio DTS: %"PRId64" dts %"PRId64" [%s/%d]",
                   service_nicename(t->prs_service), t->prs_current_pcr, dts,
                   streaming_component_type2txt(st->es_type), st->es_pid);
  st->es_last_pcr = t->prs_current_pcr;
  st->es_last_pcr_dts = dts;
}

/**
 * HBBTV
 */
static void
ts_recv_hbbtv1_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  parser_es_t *pes = (parser_es_t *)mt->mt_opaque;
  parser_t *prs = pes->es_parser;
  htsmsg_t *apps = dvb_psi_parse_hbbtv(mt, buf, len, NULL);

  if (apps) {
    void *bin;
    size_t binlen;
    parse_mpeg_ts(prs, pes, buf, len, 1, 0);
    if (!htsmsg_binary2_serialize(apps, &bin, &binlen, 128*1024)) {
      parse_mpeg_ts(prs, pes, bin, binlen, 1, 0);
      free(bin);
    }
  }
}

/**
 *
 */
static void parser_input_mpegts(parser_t *prs, pktbuf_t *pb)
{
  const uint8_t *tsb = pb->pb_data;
  size_t len = pb->pb_size;
  int err = pb->pb_err;
  int pid, last_pid = -1;
  int pusi, error, off;
  parser_es_t *pes = NULL;

  for (; len > 0; tsb += 188, len -= 188) {
    pid = (tsb[1] & 0x1f) << 8 | tsb[2];
    if (pid != last_pid) {
      pes = parser_find_es(prs, pid);
      last_pid = pid;
      if (err && pes) {
        /* FIXME: we have global error count */
        /*        assign errors more precisely */
        sbuf_err(&pes->es_buf, err);
        err = 0;
      }
    }
    if (pes == NULL)
      continue;

    pusi  = (tsb[1] >> 6) & 1; /* 0x40 */
    error = (tsb[1] >> 7) & 1; /* 0x80 */

    if (tsb[3] & 0x20) {
      off = tsb[4] + 5;
      if (pes->es_pid == prs->prs_pcr_pid && !error && off > 10 &&
          (tsb[5] & 0x10) != 0 && off <= 188)
        ts_recv_pcr(prs, tsb);
    } else {
      off = 4;
    }

    if (pusi && !error && off < 188 - 16 &&
        tsb[off] == 0 && tsb[off+1] == 0 && tsb[off+2] == 1 &&
        SCT_ISAUDIO(pes->es_type)) {
      ts_recv_pcr_audio(prs, pes, tsb + off, 188 - off);
    } else if (pes->es_type == SCT_HBBTV) {
      dvb_table_parse(&pes->es_psi, "parser", tsb, 188, 1, 0, ts_recv_hbbtv1_cb);
      continue;
    }

    if (off <= 188)
      parse_mpeg_ts(prs, pes, tsb + off, 188 - off, pusi, error);
  }
}

/**
 *
 */
static void parser_init_es(parser_es_t *pes, parser_t *prs)
{
  pes->es_parser = prs;
  pes->es_parse_callback = NULL;
  pes->es_incomplete = 0;
  pes->es_header_mode = 0;
  pes->es_parser_state = 0;
  pes->es_blank = 0;
  pes->es_startcond = 0xffffffff;
  pes->es_curdts = PTS_UNSET;
  pes->es_curpts = PTS_UNSET;
  pes->es_prevdts = PTS_UNSET;
  pes->es_last_pcr = PTS_UNSET;
  pes->es_last_pcr_dts = PTS_UNSET;
  TAILQ_INIT(&pes->es_backlog);
  if (pes->es_type == SCT_HBBTV && pes->es_psi.mt_name == NULL)
    dvb_table_parse_init(&pes->es_psi, "hbbtv", LS_TS, pes->es_pid,
                         DVB_HBBTV_BASE, DVB_HBBTV_MASK, pes);
}

/**
 *
 */
static void parser_clean_es(parser_es_t *pes)
{
  free(pes->es_priv);
  pes->es_priv = NULL;

  streaming_queue_clear(&pes->es_backlog);

  if (pes->es_psi.mt_name)
    dvb_table_reset(&pes->es_psi);

  pes->es_startcode = 0;

  sbuf_free(&pes->es_buf);
  sbuf_free(&pes->es_buf_a);

  if(pes->es_curpkt != NULL) {
    pkt_ref_dec(pes->es_curpkt);
    pes->es_curpkt = NULL;
  }

  free(pes->es_global_data);
  pes->es_global_data = NULL;
  pes->es_global_data_len = 0;

  tvhlog_limit_reset(&pes->es_pes_log);
  tvhlog_limit_reset(&pes->es_pcr_log);
}

static void
parser_do_rstlog(parser_t *prs)
{
  streaming_message_t *sm;
  th_pkt_t *pkt;
  while ((sm = TAILQ_FIRST(&prs->prs_rstlog)) != NULL) {
    TAILQ_REMOVE(&prs->prs_rstlog, sm, sm_link);
    pkt = sm->sm_data;
    pkt_trace(LS_PARSER, pkt, "deliver from rstlog");
    streaming_target_deliver2(prs->prs_output, streaming_msg_create_pkt(pkt));
    sm->sm_data = NULL;
    streaming_msg_free(sm);
  }
}

/**
 *
 */
static void parser_input_start(parser_t *prs, streaming_message_t *sm)
{
  streaming_start_t *ss = sm->sm_data;
  elementary_set_t *set = &prs->prs_components;
  elementary_stream_t *es;

  TAILQ_FOREACH(es, &set->set_all, es_link)
    parser_clean_es((parser_es_t *)es);
  elementary_set_clean(set, prs->prs_service, 1);

  elementary_stream_create_from_start(set, ss, sizeof(parser_es_t));
  TAILQ_FOREACH(es, &set->set_all, es_link) {
    parser_init_es((parser_es_t *)es, prs);
    TAILQ_INSERT_TAIL(&set->set_filter, es, es_filter_link);
  }

  prs->prs_pcr_pid = ss->ss_pcr_pid;
  prs->prs_current_pcr = PTS_UNSET;
  prs->prs_candidate_pcr = PTS_UNSET;
  prs->prs_current_pcr_guess = 0;
  prs->prs_pcr_boundary = 90000;
  if (elementary_stream_has_no_audio(set, 1))
    prs->prs_pcr_boundary = 6*90000;

  streaming_target_deliver2(prs->prs_output, sm);
  /* do_rstlog */
  if (!TAILQ_EMPTY(&prs->prs_rstlog)) {
    parser_do_rstlog(prs);
  }
}

/**
 *
 */
static void
parser_input(void *opaque, streaming_message_t *sm)
{
  parser_t *prs = opaque;

  switch(sm->sm_type) {
  case SMT_MPEGTS:
    parser_input_mpegts(prs, (pktbuf_t *)sm->sm_data);
    streaming_msg_free(sm);
    break;
  case SMT_START:
    parser_input_start(prs, sm);
    break;
  default:
    streaming_target_deliver2(prs->prs_output, sm);
    break;
  }
}

static htsmsg_t *
parser_input_info(void *opaque, htsmsg_t *list)
{
  parser_t *prs = opaque;
  streaming_target_t *st = prs->prs_output;
  htsmsg_add_str(list, NULL, "parser input");
  return st->st_ops.st_info(st->st_opaque, list);
}

static streaming_ops_t parser_input_ops = {
  .st_cb   = parser_input,
  .st_info = parser_input_info
};

/**
 * Parser get output target
 */
streaming_target_t *
parser_output(streaming_target_t *pad)
{
  return ((parser_t *)pad)->prs_output;
}

/**
 * Parser create
 */
streaming_target_t *
parser_create(streaming_target_t *output, th_subscription_t *ts)
{
  parser_t *prs = calloc(1, sizeof(parser_t));
  service_t *t = ts->ths_service;

  prs->prs_output = output;
  prs->prs_subscription = ts;
  prs->prs_service = t;
  TAILQ_INIT(&prs->prs_rstlog);
  elementary_set_init(&prs->prs_components, LS_PARSER, service_nicename(t), t);
  streaming_target_init(&prs->prs_input, &parser_input_ops, prs, 0);
  return &prs->prs_input;

}

/*
 * Parser destroy
 */
void
parser_destroy(streaming_target_t *pad)
{
  parser_t *prs = (parser_t *)pad;
  elementary_stream_t *es;
  parser_es_t *pes;
  streaming_queue_clear(&prs->prs_rstlog);

  TAILQ_FOREACH(es, &prs->prs_components.set_all, es_link) {
    pes = (parser_es_t *)es;
    parser_clean_es(pes);
    if (pes->es_psi.mt_name)
      dvb_table_parse_done(&pes->es_psi);
  }
  elementary_set_clean(&prs->prs_components, NULL, 0);
  free(prs);
}
