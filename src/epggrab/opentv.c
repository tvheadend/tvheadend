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

static epggrab_module_t _opentv_mod;
static epggrab_channel_tree_t _opentv_channels;

/* ************************************************************************
 * Configuration
 *
 * TODO: I think much of this may be moved to a generic lib, as some of
 *       the other OTA EPGs will have similar config reqs
 * ***********************************************************************/

typedef struct opentv_dict
{
  char                  *key;
  huffman_node_t        *codes;
  RB_ENTRY(opentv_dict) h_link;
} opentv_dict_t;

typedef struct opentv_prov
{
  char                  *key;
  RB_ENTRY(opentv_prov) h_link;
  int                   enabled;

  int                   tsid;
  int                   sid;
  int                   *channel;
  int                   *title;
  int                   *summary;
  opentv_dict_t         *dict;
} opentv_prov_t;

RB_HEAD(opentv_dict_tree, opentv_dict);
RB_HEAD(opentv_prov_tree, opentv_prov);
struct opentv_dict_tree _opentv_dicts;
struct opentv_prov_tree _opentv_provs;

static int _dict_cmp ( void *a, void *b )
{
  return strcmp(((opentv_dict_t*)a)->key, ((opentv_dict_t*)b)->key);
}

static int _prov_cmp ( void *a, void *b )
{
  return strcmp(((opentv_prov_t*)a)->key, ((opentv_prov_t*)b)->key);
}

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

static opentv_dict_t *_opentv_dict_find ( const char *key )
{
  opentv_dict_t skel;
  skel.key = (char*)key;
  return RB_FIND(&_opentv_dicts, &skel, h_link, _dict_cmp);
}

#if 0
static opentv_prov_t *_opentv_prov_find ( const char *key )
{
  opentv_prov_t skel;
  skel.key = (char*)key;
  return RB_FIND(&_opentv_provs, &skel, h_link, _prov_cmp);
}
#endif

static int _opentv_dict_load ( const char *key, htsmsg_t *m )
{
  opentv_dict_t *dict = calloc(1, sizeof(opentv_dict_t));
  dict->key = (char*)key;
  if (RB_INSERT_SORTED(&_opentv_dicts, dict, h_link, _dict_cmp)) {
    tvhlog(LOG_WARNING, "opentv", "ignore duplicate dictionary %s", key);
    free(dict);
    return 0;
  } else {
    dict->codes = huffman_tree_build(m);
    if (!dict->codes) {
      RB_REMOVE(&_opentv_dicts, dict, h_link);
      free(dict);
      return -1;
    } else {
      dict->key = strdup(key);
      return 1;
    }
  }
}

static int _opentv_prov_load ( const char *key, htsmsg_t *m )
{
  htsmsg_t *cl, *tl, *sl;
  uint32_t tsid, sid;
  const char *str;
  opentv_dict_t *dict;
  opentv_prov_t *prov;

  /* Check config */
  if (!(str  = htsmsg_get_str(m, "dict"))) return -1;
  if (!(dict = _opentv_dict_find(str))) return -1;
  if (!(cl   = htsmsg_get_list(m, "channel"))) return -1;
  if (!(tl   = htsmsg_get_list(m, "title"))) return -1;
  if (!(sl   = htsmsg_get_list(m, "summary"))) return -1;
  if (htsmsg_get_u32(m, "tsid", &tsid)) return -1;
  if (htsmsg_get_u32(m, "sid", &sid)) return -1;

  prov = calloc(1, sizeof(opentv_prov_t));
  prov->key = (char*)key;
  if (RB_INSERT_SORTED(&_opentv_provs, prov, h_link, _prov_cmp)) {
    tvhlog(LOG_WARNING, "opentv", "ignore duplicate provider %s", key);
    free(prov);
    return 0;
  } else {
    prov->key     = strdup(key);
    prov->dict    = dict;
    prov->tsid    = tsid;
    prov->sid     = sid;
    prov->channel = _pid_list_to_array(cl);
    prov->title   = _pid_list_to_array(tl);
    prov->summary = _pid_list_to_array(sl);
    return 1;
  }
}

/* ************************************************************************
 * Parser
 * ***********************************************************************/

static epggrab_channel_t *_opentv_find_channel ( int cid, int create, int *save )
{
  char chid[16];
  sprintf(chid, "opentv-%d", cid);
  return epggrab_module_channel_find(&_opentv_mod, chid, create, save);
}

static char *_parse_string ( opentv_prov_t *prov, uint8_t *buf, int len )
{
  return huffman_decode(prov->dict->codes, buf, len, 0x20);
}

static int _parse_record ( opentv_prov_t *prov, uint8_t *buf, int len, epg_broadcast_t *ebc, int *save )
{
  epg_episode_t *ee;
  char *str;
  uint8_t rtag = buf[0];
  uint8_t rlen = buf[1];
  if (rlen+2 > len) return rlen+2;
  switch (rtag) {
#if 0
    case 0xb5:
      str = _parse_string(buf+2, rlen);
      if (str) {
        printf("EID: %d, TITLE: %s\n", eid, str);
        free(str);
      }
      break;
#endif
    case 0xb9:
      str = _parse_string(prov, buf+2, rlen);
      if (str) {
        char *uri = md5sum(str);
        ee = epg_episode_find_by_uri(uri, 1, save);
        if (ee) *save |= epg_episode_set_summary(ee, str);
        if (ee) *save |= epg_broadcast_set_episode(ebc, ee);
        free(str);
        free(uri);
      }
      break;
    case 0xbb:
      //str = _parse_string(prov, buf+2, rlen);
      // TODO
      break;
    case 0xc1:
      //_parse_series_link(buf+2, rlen);
      break;
    default:
      break;
  }
  return rlen + 2;
}

static int _parse_summary ( opentv_prov_t *prov, uint8_t *buf, int len, channel_t *ch, int *save )
{
  epg_broadcast_t *ebc;
  uint16_t eid  = ((uint16_t)buf[0] << 8) | buf[1];
  int      slen = ((int)buf[2] & 0xf << 8) | buf[3];
  int      i    = 4;
  if (!(ebc = epg_broadcast_find_by_eid(eid, ch))) return slen+4;
  if (slen+4 > len) return slen+4;
  while (i < slen+4) {
    i += _parse_record(prov, buf+i, len-i, ebc, save);
  }
  return slen+4;
}

static int _opentv_summary_callback 
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  int save = 0;
  int i = 0;
  uint16_t cid;//, mjd;
  epggrab_channel_t *ec;
  if (len < 20) return 0;
  cid = ((uint16_t)buf[0] << 8) | buf[1];
  if (!(ec  = _opentv_find_channel(cid, 0, NULL))) return 0;
  if (!ec->channel) return 0;
  //mjd = ((uint16_t)buf[5] << 8) | buf[6];
  i   = 7;
  while (i < len) {
    i += _parse_summary((opentv_prov_t*)p, buf+i, len-i, ec->channel, &save);
  }
  if (save) epg_updated();
  return 0;
}

static channel_t *_find_channel ( int tsid, int sid )
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

static int _opentv_channel_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  static epggrab_channel_t *ec;
  int tsid, cid, cnum;
  uint16_t sid;
  int i, j, k, tdlen, dlen, dtag, tslen;
  channel_t *ch;
  if (tid != 0x4a) return 0;

  // TODO: bouqets
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
          cnum = ((int)buf[k+5] << 8) | buf[k+6];

          /* Find the channel */
          ch = _find_channel(tsid, sid);
          if (ch) {
            int save = 0;
            ec = _opentv_find_channel(cid, 1, &save);
            if (save) {
              ec->channel = ch; // Note: could use set_sid() but not nec.
              epggrab_channel_set_number(ec, cnum);
              epggrab_channel_updated(ec);
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

static int _opentv_title_callback
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  int save = 0, save2 = 0;
  int cid, mjd, i, eid;
  time_t start, stop;
  //uint8_t cat;
  int l1, l2;
  char *title;
  epggrab_channel_t *ec;
  channel_t *ch;
  opentv_prov_t *prov = (opentv_prov_t*)p;
  epg_broadcast_t *ebc;
  char *uri;

  if (len < 20) return 0;
  
  /* Get channel/time */
  cid = ((int)buf[0] << 8) | buf[1];
  mjd = ((int)buf[5] << 8) | buf[6];
  if (!(ec  = _opentv_find_channel(cid, 0, NULL))) return 0;
  if (!ec->channel) return 0;
  ch = ec->channel;

  /* Process events */
  i = 7;
  while ( i < len ) {
    eid = ((int)buf[i] << 8) | buf[i+1];
    l1  = (((int)buf[i+2] & 0xf) << 8) | buf[i+3];
    if (buf[i+4] != 0xb5) return 0;
    i += 4;
    l2    = buf[i+1] - 7;
    start = ((mjd - 40587) * 86400) + (((int)buf[i+2] << 9) | (buf[i+3] << 1));
    stop  = start                   + (((int)buf[i+4] << 9) | (buf[i+5] << 1));
    title = huffman_decode(prov->dict->codes, buf+i+9, l2, 0x20);
    uri   = md5sum(title);
    //cat   = buf[i+6];
    i += l1;
    save = 0;
    ebc = epg_broadcast_find_by_time(ch, start, stop, 1, &save);
    save2 |= save;
    if (ebc) {
      // TODO: if EIDs do not match?
      if (ebc->dvb_eid != eid) {
        ebc->dvb_eid = eid;
        save2 = 1;
      }
      if (ebc->episode) {
        save2 |= epg_episode_set_title(ebc->episode, title);
      }
    }
    free(uri);
    free(title);
  }
  if (save2) epg_updated();
  return 0;
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static void _opentv_prov_add_tables
  ( opentv_prov_t *prov, th_dvb_mux_instance_t *tdmi )
{
  int *t;
  struct dmx_sct_filter_params *fp;
  tvhlog(LOG_INFO, "opentv", "install provider %s", prov->key);
  
  /* Channels */
  t = prov->channel;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0x4a;
    fp->filter.mask[0]   = 0xff;
    // TODO: what about 0x46 (service description)
    tdt_add(tdmi, fp, _opentv_channel_callback, prov,
            "opentv-c", TDT_CRC, *t++, NULL);
  }

  /* Titles */
  t = prov->title;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0xa0;
    fp->filter.mask[0]   = 0xfc;
    tdt_add(tdmi, fp, _opentv_title_callback, prov,
            "opentv-t", TDT_CRC, *t++, NULL);
  }

  /* Summaries */
  t = prov->summary;
  while (*t) {
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0xa8;
    fp->filter.mask[0]   = 0xfc;
    tdt_add(tdmi, fp, _opentv_summary_callback, prov,
            "opentv-s", TDT_CRC, *t++, NULL);
  }
}

static void _opentv_tune ( epggrab_module_t *m, th_dvb_mux_instance_t *tdmi )
{
  opentv_prov_t *prov;
  if (!m->enabled) return;

  /* Check if this in our list of enabled providers */
  RB_FOREACH(prov, &_opentv_provs, h_link) {
    if (prov->enabled && prov->tsid == tdmi->tdmi_transport_stream_id)
      _opentv_prov_add_tables(prov, tdmi);
  }
}

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

static int _opentv_enable ( epggrab_module_t *m, uint8_t e )
{
  int save = 0;
  if (m->enabled != e) {
    m->enabled = e;
    save = 1;
  }
  return save;
}

void opentv_init ( epggrab_module_list_t *list )
{
  _opentv_mod.id       = strdup("opentv");
  _opentv_mod.name     = strdup("OpenTV EPG");
  _opentv_mod.enable   = _opentv_enable;
  _opentv_mod.tune     = _opentv_tune;
  _opentv_mod.channels = &_opentv_channels;
  *((uint8_t*)&_opentv_mod.flags) = EPGGRAB_MODULE_EXTERNAL; // TODO: hack
  LIST_INSERT_HEAD(list, &_opentv_mod, link);
}

void opentv_load ( void )
{
  int r;
  htsmsg_t *m, *e;
  htsmsg_field_t *f;
  opentv_prov_t *p;

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

  /* TODO: do this properly */
  RB_FOREACH(p, &_opentv_provs, h_link) {
    p->enabled = 1;
    tvhlog(LOG_INFO, "opentv", "enabled %s", p->key);
  }
}
