/*
 *  T2MI Decapsulation - DVB-T2 Modulator Interface
 *
 *  Based on ETSI TS 102 773
 *
 *  Copyright (C) 2024
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#ifndef __T2MI_DECAP_H__
#define __T2MI_DECAP_H__

#include <stdint.h>
#include <stddef.h>

/* T2MI packet types (ETSI TS 102 773, Table 4) */
#define T2MI_PKT_BASEBAND_FRAME     0x00
#define T2MI_PKT_AUX_IQ_DATA        0x01
#define T2MI_PKT_ARBITRARY_CELL     0x02
#define T2MI_PKT_L1_CURRENT         0x10
#define T2MI_PKT_L1_FUTURE          0x11
#define T2MI_PKT_P2_BIAS_BALANCING  0x12
#define T2MI_PKT_DVB_T2_TIMESTAMP   0x20
#define T2MI_PKT_INDIVIDUAL_ADDR    0x21
#define T2MI_PKT_FEF_NULL           0x30
#define T2MI_PKT_FEF_IQ             0x31
#define T2MI_PKT_FEF_COMPOSITE      0x32
#define T2MI_PKT_FEF_SUBPART        0x33

/* TS packet size */
#define T2MI_TS_PACKET_SIZE  188
#define T2MI_TS_SYNC_BYTE    0x47

/* T2MI header size (before payload) */
#define T2MI_HEADER_SIZE  6

/* BBframe header size */
#define T2MI_BBFRAME_HEADER_SIZE  10

/* PLP filter: extract all PLPs */
#define T2MI_PLP_ALL    0xFF

/* Opaque context */
typedef struct t2mi_ctx t2mi_ctx_t;

/* Callback for extracted inner TS packets
 * Called for each complete 188-byte TS packet extracted from T2MI stream
 *
 * @param opaque    User-provided context
 * @param pkt       Pointer to 188-byte TS packet
 * @param plp_id    PLP ID this packet came from
 */
typedef void (*t2mi_output_cb)(void *opaque, const uint8_t *pkt, uint8_t plp_id);

/* Statistics */
typedef struct {
    uint64_t ts_packets_in;      /* Outer TS packets fed */
    uint64_t t2mi_packets;       /* T2MI packets parsed */
    uint64_t bbframes;           /* Baseband frames processed */
    uint64_t ts_packets_out;     /* Inner TS packets extracted */
    uint64_t errors_cc;          /* Continuity counter errors */
    uint64_t errors_crc;         /* CRC errors */
    uint64_t errors_sync;        /* Sync byte errors */
} t2mi_stats_t;

/**
 * Create T2MI decapsulation context
 *
 * @param pid       T2MI PID to filter (or 0 to accept any PID)
 * @param plp       PLP ID to extract (or T2MI_PLP_ALL for all)
 * @param cb        Callback for extracted TS packets
 * @param opaque    User context passed to callback
 * @return          Context handle, or NULL on error
 */
t2mi_ctx_t *t2mi_decap_create(uint16_t pid, uint8_t plp, t2mi_output_cb cb, void *opaque);

/**
 * Destroy T2MI context and free resources
 */
void t2mi_decap_destroy(t2mi_ctx_t *ctx);

/**
 * Feed a 188-byte TS packet into the decapsulator
 *
 * @param ctx       Context handle
 * @param ts_pkt    Pointer to 188-byte TS packet
 * @return          0 on success, -1 on error
 */
int t2mi_decap_feed(t2mi_ctx_t *ctx, const uint8_t *ts_pkt);

/**
 * Get decapsulation statistics
 */
void t2mi_decap_get_stats(t2mi_ctx_t *ctx, t2mi_stats_t *stats);

/**
 * Reset statistics counters
 */
void t2mi_decap_reset_stats(t2mi_ctx_t *ctx);

#endif /* __T2MI_DECAP_H__ */
