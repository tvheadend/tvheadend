/*
 *  T2MI Decapsulation - DVB-T2 Modulator Interface
 *
 *  Based on ETSI TS 102 773 and libt2mi
 *
 *  Copyright (C) 2024
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "t2mi_decap.h"

/* T2MI packet header offsets */
#define T2MI_HDR_TYPE           0
#define T2MI_HDR_COUNT          1
#define T2MI_HDR_SUPERFRAME     2   /* high nibble */
#define T2MI_HDR_RFU            3
#define T2MI_HDR_PAYLOAD_LEN_HI 4
#define T2MI_HDR_PAYLOAD_LEN_LO 5

/* BBframe packet header offsets (within T2MI payload) */
#define BBF_HDR_FRAME_IDX       0
#define BBF_HDR_PLP_ID          1
#define BBF_HDR_FLAGS           2

/* BBframe data header offsets (within BBframe payload) */
#define BBFRAME_MATYPE1         0
#define BBFRAME_MATYPE2         1
#define BBFRAME_UPL_HI          2
#define BBFRAME_UPL_LO          3
#define BBFRAME_DFL_HI          4
#define BBFRAME_DFL_LO          5
#define BBFRAME_SYNC            6
#define BBFRAME_SYNCD_HI        7
#define BBFRAME_SYNCD_LO        8
#define BBFRAME_CRC8            9
#define BBFRAME_DATA_START      10

/* Maximum T2MI packet size (conservative) */
#define T2MI_MAX_PACKET_SIZE    65536

/* Reassembly buffer size */
#define REASSEMBLY_BUFFER_SIZE  (256 * 1024)

/* BBframe format types (TS/GS field) */
#define BBFRAME_FORMAT_GFPS     0   /* Generic Fixed Packetized Stream */
#define BBFRAME_FORMAT_GCS      1   /* Generic Continuous Stream */
#define BBFRAME_FORMAT_GSE      2   /* GSE / GSE-HEM */
#define BBFRAME_FORMAT_TS       3   /* Transport Stream */

/* TS packet size without sync byte (High Efficiency Mode) */
#define TS_PACKET_SIZE_HEM      (T2MI_TS_PACKET_SIZE - 1)

struct t2mi_ctx {
    uint16_t            pid;            /* T2MI PID filter (0 = any) */
    uint8_t             plp;            /* PLP filter (T2MI_PLP_ALL = any) */
    t2mi_output_cb      callback;       /* Output callback */
    void               *opaque;         /* User context */

    /* TS packet reassembly */
    uint8_t             ts_buffer[REASSEMBLY_BUFFER_SIZE];
    size_t              ts_pos;         /* Current position in buffer */
    uint8_t             last_cc;        /* Last continuity counter */
    int                 synced;         /* Have we synced to T2MI stream? */

    /* T2MI packet state */
    uint8_t             t2mi_packet[T2MI_MAX_PACKET_SIZE];
    size_t              t2mi_packet_len;    /* Expected length */
    size_t              t2mi_packet_pos;    /* Current fill position */
    int                 in_packet;          /* Currently assembling packet */

    /* Inner TS reassembly (for fragmented packets across BBframes) */
    uint8_t             fragment[T2MI_TS_PACKET_SIZE];
    size_t              fragment_len;       /* Bytes in fragment buffer */

    /* Statistics */
    t2mi_stats_t        stats;
};

/* Forward declarations */
static void t2mi_process_packet(t2mi_ctx_t *ctx);

t2mi_ctx_t *t2mi_decap_create(uint16_t pid, uint8_t plp, t2mi_output_cb cb, void *opaque)
{
    t2mi_ctx_t *ctx;

    if (!cb)
        return NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->pid = pid;
    ctx->plp = plp;
    ctx->callback = cb;
    ctx->opaque = opaque;
    ctx->last_cc = 0xFF;  /* Invalid CC to detect first packet */

    return ctx;
}

void t2mi_decap_destroy(t2mi_ctx_t *ctx)
{
    if (ctx)
        free(ctx);
}

int t2mi_decap_feed(t2mi_ctx_t *ctx, const uint8_t *ts_pkt)
{
    uint16_t pid;
    uint8_t afc, cc, pusi;
    const uint8_t *payload;
    size_t payload_len;
    size_t offset;

    if (!ctx || !ts_pkt)
        return -1;

    /* Check sync byte */
    if (ts_pkt[0] != T2MI_TS_SYNC_BYTE) {
        ctx->stats.errors_sync++;
        return -1;
    }

    ctx->stats.ts_packets_in++;

    /* Extract PID */
    pid = ((ts_pkt[1] & 0x1F) << 8) | ts_pkt[2];

    /* Filter by PID if specified */
    if (ctx->pid != 0 && pid != ctx->pid)
        return 0;

    /* Check adaptation field control */
    afc = (ts_pkt[3] >> 4) & 0x03;
    if (afc == 0 || afc == 2)  /* No payload */
        return 0;

    /* Get continuity counter */
    cc = ts_pkt[3] & 0x0F;

    /* Check CC continuity */
    if (ctx->last_cc != 0xFF) {
        uint8_t expected = (ctx->last_cc + 1) & 0x0F;
        if (cc != expected) {
            ctx->stats.errors_cc++;
            /* Reset reassembly on CC error */
            ctx->in_packet = 0;
            ctx->t2mi_packet_pos = 0;
            ctx->synced = 0;
        }
    }
    ctx->last_cc = cc;

    /* Find payload start */
    offset = 4;
    if (afc == 3) {  /* Adaptation field present */
        offset = 5 + ts_pkt[4];
        if (offset >= T2MI_TS_PACKET_SIZE)
            return 0;
    }

    payload = ts_pkt + offset;
    payload_len = T2MI_TS_PACKET_SIZE - offset;

    /* Check PUSI (Payload Unit Start Indicator) */
    pusi = (ts_pkt[1] >> 6) & 0x01;

    if (pusi) {
        /* Pointer field present */
        uint8_t pointer = payload[0];
        payload++;
        payload_len--;

        /* Process continuation bytes (end of previous T2MI packet) */
        if (pointer > 0 && pointer <= payload_len) {
            /* If we have a packet in progress, complete it with continuation bytes */
            if (ctx->in_packet) {
                size_t space = ctx->t2mi_packet_len - ctx->t2mi_packet_pos;
                size_t copy = (pointer < space) ? pointer : space;
                memcpy(ctx->t2mi_packet + ctx->t2mi_packet_pos, payload, copy);
                ctx->t2mi_packet_pos += copy;

                if (ctx->t2mi_packet_pos >= ctx->t2mi_packet_len) {
                    t2mi_process_packet(ctx);
                    ctx->in_packet = 0;
                }
            }
            /* ALWAYS skip pointer bytes - they belong to previous packet */
            payload += pointer;
            payload_len -= pointer;
        }

        /* Start new T2MI packet(s) - loop to handle multiple small packets per payload */
        while (payload_len >= T2MI_HEADER_SIZE) {
            uint16_t payload_bits = (payload[T2MI_HDR_PAYLOAD_LEN_HI] << 8) |
                                     payload[T2MI_HDR_PAYLOAD_LEN_LO];
            size_t payload_bytes = (payload_bits + 7) / 8;
            size_t total_len = T2MI_HEADER_SIZE + payload_bytes + 4; /* +4 for CRC */

            if (total_len > T2MI_MAX_PACKET_SIZE)
                break;

            ctx->t2mi_packet_len = total_len;
            ctx->t2mi_packet_pos = 0;
            ctx->in_packet = 1;

            /* Copy what we have */
            size_t copy = (payload_len < total_len) ? payload_len : total_len;
            memcpy(ctx->t2mi_packet, payload, copy);
            ctx->t2mi_packet_pos = copy;

            /* Check if complete - if so, process and check for more packets */
            if (ctx->t2mi_packet_pos >= ctx->t2mi_packet_len) {
                t2mi_process_packet(ctx);
                ctx->in_packet = 0;
                payload += total_len;
                payload_len -= total_len;
                /* Loop continues to check for another packet */
            } else {
                /* Packet spans multiple TS packets - stop scanning */
                break;
            }
        }

        /* Handle partial header - save remaining bytes for next TS packet */
        if (payload_len > 0 && payload_len < T2MI_HEADER_SIZE && !ctx->in_packet) {
            memcpy(ctx->t2mi_packet, payload, payload_len);
            ctx->t2mi_packet_pos = payload_len;
            ctx->t2mi_packet_len = 0;  /* Unknown until we get full header */
            ctx->in_packet = 1;
        }
    } else {
        /* Continuation packet - just append data */
        if (ctx->in_packet) {
            /* Check if we're in partial header mode */
            if (ctx->t2mi_packet_len == 0) {
                /* Still gathering header bytes */
                size_t need = T2MI_HEADER_SIZE - ctx->t2mi_packet_pos;
                size_t copy = (payload_len < need) ? payload_len : need;
                memcpy(ctx->t2mi_packet + ctx->t2mi_packet_pos, payload, copy);
                ctx->t2mi_packet_pos += copy;
                payload += copy;
                payload_len -= copy;

                /* Now check if we have full header */
                if (ctx->t2mi_packet_pos >= T2MI_HEADER_SIZE) {
                    uint16_t payload_bits = (ctx->t2mi_packet[T2MI_HDR_PAYLOAD_LEN_HI] << 8) |
                                             ctx->t2mi_packet[T2MI_HDR_PAYLOAD_LEN_LO];
                    size_t payload_bytes = (payload_bits + 7) / 8;
                    size_t total_len = T2MI_HEADER_SIZE + payload_bytes + 4;

                    if (total_len > T2MI_MAX_PACKET_SIZE) {
                        ctx->in_packet = 0;  /* Invalid - discard */
                        return 0;
                    }
                    ctx->t2mi_packet_len = total_len;

                    /* Continue copying remaining payload if any */
                    if (payload_len > 0) {
                        size_t space = total_len - ctx->t2mi_packet_pos;
                        copy = (payload_len < space) ? payload_len : space;
                        memcpy(ctx->t2mi_packet + ctx->t2mi_packet_pos, payload, copy);
                        ctx->t2mi_packet_pos += copy;
                    }

                    if (ctx->t2mi_packet_pos >= ctx->t2mi_packet_len) {
                        t2mi_process_packet(ctx);
                        ctx->in_packet = 0;
                    }
                }
            } else {
                /* Normal continuation - we know the length */
                size_t space = ctx->t2mi_packet_len - ctx->t2mi_packet_pos;
                size_t copy = (payload_len < space) ? payload_len : space;
                memcpy(ctx->t2mi_packet + ctx->t2mi_packet_pos, payload, copy);
                ctx->t2mi_packet_pos += copy;

                /* Check if complete */
                if (ctx->t2mi_packet_pos >= ctx->t2mi_packet_len) {
                    t2mi_process_packet(ctx);
                    ctx->in_packet = 0;
                }
            }
        }
    }

    return 0;
}

/*
 * Output a complete TS packet (prepend sync byte for HEM mode)
 */
static void t2mi_output_ts(t2mi_ctx_t *ctx, const uint8_t *data, uint8_t plp_id)
{
    uint8_t ts_pkt[T2MI_TS_PACKET_SIZE];

    /* Prepend sync byte (HEM mode removes it from transmission) */
    ts_pkt[0] = T2MI_TS_SYNC_BYTE;
    memcpy(ts_pkt + 1, data, TS_PACKET_SIZE_HEM);

    ctx->stats.ts_packets_out++;
    ctx->callback(ctx->opaque, ts_pkt, plp_id);
}

/*
 * Process TS-mode BBframe data field
 * In High Efficiency Mode (HEM), TS packets are 187 bytes (no sync byte)
 *
 * Key insight: SYNCD tells us exactly where the first complete packet starts.
 * Some BBframes have trailing padding which makes our fragment calculation wrong.
 * We must trust SYNCD as the definitive source of truth.
 */
static void t2mi_process_ts_bbframe(t2mi_ctx_t *ctx, const uint8_t *data,
                                     size_t data_len, uint16_t syncd, uint8_t plp_id)
{
    const uint8_t *ptr = data;
    const uint8_t *end = data + data_len;
    size_t syncd_bytes = (syncd != 0xFFFF) ? (syncd / 8) : 0;

    /*
     * Data field structure:
     * - Bytes 0 to (syncd_bytes - 1): Continuation of previous User Packet
     * - Bytes syncd_bytes onwards: Complete User Packets (187 bytes each in HEM)
     * - Possibly trailing padding at end (not part of any packet)
     */

    /* Handle continuation data (complete previous fragment) */
    if (syncd != 0xFFFF && syncd_bytes > 0 && ctx->fragment_len > 0) {
        /*
         * SYNCD tells us exactly how many continuation bytes there are.
         * Our saved fragment might include trailing padding from previous BBframe.
         * Calculate the REAL fragment size based on SYNCD.
         */
        size_t continuation = syncd_bytes;
        size_t real_fragment_len = TS_PACKET_SIZE_HEM - continuation;

        if (real_fragment_len <= ctx->fragment_len && continuation <= data_len) {
            /*
             * Use only real_fragment_len bytes from our saved fragment,
             * then append continuation bytes from current BBframe.
             */
            memcpy(ctx->fragment + real_fragment_len, data, continuation);
            t2mi_output_ts(ctx, ctx->fragment, plp_id);
            ctx->fragment_len = 0;
        } else {
            /* Something wrong - discard fragment */
            ctx->fragment_len = 0;
        }
    } else if (syncd != 0xFFFF && ctx->fragment_len > 0 && syncd_bytes == 0) {
        /* SYNCD=0 means first packet starts at beginning - discard stale fragment */
        ctx->fragment_len = 0;
    }

    /* Skip to first complete UP (at syncd position) */
    if (syncd != 0xFFFF && syncd_bytes < data_len) {
        ptr = data + syncd_bytes;
    } else if (syncd == 0xFFFF) {
        /* No UP starts in this BBframe - all data is continuation */
        if (ctx->fragment_len > 0) {
            size_t need = TS_PACKET_SIZE_HEM - ctx->fragment_len;
            size_t avail = end - ptr;

            if (avail >= need) {
                memcpy(ctx->fragment + ctx->fragment_len, ptr, need);
                t2mi_output_ts(ctx, ctx->fragment, plp_id);
                ctx->fragment_len = 0;
                ptr += need;
            } else {
                memcpy(ctx->fragment + ctx->fragment_len, ptr, avail);
                ctx->fragment_len += avail;
                return;
            }
        }
        /* If no fragment pending, this BBframe has no useful data for us */
        return;
    }

    /* Process complete User Packets (187 bytes each in HEM) */
    while (ptr + TS_PACKET_SIZE_HEM <= end) {
        t2mi_output_ts(ctx, ptr, plp_id);
        ptr += TS_PACKET_SIZE_HEM;
    }

    /*
     * Save remaining bytes as fragment.
     * Note: This might include trailing padding - the NEXT BBframe's SYNCD
     * will tell us the real fragment size.
     */
    if (ptr < end) {
        size_t remaining = end - ptr;
        if (remaining < TS_PACKET_SIZE_HEM) {
            memcpy(ctx->fragment, ptr, remaining);
            ctx->fragment_len = remaining;
        }
    } else {
        ctx->fragment_len = 0;
    }
}

static void t2mi_process_packet(t2mi_ctx_t *ctx)
{
    uint8_t pkt_type;
    uint8_t plp_id;
    uint8_t matype1;
    uint8_t format;
    uint16_t dfl, syncd;
    size_t header_offset;
    size_t data_offset;
    size_t data_len;

    ctx->stats.t2mi_packets++;

    /* Get packet type */
    pkt_type = ctx->t2mi_packet[T2MI_HDR_TYPE];

    /* Only process Baseband Frame packets */
    if (pkt_type != T2MI_PKT_BASEBAND_FRAME)
        return;

    ctx->stats.bbframes++;

    /* Get PLP ID from BBframe wrapper */
    plp_id = ctx->t2mi_packet[T2MI_HEADER_SIZE + BBF_HDR_PLP_ID];

    /* Filter by PLP if specified */
    if (ctx->plp != T2MI_PLP_ALL && plp_id != ctx->plp)
        return;

    /* BBframe header starts after T2MI header (6) + BBframe wrapper (3) */
    header_offset = T2MI_HEADER_SIZE + 3;

    /* Check we have enough data for BBframe header */
    if (ctx->t2mi_packet_pos < header_offset + BBFRAME_DATA_START)
        return;

    /* Parse BBframe header */
    matype1 = ctx->t2mi_packet[header_offset + BBFRAME_MATYPE1];
    format = (matype1 >> 6) & 0x03;

    /* DFL = Data Field Length in bits */
    dfl = (ctx->t2mi_packet[header_offset + BBFRAME_DFL_HI] << 8) |
           ctx->t2mi_packet[header_offset + BBFRAME_DFL_LO];

    /* SYNCD = distance to first UP start in bits (0xFFFF = no sync) */
    syncd = (ctx->t2mi_packet[header_offset + BBFRAME_SYNCD_HI] << 8) |
             ctx->t2mi_packet[header_offset + BBFRAME_SYNCD_LO];

    /* Data field starts after BBframe header */
    data_offset = header_offset + BBFRAME_DATA_START;
    data_len = (dfl + 7) / 8;

    /* Check bounds */
    if (data_offset + data_len > ctx->t2mi_packet_pos)
        data_len = ctx->t2mi_packet_pos - data_offset;

    /* Process based on format */
    if (format == BBFRAME_FORMAT_TS) {
        /* Transport Stream mode - use HEM extraction */
        t2mi_process_ts_bbframe(ctx, ctx->t2mi_packet + data_offset,
                                 data_len, syncd, plp_id);
    }
    /* Other formats (GSE, GCS) not supported yet */
}


void t2mi_decap_get_stats(t2mi_ctx_t *ctx, t2mi_stats_t *stats)
{
    if (ctx && stats)
        memcpy(stats, &ctx->stats, sizeof(*stats));
}

void t2mi_decap_reset_stats(t2mi_ctx_t *ctx)
{
    if (ctx)
        memset(&ctx->stats, 0, sizeof(ctx->stats));
}
