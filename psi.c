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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <ffmpeg/common.h>

#include "tvhead.h"
#include "psi.h"
#include "transports.h"
#include "dvb_support.h"

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

  if(chkcrc) {
    /* XXX: Add CRC check */
  }
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
  th_pid_t *pi;

  if(len < 5)
    return -1;

  ptr += 5;
  len -= 5;

  while(len >= 4) {
    
    prognum =  ptr[0]         << 8 | ptr[1];
    pid     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      pi = transport_add_pid(t, pid, HTSTV_TABLE);
      pi->tp_got_section = pmt_callback;

    }
    ptr += 4;
    len -= 4;
  }
  return 0;
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

  if(chksvcid && sid != t->tht_dvb_service_id)
    return -1;
  
  while(dllen > 2) {
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

    while(dllen > 2) {
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
      transport_add_pid(t, pid, hts_stream_type);
  } 
  return 0;
}






const char *
htstvstreamtype2txt(tv_streamtype_t s)
{
  switch(s) {
  case HTSTV_MPEG2VIDEO: return "mpeg2video";
  case HTSTV_MPEG2AUDIO: return "mpeg2audio";
  case HTSTV_H264:       return "h264";
  case HTSTV_AC3:        return "AC-3";
  case HTSTV_TELETEXT:   return "teletext";
  case HTSTV_SUBTITLES:  return "subtitles";
  case HTSTV_TABLE:      return "PSI table";
  default:               return "<unknown>";
  }
}

