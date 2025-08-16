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

#include "tvheadend.h"
#include "config.h"
#include "input.h"
#include "lang_codes.h"
#include "descrambler/caid.h"
#include "descrambler/dvbcam.h"
#include "dvb_psi_pmt.h"
#include "dvb_psi_hbbtv.h"

/*
 * PMT processing
 */

static inline uint16_t
extract_2byte(const uint8_t *ptr)
{
  return (((uint16_t)ptr[0]) << 8) | ptr[1];
}

static inline uint16_t
extract_pid(const uint8_t *ptr)
{
  return ((ptr[0] & 0x1f) << 8) | ptr[1];
}

static inline uint16_t
extract_svcid(const uint8_t *ptr)
{
  return (ptr[0] << 8) | ptr[1];
}

/**
 * Add a CA descriptor
 */
static int
psi_desc_add_ca
  (mpegts_table_t *mt, elementary_set_t *set,
   uint16_t caid, uint32_t provid, uint16_t pid)
{
  elementary_stream_t *st;
  caid_t *c;
  int r = 0;

  tvhdebug(mt->mt_subsys, "%s:  caid %04X (%s) provider %08X pid %04X",
           mt->mt_name, caid, caid2name(caid), provid, pid);

  st = elementary_stream_find(set, pid);
  if (st == NULL || st->es_type != SCT_CA) {
    st = elementary_stream_create(set, pid, SCT_CA);
    r |= PMT_UPDATE_NEW_CA_STREAM;
  }

  st->es_delete_me = 0;

  st->es_position = 0x40000;

  LIST_FOREACH(c, &st->es_caids, link) {
    if(c->caid == caid) {
      if (c->pid > 0 && c->pid != pid)
        r |= PMT_UPDATE_CAID_PID;
      c->pid = pid;
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
  c->use = 1;
  c->pid = pid;
  c->delete_me = 0;
  c->filter = 0;
  LIST_INSERT_HEAD(&st->es_caids, c, link);
  r |= PMT_UPDATE_NEW_CAID;
  return r;
}

/**
 * Parser for CA descriptors
 */
static int 
psi_desc_ca
  (mpegts_table_t *mt, elementary_set_t *set, const uint8_t *buffer, int size)
{
  int r = 0;
  int i;
  uint32_t provid = 0;
  uint16_t caid, pid;

  if (size < 4)
    return 0;

  caid = extract_2byte(buffer);
  pid = extract_pid(buffer + 2);

  switch (caid & 0xFF00) {
  case 0x0100: // SECA/Mediaguard
    if (size < 6)
      return 0;
    provid = extract_2byte(buffer + 4);

    //Add extra providers, if any
    for (i = 17; i < size; i += 15){
      uint16_t xpid = extract_pid(buffer + i);
      uint16_t xprovid = extract_2byte(buffer + i + 2);

      r |= psi_desc_add_ca(mt, set, caid, xprovid, xpid);
    }
    break;
  case 0x0500:// Viaccess
    for (i = 4; i + 5 <= size;) {
      uint8_t nano    = buffer[i++];
      uint8_t nanolen = buffer[i++];

      if (nano == 0x14) {
        provid = (buffer[i] << 16) | (buffer[i + 1] << 8) | (buffer[i + 2] & 0xf0);
        break;
      }

      i += nanolen;
    }
    break;
  case 0x4a00:
    if (caid == 0x4ad2)//streamguard
       provid=0;
    if (caid != 0x4aee && caid != 0x4ad2) { // Bulcrypt
       provid = size < 5 ? 0 : buffer[4];
    }
    break;
  case 0x1800: // Nagra
    if (size == 0x7)
      provid = extract_2byte(buffer + 5);
    else
      provid = 0;
    break;
  default:
    provid = 0;
    break;
  }

  r |= psi_desc_add_ca(mt, set, caid, provid, pid);

  return r;
}

/**
 * Parser for teletext descriptor
 */
static int
psi_desc_teletext(elementary_set_t *set, const uint8_t *ptr, int size,
                  int parent_pid, int *position)
{
  int r = 0;
  const char *lang;
  elementary_stream_t *st;

  while(size >= 5) {
    int page = (ptr[3] & 0x7 ?: 8) * 100 + (ptr[4] >> 4) * 10 + (ptr[4] & 0xf);
    int type = ptr[3] >> 3;

    if(page > 0 && (type == 2 || type == 5)) {
      // 2 = subtitle page, 5 = subtitle page [hearing impaired]

      // We put the teletext subtitle driven streams on a list of pids
      // higher than normal MPEG TS (0x2000 ++)
      int pid = DVB_TELETEXT_BASE + page;
    
      st = elementary_stream_find_parent(set, pid, parent_pid);
      if (st == NULL || st->es_type != SCT_TEXTSUB) {
        r |= PMT_UPDATE_NEW_STREAM;
        st = elementary_stream_create_parent(set, pid, parent_pid, SCT_TEXTSUB);
      }

      lang = lang_code_get2((const char*)ptr, 3);
      if(memcmp(st->es_lang,lang,3)) {
        r |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
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
static void
dvb_pmt_hbbtv_table_remove(mpegts_mux_t *mm, elementary_stream_t *st)
{
  mpegts_table_t *mt = mpegts_table_find(mm, "hbbtv", st);
  if (mt)
    mpegts_table_destroy(mt);
}

/** 
 * PMT parser, from ISO 13818-1 and ETSI EN 300 468
 */
uint32_t
dvb_psi_parse_pmt
  (mpegts_table_t *mt, const char *nicename, elementary_set_t *set,
   const uint8_t *ptr, int len)
{
  uint16_t pcr_pid, pid;
  uint8_t estype;
  int dllen;
  uint8_t dtag, dlen;
  streaming_component_type_t hts_stream_type;
  elementary_stream_t *st, *next;
  uint32_t update = 0;
  int composition_id;
  int ancillary_id;
  int version;
  int position;
  int tt_position;
  int video_stream;
  int rds_uecp;
  int ac4;
  int pcr_shared = 0;
  const char *lang;
  uint8_t audio_type, audio_version;
  mpegts_mux_t *mux = mt->mt_mux;
  caid_t *c, *cn;

  version = (ptr[2] >> 1) & 0x1f;
  pcr_pid = extract_pid(ptr + 5);
  dllen   = (ptr[7] & 0xf) << 8 | ptr[8];
  
  if(set->set_pcr_pid != pcr_pid) {
    set->set_pcr_pid = pcr_pid;
    update |= PMT_UPDATE_PCR;
  }
  tvhdebug(mt->mt_subsys, "%s:  pcr_pid %04X", mt->mt_name, pcr_pid);

  ptr += 9;
  len -= 9;

  /* Mark all streams for deletion */
  TAILQ_FOREACH(st, &set->set_all, es_link) {
    st->es_delete_me = 1;

    LIST_FOREACH(c, &st->es_caids, link)
      c->delete_me = 1;
  }

  // Common descriptors
  while(dllen > 1) {
    dtag = ptr[0];
    dlen = ptr[1];

    tvhlog_hexdump(mt->mt_subsys, ptr, dlen + 2);
    len -= 2; ptr += 2; dllen -= 2; 
    if(dlen > len)
      break;

    switch(dtag) {
    case DVB_DESC_CA:
      update |= psi_desc_ca(mt, set, ptr, dlen);
      break;

    default:
      break;
    }
    len -= dlen; ptr += dlen; dllen -= dlen;
  }

  while(len >= 5) {
    estype  = ptr[0];
    pid     = extract_pid(ptr + 1);
    dllen   = (ptr[3] & 0xf) << 8 | ptr[4];
    tvhdebug(mt->mt_subsys, "%s:  pid %04X estype %d", mt->mt_name, pid, estype);
    tvhlog_hexdump(mt->mt_subsys, ptr, 5);

    ptr += 5;
    len -= 5;

    hts_stream_type = SCT_UNKNOWN;
    composition_id = -1;
    ancillary_id = -1;
    rds_uecp = 0;
    ac4 = 0;
    position = 0;
    tt_position = 1000;
    lang = NULL;
    audio_type = 0;
    audio_version = 0;
    video_stream = 0;

    switch(estype) {
    case 0x01:
    case 0x02:
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x03:
    case 0x04:
      hts_stream_type = SCT_MPEG2AUDIO;
      audio_version = 2; /* Assume Layer 2 */
      break;

    case 0x05:
      if (config.hbbtv)
        hts_stream_type = SCT_HBBTV;
      break;

    case 0x06:
      /* 0x06 is Chinese Cable TV AC-3 audio track */
      /* but mark it so only when no more descriptors exist */
      if (dllen <= 1 && mux->mm_pmt_ac3 == MM_AC3_PMT_06)
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

    case 0x24:
      hts_stream_type = SCT_HEVC;
      break;

    case 0x80:	/* DigiCipher II (North American cable) encrypted MPEG-2 */
      hts_stream_type = SCT_MPEG2VIDEO;
      break;

    case 0x81:
      hts_stream_type = SCT_AC3;
      break;

    case 0x87:	/* ATSC */
      hts_stream_type = SCT_EAC3;
      break;

    default:
      break;
    }

    while(dllen > 1) {
      dtag = ptr[0];
      dlen = ptr[1];

      tvhlog_hexdump(mt->mt_subsys, ptr, dlen + 2);
      len -= 2; ptr += 2; dllen -= 2; 
      if(dlen > len)
        break;

      switch(dtag) {
      case DVB_DESC_CA:
        update |= psi_desc_ca(mt, set, ptr, dlen);
        break;

      case DVB_DESC_VIDEO_STREAM:
        video_stream = dlen > 0 && SCT_ISVIDEO(hts_stream_type);
        break;

      case DVB_DESC_REGISTRATION:
        if(mux->mm_pmt_ac3 != MM_AC3_PMT_N05 && dlen == 4 &&
           ptr[0] == 'A' && ptr[1] == 'C' && ptr[2] == '-' &&  ptr[3] == '3')
          hts_stream_type = SCT_AC3;
        /* seen also these formats: */
        /* LU-A, ADV1 */
        break;

      case DVB_DESC_LANGUAGE:
        lang = lang_code_get2((const char*)ptr, 3);
        audio_type = ptr[3];
        break;

      case DVB_DESC_TELETEXT:
        if(estype == 0x06) {
          hts_stream_type = SCT_TELETEXT;
          update |= psi_desc_teletext(set, ptr, dlen, pid, &tt_position);
        }
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
        if(dlen < 8 || video_stream)
          break;

        lang = lang_code_get2((const char*)ptr, 3);
        composition_id = extract_2byte(ptr + 4);
        ancillary_id   = extract_2byte(ptr + 6);
        hts_stream_type = SCT_DVBSUB;
        break;

      case DVB_DESC_EAC3:
        if(estype == 0x06 || estype == 0x81)
          hts_stream_type = SCT_EAC3;
        break;

      case DVB_DESC_EXTENSION:
        if(dlen < 1)
          break;

        if(ptr[0] == 0x15) /* descriptor_tag_extension : AC-4 */
          ac4 = 1;

        if((estype == 0x06 || estype == 0x81) && ac4)
          hts_stream_type = SCT_AC4;
        break;

      case DVB_DESC_ANCILLARY_DATA:
        if(dlen < 1)
          break;

        if((ptr[0] & 0x40) == 0x40) /* ancillary_data_id : RDS via UECP */
          rds_uecp = 1;

        if(rds_uecp && hts_stream_type == SCT_UNKNOWN && estype == 0x89)
          hts_stream_type = SCT_RDS;
        break;

      default:
        break;
      }
      len -= dlen; ptr += dlen; dllen -= dlen;
    }
    
    if (hts_stream_type != SCT_UNKNOWN) {

      st = elementary_stream_find(set, pid);
      if (st == NULL || st->es_type != hts_stream_type) {
        update |= PMT_UPDATE_NEW_STREAM;
        st = elementary_stream_create(set, pid, hts_stream_type);
      }

      if (st->es_type != hts_stream_type) {
        update |= PMT_UPDATE_STREAM_CHANGE;
        st->es_type = hts_stream_type;
        st->es_audio_version = audio_version;
      }

      st->es_delete_me = 0;

      tvhdebug(mt->mt_subsys, "%s:    type %s position %d",
               mt->mt_name, streaming_component_type2txt(st->es_type), position);
      if (lang)
        tvhdebug(mt->mt_subsys, "%s:    language %s", mt->mt_name, lang);
      if (composition_id != -1)
        tvhdebug(mt->mt_subsys, "%s:    composition_id %d", mt->mt_name, composition_id);
      if (ancillary_id != -1)
        tvhdebug(mt->mt_subsys, "%s:    ancillary_id %d", mt->mt_name, ancillary_id);

      if(st->es_position != position) {
        update |= PMT_REORDERED;
        st->es_position = position;
      }

      if(lang && memcmp(st->es_lang, lang, 3)) {
        update |= PMT_UPDATE_LANGUAGE;
        memcpy(st->es_lang, lang, 4);
      }

      if(st->es_audio_type != audio_type) {
        update |= PMT_UPDATE_AUDIO_TYPE;
        st->es_audio_type = audio_type;
        st->es_audio_version = audio_version;
      }

      /* FIXME: it might make sense that PMT info has greater priority */
      /*        but we use this field only for MPEG1/2/3 audio which */
      /*        is detected in the parser code */
      if(audio_version && !st->es_audio_version) {
        update |= PMT_UPDATE_AUDIO_VERSION;
        st->es_audio_version = audio_version;
      }

      if(composition_id != -1 && st->es_composition_id != composition_id) {
        st->es_composition_id = composition_id;
        update |= PMT_UPDATE_COMPOSITION_ID;
      }

      if(ancillary_id != -1 && st->es_ancillary_id != ancillary_id) {
        st->es_ancillary_id = ancillary_id;
        update |= PMT_UPDATE_ANCILLARY_ID;
      }

      if(st->es_rds_uecp != rds_uecp) {
        st->es_rds_uecp = rds_uecp;
        update |= PMT_UPDATE_RDS_UECP;
      }

      if (st->es_pid == set->set_pcr_pid)
        pcr_shared = 1;
    }
    position++;
  }

  /* Handle PCR 'elementary stream' */
  if (!pcr_shared) {
    st = elementary_stream_type_modify(set, set->set_pcr_pid, SCT_PCR);
    st->es_delete_me = 0;
  }

  /* Scan again to see if any streams should be deleted */
  for(st = TAILQ_FIRST(&set->set_all); st != NULL; st = next) {
    next = TAILQ_NEXT(st, es_link);

    for(c = LIST_FIRST(&st->es_caids); c != NULL; c = cn) {
      cn = LIST_NEXT(c, link);
      if (c->delete_me) {
        LIST_REMOVE(c, link);
        free(c);
        update |= PMT_UPDATE_CAID_DELETED;
      }
    }

    if(st->es_delete_me) {
      if (st->es_type == SCT_HBBTV)
        dvb_pmt_hbbtv_table_remove(mt->mt_mux, st);
      elementary_set_stream_destroy(set, st);
      update |= PMT_UPDATE_STREAM_DELETED;
    }
  }

  if(update & PMT_REORDERED)
    elementary_set_sort_streams(set);

  if (update) {
    tvhdebug(mt->mt_subsys, "%s: Service \"%s\" PMT (version %d) updated"
     "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
     mt->mt_name,
     nicename, version,
     update&PMT_UPDATE_PCR               ? ", PCR PID changed":"",
     update&PMT_UPDATE_NEW_STREAM        ? ", New elementary stream":"",
     update&PMT_UPDATE_STREAM_CHANGE     ? ", Changed elementary stream":"",
     update&PMT_UPDATE_STREAM_DELETED    ? ", Stream deleted":"",
     update&PMT_UPDATE_LANGUAGE          ? ", Language changed":"",
     update&PMT_UPDATE_AUDIO_TYPE        ? ", Audio type changed":"",
     update&PMT_UPDATE_AUDIO_VERSION     ? ", Audio version changed":"",
     update&PMT_UPDATE_FRAME_DURATION    ? ", Frame duration changed":"",
     update&PMT_UPDATE_COMPOSITION_ID    ? ", Composition ID changed":"",
     update&PMT_UPDATE_ANCILLARY_ID      ? ", Ancillary ID changed":"",
     update&PMT_UPDATE_NEW_CA_STREAM     ? ", New CA stream":"",
     update&PMT_UPDATE_NEW_CAID          ? ", New CAID":"",
     update&PMT_UPDATE_CA_PROVIDER_CHANGE? ", CA provider changed":"",
     update&PMT_UPDATE_CAID_DELETED      ? ", CAID deleted":"",
     update&PMT_UPDATE_CAID_PID          ? ", CAID PID changed":"",
     update&PMT_UPDATE_RDS_UECP          ? ", RDS UECP flag changed":"",
     update&PMT_REORDERED                ? ", PIDs reordered":"");
  }

  return update;
}

int
dvb_pmt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver, restart;
  uint32_t update;
  uint16_t sid;
  mpegts_mux_t *mm = mt->mt_mux;
  mpegts_service_t *s;
  elementary_stream_t *es;
  mpegts_psi_table_state_t *st  = NULL;

  /* Start */
  if (len < 2) return -1;
  sid = extract_svcid(ptr);
  r   = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                        tableid, sid, 9, &st, &sect, &last, &ver, 0);
  if (r != 1) return r;
  if (mm->mm_sid_filter > 0 && sid != mm->mm_sid_filter)
    goto end;

  /* Find service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
    if (service_id16(s) == sid) break;
  if (!s) return -1;

  /* Process */
  tvhdebug(mt->mt_subsys, "%s: sid %04X (%d)", mt->mt_name, sid, sid);
  update = 0;
  tvh_mutex_lock(&s->s_stream_mutex);
  update = dvb_psi_parse_pmt(mt, service_nicename((service_t *)s),
                             &s->s_components, ptr, len);
  if (update) {
    if (s->s_status == SERVICE_RUNNING)
      elementary_set_filter_build(&s->s_components);
    service_request_save((service_t*)s);
  }
  /* Only restart if something that our clients worry about did change */
  restart = 0;
  if (update) {
    if (update & ~(PMT_UPDATE_NEW_CA_STREAM |
                   PMT_UPDATE_NEW_CAID |
                   PMT_UPDATE_CA_PROVIDER_CHANGE |
                   PMT_UPDATE_CAID_DELETED |
                   PMT_UPDATE_CAID_PID)) {
      restart = s->s_status == SERVICE_RUNNING;
    }
  }
  /* autoenable */
  if (elementary_stream_has_audio_or_video(&s->s_components)) {
    mpegts_service_autoenable(s, "PAT and PMT");
    s->s_verified = 1;
  }
  tvh_mutex_unlock(&s->s_stream_mutex);
  if (restart)
    service_restart((service_t*)s);
  if (update & (PMT_UPDATE_NEW_CA_STREAM|PMT_UPDATE_NEW_CAID|
                PMT_UPDATE_CAID_DELETED|PMT_UPDATE_CAID_PID))
    descrambler_caid_changed((service_t *)s);

  TAILQ_FOREACH(es, &s->s_components.set_filter, es_filter_link) {
    if (es->es_type != SCT_HBBTV) continue;
    tvhdebug(mt->mt_subsys, "%s:    install hbbtv pid %04X (%d)",
             mt->mt_name, es->es_pid, es->es_pid);
    mpegts_table_add(mm, DVB_HBBTV_BASE, DVB_HBBTV_MASK,
                     dvb_hbbtv_callback, es, "hbbtv", LS_TBL_BASE,
                     MT_CRC | MT_FULL | MT_QUICKREQ | MT_ONESHOT,
                     es->es_pid, MPS_WEIGHT_HBBTV_SCAN);

  }

#if ENABLE_LINUXDVB_CA
  dvbcam_pmt_data(s, ptr, len);
#endif

  /* Finish */
end:
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}
