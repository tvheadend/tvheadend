/*
 *  Electronic Program Guide - eit grabber
 *  Copyright (C) 2012 Adam Sutton
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

#include <string.h>

#include "tvheadend.h"
#include "channels.h"
#include "dvb/dvb.h"
#include "dvb/dvb_support.h"
#include "service.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "dvb/dvb_charset.h"

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct eit_table_status
{
  LIST_ENTRY(eit_table_status) link;
  int                          tid;
  uint16_t                     onid;
  uint16_t                     tsid;
  uint16_t                     sid;
  uint32_t                     sec[8];
  uint8_t                      ver;
  enum {
    EIT_STATUS_START,
    EIT_STATUS_PROCESS,
    EIT_STATUS_LAST,
    EIT_STATUS_DONE
  }                            state;
} eit_table_status_t;

typedef struct eit_status
{
  eit_table_status_t            *first;
  LIST_HEAD(, eit_table_status) tables;
} eit_status_t;

static eit_table_status_t *eit_status_find
  ( eit_status_t *status, int tableid,
    uint16_t onid, uint16_t tsid, uint16_t sid,
    uint8_t sec, uint8_t lst, uint8_t seg, uint8_t ver )
{
  int i, sec_index;
  uint32_t sec_seen_mask;
  eit_table_status_t *sta;

  /* Find */
  LIST_FOREACH(sta, &status->tables, link)
    if (sta->tid == tableid && sta->tsid == tsid && sta->onid == onid &&
        sta->sid == sid)
      break;

  /* Already complete */
  if (sta && sta->state == EIT_STATUS_DONE && sta->ver == ver) return sta;

  /* Insert new entry */
  if (!sta) {
    sta = calloc(1, sizeof(eit_table_status_t));
    LIST_INSERT_HEAD(&status->tables, sta, link);
    sta->tid  = tableid;
    sta->onid = onid;
    sta->tsid = tsid;
    sta->sid  = sid;
    sta->ver  = 255; // Note: force update below
  }

  /* Reset */
  if (sta->ver != ver) {
    sta->ver = ver;
    for (i = 0; i < (lst / 32); i++)
      sta->sec[i] = 0xFFFFFFFF;
    sta->sec[i] = (0xFFFFFFFF >> (31-(lst%32)));
    sta->state = EIT_STATUS_PROCESS;
  }

  /* Get "section(s) seen" mask. See ETSI TS 101 211 (V1.11.1) section 4.1.4 
   * and ETSI EN 300 468 (V1.13.1) section 5.2.4 for usage of the
   * of the "seg" (= segment_last_section_number) field in the
   * now/next (tableid < 0x50) and schedule (tableid >= 0x50) tables
   */
  sec_index = sec/32;
  sec_seen_mask = 0x1 << (sec%32);
  if (tableid >= 0x50) {
    // ETSI TS 101 211 (V1.11.1) section 4.1.4 specifies seg in eit schedule tables 
    // as the index of the first section in the segment (always a multiple of 8, 
    // because a segment has eight sections) plus an offset thaqt depends on the
    // number of sections used:
    // * seg_first_section + n - 1  if 0<n<8 segments are used (see section 4.1.4 e)
    // * seg_first_section + 7      if all segments are used (see section 4.1.4 f)
    // * seg_first_section + 0      if the section is empty (see section 4.1.4 g)
    // This means that we can calculate the mask offset of the last used section
    // simply by taking the lowest three bits of seg + 1 (empty segments count as 
    // "one section used"). We use this offset to calculate a mask for all *unused* 
    // sections by shifting 0xff left by this offset, take the lowest eight bits, shift 
    // them left by the first section offset and finally expand seen_mask (seen_mask) 
    // by calculating "seen_mask |= unused_mask"

    uint32_t seg_first_section = seg & ~0x07;
    uint32_t seg_last_used_section_offset = (seg & 0x07) + 1;
    uint32_t unused_mask = ((0xff << seg_last_used_section_offset) & 0xff) << (seg_first_section%32);
    sec_seen_mask |= unused_mask;
  }

  /* Already seen */
  if (!(sta->sec[sec_index] & sec_seen_mask)) {
    if (sta->state == EIT_STATUS_START) {
      sta->state = EIT_STATUS_DONE;
      goto done;
    }
    return NULL;
  }

  /* Update */
  sta->sec[sec_index] &= ~sec_seen_mask;

  /* Check complete? */
done:
  sta->state = EIT_STATUS_LAST;
  for (i = 0; i < 8; i++ ) {
    if (sta->sec[i]) {
      sta->state = EIT_STATUS_PROCESS;
      break;
    }
  }
  return sta;
}

typedef struct eit_event
{
  char              uri[257];
  char              suri[257];
  
  lang_str_t       *title;
  lang_str_t       *summary;
  lang_str_t       *desc;

  const char       *default_charset;

  htsmsg_t         *extra;

  epg_genre_list_t *genre;

  uint8_t           hd, ws;
  uint8_t           ad, st, ds;
  uint8_t           bw;

  uint8_t           parental;

} eit_event_t;

/* ************************************************************************
 * Diagnostics
 * ***********************************************************************/

// Dump a descriptor tag for debug (looking for new tags etc...)
static void _eit_dtag_dump ( epggrab_module_t *mod, uint8_t dtag, uint8_t dlen, uint8_t *buf )
{
#if APS_DEBUG
  int i = 0, j = 0;
  char tmp[100];
  tvhlog(LOG_DEBUG, mod->id, "  dtag 0x%02X len %d", dtag, dlen);
  while (i < dlen) {
    j += sprintf(tmp+j, "%02X ", buf[i]);
    i++;
    if ((i % 8) == 0 || (i == dlen)) {
      tvhlog(LOG_DEBUG, mod->id, "    %s", tmp);
      j = 0;
    }
  }
#endif
}

/* ************************************************************************
 * EIT Event descriptors
 * ***********************************************************************/

static dvb_string_conv_t _eit_freesat_conv[2] = {
  { 0x1f, freesat_huffman_decode },
  { 0x00, NULL }
};

/*
 * Get string
 */
static int _eit_get_string_with_len
  ( epggrab_module_t *m,
    char *dst, size_t dstlen, 
		const uint8_t *src, size_t srclen, const char *charset )
{
  dvb_string_conv_t *cptr = NULL;

  /* Enable huffman decode (for freeview and/or freesat) */
  m = epggrab_module_find_by_id("uk_freesat");
  if (m && m->enabled) cptr = _eit_freesat_conv;
  else
    m = epggrab_module_find_by_id("uk_freeview");
    if (m && m->enabled) cptr = _eit_freesat_conv;

  /* Convert */
  return dvb_get_string_with_len(dst, dstlen, src, srclen, charset, cptr);
}

/*
 * Short Event - 0x4d
 */
static int _eit_desc_short_event
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev )
{
  int r;
  char lang[4];
  char buf[512];

  if ( len < 5 ) return -1;

  /* Language */
  memcpy(lang, ptr, 3);
  lang[3] = '\0';
  len -= 3;
  ptr += 3;

  /* Title */
  if ( (r = _eit_get_string_with_len(mod, buf, sizeof(buf),
                                     ptr, len, ev->default_charset)) < 0 ) {
    return -1;
  } else {
    if (!ev->title) ev->title = lang_str_create();
    lang_str_add(ev->title, buf, lang, 0);
  }

  len -= r;
  ptr += r;
  if ( len < 1 ) return -1;

  /* Summary */
  if ( (r = _eit_get_string_with_len(mod, buf, sizeof(buf),
                                     ptr, len, ev->default_charset)) < 0 ) {
    return -1;
  } else {
    if (!ev->summary) ev->summary = lang_str_create();
    lang_str_add(ev->summary, buf, lang, 0);
  }

  return 0;
}

/*
 * Extended Event - 0x4e
 */
static int _eit_desc_ext_event
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev )
{
  int r, ilen;
  char ikey[512], ival[512];
  char buf[512], lang[4];
  uint8_t *iptr;

  if (len < 6) return -1;

  /* Descriptor numbering (skip) */
  len -= 1;
  ptr += 1;

  /* Language */
  memcpy(lang, ptr, 3);
  lang[3] = '\0';
  len -= 3;
  ptr += 3;

  /* Key/Value items */
  ilen  = *ptr;
  len  -= 1;
  ptr  += 1;
  iptr  = ptr;
  if (len < ilen) return -1;

  /* Skip past */
  ptr += ilen;
  len -= ilen;

  /* Process */
  while (ilen) {

    /* Key */
    if ( (r = _eit_get_string_with_len(mod, ikey, sizeof(ikey),
                                       iptr, ilen, ev->default_charset)) < 0 )
      break;
    
    ilen -= r;
    iptr += r;

    /* Value */
    if ( (r = _eit_get_string_with_len(mod, ival, sizeof(ival),
                                       iptr, ilen, ev->default_charset)) < 0 )
      break;

    ilen -= r;
    iptr += r;

    /* Store */
    // TODO: extend existing?
#if TODO_ADD_EXTRA
    if (*ikey && *ival) {
      if (!ev->extra) ev->extra = htsmsg_create_map();
      htsmsg_add_str(ev->extra, ikey, ival);
    }
#endif
  }

  /* Description */
  if ( _eit_get_string_with_len(mod,
                                buf, sizeof(buf),
                                ptr, len,
                                ev->default_charset) > 0 ) {
    if (!ev->desc) ev->desc = lang_str_create();
    lang_str_append(ev->desc, buf, lang);
  }

  return 0;
}

/*
 * Component Descriptor - 0x50
 */

static int _eit_desc_component
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev )
{
  uint8_t c, t;

  if (len < 6) return -1;

  /* Stream Content and Type */
  c = *ptr & 0x0f;
  t = ptr[1];

  /* MPEG2 (video) */
  if (c == 0x1) {
    if (t > 0x08 && t < 0x11) {
      ev->hd = 1;
      if ( t != 0x09 && t != 0x0d )
        ev->ws = 1;
    } else if (t == 0x02 || t == 0x03 || t == 0x04 ||
               t == 0x06 || t == 0x07 || t == 0x08 ) {
      ev->ws = 1;
    }

  /* MPEG2 (audio) */
  } else if (c == 0x2) {

    /* Described */
    if (t == 0x40 || t == 0x41)
      ev->ad = 1;

  /* Misc */
  } else if (c == 0x3) {
    if (t == 0x1 || (t >= 0x10 && t <= 0x14) || (t >= 0x20 && t <= 0x24))
      ev->st = 1;
    else if (t == 0x30 || t == 0x31)
      ev->ds = 1;

  /* H264 */
  } else if (c == 0x5) {
    if (t == 0x0b || t == 0x0c || t == 0x10)
      ev->hd = ev->ws = 1;
    else if (t == 0x03 || t == 0x04 || t == 0x07 || t == 0x08)
      ev->ws = 1;

  /* AAC */
  } else if ( c == 0x6 ) {

    /* Described */
    if (t == 0x40 || t == 0x44)
      ev->ad = 1;
  }

  return 0;
}

/*
 * Content Descriptor - 0x54
 */

static int _eit_desc_content
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev )
{
  while (len > 1) {
    if (*ptr == 0xb1)
      ev->bw = 1;
    else if (*ptr < 0xb0) {
      if (!ev->genre) ev->genre = calloc(1, sizeof(epg_genre_list_t));
      epg_genre_list_add_by_eit(ev->genre, *ptr);
    }
    len -= 2;
    ptr += 2;
  }
  return 0;
}

/*
 * Parental rating Descriptor - 0x55
 */
static int _eit_desc_parental
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev )
{
  int cnt = 0, sum = 0, i = 0;
  while (len > 3) {
    if ( ptr[i] && ptr[i] < 0x10 ) {
      cnt++;
      sum += (ptr[i] + 3);
    }
    len -= 4;
    i   += 4;
  }
  // Note: we ignore the country code and average the lot!
  if (cnt)
    ev->parental = (uint8_t)(sum / cnt);

  return 0;
}

/*
 * Content ID - 0x76
 */
static int _eit_desc_crid
  ( epggrab_module_t *mod, uint8_t *ptr, int len, eit_event_t *ev, service_t *svc )
{
  int r;
  uint8_t type;
  char buf[512], *crid;
  int clen;

  while (len > 3) {

    /* Explicit only */
    if ( (*ptr & 0x3) == 0 ) {
      crid = NULL;
      type = *ptr >> 2;

      r = _eit_get_string_with_len(mod, buf, sizeof(buf),
                                   ptr+1, len-1,
                                   ev->default_charset);
      if (r < 0) return -1;

      /* Episode */
      if (type == 0x1 || type == 0x31) {
        crid = ev->uri;
        clen = sizeof(ev->uri);

      /* Season */
      } else if (type == 0x2 || type == 0x32) {
        crid = ev->suri;
        clen = sizeof(ev->suri);
      }
    
      if (crid) {
        if (strstr(buf, "crid://") == buf) {
          strncpy(crid, buf, clen);
        } else if ( *buf != '/' ) {
          snprintf(crid, clen, "crid://%s", buf);
        } else {
          char *defauth = svc->s_default_authority;
          if (!defauth)
            defauth = svc->s_dvb_mux_instance->tdmi_default_authority;
          if (defauth)
            snprintf(crid, clen, "crid://%s%s", defauth, buf);
        }
      }

      /* Next */
      len -= 1 + r;
      ptr += 1 + r;
    }
  }

  return 0;
}


/* ************************************************************************
 * EIT Event
 * ***********************************************************************/

static int _eit_process_event
  ( epggrab_module_t *mod, int tableid,
    service_t *svc, uint8_t *ptr, int len,
    int *resched, int *save )
{
  int save2 = 0;
  int ret, dllen;
  time_t start, stop;
  uint16_t eid;
  uint8_t dtag, dlen;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  epg_serieslink_t *es;
  eit_event_t ev;

  if ( len < 12 ) return -1;

  /* Core fields */
  eid   = ptr[0] << 8 | ptr[1];
  start = dvb_convert_date(&ptr[2]);
  stop  = start + bcdtoint(ptr[7] & 0xff) * 3600 +
                  bcdtoint(ptr[8] & 0xff) * 60 +
                  bcdtoint(ptr[9] & 0xff);
  dllen = ((ptr[10] & 0x0f) << 8) | ptr[11];

  len -= 12;
  ptr += 12;
  if ( len < dllen ) return -1;
  ret  = 12 + dllen;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(svc->s_ch, start, stop, eid, 1, &save2);
#ifdef EPG_EIT_TRACE
  tvhlog(LOG_DEBUG, mod->id, "eid=%5d, start=%lu, stop=%lu, ebc=%p",
         eid, start, stop, ebc);
#endif
  if (!ebc) return dllen + 12;

  /* Mark re-schedule detect (only now/next) */
  if (save2 && tableid < 0x50) *resched = 1;
  *save |= save2;

  /* Process tags */
  memset(&ev, 0, sizeof(ev));
  ev.default_charset = svc->s_dvb_charset;

  /* Override */
  if (!ev.default_charset) {
    ev.default_charset
      = dvb_charset_find(svc->s_dvb_mux_instance->tdmi_network_id,
                         svc->s_dvb_mux_instance->tdmi_transport_stream_id,
                         svc->s_dvb_service_id);
  }

  while (dllen > 2) {
    int r;
    dtag = ptr[0];
    dlen = ptr[1];

    dllen -= 2;
    ptr   += 2;
    if (dllen < dlen) break;

    switch (dtag) {
      case DVB_DESC_SHORT_EVENT:
        r = _eit_desc_short_event(mod, ptr, dlen, &ev);
        break;
      case DVB_DESC_EXT_EVENT:
        r = _eit_desc_ext_event(mod, ptr, dlen, &ev);
        break;
      case DVB_DESC_CONTENT:
        r = _eit_desc_content(mod, ptr, dlen, &ev);
        break;
      case DVB_DESC_COMPONENT:
        r = _eit_desc_component(mod, ptr, dlen, &ev);
        break;
      case DVB_DESC_PARENTAL_RAT:
        r = _eit_desc_parental(mod, ptr, dlen, &ev);
        break;
      case DVB_DESC_CRID:
        r = _eit_desc_crid(mod, ptr, dlen, &ev, svc);
        break;
      default:
        r = 0;
        _eit_dtag_dump(mod, dtag, dlen, ptr);
        break;
    }

    if (r < 0) break;
    dllen -= dlen;
    ptr   += dlen;
  }

  /*
   * Broadcast
   */

  /* Summary/Description */
  if ( ev.summary )
    *save |= epg_broadcast_set_summary2(ebc, ev.summary, mod);
  if ( ev.desc )
    *save |= epg_broadcast_set_description2(ebc, ev.desc, mod);

  /* Broadcast Metadata */
  *save |= epg_broadcast_set_is_hd(ebc, ev.hd, mod);
  *save |= epg_broadcast_set_is_widescreen(ebc, ev.ws, mod);
  *save |= epg_broadcast_set_is_audio_desc(ebc, ev.ad, mod);
  *save |= epg_broadcast_set_is_subtitled(ebc, ev.st, mod);
  *save |= epg_broadcast_set_is_deafsigned(ebc, ev.ds, mod);

  /*
   * Series link
   */

  if (*ev.suri) {
    if ((es = epg_serieslink_find_by_uri(ev.suri, 1, save)))
      *save |= epg_broadcast_set_serieslink(ebc, es, mod);
  }

  /*
   * Episode
   */

  /* Find episode */
  if (*ev.uri) {
    if ((ee = epg_episode_find_by_uri(ev.uri, 1, save)))
      *save |= epg_broadcast_set_episode(ebc, ee, mod);

  /* Existing/Artificial */
  } else
    ee = epg_broadcast_get_episode(ebc, 1, save);

  /* Update Episode */
  if (ee) {
    *save |= epg_episode_set_is_bw(ee, ev.bw, mod);
    if ( ev.title )
      *save |= epg_episode_set_title2(ee, ev.title, mod);
    if ( ev.genre )
      *save |= epg_episode_set_genre(ee, ev.genre, mod);
    if ( ev.parental )
      *save |= epg_episode_set_age_rating(ee, ev.parental, mod);
#if TODO_ADD_EXTRA
    if ( ev.extra )
      *save |= epg_episode_set_extra(ee, extra, mod);
#endif
  }

  /* Tidy up */
#if TODO_ADD_EXTRA
  if (ev.extra)   htsmsg_destroy(ev.extra);
#endif
  if (ev.genre)   epg_genre_list_destroy(ev.genre);
  if (ev.title)   lang_str_destroy(ev.title);
  if (ev.summary) lang_str_destroy(ev.summary);
  if (ev.desc)    lang_str_destroy(ev.desc);

  return ret;
}

static int _eit_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len, 
    uint8_t tableid, void *opaque )
{
  epggrab_module_t *mod = opaque;
  epggrab_ota_mux_t *ota;
  th_dvb_adapter_t *tda;
  service_t *svc;
  eit_status_t *sta;
  eit_table_status_t *tsta;
  int resched = 0, save = 0;
  uint16_t onid, tsid, sid;
  uint16_t sec, lst, seg, ver;

  /* Invalid */
  if(tableid < 0x4e || tableid > 0x6f || len < 11) return -1;

  /* Get OTA */
  ota = epggrab_ota_find((epggrab_module_ota_t*)mod, tdmi);
  if (!ota || !ota->status) return -1;
  sta = ota->status;

  /* Begin: reset sta->state to force revisiting of all tables */
  if (epggrab_ota_begin(ota)) {
    sta->first = NULL;
    LIST_FOREACH(tsta, &sta->tables, link) {
      tsta->state = EIT_STATUS_START;
    }
  }

  /* Get table info */
  sid  = ptr[0] << 8 | ptr[1];
  tsid = ptr[5] << 8 | ptr[6];
  onid = ptr[7] << 8 | ptr[8];
  sec  = ptr[3];
  lst  = ptr[4];
  seg  = ptr[9];
  ver  = (ptr[2] >> 1) & 0x1f;
#ifdef EPG_EIT_TRACE
  tvhlog(LOG_DEBUG, mod->id,
         "tid=0x%02X, onid=0x%04X, tsid=0x%04X, sid=0x%04X, sec=%3d/%3d, seg=%3d, ver=%2d, cur=%d",
         tableid, onid, tsid, sid, sec, lst, seg, ver, ptr[2] & 1);
#endif

  /* Don't process */
  if((ptr[2] & 1) == 0) return 0;

  /* Current status */
  tsta = eit_status_find(sta, tableid, onid, tsid, sid, sec, lst, seg, ver);
#ifdef EPG_EIT_TRACE
  tvhlog(LOG_DEBUG, mod->id, tsta && tsta->state != EIT_STATUS_DONE ? "section process" : "section seen");
#endif
  if (!tsta) return 0; // already seen, no state change
  if (tsta->state == EIT_STATUS_DONE) goto done;

  /* Get transport stream */
  // Note: tableid=0x4f,0x60-0x6f is other TS
  //       so must find the tdmi
  if(tableid == 0x4f || tableid >= 0x60) {
    tda  = tdmi->tdmi_adapter;
    tdmi = dvb_mux_find(tda, NULL, onid, tsid, 1);
  } else {
    if (tdmi->tdmi_transport_stream_id != tsid ||
        tdmi->tdmi_network_id != onid) {
#ifdef EPG_EIT_TRACE
      tvhlog(LOG_DEBUG, mod->id,
             "invalid transport id found tid 0x%02X, onid:tsid %d:%d != %d:%d",
             tableid, tdmi->tdmi_network_id, tdmi->tdmi_transport_stream_id,
             onid, tsid);
#endif
      tdmi = NULL;
    }
  }
  if(!tdmi) goto done;

  /* Get service */
  svc = dvb_service_find3(NULL, tdmi, NULL, 0, 0, sid, 1, 1);
  if (!svc || !svc->s_ch) goto done;

  /* Register as interesting */
  if (tableid < 0x50)
    epggrab_ota_register(ota, 20, 300); // 20s grab, 5min interval
  else
    epggrab_ota_register(ota, 600, 3600); // 10min grab, 1hour interval
  // Note: this does mean you will get a slight oddity for muxes that
  //       carry both, since they will end up with setting of 600/300 
  // Note: could we be more dynamic for now/next interval?

  /* Process events */
  len -= 11;
  ptr += 11;
  while (len) {
    int r;
    if ((r = _eit_process_event(mod, tableid, svc, ptr, len,
                                &resched, &save)) < 0)
      break;
    len -= r;
    ptr += r;
  }

  /* Complete */
done:
  if (tsta->state == EIT_STATUS_LAST)
    tsta->state = EIT_STATUS_DONE;
  if (tsta->state == EIT_STATUS_DONE) {
    if (!sta->first)
      sta->first = tsta;
    else if (sta->first == tsta) {
      LIST_FOREACH(tsta, &sta->tables, link)
        if (tsta->state != EIT_STATUS_DONE) break;
      if (!tsta) epggrab_ota_complete(ota);
    }
  }
#ifdef EPG_EIT_TRACE
  if (ota->state != EPGGRAB_OTA_MUX_COMPLETE)
  {
    int total = 0;
    int finished = 0;
    tvhlog(LOG_DEBUG, mod->id, "scan status");
    LIST_FOREACH(tsta, &sta->tables, link) {
      total++;
      tvhlog(LOG_DEBUG, mod->id,
             "  tid=0x%02X, onid=0x%04X, tsid=0x%04X, sid=0x%04X, ver=%02d, done=%d, "
             "mask=%08X|%08X|%08X|%08X|%08X|%08X|%08X|%08X", 
             tsta->tid, tsta->onid, tsta->tsid, tsta->sid, tsta->ver,
             tsta->state == EIT_STATUS_DONE,
             tsta->sec[7], tsta->sec[6], tsta->sec[5], tsta->sec[4],
             tsta->sec[3], tsta->sec[2], tsta->sec[1], tsta->sec[0]);
      if (tsta->state == EIT_STATUS_DONE) finished++;
    }
    tvhlog(LOG_DEBUG, mod->id, "  completed %d of %d", finished, total);
  }
#endif
  
  /* Update EPG */
  if (resched) epggrab_resched();
  if (save)    epg_updated();

  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _eit_ota_destroy ( epggrab_ota_mux_t *ota )
{
  eit_status_t *sta = ota->status;
  eit_table_status_t *tsta;

  /* Remove all entries */
  while ((tsta = LIST_FIRST(&sta->tables))) {
    LIST_REMOVE(tsta, link);
    free(tsta);
  }

  free(sta);
  free(ota);
}

static void _eit_start 
  ( epggrab_module_ota_t *m, th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_mux_t *ota;

  /* Disabled */
  if (!m->enabled) return;

  /* Freeview (switch to EIT, ignore if explicitly enabled) */
  // Note: do this as PID is the same
  if (!strcmp(m->id, "uk_freeview")) {
    m = (epggrab_module_ota_t*)epggrab_module_find_by_id("eit");
    if (m->enabled) return;
  }

  /* Register */
  if (!(ota = epggrab_ota_create(m, tdmi))) return;
  if (!ota->status) {
    ota->status  = calloc(1, sizeof(eit_status_t));
    ota->destroy = _eit_ota_destroy;
  }

  /* Freesat (3002/3003) */
  if (!strcmp("uk_freesat", m->id)) {
#ifdef IGNORE_TOO_SLOW
    tdt_add(tdmi, 0, 0, dvb_pidx11_callback, m, m->id, TDT_CRC, 3840, NULL);
    tdt_add(tdmi, 0, 0, _eit_callback, m, m->id, TDT_CRC, 3841, NULL);
#endif
    tdt_add(tdmi, 0, 0, dvb_pidx11_callback, m, m->id, TDT_CRC, 3002);
    tdt_add(tdmi, 0, 0, _eit_callback, m, m->id, TDT_CRC, 3003);

  /* Viasat Baltic (0x39) */
  } else if (!strcmp("viasat_baltic", m->id)) {
    tdt_add(tdmi, 0, 0, _eit_callback, m, m->id, TDT_CRC, 0x39);

  /* Standard (0x12) */
  } else {
    tdt_add(tdmi, 0, 0, _eit_callback, m, m->id, TDT_CRC, 0x12);
  }
  tvhlog(LOG_DEBUG, m->id, "install table handlers");
}

static int _eit_enable ( void *m, uint8_t e )
{
 epggrab_module_ota_t *mod = m;

  if (mod->enabled == e) return 0;
  mod->enabled = e;

  /* Register interest */
  if (e) {

    /* Freesat (register fixed MUX) */
    if (!strcmp(mod->id, "uk_freesat")) {
#ifdef IGNORE_TOO_SLOW
      epggrab_ota_create_and_register_by_id((epggrab_module_ota_t*)mod,
                                            0, 2050,
                                            600, 3600, "ASTRA");
#endif
      epggrab_ota_create_and_register_by_id((epggrab_module_ota_t*)mod,
                                            0, 2315,
                                            600, 3600, "Freesat");
    }
  /* Remove all links */
  } else {
    epggrab_ota_destroy_by_module((epggrab_module_ota_t*)mod);
  }

  return 1;
}

void eit_init ( void )
{
  epggrab_module_ota_create(NULL, "eit", "EIT: DVB Grabber", 1,
                            _eit_start, _eit_enable, NULL);
  epggrab_module_ota_create(NULL, "uk_freesat", "UK: Freesat", 5,
                            _eit_start, _eit_enable, NULL);
  epggrab_module_ota_create(NULL, "uk_freeview", "UK: Freeview", 5,
                            _eit_start, _eit_enable, NULL);
  epggrab_module_ota_create(NULL, "viasat_baltic", "VIASAT: Baltic", 5,
                            _eit_start, _eit_enable, NULL);
}

void eit_load ( void )
{
}
