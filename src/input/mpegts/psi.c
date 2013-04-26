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

#include "tvheadend.h"
#include "psi.h"
#include "dvb.h"
#include "tsdemux.h"
#include "parsers.h"
#include "parsers/parser_teletext.h" // TODO: only for PID
#include "lang_codes.h"

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

  if(crc && tvh_crc32(ps->ps_data, tsize, 0xffffffff))
    return -1;

  ps->ps_offset = 0;
  cb(ps->ps_data, tsize - (crc ? 4 : 0), opaque);
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

  while(off < 188) {
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
#define PMT_REORDERED                 0x1000

/**
 * Add a CA descriptor
 */
static int
psi_desc_add_ca(mpegts_service_t *t, uint16_t caid, uint32_t provid, uint16_t pid)
{
  elementary_stream_t *st;
  caid_t *c;
  int r = 0;

  if((st = service_stream_find((service_t*)t, pid)) == NULL) {
    st = service_stream_create((service_t*)t, pid, SCT_CA);
    r |= PMT_UPDATE_NEW_CA_STREAM;
  }

  st->es_delete_me = 0;

  st->es_position = 0x40000;

  LIST_FOREACH(c, &st->es_caids, link) {
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
  LIST_INSERT_HEAD(&st->es_caids, c, link);
  r |= PMT_UPDATE_NEW_CAID;
  return r;
}

/**
 * Parser for CA descriptors
 */
static int 
psi_desc_ca(mpegts_service_t *t, const uint8_t *buffer, int size)
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
      uint16_t xpid = ((buffer[i]&0x1F) << 8) | buffer[i + 1];
      uint16_t xprovid = (buffer[i + 2] << 8) | buffer[i + 3];

      r |= psi_desc_add_ca(t, caid, xprovid, xpid);
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
  case 0x4a00://DRECrypt
    if (caid != 0x4aee) { // Bulcrypt
      provid = size < 4 ? 0 : buffer[4];
      break;
    }
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
psi_desc_teletext(mpegts_service_t *t, const uint8_t *ptr, int size,
		  int parent_pid, int *position)
{
  int r = 0;
  const char *lang;
  elementary_stream_t *st;

  while(size >= 5) {
    int page = (ptr[3] & 0x7 ?: 8) * 100 + (ptr[4] >> 4) * 10 + (ptr[4] & 0xf);
    int type = ptr[3] >> 3;

    if(type == 2 || type == 5) {
      // 2 = subtitle page, 5 = subtitle page [hearing impaired]

      // We put the teletext subtitle driven streams on a list of pids
      // higher than normal MPEG TS (0x2000 ++)
      int pid = PID_TELETEXT_BASE + page;
    
      if((st = service_stream_find((service_t*)t, pid)) == NULL) {
	r |= PMT_UPDATE_NEW_STREAM;
	st = service_stream_create((service_t*)t, pid, SCT_TEXTSUB);
	st->es_delete_me = 1;
      }

  
      lang = lang_code_get2((const char*)ptr, 3);
      if(memcmp(st->es_lang,lang,3)) {
	      r |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
      }

      if(st->es_parent_pid != parent_pid) {
	r |= PMT_UPDATE_PARENT_PID;
	st->es_parent_pid = parent_pid;
      }

      // Check es_delete_me so we only compute position once per PMT update
      if(st->es_position != *position && st->es_delete_me) {
	st->es_position = *position;
	r |= PMT_REORDERED;
      }
      st->es_delete_me = 0;
      (*position)++;
    }
    ptr += 5;
    size -= 5;
  }
  return r;
}

/**
 *
 */
static int
pidcmp(const void *A, const void *B)
{
  elementary_stream_t *a = *(elementary_stream_t **)A;
  elementary_stream_t *b = *(elementary_stream_t **)B;
  return a->es_position - b->es_position;
}



/**
 *
 */
static void
sort_pids(mpegts_service_t *t)
{
  elementary_stream_t *st, **v;
  int num = 0, i = 0;

  TAILQ_FOREACH(st, &t->s_components, es_link)
    num++;

  v = alloca(num * sizeof(elementary_stream_t *));
  TAILQ_FOREACH(st, &t->s_components, es_link)
    v[i++] = st;

  qsort(v, num, sizeof(elementary_stream_t *), pidcmp);

  TAILQ_INIT(&t->s_components);
  for(i = 0; i < num; i++)
    TAILQ_INSERT_TAIL(&t->s_components, v[i], es_link);
}


/** 
 * PMT parser, from ISO 13818-1 and ETSI EN 300 468
 */
int
psi_parse_pmt(mpegts_service_t *t, const uint8_t *ptr, int len, int chksvcid,
	      int delete)
{
  uint16_t pcr_pid, pid;
  uint8_t estype;
  int dllen;
  uint8_t dtag, dlen;
  uint16_t sid;
  streaming_component_type_t hts_stream_type;
  elementary_stream_t *st, *next;
  int update = 0;
  int had_components;
  int composition_id;
  int ancillary_id;
  int version;
  int position = 0;
  int tt_position = 1000;
  const char *lang = NULL;

  caid_t *c, *cn;

  if(len < 9)
    return -1;

  lock_assert(&t->s_stream_mutex);

  had_components = !!TAILQ_FIRST(&t->s_components);

  sid     = ptr[0] << 8 | ptr[1];
  version = ptr[2] >> 1 & 0x1f;
  
  if((ptr[2] & 1) == 0) {
    /* current_next_indicator == next, skip this */
    return -1;
  }

  pcr_pid = (ptr[5] & 0x1f) << 8 | ptr[6];
  dllen   = (ptr[7] & 0xf) << 8 | ptr[8];
  
  if(chksvcid && sid != t->s_dvb_service_id)
    return -1;

  if(t->s_pcr_pid != pcr_pid) {
    t->s_pcr_pid = pcr_pid;
    update |= PMT_UPDATE_PCR;
  }

  ptr += 9;
  len -= 9;

  /* Mark all streams for deletion */
  if(delete) {
    TAILQ_FOREACH(st, &t->s_components, es_link) {

      if(st->es_type == SCT_PMT)
        continue;

      st->es_delete_me = 1;

      LIST_FOREACH(c, &st->es_caids, link)
	c->delete_me = 1;
    }
  }

  // Common descriptors
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

    hts_stream_type = SCT_UNKNOWN;
    composition_id = -1;
    ancillary_id = -1;

    switch(estype) {
    case 0x01:
    case 0x02:
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
      hts_stream_type = SCT_MPEG2AUDIO;
      break;

    case 0x81:
      hts_stream_type = SCT_AC3;
      break;
    
    case 0x0f:
      hts_stream_type = SCT_MP4A;
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

      case DVB_DESC_LANGUAGE:
        lang = lang_code_get2((const char*)ptr, 3);
	      break;

      case DVB_DESC_TELETEXT:
	if(estype == 0x06)
	  hts_stream_type = SCT_TELETEXT;
	
	update |= psi_desc_teletext(t, ptr, dlen, pid, &tt_position);
	break;

      case DVB_DESC_AC3:
	if(estype == 0x06 || estype == 0x81)
	  hts_stream_type = SCT_AC3;
	break;

      case DVB_DESC_AAC:
	if(estype == 0x0f)
	  hts_stream_type = SCT_MP4A;
	else if(estype == 0x11)
	  hts_stream_type = SCT_AAC;
	break;

      case DVB_DESC_SUBTITLE:
        if(dlen < 8)
	        break;

        lang = lang_code_get2((const char*)ptr, 3);
        composition_id = ptr[4] << 8 | ptr[5];
        ancillary_id   = ptr[6] << 8 | ptr[7];
        hts_stream_type = SCT_DVBSUB;
        break;

      case DVB_DESC_EAC3:
	if(estype == 0x06 || estype == 0x81)
	  hts_stream_type = SCT_EAC3;
	break;

      default:
	break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }
    
    if(hts_stream_type == SCT_UNKNOWN && estype == 0x06 &&
       pid == 3401 && t->s_dvb_service_id == 10510) {
      // Workaround for ITV HD
      hts_stream_type = SCT_H264;
    }

    if(hts_stream_type != SCT_UNKNOWN) {

      if((st = service_stream_find((service_t*)t, pid)) == NULL) {
	update |= PMT_UPDATE_NEW_STREAM;
	st = service_stream_create((service_t*)t, pid, hts_stream_type);
      }

      // Jernej: I don't know why. But it seems that sometimes the stream is created with a wrong es_type??
      if(st->es_type != hts_stream_type) {
        st->es_type = hts_stream_type;
      }

      st->es_delete_me = 0;

      if(st->es_position != position) {
	update |= PMT_REORDERED;
	st->es_position = position;
      }

      if(lang && memcmp(st->es_lang, lang, 3)) {
        update |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
      }

      if(composition_id != -1 && st->es_composition_id != composition_id) {
	st->es_composition_id = composition_id;
	update |= PMT_UPDATE_COMPOSITION_ID;
      }

      if(ancillary_id != -1 && st->es_ancillary_id != ancillary_id) {
	st->es_ancillary_id = ancillary_id;
	update |= PMT_UPDATE_ANCILLARY_ID;
      }
    }
    position++;
  }

  /* Scan again to see if any streams should be deleted */
  for(st = TAILQ_FIRST(&t->s_components); st != NULL; st = next) {
    next = TAILQ_NEXT(st, es_link);

    for(c = LIST_FIRST(&st->es_caids); c != NULL; c = cn) {
      cn = LIST_NEXT(c, link);
      if(c->delete_me) {
	LIST_REMOVE(c, link);
	free(c);
	update |= PMT_UPDATE_CAID_DELETED;
      }
    }


    if(st->es_delete_me) {
      service_stream_destroy((service_t*)t, st);
      update |= PMT_UPDATE_STREAM_DELETED;
    }
  }

  if(update & PMT_REORDERED)
    sort_pids(t);

  if(update) {
    tvhlog(LOG_DEBUG, "PSI", "Service \"%s\" PMT (version %d) updated"
	   "%s%s%s%s%s%s%s%s%s%s%s%s%s",
	   service_nicename((service_t*)t), version,
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
	   update&PMT_UPDATE_CAID_DELETED      ? ", CAID deleted":"",
	   update&PMT_REORDERED                ? ", PIDs reordered":"");
    
    service_request_save((service_t*)t, 0);

    // Only restart if something that our clients worry about did change
    if(update & ~(PMT_UPDATE_NEW_CA_STREAM |
		  PMT_UPDATE_NEW_CAID |
		  PMT_UPDATE_CA_PROVIDER_CHANGE | 
		  PMT_UPDATE_CAID_DELETED)) {
      if(t->s_status == SERVICE_RUNNING)
	service_restart((service_t*)t, had_components);
    }
  }
  return 0;
}

/**
 * Store service settings into message
 */
void
psi_save_service_settings(htsmsg_t *m, mpegts_service_t *t)
{
  elementary_stream_t *st;
  htsmsg_t *sub;

  htsmsg_add_u32(m, "pcr", t->s_pcr_pid);

  htsmsg_add_u32(m, "disabled", !t->s_enabled);

  lock_assert(&t->s_stream_mutex);

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    sub = htsmsg_create_map();

    htsmsg_add_u32(sub, "pid", st->es_pid);
#ifdef TODO_FIXME
    htsmsg_add_str(sub, "type", val2str(st->es_type, streamtypetab) ?: "?");
#endif
    htsmsg_add_u32(sub, "position", st->es_position);

    if(st->es_lang[0])
      htsmsg_add_str(sub, "language", st->es_lang);

    if(st->es_type == SCT_CA) {

      caid_t *c;
      htsmsg_t *v = htsmsg_create_list();

      LIST_FOREACH(c, &st->es_caids, link) {
	htsmsg_t *caid = htsmsg_create_map();

	htsmsg_add_u32(caid, "caid", c->caid);
	if(c->providerid)
	  htsmsg_add_u32(caid, "providerid", c->providerid);
	htsmsg_add_msg(v, NULL, caid);
      }

      htsmsg_add_msg(sub, "caidlist", v);
    }

    if(st->es_type == SCT_DVBSUB) {
      htsmsg_add_u32(sub, "compositionid", st->es_composition_id);
      htsmsg_add_u32(sub, "ancillartyid", st->es_ancillary_id);
    }

    if(st->es_type == SCT_TEXTSUB)
      htsmsg_add_u32(sub, "parentpid", st->es_parent_pid);

    if(st->es_type == SCT_MPEG2VIDEO || st->es_type == SCT_H264) {
      if(st->es_width && st->es_height) {
	htsmsg_add_u32(sub, "width", st->es_width);
	htsmsg_add_u32(sub, "height", st->es_height);
      }
      if(st->es_frame_duration)
        htsmsg_add_u32(sub, "duration", st->es_frame_duration);
    }
    
    htsmsg_add_msg(m, "stream", sub);
  }
}


/**
 *
 */
static void
add_caid(elementary_stream_t *st, uint16_t caid, uint32_t providerid)
{
  caid_t *c = malloc(sizeof(caid_t));
  c->caid = caid;
  c->providerid = providerid;
  c->delete_me = 0;
  LIST_INSERT_HEAD(&st->es_caids, c, link);
}


/**
 *
 */
static void
load_legacy_caid(htsmsg_t *c, elementary_stream_t *st)
{
#if TODO_FIXME
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
#endif
}


/**
 *
 */
static void 
load_caid(htsmsg_t *m, elementary_stream_t *st)
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
 * Load service info from htsmsg
 */
void
psi_load_service_settings(htsmsg_t *m, mpegts_service_t *t)
{
  htsmsg_t *c;
  htsmsg_field_t *f;
  uint32_t u32, pid;
  elementary_stream_t *st;
  streaming_component_type_t type = 0; // TODO: FIXME
  const char *v;

  if(!htsmsg_get_u32(m, "pcr", &u32))
    t->s_pcr_pid = u32;

  if(!htsmsg_get_u32(m, "disabled", &u32))
    t->s_enabled = !u32;
  else
    t->s_enabled = 1;

  HTSMSG_FOREACH(f, m) {
    if(strcmp(f->hmf_name, "stream"))
      continue;

    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if((v = htsmsg_get_str(c, "type")) == NULL)
      continue;

#ifdef TODO_FIXME
    type = str2val(v, streamtypetab);
    if(type == -1)
      continue;
#endif

    if(htsmsg_get_u32(c, "pid", &pid))
      continue;

    st = service_stream_create((service_t*)t, pid, type);
    
    if((v = htsmsg_get_str(c, "language")) != NULL)
      strncpy(st->es_lang, lang_code_get(v), 3);

    if(!htsmsg_get_u32(c, "position", &u32))
      st->es_position = u32;
   
    load_legacy_caid(c, st);
    load_caid(c, st);

    if(type == SCT_DVBSUB) {
      if(!htsmsg_get_u32(c, "compositionid", &u32))
	st->es_composition_id = u32;

      if(!htsmsg_get_u32(c, "ancillartyid", &u32))
	st->es_ancillary_id = u32;
    }

    if(type == SCT_TEXTSUB) {
      if(!htsmsg_get_u32(c, "parentpid", &u32))
	st->es_parent_pid = u32;
    }

    if(type == SCT_MPEG2VIDEO || type == SCT_H264) {
      if(!htsmsg_get_u32(c, "width", &u32))
	st->es_width = u32;

      if(!htsmsg_get_u32(c, "height", &u32))
	st->es_height = u32;

      if(!htsmsg_get_u32(c, "duration", &u32))
        st->es_frame_duration = u32;
    }

  }
  sort_pids(t);
}
