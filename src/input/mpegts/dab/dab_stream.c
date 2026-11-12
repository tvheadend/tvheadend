/*
 *  DAB Streaming - Digital Audio Broadcasting decapsulation
 *  Wrapper around libdvbdab streamer for tvheadend integration
 *
 *  Copyright (C) 2025 L-S-D
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "tvheadend.h"

#if ENABLE_DVBDAB

#include "dab_stream.h"
#include "input.h"
#include "input/mpegts/mpegts_dvb.h"
#include <dvbdab/dvbdab_c.h>

/*
 * libdvbdab logs through tvhlog (by default it writes everything, with names
 * from the stream, to stderr)
 */
static void
dab_log_cb(int level, const char *tag, const char *msg)
{
  switch (level) {
  case DVBDAB_LOG_ERROR: tvherror(LS_MPEGTS, "dvbdab: %s: %s", tag, msg); break;
  case DVBDAB_LOG_WARN:  tvhwarn(LS_MPEGTS, "dvbdab: %s: %s", tag, msg); break;
  case DVBDAB_LOG_TRACE: tvhtrace(LS_MPEGTS, "dvbdab: %s: %s", tag, msg); break;
  default:               tvhdebug(LS_MPEGTS, "dvbdab: %s: %s", tag, msg); break;
  }
}

static pthread_once_t dab_log_once = PTHREAD_ONCE_INIT;

static void
dab_log_setup(void)
{
  dvbdab_set_log_callback(dab_log_cb);
}

void
dab_log_init(void)
{
  pthread_once(&dab_log_once, dab_log_setup);
  /* the library formats every message it lets through, per packet at debug:
   * only errors and warnings unless tvheadend itself logs debug; no trace */
  dvbdab_set_log_level(atomic_get(&tvhlog_level) >= LOG_DEBUG ? DVBDAB_LOG_DEBUG
                                                              : DVBDAB_LOG_WARN);
}

struct dab_stream_ctx {
  dvbdab_streamer_t      *streamer;
  dab_stream_output_cb    callback;
  void                   *opaque;
  int                     started;
};

/*
 * libdvbdab output callback - receives TS packets
 */
static void
dab_stream_ts_cb(void *opaque, const uint8_t *data, size_t len)
{
  dab_stream_ctx_t *ctx = opaque;

  if (ctx && ctx->callback && data && len > 0)
    ctx->callback(ctx->opaque, data, len);
}

dab_stream_ctx_t *
dab_stream_create(const dab_stream_config_t *config,
                   dab_stream_output_cb cb, void *opaque)
{
  dab_stream_ctx_t *ctx;
  dvbdab_streamer_config_t scfg;

  if (!config || !cb)
    return NULL;

  dab_log_init();

  ctx = calloc(1, sizeof(*ctx));
  if (!ctx)
    return NULL;

  /* Set up libdvbdab streamer config */
  memset(&scfg, 0, sizeof(scfg));
  scfg.pid = config->pid;

  switch (config->format) {
  case 0: /* ETI-NA */
    scfg.format = DVBDAB_FORMAT_ETI_NA;
    scfg.eti_padding = config->eti_padding;
    scfg.eti_bit_offset = config->eti_bit_offset;
    scfg.eti_inverted = config->eti_inverted;
    break;
  case 1: /* MPE */
    scfg.format = DVBDAB_FORMAT_MPE;
    scfg.filter_ip = config->filter_ip;
    scfg.filter_port = config->filter_port;
    break;
  case 4: /* TSNI - TS NI V.11 */
    scfg.format = DVBDAB_FORMAT_TSNI;
    /* TSNI only needs PID, no IP/port filtering */
    break;
  default:
    free(ctx);
    return NULL;
  }

  /* Create libdvbdab streamer */
  ctx->streamer = dvbdab_streamer_create(&scfg);
  if (!ctx->streamer) {
    free(ctx);
    return NULL;
  }

  /* Set output callback */
  dvbdab_streamer_set_output(ctx->streamer, dab_stream_ts_cb, ctx);

  ctx->callback = cb;
  ctx->opaque = opaque;
  ctx->started = 0;

  return ctx;
}

void
dab_stream_destroy(dab_stream_ctx_t *ctx)
{
  if (!ctx)
    return;

  if (ctx->streamer)
    dvbdab_streamer_destroy(ctx->streamer);

  free(ctx);
}

int
dab_stream_feed(dab_stream_ctx_t *ctx, const uint8_t *data, size_t len)
{
  if (!ctx || !ctx->streamer || !data)
    return -1;

  /* Feed to libdvbdab */
  dvbdab_streamer_feed(ctx->streamer, data, len);

  /* Auto-start all services once ready */
  if (!ctx->started && dvbdab_streamer_is_basic_ready(ctx->streamer)) {
    dvbdab_streamer_start_all(ctx->streamer);
    ctx->started = 1;
  }

  return 0;
}

/* **************************************************************************
 * Decapsulation of a DAB sub-mux: while its instance runs, the packets of the
 * input are fed to libdvbdab, its TS goes on as the stream of the mux
 * *************************************************************************/

static void
dab_decap_output_cb(void *opaque, const uint8_t *pkt, size_t len)
{
  mpegts_mux_instance_t *mmi = opaque;

  sbuf_append(&mmi->mmi_decap_out, pkt, len);
}

static int
dab_decap_format(const mpegts_mux_t *mm)
{
  switch (mm->mm_type) {
  case MM_TYPE_DAB_ETI:  return 0;
  case MM_TYPE_DAB_MPE:  return 1;
  case MM_TYPE_DAB_TSNI: return 4;
  default:               return -1;
  }
}

int
mpegts_mux_decap_start(mpegts_mux_instance_t *mmi)
{
  mpegts_mux_t *mm = mmi->mmi_mux;
  const dvb_mux_conf_t *dmc;
  dab_stream_config_t cfg;
  int format = dab_decap_format(mm);

  if (format < 0)
    return 0;   /* nothing to decapsulate */

  dmc = &((dvb_mux_t *)mm)->lm_tuning;
  memset(&cfg, 0, sizeof(cfg));
  cfg.format = format;
  cfg.pid = dmc->dmc_fe_pid;
  if (cfg.pid == 0) {
    tvherror(LS_MPEGTS, "%s: DAB mux has no PID configured", mm->mm_nicename);
    return -1;
  }
  if (format == 0) {            /* ETI-NA */
    cfg.eti_padding = dmc->dmc_dab_eti_padding;
    cfg.eti_bit_offset = dmc->dmc_dab_eti_bit_offset;
    cfg.eti_inverted = dmc->dmc_dab_eti_inverted;
  } else if (format == 1) {     /* MPE */
    cfg.filter_ip = dmc->dmc_dab_ip;
    cfg.filter_port = dmc->dmc_dab_port;
  }                             /* TS NI: the PID is all it needs */

  mmi->mmi_decap = dab_stream_create(&cfg, dab_decap_output_cb, mmi);
  if (mmi->mmi_decap == NULL) {
    tvherror(LS_MPEGTS, "%s: failed to create the DAB streaming context",
             mm->mm_nicename);
    return -1;
  }
  sbuf_init(&mmi->mmi_decap_out);
  tvhdebug(LS_MPEGTS, "%s: DAB streaming initialized for PID 0x%04X (format %d)",
           mm->mm_nicename, cfg.pid, format);
  return 0;
}

void
mpegts_mux_decap_stop(mpegts_mux_instance_t *mmi)
{
  if (mmi->mmi_decap == NULL)
    return;
  dab_stream_destroy(mmi->mmi_decap);
  mmi->mmi_decap = NULL;
  sbuf_free(&mmi->mmi_decap_out);
}

void
mpegts_mux_decap_feed(mpegts_mux_instance_t *mmi, sbuf_t *sb)
{
  const uint8_t *data = sb->sb_data;
  size_t len = sb->sb_ptr;
  size_t i = 0;
  size_t j;

  /* whole TS packets only: a lost sync byte is searched for again, the
   * start of a packet at the end waits for the next read */
  while (i + 188 <= len) {
    if (data[i] != 0x47) {
      i++;
      continue;
    }
    for (j = i; j + 188 <= len && data[j] == 0x47; j += 188)
      ;
    dab_stream_feed(mmi->mmi_decap, data + i, j - i);
    i = j;
  }
  sbuf_cut(sb, i);

  if (mmi->mmi_decap_out.sb_ptr > 0)
    mpegts_input_recv_packets(mmi, &mmi->mmi_decap_out, MPEGTS_DATA_DECAPSULATED, NULL);
}

#endif /* ENABLE_DVBDAB */
