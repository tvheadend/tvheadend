/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 Andreas Öman
 *
 * Packet parsing functions
 */

#ifndef PARSER_H264_H_
#define PARSER_H264_H_

#include "bitstream.h"

#define H264_NAL_SLICE           1
#define H264_NAL_DPA             2
#define H264_NAL_DPB             3
#define H264_NAL_DPC             4
#define H264_NAL_IDR_SLICE       5
#define H264_NAL_SEI             6
#define H264_NAL_SPS             7
#define H264_NAL_PPS             8
#define H264_NAL_AUD             9
#define H264_NAL_END_SEQUENCE    10
#define H264_NAL_END_STREAM      11
#define H264_NAL_FILLER_DATA     12
#define H264_NAL_SPS_EXT         13
#define H264_NAL_AUXILIARY_SLICE 19

void *h264_nal_deescape(bitstream_t *bs, const uint8_t *data, int size);

int h264_decode_seq_parameter_set(parser_es_t *st, bitstream_t *bs);

int h264_decode_pic_parameter_set(parser_es_t *st, bitstream_t *bs);

int h264_decode_slice_header(parser_es_t *st, bitstream_t *bs,
			     int *pkttype, int *isfield);

#endif /* PARSER_H264_H_ */
