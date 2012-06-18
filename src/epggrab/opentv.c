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
#include "epggrab/opentv.h"
#include "subscriptions.h"
#include "service.h"
#include "htsmsg.h"
#include "settings.h"

/* ************************************************************************
 * Configuration
 *
 * TODO: I think much of this may be moved to a generic lib, as some of
 *       the other OTA EPGs will have similar config reqs
 *
 * TODO: some of this is a bit overkill, for example the global tree of
 *       dicts/provs etc... they're only used to get things up and running
 * ***********************************************************************/

/* Huffman dictionary */
typedef struct opentv_dict
{
  char                  *id;
  huffman_node_t        *codes;
  RB_ENTRY(opentv_dict) h_link;
} opentv_dict_t;

/* Provider configuration */
typedef struct opentv_prov
{
  char                  *id;
  char                  *name;
  RB_ENTRY(opentv_prov) h_link;

  int                   nid;
  int                   tsid;
  int                   sid;
  int                   *channel;
  int                   *title;
  int                   *summary;
  opentv_dict_t         *dict;
} opentv_prov_t;

/* Extension of epggrab module to include linked provider */
typedef struct opentv_module
{
  epggrab_module_t _;     ///< Base struct
  opentv_prov_t    *prov; ///< Associated provider config
} opentv_module_t;

/*
 * Lists/Comparators
 */
RB_HEAD(opentv_dict_tree, opentv_dict);
RB_HEAD(opentv_prov_tree, opentv_prov);
struct opentv_dict_tree _opentv_dicts;
struct opentv_prov_tree _opentv_provs;

static int _dict_cmp ( void *a, void *b )
{
  return strcmp(((opentv_dict_t*)a)->id, ((opentv_dict_t*)b)->id);
}

static int _prov_cmp ( void *a, void *b )
{
  return strcmp(((opentv_prov_t*)a)->id, ((opentv_prov_t*)b)->id);
}

static opentv_dict_t *_opentv_dict_find ( const char *id )
{
  opentv_dict_t skel;
  skel.id = (char*)id;
  return RB_FIND(&_opentv_dicts, &skel, h_link, _dict_cmp);
}

/*
 * Configuration loading
 */

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
    if (f->hmf_s64)
      ret[i++] = (int)f->hmf_s64;
  return ret;
}

static int _opentv_dict_load ( const char *id, htsmsg_t *m )
{
  opentv_dict_t *dict = calloc(1, sizeof(opentv_dict_t));
  dict->id = (char*)id;
  if (RB_INSERT_SORTED(&_opentv_dicts, dict, h_link, _dict_cmp)) {
    tvhlog(LOG_WARNING, "opentv", "ignore duplicate dictionary %s", id);
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

static int _opentv_prov_load ( const char *id, htsmsg_t *m )
{
  htsmsg_t *cl, *tl, *sl;
  uint32_t tsid, sid, nid;
  const char *str, *name;
  opentv_dict_t *dict;
  opentv_prov_t *prov;

  /* Check config */
  if (!(name = htsmsg_get_str(m, "name"))) return -1;
  if (!(str  = htsmsg_get_str(m, "dict"))) return -1;
  if (!(dict = _opentv_dict_find(str))) return -1;
  if (!(cl   = htsmsg_get_list(m, "channel"))) return -1;
  if (!(tl   = htsmsg_get_list(m, "title"))) return -5;
  if (!(sl   = htsmsg_get_list(m, "summary"))) return -1;
  if (htsmsg_get_u32(m, "nid", &nid)) return -1;
  if (htsmsg_get_u32(m, "tsid", &tsid)) return -1;
  if (htsmsg_get_u32(m, "sid", &sid)) return -1;

  prov = calloc(1, sizeof(opentv_prov_t));
  prov->id = (char*)id;
  if (RB_INSERT_SORTED(&_opentv_provs, prov, h_link, _prov_cmp)) {
    tvhlog(LOG_WARNING, "opentv", "ignore duplicate provider %s", id);
    free(prov);
    return 0;
  } else {
    prov->id      = strdup(id);
    prov->name    = strdup(name);
    prov->dict    = dict;
    prov->nid     = nid;
    prov->tsid    = tsid;
    prov->sid     = sid;
    prov->channel = _pid_list_to_array(cl);
    prov->title   = _pid_list_to_array(tl);
    prov->summary = _pid_list_to_array(sl);
    return 1;
  }
}

/* ************************************************************************
 * EPG Object wrappers
 * ***********************************************************************/

static epggrab_channel_t *_opentv_find_epggrab_channel
  ( opentv_module_t *mod, int cid, int create, int *save )
{
  char chid[32];
  sprintf(chid, "%s-%d", mod->prov->id, cid);
  return epggrab_module_channel_find((epggrab_module_t*)mod, chid, create, save);
}

static channel_t *_opentv_find_channel ( int tsid, int sid )
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  service_t *t = NULL;
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if (tdmi->tdmi_transport_stream_id != tsid) continue;
      LIST_FOREACH(t, &tdmi->tdmi_transports, s_group_link) {
        if (t->s_dvb_service_id == sid) return t->s_ch;
      }
    }
  }
  return NULL;
}

/* ************************************************************************
 * OpenTV data processing
 * ***********************************************************************/

typedef struct opentv_event
{ 
  int     eid;
  int     start;
  int     duration;
  char    *title;
  char    *summary;
  char    *description;
  int     serieslink;
  uint8_t cat;
} opentv_event_t;

/* Parse huffman encoded string */
static char *_parse_string ( opentv_prov_t *prov, uint8_t *buf, int len )
{
  return huffman_decode(prov->dict->codes, buf, len, 0x20);
}

/* Parse a specific record */
static int _opentv_parse_event_record
 ( opentv_prov_t *prov, opentv_event_t *ev, uint8_t *buf, int len )
{
  uint8_t rtag = buf[0];
  uint8_t rlen = buf[1];
  if (rlen+2 <= len) {
    switch (rtag) {
      case 0xb5: // title
        ev->start       = (((int)buf[2] << 9) | (buf[3] << 1));
        ev->duration    = (((int)buf[4] << 9) | (buf[5] << 1));
        ev->cat         = buf[6];
        ev->title       = _parse_string(prov, buf+7, rlen-7);
        break;
      case 0xb9: // summary
        ev->summary     = _parse_string(prov, buf+2, rlen);
        break;
      case 0xbb: // description
        ev->description = _parse_string(prov, buf+2, rlen);
        break;
      case 0xc1: // series link
        ev->serieslink  = ((int)buf[2] << 8) | buf[3];
      break;
      default:
        break;
    }
  }
  return rlen + 2;
}

/* Parse a specific event */
static int _opentv_parse_event
  ( opentv_prov_t *prov, opentv_event_t *ev, uint8_t *buf, int len )
{
  int      slen = ((int)buf[2] & 0xf << 8) | buf[3];
  int      i    = 4;
  ev->eid  = ((uint16_t)buf[0] << 8) | buf[1];
  while (i < slen) {
    i += _opentv_parse_event_record(prov, ev, buf+i, len-i);
  }
  return slen+4;
}

/* Parse an event section */
static int _opentv_parse_event_section
  ( opentv_module_t *mod, uint8_t *buf, int len )
{
  int i, cid, mjd, save = 0;
  char *uri;
  epggrab_channel_t *ec;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  opentv_event_t ev;

  /* Channel */
  cid = ((int)buf[0] << 8) | buf[1];
  if (!(ec = _opentv_find_epggrab_channel(mod, cid, 0, NULL))) return 0;
  if (!ec->channel) return 0;
  
  /* Time (start/stop referenced to this) */
  mjd = ((int)buf[5] << 8) | buf[6];

  /* Loop around event entries */
  i = 7;
  while (i < len) {
    memset(&ev, 0, sizeof(opentv_event_t));
    i += _opentv_parse_event(mod->prov, &ev, buf+i, len-i);

    /* Process the event */

    /* Create/Find broadcast */
    if (ev.start && ev.duration) {
      time_t start = ev.start + ((mjd - 40587) * 86400);
      time_t stop  = ev.start + ev.duration;
      ebc = epg_broadcast_find_by_time(ec->channel, start, stop, 1, &save);
    } else {
      ebc = epg_broadcast_find_by_eid(ev.eid, ec->channel);
    }

    if (ebc) {
      if (ebc->dvb_eid != ev.eid) {
        ebc->dvb_eid = ev.eid;
        save = 1;
      }

      /* Create/Find episode */
      if (ev.description || ev.summary) {
        uri   = md5sum(ev.description ?: ev.summary);
        ee    = epg_episode_find_by_uri(uri, 1, &save);
        free(uri);
      } else if (ebc) {
        ee    = ebc->episode;
      }

      /* Set episode data */
      if (ee) {
        if (ev.description) 
          save |= epg_episode_set_description(ee, ev.description);
        if (ev.summary)
          save |= epg_episode_set_summary(ee, ev.summary);
        if (ev.title)
          save |= epg_episode_set_title(ee, ev.title);
        if (ev.cat)
          save |= epg_episode_set_genre(ee, &ev.cat, 1);
        // TODO: series link

        save |= epg_broadcast_set_episode(ebc, ee);
      }
    }

    /* Cleanup */
    if (ev.title)       free(ev.title);
    if (ev.summary)     free(ev.summary);
    if (ev.description) free(ev.description);
  }

  /* Update EPG */
  if (save) epg_updated();
  return 0;
}

static int _opentv_event_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  return _opentv_parse_event_section((opentv_module_t*)p, buf, len);
}

// TODO: this function is currently a bit of a mess
// TODO: bouqets are ignored, what useful info can we get from them?
static int _opentv_channel_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  opentv_module_t *mod = (opentv_module_t*)p;
  epggrab_channel_t *ec;
  int tsid, cid;//, cnum;
  uint16_t sid;
  int i, j, k, tdlen, dlen, dtag, tslen;
  channel_t *ch;
  if (tid != 0x4a) return 0;
  i     = 7 + ((((int)buf[5] & 0xf) << 8) | buf[6]);
  tslen = (((int)buf[i] & 0xf) << 8) | buf[i+1];
  i    += 2;
  while (tslen > 0) {
    tsid  = ((int)buf[i] << 8) | buf[i+1];
    //nid   = ((int)buf[i+2] << 8) | buf[i+3];
    tdlen = (((int)buf[i+4] & 0xf) << 8) | buf[i+5];
    j      = i + 6;
    i     += (tdlen + 6);
    tslen -= (tdlen + 6);
    while (tdlen > 0) {
      dtag   = buf[j];
      dlen   = buf[j+1];
      k      = j + 2;
      j     += (dlen + 2);
      tdlen -= (dlen + 2);
      if (dtag == 0xb1) {
        k    += 2;
        dlen -= 2;
        while (dlen > 0) {
          sid  = ((int)buf[k] << 8) | buf[k+1];
          cid  = ((int)buf[k+3] << 8) | buf[k+4];
          //cnum = ((int)buf[k+5] << 8) | buf[k+6];

          /* Find the channel */
          ch = _opentv_find_channel(tsid, sid);
          if (ch) {
            int save = 0;
            ec = _opentv_find_epggrab_channel(mod, cid, 1, &save);
            if (save) {
              // Note: could use set_sid() but not nec.
              ec->channel = ch; 
              //TODO: must be configurable
              //epggrab_channel_set_number(ec, cnum);
            }
          }
          k    += 9;
          dlen -= 9;
        }
      }
    }
  }
  return 0;
}

/* ************************************************************************
 * Tuning Thread
 * ***********************************************************************/

#if 0
static void* _opentv_thread ( void *p )
{
  int err = 0;
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  service_t *svc = NULL;

  pthread_mutex_lock(&global_lock);

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if ((svc = dvb_transport_find(tdmi, 4152, 0, NULL))) {
        _opentv_tdmi = tdmi;
	break;
      }
    }
  }

  /* start */
  printf("found svc = %p\n", svc);
  if(svc) err = service_start(svc, 1, 0);
  printf("service_start = %d\n", err);
  
  pthread_mutex_unlock(&global_lock);

  while (1) {
    // check status?
    sleep(10);
  }
  return NULL;
}
#endif

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_channel_tree_t _opentv_channels;

static void _opentv_tune ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi )
{
  int *t;
  struct dmx_sct_filter_params *fp;
  opentv_module_t *mod = (opentv_module_t*)m;

  /* Install tables */
  if (m->enabled && (mod->prov->tsid == tdmi->tdmi_transport_stream_id)) {
    tvhlog(LOG_INFO, "opentv", "install provider %s tables", mod->prov->id);

    /* Channels */
    t = mod->prov->channel;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0x4a;
      fp->filter.mask[0]   = 0xff;
      // TODO: what about 0x46 (service description)
      tdt_add(tdmi, fp, _opentv_channel_callback, mod,
              m->id, TDT_CRC, *t++, NULL);
    }

    /* Titles */
    t = mod->prov->title;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0xa0;
      fp->filter.mask[0]   = 0xfc;
      tdt_add(tdmi, fp, _opentv_event_callback, mod,
              m->id, TDT_CRC, *t++, NULL);
    }

    /* Summaries */
    t = mod->prov->summary;
    while (*t) {
      fp = dvb_fparams_alloc();
      fp->filter.filter[0] = 0xa8;
      fp->filter.mask[0]   = 0xfc;
      tdt_add(tdmi, fp, _opentv_event_callback, mod,
              m->id, TDT_CRC, *t++, NULL);
    }
  }
}

static int _opentv_enable ( epggrab_module_t *m, uint8_t e )
{
  // TODO: do I need to kick off a thread here or elsewhere?
  int save = 0;
  if (m->enabled != e) {
    m->enabled = e;
    save = 1;
  }
  return save;
}

void opentv_init ( epggrab_module_list_t *list )
{
  int r;
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  opentv_prov_t *p;
  opentv_module_t *mod;
  char buf[100];

  /* Load the dictionaries */
  if ((m = hts_settings_load("epggrab/opentv/dict"))) {
    HTSMSG_FOREACH(f, m) {
      if ((e = htsmsg_get_list(m, f->hmf_name))) {
        if ((r = _opentv_dict_load(f->hmf_name, e))) {
          if (r > 0) 
            tvhlog(LOG_INFO, "opentv", "dictionary %s loaded", f->hmf_name);
          else
            tvhlog(LOG_WARNING, "opentv", "dictionary %s failed", f->hmf_name);
        }
      }
    }
    htsmsg_destroy(m);
  }
  tvhlog(LOG_INFO, "opentv", "dictonaries loaded");

  /* Load providers */
  if ((m = hts_settings_load("epggrab/opentv/prov"))) {
    HTSMSG_FOREACH(f, m) {
      if ((e = htsmsg_get_map_by_field(f))) {
        if ((r = _opentv_prov_load(f->hmf_name, e))) {
          if (r > 0)
            tvhlog(LOG_INFO, "opentv", "provider %s loaded", f->hmf_name);
          else
            tvhlog(LOG_WARNING, "opentv", "provider %s failed", f->hmf_name);
        }
      }
    }
    htsmsg_destroy(m);
  }
  tvhlog(LOG_INFO, "opentv", "providers loaded");
  
  /* Create modules */
  RB_FOREACH(p, &_opentv_provs, h_link) {
    mod = calloc(1, sizeof(opentv_module_t));
    sprintf(buf, "opentv-%s", p->id);
    mod->_.id       = strdup(buf);
    sprintf(buf, "OpenTV: %s", p->name);
    mod->_.name     = strdup(buf);
    mod->_.enable   = _opentv_enable;
    mod->_.tune     = _opentv_tune;
    mod->_.channels = &_opentv_channels;
    *((uint8_t*)&mod->_.flags) = EPGGRAB_MODULE_OTA;
    LIST_INSERT_HEAD(list, &mod->_, link);
  }
}

void opentv_load ( void )
{
  // TODO: do we want to keep a list of channels stored?
}
