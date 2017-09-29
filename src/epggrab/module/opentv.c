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
#include <ctype.h>
#include "tvheadend.h"
#include "channels.h"
#include "huffman.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "eitpatternlist.h"
#include "subscriptions.h"
#include "streaming.h"
#include "service.h"
#include "htsmsg.h"
#include "settings.h"
#include "input.h"

#define OPENTV_TITLE_BASE   0xA0
#define OPENTV_SUMMARY_BASE 0xA8
#define OPENTV_TABLE_MASK   0xFC

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

  int                   onid;
  int                   tsid;
  int                   sid;
  int                   bouquetid;
  int                   bouquet_auto;
  int                  *channel;
  int                  *title;
  int                  *summary;
  opentv_dict_t        *dict;
  opentv_genre_t       *genre;
  int                   titles_time;
  int                   summaries_time;
  eit_pattern_list_t p_snum;
  eit_pattern_list_t p_enum;
  eit_pattern_list_t p_pnum;
  eit_pattern_list_t p_subt;
  eit_pattern_list_t p_cleanup_title;
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
 * Status monitoring
 * ***********************************************************************/

/* Internal event structure */
typedef struct opentv_event
{ 
  uint16_t               eid;         ///< Events ID
  time_t                 start;       ///< Start time
  time_t                 stop;        ///< Event stop time
  char                  *title;       ///< Event title
  char                  *summary;     ///< Event summary
  char                  *desc;        ///< Event description
  uint8_t                cat;         ///< Event category
  uint16_t               serieslink;  ///< Series link ID
} opentv_event_t;

/* Scan status */
typedef struct opentv_status
{
  opentv_module_t   *os_mod;
  epggrab_ota_map_t *os_map;
  int                os_refcount;
  epggrab_ota_mux_t *os_ota;
  int64_t            os_titles_start;
  int64_t            os_summaries_start;
} opentv_status_t;

static void
opentv_status_destroy ( mpegts_table_t *mt )
{
  opentv_status_t *st = mt->mt_opaque;
  lock_assert(&global_lock);
  assert(st->os_refcount > 0);
  --st->os_refcount;
  if (!st->os_refcount) 
    free(st);
}

/* ************************************************************************
 * EPG Object wrappers
 * ***********************************************************************/

static epggrab_channel_t *_opentv_find_epggrab_channel
  ( opentv_module_t *mod, int cid, int create, int *save )
{
  char chid[256];
  snprintf(chid, sizeof(chid), "%s-%d", mod->id, cid);
  return epggrab_channel_find((epggrab_module_t*)mod, chid, create, save);
}

/* ************************************************************************
 * OpenTV event processing
 * ***********************************************************************/

/* Parse huffman encoded string */
static char *_opentv_parse_string 
  ( opentv_module_t *prov, const uint8_t *buf, int len )
{
  int ok = 0;
  char *ret, *tmp;

  if (len <= 0) return NULL;

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
 ( opentv_module_t *prov, opentv_event_t *ev,
   const uint8_t *buf, int len,
   time_t mjd )
{
  uint8_t rtag = buf[0];
  int rlen = buf[1];
  if (rlen+2 > len)
    return -1;
  if (rlen+2 <= len) {
    switch (rtag) {
      case 0xb5: // title
        if (rlen >= 7) {
          ev->start       = (((int)buf[2] << 9) | (buf[3] << 1))
                          + mjd;
          ev->stop        = (((int)buf[4] << 9) | (buf[5] << 1))
                          + ev->start;
          ev->cat         = buf[6];
          if (prov->genre)
            ev->cat = prov->genre->map[ev->cat];
          if (!ev->title) {
            ev->title     = _opentv_parse_string(prov, buf+9, rlen-7);
            if (!strcmp(prov->dict->id, "skynz")) {
              if ((strlen(ev->title) >= 6) && (ev->title[0] == '[') && (ev->title[1] == '[') && (ev->title[4] == ']') && (ev->title[5] == ']'))
		memmove(ev->title,ev->title+6,strlen(ev->title)-5);
	    }
	  }
        }
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
        if (!ev->serieslink)
          ev->serieslink = ((uint16_t)buf[2] << 8) | buf[3];
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
    const uint8_t *buf, int len, int cid, time_t mjd,
    opentv_event_t *ev )
{
  int      slen = (((int)buf[2] & 0xf) << 8) | buf[3];
  int      i    = 4, r;

  if (slen+4 > len) {
    tvhtrace(LS_OPENTV, "event len (%d) > table len (%d)", slen+4, len);
    return -1;
  }

  ev->eid = ((uint16_t)buf[0] << 8) | buf[1];

  /* Process records */ 
  while (i < slen+4) {
    r = _opentv_parse_event_record(prov, ev, buf+i, len-i, mjd);
    if (r < 0)
      return -1;
    i += r;
  }
  return slen+4;
}

/* Parse an event section */
static int
opentv_parse_event_section_one
  ( opentv_status_t *sta, int cid, int mjd,
    channel_t *ch, const char *lang,
    const uint8_t *buf, int len )
{
  int i, r, save = 0, merge;
  opentv_module_t  *mod = sta->os_mod;
  epggrab_module_t *src = (epggrab_module_t*)mod;
  epg_broadcast_t *ebc;
  epg_episode_t *ee;
  epg_serieslink_t *es;
  opentv_event_t ev;
  char buffer[2048], *s;
  lang_str_t *ls;
  uint32_t changes, changes2, changes3;

  /* Loop around event entries */
  i = 7;
  while (i < len) {
    memset(&ev, 0, sizeof(opentv_event_t));
    r = _opentv_parse_event(mod, sta, buf+i, len-i, cid, mjd, &ev);
    if (r < 0) break;
    i += r;

    /*
     * Broadcast
     */

    merge = changes = changes2 = changes3 = 0;

    /* Find broadcast */
    if (ev.start && ev.stop) {
      ebc = epg_broadcast_find_by_time(ch, src, ev.start, ev.stop,
                                       1, &save, &changes);
      tvhdebug(LS_OPENTV, "find by time start %"PRItime_t " stop "
               "%"PRItime_t " eid %d = %p",
               ev.start, ev.stop, ev.eid, ebc);
      save |= epg_broadcast_set_dvb_eid(ebc, ev.eid, &changes);
    } else {
      ebc = epg_broadcast_find_by_eid(ch, ev.eid);
      tvhdebug(LS_OPENTV, "find by eid %d = %p", ev.eid, ebc);
      if (ebc && ebc->grabber != src)
        goto done;
      merge = 1;
    }
    if (!ebc)
      goto done;

    /* Summary / Description */
    if (ev.summary) {
      tvhdebug(LS_OPENTV, "  summary '%s'", ev.summary);
      ls = lang_str_create2(ev.summary, lang);
      save |= epg_broadcast_set_summary(ebc, ls, &changes);
      lang_str_destroy(ls);
    }
    if (ev.desc) {
      tvhdebug(LS_OPENTV, "  desc '%s'", ev.desc);
      ls = lang_str_create2(ev.desc, lang);
      save |= epg_broadcast_set_description(ebc, ls, &changes);
      lang_str_destroy(ls);
    }

    /*
     * Series link
     */

    if (ev.serieslink) {
      char suri[257], ubuf[UUID_HEX_SIZE];
      snprintf(suri, 256, "opentv://channel-%s/series-%d",
               channel_get_uuid(ch, ubuf), ev.serieslink);
      if ((es = epg_serieslink_find_by_uri(suri, src, 1, &save, &changes2))) {
        save |= epg_broadcast_set_serieslink(ebc, es, &changes);
        save |= epg_serieslink_change_finish(es, changes2, merge);
      }
    }

    /*
     * Episode
     */

    if ((ee = epg_episode_find_by_broadcast(ebc, src, 1, &save, &changes3))) {
      save |= epg_broadcast_set_episode(ebc, ee, &changes);
      tvhdebug(LS_OPENTV, "  find episode %p", ee);
      if (ev.title) {
        tvhdebug(LS_OPENTV, "    title '%s'", ev.title);

        /* try to cleanup the title */
        if (eit_pattern_apply_list(buffer, sizeof(buffer), ev.title, &mod->p_cleanup_title)) {
          tvhtrace(LS_OPENTV, "  clean title '%s'", buffer);
          s = buffer;
        } else {
          s = ev.title;
        }
        ls = lang_str_create2(s, lang);
        save |= epg_episode_set_title(ee, ls, &changes3);
        lang_str_destroy(ls);
      }
      if (ev.cat) {
        epg_genre_list_t *egl = calloc(1, sizeof(epg_genre_list_t));
        epg_genre_list_add_by_eit(egl, ev.cat);
        save |= epg_episode_set_genre(ee, egl, &changes3);
        epg_genre_list_destroy(egl);
      }
      if (ev.summary) {
        epg_episode_num_t en;

        memset(&en, 0, sizeof(en));
        /* search for season number */
        if (eit_pattern_apply_list(buffer, sizeof(buffer), ev.summary, &mod->p_snum))
          if ((en.s_num = atoi(buffer)))
            tvhtrace(LS_OPENTV,"  extract season number %d", en.s_num);
        /* ...for episode number */
        if (eit_pattern_apply_list(buffer, sizeof(buffer), ev.summary, &mod->p_enum))
          if ((en.e_num = atoi(buffer)))
            tvhtrace(LS_OPENTV,"  extract episode number %d", en.e_num);
        /* ...for part number */
        if (eit_pattern_apply_list(buffer, sizeof(buffer), ev.summary, &mod->p_pnum)) {
          if (buffer[0] >= 'a' && buffer[0] <= 'z')
            en.p_num = buffer[0] - 'a' + 1;
          else
            if (buffer[0] >= 'A' && buffer[0] <= 'Z')
              en.p_num = buffer[0] - 'A' + 1;
          if (en.p_num)
            tvhtrace(LS_OPENTV,"  extract part number %d", en.p_num);
        }
        /* save any found number */
        if (en.s_num || en.e_num || en.p_num)
          save |= epg_episode_set_epnum(ee, &en, &changes3);

        /* ...for subtitle */
        if (eit_pattern_apply_list(buffer, sizeof(buffer), ev.summary, &mod->p_subt)) {
          tvhtrace(LS_OPENTV, "  extract subtitle '%s'", buffer);
          ls = lang_str_create2(buffer, lang);
          save |= epg_episode_set_subtitle(ee, ls, &changes3);
          lang_str_destroy(ls);
        }
      }
      save |= epg_episode_change_finish(ee, changes3, merge);
    }

    save |= epg_broadcast_change_finish(ebc, changes, merge);

    /* Cleanup */
done:
    if (ev.title)   free(ev.title);
    if (ev.summary) free(ev.summary);
    if (ev.desc)    free(ev.desc);
  }

  return save;
}

static int
opentv_parse_event_section
  ( opentv_status_t *sta, int cid, int mjd,
    const uint8_t *buf, int len )
{
  opentv_module_t *mod = sta->os_mod;
  channel_t *ch;
  epggrab_channel_t *ec;
  idnode_list_mapping_t *ilm;
  const char *lang = NULL;
  int save = 0;

  /* Get language (bit of a hack) */
  if      (!strcmp(mod->dict->id, "skyit"))  lang = "it";
  else if (!strcmp(mod->dict->id, "skyeng")) lang = "eng";
  else if (!strcmp(mod->dict->id, "skynz"))  lang = "eng";

  /* Channel */
  if (!(ec = _opentv_find_epggrab_channel(mod, cid, 0, NULL))) return 0;

  /* Iterate all channels */
  LIST_FOREACH(ilm, &ec->channels, ilm_in2_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (!ch->ch_enabled || ch->ch_epg_parent) continue;
    save |= opentv_parse_event_section_one(sta, cid, mjd, ch,
                                           lang, buf, len);
  }

  /* Update EPG */
  if (save) epg_updated();
  return 0;
}

/* ************************************************************************
 * OpenTV channel processing
 * ***********************************************************************/

static int
opentv_desc_channels
  ( mpegts_table_t *mt, mpegts_mux_t *mm, uint16_t nbid,
    const uint8_t dtag, const uint8_t *buf, int len )
{
  opentv_status_t *sta = mt->mt_opaque;
  opentv_module_t *mod = sta->os_mod;
  epggrab_channel_t *ec;
  idnode_list_mapping_t *ilm;
  mpegts_service_t *svc;
  channel_t *ch;
  int sid, cid, cnum, unk;
  int type;
  int save = 0;
  int i = 2;

  while (i < len) {
    sid  = ((int)buf[i] << 8) | buf[i+1];
    type = buf[2];
    cid  = ((int)buf[i+3] << 8) | buf[i+4];
    cnum = ((int)buf[i+5] << 8) | buf[i+6];
    unk  = ((int)buf[i+7] << 8) | buf[i+8];
    tvhtrace(LS_OPENTV, "%s:     sid %04X type %02X cid %04X cnum %d unk %04X", mt->mt_name, sid, type, cid, cnum, unk);
    cnum = cnum < 65535 ? cnum : 0;

    /* Find the service */
    svc = mpegts_service_find(mm, sid, 0, 0, NULL);
    tvhtrace(LS_OPENTV, "%s:     svc %p [%s]", mt->mt_name, svc, svc ? svc->s_nicename : NULL);
    if (svc && svc->s_dvb_opentv_chnum != cnum &&
        (!svc->s_dvb_opentv_id || svc->s_dvb_opentv_id == unk)) {
      if (mod->bouquetid != nbid) {
        if (mod->bouquet_auto) {
          if (nbid < mod->bouquetid) {
            tvhwarn(LS_OPENTV, "%s: bouquet id set to %d, report this!", mt->mt_name, nbid);
            mod->bouquetid = nbid;
          } else
            goto skip_chnum;
        } else
          goto skip_chnum;
      }
      tvhtrace(LS_OPENTV, "%s:      cnum changed (%i != %i)", mt->mt_name, cnum, (int)svc->s_dvb_opentv_chnum);
      svc->s_dvb_opentv_chnum = cnum;
      svc->s_dvb_opentv_id = unk;
      mpegts_network_bouquet_trigger(mm->mm_network, 0);
      service_request_save((service_t *)svc, 0);
    }
skip_chnum:
    if (svc && LIST_FIRST(&svc->s_channels)) {
      ec  =_opentv_find_epggrab_channel(mod, cid, 1, &save);
      ilm = LIST_FIRST(&ec->channels);
      ch  = (channel_t *)LIST_FIRST(&svc->s_channels)->ilm_in2;
      tvhtrace(mt->mt_subsys, "%s:       ec = %p, ilm = %p", mt->mt_name, ec, ilm);

      if (ilm && ilm->ilm_in2 != &ch->ch_id) {
        epggrab_channel_link_delete(ec, ch, 1);
        ilm = NULL;
      }
      
      if (!ilm && ec->enabled)
        epggrab_channel_link(ec, ch, NULL);
      save |= epggrab_channel_set_number(ec, cnum, 0);
    }
    i += 9;
  }

  return 0;
}

static int
opentv_table_callback
  ( mpegts_table_t *mt, const uint8_t *buf, int len, int tableid )
{
  int r = 1, cid, mjd;
  int sect, last, ver;
  mpegts_psi_table_state_t *st;
  opentv_status_t *sta;
  opentv_module_t *mod;
  epggrab_ota_mux_t *ota;
  th_subscription_t *ths;

  if (!epggrab_ota_running) return -1;

  sta = mt->mt_opaque;
  mod = sta->os_mod;
  ota = sta->os_ota;

  /* Validate */
  if (len < 7) return -1;

  /* Extra ID */
  cid = ((int)buf[0] << 8) | buf[1];
  mjd = ((int)buf[5] << 8) | buf[6];
  mjd = (mjd - 40587) * 86400;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mt->mt_mux, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }


  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, buf, len,
                      tableid, (uint64_t)cid << 32 | mjd, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) goto done;

  /* Process */
  r = opentv_parse_event_section(sta, cid, mjd, buf, len);

  /* End */
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);

  /* Complete */
done:
  if (!r) {
    sta->os_map->om_first = 0; /* valid data mark */
    tvhtrace(mt->mt_subsys, "%s: pid %d complete remain %d",
             mt->mt_name, mt->mt_pid, sta->os_refcount-1);
  
    /* Last PID */
    if (sta->os_refcount == 1) {
  
      if (mt->mt_table == OPENTV_TITLE_BASE) {
        if (sta->os_titles_start + sec2mono(mod->titles_time) < mclk()) {
          int *t;
          tvhinfo(mt->mt_subsys, "%s: titles complete", mt->mt_name);

          /* Summaries */
          t = mod->summary;
          while (*t) {
            mpegts_table_t *mt2;
            mt2 = mpegts_table_add(mt->mt_mux,
                                   OPENTV_SUMMARY_BASE, OPENTV_TABLE_MASK,
                                   opentv_table_callback, sta,
                                   mod->id, LS_OPENTV, MT_CRC, *t++,
                                   MPS_WEIGHT_EIT);
            if (mt2) {
              sta->os_refcount++;
              mt2->mt_destroy    = opentv_status_destroy;
            }
          }
          mpegts_table_destroy(mt);
          sta->os_summaries_start = mclk();
        }
      } else {
        if (sta->os_summaries_start + sec2mono(mod->summaries_time) < mclk()) {
          tvhinfo(mt->mt_subsys, "%s: summaries complete", mt->mt_name);
          mpegts_table_destroy(mt);
          if (ota)
            epggrab_ota_complete((epggrab_module_ota_t*)mod, ota);
        }
      }
    } else {
      mpegts_table_destroy(mt);
    }
  }

  return 0;
}

static int
opentv_bat_callback
  ( mpegts_table_t *mt, const uint8_t *buf, int len, int tableid )
{
  int *t, r;
  opentv_status_t *sta;
  opentv_module_t *mod;
  epggrab_ota_mux_t *ota;
  th_subscription_t *ths;

  if (!epggrab_ota_running) return -1;

  sta = mt->mt_opaque;
  mod = sta->os_mod;
  ota = sta->os_ota;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mt->mt_mux, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  r = dvb_bat_callback(mt, buf, len, tableid);

  /* Register */
  if (!ota) {
    sta->os_ota = ota
      = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mt->mt_mux);
  }

  /* Complete */
  if (!r) {
    tvhinfo(mt->mt_subsys, "%s: channels complete", mt->mt_name);

    /* Install event handlers */
    t = mod->title;
    while (*t) {
      mpegts_table_t *mt2;
      mt2 = mpegts_table_add(mt->mt_mux,
                             OPENTV_TITLE_BASE, OPENTV_TABLE_MASK,
                             opentv_table_callback, mt->mt_opaque,
                             mod->id, LS_OPENTV, MT_CRC, *t++,
                             MPS_WEIGHT_EIT);
      if (mt2) {
        if (!mt2->mt_destroy) {
          sta->os_refcount++;
          mt2->mt_destroy    = opentv_status_destroy;
        }
      }
    }
    sta->os_titles_start = mclk();

    /* Remove BAT handler */
    mpegts_table_destroy(mt);
  }

  return r;
}

/* ************************************************************************
 * Module callbacks
 * ***********************************************************************/

static int _opentv_start
  ( epggrab_ota_map_t *map, mpegts_mux_t *mm )
{
  int *t;
  opentv_module_t *mod = (opentv_module_t*)map->om_module;
  epggrab_module_ota_t *m = map->om_module;
  opentv_status_t *sta = NULL;
  mpegts_table_t *mt;
  static struct mpegts_table_mux_cb bat_desc[] = {
    { .tag = 0xB1, .cb = opentv_desc_channels },
    { .tag = 0x00, .cb = NULL }
  };

  /* Ignore */
  if (!m->enabled && !map->om_forced) return -1;
  if (mod->tsid != mm->mm_tsid) return -1;

  /* Install tables */
  tvhdebug(mod->subsys, "%s: install table handlers", mod->id);

  /* Channels */
  t   = mod->channel;
  while (*t) {
    if (!sta) {
      sta = calloc(1, sizeof(opentv_status_t));
      sta->os_mod = mod;
      sta->os_map = map;
    }
    mt = mpegts_table_add(mm, DVB_BAT_BASE, DVB_BAT_MASK,
                          opentv_bat_callback, sta,
                          m->id, LS_OPENTV, MT_CRC, *t++,
                          MPS_WEIGHT_EIT);
    if (mt) {
      mt->mt_mux_cb  = bat_desc;
      if (!mt->mt_destroy) {
        sta->os_refcount++;
        mt->mt_destroy = opentv_status_destroy;
      }
    }
  }
  if (!sta->os_refcount)
    free(sta);

  // TODO: maybe if we had global cbs and event on complete we wouldn't
  //       have to potentially process the BAT twice (but is that really
  //       a big deal?)

  // Note: we process the data in a serial fashion, first we do channels
  //       then we do titles, then we do summaries
  return 0;
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
    tvhdebug(LS_OPENTV, "ignore duplicate genre map %s", id);
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
          tvhdebug(LS_OPENTV, "genre map %s loaded", f->hmf_name);
        else
          tvhwarn(LS_OPENTV, "genre map %s failed", f->hmf_name);
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
    tvhdebug(LS_OPENTV, "ignore duplicate dictionary %s", id);
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
          tvhdebug(LS_OPENTV, "dictionary %s loaded", f->hmf_name);
        else
          tvhwarn(LS_OPENTV, "dictionary %s failed", f->hmf_name);
      }
    }
  } 
  htsmsg_destroy(m);
}


static void _opentv_done( void *m )
{
  opentv_module_t *mod = (opentv_module_t *)m;

  free(mod->channel);
  free(mod->title);
  free(mod->summary);

  eit_pattern_free_list(&mod->p_snum);
  eit_pattern_free_list(&mod->p_enum);
  eit_pattern_free_list(&mod->p_pnum);
  eit_pattern_free_list(&mod->p_subt);
  eit_pattern_free_list(&mod->p_cleanup_title);
}

static int _opentv_tune
  ( epggrab_ota_map_t *map, epggrab_ota_mux_t *om, mpegts_mux_t *mm )
{
  epggrab_module_ota_t *m = map->om_module;
  opentv_module_t *mod = (opentv_module_t*)m;

  /* Ignore */
  if (!m->enabled) return 0;
  if (mod->tsid != mm->mm_tsid) return 0;

  return 1;
}

static int _opentv_prov_load_one ( const char *id, htsmsg_t *m )
{
  char ibuf[100], nbuf[1000];
  htsmsg_t *cl, *tl, *sl;
  uint32_t tsid, sid, onid, bouquetid;
  uint32_t titles_time, summaries_time;
  const char *str, *name;
  opentv_dict_t *dict;
  opentv_genre_t *genre;
  opentv_module_t *mod;
  static epggrab_ota_module_ops_t ops = {
    .start = _opentv_start,
    .done  = _opentv_done,
    .tune =  _opentv_tune,
  };

  /* Check config */
  if (!(name = htsmsg_get_str(m, "name"))) return -1;
  if (!(str  = htsmsg_get_str(m, "dict"))) return -1;
  if (!(dict = _opentv_dict_find(str))) return -1;
  if (!(cl   = htsmsg_get_list(m, "channel"))) return -1;
  if (!(tl   = htsmsg_get_list(m, "title"))) return -1;
  if (!(sl   = htsmsg_get_list(m, "summary"))) return -1;
  if (htsmsg_get_u32(m, "onid", &onid))
    if (htsmsg_get_u32(m, "nid", &onid)) return -1;
  if (htsmsg_get_u32(m, "tsid", &tsid)) return -1;
  if (htsmsg_get_u32(m, "sid", &sid)) return -1;
  if (htsmsg_get_u32(m, "bouquetid", &bouquetid)) return -1;
  titles_time = htsmsg_get_u32_or_default(m, "titles_time", 30);
  summaries_time = htsmsg_get_u32_or_default(m, "summaries_time", 240);

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
  mod = (opentv_module_t *)
    epggrab_module_ota_create(calloc(1, sizeof(opentv_module_t)),
                              ibuf, LS_OPENTV, NULL, nbuf, 2, 0, &ops);

  /* Add provider details */
  mod->dict     = dict;
  mod->genre    = genre;
  mod->onid     = onid;
  mod->tsid     = tsid;
  mod->sid      = sid;
  mod->bouquetid = bouquetid;
  mod->bouquet_auto = bouquetid == 0;
  mod->titles_time = MAX(titles_time, 120);
  mod->summaries_time = MAX(summaries_time, 600);
  mod->channel  = _pid_list_to_array(cl);
  mod->title    = _pid_list_to_array(tl);
  mod->summary  = _pid_list_to_array(sl);
  eit_pattern_compile_list(&mod->p_snum, htsmsg_get_list(m, "season_num"));
  eit_pattern_compile_list(&mod->p_enum, htsmsg_get_list(m, "episode_num"));
  eit_pattern_compile_list(&mod->p_pnum, htsmsg_get_list(m, "part_num"));
  eit_pattern_compile_list(&mod->p_subt, htsmsg_get_list(m, "subtitle"));
  eit_pattern_compile_list(&mod->p_cleanup_title, htsmsg_get_list(m, "cleanup_title"));

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
          tvhdebug(LS_OPENTV, "provider %s loaded", f->hmf_name);
        else
          tvhwarn(LS_OPENTV, "provider %s failed", f->hmf_name);
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

  /* Load dictionaries */
  if ((m = hts_settings_load("epggrab/opentv/dict")))
    _opentv_dict_load(m);
  tvhdebug(LS_OPENTV, "dictonaries loaded");

  /* Load genres */
  if ((m = hts_settings_load("epggrab/opentv/genre")))
    _opentv_genre_load(m);
  tvhdebug(LS_OPENTV, "genre maps loaded");

  /* Load providers */
  if ((m = hts_settings_load("epggrab/opentv/prov")))
    _opentv_prov_load(m);
  tvhdebug(LS_OPENTV, "providers loaded");
}

void opentv_done ( void )
{
  opentv_dict_t *dict;
  opentv_genre_t *genre;

  while ((dict = RB_FIRST(&_opentv_dicts)) != NULL) {
    RB_REMOVE(&_opentv_dicts, dict, h_link);
    huffman_tree_destroy(dict->codes);
    free(dict->id);
    free(dict);
  }
  while ((genre = RB_FIRST(&_opentv_genres)) != NULL) {
    RB_REMOVE(&_opentv_genres, genre, h_link);
    free(genre->id);
    free(genre);
  }
}

void opentv_load ( void )
{
  epggrab_module_t *m;

  LIST_FOREACH(m, &epggrab_modules, link)
    if (strncmp(m->id, "opentv-", 7) == 0)
      epggrab_module_channels_load(m->id);
}
