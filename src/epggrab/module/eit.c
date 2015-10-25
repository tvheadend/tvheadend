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
#include "service.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "input.h"
#include "input/mpegts/dvb_charset.h"

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

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
static void
_eit_dtag_dump 
  ( epggrab_module_t *mod, uint8_t dtag, uint8_t dlen, const uint8_t *buf )
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
  if (m && m->enabled) {
    cptr = _eit_freesat_conv;
  } else {
    m = epggrab_module_find_by_id("uk_freeview");
    if (m && m->enabled) cptr = _eit_freesat_conv;
  }

  /* Convert */
  return dvb_get_string_with_len(dst, dstlen, src, srclen, charset, cptr);
}

/*
 * Short Event - 0x4d
 */
static int _eit_desc_short_event
  ( epggrab_module_t *mod, const uint8_t *ptr, int len, eit_event_t *ev )
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
  } else if ( r > 1 ) {
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
  } else if ( r > 1 ) {
    if (!ev->summary) ev->summary = lang_str_create();
    lang_str_add(ev->summary, buf, lang, 0);
  }

  return 0;
}

/*
 * Extended Event - 0x4e
 */
static int _eit_desc_ext_event
  ( epggrab_module_t *mod, const uint8_t *ptr, int len, eit_event_t *ev )
{
  int r, ilen;
  char ikey[512], ival[512];
  char buf[512], lang[4];
  const uint8_t *iptr;

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
                                ev->default_charset) > 1 ) {
    if (!ev->desc) ev->desc = lang_str_create();
    lang_str_append(ev->desc, buf, lang);
  }

  return 0;
}

/*
 * Component Descriptor - 0x50
 */

static int _eit_desc_component
  ( epggrab_module_t *mod, const uint8_t *ptr, int len, eit_event_t *ev )
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
  ( epggrab_module_t *mod, const uint8_t *ptr, int len, eit_event_t *ev )
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
  ( epggrab_module_t *mod, const uint8_t *ptr, int len, eit_event_t *ev )
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
  ( epggrab_module_t *mod, const uint8_t *ptr, int len,
    eit_event_t *ev, mpegts_service_t *svc )
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
      if (r == 0) continue;

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
          crid[clen-1] = '\0';
        } else if ( *buf != '/' ) {
          snprintf(crid, clen, "crid://%s", buf);
        } else {
          const char *defauth = svc->s_dvb_cridauth;
          if (!defauth)
            defauth = svc->s_dvb_mux->mm_crid_authority;
          if (defauth)
            snprintf(crid, clen, "crid://%s%s", defauth, buf);
          else
            snprintf(crid, clen, "crid://onid-%d%s", svc->s_dvb_mux->mm_onid, buf);
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

static int _eit_process_event_one
  ( epggrab_module_t *mod, int tableid,
    mpegts_service_t *svc, channel_t *ch,
    const uint8_t *ptr, int len,
    int local, int *resched, int *save )
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
  start = dvb_convert_date(&ptr[2], local);
  stop  = start + bcdtoint(ptr[7] & 0xff) * 3600 +
                  bcdtoint(ptr[8] & 0xff) * 60 +
                  bcdtoint(ptr[9] & 0xff);
  dllen = ((ptr[10] & 0x0f) << 8) | ptr[11];

  len -= 12;
  ptr += 12;
  if ( len < dllen ) return -1;
  ret  = 12 + dllen;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, start, stop, eid, 1, &save2);
  tvhtrace("eit", "svc='%s', ch='%s', eid=%5d, start=%"PRItime_t","
                  " stop=%"PRItime_t", ebc=%p",
           svc->s_dvb_svcname ?: "(null)", ch ? channel_get_name(ch) : "(null)",
           eid, start, stop, ebc);
  if (!ebc) return dllen + 12;

  /* Mark re-schedule detect (only now/next) */
  if (save2 && tableid < 0x50) *resched = 1;
  *save |= save2;

  /* Process tags */
  memset(&ev, 0, sizeof(ev));
  ev.default_charset = dvb_charset_find(NULL, NULL, svc);

  while (dllen > 2) {
    int r;
    dtag = ptr[0];
    dlen = ptr[1];

    dllen -= 2;
    ptr   += 2;
    if (dllen < dlen) break;

    tvhtrace(mod->id, "  dtag %02X dlen %d", dtag, dlen);
    tvhlog_hexdump(mod->id, ptr, dlen);

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
    if ( ev.summary )
      *save |= epg_episode_set_subtitle2(ee, ev.summary, mod);
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

static int _eit_process_event
  ( epggrab_module_t *mod, int tableid,
    mpegts_service_t *svc, const uint8_t *ptr, int len,
    int local, int *resched, int *save )
{
  idnode_list_mapping_t *ilm;
  int ret = 0;

  if ( len < 12 ) return -1;

  LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link)
    ret = _eit_process_event_one(mod, tableid, svc,
                                 (channel_t *)ilm->ilm_in2,
                                 ptr, len, local, resched, save);
  return ret;
}


static int
_eit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver, save, resched;
  uint8_t  seg;
  uint16_t onid, tsid, sid;
  uint32_t extraid;
  mpegts_service_t     *svc;
  mpegts_mux_t         *mm;
  epggrab_ota_map_t    *map;
  epggrab_module_t     *mod;
  epggrab_ota_mux_t    *ota = NULL;
  mpegts_psi_table_state_t *st;
  th_subscription_t    *ths;
  char ubuf[UUID_HEX_SIZE];

  if (!epggrab_ota_running)
    return -1;

  mm  = mt->mt_mux;
  map = mt->mt_opaque;
  mod = (epggrab_module_t *)map->om_module;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mm, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  /* Validate */
  if(tableid < 0x4e || tableid > 0x6f || len < 11) {
    if (ths)
      ths->ths_total_err++;
    return -1;
  }

  /* Basic info */
  sid     = ptr[0] << 8 | ptr[1];
  tsid    = ptr[5] << 8 | ptr[6];
  onid    = ptr[7] << 8 | ptr[8];
  seg     = ptr[9];
  extraid = ((uint32_t)tsid << 16) | sid;
  // TODO: extra ID should probably include onid

  /* Register interest */
  if (tableid == 0x4e || tableid >= 0x50)
    ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, extraid, 11, &st, &sect, &last, &ver);
  if (r != 1) return r;
  if (st) {
    uint32_t mask;
    int sa = seg & 0xF8;
    int sb = 7 - (seg & 0x07);
    mask = (~(0xFF << sb) & 0xFF);
    mask <<= (24 - (sa % 32));
    st->sections[sa/32] &= ~mask;
  }

  /* Get transport stream */
  // Note: tableid=0x4f,0x60-0x6f is other TS
  //       so must find the tdmi
  if(tableid == 0x4f || tableid >= 0x60) {
    mm = mpegts_network_find_mux(mm->mm_network, onid, tsid);

  } else {
    if ((mm->mm_tsid != tsid || mm->mm_onid != onid) &&
        !mm->mm_eit_tsid_nocheck) {
      if (mm->mm_onid != MPEGTS_ONID_NONE &&
          mm->mm_tsid != MPEGTS_TSID_NONE)
        tvhtrace("eit",
                "invalid tsid found tid 0x%02X, onid:tsid %d:%d != %d:%d",
                tableid, mm->mm_onid, mm->mm_tsid, onid, tsid);
      mm = NULL;
    }
  }
  if(!mm)
    goto done;

  /* Get service */
  svc = mpegts_mux_find_service(mm, sid);
  if (!svc) {
    tvhtrace("eit", "sid %i not found", sid);
    goto done;
  }

  if (map->om_first) {
    map->om_tune_count++;
    map->om_first = 0;
  }

  /* Register this */
  if (ota)
    epggrab_ota_service_add(map, ota, idnode_uuid_as_str(&svc->s_id, ubuf), 1);

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  if (svc->s_dvb_ignore_eit)
    goto done;

  /* Process events */
  save = resched = 0;
  len -= 11;
  ptr += 11;
  while (len) {
    int r;
    if ((r = _eit_process_event(mod, tableid, svc, ptr, len,
                                mm->mm_network->mn_localtime,
                                &resched, &save)) < 0)
      break;
    len -= r;
    ptr += r;
  }

  /* Update EPG */
  if (resched) epggrab_resched();
  if (save)    epg_updated();
  
done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
  if (ota && !r)
    epggrab_ota_complete((epggrab_module_ota_t*)mod, ota);
  
  return r;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static int _eit_start
  ( epggrab_ota_map_t *map, mpegts_mux_t *dm )
{
  epggrab_module_ota_t *m = map->om_module;
  int pid, opts = 0;

  /* Disabled */
  if (!m->enabled && !map->om_forced) return -1;

  /* Freeview (switch to EIT, ignore if explicitly enabled) */
  // Note: do this as PID is the same
  if (!strcmp(m->id, "uk_freeview")) {
    m = (epggrab_module_ota_t*)epggrab_module_find_by_id("eit");
    if (m->enabled) return -1;
  }

  /* Freesat (3002/3003) */
  if (!strcmp("uk_freesat", m->id)) {
    mpegts_table_add(dm, 0, 0, dvb_bat_callback, NULL, "bat", MT_CRC, 3002, MPS_WEIGHT_EIT);
    pid = 3003;

  /* Viasat Baltic (0x39) */
  } else if (!strcmp("viasat_baltic", m->id)) {
    pid = 0x39;

  /* Bulsatcom 39E (0x12b) */
  } else if (!strcmp("Bulsatcom_39E", m->id)) {
    pid = 0x12b;

  /* Standard (0x12) */
  } else {
    pid  = DVB_EIT_PID;
    opts = MT_RECORD;
  }
  mpegts_table_add(dm, 0, 0, _eit_callback, map, m->id, MT_CRC | opts, pid, MPS_WEIGHT_EIT);
  // TODO: might want to limit recording to EITpf only
  tvhlog(LOG_DEBUG, m->id, "installed table handlers");
  return 0;
}

static int _eit_tune
  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *om, mpegts_mux_t *mm )
{
  int r = 0;
  epggrab_module_ota_t *m = map->om_module;
  mpegts_service_t *s;
  epggrab_ota_svc_link_t *osl, *nxt;

  lock_assert(&global_lock);

  /* Disabled */
  if (!m->enabled) return 0;

  /* Have gathered enough info to decide */
  if (!om->om_complete)
    return 1;

  /* Check if any services are mapped */
  // TODO: using indirect ref's like this is inefficient, should 
  //       consider changeing it?
  for (osl = RB_FIRST(&map->om_svcs); osl != NULL; osl = nxt) {
    nxt = RB_NEXT(osl, link);
    /* rule: if 5 mux scans fail for this service, remove it */
    if (osl->last_tune_count + 5 <= map->om_tune_count ||
        !(s = mpegts_service_find_by_uuid(osl->uuid))) {
      epggrab_ota_service_del(map, om, osl, 1);
    } else {
      if (LIST_FIRST(&s->s_channels))
        r = 1;
    }
  }

  return r;
}

void eit_init ( void )
{
  static epggrab_ota_module_ops_t ops = {
    .start = _eit_start,
    .tune  = _eit_tune,
  };

  epggrab_module_ota_create(NULL, "eit", NULL, "EIT: DVB Grabber", 1, &ops);
  epggrab_module_ota_create(NULL, "uk_freesat", NULL, "UK: Freesat", 5, &ops);
  epggrab_module_ota_create(NULL, "uk_freeview", NULL, "UK: Freeview", 5, &ops);
  epggrab_module_ota_create(NULL, "viasat_baltic", NULL, "VIASAT: Baltic", 5, &ops);
  epggrab_module_ota_create(NULL, "Bulsatcom_39E", NULL, "Bulsatcom: Bula 39E", 5, &ops);
}

void eit_done ( void )
{
}
