/*
 *  Multicasted IPTV Input
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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <ffmpeg/common.h>

#include "tvhead.h"
#include "psi.h"
#include "transports.h"
#include "dvb_support.h"
#include "tsdemux.h"

int
psi_section_reassemble(psi_section_t *ps, uint8_t *data, int len, 
		       int start, int chkcrc)
{
  int remain, a, tsize;

  if(start)
    ps->ps_offset = 0;

  if(ps->ps_offset < 0)
    return -1;

  remain = PSI_SECTION_SIZE - ps->ps_offset;

  a = FFMAX(0, FFMIN(remain, len));

  memcpy(ps->ps_data + ps->ps_offset, data, a);
  ps->ps_offset += a;
  tsize = 3 + (((ps->ps_data[1] & 0xf) << 8) | ps->ps_data[2]);
  if(ps->ps_offset < tsize)
    return -1;

  if(chkcrc && psi_crc32(ps->ps_data, tsize))
    return -1;

  ps->ps_offset = tsize - (chkcrc ? 4 : 0);
  return 0;
}






/** 
 * PAT parser, from ISO 13818-1
 */

int
psi_parse_pat(th_transport_t *t, uint8_t *ptr, int len,
	      pid_section_callback_t *pmt_callback)
{
  uint16_t prognum;
  uint16_t pid;
  th_stream_t *st;

  if(len < 5)
    return -1;

  ptr += 5;
  len -= 5;

  while(len >= 4) {
    
    prognum =  ptr[0]         << 8 | ptr[1];
    pid     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      st = transport_add_stream(t, pid, HTSTV_TABLE);
      st->st_got_section = pmt_callback;

    }
    ptr += 4;
    len -= 4;
  }
  return 0;
}


/**
 * Append CRC
 */

static int
psi_append_crc32(uint8_t *buf, int offset, int maxlen)
{
  uint32_t crc;

  if(offset + 4 > maxlen)
    return -1;

  crc = psi_crc32(buf, offset);

  buf[offset + 0] = crc >> 24;
  buf[offset + 1] = crc >> 16;
  buf[offset + 2] = crc >> 8;
  buf[offset + 3] = crc;

  assert(psi_crc32(buf, offset + 4) == 0);

  return offset + 4;
}


/** 
 * PAT generator
 */

int
psi_build_pat(th_transport_t *t, uint8_t *buf, int maxlen)
{
  if(maxlen < 12)
    return -1;

  buf[0] = 0;
  buf[1] = 0xb0;       /* reserved */
  buf[2] = 12 + 4 - 3; /* Length */

  buf[3] = 0x00; /* transport stream id */
  buf[4] = 0x01;

  buf[5] = 0xc1; /* reserved + current_next_indicator + version */
  buf[6] = 0;
  buf[7] = 0;

  buf[8] = 0;    /* Program number, we only have one program */
  buf[9] = 1;

  buf[10] = 0;   /* PMT pid */
  buf[11] = 100;

  return psi_append_crc32(buf, 12, maxlen);
}







/** 
 * PMT parser, from ISO 13818-1 and ETSI EN 300 468
 */

int
psi_parse_pmt(th_transport_t *t, uint8_t *ptr, int len, int chksvcid)
{
  uint16_t pcr_pid, pid;
  uint8_t estype;
  int dllen;
  uint8_t dtag, dlen;
  uint16_t sid;
  tv_streamtype_t hts_stream_type;

  if(len < 9)
    return -1;

  sid     = ptr[0] << 8 | ptr[1];
  pcr_pid = (ptr[5] & 0x1f) << 8 | ptr[6];
  dllen   = (ptr[7] & 0xf) << 8 | ptr[8];
  
  t->tht_pcr_pid = pcr_pid;

  ptr += 9;
  len -= 9;

  if(chksvcid && sid != t->tht_dvb_service_id) {
    printf("the service id is invalid\n");
    return -1;
  }
  while(dllen > 1) {
    dtag = ptr[0];
    dlen = ptr[1];

    len -= 2; ptr += 2; dllen -= 2; 
    if(dlen > len)
      break;

    len -= dlen; ptr += dlen; dllen -= dlen;
  }

  while(len >= 5) {
    estype  = ptr[0];
    pid     = (ptr[1] & 0x1f) << 8 | ptr[2];
    dllen   = (ptr[3] & 0xf) << 8 | ptr[4];

    ptr += 5;
    len -= 5;

    hts_stream_type = 0;

    switch(estype) {
    case 0x01:
    case 0x02:
      hts_stream_type = HTSTV_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
    case 0x81:
      hts_stream_type = HTSTV_MPEG2AUDIO;
      break;

    case 0x1b:
      hts_stream_type = HTSTV_H264;
      break;
    }

    while(dllen > 1) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 
      if(dlen > len)
	break;

      switch(dtag) {
      case DVB_DESC_TELETEXT:
	if(estype == 0x06)
	  hts_stream_type = HTSTV_TELETEXT;
	break;

      case DVB_DESC_SUBTITLE:
	break;

      case DVB_DESC_AC3:
	if(estype == 0x06 || estype == 0x81)
	  hts_stream_type = HTSTV_AC3;
	break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    if(hts_stream_type != 0)
      transport_add_stream(t, pid, hts_stream_type);
  } 

  t->tht_pmt_seen = 1;
  return 0;
}





/** 
 * PAT generator
 */

int
psi_build_pmt(th_muxer_t *tm, uint8_t *buf0, int maxlen)
{
  th_stream_t *st;
  th_muxstream_t *tms;
  int c, tlen, dlen, l;
  uint8_t *buf, *buf1;
  

  buf = buf0;

  if(maxlen < 12)
    return -1;

  buf[0] = 2; /* table id, always 2 */

  buf[3] = 0x00; /* program id */
  buf[4] = 0x01;

  buf[5] = 0xc1; /* current_next_indicator + version */
  buf[6] = 0;
  buf[7] = 0;

  /* Find PID that carries PCR */

  LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link)
    if(tms->tms_dopcr)
      break;
  
  if(tms == NULL) {
    buf[8] = 0xff;
    buf[9] = 0xff;
  } else {
    buf[8] = 0xe0 | (tms->tms_index >> 8);
    buf[9] =         tms->tms_index;
  }

  buf[10] = 0xf0; /* Program info length */
  buf[11] = 0x00; /* We dont have any such things atm */

  buf += 12;
  tlen = 12;

  LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link) {
    st = tms->tms_stream;

    switch(st->st_type) {
    case HTSTV_MPEG2VIDEO:
      c = 0x02;
      break;

    case HTSTV_MPEG2AUDIO:
      c = 0x04;
      break;

    case HTSTV_H264:
      c = 0x1b;
      break;

    case HTSTV_AC3:
      c = 0x06;
      break;

    default:
      continue;
    }


    buf[0] = c;
    buf[1] = 0xe0 | (tms->tms_index >> 8);
    buf[2] =         tms->tms_index;

    buf1 = &buf[3];
    tlen += 5;
    buf  += 5;
    dlen = 0;

    switch(st->st_type) {
    case HTSTV_AC3:
      buf[0] = DVB_DESC_AC3;
      buf[1] = 1;
      buf[2] = 0; /* XXX: generate real AC3 desc */
      dlen = 3;
      break;
    default:
      break;
    }

    tlen += dlen;
    buf  += dlen;

    buf1[0] = 0xf0 | (dlen >> 8);
    buf1[1] =         dlen;
  }

  l = tlen - 3 + 4;

  buf0[1] = 0xb0 | (l >> 8);
  buf0[2] =         l;

  return psi_append_crc32(buf0, tlen, maxlen);
}





/*
 * CRC32 
 */
static uint32_t crc_tab[256] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
  0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
  0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
  0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
  0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
  0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
  0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
  0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
  0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
  0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
  0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
  0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
  0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
  0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
  0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
  0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
  0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
  0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
  0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
  0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
  0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
  0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t
psi_crc32(uint8_t *data, size_t datalen)
{
  uint32_t crc = 0xffffffff;

  while(datalen--)
    crc = (crc << 8) ^ crc_tab[((crc >> 24) ^ *data++) & 0xff];

  return crc;
}
