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

static void _opentv_prov_add_tables
  ( opentv_prov_t *prov, th_dvb_mux_instance_t *tdmi )
{
  tvhlog(LOG_INFO, "opentv", "install provider %s", prov->key);
  /* Channels */
  /* Titles */
  /* Summaries */
#if 0
  struct dmx_sct_filter_params *fp;
    fp = dvb_fparams_alloc();
    fp->filter.filter[0] = 0xa8;
    fp->filter.mask[0]   = 0xfc;
    tdt_add(tdmi, fp, _opentv_callback, NULL, "opentv", TDT_CRC, 0x40, NULL);
#endif
}

/* ************************************************************************
 * Parser
 * ***********************************************************************/

#if 0
static void _parse_short_desc ( uint8_t *buf, int len, uint16_t eid )
{
#if 0
  char *str = huffman_decode(_opentv_dict, (char*)buf, len, 0x20);
  printf("EID: %5d, SUMMARY: %s\n", eid, str);
  free(str);
#endif
}

static int _parse_record ( uint8_t *buf, int len, uint16_t eid )
{
  uint8_t rtag = buf[0];
  uint8_t rlen = buf[1];
  if (rlen+2 > len) return rlen+2;
  switch (rtag) {
    case 0xb5:
      //_parse_title(buf+2, rlen);
      break;
    case 0xb9:
      _parse_short_desc(buf+2, rlen, eid);
      break;
    case 0xbb:
      //_parse_long_desc(buf+2, rlen);
      break;
    case 0xc1:
      //_parse_series_link(buf+2, rlen);
      break;
    default:
      break;
  }
  return rlen + 2;
}

static int _parse_summary ( uint8_t *buf, int len )
{
  uint16_t eid  = ((uint16_t)buf[0] << 8) | buf[1];
  int      slen = ((int)buf[2] & 0xf << 8) | buf[3];
  int      i    = 4;
  if (slen+4 > len) return slen+4;
  while (i < slen+4) {
    i += _parse_record(buf+i, len-i, eid);
  }
  return i;
}

static int _opentv_callback 
  ( th_dvb_mux_instance_t *tdmi, uint8_t *buf, int len, uint8_t tid, void *p )
{
  // TODO: currently this will only get summary tables!
  int i = 0;
  //uint16_t cid, mjd;
  if (len < 20) return 0;
  //cid = ((uint16_t)buf[0] << 8) | buf[1];
  //mjd = ((uint16_t)buf[2] << 8) | buf[3];
  i   = 10;
  while (i < len) {
    i += _parse_summary(buf+i, len-i);
  }
  return 0;
}
#endif

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t _opentv_mod;

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
  _opentv_mod.id     = strdup("opentv");
  _opentv_mod.name   = strdup("OpenTV EPG");
  _opentv_mod.enable = _opentv_enable;
  _opentv_mod.tune   = _opentv_tune;
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
