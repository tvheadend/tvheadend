/*
 *  Electronic Program Guide - psip grabber
 *  Copyright (C) 2012 Adam Sutton
 *                2014 Lauri Mylläri
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

typedef struct psip_event
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

} psip_event_t;

/* ************************************************************************
 * Diagnostics
 * ***********************************************************************/


/* ************************************************************************
 * EIT Event descriptors
 * ***********************************************************************/


/* ************************************************************************
 * EIT Event
 * ***********************************************************************/

static int
_psip_eit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  // int count, i;
  int count;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_mux_t         *mm  = mt->mt_mux;
  mpegts_service_t     *svc;
  mpegts_table_state_t *st;

  /* Validate */
  if (tableid != 0xcb) return -1;

  /* Extra ID - this identified the channel */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Look up channel based on the source id */
  LIST_FOREACH(svc, &mm->mm_services, s_dvb_mux_link) {
    if (svc->s_atsc_source_id == tsid)
      break;
  }
  if (!svc) {
    tvhwarn("psip", "EIT with no associated channel found (tsid 0x%04x)", tsid);
    return -1;
  }

  /* Begin */
  r = dvb_table_begin(mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhdebug("psip", "0x%04x: EIT tsid %04X (%s), ver %d",
		  mt->mt_pid, tsid, svc->s_dvb_svcname, ver);

  /* # events */
  count = ptr[6];
  tvhdebug("psip", "event count %d", count);
  ptr  += 7;
  len  -= 7;

  return dvb_table_end(mt, st, sect);
}

static int
_psip_ett_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_table_state_t *st;

  /* Validate */
  if (tableid != 0xcc) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin(mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhdebug("psip", "0x%04x: ETT tsid %04X (%d), ver %d", mt->mt_pid, tsid, tsid, ver);

  return dvb_table_end(mt, st, sect);
}


static int
_psip_mgt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
#if 0
  int r;
  int sect, last, ver, save, resched;
  uint8_t  seg;
  uint16_t onid, tsid, sid;
  uint32_t extraid;
  mpegts_service_t     *svc;
  mpegts_mux_t         *mm  = mt->mt_mux;
  epggrab_ota_map_t    *map = mt->mt_opaque;
  epggrab_module_t     *mod = (epggrab_module_t *)map->om_module;
  epggrab_ota_mux_t    *ota = NULL;
  mpegts_table_state_t *st;
#endif
  int r;
  int sect, last, ver;
  int count, i;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_mux_t         *mm  = mt->mt_mux;
  epggrab_ota_map_t    *map = mt->mt_opaque;
  mpegts_table_state_t *st;

  /* Validate */
  if (tableid != 0xc7) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin(mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhdebug("psip", "0x%04x: MGT tsid %04X (%d), ver %d", mt->mt_pid, tsid, tsid, ver);

  /* # tables */
  count = ptr[6] << 9 | ptr[7];
  tvhdebug("psip", "table count %d", count);
  ptr  += 8;
  len  -= 8;

  for (i = 0; i < count && len >= 11; i++) {
    unsigned int type, tablepid, tablever, tablesize;
    unsigned int dlen;
    dlen = ((ptr[9] & 0x3) << 8) | ptr[10];
    if (dlen + 11 > len) return -1;

    type = ptr[0] << 8 | ptr[1];
    tablepid = (ptr[2] & 0x1f) << 8 | ptr[3];
    tablever = ptr[4] & 0x1f;
    tablesize = ptr[5] << 24 | ptr[6] << 16 | ptr[7] << 8 | ptr[8];

    tvhdebug("psip", "table %d", i);
    tvhdebug("psip", "  type 0x%04X", type);
    tvhdebug("psip", "  pid  0x%04X", tablepid);
    tvhdebug("psip", "  ver  0x%04X", tablever);
    tvhdebug("psip", "  size 0x%08X", tablesize);

    if (type >= 0x100 && type <= 0x17f) {
      /* This is an EIT table */
      tvhdebug("psip", "  EIT-%d", type-0x100);
      mpegts_table_add(mm, 0xcb, 0xff, _psip_eit_callback, map, "eit", MT_QUICKREQ | MT_CRC | MT_RECORD, tablepid);
    } else if (type >= 0x200 && type <= 0x27f) {
      /* This is an ETT table */
      tvhdebug("psip", "  ETT-%d", type-0x200);
      mpegts_table_add(mm, 0xcc, 0xff, _psip_ett_callback, map, "ett", MT_QUICKREQ | MT_CRC | MT_RECORD, tablepid);
    } else if (type == 0x04) {
      /* This is channel ETT */
      tvhdebug("psip", "  ETT-channel");
      mpegts_table_add(mm, 0xcc, 0xff, _psip_ett_callback, map, "ett", MT_QUICKREQ | MT_CRC | MT_RECORD, tablepid);
    } else {
      /* Skip this table */
      goto next;
    }

    /* Move on */
next:
    ptr += dlen + 11;
    len -= dlen + 11;
  }

  return dvb_table_end(mt, st, sect);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static int _psip_start
  ( epggrab_ota_map_t *map, mpegts_mux_t *dm )
{
  epggrab_module_ota_t *m = map->om_module;
  int pid, opts = 0;

  /* Disabled */
  if (!m->enabled && !map->om_forced) return -1;

  pid  = 0x1FFB;
  opts = MT_QUICKREQ | MT_RECORD;

  /* Listen for Master Guide Table */
  mpegts_table_add(dm, 0xC7, 0xFF, _psip_mgt_callback, map, "mgt", MT_CRC | opts, pid);

  tvhlog(LOG_DEBUG, m->id, "installed table handlers");
  return 0;
}

static int _psip_tune
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

void psip_init ( void )
{
  static epggrab_ota_module_ops_t ops = {
    .start = _psip_start,
    .tune  = _psip_tune,
  };

  epggrab_module_ota_create(NULL, "psip", "PSIP: ATSC Grabber", 1, &ops, NULL);
}

void psip_done ( void )
{
}
