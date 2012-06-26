/*
 *  Electronic Program Guide - opentv epg grabber
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
#include <assert.h>
#include <unistd.h>
#include <linux/dvb/dmx.h>
#include "tvheadend.h"
#include "dvb/dvb.h"
#include "channels.h"
#include "huffman.h"
#include "epg.h"
#include "epggrab/ota.h"
#include "epggrab/opentv.h"
#include "subscriptions.h"
#include "streaming.h"
#include "service.h"
#include "htsmsg.h"
#include "settings.h"

static epggrab_channel_tree_t _opentv_channels;

static void _opentv_tune   
  ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi );
static int  _opentv_enable
  ( epggrab_module_t *m, uint8_t e );

/* ************************************************************************
 * Data structures
 * ***********************************************************************/

#define OPENTV_SCAN_MAX 600    // 10min max scan period
#define OPENTV_SCAN_PER 3600   // 1hour interval

/* Data carousel status */
typedef struct opentv_status
{
  LIST_ENTRY(opentv_status) link;
  int                       pid;
  enum {
    OPENTV_STA_INIT,
    OPENTV_STA_STARTED,
    OPENTV_STA_COMPLETE
  }                         status;
  uint8_t                   start[20];
} opentv_status_t;

/* Huffman dictionary */
typedef struct opentv_dict
{
  char                  *id;
  huffman_node_t        *codes;
  RB_ENTRY(opentv_dict) h_link;
} opentv_dict_t;

/* Provider configuration */
typedef struct opentv_module_t
{
  epggrab_module_t      ;      ///< Base struct

  int                   nid;
  int                   tsid;
  int                   sid;
  int                   *channel;
  int                   *title;
  int                   *summary;
  opentv_dict_t         *dict;

  int                   endbat;
  int                   begbat;
  LIST_HEAD(, opentv_status)  status;
  
} opentv_module_t;

/*
 * Dictionary list
 */
RB_HEAD(, opentv_dict) _opentv_dicts;

static int _dict_cmp ( void *a, void *b )
{
  return strcmp(((opentv_dict_t*)a)->id, ((opentv_dict_t*)b)->id);
}

static opentv_dict_t *_opentv_dict_find ( const char *id )
{
  opentv_dict_t skel;
  skel.id = (char*)id;
  return RB_FIND(&_opentv_dicts, &skel, h_link, _dict_cmp);
}

/* ************************************************************************
 * Module functions
 * ***********************************************************************/

static opentv_status_t *_opentv_module_get_status 
  ( opentv_module_t *mod, int pid )
{
  opentv_status_t *sta;
  LIST_FOREACH(sta, &mod->status, link)
    if (sta->pid == pid) break;
  if (!sta) {
    sta = calloc(1, sizeof(opentv_status_t));
    sta->pid = pid;
    LIST_INSERT_HEAD(&mod->status, sta, link);
  }
  return sta;
}

/* ************************************************************************
 * Configuration loading
 * ***********************************************************************/

static int* _pid_list_to_array ( htsmsg_t *m, opentv_module_t *mod )
{ 
  int i = 1;
  int *ret;
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) 
    if (f->hmf_s64) i++;
  ret = calloc(i, sizeof(int));
  i   = 0;
  HTSMSG_FOREACH(f, m) 
    if (f->hmf_s64) {
      ret[i] = (int)f->hmf_s64;
      if (mod) (void)_opentv_module_get_status(mod, ret[i]);
      i++;
    }
  return ret;
}

static int _opentv_dict_load_one ( const char *id, htsmsg_t *m )
{
  opentv_dict_t *dict = calloc(1, sizeof(opentv_dict_t));
  dict->id = (char*)id;
  if (RB_INSERT_SORTED(&_opentv_dicts, dict, h_link, _dict_cmp)) {
    tvhlog(LOG_DEBUG, "opentv", "ignore duplicate dictionary %s", id);
    free(dict);
    return 0;
  } else {
    dict->codes = huffman_tree_build(m);
    if (!dict->codes) {
      RB_REMOVE(&_opentv_dicts, dict, h_link);
      free(dict);
      return -1;
    } else {
      dict->id = strdup(id);
      return 1;
    }
  }
}

static void _opentv_dict_load ( htsmsg_t *m )
{
  int r;
  htsmsg_t *e;
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) {
    if ((e = htsmsg_get_list(m, f->hmf_name))) {
      if ((r = _opentv_dict_load_one(f->hmf_name, e))) {
        if (r > 0) 
          tvhlog(LOG_INFO, "opentv", "dictionary %s loaded", f->hmf_name);
        else
          tvhlog(LOG_WARNING, "opentv", "dictionary %s failed", f->hmf_name);
      }
    }
  }
  htsmsg_destroy(m);
}

static int _opentv_prov_load_one 
  ( const char *id, htsmsg_t *m, epggrab_module_list_t *list )
{
  char buf[100];
  htsmsg_t *cl, *tl, *sl;
  uint32_t tsid, sid, nid;
  const char *str, *name;
  opentv_dict_t *dict;
  static opentv_module_t *mod = NULL;

  /* Check config */
  if (!(name = htsmsg_get_str(m, "name"))) return -1;
  if (!(str  = htsmsg_get_str(m, "dict"))) return -1;
  if (!(dict = _opentv_dict_find(str))) return -1;
  if (!(cl   = htsmsg_get_list(m, "channel"))) return -1;
  if (!(tl   = htsmsg_get_list(m, "title"))) return -1;
  if (!(sl   = htsmsg_get_list(m, "summary"))) return -1;
  if (htsmsg_get_u32(m, "nid", &nid)) return -1;
  if (htsmsg_get_u32(m, "tsid", &tsid)) return -1;
  if (htsmsg_get_u32(m, "sid", &sid)) return -1;

  /* Exists */
  sprintf(buf, "opentv-%s", id);
  if (epggrab_module_find_by_id(buf)) return 0;

  /* Create module */
  mod           = calloc(1, sizeof(opentv_module_t));
  mod->id       = strdup(buf);
  sprintf(buf, "OpenTV: %s", name);
  mod->name     = strdup(buf);
  mod->enable   = _opentv_enable;
  mod->tune     = _opentv_tune;
  mod->channels = &_opentv_channels;
  *((uint8_t*)&mod->flags) = EPGGRAB_MODULE_OTA;
  LIST_INSERT_HEAD(list, ((epggrab_module_t*)mod), link);

  /* Add provider details */
  mod->dict    = dict;
  mod->nid     = nid;
  mod->tsid    = tsid;
  mod->sid     = sid;
  mod->channel = _pid_list_to_array(cl, NULL);
  mod->title   = _pid_list_to_array(tl, mod);
  mod->summary = _pid_list_to_array(sl, mod);

  return 1;
}

static void _opentv_prov_load ( htsmsg_t *m, epggrab_module_list_t *list )
{
  int r;
  htsmsg_t *e;
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) {
    if ((e = htsmsg_get_map_by_field(f))) {
      if ((r = _opentv_prov_load_one(f->hmf_name, e, list))) {
        if (r > 0)
          tvhlog(LOG_INFO, "opentv", "provider %s loaded", f->hmf_name);
        else
          tvhlog(LOG_WARNING, "opentv", "provider %s failed", f->hmf_name);
      }
    }
  }
  htsmsg_destroy(m);
}

/* ************************************************************************
 * EPG Object wrappers
 * ***********************************************************************/

static epggrab_channel_t *_opentv_find_epggrab_channel
  ( opentv_module_t *mod, int cid, int create, int *save )
{
  char chid[32];
  sprintf(chid, "%s-%d", mod->id, cid);
  return epggrab_module_channel_find((epggrab_module_t*)mod, chid, create, save);
}

static epg_season_t *_opentv_find_season 
  ( opentv_module_t *mod, int cid, int slink )
{
  int save = 0;
  char uri[64];
  sprintf(uri, "%s-%d-%d", mod->id, cid, slink);
  return epg_season_find_by_uri(uri, 1, &save);
}

static service_t *_opentv_find_service ( int tsid, int sid )
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  service_t *t = NULL;
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if (tdmi->tdmi_transport_stream_id != tsid) continue;
      LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
        if (t->s_dvb_service_id == sid) return t;
      }
    }
  }
  return NULL;
}

static channel_t *_opentv_find_channel ( int tsid, int sid )
{
  service_t *t = _opentv_find_service(tsid, sid);
  return t ? t->s_ch : NULL;
}

/* ************************************************************************
 * OpenTV event processing
 * ***********************************************************************/

#define OPENTV_TITLE_LEN   1024
#define OPENTV_SUMMARY_LEN 1024
#define OPENTV_DESC_LEN    2048

#define OPENTV_TITLE       0x01
#define OPENTV_SUMMARY     0x02

/* Internal event structure */
typedef struct opentv_event
{ 
  RB_ENTRY(opentv_event) ev_link;     ///< List of partial events
  uint16_t               cid;         ///< Channel ID
  uint16_t               eid;         ///< Events ID
  time_t                 start;       ///< Start time
  time_t                 stop;        ///< Event stop time
  char                  *title;       ///< Event title
  char                  *summary;     ///< Event summary
  char                  *desc;        ///< Event description
  uint8_t                cat;         ///< Event category
  uint16_t               series;      ///< Series (link) reference
  
  uint8_t                type;        ///< 0x1=title, 0x2=summary
} opentv_event_t;

/* List of partial events */
RB_HEAD(, opentv_event) _opentv_events;

/* Event list comparator */
static int _ev_cmp ( void *_a, void *_b )
{
  opentv_event_t *a = (opentv_event_t*)_a;
  opentv_event_t *b = (opentv_event_t*)_b;
  int r = a->cid - b->cid;
  if (r) return r;
  return a->eid - b->eid;
}

/* Parse huffman encoded string */
static char *_opentv_parse_string 
  ( opentv_module_t *prov, uint8_t *buf, int len )
{
  int ok = 0;
  char *ret, *tmp;

  // Note: unlikely decoded string will be longer (though its possible)
  ret = tmp = malloc(2*len);
  *ret = 0;
  if (huffman_decode(prov->dict->codes, buf, len, 0x20, tmp, 2*len)) {

    /* Ignore (empty) strings */
    while (*tmp) {
      if (*tmp > 0x20) {
        ok = 1;
        break;
      }
      tmp++;
    }
  }
  if (!ok) {
    free(ret);
    ret = NULL;
  }
  return ret;
}

/* Parse a specific record */
static int _opentv_parse_event_record
 ( opentv_module_t *prov, opentv_event_t *ev, uint8_t *buf, int len,
   time_t mjd )
{
  uint8_t rtag = buf[0];
  uint8_t rlen = buf[1];
  if (rlen+2 <= len) {
    switch (rtag) {
      case 0xb5: // title
        ev->start       = (((int)buf[2] << 9) | (buf[3] << 1))
                        + mjd;
        ev->stop        = (((int)buf[4] << 9) | (buf[5] << 1))
                        + ev->start;
        ev->cat         = buf[6];
        if (!ev->title)
          ev->title     = _opentv_parse_string(prov, buf+9, rlen-7);
        break;
      case 0xb9: // summary
        if (!ev->summary)
          ev->summary   = _opentv_parse_string(prov, buf+2, rlen);
        break;
      case 0xbb: // description
        if (!ev->desc)
          ev->desc      = _opentv_parse_string(prov, buf+2, rlen);
        break;
      case 0xc1: // series link
        ev->series      = ((uint16_t)buf[2] << 8) | buf[3];
      break;
      default:
        break;
    }
  }
  return rlen + 2;
}

/* Parse a specific event */
static int _opentv_parse_event
  ( opentv_module_t *prov, uint8_t *buf, int len, int cid, time_t mjd,
    opentv_event_t *ev, int type )
{
  int      slen = ((int)buf[2] & 0xf << 8) | buf[3];
  int      i    = 4;
  opentv_event_t *e;

  ev->cid = cid;
  ev->eid = ((uint16_t)buf[0] << 8) | buf[1];

  /* Get existing summary */
  if ( type == OPENTV_TITLE ) {
    e = RB_FIND(&_opentv_events, ev, ev_link, _ev_cmp);
    if (e) {
      RB_REMOVE(&_opentv_events, e, ev_link);
      memcpy(ev, e, sizeof(opentv_event_t));
      free(e);
    }
  }
  ev->type |= type;

  /* Process records */ 
  while (i < slen+4) {
    i += _opentv_parse_event_record(prov, ev, buf+i, len-i, mjd);
  }
  return slen+4;
}

/* Parse an event section */
static int _opentv_parse_event_section
  ( opentv_module_t *mod, uint8_t *buf, int len, int type )
{
  int i, cid, save = 0;
  time_t mjd;
  char *uri;
  epggrab_channel_t *ec;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  epg_season_t *es;
  opentv_event_t ev;

  /* Channel */
  cid = ((int)buf[0] << 8) | buf[1];
  if (!(ec = _opentv_find_epggrab_channel(mod, cid, 0, NULL))) return 0;
  if (!ec->channel) return 0;
  if (!*ec->channel->ch_name) return 0; // ignore unnamed channels
  
  /* Time (start/stop referenced to this) */
  mjd = ((int)buf[5] << 8) | buf[6];
  mjd = (mjd - 40587) * 86400;

  /* Loop around event entries */
  i = 7;
  while (i < len) {
    memset(&ev, 0, sizeof(opentv_event_t));
    i += _opentv_parse_event(mod, buf+i, len-i, cid, mjd, &ev, type);

    /* Find broadcast */
    if (ev.type & OPENTV_TITLE) {
      ebc = epg_broadcast_find_by_time(ec->channel, ev.start, ev.stop, ev.eid,
                                       1, &save);
    } else {
      ebc = epg_broadcast_find_by_eid(ec->channel, ev.eid);

      /* Store */
      if (!ebc) {
        opentv_event_t *skel = malloc(sizeof(opentv_event_t));
        memcpy(skel, &ev, sizeof(opentv_event_t));
        assert(!RB_INSERT_SORTED(&_opentv_events, skel, ev_link, _ev_cmp));
        continue; // don't want to free() anything
      }
    }

    /* Find episode */
    if (ebc) {
      ee = NULL;

      /* Find episode */
      if (ev.type & OPENTV_SUMMARY || !ebc->episode) {
        uri = epg_hash(ev.title, ev.summary, ev.desc);
        if (uri) {
          ee = epg_episode_find_by_uri(uri, 1, &save);
          free(uri);
        }
      }

      /* Use existing */
      if (!ee) ee = ebc->episode;

      /* Update */
      if (ee) {
        if (ev.title)
          save |= epg_episode_set_title(ee, ev.title);
        if (ev.summary)
          save |= epg_episode_set_summary(ee, ev.summary);
        if (ev.desc)
          save |= epg_episode_set_description(ee, ev.desc);
        if (ev.cat)
          save |= epg_episode_set_genre(ee, &ev.cat, 1);
        // Note: don't override the season (since the ID is channel specific
        //       it'll keep changing!
        if (ev.series && !ee->season) {
          es = _opentv_find_season(mod, cid, ev.series);
          if (es) save |= epg_episode_set_season(ee, es);
        }

        save |= epg_broadcast_set_episode(ebc, ee);
      }
    }

    /* Cleanup */
    if (ev.title)   free(ev.title);
    if (ev.summary) free(ev.summary);
    if (ev.desc)    free(ev.desc);
  }

  /* Update EPG */
  if (save) epg_updated();
  return 0;
}

/* ************************************************************************
 * OpenTV channel processing
 * ***********************************************************************/

static void _opentv_parse_channels
  ( opentv_module_t *mod, uint8_t *buf, int len, uint16_t tsid )
{
  epggrab_channel_t *ec;
  channel_t *ch;
  int sid, cid;//, cnum;
  int save = 0;
  int i = 2;
  while (i < len) {
    sid  = ((int)buf[i] << 8) | buf[i+1];
    cid  = ((int)buf[i+3] << 8) | buf[i+4];
    //cnum = ((int)buf[i+5] << 8) | buf[i+6];

    /* Find the channel */
    if ((ch = _opentv_find_channel(tsid, sid))) {
      ec = _opentv_find_epggrab_channel(mod, cid, 1, &save);
      if (save) {
        ec->channel = ch; 
        //TODO: must be configurable
        //epggrab_channel_set_number(ec, cnum);
      }
    }
    i += 9;
  }
}

static int _opentv_parse_ts_desc
  ( opentv_module_t *mod, uint8_t *buf, int len, uint16_t tsid )
{
  int dtag = buf[0];
  int dlen = buf[1];
  if (dlen+2 > len) return -1;
  if (dtag == 0xb1)
    _opentv_parse_channels(mod, buf+2, dlen, tsid);
  return dlen + 2;
}

static int _opentv_bat_section
  ( opentv_module_t *mod, uint8_t *buf, int len )
{
  int i, r;
  int bdlen, tllen, tdlen;
  uint16_t bid, tsid;
  uint8_t sec;

  /* Skip (not yet applicable) */
  if (!(buf[2] & 0x1)) return 0;
  bid  = buf[0] << 8 | buf[1];
  //ver  = (buf[2] >> 1) & 0x1f;
  sec  = buf[3];
  //lst  = buf[4];

  /* Check for finish */
  // Note: this is NOT the most robust of approaches, but it works
  //       most of the time
  i = 0x80000000 | (bid << 8) | sec;
  if (!mod->begbat) {
    mod->begbat = i;
  } else if (mod->begbat == i) {
    mod->endbat = 1;
    return 0;
  }

  /* Skip (ignore bouquet info for now) */
  bdlen = ((int)buf[5] & 0xf << 8) | buf[6];
  if (bdlen+7 > len) return -1;
  buf   += (7 + bdlen);
  len   -= (7 + bdlen);

  /* TS descriptors */
  tllen = ((int)buf[0] & 0xf << 8) | buf[1];
  buf   += 2;
  len   -= 2;
  if (tllen > len) return -1;
  while (len > 0) {
    tsid  = ((int)buf[0] << 8) | buf[1];
    //nid  = ((int)buf[2] << 8) | buf[3];
    tdlen = ((int)buf[4] & 0xf) << 8 | buf[5];
    buf += 6;
    len -= 6;
    if (tdlen > len) return -1;
    i = 0;
    while (i < tdlen) {
      r = _opentv_parse_ts_desc(mod, buf+i, tdlen-i, tsid);
      if (r < 0) return -1;
      i += r;
    }
    buf += tdlen;
    len -= tdlen;
  }

  return 0;
}

/* ************************************************************************
 * Table Callbacks
 * ***********************************************************************/

static opentv_module_t *_opentv_table_callback 
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  th_dvb_table_t *tdt = (th_dvb_table_t*)p;
  opentv_module_t *mod = (opentv_module_t*)tdt->tdt_opaque;
  epggrab_ota_mux_t *ota;
  opentv_status_t *sta;

  /* Ignore BAT not complete */
  if (!mod->endbat) return NULL;
  
  /* Ignore (not enough data) */
  if (len < 20) return NULL;

  /* Register */
  ota = epggrab_ota_register((epggrab_module_t*)mod, tdmi,
                             OPENTV_SCAN_MAX, OPENTV_SCAN_PER);
  if (!ota) return NULL;

  /* Finished / Blocked */
  if (epggrab_ota_is_complete(ota)) return NULL;
  if (epggrab_ota_is_blocked(ota))  return NULL;

  /* Begin (reset state) */
  if (epggrab_ota_begin(ota)) {
    opentv_event_t *ev;

    /* Remove outstanding event data */
    while ((ev = RB_FIRST(&_opentv_events))) {
      RB_REMOVE(&_opentv_events, ev, ev_link);
      if (ev->title)   free(ev->title);
      if (ev->desc)    free(ev->desc);
      if (ev->summary) free(ev->summary);
      free(ev);
    }

    /* Reset status */
    LIST_FOREACH(sta, &mod->status, link)
      sta->status = OPENTV_STA_INIT;
  }

  /* Insert/Find */
  sta = _opentv_module_get_status(mod, tdt->tdt_pid);

  /* Begin PID */
  if (sta->status == OPENTV_STA_INIT) {
    sta->status = OPENTV_STA_STARTED;
    memcpy(sta->start, buf, 20);
    return mod;
 
  /* PID Already complete */
  } else if (sta->status == OPENTV_STA_COMPLETE) {
    return NULL;

  /* End PID */
  } else if (!memcmp(sta->start, buf, 20)) {
    sta->status = OPENTV_STA_COMPLETE;

    /* Check rest */
    LIST_FOREACH(sta, &mod->status, link)
      if (sta->status != OPENTV_STA_COMPLETE) return mod;

  /* PID in progress */
  } else {
    return mod;
  }

  /* Mark complete */
  epggrab_ota_complete(ota);
  
  return NULL;
}

static int _opentv_title_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  opentv_module_t *mod = _opentv_table_callback(tdmi, buf, len, tid, p);
  if (mod)
    return _opentv_parse_event_section(mod, buf, len, OPENTV_TITLE);
  return 0;
}

static int _opentv_summary_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  opentv_module_t *mod = _opentv_table_callback(tdmi, buf, len, tid, p);
  if (mod)
    return _opentv_parse_event_section(mod, buf, len, OPENTV_SUMMARY);
  return 0;
}

static int _opentv_channel_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  opentv_module_t *mod = (opentv_module_t*)p;
  if (!mod->endbat)
    return _opentv_bat_section(mod, buf, len);
  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _opentv_tune ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi )
{
  int *t;
  struct dmx_sct_filter_params *fp;
  opentv_module_t *mod = (opentv_module_t*)m;
  
  /* Install tables */
  if (m->enabled && (mod->tsid == tdmi->tdmi_transport_stream_id)) {
    tvhlog(LOG_INFO, "opentv", "install provider %s tables", mod->id);

    /* Channels */
    t = mod->channel;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0x4a;
      fp->filter.mask[0]   = 0xff;
      // TODO: what about 0x46 (service description)
      tdt_add(tdmi, fp, _opentv_channel_callback, mod,
              m->id, TDT_CRC, *t++, NULL);
    }

    /* Titles */
    t = mod->title;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0xa0;
      fp->filter.mask[0]   = 0xfc;
      tdt_add(tdmi, fp, _opentv_title_callback, mod,
              m->id, TDT_CRC | TDT_TDT, *t++, NULL);
    }

    /* Summaries */
    t = mod->summary;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0xa8;
      fp->filter.mask[0]   = 0xfc;
      tdt_add(tdmi, fp, _opentv_summary_callback, mod,
              m->id, TDT_CRC | TDT_TDT, *t++, NULL);
    }

    /* Must rebuild the BAT */
    mod->begbat = mod->endbat = 0;
  }
}

static int _opentv_enable ( epggrab_module_t *m, uint8_t e )
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  opentv_module_t *mod = (opentv_module_t*)m;

  if (m->enabled == e) return 0;

  m->enabled = e;

  /* Find muxes and enable/disable */
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if (tdmi->tdmi_transport_stream_id != mod->tsid) continue;
      if (e) {
        epggrab_ota_register(m, tdmi, OPENTV_SCAN_MAX, OPENTV_SCAN_PER);
      } else {
        epggrab_ota_unregister_one(m, tdmi);
      }
    }
  }

  return 1;
}

void opentv_init ( epggrab_module_list_t *list )
{
  htsmsg_t *m;
  const char *dr = tvheadend_dataroot();

  /* Load dictionaries */
  if ((m = hts_settings_load("epggrab/opentv/dict")))
    _opentv_dict_load(m);
  if ((m = hts_settings_load("%s/data/epggrab/opentv/dict", dr)))
    _opentv_dict_load(m);
  tvhlog(LOG_INFO, "opentv", "dictonaries loaded");

  /* Load providers */
  if ((m = hts_settings_load("epggrab/opentv/prov")))
    _opentv_prov_load(m, list);
  if ((m = hts_settings_load("%s/data/epggrab/opentv/prov", dr)))
    _opentv_prov_load(m, list);
  tvhlog(LOG_INFO, "opentv", "providers loaded");
}

void opentv_load ( void )
{
  // TODO: do we want to keep a list of channels stored?
}
