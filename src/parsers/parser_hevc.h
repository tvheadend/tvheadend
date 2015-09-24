/*
 * Copyright (c) 2014 Tim Walker <tdskywalker@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PARSER_HEVC_H
#define PARSER_HEVC_H

#include "tvheadend.h"
#include "packet.h"
#include "bitstream.h"

#define HEVC_NAL_TRAIL_N    0
#define HEVC_NAL_TRAIL_R    1

#define HEVC_NAL_TSA_N      2
#define HEVC_NAL_TSA_R      3
#define HEVC_NAL_STSA_N     4
#define HEVC_NAL_STSA_R     5
#define HEVC_NAL_RADL_N     6
#define HEVC_NAL_RADL_R     7
#define HEVC_NAL_RASL_N     8
#define HEVC_NAL_RASL_R     9

#define HEVC_NAL_BLA_W_LP   16
#define HEVC_NAL_BLA_W_RADL 17
#define HEVC_NAL_BLA_N_LP   18
#define HEVC_NAL_IDR_W_RADL 19
#define HEVC_NAL_IDR_N_LP   20
#define HEVC_NAL_CRA_NUT    21

#define HEVC_NAL_VPS        32
#define HEVC_NAL_SPS        33
#define HEVC_NAL_PPS        34
#define NAL_AUD             35
#define NAL_EOS_NUT         36
#define NAL_EOB_NUT         37
#define NAL_FD_NUT          38
#define HEVC_NAL_SEI_PREFIX 39
#define HEVC_NAL_SEI_SUFFIX 40

int isom_write_hvcc(sbuf_t *pb, const uint8_t *src, int size);

th_pkt_t * hevc_convert_pkt(th_pkt_t *src);

void hevc_decode_vps(struct elementary_stream *st, bitstream_t *bs);
void hevc_decode_sps(struct elementary_stream *st, bitstream_t *bs);
void hevc_decode_pps(struct elementary_stream *st, bitstream_t *bs);

int hevc_decode_slice_header(struct elementary_stream *st, bitstream_t *bs,
                             int *pkttype);

#endif /* PARSER_HEVC_H */
