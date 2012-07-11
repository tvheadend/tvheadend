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
#include "epggrab.h"
#include "epggrab/private.h"
#include "subscriptions.h"
#include "streaming.h"
#include "service.h"
#include "htsmsg.h"
#include "settings.h"

static epggrab_channel_tree_t _opentv_channels;

/* ************************************************************************
 * Status monitoring
 * ***********************************************************************/

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

/* PID status (for event PIDs) */
typedef struct opentv_pid
{
  LIST_ENTRY(opentv_pid) link;
  int                    pid;
  enum {
    OPENTV_PID_INIT,
    OPENTV_PID_STARTED,
    OPENTV_PID_COMPLETE
  }                      state;
  uint8_t                start[20];
} opentv_pid_t;

/* Scan status */
typedef struct opentv_status
{
  int                     begbat;
  int                     endbat;
  LIST_HEAD(, opentv_pid) pids;
  RB_HEAD(, opentv_event) events;
} opentv_status_t;

/* Get a pid entry */
static opentv_pid_t *_opentv_status_get_pid
  ( opentv_status_t *sta, int pid )
{
  opentv_pid_t *p;
  LIST_FOREACH(p, &sta->pids, link) {
    if (p->pid == pid) break;
  }
  if (!p) {
    p = calloc(1, sizeof(opentv_pid_t));
    p->pid = pid;
    LIST_INSERT_HEAD(&sta->pids, p, link);
  }
  return p;
}

/* Clear events */
static void _opentv_status_remove_events ( opentv_status_t *sta )
{
  opentv_event_t *ev;
  while ((ev = RB_FIRST(&sta->events))) {
    RB_REMOVE(&sta->events, ev, ev_link);
    if (ev->title)   free(ev->title);
    if (ev->summary) free(ev->summary);
    if (ev->desc)    free(ev->desc);
    free(ev);
  }
}

/* ************************************************************************
 * Module structure
 * ***********************************************************************/

/* Huffman dictionary */
typedef struct opentv_dict
{
  char                  *id;
  huffman_node_t        *codes;
  RB_ENTRY(opentv_dict) h_link;
} opentv_dict_t;

typedef struct opentv_genre
{
  char                  *id;
  uint8_t                map[256];
  RB_ENTRY(opentv_genre) h_link;
} opentv_genre_t;

/* Provider configuration */
typedef struct opentv_module_t
{
  epggrab_module_ota_t  ;      ///< Base struct

  int                   nid;
  int                   tsid;
  int                   sid;
  int                   *channel;
  int                   *title;
  int                   *summary;
  opentv_dict_t         *dict;
  opentv_genre_t        *genre;
  
} opentv_module_t;

/*
 * Dictionary list
 */
RB_HEAD(, opentv_dict)  _opentv_dicts;

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

/*
 * Genre mapping list
 */
RB_HEAD(, opentv_genre) _opentv_genres;

static int _genre_cmp ( void *a, void *b )
{
  return strcmp(((opentv_genre_t*)a)->id, ((opentv_genre_t*)b)->id);
}

static opentv_genre_t *_opentv_genre_find ( const char *id )
{
  opentv_genre_t skel;
  skel.id = (char*)id;
  return RB_FIND(&_opentv_genres, &skel, h_link, _genre_cmp);
}

/* ************************************************************************
 * EPG Object wrappers
 * ***********************************************************************/

static epggrab_channel_t *_opentv_find_epggrab_channel
  ( opentv_module_t *mod, int cid, int create, int *save )
{
  char chid[32];
  sprintf(chid, "%s-%d", mod->id, cid);
  return epggrab_channel_find(&_opentv_channels, chid, create, save,
                              (epggrab_module_t*)mod);
}

static epg_season_t *_opentv_find_season 
  ( opentv_module_t *mod, int cid, opentv_event_t *ev, int *save )
{
  char uri[64];
  sprintf(uri, "%s-%d-%d", mod->id, cid, ev->series);
  return epg_season_find_by_uri(uri, 1, save);
}

static epg_episode_t *_opentv_find_episode
  ( opentv_module_t *mod, int cid, opentv_event_t *ev, int *save )
{
  char uri[100];
  char *tmp = epg_hash(ev->title, ev->summary, ev->desc);
  if (tmp) {
    snprintf(uri, 100, "%s-%d-%s", mod->id, cid, tmp);
    free(tmp);
    return epg_episode_find_by_uri(uri, 1, save);
  }
  return NULL;
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

/* ************************************************************************
 * OpenTV event processing
 * ***********************************************************************/

#define OPENTV_TITLE_LEN   1024
#define OPENTV_SUMMARY_LEN 1024
#define OPENTV_DESC_LEN    2048

#define OPENTV_TITLE       0x01
#define OPENTV_SUMMARY     0x02

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
        if (prov->genre)
          ev->cat = prov->genre->map[ev->cat];
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
  ( opentv_module_t *prov, opentv_status_t *sta,
    uint8_t *buf, int len, int cid, time_t mjd,
    opentv_event_t *ev, int type )
{
  int      slen = ((int)buf[2] & 0xf << 8) | buf[3];
  int      i    = 4;
  opentv_event_t *e;

  ev->cid = cid;
  ev->eid = ((uint16_t)buf[0] << 8) | buf[1];

  /* Get existing summary */
  e = RB_FIND(&sta->events, ev, ev_link, _ev_cmp);
  if (e) {
    RB_REMOVE(&sta->events, e, ev_link);
    memcpy(ev, e, sizeof(opentv_event_t));
    free(e);
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
  ( opentv_module_t *mod, opentv_status_t *sta,
    uint8_t *buf, int len, int type )
{
  int i, cid, save = 0;
  time_t mjd;
  epggrab_channel_t *ec;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  epg_season_t *es;
  opentv_event_t ev;
  epggrab_module_t *src = (epggrab_module_t*)mod;

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
    i += _opentv_parse_event(mod, sta, buf+i, len-i, cid, mjd, &ev, type);

    /* Find broadcast */
    if (ev.type & OPENTV_TITLE) {
      ebc = epg_broadcast_find_by_time(ec->channel, ev.start, ev.stop, ev.eid,
                                       1, &save);

    /* Store */
    } else if (!(ebc = epg_broadcast_find_by_eid(ec->channel, ev.eid))) {
      opentv_event_t *skel = malloc(sizeof(opentv_event_t));
      memcpy(skel, &ev, sizeof(opentv_event_t));
      assert(!RB_INSERT_SORTED(&sta->events, skel, ev_link, _ev_cmp));
      continue; // don't want to free() anything
    }

    /* Find episode */
    if (ebc) {
      ee = NULL;

      /* Find episode */
      if (ev.type & OPENTV_SUMMARY || !ebc->episode)
        ee = _opentv_find_episode(mod, cid, &ev, &save);

      /* Use existing */
      if (!ee) ee = ebc->episode;

      /* Update */
      if (ee) {
        if (!ev.title && ebc->episode)
          save |= epg_episode_set_title(ee, ebc->episode->title, src);
        else if (ev.title)
          save |= epg_episode_set_title(ee, ev.title, src);
        if (ev.summary)
          save |= epg_episode_set_summary(ee, ev.summary, src);
        if (ev.desc)
          save |= epg_episode_set_description(ee, ev.desc, src);
        if (ev.cat) {
          epg_genre_list_t *egl = calloc(1, sizeof(epg_genre_list_t));
          epg_genre_list_add_by_eit(egl, ev.cat);
          save |= epg_episode_set_genre(ee, egl, src);
          epg_genre_list_destroy(egl);
        }
        if (ev.series) {
          es = _opentv_find_season(mod, cid, &ev, &save);
          if (es) save |= epg_episode_set_season(ee, es, src);
        }

        save |= epg_broadcast_set_episode(ebc, ee, src);
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
  service_t *svc;
  int sid, cid, cnum;
  int save = 0;
  int i = 2;
  while (i < len) {
    sid  = ((int)buf[i] << 8) | buf[i+1];
    cid  = ((int)buf[i+3] << 8) | buf[i+4];
    cnum = ((int)buf[i+5] << 8) | buf[i+6];

    /* Find the service */
    svc = _opentv_find_service(tsid, sid);
    if (svc && svc->s_ch) {
      ec  =_opentv_find_epggrab_channel(mod, cid, 1, &save);
      if (service_is_primary_epg(svc))
        ec->channel = svc->s_ch;
      else
        ec->channel = NULL;
      save |= epggrab_channel_set_number(ec, cnum);
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
  ( opentv_module_t *mod, opentv_status_t *sta, uint8_t *buf, int len )
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
  if (!sta->begbat) {
    tvhlog(LOG_DEBUG, "opentv", "begin processing BAT");
    sta->begbat = i;
  } else if (sta->begbat == i) {
    sta->endbat = 1;
    tvhlog(LOG_DEBUG, "opentv", "finish processing BAT");
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

static epggrab_ota_mux_t *_opentv_event_callback 
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  th_dvb_table_t    *tdt = p;
  opentv_module_t   *mod = tdt->tdt_opaque;
  epggrab_ota_mux_t *ota = epggrab_ota_find((epggrab_module_ota_t*)mod, tdmi);
  opentv_status_t *sta;
  opentv_pid_t *pid;

  /* Ignore (invalid - stopped?) */
  if (!ota || !ota->status) return NULL;
  sta = ota->status;

  /* Ignore (not enough data) */
  if (len < 20) return NULL;

  /* Ignore (don't have BAT) */
  if (!sta->endbat) return NULL;

  /* Finished / Blocked */
  if (epggrab_ota_is_complete(ota)) return NULL;

  /* Begin (reset state) */
  if (epggrab_ota_begin(ota)) {

    /* Remove outstanding event data */
    _opentv_status_remove_events(sta);

    /* Reset status */
    LIST_FOREACH(pid, &sta->pids, link)
      pid->state = OPENTV_PID_INIT;
  }

  /* Insert/Find */
  pid = _opentv_status_get_pid(sta, tdt->tdt_pid);

  /* Begin PID */
  if (pid->state == OPENTV_PID_INIT) {
    pid->state = OPENTV_PID_STARTED;
    memcpy(pid->start, buf, 20);
    return ota;
 
  /* PID Already complete */
  } else if (pid->state == OPENTV_PID_COMPLETE) {
    return NULL;

  /* End PID */
  } else if (!memcmp(pid->start, buf, 20)) {
    pid->state = OPENTV_PID_COMPLETE;

    /* Check rest */
    LIST_FOREACH(pid, &sta->pids, link)
      if (pid->state != OPENTV_PID_COMPLETE) return ota;

  /* PID in progress */
  } else {
    return ota;
  }

  /* Mark complete */
  tvhlog(LOG_DEBUG, "opentv", "finish processing events");
  
  return NULL;
}

static int _opentv_title_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  epggrab_ota_mux_t *ota = _opentv_event_callback(tdmi, buf, len, tid, p);
  if (ota)
    return _opentv_parse_event_section((opentv_module_t*)ota->grab,
                                       (opentv_status_t*)ota->status,
                                       buf, len, OPENTV_TITLE);
  return 0;
}

static int _opentv_summary_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  epggrab_ota_mux_t *ota = _opentv_event_callback(tdmi, buf, len, tid, p);
  if (ota)
    return _opentv_parse_event_section((opentv_module_t*)ota->grab,
                                       (opentv_status_t*)ota->status,
                                       buf, len, OPENTV_SUMMARY);
  return 0;
}

static int _opentv_channel_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  opentv_module_t      *mod = p;
  epggrab_ota_mux_t    *ota = epggrab_ota_find((epggrab_module_ota_t*)mod, tdmi);
  opentv_status_t      *sta;
  if (!ota || !ota->status) return 0;
  sta = ota->status;
  if (!sta->endbat)
    return _opentv_bat_section(mod, sta, buf, len);
  return 0;
}

/* ************************************************************************
 * Module callbacks
 * ***********************************************************************/

static void _opentv_ota_destroy ( epggrab_ota_mux_t *ota )
{
  opentv_status_t *sta = ota->status;
  opentv_pid_t    *pid;

  /* Empty the events */
  _opentv_status_remove_events(sta);

  /* Empty pids */
  while ((pid = LIST_FIRST(&sta->pids))) {
    LIST_REMOVE(pid, link);
    free(pid);
  }

  /* Free the rest */
  free(sta);
  free(ota);
}

static void _opentv_start
  ( epggrab_module_ota_t *m, th_dvb_mux_instance_t *tdmi )
{
  int *t;
  struct dmx_sct_filter_params *fp;
  epggrab_ota_mux_t *ota;
  opentv_module_t *mod = (opentv_module_t*)m;
  opentv_status_t *sta;

  /* Ignore */
  if (!m->enabled)  return;
  if (mod->tsid != tdmi->tdmi_transport_stream_id) return;

  /* Create link */
  if (!(ota = epggrab_ota_create(m, tdmi))) return;
  if (!ota->status) {
    ota->status  = calloc(1, sizeof(opentv_status_t));
    ota->destroy = _opentv_ota_destroy;
  }
  sta = ota->status;
  sta->begbat = sta->endbat = 0;

  /* Register (just in case we missed it on enable somehow) */
  epggrab_ota_register(ota, 600, 3600); // 10min scan every hour
  
  /* Install tables */
  tvhlog(LOG_INFO, "opentv", "install provider %s tables", mod->id);

  /* Channels */
  t = mod->channel;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0x4a;
    fp->filter.mask[0]   = 0xff;
    // TODO: what about 0x46 (service description)
    tdt_add(tdmi, fp, _opentv_channel_callback, m,
            m->id, TDT_CRC, *t++, NULL);
  }

  /* Titles */
  t = mod->title;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0xa0;
    fp->filter.mask[0]   = 0xfc;
    _opentv_status_get_pid(sta, *t);
    tdt_add(tdmi, fp, _opentv_title_callback, m,
            m->id, TDT_CRC | TDT_TDT, *t++, NULL);
  }

  /* Summaries */
  t = mod->summary;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0xa8;
    fp->filter.mask[0]   = 0xfc;
    _opentv_status_get_pid(sta, *t);
    tdt_add(tdmi, fp, _opentv_summary_callback, m,
            m->id, TDT_CRC | TDT_TDT, *t++, NULL);
  }
}

static int _opentv_enable ( void  *m, uint8_t e )
{
  opentv_module_t *mod = (opentv_module_t*)m;

  if (mod->enabled == e) return 0;
  mod->enabled = e;

  /* Register interest */
  if (e) {
    epggrab_ota_create_and_register_by_id((epggrab_module_ota_t*)mod,
                                          mod->nid, mod->tsid,
                                          600, 3600);
  /* Remove all links */
  } else {
    epggrab_ota_destroy_by_module((epggrab_module_ota_t*)mod);
  }

  return 1;
}

/* ************************************************************************
 * Configuration
 * ***********************************************************************/

static int* _pid_list_to_array ( htsmsg_t *m )
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
      i++;
    }
  return ret;
}

static int _opentv_genre_load_one ( const char *id, htsmsg_t *m )
{
  htsmsg_field_t *f;
  opentv_genre_t *genre = calloc(1, sizeof(opentv_genre_t));
  genre->id = (char*)id;
  if (RB_INSERT_SORTED(&_opentv_genres, genre, h_link, _genre_cmp)) {
    tvhlog(LOG_DEBUG, "opentv", "ignore duplicate genre map %s", id);
    free(genre);
    return 0;
  } else {
    genre->id = strdup(id);
    int i = 0;
    HTSMSG_FOREACH(f, m) {
      genre->map[i] = (uint8_t)f->hmf_s64;
      if (++i == 256) break;
    }
  }
  return 1;
}

static void _opentv_genre_load ( htsmsg_t *m )
{
  int r;
  htsmsg_t *e;
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) {
    if ((e = htsmsg_get_list(m, f->hmf_name))) {
      if ((r = _opentv_genre_load_one(f->hmf_name, e))) {
        if (r > 0) 
          tvhlog(LOG_INFO, "opentv", "genre map %s loaded", f->hmf_name);
        else
          tvhlog(LOG_WARNING, "opentv", "genre map %s failed", f->hmf_name);
      }
    }
  }
  htsmsg_destroy(m);
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

static int _opentv_prov_load_one ( const char *id, htsmsg_t *m )
{
  char ibuf[100], nbuf[1000];
  htsmsg_t *cl, *tl, *sl;
  uint32_t tsid, sid, nid;
  const char *str, *name;
  opentv_dict_t *dict;
  opentv_genre_t *genre;
  opentv_module_t *mod;

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

  /* Genre map (optional) */
  str = htsmsg_get_str(m, "genre");
  if (str)
    genre = _opentv_genre_find(str);
  else
    genre = NULL;
 

  /* Exists (we expect some duplicates due to config layout) */
  sprintf(ibuf, "opentv-%s", id);
  if (epggrab_module_find_by_id(ibuf)) return 0;

  /* Create */
  sprintf(nbuf, "OpenTV: %s", name);
  mod = (opentv_module_t*)
    epggrab_module_ota_create(calloc(1, sizeof(opentv_module_t)),
                              ibuf, nbuf, 2,
                              _opentv_start, _opentv_enable,
                              NULL);
  
  /* Add provider details */
  mod->dict     = dict;
  mod->genre    = genre;
  mod->nid      = nid;
  mod->tsid     = tsid;
  mod->sid      = sid;
  mod->channel  = _pid_list_to_array(cl);
  mod->title    = _pid_list_to_array(tl);
  mod->summary  = _pid_list_to_array(sl);
  mod->channels = &_opentv_channels;
  mod->ch_rem   = epggrab_module_ch_rem;

  return 1;
}

static void _opentv_prov_load ( htsmsg_t *m )
{
  int r;
  htsmsg_t *e;
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) {
    if ((e = htsmsg_get_map_by_field(f))) {
      if ((r = _opentv_prov_load_one(f->hmf_name, e))) {
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
 * Module Setup
 * ***********************************************************************/

void opentv_init ( void )
{
  htsmsg_t *m;
  const char *dr = tvheadend_dataroot();

  /* Load dictionaries */
  if ((m = hts_settings_load("epggrab/opentv/dict")))
    _opentv_dict_load(m);
  if ((m = hts_settings_load("%s/data/epggrab/opentv/dict", dr)))
    _opentv_dict_load(m);
  tvhlog(LOG_INFO, "opentv", "dictonaries loaded");

  /* Load genres */
  if ((m = hts_settings_load("epggrab/opentv/genre")))
    _opentv_genre_load(m);
  if ((m = hts_settings_load("%s/data/epggrab/opentv/genre", dr)))
    _opentv_genre_load(m);
  tvhlog(LOG_INFO, "opentv", "genre maps loaded");

  /* Load providers */
  if ((m = hts_settings_load("epggrab/opentv/prov")))
    _opentv_prov_load(m);
  if ((m = hts_settings_load("%s/data/epggrab/opentv/prov", dr)))
    _opentv_prov_load(m);
  tvhlog(LOG_INFO, "opentv", "providers loaded");
}

void opentv_load ( void )
{
  // TODO: do we want to keep a list of channels stored?
}
