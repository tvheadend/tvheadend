/*
 *  DAB Streaming - Digital Audio Broadcasting decapsulation
 *  Wrapper around libdvbdab streamer for tvheadend integration
 *
 *  Copyright (C) 2025
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include "dab_stream.h"
#include <dvbdab/dvbdab_c.h>

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
  case 2: /* GSE */
    scfg.format = DVBDAB_FORMAT_GSE;
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

int
dab_stream_is_ready(dab_stream_ctx_t *ctx)
{
  if (!ctx || !ctx->streamer)
    return 0;

  return dvbdab_streamer_is_ready(ctx->streamer);
}

int
dab_stream_start_all(dab_stream_ctx_t *ctx)
{
  if (!ctx || !ctx->streamer)
    return -1;

  ctx->started = 1;
  return dvbdab_streamer_start_all(ctx->streamer);
}

int
dab_stream_start_service(dab_stream_ctx_t *ctx, uint8_t subchannel_id)
{
  if (!ctx || !ctx->streamer)
    return -1;

  ctx->started = 1;
  return dvbdab_streamer_start_service(ctx->streamer, subchannel_id);
}
