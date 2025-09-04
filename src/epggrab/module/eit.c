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
#include "ratinglabels.h"

/* ************************************************************************
 * Opaque
 * ***********************************************************************/

typedef struct eit_nit {
  LIST_ENTRY(eit_nit) link;
  char *name;
  uint16_t onid[32];
  uint16_t tsid[32];
  uint16_t nbid[32];
  int onid_count;
  int tsid_count;
  int nbid_count;
} eit_nit_t;

typedef struct eit_sdt {
  LIST_ENTRY(eit_sdt) link;
  uint16_t onid[32];
  uint16_t tsid[32];
  int onid_count;
  int tsid_count;
} eit_sdt_t;

typedef struct eit_private
{
  TAILQ_ENTRY(eit_private) link;
  lang_str_t *name;
  epggrab_module_ota_t *module;
  uint16_t pid;
  uint16_t bat_pid;
  int conv;
  uint32_t sdt_enable;
  uint32_t hacks;
  uint32_t priv;
  char slave[32];
  LIST_HEAD(, eit_nit) nit;
  LIST_HEAD(, eit_sdt) sdt;
  epggrab_ota_module_ops_t *ops;
} eit_private_t;

#define EIT_CONV_HUFFMAN            1

#define EIT_HACK_INTEREST4E         (1<<0)
#define EIT_HACK_EXTRAMUXLOOKUP     (1<<1)
#define EIT_HACK_SVCNETLOOKUP       (1<<2)

/* Queued data structure */
typedef struct eit_data
{
  tvh_uuid_t svc_uuid;
  uint16_t   onid;
  int        tableid;
  int        sect;
  int        local_time;
  uint16_t   charset_len;
  uint16_t   cridauth_len;
  uint8_t    data[0];
} eit_data_t;

/* Provider configuration */
typedef struct eit_module_t
{
  epggrab_module_ota_scraper_t  ;      ///< Base struct
  int short_target;
  int running_immediate;               ///< Handle quickly the events from the current table
  eit_pattern_list_t p_snum;
  eit_pattern_list_t p_enum;
  eit_pattern_list_t p_airdate;        ///< Original air date parser
  eit_pattern_list_t p_scrape_title;   ///< Scrape title from title + summary data
  eit_pattern_list_t p_scrape_subtitle;///< Scrape subtitle from summary data
  eit_pattern_list_t p_scrape_summary; ///< Scrape summary from summary data
  eit_pattern_list_t p_is_new;         ///< Is programme new to air
} eit_module_t;

static TAILQ_HEAD(, eit_private) eit_private_list;

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct eit_event
{
  char              uri[529];
  char              suri[529];

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
  ratinglabel_t     *rating_label;

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
    lang_str_add(ev->title, buf, lang);
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
    lang_str_add(ev->summary, buf, lang);
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
  uint8_t tempPtr = 0;  //Temporary variable to hold a (potentially) changed *ptr value.
  tempPtr = *ptr;
  while (len > 1) {
    //If the genre translation table has been loaded, it will not be a null pointer.
    if (epggrab_ota_genre_translation){
      //Get the potentially new genre value.
      tempPtr = epggrab_ota_genre_translation[*ptr];
      //If we did get a translation, write a trace for debugging.
      if(tempPtr != *ptr)
      {
        tvhtrace(LS_TBL_EIT, "Translating '%d' (0x%02x) to '%d' (0x%02x)", *ptr, *ptr, tempPtr, tempPtr);
      }
    }//END genre translation table loaded.

    if (tempPtr == 0xb1)  //0xB1 is the genre code for 'Black and White'
      ev->bw = 1;
    else if (tempPtr < 0xb0) {  //0xB0 is the start of the 'Special Characteristics' block.
      if (!ev->genre) ev->genre = calloc(1, sizeof(epg_genre_list_t));
      epg_genre_list_add_by_eit(ev->genre, (const uint8_t)tempPtr);  //Cast as a 'const'
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
  int cnt = 0, sum = 0, i = 3;

  char            tmpCountry[4];
  int             tmpAge = 0;
  ratinglabel_t   *rl = NULL;

  while (len > 3) {

    //If we are processing parental rating labels.
    if(epggrab_conf.epgdb_processparentallabels)
    {
      //Get the recommended age for this rating.
      //0x00 undefined
      //0x01 to 0x0F minimum age = rating + 3 years
      //0x10 to 0xFF defined by the broadcaster
      if(ptr[i] == 0)
      {
        tmpAge = 0;
      }
      else
      {
        tmpAge = ptr[i];  //Do not add 3 here, do that with the 'display age'.
      }

      //Get the country code for this rating.
      tmpCountry[0] = ptr[0];
      tmpCountry[1] = ptr[1];
      tmpCountry[2] = ptr[2];
      tmpCountry[3] = 0;

      tvhtrace(LS_TBL_EIT, "Country '%s', age '%d'", tmpCountry, tmpAge);

      //Look for a matching rating label
      rl = ratinglabel_find_from_eit(tmpCountry, tmpAge);

      //If we have found a rating label, save the details and exit.
      //ie, ony use the first parental rating found.
      //TODO: In future, if (eg in Europe) the rating codes from multiple
      //countries are present, select the one that user prefers.
      //A new config option will be needed for this.
      //HOWEVER: A sampling of EIT data from European users
      //suggests that this will not be necessary.
      if(rl){
        ev->parental = rl->rl_display_age;
        ev->rating_label = rl;
        return 0;
      }
    }//END rating labels are being processed.
    //If rating labels are not processed, do the original TVH process.

    if ( ptr[i] && ptr[i] < 0x10 ) {
      cnt++;
      sum += (ptr[i] + 3);
    }
    len -= 4;
    i   += 4;
  }//END loop through descriptors
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
          strlcpy(crid, buf, clen);
        } else if (*buf != '/') {
          snprintf(crid, clen, "crid://%s", buf);
        } else {
          if (ed->cridauth_len)
            snprintf(crid, clen, "crid://%s%s", ed->data, buf);
          else
            snprintf(crid, clen, "crid://onid-%d%s", ed->onid, buf);
        }
      }

      /* Next */
      len -= 1 + r;
      ptr += 1 + r;
    } else {
      break;
    }
  }
  if (len > 3) {
    return -1;
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
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_snum))
      if ((ev->en.s_num = positive_atoi(buffer))) {
        tvhtrace(LS_TBL_EIT,"  extract season number %d using %s", ev->en.s_num, eit_mod->id);
        break;
      }
  }

  /* ...for episode number */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_enum))
     if ((ev->en.e_num = positive_atoi(buffer))) {
       tvhtrace(LS_TBL_EIT,"  extract episode number %d using %s", ev->en.e_num, eit_mod->id);
       break;
     }
  }

  /* Extract original air date year */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_airdate)) {
      if (strlen(buffer) == 4) {
        /* Year component only, so assume it is the copyright year. */
        ev->copyright_year = positive_atoi(buffer);
        break;
      }
    }
  }

  /* Extract is_new flag. Any match is assumed to mean "new" */
  RB_FOREACH(se, str, link) {
    if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_is_new)) {
      ev->is_new = 1;
      break;
    }
  }
}

/* Scrape title/subtitle/summary data from the broadcast data.
 * @param eit_mod - our module with regex to use.
 * @param ev - [out] modified event data.
 */
static void
_eit_scrape_text(eit_module_t *eit_mod, eit_event_t *ev)
{
  lang_str_ele_t *se;
  char buffer[2048];

  if (!ev->summary)
    return;

  /* UK Freeview/Freesat have a subtitle as part of the summary in the format
   * "subtitle: desc". They may also have the title continue into the
   * summary. So if configured, run scrapers for the title, the subtitle
   * and the summary (the latter to tidy up).
   */
  if (ev->title && eit_mod->scrape_title) {
    char title_summary[2048];
    lang_str_t *ls = lang_str_create();
    RB_FOREACH(se, ev->title, link) {
      snprintf(title_summary, sizeof(title_summary), "%s %% %s",
               se->str, lang_str_get(ev->summary, se->lang));
      if (eit_pattern_apply_list(buffer, sizeof(buffer), title_summary, se->lang, &eit_mod->p_scrape_title)) {
        tvhtrace(LS_TBL_EIT, "  scrape title '%s' from '%s' using %s",
                 buffer, title_summary, eit_mod->id);
        lang_str_set(&ls, buffer, se->lang);
      }
    }
    lang_str_set_multi(&ev->title, ls);
    lang_str_destroy(ls);
  }

  if (eit_mod->scrape_subtitle) {
    RB_FOREACH(se, ev->summary, link) {
      if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_scrape_subtitle)) {
        tvhtrace(LS_TBL_EIT, "  scrape subtitle '%s' from '%s' using %s",
                 buffer, se->str, eit_mod->id);
        lang_str_set(&ev->subtitle, buffer, se->lang);
      }
    }
  }

  if (eit_mod->scrape_summary) {
    lang_str_t *ls = lang_str_create();
    RB_FOREACH(se, ev->summary, link) {
      if (eit_pattern_apply_list(buffer, sizeof(buffer), se->str, se->lang, &eit_mod->p_scrape_summary)) {
        tvhtrace(LS_TBL_EIT, "  scrape summary '%s' from '%s' using %s",
                 buffer, se->str, eit_mod->id);
        lang_str_set(&ls, buffer, se->lang);
      }
    }
    lang_str_set_multi(&ev->summary, ls);
    lang_str_destroy(ls);
  }
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
  epg_running_t run;
  epg_changes_t changes = 0;
  char tm1[32], tm2[32];
  int short_target = ((eit_module_t *)mod)->short_target;

  lang_str_t *temp_string;        //}
  lang_str_ele_t *temp_ele;       //} Used for appending/prepending to the desc.
  const char *temp_s1, *temp_s2;  //}

  /* Core fields */
  eid   = ptr[0] << 8 | ptr[1];
  start = dvb_convert_date(&ptr[2], local);
  stop  = start + bcdtoint(ptr[7] & 0xff) * 3600 +
                  bcdtoint(ptr[8] & 0xff) * 60 +
                  bcdtoint(ptr[9] & 0xff);
  running = (ptr[10] >> 5) & 0x07;

  if (epg_channel_ignore_broadcast(ch, start))
    return 0;

  /* Find broadcast */
  ebc  = epg_broadcast_find_by_time(ch, mod, start, stop, 1, &save2, &changes);
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
    _ebc.dvb_eid = eid;
    _ebc.start = start;
    _ebc.stop = stop;
    _ebc.episodelink = epg_set_broadcast_find_by_uri(&epg_episodelinks, ev->uri);
    _ebc.serieslink = epg_set_broadcast_find_by_uri(&epg_serieslinks, ev->suri);
    _ebc.title = lang_str_copy(ev->title);
    ebc = epg_match_now_next(ch, &_ebc);
    tvhtrace(mod->subsys, "%s:  running state only ebc=%p", svc->s_dvb_svcname ?: "(null)", ebc);
    lang_str_destroy(_ebc.title);
    goto running;
  } else {
    *save = save2;
  }

  /*
   * Broadcast
   */

  *save |= epg_broadcast_set_dvb_eid(ebc, eid, &changes);

  /* Summary/Description */
  if (ev->summary)
    if (short_target != 0 ||
        (ev->subtitle && lang_str_compare(ev->summary, ev->subtitle)))
      *save |= epg_broadcast_set_summary(ebc, ev->summary, &changes);
  if (ev->desc)
    *save |= epg_broadcast_set_description(ebc, ev->desc, &changes);

  /* Broadcast Metadata */
  *save |= epg_broadcast_set_is_hd(ebc, ev->hd, &changes);
  *save |= epg_broadcast_set_is_widescreen(ebc, ev->ws, &changes);
  *save |= epg_broadcast_set_is_audio_desc(ebc, ev->ad, &changes);
  *save |= epg_broadcast_set_is_subtitled(ebc, ev->st, &changes);
  *save |= epg_broadcast_set_is_deafsigned(ebc, ev->ds, &changes);

  /*
   * Series link
   */

  if (*ev->suri)
    *save |= epg_broadcast_set_serieslink_uri(ebc, ev->suri, &changes);

  /*
   * Episode
   */

  /* Find episode */
  if (*ev->uri)
    *save |= epg_broadcast_set_episodelink_uri(ebc, ev->uri, &changes);

  /* Update Episode */
  if (ev->is_new)
    *save |= epg_broadcast_set_is_new(ebc, 1, &changes);
  *save |= epg_broadcast_set_is_bw(ebc, ev->bw, &changes);
  if (ev->title)
    *save |= epg_broadcast_set_title(ebc, ev->title, &changes);
  if (ev->genre)
    *save |= epg_broadcast_set_genre(ebc, ev->genre, &changes);
  if (ev->parental)
    *save |= epg_broadcast_set_age_rating(ebc, ev->parental, &changes);

  if (ev->rating_label)
    {
    tvhtrace(mod->subsys, "About to save rating label '%p'", ev->rating_label);
    *save |= epg_broadcast_set_rating_label(ebc, ev->rating_label, &changes);
    }

  /*
      Notes on EPG fields. DMC, May 2025.

      ev->title :   Originates from EIT tag 0x4d 'short_event_descriptor' 'event name'
      ev->summary : Originates from EIT tag 0x4d 'short_event_descriptor' 'text describing the event'
      ev->desc :    Originates from EIT tag 0c4e 'extended_event_descriptor'
      ev-subtitle : Originates from scraping operations performed by previous functions.

      If the grabber scraping option is enabled:
      ev->title, ev->subtitle, ev->summary may be set/update/replaced by scraped values.

      Eventually, if the sub-title is empty, the summary will be used in its place.

      Summary of possible EPG Text manipulation operations.

      Defined in EIT EPG Grabber
      'Set the short EPG description to given target'
      short_target: *0 = Subtitle* | 1 = Summary | 2 = Subtitle and summary

      (New features from here..)
      Defined in MPGTS Service
      svc->s_dvb_subtitle_processing
      [*None* | Save in Description | Append to Description | Prepend to Description]
      This is only active if short_target == 0 so that the EPG grabber setting
      takes priority over the service setting.
      
      svc->s_dvb_ignore_matching_subtitle [True | *False*]
      If the Sub-title and the Title contain identical content, ignore the Sub-title and only save the Title.
   */

  //If processing is enabled AND the grabber is saving the sub-title in the sub-title then
  //save the sub-title in the description provided that the description is also empty OR
  //append/prepend the sub-title to the description if the description is not empty.
  if ((svc->s_dvb_subtitle_processing != SVC_PROCESS_SUBTITLE_NONE) && (ev->summary || ev->subtitle) && short_target == 0)
  {
      //If there is not already a description, just use the subtitle/summary instead
      if(!ev->desc)
      {
        //If there was a sub-title scraped, use that.
        if (ev->subtitle)
        {
          *save |= epg_broadcast_set_description(ebc, ev->subtitle, &changes);
          lang_str_destroy(ev->subtitle);
          ev->subtitle = lang_str_create();
          tvhtrace(LS_TBL_EIT, "Description set to sub-title.");
        }
        else
        {
          *save |= epg_broadcast_set_description(ebc, ev->summary, &changes);
          lang_str_destroy(ev->summary);
          ev->summary = lang_str_create();
          tvhtrace(LS_TBL_EIT, "Description set to summary.");
        }
      }
      else //There is a description present so append/prepend as required.
      {
        temp_string = lang_str_create();
        if(svc->s_dvb_subtitle_processing == SVC_PROCESS_SUBTITLE_APPEND)
        {
          if(ev->subtitle)
          {
            RB_FOREACH(temp_ele, ev->subtitle, link) {
              temp_s1 = lang_str_get(ev->desc, temp_ele->lang);
              lang_str_append(temp_string, temp_s1, temp_ele->lang);
              lang_str_append(temp_string, " ", temp_ele->lang);
              temp_s2 = lang_str_get(ev->subtitle, temp_ele->lang);
              lang_str_append(temp_string, temp_s2, temp_ele->lang);
              tvhtrace(LS_TBL_EIT, "Sub-title appended to description.");
            }
          }
          else if(ev->summary)
          {
            RB_FOREACH(temp_ele, ev->summary, link) {
              temp_s1 = lang_str_get(ev->desc, temp_ele->lang);
              lang_str_append(temp_string, temp_s1, temp_ele->lang);
              lang_str_append(temp_string, " ", temp_ele->lang);
              temp_s2 = lang_str_get(ev->summary, temp_ele->lang);
              lang_str_append(temp_string, temp_s2, temp_ele->lang);
              tvhtrace(LS_TBL_EIT, "Summary appended to description.");
            }
          }
        }//END Append

        if(svc->s_dvb_subtitle_processing == SVC_PROCESS_SUBTITLE_PREPEND)
        {
          if(ev->subtitle)
          {
            RB_FOREACH(temp_ele, ev->subtitle, link) {
              temp_s1 = lang_str_get(ev->subtitle, temp_ele->lang);
              lang_str_append(temp_string, temp_s1, temp_ele->lang);
              lang_str_append(temp_string, " ", temp_ele->lang);
              temp_s2 = lang_str_get(ev->desc, temp_ele->lang);
              lang_str_append(temp_string, temp_s2, temp_ele->lang);
              tvhtrace(LS_TBL_EIT, "Sub-title prepended to description.");
            }
          }
          else if(ev->summary)
          {
            RB_FOREACH(temp_ele, ev->summary, link) {
              temp_s1 = lang_str_get(ev->summary, temp_ele->lang);
              lang_str_append(temp_string, temp_s1, temp_ele->lang);
              lang_str_append(temp_string, " ", temp_ele->lang);
              temp_s2 = lang_str_get(ev->desc, temp_ele->lang);
              lang_str_append(temp_string, temp_s2, temp_ele->lang);
              tvhtrace(LS_TBL_EIT, "Summary prepended to description.");
            }
          }
        }//END Prepend

        //Save the new desc with the appended/prepended content.
        *save |= epg_broadcast_set_description(ebc, temp_string, &changes);

        //Nuke the summary because it is now part of the desc.
        if(ev->summary)
        {
          lang_str_destroy(ev->summary);
          ev->summary = lang_str_create();
        }

        //Nuke the subtitle because it is now part of the desc.
        if(ev->subtitle)
        {
          lang_str_destroy(ev->subtitle);
          ev->subtitle = lang_str_create();
        }

        //Clean up the temp language string.
        if (temp_string)    lang_str_destroy(temp_string);

      }//END append or prepend
  }//END DVB sub-title processing

  //If processing is enabled, delete the summary/sub-title if it is the same as the title.
  if (svc->s_dvb_ignore_matching_subtitle && (ev->summary || ev->subtitle))
  {
      if (lang_str_compare(ev->title, ev->summary) == 0)  //0 = no differences
      {
          lang_str_destroy(ev->summary);
          ev->summary = lang_str_create();
          tvhtrace(LS_TBL_EIT, "Deleting summary, same as title.");
      }

      if (lang_str_compare(ev->title, ev->subtitle) == 0)  //0 = no differences
      {
          lang_str_destroy(ev->subtitle);
          ev->subtitle = lang_str_create();
          tvhtrace(LS_TBL_EIT, "Deleting sub-title, same as title.");
      }
  }

  //The sub-title is set by scraping it from the EIT short description (held in the summary).
  if (ev->subtitle)
    *save |= epg_broadcast_set_subtitle(ebc, ev->subtitle, &changes);
    //short_target: 0 = Subtitle | 2 = Subtitle and summary
  else if ((short_target == 0 || short_target == 2) && ev->summary)
    *save |= epg_broadcast_set_subtitle(ebc, ev->summary, &changes);
#if TODO_ADD_EXTRA
  if (ev->extra)
    *save |= epg_broadcast_set_extra(ebc, extra, &changes);
#endif
  /* save any found episode number */
  if (ev->en.s_num || ev->en.e_num || ev->en.p_num)
    *save |= epg_broadcast_set_epnum(ebc, &ev->en, &changes);
  if (ev->first_aired > 0)
    *save |= epg_broadcast_set_first_aired(ebc, ev->first_aired, &changes);
  if (ev->copyright_year > 0)
    *save |= epg_broadcast_set_copyright_year(ebc, ev->copyright_year, &changes);

  *save |= epg_broadcast_change_finish(ebc, changes, 0);


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

  return 0;
}

static int _eit_process_event
  ( epggrab_module_t *mod, eit_data_t *ed,
    const uint8_t *ptr0, int len0,
    int *save, int lock )
{
  eit_module_t *eit_mod = (eit_module_t *)mod;
  idnode_list_mapping_t *ilm = NULL;
  mpegts_service_t *svc;
  channel_t *ch;
  eit_event_t ev;
  const int tableid = ed->tableid;
  const int sect = ed->sect;
  const int local = ed->local_time;
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
  if (ed->charset_len)
    ev.default_charset = (char *)ed->data + ed->cridauth_len;
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
        if(epggrab_conf.epgdb_processparentallabels){
            if(ev.rating_label){
              tvhtrace(mod->subsys, "RATINGLABEL '%d'  '%s'", ev.parental, ev.rating_label->rl_display_label);
            } else {
              tvhtrace(mod->subsys, "RATINGLABEL '%d'  '<NONE>'", ev.parental);
            }
        }

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

  _eit_scrape_text(eit_mod, &ev);

  if (lock)
    tvh_mutex_lock(&global_lock);
  svc = (mpegts_service_t *)service_find_by_uuid0(&ed->svc_uuid);
  if (svc && eit_mod->opaque) {
    LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
      ch = (channel_t *)ilm->ilm_in2;
      if (!ch->ch_enabled || ch->ch_epg_parent) continue;
      if (_eit_process_event_one(mod, tableid, sect, svc, ch,
                                 &ev, ptr0, len0, local, save) < 0)
        break;
    }
  }
  if (lock)
    tvh_mutex_unlock(&global_lock);

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
  size_t hlen;
  eit_data_t *ed = data;

  assert(len >= sizeof(*ed));
  hlen = sizeof(*ed) + ed->cridauth_len + ed->charset_len;
  assert(len >= hlen);
  data += hlen;
  len -= hlen;

  while (len) {
    if ((r = _eit_process_event(m, ed, data, len, &save, 1)) < 0)
      break;
    assert(r > 0);
    len -= r;
    data += r;
  }

  if (save) {
    tvh_mutex_lock(&global_lock);
    epg_updated();
    tvh_mutex_unlock(&global_lock);
  }
}

static void
_eit_process_immediate(void *m, const void *ptr, uint32_t len, eit_data_t *ed)
{
  int save = 0, r;

  while (len) {
    if ((r = _eit_process_event(m, ed, ptr, len, &save, 0)) < 0)
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
  int r, sect, last, ver;
  uint8_t  seg;
  uint16_t onid, tsid, sid;
  uint32_t extraid, hacks;
  mpegts_service_t     *svc;
  mpegts_mux_t         *mm;
  epggrab_ota_map_t    *map;
  epggrab_module_t     *mod;
  epggrab_ota_mux_t    *ota = NULL;
  mpegts_psi_table_state_t *st;
  th_subscription_t    *ths;
  eit_data_t           *data;
  const char           *cridauth, *charset;
  int                   cridauth_len, charset_len, data_len;
  eit_private_t        *priv;

  if (!epggrab_ota_running)
    return -1;

  mm  = mt->mt_mux;
  map = mt->mt_opaque;
  mod = (epggrab_module_t *)map->om_module;
  priv = (eit_private_t *)((epggrab_module_ota_t *)mod)->opaque;
  if (priv == NULL)
    return -1;
  hacks = priv->hacks;

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

  /* Register interest */
  if (tableid == 0x4e || (tableid >= 0x50 && tableid < 0x60) ||
      (hacks & EIT_HACK_INTEREST4E) != 0 /* uk_freesat hack */)
    ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, extraid, 11, &st, &sect, &last, &ver, 0);
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
  if ((hacks & EIT_HACK_EXTRAMUXLOOKUP) != 0 && (tableid == 0x50 || tableid == 0x4E)) {
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
    if (hacks & EIT_HACK_SVCNETLOOKUP) {
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
    epggrab_ota_service_add(map, ota, &svc->s_id.in_uuid, 1);

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  if (svc->s_dvb_ignore_eit)
    goto done;

  /* Queue events */
  len -= 11;
  ptr += 11;
  if (len >= 12) {
    cridauth = svc->s_dvb_cridauth;
    if (!cridauth)
      cridauth = svc->s_dvb_mux->mm_crid_authority;
    cridauth_len = cridauth ? strlen(cridauth) + 1 : 0;
    charset = dvb_charset_find(NULL, NULL, svc);
    charset_len = charset ? strlen(charset) + 1 : 0;
    data_len = sizeof(*data) + cridauth_len + charset_len;
    data = alloca(data_len);
    data->tableid = tableid;
    data->sect = sect;
    data->svc_uuid = svc->s_id.in_uuid;
    data->onid = onid;
    data->local_time = mm->mm_network->mn_localtime;
    if (cridauth_len) {
      data->cridauth_len = cridauth_len;
      memcpy(data->data, cridauth, cridauth_len);
    } else {
      data->cridauth_len = 0;
    }
    if (charset_len) {
      data->charset_len = charset_len;
      memcpy(data->data + cridauth_len, charset, charset_len);
    } else {
      data->charset_len = 0;
    }
    if (((eit_module_t *)mod)->running_immediate && tableid == 0x4e) {
      /* handle running state immediately */
      _eit_process_immediate(mod, ptr, len, data);
    } else {
      /* handle those data later */
      epggrab_queue_data(mod, data, data_len, ptr, len);
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
  return 0;
}

static void _eit_install_one_handler
  ( mpegts_mux_t *dm, epggrab_ota_map_t *map )
{
  epggrab_module_ota_t *m = map->om_module;
  eit_private_t *priv = m->opaque;
  int pid = priv->pid;
  int opts = 0;

  /* Standard (0x12) */
  if (pid == 0) {
    pid  = DVB_EIT_PID;
    opts = MT_RECORD;
  }

  mpegts_table_add(dm, 0, 0, _eit_callback, map, map->om_module->id, LS_TBL_EIT,
                   MT_CRC | opts, pid, MPS_WEIGHT_EIT);
  tvhdebug(m->subsys, "%s: installed table handler (pid %d)", m->id, pid);
}

static void _eit_install_handlers
  ( epggrab_ota_map_t *_map, mpegts_mux_t *dm )
{
  epggrab_ota_mux_t *om;
  epggrab_ota_map_t *map, *map2;
  epggrab_module_ota_t *m, *m2;
  epggrab_ota_mux_eit_plist_t *plist;
  eit_private_t *priv, *priv2;
  const char *modname;

  om = epggrab_ota_find_mux(dm);
  if (!om)
    return;
  modname = om->om_force_modname;

  priv = NULL;
  map = NULL;
  if (strempty(modname)) {
    LIST_FOREACH(plist, &om->om_eit_plist, link) {
      priv2 = (eit_private_t *)plist->priv;
      if (!priv || priv->module->priority < priv2->module->priority) {
        /* ignore priority for the slave, always prefer master */
        if (priv && strcmp(priv->slave, priv2->module->id) == 0)
          continue;
        /* find the ota map */
        m = priv2->module;
        map = epggrab_ota_find_map(om, m);
        if (!map || !m->enabled) {
          tvhtrace(m->subsys, "handlers - module '%s' not enabled", m->id);
          continue;
        }
        priv = priv2;
      }
    }
  } else {
    m = (epggrab_module_ota_t *)epggrab_module_find_by_id(modname);
    if (m) {
      priv = (eit_private_t *)m->opaque;
      map = epggrab_ota_find_map(om, m);
    }
  }

  if (!priv || !map)
    return;

  epggrab_ota_free_eit_plist(om);

  if (priv->bat_pid) {
    mpegts_table_add(dm, DVB_BAT_BASE, DVB_BAT_MASK, dvb_bat_callback, NULL,
                     "ebat", LS_TBL_BASE, MT_CRC, priv->bat_pid, MPS_WEIGHT_EIT);
  }

  m = priv->module;

  tvhtrace(m->subsys, "handlers - detected module '%s'", m->id);

  if (!strempty(priv->slave)) {
    m2 = (epggrab_module_ota_t *)epggrab_module_find_by_id(priv->slave);
    if (m2) {
      map2 = epggrab_ota_find_map(om, m2);
      if (map2) {
        tvhtrace(m->subsys, "handlers - detected slave module '%s'", m2->id);
        _eit_install_one_handler(dm, map2);
      }
    }
  }

  _eit_install_one_handler(dm, map);

  mpegts_mux_set_epg_module(dm, m->id);
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
  mpegts_service_t *s;
  epggrab_ota_svc_link_t *osl, *nxt;

  lock_assert(&global_lock);

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
        !(s = mpegts_service_find_by_uuid0(&osl->uuid))) {
      epggrab_ota_service_del(map, om, osl, 1);
    } else {
      if (LIST_FIRST(&s->s_channels))
        r = 1;
    }
  }

  return r;
}

static int eit_nit_array_check(uint16_t val, uint16_t *array, int array_count)
{
  int i;

  if (array_count <= 0)
    return 0;
  for (i = 0; i < array_count; i++)
    if (array[i] == val)
      return 0;
  return 1;
}

static void eit_queue_priv
  (mpegts_mux_t *dm, const char *src, eit_private_t *priv)
{
  epggrab_ota_mux_t *om;
  epggrab_ota_mux_eit_plist_t *plist, *plist2;

  om = epggrab_ota_find_mux(dm);
  if (!om)
    return;

  LIST_FOREACH(plist2, &om->om_eit_plist, link)
    if (plist2->priv == priv)
      return;

  tvhtrace(LS_TBL_EIT, "%s - detected module '%s'", src, priv->module->id);
  plist = calloc(1, sizeof(*plist));
  plist->src = src;
  plist->priv = priv;
  LIST_INSERT_HEAD(&om->om_eit_plist, plist, link);

  om->om_detected = 1;
}

void eit_nit_callback
  (mpegts_table_t *mt, uint16_t nbid, const char *name, uint32_t nitpriv)
{
  mpegts_mux_t *dm = mt->mt_mux;
  eit_nit_t *nit;
  eit_private_t *priv = NULL;

  tvhtrace(LS_TBL_EIT, "NIT - tsid %04X (%d) onid %04X (%d) nbid %04X (%d) network name '%s' private %08X",
           dm->mm_tsid, dm->mm_tsid, dm->mm_onid, dm->mm_onid, nbid, nbid, name, nitpriv);

  TAILQ_FOREACH(priv, &eit_private_list, link) {
    if (priv->priv && priv->priv != nitpriv)
      continue;
    if (LIST_FIRST(&priv->nit)) {
      LIST_FOREACH(nit, &priv->nit, link) {
        if (nit->name && strcmp(nit->name, name))
          continue;
        if (eit_nit_array_check(dm->mm_onid, nit->onid, nit->onid_count))
          continue;
        if (eit_nit_array_check(dm->mm_tsid, nit->tsid, nit->tsid_count))
          continue;
        if (eit_nit_array_check(nbid, nit->nbid, nit->nbid_count))
          continue;
        break;
      }
      if (nit)
        break;
    } else {
      break;
    }
  }

  if (!priv)
    return;

  eit_queue_priv(dm, "NIT", priv);
}

void eit_sdt_callback(mpegts_table_t *mt, uint32_t sdtpriv)
{
  mpegts_mux_t *dm = mt->mt_mux;
  eit_sdt_t *sdt;
  eit_private_t *priv = NULL;

  tvhtrace(LS_TBL_EIT, "SDT - tsid %04X (%d) onid %04X (%d) private %08X",
           dm->mm_tsid, dm->mm_tsid, dm->mm_onid, dm->mm_onid, sdtpriv);

  TAILQ_FOREACH(priv, &eit_private_list, link) {
    if (!priv->sdt_enable)
      continue;
    if (priv->priv && priv->priv != sdtpriv)
      continue;
    if (LIST_FIRST(&priv->sdt)) {
      LIST_FOREACH(sdt, &priv->sdt, link) {
        if (eit_nit_array_check(dm->mm_onid, sdt->onid, sdt->onid_count))
          continue;
        if (eit_nit_array_check(dm->mm_tsid, sdt->tsid, sdt->tsid_count))
          continue;
        break;
      }
      if (sdt)
        break;
    } else {
      break;
    }
  }

  if (!priv)
    return;

  eit_queue_priv(dm, "SDT", priv);
}

static void _eit_scrape_clear(eit_module_t *mod)
{
  eit_pattern_free_list(&mod->p_snum);
  eit_pattern_free_list(&mod->p_enum);
  eit_pattern_free_list(&mod->p_airdate);
  eit_pattern_free_list(&mod->p_scrape_title);
  eit_pattern_free_list(&mod->p_scrape_subtitle);
  eit_pattern_free_list(&mod->p_scrape_summary);
  eit_pattern_free_list(&mod->p_is_new);
}

static int _eit_scrape_load_one ( htsmsg_t *m, eit_module_t* mod )
{
  if (mod->scrape_episode) {
    eit_pattern_compile_named_list(&mod->p_snum, m, "season_num");
    eit_pattern_compile_named_list(&mod->p_enum, m, "episode_num");
    eit_pattern_compile_named_list(&mod->p_airdate, m, "airdate");
    eit_pattern_compile_named_list(&mod->p_is_new, m, "is_new");
  }

  if (mod->scrape_title) {
    eit_pattern_compile_named_list(&mod->p_scrape_title, m, "scrape_title");
  }

  if (mod->scrape_subtitle) {
    eit_pattern_compile_named_list(&mod->p_scrape_subtitle, m, "scrape_subtitle");
  }

  if (mod->scrape_summary) {
    eit_pattern_compile_named_list(&mod->p_scrape_summary, m, "scrape_summary");
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

static void _eit_done0( eit_private_t *priv )
{
  eit_nit_t *nit;
  while ((nit = LIST_FIRST(&priv->nit)) != NULL) {
    LIST_REMOVE(nit, link);
    free(nit->name);
    free(nit);
  }
  free(priv->ops);
  lang_str_destroy(priv->name);
  free(priv);
}

void _eit_done ( void *m )
{
  eit_module_t *mod = m;
  eit_private_t *priv = mod->opaque;
  _eit_scrape_clear(mod);
  mod->opaque = NULL;
  TAILQ_REMOVE(&eit_private_list, priv, link);
  _eit_done0(priv);
}

static htsmsg_t *
epggrab_mod_eit_class_short_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Subtitle"),  0 },
    { N_("Summary"), 1 },
    { N_("Subtitle and summary"), 2 }
  };
  return strtab2htsmsg(tab, 1, lang);
}

static const idclass_t epggrab_mod_eit_class = {
  .ic_super      = &epggrab_mod_ota_scraper_class,
  .ic_class      = "epggrab_mod_eit",
  .ic_caption    = N_("Over-the-air EIT EPG grabber"),
  .ic_properties = (const property_t[]){
    {
      .type   = PT_INT,
      .id     = "short_target",
      .name   = N_("Short EIT description"),
      .desc   = N_("Set the short EIT destription to given target (subtitle, summary or both)."),
      .off    = offsetof(eit_module_t, short_target),
      .list   = epggrab_mod_eit_class_short_list,
      .group  = 1,
      .opts   = PO_EXPERT,
    },
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

static void eit_parse_list
  ( htsmsg_t *conf, const char *fname, uint16_t *list, int list_len, int *count )
{
  htsmsg_t *l = htsmsg_get_list(conf, fname);
  htsmsg_field_t *f;
  int val;
  *count = 0;
  if (l == 0) {
    val = htsmsg_get_s32_or_default(conf, fname, -1);
    if (val >= 0) {
      list[0] = val;
      *count = 1;
    }
    return;
  }
  HTSMSG_FOREACH(f, l) {
    if (!htsmsg_field_get_s32(f, &val) && val >= 0 && val < 65536) {
      *list++ = val;
      (*count)++;
      list_len--;
    }
    if (list_len == 0)
      break;
  }
}

static void eit_init_one ( const char *id, htsmsg_t *conf )
{
  epggrab_ota_module_ops_t *ops;
  eit_private_t *priv;
  eit_nit_t *nit;
  eit_sdt_t *sdt;
  const char *s;
  htsmsg_t *map, *e;
  htsmsg_field_t *f;
  int prio = htsmsg_get_s32_or_default(conf, "prio", 1);
  lang_str_t *name_str = lang_str_deserialize(conf, "name");

  ops = calloc(1, sizeof(*ops));
  priv = calloc(1, sizeof(*priv));
  ops->start = _eit_start;
  ops->handlers = _eit_install_handlers;
  ops->done = _eit_done;
  ops->activate = _eit_activate;
  ops->process_data = _eit_process_data;
  ops->tune = _eit_tune;
  ops->opaque = priv;
  priv->ops = ops;
  priv->pid = htsmsg_get_s32_or_default(conf, "pid", 0);
  s = htsmsg_get_str(conf, "conv");
  if (s && strcmp(s, "huffman") == 0)
    priv->conv = EIT_CONV_HUFFMAN;
  priv->priv = htsmsg_get_u32_or_default(conf, "priv", 0);
  map = htsmsg_get_map(conf, "nit");
  if (map) {
    HTSMSG_FOREACH(f, map) {
      nit = calloc(1, sizeof(*nit));
      nit->name = htsmsg_field_name(f)[0] ? strdup(htsmsg_field_name(f)) : NULL;
      if ((e = htsmsg_field_get_map(f)) != NULL) {
        eit_parse_list(e, "onid", nit->onid, ARRAY_SIZE(nit->onid), &nit->onid_count);
        eit_parse_list(e, "tsid", nit->tsid, ARRAY_SIZE(nit->tsid), &nit->tsid_count);
        eit_parse_list(e, "nbid", nit->nbid, ARRAY_SIZE(nit->nbid), &nit->nbid_count);
      }
      LIST_INSERT_HEAD(&priv->nit, nit, link);
    }
  }
  priv->sdt_enable = htsmsg_get_u32_or_default(conf, "sdt_enable", 0);
  map = htsmsg_get_map(conf, "sdt");
  if (map) {
    HTSMSG_FOREACH(f, map) {
      sdt = calloc(1, sizeof(*sdt));
      if ((e = htsmsg_field_get_map(f)) != NULL) {
        eit_parse_list(e, "onid", sdt->onid, ARRAY_SIZE(sdt->onid), &sdt->onid_count);
        eit_parse_list(e, "tsid", sdt->tsid, ARRAY_SIZE(sdt->tsid), &sdt->tsid_count);
      }
      LIST_INSERT_HEAD(&priv->sdt, sdt, link);
    }
  }
  map = htsmsg_get_map(conf, "hacks");
  if (map) {
    HTSMSG_FOREACH(f, map) {
      if (strcmp(htsmsg_field_name(f), "slave") == 0) {
        s = htsmsg_field_get_str(f);
        if (s) strncpy(priv->slave, s, sizeof(priv->slave) - 1);
      } else if (strcmp(htsmsg_field_name(f), "interest-4e") == 0)
        priv->hacks |= EIT_HACK_INTEREST4E;
      else if (strcmp(htsmsg_field_name(f), "extra-mux-lookup") == 0)
        priv->hacks |= EIT_HACK_EXTRAMUXLOOKUP;
      else if (strcmp(htsmsg_field_name(f), "svc-net-lookup") == 0)
        priv->hacks |= EIT_HACK_EXTRAMUXLOOKUP;
      else if (strcmp(htsmsg_field_name(f), "bat") == 0) {
        if (!(e = htsmsg_field_get_map(f))) continue;
        priv->bat_pid = htsmsg_get_s32_or_default(e, "pid", 0);
      }
    }
  }
  priv->name = name_str;
  if (name_str) {
    priv->module = (epggrab_module_ota_t *)
      eit_module_ota_create(id, LS_TBL_EIT, NULL,
                            lang_str_get(name_str, NULL),
                            prio, ops);
    TAILQ_INSERT_TAIL(&eit_private_list, priv, link);
  } else {
    tvherror(LS_TBL_EIT, "missing name for '%s' in config", id);
    _eit_done0(priv);
  }
}

void eit_init ( void )
{
  htsmsg_field_t *f;
  htsmsg_t *c, *e;

  TAILQ_INIT(&eit_private_list);

  c = hts_settings_load("epggrab/eit/config");
  if (!c) {
    tvhwarn(LS_TBL_EIT, "EIT configuration file missing");
    return;
  }
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    eit_init_one(htsmsg_field_name(f), e);
  }
  htsmsg_destroy(c);
}

htsmsg_t *eit_module_id_list( const char *lang )
{
  eit_private_t *priv, *priv2;
  htsmsg_t *e, *l = htsmsg_create_list();

  TAILQ_FOREACH(priv, &eit_private_list, link) {
    TAILQ_FOREACH(priv2, &eit_private_list, link)
      if (strcmp(priv2->slave, priv->module->id) == 0)
        break;
    if (priv2) continue; /* show only parents */
    e = htsmsg_create_key_val(priv->module->id, lang_str_get(priv->name, lang));
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

const char *eit_check_module_id ( const char *id )
{
  eit_private_t *priv;

  if (!id) return NULL;
  TAILQ_FOREACH(priv, &eit_private_list, link) {
    if (strcmp(id, priv->module->id) == 0)
      return priv->module->id;
  }
  return NULL;
}

void eit_done ( void )
{
}

void eit_load ( void )
{
}
