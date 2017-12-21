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
#include "eitpatternlist.h"
#include "input.h"
#include "input/mpegts/dvb_charset.h"
#include "dvr/dvr.h"

/* ************************************************************************
 * Opaque
 * ***********************************************************************/

typedef struct eit_private
{
  uint16_t pid;
  uint8_t  conv;
  uint8_t  spec;
} eit_private_t;

#define EIT_CONV_HUFFMAN     1

#define EIT_SPEC_UK_FREESAT         1
#define EIT_SPEC_NZ_FREEVIEW        2
#define EIT_SPEC_UK_CABLE_VIRGIN    3

/* Queued data structure */
typedef struct eit_data
{
  tvh_uuid_t svc_uuid;
  uint16_t   onid;
  int        tableid;
  int        sect;
  int        local_time;
  char       cridauth[64];
} eit_data_t;

/* Provider configuration */
typedef struct eit_module_t
{
  epggrab_module_ota_scraper_t  ;      ///< Base struct
  int running_immediate;               ///< Handle quickly the events from the current table
  eit_pattern_list_t p_snum;
  eit_pattern_list_t p_enum;
  eit_pattern_list_t p_airdate;        ///< Original air date parser
  eit_pattern_list_t p_scrape_subtitle;///< Scrape subtitle from summary data
  eit_pattern_list_t p_is_new;         ///< Is programme new to air
} eit_module_t;

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct eit_event
{
  char              uri[257];
  char              suri[257];
  
  lang_str_t       *title;
  lang_str_t       *subtitle;
  lang_str_t       *summary;
  lang_str_t       *desc;

  const char       *default_charset;

#if TODO_ADD_EXTRA
  htsmsg_t         *extra;
#endif

  epg_genre_list_t *genre;

  epg_episode_num_t en;

  uint8_t           hd, ws;
  uint8_t           ad, st, ds;
  uint8_t           bw;

  uint8_t           parental;

  uint8_t           is_new;
  time_t            first_aired;
  uint16_t          copyright_year;

} eit_event_t;


/*
 * Forward declarations
 */
static void _eit_module_load_config(eit_module_t *mod);
static void _eit_scrape_clear(eit_module_t *mod);
static void _eit_done(void *mod);

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
  tvhdebug(mod->subsys, "%s:  dtag 0x%02X len %d", mt->mt_name, dtag, dlen);
  while (i < dlen) {
    j += sprintf(tmp+j, "%02X ", buf[i]);
    i++;
    if ((i % 8) == 0 || (i == dlen)) {
      tvhdebug(mod->subsys, "%s:    %s", mt->mt_name, tmp);
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
  ( epggrab_module_t *mod,
    char *dst, size_t dstlen, 
    const uint8_t *src, size_t srclen, const char *charset )
{
  epggrab_module_ota_t *m = (epggrab_module_ota_t *)mod;
  dvb_string_conv_t *cptr = NULL;

  if (((eit_private_t *)m->opaque)->conv == EIT_CONV_HUFFMAN)
    cptr = _eit_freesat_conv;

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

  /* HEVC */
  } else if ( c == 0x9 ) {

    ev->ws = 1;
    if (t > 3)
      ev->hd = 2;

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
    eit_event_t *ev, eit_data_t *ed )
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
        } else if (*buf != '/') {
          snprintf(crid, clen, "crid://%s", buf);
        } else {
          if (ed->cridauth)
            snprintf(crid, clen, "crid://%s%s", ed->cridauth, buf);
          else
            snprintf(crid, clen, "crid://onid-%d%s", ed->onid, buf);
        }
      }

      /* Next */
      len -= 1 + r;
      ptr += 1 + r;
    }
  }

  return 0;
}

/*
 *
 */
static int positive_atoi(const char *s)
{
  int i = atoi(s);
  return i >= 0 ? i : 0;
}

/* Scrape episode data from the broadcast data.
 * @param str - string from broadcaster to search for all languages.
 * @param eit_mod - our module with regex to use.
 * @param ev - [out] modified event data.
 */
static void
_eit_scrape_episode(lang_str_t *str,
                    eit_module_t *eit_mod,
                    eit_event_t *ev)
{
  lang_str_ele_t *se;
  char buffer[2048];

  if (!str) return;

  /* search for season number */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, &eit_mod->p_snum))
      if ((ev->en.s_num = positive_atoi(buffer))) {
        tvhtrace(LS_TBL_EIT,"  extract season number %d using %s", ev->en.s_num, eit_mod->id);
        break;
      }
  }

  /* ...for episode number */
  RB_FOREACH(se, str, link) {
   if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, &eit_mod->p_enum))
     if ((ev->en.e_num = positive_atoi(buffer))) {
       tvhtrace(LS_TBL_EIT,"  extract episode number %d using %s", ev->en.e_num, eit_mod->id);
       break;
     }
  }

  /* Extract original air date year */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, &eit_mod->p_airdate)) {
      if (strlen(buffer) == 4) {
        /* Year component only, so assume it is the copyright year. */
        ev->copyright_year = positive_atoi(buffer);
        break;
      }
    }
  }

  /* Extract is_new flag. Any match is assumed to mean "new" */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, &eit_mod->p_is_new)) {
      ev->is_new = 1;
      break;
    }
  }
}

/* Scrape subtitle data from the broadcast data.
 * @param text - string from broadcaster to search for all languages.
 * @param eit_mod - our module with regex to use.
 * @param ev - [out] modified event data.
 */
static void
_eit_scrape_subtitle(eit_module_t *eit_mod,
                     eit_event_t *ev)
{
  lang_str_ele_t *se;
  lang_str_t *ls;
  char buffer1[2048];
  char buffer2[2048];
  char *bufs[2] = { buffer1, buffer2 };
  size_t sizes[2] = { sizeof(buffer1), sizeof(buffer2) };

  /* Freeview/Freesat have a subtitle as part of the summary in the format
   * "subtitle: desc". So try and extract it and use that.
   * If we can't find a subtitle then default to previous behaviour of
   * setting the summary as the subtitle.
   */
  ls = lang_str_create();
  RB_FOREACH(se, ev->summary, link) {
    if (eit_pattern_apply_list_2(bufs, sizes, se->str, &eit_mod->p_scrape_subtitle)) {
      tvhtrace(LS_TBL_EIT, "  scrape subtitle '%s'/'%s' from '%s' using %s",
               buffer1, buffer2, se->str, eit_mod->id);
      lang_str_set(&ev->subtitle, buffer1, se->lang);
      if (bufs[1])
        lang_str_set(&ls, buffer2, se->lang);
    }
  }
  RB_FOREACH(se, ls, link) {
      lang_str_set(&ev->summary, se->str, se->lang);
  }
  lang_str_destroy(ls);
}

/* ************************************************************************
 * EIT Event
 * ***********************************************************************/

static int _eit_process_event_one
  ( epggrab_module_t *mod, int tableid, int sect,
    mpegts_service_t *svc, channel_t *ch,
    eit_event_t *ev,
    const uint8_t *ptr, int len,
    int local, int *save )
{
  int save2 = 0, rsonly = 0;
  time_t start, stop;
  uint16_t eid;
  uint8_t running;
  epg_broadcast_t *ebc, _ebc;
  epg_episode_t *ee = NULL, _ee;
  epg_serieslink_t *es;
  epg_running_t run;
  lang_str_t *title_copy = NULL;
  uint32_t changes2 = 0, changes3 = 0, changes4 = 0;
  char tm1[32], tm2[32];

  /* Core fields */
  eid   = ptr[0] << 8 | ptr[1];
  start = dvb_convert_date(&ptr[2], local);
  stop  = start + bcdtoint(ptr[7] & 0xff) * 3600 +
                  bcdtoint(ptr[8] & 0xff) * 60 +
                  bcdtoint(ptr[9] & 0xff);
  running = (ptr[10] >> 5) & 0x07;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, mod, start, stop, 1, &save2, &changes2);
  tvhtrace(LS_TBL_EIT, "svc='%s', ch='%s', eid=%5d, tbl=%02x, running=%d, start=%s,"
                       " stop=%s, ebc=%p",
           svc->s_dvb_svcname ?: "(null)",
           ch ? channel_get_name(ch, channel_blank_name) : "(null)",
           eid, tableid, running,
           gmtime2local(start, tm1, sizeof(tm1)),
           gmtime2local(stop, tm2, sizeof(tm2)), ebc);
  if (!ebc) {
    if (tableid == 0x4e)
      rsonly = 1;
    else
      return 0;
  }

  if (rsonly) {
    if (!ev->title)
      goto running;
    memset(&_ebc, 0, sizeof(_ebc));
    if (*ev->suri)
      if ((es = epg_serieslink_find_by_uri(ev->suri, mod, 0, 0, NULL)))
        _ebc.serieslink = es;
    
    if (*ev->uri && (ee = epg_episode_find_by_uri(ev->uri, mod, 0, 0, NULL))) {
      _ee = *ee;
    } else {
      memset(&_ee, 0, sizeof(_ee));
    }
    _ebc.episode = &_ee;
    _ebc.dvb_eid = eid;
    _ebc.start = start;
    _ebc.stop = stop;
    _ee.title = title_copy = lang_str_copy(ev->title);

    ebc = epg_match_now_next(ch, &_ebc);
    tvhtrace(mod->subsys, "%s:  running state only ebc=%p", svc->s_dvb_svcname ?: "(null)", ebc);
    goto running;
  } else {
    *save = save2;
  }

  /*
   * Broadcast
   */

  *save |= epg_broadcast_set_dvb_eid(ebc, eid, &changes2);

  /* Summary/Description */
  if (ev->summary)
    *save |= epg_broadcast_set_summary(ebc, ev->summary, &changes2);
  if (ev->desc)
    *save |= epg_broadcast_set_description(ebc, ev->desc, &changes2);

  /* Broadcast Metadata */
  *save |= epg_broadcast_set_is_hd(ebc, ev->hd, &changes2);
  *save |= epg_broadcast_set_is_widescreen(ebc, ev->ws, &changes2);
  *save |= epg_broadcast_set_is_audio_desc(ebc, ev->ad, &changes2);
  *save |= epg_broadcast_set_is_subtitled(ebc, ev->st, &changes2);
  *save |= epg_broadcast_set_is_deafsigned(ebc, ev->ds, &changes2);

  /*
   * Series link
   */

  if (*ev->suri) {
    if ((es = epg_serieslink_find_by_uri(ev->suri, mod, 1, save, &changes3))) {
      *save |= epg_broadcast_set_serieslink(ebc, es, &changes2);
      *save |= epg_serieslink_change_finish(es, changes3, 0);
    }
  }

  /*
   * Episode
   */

  /* Find episode */
  if (*ev->uri) {
    ee = epg_episode_find_by_uri(ev->uri, mod, 1, save, &changes4);
  } else {
    ee = epg_episode_find_by_broadcast(ebc, mod, 1, save, &changes4);
  }

  /* Update Episode */
  if (ee) {
    *save |= epg_broadcast_set_episode(ebc, ee, &changes2);
    if (ev->is_new > 0)
      *save |= epg_broadcast_set_is_new(ebc, ev->is_new - 1, &changes2);
    *save |= epg_episode_set_is_bw(ee, ev->bw, &changes4);
    if (ev->title)
      *save |= epg_episode_set_title(ee, ev->title, &changes4);
    if (ev->genre)
      *save |= epg_episode_set_genre(ee, ev->genre, &changes4);
    if (ev->parental)
      *save |= epg_episode_set_age_rating(ee, ev->parental, &changes4);
    if (ev->subtitle)
      *save |= epg_episode_set_subtitle(ee, ev->subtitle, &changes4);
    else if (ev->summary)
      *save |= epg_episode_set_subtitle(ee, ev->summary, &changes4);
#if TODO_ADD_EXTRA
    if (ev->extra)
      *save |= epg_episode_set_extra(ee, extra, &changes4);
#endif
    /* save any found episode number */
    if (ev->en.s_num || ev->en.e_num || ev->en.p_num)
      *save |= epg_episode_set_epnum(ee, &ev->en, &changes4);
    if (ev->first_aired > 0)
      *save |= epg_episode_set_first_aired(ee, ev->first_aired, &changes4);
    if (ev->copyright_year > 0)
      *save |= epg_episode_set_copyright_year(ee, ev->copyright_year, &changes4);
    *save |= epg_episode_change_finish(ee, changes4, 0);
  }

  *save |= epg_broadcast_change_finish(ebc, changes2, 0);


running:
  /* use running flag only for current broadcast */
  if (ebc && running && tableid == 0x4e) {
    if (sect == 0) {
      switch (running) {
      case 2:  run = EPG_RUNNING_WARM;  break;
      case 3:  run = EPG_RUNNING_PAUSE; break;
      case 4:  run = EPG_RUNNING_NOW;   break;
      default: run = EPG_RUNNING_STOP;  break;
      }
      *save |= epg_broadcast_set_running(ebc, run);
    } else if (sect == 1 && running != 2 && running != 3 && running != 4) {
    }
  }

  if (title_copy) lang_str_destroy(title_copy);
  return 0;
}

static int _eit_process_event
  ( epggrab_module_t *mod, int tableid, int sect,
    eit_data_t *ed, const uint8_t *ptr0, int len0,
    int local, int *save, int lock )
{
  eit_module_t *eit_mod = (eit_module_t *)mod;
  idnode_list_mapping_t *ilm;
  mpegts_service_t *svc;
  channel_t *ch;
  eit_event_t ev;
  const uint8_t *ptr;
  int r, len;
  uint8_t dtag, dlen;
  int dllen;

  if (len0 < 12) return -1;

  dllen = ((ptr0[10] & 0x0f) << 8) | ptr0[11];
  len = len0 - 12;
  ptr = ptr0 + 12;

  if (len < dllen) return -1;

  memset(&ev, 0, sizeof(ev));
  svc = (mpegts_service_t *)service_find_by_uuid0(&ed->svc_uuid);
  if (svc)
    ev.default_charset = dvb_charset_find(NULL, NULL, svc);
  while (dllen > 2) {
    dtag = ptr[0];
    dlen = ptr[1];

    dllen -= 2;
    ptr   += 2;
    if (dllen < dlen) break;

    tvhtrace(mod->subsys, "%s:  dtag %02X dlen %d", mod->id, dtag, dlen);
    tvhlog_hexdump(mod->subsys, ptr, dlen);

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
        r = _eit_desc_crid(mod, ptr, dlen, &ev, ed);
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

  /* Do all scraping here, outside the global lock.
   *
   * We search across all the main fields using the same regex and
   * merge the results with the last match taking precendence.  So if
   * EIT has episode in title and a different one in the description
   * then we use the one from the description.
   */
  if (eit_mod->scrape_episode) {
    if (ev.title)
      _eit_scrape_episode(ev.title, eit_mod, &ev);
    if (ev.desc)
      _eit_scrape_episode(ev.desc, eit_mod, &ev);
    if (ev.summary)
      _eit_scrape_episode(ev.summary, eit_mod, &ev);
  }

  if (ev.summary && eit_mod->scrape_subtitle)
    _eit_scrape_subtitle(eit_mod, &ev);

  if (lock)
    pthread_mutex_lock(&global_lock);
  if (svc) {
    LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
      ch = (channel_t *)ilm->ilm_in2;
      if (!ch->ch_enabled || ch->ch_epg_parent) continue;
      if (_eit_process_event_one(mod, tableid, sect, svc, ch,
                                 &ev, ptr0, len0, local, save) < 0)
        break;
    }
  }
  if (lock)
    pthread_mutex_unlock(&global_lock);

#if TODO_ADD_EXTRA
  if (ev.extra)    htsmsg_destroy(ev.extra);
#endif
  if (ev.genre)    epg_genre_list_destroy(ev.genre);
  if (ev.title)    lang_str_destroy(ev.title);
  if (ev.subtitle) lang_str_destroy(ev.subtitle);
  if (ev.summary)  lang_str_destroy(ev.summary);
  if (ev.desc)     lang_str_destroy(ev.desc);

  if (ilm)
    return -1;
  return 12 + (((ptr0[10] & 0x0f) << 8) | ptr0[11]);
}

static void
_eit_process_data(void *m, void *data, uint32_t len)
{
  int save = 0, r;
  eit_data_t ed;

  assert(len >= sizeof(ed));
  memcpy(&ed, data, sizeof(ed));
  data += sizeof(ed);
  len -= sizeof(ed);

  while (len) {
    if ((r = _eit_process_event(m, ed.tableid, ed.sect, &ed,
                                data, len, ed.local_time, &save, 1)) < 0)
      break;
    assert(r > 0);
    len -= r;
    data += r;
  }

  if (save) {
    pthread_mutex_lock(&global_lock);
    epg_updated();
    pthread_mutex_unlock(&global_lock);
  }
}

static void
_eit_process_immediate(void *m, const void *ptr, uint32_t len, eit_data_t *ed)
{
  int save = 0, r;

  while (len) {
    if ((r = _eit_process_event(m, ed->tableid, ed->sect, ed,
                                ptr, len, ed->local_time, &save, 0)) < 0)
      break;
    assert(r > 0);
    len -= r;
    ptr += r;
  }

  if (save)
    epg_updated();
}

static int
_eit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver, spec;
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
  eit_data_t            data;
  const char           *cridauth;
  char ubuf[UUID_HEX_SIZE];

  if (!epggrab_ota_running)
    return -1;

  mm  = mt->mt_mux;
  map = mt->mt_opaque;
  mod = (epggrab_module_t *)map->om_module;
  spec = ((eit_private_t *)((epggrab_module_ota_t *)mod)->opaque)->spec;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mm, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  /* Validate */
  if(tableid < 0x4e || tableid > 0x6f || len < 11) {
    if (ths)
      atomic_add(&ths->ths_total_err, 1);
    return -1;
  }

  /* Basic info */
  sid     = ptr[0] << 8 | ptr[1];
  tsid    = ptr[5] << 8 | ptr[6];
  onid    = ptr[7] << 8 | ptr[8];
  seg     = ptr[9];
  extraid = ((uint32_t)tsid << 16) | sid;
  // TODO: extra ID should probably include onid

  tvhtrace(LS_TBL_EIT, "%s: sid %i tsid %04x onid %04x seg %02x",
           mt->mt_name, sid, tsid, onid, seg);

  /* Local EIT contents - give them another priority to override main events */
  if (spec == EIT_SPEC_NZ_FREEVIEW &&
      ((tsid > 0x19 && tsid < 0x1d) ||
       (tsid > 0x1e && tsid < 0x21)))
    mod = epggrab_module_find_by_id("nz_freeview");

  /* Register interest */
  if (tableid == 0x4e || (tableid >= 0x50 && tableid < 0x60) ||
      spec == EIT_SPEC_UK_FREESAT /* uk_freesat hack */)
    ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, extraid, 11, &st, &sect, &last, &ver);
  if (r == 0) goto complete;
  if (r < 0) return r;
  if (tableid != 0x4e && r != 1) return r;
  if (st && r > 0) {
    uint32_t mask;
    int sa = seg & 0xF8;
    int sb = 7 - (seg & 0x07);
    mask = (~(0xFF << sb) & 0xFF);
    mask <<= (24 - (sa % 32));
    st->sections[sa/32] &= ~mask;
  }
  
  /* UK Cable Virgin: EPG data for services in other transponders is transmitted 
  // in the 'actual' transpoder table IDs */
  if (spec == EIT_SPEC_UK_CABLE_VIRGIN && (tableid == 0x50 || tableid == 0x4E)) {
    mm = mpegts_network_find_mux(mm->mm_network, onid, tsid, 1);
  }
  if(!mm)
    goto done;

  /* Get transport stream */
  // Note: tableid=0x4f,0x60-0x6f is other TS
  //       so must find the tdmi
  if (tableid == 0x4f || tableid >= 0x60) {
    mm = mpegts_network_find_mux(mm->mm_network, onid, tsid, 1);
  } else {
    if ((mm->mm_tsid != tsid || mm->mm_onid != onid) &&
        !mm->mm_eit_tsid_nocheck) {
      if (mm->mm_onid != MPEGTS_ONID_NONE &&
          mm->mm_tsid != MPEGTS_TSID_NONE)
        tvhtrace(LS_TBL_EIT,
                "%s: invalid tsid found tid 0x%02X, onid:tsid %d:%d != %d:%d",
                mt->mt_name, tableid, mm->mm_onid, mm->mm_tsid, onid, tsid);
      mm = NULL;
    }
  }
  if(!mm)
    goto done;

  /* Get service */
  svc = mpegts_mux_find_service(mm, sid);
  if (!svc) {
    /* NZ Freesat: use main data */
    if (spec == EIT_SPEC_NZ_FREEVIEW && onid == 0x222a &&
        (tsid == 0x19 || tsid == 0x1d)) {
      svc = mpegts_network_find_active_service(mm->mm_network, sid, &mm);
      if (svc)
        goto svc_ok;
    }

    tvhtrace(LS_TBL_EIT, "sid %i not found", sid);
    goto done;
  }

svc_ok:
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

  /* Queue events */
  len -= 11;
  ptr += 11;
  if (len >= 12) {
    data.tableid = tableid;
    data.sect = sect;
    data.svc_uuid = svc->s_id.in_uuid;
    data.onid = onid;
    data.local_time = mm->mm_network->mn_localtime;
    cridauth = svc->s_dvb_cridauth;
    if (!cridauth)
      cridauth = svc->s_dvb_mux->mm_crid_authority;
    if (cridauth) {
      if (strlen(cridauth) + 1 > sizeof(data.cridauth))
        tvherror(LS_TBL_EIT, "cridauth overflow");
      strncpy(data.cridauth, cridauth, sizeof(data.cridauth)-1);
      data.cridauth[sizeof(cridauth)-1] = '\0';
    } else {
      data.cridauth[0] = '\0';
    }
    if (((eit_module_t *)mod)->running_immediate && tableid == 0x4e) {
      /* handle running state immediately */
      _eit_process_immediate(mod, ptr, len, &data);
    } else {
      /* handle those data later */
      epggrab_queue_data(mod, &data, sizeof(data), ptr, len);
    }
  }

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
complete:
  if (ota && !r && (tableid >= 0x50 && tableid < 0x60))
    epggrab_ota_complete((epggrab_module_ota_t*)mod, ota);
  
  return r;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static int _eit_start
  ( epggrab_ota_map_t *map, mpegts_mux_t *dm )
{
  epggrab_module_ota_t *m = map->om_module, *eit = NULL;
  eit_private_t *priv = (eit_private_t *)m->opaque;
  int pid, opts = 0, spec;

  /* Disabled */
  if (!m->enabled && !map->om_forced) return -1;

  spec = priv->spec;

  /* Do string conversions also for the EIT table */
  /* FIXME: It should be done only for selected muxes or networks */
  if (((eit_private_t *)m->opaque)->conv) {
    eit = (epggrab_module_ota_t*)epggrab_module_find_by_id("eit");
    ((eit_private_t *)eit->opaque)->spec = priv->spec;
    ((eit_private_t *)eit->opaque)->conv = priv->conv;
  }

  if (spec == EIT_SPEC_NZ_FREEVIEW) {
    if (eit == NULL)
      eit = (epggrab_module_ota_t*)epggrab_module_find_by_id("eit");
    if (eit->enabled)
      map->om_complete = 1;
  }

  pid = priv->pid;

  /* Freeview UK/NZ (switch to EIT, ignore if explicitly enabled) */
  /* Note: do this as PID is the same */
  if (pid == 0 && strcmp(m->id, "eit")) {
    if (eit == NULL)
      eit = (epggrab_module_ota_t*)epggrab_module_find_by_id("eit");
    if (eit->enabled) return -1;
  }

  /* Freesat (3002/3003) */
  if (pid == 3003 && !strcmp("uk_freesat", m->id))
    mpegts_table_add(dm, DVB_BAT_BASE, DVB_BAT_MASK, dvb_bat_callback, NULL,
                     "bat", LS_TBL_BASE, MT_CRC, 3002, MPS_WEIGHT_EIT);

  /* Standard (0x12) */
  if (pid == 0) {
    pid  = DVB_EIT_PID;
    opts = MT_RECORD;
  }

  mpegts_table_add(dm, 0, 0, _eit_callback, map, m->id, LS_TBL_EIT, MT_CRC | opts, pid, MPS_WEIGHT_EIT);
  // TODO: might want to limit recording to EITpf only
  tvhdebug(m->subsys, "%s: installed table handlers", m->id);
  return 0;
}

static int _eit_activate(void *m, int e)
{
  eit_module_t *mod = m;
  tvhtrace(LS_TBL_EIT, "_eit_activate %s change to %d from %d with scrape_episode of %d and scrape_subtitle of %d",
           mod->id, e, mod->active, mod->scrape_episode, mod->scrape_subtitle);
  const int original_status = mod->active;

  /* We expect to be activated/deactivated infrequently so free up the
   * lists read from the config files and reload when the scraper is
   * activated. This allows user to modify the config files and get
   * them re-read easily.
   */
  _eit_scrape_clear(mod);

  mod->active = e;

  if (e) {
    _eit_module_load_config(mod);
  }

  /* Return save if value has changed */
  return e != original_status;
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

static void _eit_scrape_clear(eit_module_t *mod)
{
  eit_pattern_free_list(&mod->p_snum);
  eit_pattern_free_list(&mod->p_enum);
  eit_pattern_free_list(&mod->p_airdate);
  eit_pattern_free_list(&mod->p_scrape_subtitle);
  eit_pattern_free_list(&mod->p_is_new);
}

static int _eit_scrape_load_one ( htsmsg_t *m, eit_module_t* mod )
{
  if (mod->scrape_episode) {
    eit_pattern_compile_list(&mod->p_snum, htsmsg_get_list(m, "season_num"));
    eit_pattern_compile_list(&mod->p_enum, htsmsg_get_list(m, "episode_num"));
    eit_pattern_compile_list(&mod->p_airdate, htsmsg_get_list(m, "airdate"));
    eit_pattern_compile_list(&mod->p_is_new, htsmsg_get_list(m, "is_new"));
  }

  if (mod->scrape_subtitle) {
    eit_pattern_compile_list(&mod->p_scrape_subtitle, htsmsg_get_list(m, "scrape_subtitle"));
  }

  return 1;
}

static void _eit_module_load_config(eit_module_t *mod)
{
  if (!mod->scrape_episode && !mod->scrape_subtitle) {
    tvhinfo(LS_TBL_EIT, "module %s - scraper disabled by config", mod->id);
    return;
  }

  const char config_path[] = "epggrab/eit/scrape/%s";
  /* Only use the user config if they have supplied one and it is not empty.
   * Otherwise we default to using configuration based on the module
   * name such as "uk_freeview".
   */
  const char *config_file = mod->scrape_config && *mod->scrape_config ?
    mod->scrape_config : mod->id;

  tvhinfo(LS_TBL_EIT, "scraper %s attempt to load config \"%s\"", mod->id, config_file);

  htsmsg_t *m = hts_settings_load(config_path, config_file);
  char *generic_name = NULL;
  if (!m) {
    /* No config file so try loading a generic config based on
     * the first component of the id. In the above case it would
     * be "uk". This allows config for a country to be shared across
     * two grabbers such as DVB-T and DVB-S.
     */
    generic_name = strdup(config_file);
    if (generic_name) {
      char *underscore = strstr(generic_name, "_");
      if (underscore) {
        /* Terminate the string at the underscore */
        *underscore = 0;
        config_file = generic_name;
        m = hts_settings_load(config_path, config_file);
      }
    }
  }

  if (m) {
    const int r = _eit_scrape_load_one(m, mod);
    if (r > 0)
      tvhinfo(LS_TBL_EIT, "scraper %s loaded config \"%s\"", mod->id, config_file);
    else
      tvhwarn(LS_TBL_EIT, "scraper %s failed to load config \"%s\"", mod->id, config_file);
    htsmsg_destroy(m);
  } else {
      tvhinfo(LS_TBL_EIT, "scraper %s no scraper config files found", mod->id);
  }

  if (generic_name)
    free(generic_name);
}

static const idclass_t epggrab_mod_eit_class = {
  .ic_super      = &epggrab_mod_ota_scraper_class,
  .ic_class      = "epggrab_mod_eit",
  .ic_caption    = N_("Over-the-air EIT EPG grabber"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_BOOL,
      .id     = "running_immediate",
      .name   = N_("Running state immediately"),
      .desc   = N_("Handle the running state (EITp/f) immediately. "
                   "Usually, keep this off. It might increase "
                   "the recordings accuracy on very slow systems."),
      .off    = offsetof(eit_module_t, running_immediate),
      .group  = 1,
      .opts   = PO_EXPERT,
    },
    {}
  }
};

static eit_module_t *eit_module_ota_create
  ( const char *id, int subsys, const char *saveid,
    const char *name, int priority,
    epggrab_ota_module_ops_t *ops )
{
  eit_module_t * mod = (eit_module_t *)
    epggrab_module_ota_create(calloc(1, sizeof(eit_module_t)),
                              id, subsys, saveid, name, priority,
                              &epggrab_mod_eit_class, ops);
  return mod;
}

#define EIT_OPS(name, _pid, _conv, _spec) \
  static eit_private_t opaque_##name = { \
    .pid = (_pid), \
    .conv = (_conv), \
    .spec = (_spec), \
  }; \
  static epggrab_ota_module_ops_t name = { \
    .start  = _eit_start, \
    .done = _eit_done, \
    .activate = _eit_activate, \
    .process_data = _eit_process_data, \
    .tune   = _eit_tune, \
    .opaque = &opaque_##name, \
  }

#define EIT_CREATE(id, name, prio, ops) \
  eit_module_ota_create(id, LS_TBL_EIT, NULL, name, prio, ops)

void eit_init ( void )
{
  EIT_OPS(ops, 0, 0, 0);
  EIT_OPS(ops_uk_freesat, 3003, EIT_CONV_HUFFMAN, EIT_SPEC_UK_FREESAT);
  EIT_OPS(ops_uk_freeview, 0, EIT_CONV_HUFFMAN, 0);
  EIT_OPS(ops_nz_freeview, 0, EIT_CONV_HUFFMAN, EIT_SPEC_NZ_FREEVIEW);
  EIT_OPS(ops_baltic, 0x39, 0, 0);
  EIT_OPS(ops_bulsat, 0x12b, 0, 0);
  EIT_OPS(ops_uk_cable_virgin, 0x2bc, 0, EIT_SPEC_UK_CABLE_VIRGIN);

  EIT_CREATE("eit", "EIT: DVB Grabber", 1, &ops);
  EIT_CREATE("uk_freesat", "UK: Freesat", 5, &ops_uk_freesat);
  EIT_CREATE("uk_freeview", "UK: Freeview", 5, &ops_uk_freeview);
  EIT_CREATE("nz_freeview", "New Zealand: Freeview", 5, &ops_nz_freeview);
  EIT_CREATE("viasat_baltic", "VIASAT: Baltic", 5, &ops_baltic);
  EIT_CREATE("Bulsatcom_39E", "Bulsatcom: Bula 39E", 5, &ops_bulsat);
  EIT_CREATE("uk_cable_virgin", "UK: Cable Virgin", 5, &ops_uk_cable_virgin);
}

void _eit_done ( void *m )
{
  eit_module_t *mod = m;
  _eit_scrape_clear(mod);
}

void eit_done ( void )
{
}

void eit_load ( void )
{
}
