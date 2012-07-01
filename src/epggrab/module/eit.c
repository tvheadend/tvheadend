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

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct eit_status
{
  int tid;
  int tsid;
  int sid;
  int sec;
} eit_status_t;

/* ************************************************************************
 * Diagnostics
 * ***********************************************************************/

// Dump a descriptor tag for debug (looking for new tags etc...)
static void _eit_dtag_dump ( uint8_t dtag, uint8_t dlen, uint8_t *buf )
{ 
  int i = 0, j = 0;
  char tmp[100];
  tvhlog(LOG_DEBUG, "eit", "  dtag 0x%02X len %d", dtag, dlen);
  while (i < dlen) {
    j += sprintf(tmp+j, "%02X ", buf[i]);
    i++;
    if ((i % 8) == 0 || !dlen) {
      tvhlog(LOG_DEBUG, "eit", "    %s", tmp);
      j = 0;
    }
  }
}

/* ************************************************************************
 * Processing
 * ***********************************************************************/

/**
 * DVB Descriptor; Short Event
 */
static int
dvb_desc_short_event(uint8_t *ptr, int len, 
         char *title, size_t titlelen,
         char *desc,  size_t desclen,
         char *dvb_default_charset)
{
  int r;

  if(len < 4)
    return -1;
  ptr += 3; len -= 3;

  if((r = dvb_get_string_with_len(title, titlelen, ptr, len, dvb_default_charset)) < 0)
    return -1;
  ptr += r; len -= r;

  if((r = dvb_get_string_with_len(desc, desclen, ptr, len, dvb_default_charset)) < 0)
    return -1;

  return 0;
}

/**
 * DVB Descriptor; Extended Event
 */
static int
dvb_desc_extended_event(uint8_t *ptr, int len, 
                        char *desc, size_t desclen,
                        htsmsg_t *extra,
                        char *dvb_default_charset)
{
  int ilen, nitems = ptr[4];
  char ikey[256], ival[256];
  if (len < 8) return -1;
  ptr += 5;
  len -= 5;

  /* key/value items */
  while (nitems-- > 0) {

    /* key */
    ilen = *ptr;
    if (ilen+1 > len) return -1;
    if(dvb_get_string_with_len(ikey, sizeof(ikey),
                               ptr+1, ilen,
                               dvb_default_charset) < 0) return -1;
      return -1;
    ptr += (ilen + 1);
    len -= (ilen + 1);

    /* value */
    ilen = *ptr;
    if (ilen+1 > len) return -1;
    if(dvb_get_string_with_len(ival, sizeof(ival),
                               ptr, ilen,
                               dvb_default_charset) < 0) return -1;
    ptr += (ilen + 1);
    len -= (ilen + 1);

    // TODO: should this extend existing strings?
    htsmsg_add_str(extra, ikey, ival);
  }

  /* raw text (append) */
  ilen = *ptr;
  if (ilen+1 > len) return -1;
  if(dvb_get_string_with_len(desc    + strlen(desc),
                             desclen - strlen(desc),
                             ptr+1, ilen,
                             dvb_default_charset) < 0) return -1;

  return 0;
}

static int _eit_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *ptr, int len, 
    uint8_t tableid, void *opaque )
{
  epggrab_ota_mux_t *ota = (epggrab_ota_mux_t*)opaque;
  th_dvb_adapter_t *tda;
  service_t *svc;
  channel_t *ch;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  eit_status_t *sta;
  int resched = 0, save = 0, save2 = 0, dllen, dtag, dlen;
  uint16_t tsid, sid, eid;
  uint8_t bw, hd, ws, ad, ds, st;
  time_t start, stop;
  int genre_idx = 0;
  uint8_t genre[10];
  char title[256];
  char summary[256];
  char desc[5000];
  htsmsg_t *extra;

  /* Invalid */
  if(tableid < 0x4e || tableid > 0x6f || len < 11)
    return -1;

  /* Get tsid/sid */
  sid  = ptr[0] << 8 | ptr[1];
  tsid = ptr[5] << 8 | ptr[6];

  /* Already complete */
  if (epggrab_ota_is_complete(ota)) return 0;

  /* Started */
  sta = ota->status;
  if (epggrab_ota_begin(ota)) {
    sta->tid  = tableid;
    sta->tsid = tsid;
    sta->sid  = sid;
    sta->sec  = ptr[3];

  /* Complete */
  } else if (sta->tid == tableid && sta->sec == ptr[3] && sta->tsid == tsid && sta->sid == sid) {
    epggrab_ota_complete(ota);
    return 0;
  }

  /* Don't process */
  if((ptr[2] & 1) == 0)
    return 0;

  /* Get transport stream */
  // Note: tableid=0x4f,0x60-0x6f is other TS
  //       so must find the tdmi
  if(tableid == 0x4f || tableid >= 0x60) {
    tda = tdmi->tdmi_adapter;
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      if(tdmi->tdmi_transport_stream_id == tsid)
        break;
  }
  if(!tdmi) return -1;

  /* Get service */
  svc = dvb_transport_find(tdmi, sid, 0, NULL);
  if (!svc || !svc->s_enabled || !(ch = svc->s_ch)) return 0;


  /* Ignore (disabled) */
  // TODO: should this be altered?
//  if (!svc->s_dvb_eit_enable) return 0;

  /* Register as interesting */
  // TODO: do we want to register for now/next?
  // TODO: want should the intervals be?
  if (tableid < 0x50)
    epggrab_ota_register(ota, 20, 0);
  else
    epggrab_ota_register(ota, 40, 0);

  /* Up to date */
#if TODO_INCLUDE_EIT_VER_CHECK
  ver  = ptr[2] >> 1 & 0x1f;
  if (svc->s_dvb_eit_version[tableid-0x4f] == ver) return 0;
  svc->s_dvb_eit_version[tableid-0x4e] = ver;
#endif

  /* Process events */
  len -= 11;
  ptr += 11;
  while(len >= 12) {
    eid   = ptr[0] << 8 | ptr[1];
    start = dvb_convert_date(&ptr[2]);
    stop  = start + bcdtoint(ptr[7] & 0xff) * 3600 +
                    bcdtoint(ptr[8] & 0xff) * 60 +
                    bcdtoint(ptr[9] & 0xff);
    dllen = ((ptr[10] & 0x0f) << 8) | ptr[11];

    len -= 12;
    ptr += 12;
    if(dllen > len) break;

    /* Get the event */
    ebc  = epg_broadcast_find_by_time(ch, start, stop, eid, 1, &save2);
    if (!ebc) {
      len -= dllen;
      ptr += dllen;
      continue;
    }
    tvhlog(LOG_DEBUG, "eit", "process tid 0x%02x eid %d event %p %"PRIu64" on %s", tableid, eid, ebc, ebc->id, ch->ch_name);

    /* Mark re-schedule detect (only now/next) */
    if (save2 && tableid < 0x50) resched = 1;

    /* Process tags */
    *title    = *summary = *desc = 0;
    extra     = NULL;
    genre_idx = 0;
    hd = ws = bw = ad = st = ds = 0;
    while(dllen > 0) {
      dtag = ptr[0];
      dlen = ptr[1];

      len -= 2; ptr += 2; dllen -= 2; 

      if(dlen > len) break;

      switch(dtag) {

        /* Short descriptor (title/summary) */
        case DVB_DESC_SHORT_EVENT:
          dvb_desc_short_event(ptr, dlen,
                               title, sizeof(title),
                               summary,  sizeof(summary),
                               svc->s_dvb_default_charset);
        break;

        /* Extended (description) */
        case DVB_DESC_EXT_EVENT:
          if (!extra) extra = htsmsg_create_map();
          dvb_desc_extended_event(ptr, dlen,
                                  desc, sizeof(desc),
                                  extra,
                                  svc->s_dvb_default_charset);
          break;

        /* Content type */
        case DVB_DESC_CONTENT:
          while (dlen > 0) {
            ptr  += 2;
            dlen -= 2;
            if   ( *ptr == 0xb1 )
              bw = 1;
            else if ( *ptr < 0xb0 && genre_idx < sizeof(genre) )
              genre[genre_idx++] = *ptr;
          }
          break;

        /* Component descriptor */
        case DVB_DESC_COMPONENT: {
          uint8_t c = *ptr & 0x0f;
          uint8_t t = ptr[1];

          /* MPEG2 (video) */
          if (c == 0x1) {
            if (t > 0x08 && t < 0x11) {
              hd = 1;
              if ( t != 0x09 && t != 0x0d )
                ws = 1;
            } else if (t == 0x02 || t == 0x03 || t == 0x04 || 
                       t == 0x06 || t == 0x07 || t == 0x08 ) {
              ws = 1;
            }

          /* MPEG2 (audio) */
          } else if (c == 0x2) {

            /* Described */
            if (t == 0x40 || t == 0x41) 
              ad = 1;

          /* Subtitles */
          } else if (c == 0x3) {
            st = 1;
    
          /* H264 */
          } else if (c == 0x5) {
            if (t == 0x0b || t == 0x0c || t == 0x10)
              hd = ws = 1;
            else if (t == 0x03 || t == 0x04 || t == 0x07 || t == 0x08)
              ws = 1;

          /* AAC */
          } else if ( c == 0x6 ) {

            /* Described */
            if (t == 0x40 || t == 0x44)
              ad = 1;
          }
        }
        break;

        /* Parental Rating */
#if TODO_AGE_RATING
        case DVB_DESC_PARENTAL_RAT:
          if (*ptr > 0 && *ptr < 15)
            minage = *ptr + 3;
#endif

        /* Ignore */
        default:
          _eit_dtag_dump(dtag, dlen, ptr);
          break;
      }

      len -= dlen; ptr += dlen; dllen -= dlen;
    }

    /* Metadata */
    if ( save2 ) {
      save |= epg_broadcast_set_is_hd(ebc, hd);
      save |= epg_broadcast_set_is_widescreen(ebc, ws);
      save |= epg_broadcast_set_is_audio_desc(ebc, ad);
      save |= epg_broadcast_set_is_subtitled(ebc, st);
      save |= epg_broadcast_set_is_deafsigned(ebc, ds);
    }

    /* Create episode */
    ee = ebc->episode;
    if ( !ee || save2 ) {
      char *uri;
      uri   = epg_hash(title, summary, desc);
      if (uri) {
        ee    = epg_episode_find_by_uri(uri, 1, &save2);
        save |= epg_broadcast_set_episode(ebc, ee);
        free(uri);
      }
    }
    save |= save2;

    /* Episode data */
    if (ee) {
      save |= epg_episode_set_is_bw(ee, bw);
      if ( !ee->title && *title )
        save |= epg_episode_set_title(ee, title);
      if ( !ee->summary && *summary )
        save |= epg_episode_set_summary(ee, summary);
      if ( !ee->description && *desc )
        save |= epg_episode_set_description(ee, desc);
      if ( !ee->genre_cnt && genre_idx )
        save |= epg_episode_set_genre(ee, genre, genre_idx);
#if TODO_ADD_EXTRA
      if ( extra )
        save |= epg_episode_set_extra(ee, extra);
#endif
    }
    if (extra) free(extra);
  }
  
  /* Update EPG */
  if (resched) epggrab_resched();
  if (save)    epg_updated();

  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _eit_start 
  ( epggrab_module_ota_t *m, th_dvb_mux_instance_t *tdmi )
{
  epggrab_ota_mux_t *ota;
  if (!m->enabled) return;
  if (!(ota = epggrab_ota_create(m, tdmi))) return;
  if (!ota->status)
    ota->status = calloc(1, sizeof(eit_status_t));
  tdt_add(tdmi, NULL, _eit_callback, ota, "eit", TDT_CRC, 0x12, NULL);
  tvhlog(LOG_DEBUG, "eit", "install table handlers");
}

void eit_init ( void )
{
  epggrab_module_ota_create(NULL, "eit", "EIT: DVB Grabber",
                            _eit_start, NULL, NULL);
}

void eit_load ( void )
{
}
