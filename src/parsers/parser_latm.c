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

#include "tvheadend.h"
#include "parsers.h"
#include "parser_latm.h"
#include "bitstream.h"
#include "service.h"

typedef struct latm_private {

  int configured;
  int audio_mux_version_A;
  int aot;
  int frame_length_type;

  int sri;
  int ext_sri;
  int channel_config;

} latm_private_t;



static uint32_t
latm_get_value(bitstream_t *bs)
{
    return read_bits(bs, read_bits(bs, 2) * 8);
}

#define AOT_AAC_MAIN  1
#define AOT_AAC_LC    2
#define AOT_AAC_SSR   3
#define AOT_AAC_LTP   4
#define AOT_SBR       5
#define AOT_PS       26
#define AOT_ESCAPE   28

static inline int adts_aot(int aot)
{
  switch (aot) {
  case AOT_AAC_MAIN: return 0;
  case AOT_AAC_LC  : return 1;
  case AOT_AAC_SSR : return 2;
  default:           return 3; /* reserved or LTP */
  }
}

static inline int read_aot(bitstream_t *bs)
{
  int aot = read_bits(bs, 5);
  if (aot == AOT_ESCAPE)
    aot = read_bits(bs, 6);
  return aot;
}

static inline int read_sr(bitstream_t *bs, int *sri)
{
  *sri = read_bits(bs, 4);
  if (*sri == 0x0f)
    return read_bits(bs, 24);
  else
    return sri_to_rate(*sri);
}

static int
read_audio_specific_config(elementary_stream_t *st, latm_private_t *latm,
			   bitstream_t *bs)
{
  int aot, sr, sri;

  if ((bs->offset % 8) != 0)
    return -1;

  aot = read_aot(bs);
  sr = read_sr(bs, &latm->sri);
  latm->channel_config = read_bits(bs, 4);

  if (sr < 7350 || sr > 96000 ||
      latm->channel_config == 0 || latm->channel_config > 7)
    return -1;

  st->es_frame_duration = 1024 * 90000 / sr;

  latm->ext_sri = 0;
  if (aot == AOT_SBR ||
      (aot == AOT_PS && !(show_bits(bs, 3) & 3 && !(show_bits(bs, 9) & 0x3f)))) {
    sr  = read_sr(bs, &latm->ext_sri);
    if (sr < 7350 || sr > 96000)
      return -1;
    latm->ext_sri++;    // zero means "not set"
    aot = read_aot(bs); // this is the main object type (i.e. non-extended)
  }

  /* it's really unusual to use lower sample rates than 32000Hz */
  /* for the professional broadcasting, assume the SBR extension */
  if (latm->ext_sri == 0 && sr <= 24000) {
    sri = rate_to_sri(sr * 2);
    if (sri < 0)
      return -1;
    latm->ext_sri = sri + 1;
  }

  latm->aot = aot;

  if (read_bits1(bs))   // framelen_flag
    return -1;
  if (read_bits1(bs))   // depends_on_coder
    skip_bits(bs, 14);

  if (read_bits1(bs))   // ext_flag
     skip_bits(bs, 1);  // ext3_flag
  return 0;
}


static int
read_stream_mux_config(elementary_stream_t *st, latm_private_t *latm, bitstream_t *bs)
{
  int audio_mux_version = read_bits1(bs);
  latm->audio_mux_version_A = 0;
  if (audio_mux_version)                     // audioMuxVersion
    latm->audio_mux_version_A = read_bits1(bs);

  if (latm->audio_mux_version_A)
    return 0;

  if(audio_mux_version)
    latm_get_value(bs);                     // taraFullness

  skip_bits(bs, 1);                         // allStreamSameTimeFraming = 1
  skip_bits(bs, 6);                         // numSubFrames = 0

  if (read_bits(bs, 4))                     // numPrograms = 0
    return -1;

  // for each program (only one in DVB)
  if (read_bits(bs, 3))                     // numLayer = 0
    return -1;

  // for each layer (which there is only one in DVB)
  if(!audio_mux_version) {
    if (read_audio_specific_config(st, latm, bs) < 0)
      return -1;
  } else {
    return -1;
#if 0 // see bellow (look for dead code)
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

  if(read_bits1(bs)) {                // other data?
#if 0 // coverity - dead code - see above
    if(audio_mux_version)
      latm_get_value(bs);             // other_data_bits
    else
#endif
    {
      int esc;
      do {
	esc = read_bits1(bs);
	skip_bits(bs, 8);
      } while (esc);
    }
  }

  if(read_bits1(bs))                  // crc present?
    skip_bits(bs, 8);                 // config_crc
  latm->configured = 1;
  return 0;
}

/**
 * Parse AAC LATM
 */
th_pkt_t *
parse_latm_audio_mux_element(service_t *t, elementary_stream_t *st, 
			     const uint8_t *data, int len)
{
  latm_private_t *latm;
  bitstream_t bs, out;
  int slot_len, tmp, i;
  uint8_t *buf;

  init_rbits(&bs, data, len * 8);

  if((latm = st->es_priv) == NULL)
    latm = st->es_priv = calloc(1, sizeof(latm_private_t));

  if(!read_bits1(&bs))
    if (read_stream_mux_config(st, latm, &bs) < 0)
      return NULL;

  if(!latm->configured)
    return NULL;

  if(latm->frame_length_type != 0)
    return NULL;

  slot_len = 0;
  do {
    tmp = read_bits(&bs, 8);
    slot_len += tmp;
  } while (tmp == 255);

  if (slot_len * 8 > remaining_bits(&bs))
    return NULL;

  if(st->es_curdts == PTS_UNSET)
    return NULL;

  th_pkt_t *pkt = pkt_alloc(NULL, slot_len + 7, st->es_curdts, st->es_curdts);

  pkt->pkt_commercial = t->s_tt_commercial_advice;
  pkt->pkt_duration   = st->es_frame_duration;
  pkt->pkt_sri        = latm->sri;
  pkt->pkt_ext_sri    = latm->ext_sri;
  pkt->pkt_channels   = latm->channel_config == 7 ? 8 : latm->channel_config;

  /* 7 bytes of ADTS header */
  init_wbits(&out, pktbuf_ptr(pkt->pkt_payload), 7 * 8);

  put_bits(&out, 0xfff, 12); // Sync marker
  put_bits(&out, 0, 1);      // ID 0 = MPEG 4, 1 = MPEG 2
  put_bits(&out, 0, 2);      // Layer
  put_bits(&out, 1, 1);      // Protection absent
  put_bits(&out, adts_aot(latm->aot), 2);
  put_bits(&out, latm->sri, 4);
  put_bits(&out, 1, 1);      // Private bit
  put_bits(&out, latm->channel_config, 3);
  put_bits(&out, 1, 1);      // Original
  put_bits(&out, 1, 1);      // Copy
  
  put_bits(&out, 1, 1);      // Copyright identification bit
  put_bits(&out, 1, 1);      // Copyright identification start
  put_bits(&out, slot_len + 7, 13);
  put_bits(&out, 0x7ff, 11); // Buffer fullness
  put_bits(&out, 0, 2);      // RDB in frame

  assert(remaining_bits(&out) == 0);

  /* AAC RDB */
  buf = pktbuf_ptr(pkt->pkt_payload) + 7;
  for(i = 0; i < slot_len; i++)
    *buf++ = read_bits(&bs, 8);

  st->es_curdts += st->es_frame_duration;

  return pkt;
}
