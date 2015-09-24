/*
 *  Packet parsing functions
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

int h264_decode_seq_parameter_set(struct elementary_stream *st, bitstream_t *bs);

int h264_decode_pic_parameter_set(struct elementary_stream *st, bitstream_t *bs);

int h264_decode_slice_header(struct elementary_stream *st, bitstream_t *bs,
			     int *pkttype, int *isfield);

#endif /* PARSER_H264_H_ */
