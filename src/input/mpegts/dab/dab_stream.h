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

#ifndef __DAB_STREAM_H__
#define __DAB_STREAM_H__

#include <stdint.h>
#include <stddef.h>

/* TS packet size */
#define DAB_TS_PACKET_SIZE  188

/* Opaque DAB stream context */
typedef struct dab_stream_ctx dab_stream_ctx_t;

/* Output callback - receives TS packets with audio */
typedef void (*dab_stream_output_cb)(void *opaque, const uint8_t *pkt, size_t len);

/* DAB stream configuration */
typedef struct dab_stream_config {
  int         format;       /* 0=ETI-NA, 1=MPE, 2=GSE, 4=TSNI */
  uint16_t    pid;          /* PID containing DAB data */

  /* ETI-NA specific */
  int         eti_padding;     /* Leading 0xFF bytes */
  int         eti_bit_offset;  /* Bit position of E1 sync */
  int         eti_inverted;    /* Signal is inverted */

  /* MPE/GSE specific */
  uint32_t    filter_ip;    /* Multicast IP (host byte order) */
  uint16_t    filter_port;  /* UDP port */
} dab_stream_config_t;

/**
 * Create a DAB stream context
 * @param config    Stream configuration
 * @param cb        Output callback for TS packets
 * @param opaque    User context passed to callback
 * @return          Context handle, or NULL on error
 */
dab_stream_ctx_t *dab_stream_create(const dab_stream_config_t *config,
                                      dab_stream_output_cb cb, void *opaque);

/**
 * Destroy DAB stream context
 */
void dab_stream_destroy(dab_stream_ctx_t *ctx);

/**
 * Feed TS packet(s) to DAB stream
 * @param ctx       Stream context
 * @param data      Raw TS data (must start at sync byte 0x47)
 * @param len       Length in bytes (should be multiple of 188)
 * @return          0 on success, -1 on error
 */
int dab_stream_feed(dab_stream_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * Check if ensemble discovery is complete
 * @param ctx       Stream context
 * @return          1 if ready, 0 if still discovering
 */
int dab_stream_is_ready(dab_stream_ctx_t *ctx);

/**
 * Start streaming all services in the ensemble
 * @param ctx       Stream context
 * @return          Number of services started, or -1 on error
 */
int dab_stream_start_all(dab_stream_ctx_t *ctx);

/**
 * Start streaming a specific service
 * @param ctx           Stream context
 * @param subchannel_id Subchannel ID to stream
 * @return              0 on success, -1 on error
 */
int dab_stream_start_service(dab_stream_ctx_t *ctx, uint8_t subchannel_id);

#endif /* __DAB_STREAM_H__ */
