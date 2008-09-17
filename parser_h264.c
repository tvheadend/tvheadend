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

#include "tvhead.h"
#include "parsers.h"
#include "parser_h264.h"
#include "bitstream.h"

/**
 * H.264 parser, nal escaper
 */
int
h264_nal_deescape(bitstream_t *bs, uint8_t *data, int size)
{
  int rbsp_size, i;
  
  bs->data = malloc(size);

  /* Escape 0x000003 into 0x0000 */

  rbsp_size = 0;
  for(i = 1; i < size; i++) {
    if(i + 2 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 3) {
      bs->data[rbsp_size++] = 0;
      bs->data[rbsp_size++] = 0;
      i += 2;
    } else {
      bs->data[rbsp_size++] = data[i];
    }
  }

  bs->offset = 0;
  bs->len = rbsp_size * 8;
  return 0;
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



typedef struct h264_private {

  struct {
    int frame_duration;
    int cbpsize;
  } sps[256];
  
  struct {
    int sps;
  } pps[256];

} h264_private_t;




static int 
decode_vui(th_stream_t *st, bitstream_t *bs, int spsid)
{
  int units_in_tick;
  int time_scale;
  int fixed_rate;
  return 0;

  if(read_bits1(bs)) {
    if(read_bits(bs, 8) == 255) {
      read_bits(bs, 16);
      read_bits(bs, 16);
    }
  }

  if(read_bits1(bs)){      /* overscan_info_present_flag */
    read_bits1(bs);      /* overscan_appropriate_flag */
  }

  if(read_bits1(bs)){      /* video_signal_type_present_flag */
    read_bits(bs, 3);    /* video_format */
    read_bits1(bs);      /* video_full_range_flag */
    if(read_bits1(bs)){  /* colour_description_present_flag */
      read_bits(bs, 8); /* colour_primaries */
      read_bits(bs, 8); /* transfer_characteristics */
      read_bits(bs, 8); /* matrix_coefficients */
    }
  }

  if(read_bits1(bs)){      /* chroma_location_info_present_flag */
    read_golomb_ue(bs);  /* chroma_sample_location_type_top_field */
    read_golomb_ue(bs);  /* chroma_sample_location_type_bottom_field */
  }

  if(!read_bits1(bs)) /* We need timing info */
    return -1;
    
  units_in_tick = read_bits(bs, 32);
  time_scale    = read_bits(bs, 32);
  fixed_rate    = read_bits1(bs);
#if 0
  printf("units_in_tick = %d\n", units_in_tick);
  printf("time_scale = %d\n", time_scale);
  printf("fixed_rate = %d\n", fixed_rate);
#endif
  return 0;
}



static void 
decode_scaling_list(bitstream_t *bs, int size)
{
  int i, last = 8, next = 8;
  if(!read_bits1(bs))
    return; /* matrix not written */

  for(i=0;i<size;i++){
    if(next)
      next = (last + read_golomb_se(bs)) & 0xff;
    if(!i && !next)
      break;  /* matrix not written */
    last = next ? next : last;
  }
}


int
h264_decode_seq_parameter_set(th_stream_t *st, bitstream_t *bs)
{
  int profile_idc, level_idc, poc_type;
  unsigned int sps_id, tmp, i;
  int cbpsize = -1;
  h264_private_t *p;

  if((p = st->st_priv) == NULL)
    p = st->st_priv = calloc(1, sizeof(h264_private_t));

  profile_idc= read_bits(bs, 8);
  read_bits1(bs);   //constraint_set0_flag
  read_bits1(bs);   //constraint_set1_flag
  read_bits1(bs);   //constraint_set2_flag
  read_bits1(bs);   //constraint_set3_flag
  read_bits(bs, 4); // reserved
  level_idc= read_bits(bs, 8);
  sps_id= read_golomb_ue(bs);


  i = 0;
  while(h264_lev2cpbsize[i][0] != -1) {
    if(h264_lev2cpbsize[i][0] >= level_idc) {
      cbpsize = h264_lev2cpbsize[i][1];
      break;
    }
    i++;
  }
  if(cbpsize < 0)
    return -1;

  p->sps[sps_id].cbpsize = cbpsize * 125; /* Convert from kbit to bytes */


  if(profile_idc >= 100){ //high profile
    if(read_golomb_ue(bs) == 3) //chroma_format_idc
      read_bits1(bs);  //residual_color_transform_flag
    read_golomb_ue(bs);  //bit_depth_luma_minus8
    read_golomb_ue(bs);  //bit_depth_chroma_minus8
    read_bits1(bs);

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

  tmp = read_golomb_ue(bs); /* max frame num */
  poc_type= read_golomb_ue(bs);
 
  if(poc_type == 0){ //FIXME #define
    read_golomb_ue(bs);
  } else if(poc_type == 1){//FIXME #define
    read_bits1(bs);
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

  tmp = read_golomb_ue(bs) + 1; /* mb width */
  tmp = read_golomb_ue(bs) + 1; /* mb height */

  if(!read_bits1(bs))
    read_bits1(bs);

  read_bits1(bs);

  /* CROP */
  if(read_bits1(bs)){
    tmp = read_golomb_ue(bs);
    tmp = read_golomb_ue(bs);
    tmp = read_golomb_ue(bs);
    tmp = read_golomb_ue(bs);
  }

  if(read_bits1(bs)) {
    decode_vui(st, bs, sps_id);
    return 0;
  } else {
    return -1;
  }
}


int
h264_decode_pic_parameter_set(th_stream_t *st, bitstream_t *bs)
{
  h264_private_t *p;
  int pps_id, sps_id;

  if((p = st->st_priv) == NULL)
    p = st->st_priv = calloc(1, sizeof(h264_private_t));
  
  pps_id = read_golomb_ue(bs);
  sps_id = read_golomb_ue(bs);
  p->pps[pps_id].sps = sps_id;
  return 0;
}


int
h264_decode_slice_header(th_stream_t *st, bitstream_t *bs, int *pkttype)
{
  h264_private_t *p;
  int slice_type, pps_id, sps_id;

  if((p = st->st_priv) == NULL)
    return -1;

  read_golomb_ue(bs); /* first_mb_in_slice */
  slice_type = read_golomb_ue(bs);
  
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

  pps_id = read_golomb_ue(bs);
  sps_id = p->pps[pps_id].sps;
  if(p->sps[sps_id].cbpsize == 0)
    return -1;

  st->st_vbv_size = p->sps[sps_id].cbpsize;
  st->st_vbv_delay = -1;
  return 0;
}
