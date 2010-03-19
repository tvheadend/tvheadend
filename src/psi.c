/*
 *  MPEG TS Program Specific Information code
 *  Copyright (C) 2007 - 2010 Andreas Öman
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

#include <libavutil/common.h>
#include <libavutil/avstring.h>

#include "tvhead.h"
#include "psi.h"
#include "transports.h"
#include "dvb/dvb_support.h"
#include "tsdemux.h"
#include "parsers.h"

static int
psi_section_reassemble0(psi_section_t *ps, const uint8_t *data, 
			int len, int start, int crc,
			section_handler_t *cb, void *opaque)
{
  int excess, tsize;

  if(start) {
    // Payload unit start indicator
    ps->ps_offset = 0;
    ps->ps_lock = 1;
  }

  if(!ps->ps_lock)
    return -1;

  memcpy(ps->ps_data + ps->ps_offset, data, len);
  ps->ps_offset += len;

  if(ps->ps_offset < 3) {
    /* We don't know the total length yet */
    return len;
  }

  tsize = 3 + (((ps->ps_data[1] & 0xf) << 8) | ps->ps_data[2]);
 
  if(ps->ps_offset < tsize)
    return len; // Not there yet
  
  excess = ps->ps_offset - tsize;

  if(crc && psi_crc32(ps->ps_data, tsize))
    return -1;

  cb(ps->ps_data, tsize - (crc ? 4 : 0), opaque);
  ps->ps_offset = 0;
  return len - excess;
}


/**
 *
 */
void
psi_section_reassemble(psi_section_t *ps, const uint8_t *tsb, int crc,
		       section_handler_t *cb, void *opaque)
{
  int off = tsb[3] & 0x20 ? tsb[4] + 5 : 4;
  int pusi = tsb[1] & 0x40;
  int r;

  if(off >= 188) {
    ps->ps_lock = 0;
    return;
  }
  
  if(pusi) {
    int len = tsb[off++];
    if(len > 0) {
      if(len > 188 - off) {
	ps->ps_lock = 0;
	return;
      }
      psi_section_reassemble0(ps, tsb + off, len, 0, crc, cb, opaque);
      off += len;
    }
  }

  while(off < 188 && tsb[off] != 0xff) {
    r = psi_section_reassemble0(ps, tsb + off, 188 - off, pusi, crc,
				cb, opaque);
    if(r < 0) {
      ps->ps_lock = 0;
      break;
    }
    off += r;
    pusi = 0;
  }
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

  lock_assert(&t->tht_stream_mutex);

  if(len < 5)
    return -1;

  ptr += 5;
  len -= 5;

  while(len >= 4) {
    
    prognum =  ptr[0]         << 8 | ptr[1];
    pid     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      if(transport_stream_find(t, pid) == NULL) {
	st = transport_stream_create(t, pid, SCT_PMT);
	st->st_section_docrc = 1;
	st->st_got_section = pmt_callback;
      }
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
psi_build_pat(th_transport_t *t, uint8_t *buf, int maxlen, int pmtpid)
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

  buf[10] = 0xe0 | (pmtpid >> 8);
  buf[11] =         pmtpid;

  return psi_append_crc32(buf, 12, maxlen);
}


/**
 * PMT update reason flags
 */
#define PMT_UPDATE_PCR                0x1
#define PMT_UPDATE_NEW_STREAM         0x2
#define PMT_UPDATE_LANGUAGE           0x4
#define PMT_UPDATE_FRAME_DURATION     0x8
#define PMT_UPDATE_COMPOSITION_ID     0x10
#define PMT_UPDATE_ANCILLARY_ID       0x20
#define PMT_UPDATE_STREAM_DELETED     0x40
#define PMT_UPDATE_NEW_CA_STREAM      0x80
#define PMT_UPDATE_NEW_CAID           0x100
#define PMT_UPDATE_CA_PROVIDER_CHANGE 0x200
#define PMT_UPDATE_PARENT_PID         0x400
#define PMT_UPDATE_CAID_DELETED       0x800

/**
 * Add a CA descriptor
 */
static int
psi_desc_add_ca(th_transport_t *t, uint16_t caid, uint32_t provid, uint16_t pid)
{
  th_stream_t *st;
  caid_t *c;
  int r = 0;

  if((st = transport_stream_find(t, pid)) == NULL) {
    st = transport_stream_create(t, pid, SCT_CA);
    r |= PMT_UPDATE_NEW_CA_STREAM;
  }

  st->st_delete_me = 0;

  LIST_FOREACH(c, &st->st_caids, link) {
    if(c->caid == caid) {
      c->delete_me = 0;

      if(c->providerid != provid) {
	c->providerid = provid;
	r |= PMT_UPDATE_CA_PROVIDER_CHANGE;
      }
      return r;
    }
  }

  c = malloc(sizeof(caid_t));

  c->caid = caid;
  c->providerid = provid;
  
  c->delete_me = 0;
  LIST_INSERT_HEAD(&st->st_caids, c, link);
  r |= PMT_UPDATE_NEW_CAID;
  return r;
}

/**
 * Parser for CA descriptors
 */
static int 
psi_desc_ca(th_transport_t *t, const uint8_t *buffer, int size)
{
  int r = 0;
  int i;
  uint32_t provid = 0;
  uint16_t caid = (buffer[0] << 8) | buffer[1];
  uint16_t pid = ((buffer[2]&0x1F) << 8) | buffer[3];

#if 0
  printf("CA_DESC: ");
  for(i = 0; i < size; i++)
    printf("%02x.", buffer[i]);
  printf("\n");
#endif

  switch (caid & 0xFF00) {
  case 0x0100: // SECA/Mediaguard
    provid = (buffer[4] << 8) | buffer[5];

    //Add extra providers, if any
    for (i = 17; i < size; i += 15){
      uint16_t _pid = ((buffer[i]&0x1F) << 8) | buffer[i + 1];
      uint16_t _provid = (buffer[i + 2] << 8) | buffer[i + 3];

      r |= psi_desc_add_ca(t, caid, _provid, _pid);
    }
    break;
  case 0x0500:// Viaccess
    for (i = 4; i < size;) {
      unsigned char nano = buffer[i++];
      unsigned char nanolen = buffer[i++];

      if (nano == 0x14) {
        provid = (buffer[i] << 16) | (buffer[i + 1] << 8) | (buffer[i + 2] & 0xf0);
        break;
      }

      i += nanolen;
    }
    break;
  default:
    provid = 0;
    break;
  }

  r |= psi_desc_add_ca(t, caid, provid, pid);

  return r;
}

/**
 * Parser for teletext descriptor
 */
static int
psi_desc_teletext(th_transport_t *t, const uint8_t *ptr, int size,
		  int parent_pid)
{
  int r = 0;
  th_stream_t *st;

  while(size >= 5) {
    int page = (ptr[3] & 0x7 ?: 8) * 100 + (ptr[4] >> 4) * 10 + (ptr[4] & 0xf);
    int type = ptr[3] >> 3;

    if(type == 2 || type == 5) {
      // 2 = subtitle page, 5 = subtitle page [hearing impaired]

      // We put the teletext subtitle driven streams on a list of pids
      // higher than normal MPEG TS (0x2000 ++)
      int pid = PID_TELETEXT_BASE + page;
    
      if((st = transport_stream_find(t, pid)) == NULL) {
	r |= PMT_UPDATE_NEW_STREAM;
	st = transport_stream_create(t, pid, SCT_TEXTSUB);
      }

      st->st_delete_me = 0;

      if(memcmp(st->st_lang, ptr, 3)) {
	r |= PMT_UPDATE_LANGUAGE;
	memcpy(st->st_lang, ptr, 3);
      }

      if(st->st_parent_pid != parent_pid) {
	r |= PMT_UPDATE_PARENT_PID;
	st->st_parent_pid = parent_pid;
      }
    }
    ptr += 5;
    size -= 5;
  }
  return r;
}


/** 
 * PMT parser, from ISO 13818-1 and ETSI EN 300 468
 */
int
psi_parse_pmt(th_transport_t *t, const uint8_t *ptr, int len, int chksvcid,
	      int delete)
{
  uint16_t pcr_pid, pid;
  uint8_t estype;
  int dllen;
  uint8_t dtag, dlen;
  uint16_t sid;
  streaming_component_type_t hts_stream_type;
  th_stream_t *st, *next;
  char lang[4];
  int frameduration;
  int update = 0;
  int had_components;
  int composition_id;
  int ancillary_id;
  int version;
  caid_t *c, *cn;

  if(len < 9)
    return -1;

  lock_assert(&t->tht_stream_mutex);

  had_components = !!LIST_FIRST(&t->tht_components);

  sid     = ptr[0] << 8 | ptr[1];
  version = ptr[2] >> 1 & 0x1f;
  
  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  pcr_pid = (ptr[5] & 0x1f) << 8 | ptr[6];
  dllen   = (ptr[7] & 0xf) << 8 | ptr[8];
  
  if(chksvcid && sid != t->tht_dvb_service_id)
    return -1;

  if(t->tht_pcr_pid != pcr_pid) {
    t->tht_pcr_pid = pcr_pid;
    update |= PMT_UPDATE_PCR;
  }

  ptr += 9;
  len -= 9;

  /* Mark all streams for deletion */
  if(delete) {
    LIST_FOREACH(st, &t->tht_components, st_link) {
      st->st_delete_me = 1;

      LIST_FOREACH(c, &st->st_caids, link)
	c->delete_me = 1;

    }
  }

  while(dllen > 1) {
    dtag = ptr[0];
    dlen = ptr[1];

    len -= 2; ptr += 2; dllen -= 2; 
    if(dlen > len)
      break;

    switch(dtag) {
    case DVB_DESC_CA:
      update |= psi_desc_ca(t, ptr, dlen);
      break;

    default:
      break;
    }


    len -= dlen; ptr += dlen; dllen -= dlen;
  }

  while(len >= 5) {
    estype  = ptr[0];
    pid     = (ptr[1] & 0x1f) << 8 | ptr[2];
    dllen   = (ptr[3] & 0xf) << 8 | ptr[4];

    ptr += 5;
    len -= 5;

    frameduration = 0;
    hts_stream_type = SCT_UNKNOWN;
    memset(lang, 0, 4);
    composition_id = -1;
    ancillary_id = -1;

    switch(estype) {
    case 0x01:
    case 0x02:
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
    case 0x81:
      hts_stream_type = SCT_MPEG2AUDIO;
      break;

    case 0x11:
      hts_stream_type = SCT_AAC;
      break;

    case 0x1b:
      hts_stream_type = SCT_H264;
      break;

    default:
      break;
    }

    while(dllen > 1) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 
      if(dlen > len)
	break;

      switch(dtag) {
      case DVB_DESC_CA:
	update |= psi_desc_ca(t, ptr, dlen);
	break;

      case DVB_DESC_REGISTRATION:
	if(dlen == 4 && 
	   ptr[0] == 'A' && ptr[1] == 'C' && ptr[2] == '-' &&  ptr[3] == '3')
	  hts_stream_type = SCT_AC3;
	break;

      case DVB_DESC_VIDEO_STREAM:
	frameduration = mpeg2video_framedurations[(ptr[0] >> 3) & 0xf];
	break;

      case DVB_DESC_LANGUAGE:
	memcpy(lang, ptr, 3);
	break;

      case DVB_DESC_TELETEXT:
	if(estype == 0x06)
	  hts_stream_type = SCT_TELETEXT;
	
	update |= psi_desc_teletext(t, ptr, dlen, pid);
	break;

      case DVB_DESC_AC3:
	if(estype == 0x06 || estype == 0x81)
	  hts_stream_type = SCT_AC3;
	break;

      case DVB_DESC_AAC:
	if(estype == 0x11)
	  hts_stream_type = SCT_AAC;
	break;

      case DVB_DESC_SUBTITLE:
	if(dlen < 8)
	  break;

	memcpy(lang, ptr, 3);
	composition_id = ptr[4] << 8 | ptr[5];
	ancillary_id   = ptr[6] << 8 | ptr[7];
	hts_stream_type = SCT_DVBSUB;
	break;

      default:
	break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    if(hts_stream_type != SCT_UNKNOWN) {

      if((st = transport_stream_find(t, pid)) == NULL) {
	update |= PMT_UPDATE_NEW_STREAM;
	st = transport_stream_create(t, pid, hts_stream_type);
      }

      st->st_delete_me = 0;

      st->st_tb = (AVRational){1, 90000};

      if(memcmp(st->st_lang, lang, 4)) {
	update |= PMT_UPDATE_LANGUAGE;
	memcpy(st->st_lang, lang, 4);
      }

      if(st->st_frame_duration == 0 && frameduration != 0) {
	st->st_frame_duration = frameduration;
	update |= PMT_UPDATE_FRAME_DURATION;
      }

      if(composition_id != -1 && st->st_composition_id != composition_id) {
	st->st_composition_id = composition_id;
	update |= PMT_UPDATE_COMPOSITION_ID;
      }

      if(ancillary_id != -1 && st->st_ancillary_id != ancillary_id) {
	st->st_ancillary_id = ancillary_id;
	update |= PMT_UPDATE_ANCILLARY_ID;
      }
    }
  }

  /* Scan again to see if any streams should be deleted */
  for(st = LIST_FIRST(&t->tht_components); st != NULL; st = next) {
    next = LIST_NEXT(st, st_link);

    for(c = LIST_FIRST(&st->st_caids); c != NULL; c = cn) {
      cn = LIST_NEXT(c, link);
      if(c->delete_me) {
	LIST_REMOVE(c, link);
	free(c);
	update |= PMT_UPDATE_CAID_DELETED;
      }
    }


    if(st->st_delete_me) {
      transport_stream_destroy(t, st);
      update |= PMT_UPDATE_STREAM_DELETED;
    }
  }

  if(update) {
    tvhlog(LOG_DEBUG, "PSI", "Transport \"%s\" PMT (version %d) updated"
	   "%s%s%s%s%s%s%s%s%s%s%s%s",
	   transport_nicename(t), version,
	   update&PMT_UPDATE_PCR               ? ", PCR PID changed":"",
	   update&PMT_UPDATE_NEW_STREAM        ? ", New elementary stream":"",
	   update&PMT_UPDATE_LANGUAGE          ? ", Language changed":"",
	   update&PMT_UPDATE_FRAME_DURATION    ? ", Frame duration changed":"",
	   update&PMT_UPDATE_COMPOSITION_ID    ? ", Composition ID changed":"",
	   update&PMT_UPDATE_ANCILLARY_ID      ? ", Ancillary ID changed":"",
	   update&PMT_UPDATE_STREAM_DELETED    ? ", Stream deleted":"",
	   update&PMT_UPDATE_NEW_CA_STREAM     ? ", New CA stream":"",
	   update&PMT_UPDATE_NEW_CAID          ? ", New CAID":"",
	   update&PMT_UPDATE_CA_PROVIDER_CHANGE? ", CA provider changed":"",
	   update&PMT_UPDATE_PARENT_PID        ? ", Parent PID changed":"",
	   update&PMT_UPDATE_CAID_DELETED      ? ", CAID deleted":"");
    
    transport_request_save(t);

    // Only restart if something that our clients worry about did change
    if(update & !(PMT_UPDATE_NEW_CA_STREAM |
		  PMT_UPDATE_NEW_CAID |
		  PMT_UPDATE_CA_PROVIDER_CHANGE | 
		  PMT_UPDATE_CAID_DELETED)) {
      if(t->tht_status == TRANSPORT_RUNNING)
	transport_restart(t, had_components);
    }
  }
  return 0;
}


/** 
 * PMT generator
 */
int
psi_build_pmt(streaming_start_t *ss, uint8_t *buf0, int maxlen, int pcrpid)
{
  int c, tlen, dlen, l, i, pid;
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

  buf[8] = 0xe0 | (pcrpid >> 8);
  buf[9] =         pcrpid;

  buf[10] = 0xf0; /* Program info length */
  buf[11] = 0x00; /* We dont have any such things atm */

  buf += 12;
  tlen = 12;

  for(i = 0; i < ss->ss_num_components; i++) {
    streaming_start_component_t *ssc = &ss->ss_components[i];

    pid = 200 + i;

    switch(ssc->ssc_type) {
    case SCT_MPEG2VIDEO:
      c = 0x02;
      break;

    case SCT_MPEG2AUDIO:
      c = 0x03;
      break;

    case SCT_H264:
      c = 0x1b;
      break;

    case SCT_AC3:
      c = 0x06;
      break;

    default:
      continue;
    }


    buf[0] = c;
    buf[1] = 0xe0 | (pid >> 8);
    buf[2] =         pid;

    buf1 = &buf[3];
    tlen += 5;
    buf  += 5;
    dlen = 0;

    switch(ssc->ssc_type) {
    case SCT_AC3:
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


static struct strtab caidnametab[] = {
  { "Seca",             0x0100 }, 
  { "CCETT",            0x0200 }, 
  { "Deutsche Telecom", 0x0300 }, 
  { "Eurodec",          0x0400 }, 
  { "Viaccess",         0x0500 }, 
  { "Irdeto",           0x0600 }, 
  { "Irdeto",           0x0602 }, 
  { "Irdeto",           0x0604 }, 
  { "Jerroldgi",        0x0700 }, 
  { "Matra",            0x0800 }, 
  { "NDS",              0x0900 }, 
  { "Nokia",            0x0A00 }, 
  { "Conax",            0x0B00 }, 
  { "NTL",              0x0C00 }, 
  { "CryptoWorks",      0x0D00 }, 
  { "PowerVu",          0x0E00 }, 
  { "Sony",             0x0F00 }, 
  { "Tandberg",         0x1000 }, 
  { "Thompson",         0x1100 }, 
  { "TV-Com",           0x1200 }, 
  { "HPT",              0x1300 }, 
  { "HRT",              0x1400 }, 
  { "IBM",              0x1500 }, 
  { "Nera",             0x1600 }, 
  { "BetaCrypt",        0x1700 }, 
  { "BetaCrypt",        0x1702 }, 
  { "BetaCrypt",        0x1722 }, 
  { "BetaCrypt",        0x1762 }, 
  { "NagraVision",      0x1800 }, 
  { "Titan",            0x1900 }, 
  { "Telefonica",       0x2000 }, 
  { "Stentor",          0x2100 }, 
  { "Tadiran Scopus",   0x2200 }, 
  { "BARCO AS",         0x2300 }, 
  { "StarGuide",        0x2400 }, 
  { "Mentor",           0x2500 }, 
  { "EBU",              0x2600 }, 
  { "GI",               0x4700 }, 
  { "Telemann",         0x4800 }
};

const char *
psi_caid2name(uint16_t caid)
{
  const char *s = val2str(caid, caidnametab);
  static char buf[20];

  if(s != NULL)
    return s;
  snprintf(buf, sizeof(buf), "0x%x", caid);
  return buf;
}

/**
 *
 */
static struct strtab streamtypetab[] = {
  { "MPEG2VIDEO", SCT_MPEG2VIDEO },
  { "MPEG2AUDIO", SCT_MPEG2AUDIO },
  { "H264",       SCT_H264 },
  { "AC3",        SCT_AC3 },
  { "TELETEXT",   SCT_TELETEXT },
  { "DVBSUB",     SCT_DVBSUB },
  { "CA",         SCT_CA },
  { "PMT",        SCT_PMT },
  { "PAT",        SCT_PAT },
  { "AAC",        SCT_AAC },
  { "MPEGTS",     SCT_MPEGTS },
  { "TEXTSUB",    SCT_TEXTSUB },
};


/**
 *
 */
const char *
streaming_component_type2txt(streaming_component_type_t s)
{
  return val2str(s, streamtypetab) ?: "INVALID";
}


/**
 * Store transport settings into message
 */
void
psi_save_transport_settings(htsmsg_t *m, th_transport_t *t)
{
  th_stream_t *st;
  htsmsg_t *sub;

  htsmsg_add_u32(m, "pcr", t->tht_pcr_pid);

  htsmsg_add_u32(m, "disabled", !t->tht_enabled);

  lock_assert(&t->tht_stream_mutex);

  LIST_FOREACH(st, &t->tht_components, st_link) {
    sub = htsmsg_create_map();

    htsmsg_add_u32(sub, "pid", st->st_pid);
    htsmsg_add_str(sub, "type", val2str(st->st_type, streamtypetab) ?: "?");
    
    if(st->st_lang[0])
      htsmsg_add_str(sub, "language", st->st_lang);

    if(st->st_type == SCT_CA) {

      caid_t *c;
      htsmsg_t *v = htsmsg_create_list();

      LIST_FOREACH(c, &st->st_caids, link) {
	htsmsg_t *caid = htsmsg_create_map();

	htsmsg_add_u32(caid, "caid", c->caid);
	if(c->providerid)
	  htsmsg_add_u32(caid, "providerid", c->providerid);
	htsmsg_add_msg(v, NULL, caid);
      }

      htsmsg_add_msg(sub, "caidlist", v);
    }

    if(st->st_type == SCT_DVBSUB) {
      htsmsg_add_u32(sub, "compositionid", st->st_composition_id);
      htsmsg_add_u32(sub, "ancillartyid", st->st_ancillary_id);
    }

    if(st->st_type == SCT_TEXTSUB)
      htsmsg_add_u32(sub, "parentpid", st->st_parent_pid);

    if(st->st_frame_duration)
      htsmsg_add_u32(sub, "frameduration", st->st_frame_duration);
    
    htsmsg_add_msg(m, "stream", sub);
  }
}


/**
 *
 */
static void
add_caid(th_stream_t *st, uint16_t caid, uint32_t providerid)
{
  caid_t *c = malloc(sizeof(caid_t));
  c->caid = caid;
  c->providerid = providerid;
  c->delete_me = 0;
  LIST_INSERT_HEAD(&st->st_caids, c, link);
}


/**
 *
 */
static void
load_legacy_caid(htsmsg_t *c, th_stream_t *st)
{
  uint32_t a, b;
  const char *v;

  if(htsmsg_get_u32(c, "caproviderid", &b))
    b = 0;

  if(htsmsg_get_u32(c, "caidnum", &a)) {
    if((v = htsmsg_get_str(c, "caid")) != NULL) {
      int i = str2val(v, caidnametab);
      a = i < 0 ? strtol(v, NULL, 0) : i;
    } else {
      return;
    }
  }

  add_caid(st, a, b);
}


/**
 *
 */
static void 
load_caid(htsmsg_t *m, th_stream_t *st)
{
  htsmsg_field_t *f;
  htsmsg_t *c, *v = htsmsg_get_list(m, "caidlist");
  uint32_t a, b;

  if(v == NULL)
    return;

  HTSMSG_FOREACH(f, v) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;
    
    if(htsmsg_get_u32(c, "caid", &a))
      continue;

    if(htsmsg_get_u32(c, "providerid", &b))
      b = 0;

    add_caid(st, a, b);
  }
}



/**
 * Load transport info from htsmsg
 */
void
psi_load_transport_settings(htsmsg_t *m, th_transport_t *t)
{
  htsmsg_t *c;
  htsmsg_field_t *f;
  uint32_t u32, pid;
  th_stream_t *st;
  streaming_component_type_t type;
  const char *v;

  if(!htsmsg_get_u32(m, "pcr", &u32))
    t->tht_pcr_pid = u32;

  if(!htsmsg_get_u32(m, "disabled", &u32))
    t->tht_enabled = !u32;
  else
    t->tht_enabled = 1;

  HTSMSG_FOREACH(f, m) {
    if(strcmp(f->hmf_name, "stream"))
      continue;

    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if((v = htsmsg_get_str(c, "type")) == NULL)
      continue;

    type = str2val(v, streamtypetab);
    if(type == -1)
      continue;

    if(htsmsg_get_u32(c, "pid", &pid))
      continue;

    st = transport_stream_create(t, pid, type);
    st->st_tb = (AVRational){1, 90000};
    
    if((v = htsmsg_get_str(c, "language")) != NULL)
      av_strlcpy(st->st_lang, v, 4);

    if(!htsmsg_get_u32(c, "frameduration", &u32))
      st->st_frame_duration = u32;

   
    load_legacy_caid(c, st);
    load_caid(c, st);

    if(type == SCT_DVBSUB) {
      if(!htsmsg_get_u32(c, "compositionid", &u32))
	st->st_composition_id = u32;

      if(!htsmsg_get_u32(c, "ancillartyid", &u32))
	st->st_ancillary_id = u32;
    }

    if(type == SCT_TEXTSUB) {
      if(!htsmsg_get_u32(c, "parentpid", &u32))
	st->st_parent_pid = u32;
    }
  }
}
