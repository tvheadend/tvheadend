/*
 * h264 SPS / PPS Parser
 *
 * Based on h264.c from ffmpeg (www.ffmpeg.org)
 *
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tvheadend.h"
#include "parsers.h"
#include "parser_h264.h"
#include "bitstream.h"
#include "service.h"

#define MAX_SPS_COUNT          32
#define MAX_PPS_COUNT         256

/**
 * H.264 parser, nal escaper
 */
void *
h264_nal_deescape(bitstream_t *bs, const uint8_t *data, int size)
{
  uint_fast8_t c;
  uint8_t *d;
  const uint8_t *end, *end2;

  bs->rdata = d = malloc(size);
  bs->offset = 0;

  end2 = data + size;

  if (size > 2) {

    /* Escape 0x000003 into 0x0000 */

    end = end2 - 2;
    while (data < end) {
      c = *data++;
      if (c || *data || *(data + 1) != 3) {
        *d++ = c;
      } else {
        *d++ = 0;
        *d++ = 0;
        data += 2;
      }
    }

  }

  while (data < end2)
    *d++ = *data++;

  bs->len = (d - bs->rdata) * 8;
  return (void *)bs->rdata;
}


/**
 * Level to CPB size table
 */

static const int h264_lev2cpbsize[][2] = {
  {10, 175},
  {11, 500},
  {12, 1000},
  {13, 2000},
  {20, 2000},
  {21, 4000},
  {22, 4000},
  {30, 10000},
  {31, 14000},
  {32, 20000},
  {40, 25000},
  {41, 62500},
  {42, 62500},
  {50, 135000},
  {51, 240000},
  {-1, -1},
};

typedef struct h264_sps {
  uint8_t  valid: 1;
  uint8_t  mbs_only_flag;
  uint8_t  aff;
  uint8_t  fixed_rate;
  uint32_t cbpsize;
  uint16_t width;
  uint16_t height;
  uint16_t max_frame_num_bits;
  uint32_t units_in_tick;
  uint32_t time_scale;
  uint16_t aspect_num;
  uint16_t aspect_den;
} h264_sps_t;

typedef struct h264_pps {
  uint8_t valid: 1;
  uint8_t sps_id;
} h264_pps_t;

typedef struct h264_private {

  h264_sps_t sps[MAX_SPS_COUNT];
  h264_pps_t pps[MAX_PPS_COUNT];

  int64_t start;

} h264_private_t;



static const uint16_t h264_aspect[17][2] = {
 {0, 1},
 {1, 1},
 {12, 11},
 {10, 11},
 {16, 11},
 {40, 33},
 {24, 11},
 {20, 11},
 {32, 11},
 {80, 33},
 {18, 11},
 {15, 11},
 {64, 33},
 {160,99},
 {4, 3},
 {3, 2},
 {2, 1},
};


static int 
decode_vui(h264_sps_t *sps, bitstream_t *bs)
{
  sps->aspect_num = 1;
  sps->aspect_den = 1;

  if (read_bits1(bs)) {
    uint32_t aspect = read_bits(bs, 8);
    if(aspect == 255) {
      sps->aspect_num = read_bits(bs, 16);
      sps->aspect_den = read_bits(bs, 16);
    } else if(aspect < ARRAY_SIZE(h264_aspect)) {
      sps->aspect_num = h264_aspect[aspect][0];
      sps->aspect_den = h264_aspect[aspect][1];
    }
  }

  if (read_bits1(bs))   /* overscan_info_present_flag */
    skip_bits1(bs);     /* overscan_appropriate_flag */

  if (read_bits1(bs)) { /* video_signal_type_present_flag */
    skip_bits(bs, 3);   /* video_format */
    skip_bits1(bs);     /* video_full_range_flag */
    if(read_bits1(bs)){ /* colour_description_present_flag */
      skip_bits(bs, 8); /* colour_primaries */
      skip_bits(bs, 8); /* transfer_characteristics */
      skip_bits(bs, 8); /* matrix_coefficients */
    }
  }

  if (read_bits1(bs)) { /* chroma_location_info_present_flag */
    read_golomb_ue(bs); /* chroma_sample_location_type_top_field */
    read_golomb_ue(bs); /* chroma_sample_location_type_bottom_field */
  }

  if (!read_bits1(bs))   /* We need timing info */
    return 0;

  if (remaining_bits(bs) < 65)
    return 0;

  sps->units_in_tick = read_bits(bs, 32);
  sps->time_scale    = read_bits(bs, 32);
  sps->fixed_rate    = read_bits1(bs);
  return 0;
}



static void 
decode_scaling_list(bitstream_t *bs, int size)
{
  int i, last = 8, next = 8;
  if(!read_bits1(bs))
    return; /* matrix not written */

  for (i=0;i<size;i++){
    if (next)
      next = (last + read_golomb_se(bs)) & 0xff;
    if (!i && !next)
      break;  /* matrix not written */
    last = next ? next : last;
  }
}


int
h264_decode_seq_parameter_set(parser_es_t *st, bitstream_t *bs)
{
  uint32_t profile_idc, level_idc, poc_type;
  uint32_t sps_id, tmp, i, width, height;
  uint32_t cbpsize, mbs_only_flag, aff;
  uint32_t max_frame_num_bits;
  uint32_t crop_left, crop_right, crop_top, crop_bottom;
  h264_private_t *p;
  h264_sps_t *sps;

  if ((p = st->es_priv) == NULL) {
    p = st->es_priv = calloc(1, sizeof(h264_private_t));
    p->start = mclk();
  }

  profile_idc = read_bits(bs, 8);
  skip_bits1(bs);   //constraint_set0_flag
  skip_bits1(bs);   //constraint_set1_flag
  skip_bits1(bs);   //constraint_set2_flag
  skip_bits1(bs);   //constraint_set3_flag
  skip_bits(bs, 4); // reserved
  level_idc = read_bits(bs, 8);

  sps_id = read_golomb_ue(bs);
  if(sps_id >= MAX_SPS_COUNT)
    return -1;
  sps = &p->sps[sps_id];

  i = 0;
  cbpsize = -1;
  while (h264_lev2cpbsize[i][0] != -1) {
    if (h264_lev2cpbsize[i][0] >= level_idc) {
      cbpsize = h264_lev2cpbsize[i][1];
      break;
    }
    i++;
  }
  if (cbpsize == -1)
    return -1;

  if (profile_idc >= 100){ //high profile
    tmp = read_golomb_ue(bs);
    if (tmp == 3)          //chroma_format_idc
      read_bits1(bs);      //residual_color_transform_flag
    read_golomb_ue(bs);    //bit_depth_luma_minus8
    read_golomb_ue(bs);    //bit_depth_chroma_minus8
    read_bits1(bs);        //transform_bypass

    if(read_bits1(bs)) {
      /* Scaling matrices */
      decode_scaling_list(bs, 16);
      decode_scaling_list(bs, 16);
      decode_scaling_list(bs, 16);
      decode_scaling_list(bs, 16);
      decode_scaling_list(bs, 16);
      decode_scaling_list(bs, 16);

      decode_scaling_list(bs, 64);
      decode_scaling_list(bs, 64);

    }
  }

  max_frame_num_bits = read_golomb_ue(bs) + 4;
  poc_type = read_golomb_ue(bs); // pic_order_cnt_type

  if(poc_type == 0){
    read_golomb_ue(bs);
  } else if(poc_type == 1){
    skip_bits1(bs);
    read_golomb_se(bs);
    read_golomb_se(bs);
    tmp = read_golomb_ue(bs); /* poc_cycle_length */
    for(i = 0; i < tmp; i++)
      read_golomb_se(bs);

  }else if(poc_type != 2){
    /* Illegal poc */
    return -1;
  }

  tmp = read_golomb_ue(bs);

  read_bits1(bs);

  width  = read_golomb_ue(bs) + 1; /* mb width */
  height = read_golomb_ue(bs) + 1; /* mb height */

  mbs_only_flag = read_bits1(bs);
  aff = 0;
  if(!mbs_only_flag)
    aff = read_bits1(bs);

  read_bits1(bs);

  /* CROP */
  crop_left = crop_right = crop_top = crop_bottom = 0;
  if (read_bits1(bs)){
    crop_left   = read_golomb_ue(bs) * 2;
    crop_right  = read_golomb_ue(bs) * 2;
    crop_top    = read_golomb_ue(bs) * 2;
    crop_bottom = read_golomb_ue(bs) * 2;
  }

  if (read_bits1(bs)) /* vui present */
    if (decode_vui(sps, bs))
      return -1;

  sps->max_frame_num_bits = max_frame_num_bits;
  sps->mbs_only_flag = mbs_only_flag;
  sps->aff = aff;
  sps->cbpsize = cbpsize * 125; /* Convert from kbit to bytes */
  sps->width   = width  * 16 - crop_left - crop_right;
  sps->height  = height * 16 - crop_top  - crop_bottom;
  sps->valid   = 1;

  return 0;
}


int
h264_decode_pic_parameter_set(parser_es_t *st, bitstream_t *bs)
{
  h264_private_t *p;
  uint32_t pps_id, sps_id;

  if((p = st->es_priv) == NULL) {
    p = st->es_priv = calloc(1, sizeof(h264_private_t));
    p->start = mclk();
  }
  
  pps_id = read_golomb_ue(bs);
  if(pps_id >= MAX_PPS_COUNT)
    return 0;

  sps_id = read_golomb_ue(bs);
  if(sps_id >= MAX_SPS_COUNT)
    return -1;
  if (!p->sps[sps_id].valid)
    return -1;

  p->pps[pps_id].sps_id = sps_id;
  p->pps[pps_id].valid = 1;
  return 0;
}


int
h264_decode_slice_header(parser_es_t *st, bitstream_t *bs, int *pkttype,
			 int *isfield)
{
  h264_private_t *p;
  h264_pps_t *pps;
  h264_sps_t *sps;
  uint32_t slice_type, pps_id, width, height, v;
  uint64_t d;

  *pkttype = 0;
  *isfield = 0;

  if ((p = st->es_priv) == NULL)
    return -1;

  read_golomb_ue(bs); /* first_mb_in_slice */
  slice_type = read_golomb_ue(bs);

  if (slice_type > 4)
    slice_type -= 5;  /* Fixed slice type per frame */

  pps_id = read_golomb_ue(bs);
  if (pps_id >= MAX_PPS_COUNT)
    return -1;
  pps = &p->pps[pps_id];
  if (!pps->valid)
    return -1;
  sps = &p->sps[pps->sps_id];
  if (!sps->valid)
    return -1;

  if (!sps->max_frame_num_bits)
    return -1;

  switch(slice_type) {
  case 0:
    *pkttype = PKT_P_FRAME;
    break;
  case 1:
    *pkttype = PKT_B_FRAME;
    break;
  case 2:
    *pkttype = PKT_I_FRAME;
    break;
  default:
    return -1;
  }

  skip_bits(bs, sps->max_frame_num_bits);

  if (!sps->mbs_only_flag)
    if (read_bits1(bs)) {
      skip_bits1(bs); // bottom field
      *isfield = 1;
    }

  d = 0;
  if (sps->time_scale)
    d = 180000 * (uint64_t)sps->units_in_tick / (uint64_t)sps->time_scale;
  if (d == 0 && st->es_frame_duration < 2 && p->start + sec2mono(4) < mclk()) {
    tvhwarn(LS_PARSER, "H264 stream has not timing information, using 30fps");
    d = 3000; /* 90000/30 = 3000 : 30fps */
  }

#if 0
  if (sps->cbpsize)
    st->es_vbv_size = sps->cbpsize;
#endif

  width  = sps->width;
  height = sps->height * (2 - sps->mbs_only_flag);

  if (width && height && d)
    parser_set_stream_vparam(st, width, height, d);

  if (sps->aspect_num && sps->aspect_den) {
    width  *= sps->aspect_num;
    height *= sps->aspect_den;
    if (width && height) {
      v = gcdU32(width, height);
      st->es_aspect_num = width / v;
      st->es_aspect_den = height / v;
    }
  } else {
    st->es_aspect_num = 0;
    st->es_aspect_den = 1;
  }

  return 0;
}
