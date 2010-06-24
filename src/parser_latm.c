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
#include <assert.h>

#include "tvhead.h"
#include "parsers.h"
#include "parser_latm.h"
#include "bitstream.h"


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
read_audio_specific_config(th_stream_t *st, latm_private_t *latm,
			   bitstream_t *bs)
{
  int aot, sr;

  aot = read_bits(bs, 5);
  if(aot != 2)
    return;

  latm->sample_rate_index = read_bits(bs, 4);

  if(latm->sample_rate_index == 0xf)
    return;

  sr = sri_to_rate(latm->sample_rate_index);
  st->st_frame_duration = 1024 * 90000 / sr;

  latm->channel_config = read_bits(bs, 4);

  skip_bits(bs, 1); //framelen_flag
  if(read_bits1(bs))  // depends_on_coder
    skip_bits(bs, 14);

  if(read_bits(bs, 1))  // ext_flag
     skip_bits(bs, 1);  // ext3_flag
}


static void
read_stream_mux_config(th_stream_t *st, latm_private_t *latm, bitstream_t *bs)
{
  int audio_mux_version = read_bits(bs, 1);
  latm->audio_mux_version_A = 0;
  if(audio_mux_version)                       // audioMuxVersion
    latm->audio_mux_version_A = read_bits(bs, 1);
  
  if(latm->audio_mux_version_A)
    return;

  if(audio_mux_version)
    latm_get_value(bs);                  // taraFullness

  skip_bits(bs, 1);                         // allStreamSameTimeFraming = 1
  skip_bits(bs, 6);                         // numSubFrames = 0
  skip_bits(bs, 4);                         // numPrograms = 0

  // for each program (which there is only on in DVB)
  skip_bits(bs, 3);                         // numLayer = 0
    
  // for each layer (which there is only on in DVB)
  if(!audio_mux_version)
    read_audio_specific_config(st, latm, bs);
  else {
    return;
#if 0
    uint32_t ascLen = latm_get_value(bs);
    abort(); // ascLen -= read_audio_specific_config(filter, gb);
    skip_bits(bs, ascLen);
#endif
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
    break;
  }

  if(read_bits(bs, 1)) {                   // other data?
    if(audio_mux_version)
      latm_get_value(bs);              // other_data_bits
    else {
      int esc;
      do {
	esc = read_bits(bs, 1);
	skip_bits(bs, 8);
      } while (esc);
    }
  }

  if(read_bits(bs, 1))                   // crc present?
    skip_bits(bs, 8);                     // config_crc
  latm->configured = 1;
}

/**
 * Parse AAC LATM
 */
th_pkt_t *
parse_latm_audio_mux_element(th_transport_t *t, th_stream_t *st, uint8_t *data,
			     int len)
{
  latm_private_t *latm;
  bitstream_t bs, out;
  int slot_len, tmp, i;
  uint8_t *buf;

  init_bits(&bs, data, len * 8);

  if((latm = st->st_priv) == NULL)
    latm = st->st_priv = calloc(1, sizeof(latm_private_t));

  if(!read_bits1(&bs))
    read_stream_mux_config(st, latm, &bs);

  if(!latm->configured)
    return NULL;

  if(latm->frame_length_type != 0)
    return NULL;

  slot_len = 0;
  do {
    tmp = read_bits(&bs, 8);
    slot_len += tmp;
  } while (tmp == 255);


  if(slot_len * 8 > remaining_bits(&bs))
    return NULL;

  if(st->st_curdts == PTS_UNSET)
    return NULL;

  th_pkt_t *pkt = pkt_alloc(NULL, slot_len + 7, st->st_curdts, st->st_curdts);

  pkt->pkt_commercial = t->tht_tt_commercial_advice;
  pkt->pkt_duration = st->st_frame_duration;
  pkt->pkt_sri = latm->sample_rate_index;
  pkt->pkt_channels = latm->channel_config;

  /* 7 bytes of ADTS header */
  init_bits(&out, pktbuf_ptr(pkt->pkt_payload), 56);

  put_bits(&out, 0xfff, 12); // Sync marker
  put_bits(&out, 0, 1);      // ID 0 = MPEG 4
  put_bits(&out, 0, 2);      // Layer
  put_bits(&out, 1, 1);      // Protection absent
  put_bits(&out, 2, 2);      // AOT
  put_bits(&out, latm->sample_rate_index, 4);
  put_bits(&out, 1, 1);      // Private bit
  put_bits(&out, latm->channel_config, 3);
  put_bits(&out, 1, 1);      // Original
  put_bits(&out, 1, 1);      // Copy
  
  put_bits(&out, 1, 1);      // Copyright identification bit
  put_bits(&out, 1, 1);      // Copyright identification start
  put_bits(&out, slot_len, 13); 
  put_bits(&out, 0, 11);     // Buffer fullness
  put_bits(&out, 0, 2);      // RDB in frame

  assert(remaining_bits(&out) == 0);

  /* AAC RDB */
  buf = pktbuf_ptr(pkt->pkt_payload) + 7;
  for(i = 0; i < slot_len; i++)
    *buf++ = read_bits(&bs, 8);

  st->st_curdts += st->st_frame_duration;

  return pkt;
}
