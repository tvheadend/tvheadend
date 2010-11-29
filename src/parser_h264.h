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

void *h264_nal_deescape(bitstream_t *bs, const uint8_t *data, int size);

int h264_decode_seq_parameter_set(struct th_stream *st, bitstream_t *bs);

int h264_decode_pic_parameter_set(struct th_stream *st, bitstream_t *bs);

int h264_decode_slice_header(struct th_stream *st, bitstream_t *bs,
			     int *pkttype, int *duration, int *isfield);

#endif /* PARSER_H264_H_ */
