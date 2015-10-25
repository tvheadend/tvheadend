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
  int count, i;
  int save = 0;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_mux_t         *mm  = mt->mt_mux;
  epggrab_ota_map_t    *map = mt->mt_opaque;
  epggrab_module_t     *mod = (epggrab_module_t *)map->om_module;
  epggrab_ota_mux_t    *ota = NULL;
  mpegts_service_t     *svc;
  mpegts_psi_table_state_t *st;
  char ubuf[UUID_HEX_SIZE];

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
    tvhtrace("psip", "EIT with no associated channel found (tsid 0x%04x)", tsid);
    return -1;
  }

  /* Register interest */
  ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhtrace("psip", "0x%04x: EIT tsid %04X (%s), ver %d",
		  mt->mt_pid, tsid, svc->s_dvb_svcname, ver);

  /* # events */
  count = ptr[6];
  tvhtrace("psip", "event count %d", count);
  ptr  += 7;
  len  -= 7;

  /* Register this */
  if (ota)
    epggrab_ota_service_add(map, ota, idnode_uuid_as_str(&svc->s_id, ubuf), 1);

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  for (i = 0; i < count && len >= 12; i++) {
    uint16_t eventid;
    uint32_t starttime, length;
    time_t start, stop;
    int save2 = 0;
    uint8_t titlelen;
    unsigned int dlen;
    char buf[512];
    epg_broadcast_t *ebc;
    epg_episode_t *ee;
    channel_t *ch = (channel_t *)LIST_FIRST(&svc->s_channels)->ilm_in2;
    lang_str_t       *title;

    eventid = (ptr[0] & 0x3f) << 8 | ptr[1];
    starttime = ptr[2] << 24 | ptr[3] << 16 | ptr[4] << 8 | ptr[5];
    start = atsc_convert_gpstime(starttime);
    length = (ptr[6] & 0x0f) << 16 | ptr[7] << 8 | ptr[8];
    stop = start + length;
    titlelen = ptr[9];
    dlen = ((ptr[10+titlelen] & 0x0f) << 8) | ptr[11+titlelen];
    // tvhtrace("psip", "  %03d: titlelen %d, dlen %d", i, titlelen, dlen);

    if (titlelen + dlen + 12 > len) return -1;

    atsc_get_string(buf, sizeof(buf), &ptr[10], titlelen, "eng");

    tvhtrace("psip", "  %03d: 0x%04x at %"PRItime_t", duration %d, title: '%s' (%d bytes)",
      i, eventid, start, length, buf, titlelen);

    ebc = epg_broadcast_find_by_time(ch, start, stop, eventid, 1, &save2);
    tvhtrace("psip", "  svc='%s', ch='%s', eid=%5d, start=%"PRItime_t","
        " stop=%"PRItime_t", ebc=%p",
        svc->s_dvb_svcname ?: "(null)", ch ? channel_get_name(ch) : "(null)",
        eventid, start, stop, ebc);
    if (!ebc) goto next;

    title = lang_str_create();
    lang_str_add(title, buf, "eng", 0);

    ee = epg_broadcast_get_episode(ebc, 1, &save2);
    save2 |= epg_episode_set_title2(ee, title, mod);

    lang_str_destroy(title);

    save |= save2;

    /* Move on */
next:
    ptr += titlelen + dlen + 12;
    len -= titlelen + dlen + 12;
  }

  if (save)
    epg_updated();

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
  if (ota && !r)
    epggrab_ota_complete((epggrab_module_ota_t*)mod, ota);

  return r;
}

static int
_psip_ett_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  int save = 0;
  uint16_t tsid;
  uint32_t extraid, sourceid, eventid;
  int isevent;
  mpegts_mux_t         *mm  = mt->mt_mux;
  epggrab_ota_map_t    *map = mt->mt_opaque;
  epggrab_module_t     *mod = (epggrab_module_t *)map->om_module;
  mpegts_service_t     *svc;
  mpegts_psi_table_state_t *st;
  char buf[4096];

  /* Validate */
  if (tableid != 0xcc) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;

  sourceid = ptr[6] << 8 | ptr[7];
  eventid = ptr[8] << 8 | ptr[9] >> 2;
  isevent = (ptr[9] & 0x2) >> 1;

  /* Look up channel based on the source id */
  LIST_FOREACH(svc, &mm->mm_services, s_dvb_mux_link) {
    if (svc->s_atsc_source_id == sourceid)
      break;
  }
  if (!svc) {
    tvhtrace("psip", "ETT with no associated channel found (sourceid 0x%04x)", sourceid);
    return -1;
  }

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  atsc_get_string(buf, sizeof(buf), &ptr[10], len-4, "eng"); // FIXME: len does not account for previous bytes

  if (!isevent) {
    tvhtrace("psip", "0x%04x: channel ETT tableid 0x%04X [%s], ver %d", mt->mt_pid, tsid, svc->s_dvb_svcname, ver);
  } else {
    channel_t *ch = (channel_t *)LIST_FIRST(&svc->s_channels)->ilm_in2;
    epg_broadcast_t *ebc;
    ebc = epg_broadcast_find_by_eid(ch, eventid);
    if (ebc) {
      lang_str_t *description;
      description = lang_str_create();
      lang_str_add(description, buf, "eng", 0);
      save |= epg_broadcast_set_description2(ebc, description, mod);
      lang_str_destroy(description);
      tvhtrace("psip", "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d) ['%s'], ver %d", mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid, lang_str_get(ebc->episode->title, "eng"), ver);
    } else {
      tvhtrace("psip", "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d), ver %d - no matching broadcast found", mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid, ver);
    }
  }

  tvhtrace("psip", "        text message: '%s'", buf);

  if (save)
    epg_updated();

done:
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}


static int
_psip_mgt_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  int count, i;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_mux_t         *mm  = mt->mt_mux;
  epggrab_ota_map_t    *map = mt->mt_opaque;
  mpegts_psi_table_state_t *st;

  /* Validate */
  if (tableid != 0xc7) return -1;

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r != 1) return r;
  tvhtrace("psip", "0x%04x: MGT tsid %04X (%d), ver %d", mt->mt_pid, tsid, tsid, ver);

  /* # tables */
  count = ptr[6] << 9 | ptr[7];
  tvhtrace("psip", "table count %d", count);
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

    tvhdebug("psip", "table %d - type 0x%04X, pid 0x%04X, ver 0x%04X, size 0x%08X",
             i, type, tablepid, tablever, tablesize);

    if (type >= 0x100 && type <= 0x17f) {
      /* This is an EIT table */
      mpegts_table_add(mm, DVB_ATSC_EIT_BASE, DVB_ATSC_EIT_MASK, _psip_eit_callback,
                       map, "aeit", MT_QUICKREQ | MT_CRC | MT_RECORD, tablepid,
                       MPS_WEIGHT_EIT);
    } else if (type == 0x04 || (type >= 0x200 && type <= 0x27f)) {
      /* This is an ETT table */
      mpegts_table_add(mm, DVB_ATSC_ETT_BASE, DVB_ATSC_ETT_MASK, _psip_ett_callback,
                       map, "ett", MT_QUICKREQ | MT_CRC | MT_RECORD, tablepid,
                       MPS_WEIGHT_ETT);
    } else {
      /* Skip this table */
      goto next;
    }

    /* Move on */
next:
    ptr += dlen + 11;
    len -= dlen + 11;
  }

  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
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

  pid  = DVB_ATSC_MGT_PID;
  opts = MT_QUICKREQ | MT_RECORD;

  /* Listen for Master Guide Table */
  mpegts_table_add(dm, DVB_ATSC_MGT_BASE, DVB_ATSC_MGT_MASK, _psip_mgt_callback,
                   map, "mgt", MT_CRC | opts, pid, MPS_WEIGHT_MGT);

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
  // FIXME: copied from the eit module - is this a good idea here?
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

  epggrab_module_ota_create(NULL, "psip", NULL, "PSIP: ATSC Grabber", 1, &ops);
}

void psip_done ( void )
{
}
