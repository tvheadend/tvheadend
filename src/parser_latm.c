/*
 * LATM / AAC Parser
 *
 * Copyright (C) 2009 Andreas Öman
 *
 * Based on LATM Parser patch sent to ffmpeg-devel@
 * copyright (c) 2009 Paul Kendall <paul@kcbbs.gen.nz>
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
#include "parser_latm.h"
#include "bitstream.h"

static const int mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};


typedef struct latm_private {

  int configured;
  int audio_mux_version_A;
  int frame_length_type;

  int sample_rate_index;
  int channel_config;

} latm_private_t;



static uint32_t
latm_get_value(bitstream_t *bs)
{
    return read_bits(bs, read_bits(bs, 2) * 8);
}


static void
read_audio_specific_config(latm_private_t *latm,  bitstream_t *bs)
{
  int aot;

  aot = read_bits(bs, 5);
  if(aot != 2)
    return;

  latm->sample_rate_index = read_bits(bs, 4);
 
  if(latm->sample_rate_index == 0xf)
    return;

  latm->channel_config = read_bits(bs, 4);

  skip_bits(bs, 1); //framelen_flag
  if(read_bits1(bs)) { // depends_on_coder
    skip_bits(bs, 14);                   // delay
  }
  skip_bits(bs, 1); // ext_flag
}


static void
read_stream_mux_config(latm_private_t *latm, bitstream_t *bs)
{
  int audio_mux_version = read_bits(bs, 1);
  latm->audio_mux_version_A = 0;
  if(audio_mux_version)                       // audioMuxVersion
    latm->audio_mux_version_A = read_bits(bs, 1);

  if(!latm->audio_mux_version_A) {

    if(audio_mux_version)
      latm_get_value(bs);                  // taraFullness

    read_bits(bs, 1);                         // allStreamSameTimeFraming = 1
    read_bits(bs, 6);                         // numSubFrames = 0
    read_bits(bs, 4);                         // numPrograms = 0

    // for each program (which there is only on in DVB)
    read_bits(bs, 3);                         // numLayer = 0
    
    // for each layer (which there is only on in DVB)
    if (!audio_mux_version)
      read_audio_specific_config(latm, bs);
    else {
      uint32_t ascLen = latm_get_value(bs);
      abort(); // ascLen -= read_audio_specific_config(filter, gb);
      skip_bits(bs, ascLen);
    }

    // these are not needed... perhaps
    latm->frame_length_type = read_bits(bs, 3);
    switch(latm->frame_length_type) {
    case 0:
      read_bits(bs, 8);
      break;
    case 1:
      read_bits(bs, 9);
      break;
    case 3:
    case 4:
    case 5:
      read_bits(bs, 6);                 // celp_table_index
      break;
    case 6:
    case 7:
      read_bits(bs, 1);                 // hvxc_table_index
    }

    if (read_bits(bs, 1)) {                   // other data?
      if (audio_mux_version)
	latm_get_value(bs);              // other_data_bits
      else {
	int esc;
	do {
	  esc = read_bits(bs, 1);
	  read_bits(bs, 8);
	} while (esc);
      }
    }

    if (read_bits(bs, 1))                     // crc present?
      read_bits(bs, 8);                     // config_crc
  }
}



/**
 * Parse AAC LATM
 */
void
parse_latm_audio_mux_element(th_transport_t *t, th_stream_t *st, uint8_t *data,
			     int len)
{
  latm_private_t *latm;

  bitstream_t bs;
  int slot_len, tmp;

  init_bits(&bs, data, len);

  if((latm = st->st_priv) == NULL)
    latm = st->st_priv = calloc(1, sizeof(latm_private_t));

  if(!read_bits1(&bs)) {
    latm->configured = 1;
    read_stream_mux_config(latm, &bs);
  }

  if(!latm->configured)
    return;

  if(latm->frame_length_type != 0)
    return;

  slot_len = 0;
  do {
    tmp = read_bits(&bs, 8);
    slot_len += tmp;
  } while (tmp == 255);

  printf("TYPE = %d, muxslotlen = %d\n", 
	 read_bits(&bs, 3), slot_len);

}
