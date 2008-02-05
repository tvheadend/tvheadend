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
#include "buffer.h"
#include "bitstream.h"

#if 0
/**
 * H.264 parser, nal constructor
 */
static int
build_nal(bitstream_t *bs, uint8_t *data, int size, int maxsize)
{
  int rbsp_size, i;
  
  bs->data = malloc(maxsize);

  /* Escape 0x000003 into 0x0000 */

  rbsp_size = 0;
  for(i = 1; i < size; i++) {
    if(i + 2 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 1] == 3) {
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

#endif


typedef struct h264_private {

    struct {
	int frame_duration;

    } sps[256];

} h264_private_t;




static int 
decode_vui(th_stream_t *st, bitstream_t *bs, int spsid)
{
  int units_in_tick;
  int time_scale;
  int fixed_rate;

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

  if(!read_bits1(bs)) /* We want timing info */
    return -1;
    
  units_in_tick = read_bits(bs, 32);
  time_scale    = read_bits(bs, 32);
  fixed_rate    = read_bits1(bs);

  printf("units_in_tick = %d\n", units_in_tick);
  printf("time_scale = %d\n", time_scale);
  printf("fixed_rate = %d\n", fixed_rate);

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
  //    h264_private_t *priv = st->st_parser_private;
  int profile_idc, level_idc, poc_type;
  unsigned int sps_id, tmp, i;

  profile_idc= read_bits(bs, 8);
  read_bits1(bs);   //constraint_set0_flag
  read_bits1(bs);   //constraint_set1_flag
  read_bits1(bs);   //constraint_set2_flag
  read_bits1(bs);   //constraint_set3_flag
  read_bits(bs, 4); // reserved
  level_idc= read_bits(bs, 8);
  sps_id= read_golomb_ue(bs);

  printf("profile = %d\n", profile_idc);
  printf("level_idc = %d\n", level_idc);
  printf("sps_id = %d\n", sps_id);


  if(profile_idc >= 100){ //high profile
    if(read_golomb_ue(bs) == 3) //chroma_format_idc
      read_bits1(bs);  //residual_color_transform_flag
    read_golomb_ue(bs);  //bit_depth_luma_minus8
    read_golomb_ue(bs);  //bit_depth_chroma_minus8
    read_bits1(bs);

    if(read_bits1(bs)) {
      printf("scaling\n");
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
  printf("maxframenum = %d\n", tmp);
  poc_type= read_golomb_ue(bs);
  printf("poc_type = %d\n", poc_type);

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
    printf("Illegal POC type %d\n", poc_type);
    /* Illegal poc */
    return -1;
  }

  tmp = read_golomb_ue(bs);
  printf("reference frames = %d\n", tmp);

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
