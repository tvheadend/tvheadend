/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * Electronic Program Guide - psip grabber
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

#define PSIP_EPG_TABLE_LIMIT 512

static int _psip_eit_callback(mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid);
static int _psip_ett_callback(mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid);

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct psip_table {
  TAILQ_ENTRY(psip_table) pt_link;
  mpegts_table_t         *pt_table; /* pointer to mpegts_table to receive EIT/ETT tables/sections */
  uint16_t                pt_pid;  /* packet ID */
  uint16_t                pt_type; /* EIT/ETT table type */
} psip_table_t;

typedef struct psip_status {
  epggrab_module_ota_t    *ps_mod;
  epggrab_ota_map_t       *ps_map;
  int                      ps_refcount;
  epggrab_ota_mux_t       *ps_ota;
  mpegts_mux_t            *ps_mm;
  TAILQ_HEAD(, psip_table) ps_tables;
} psip_status_t;

static void
psip_status_destroy ( mpegts_table_t *mt )
{
  psip_status_t *st = mt->mt_opaque;
  psip_table_t *pt;

  lock_assert(&global_lock);
  assert(st->ps_refcount > 0);
  --st->ps_refcount;
  if (!st->ps_refcount) {
    while ((pt = TAILQ_FIRST(&st->ps_tables)) != NULL) {
      TAILQ_REMOVE(&st->ps_tables, pt, pt_link);
      free(pt);
    }
    free(st);
  } else {
    TAILQ_FOREACH(pt, &st->ps_tables, pt_link)
      if (pt->pt_table == mt) {
        pt->pt_table = NULL;
        break;
      }
  }
}

/* ************************************************************************
 * Table routines
 * ***********************************************************************/

#define IS_EIT(t) ((t) >= 0x100 && (t) <= 0x17f)
#define IS_ETT(t) ((t) >= 0x200 && (t) <= 0x27f)

static psip_table_t *
psip_find_table(psip_status_t *ps, uint16_t pid)
{
  psip_table_t *pt;
  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link)
    if (pt->pt_pid == pid)
      return pt;
  return pt;
}

static psip_table_t *
psip_update_table(psip_status_t *ps, uint16_t pid, int type)
{
  psip_table_t *pt;

  if (!IS_EIT(type) && !IS_ETT(type))
    return NULL;
  pt = psip_find_table(ps, pid);
  if (pt) {
    if (pt->pt_type != type && pt->pt_table) {
      mpegts_table_destroy(pt->pt_table);
      pt->pt_table = NULL;
      goto assign;
    }
    return pt;
  }
  pt = calloc(1, sizeof(*pt));
  TAILQ_INSERT_TAIL(&ps->ps_tables, pt, pt_link);
  pt->pt_pid = pid;
assign:
  pt->pt_type = type;
  return pt;
}

static mpegts_table_t *
psip_activate_table(psip_status_t *ps, psip_table_t *pt)
{
  mpegts_table_t *mt;

  if (IS_EIT(pt->pt_type)) {
    /* This is an EIT table */
    mt =  mpegts_table_add(ps->ps_mm, DVB_ATSC_EIT_BASE, DVB_ATSC_EIT_MASK,
                          _psip_eit_callback, ps, "aeit", LS_PSIP,
                          MT_CRC | MT_RECORD, pt->pt_pid,
                          MPS_WEIGHT_EIT);
  } else if (IS_ETT(pt->pt_type)) {
    /* This is an ETT table */
    mt = mpegts_table_add(ps->ps_mm, DVB_ATSC_ETT_BASE, DVB_ATSC_ETT_MASK,
                          _psip_ett_callback, ps, "ett", LS_PSIP,
                          MT_CRC | MT_RECORD, pt->pt_pid,
                          MPS_WEIGHT_ETT);
  } else {
    abort();
  }
  ps->ps_refcount++;
  mt->mt_destroy = psip_status_destroy;
  pt->pt_table = mt;
  tvhtrace(LS_PSIP, "table activated - pid 0x%04X type 0x%04X", mt->mt_pid, pt->pt_type);
  return mt;
}

static void
psip_reschedule_tables(psip_status_t *ps)
{
  psip_table_t **tables, *pt;
  int total, i;

  total = 0;
  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link) {
    total++;
    if (pt->pt_table) {
      tvhtrace(LS_PSIP, "table late: pid = 0x%04X, type = 0x%04X\n", pt->pt_pid, pt->pt_type);
      mpegts_table_destroy(pt->pt_table);
      pt->pt_table = NULL;
    }
  }
  tables = malloc(total * sizeof(psip_table_t *));

  tvhtrace(LS_PSIP, "reschedule tables, total %d", total);

  i = 0;
  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link)
    tables[i++] = pt;

  for (i = 0; i < total && i < PSIP_EPG_TABLE_LIMIT; i++) {
    pt = tables[i];
    if (pt->pt_table)
      continue;
    psip_activate_table(ps, pt);
  }

  free(tables);
}

/* ************************************************************************
 * EIT Event
 * ***********************************************************************/

static int
_psip_eit_callback_channel
  (psip_status_t *ps, channel_t *ch, const uint8_t *ptr, int len, int count)
{
  uint16_t eventid;
  uint32_t starttime, length;
  time_t start, stop;
  int save = 0, save2, i, size;
  uint8_t titlelen;
  unsigned int dlen;
  epg_broadcast_t *ebc;
  lang_str_t *title;
  epg_changes_t changes2;
  epggrab_module_t *mod = (epggrab_module_t *)ps->ps_mod;
  int etmlocation;

  for (i = 0; len >= 12 && i < count; len -= size, ptr += size, i++) {
    eventid = (ptr[0] & 0x3f) << 8 | ptr[1];
    starttime = ptr[2] << 24 | ptr[3] << 16 | ptr[4] << 8 | ptr[5];
    start = atsc_convert_gpstime(starttime);
    etmlocation = (ptr[6] & 0x30) >> 4;
    length = (ptr[6] & 0x0f) << 16 | ptr[7] << 8 | ptr[8];
    stop = start + length;
    titlelen = ptr[9];

    if (12 + titlelen > len) break;

    dlen = ((ptr[10+titlelen] & 0x0f) << 8) | ptr[11+titlelen];
    size = titlelen + dlen + 12;
    tvhtrace(LS_PSIP, "  %03d: titlelen %d, dlen %d", i, titlelen, dlen);

    if (size > len) break;

    if (epg_channel_ignore_broadcast(ch, start)) continue;

    title = atsc_get_string(ptr+10, titlelen);
    if (title == NULL) continue;

    tvhtrace(LS_PSIP, "  %03d: [%s] eventid 0x%04x at %"PRItime_t", duration %d, etmlocation %x, title: '%s' (%d bytes)",
             i, ch ? channel_get_name(ch, channel_blank_name) : "(null)",
             eventid, start, length, etmlocation,
             lang_str_get(title, NULL), titlelen);

    save2 = changes2 = 0;

    ebc = epg_broadcast_find_by_time(ch, mod, start, stop, 1, &save2, &changes2);
    tvhtrace(LS_PSIP, "  eid=%5d, start=%"PRItime_t", stop=%"PRItime_t", ebc=%p",
             eventid, start, stop, ebc);
    if (!ebc) goto next;

    save2 |= epg_broadcast_set_dvb_eid(ebc, eventid, &changes2);
    if(etmlocation == 0)
      save2 |= epg_broadcast_set_description(ebc, NULL, NULL);

    save |= epg_broadcast_set_title(ebc, title, &changes2);
    save |= save2;

next:
    lang_str_destroy(title);
  }
  return save;
}

static int
_psip_eit_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  int count;
  int save = 0;
  uint16_t tsid;
  uint32_t extraid;
  mpegts_mux_t         *mm  = mt->mt_mux;
  psip_status_t        *ps  = mt->mt_opaque;
  epggrab_ota_map_t    *map = ps->ps_map;
  epggrab_module_t     *mod = (epggrab_module_t *)map->om_module;
  channel_t            *ch;
  mpegts_service_t     *svc;
  mpegts_psi_table_state_t *st;
  idnode_list_mapping_t *ilm;
  th_subscription_t    *ths;

  /* Validate */
  if (tableid != 0xcb) return -1;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mm, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  /* Extra ID - this identified the channel */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Look up channel based on the source id */
  LIST_FOREACH(svc, &mm->mm_services, s_dvb_mux_link) {
    if (svc->s_atsc_source_id == tsid)
      break;
  }
  if (!svc) {
    tvhtrace(LS_PSIP, "EIT with no associated channel found (tsid 0x%04x)", tsid);
    return -1;
  }

  /* Register interest */
  if (ps->ps_ota == NULL)
    ps->ps_ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver, 0);
  if (r == 0) goto complete;
  if (r != 1) return r;
  tvhtrace(LS_PSIP, "0x%04x: EIT tsid %04X (%s), ver %d",
		  mt->mt_pid, tsid, svc->s_dvb_svcname, ver);

  /* # events */
  count = ptr[6];
  tvhtrace(LS_PSIP, "event count %d data len %d", count, len);
  ptr  += 7;
  len  -= 7;

  /* Sanity check */
  if (len < 12) goto done;

  /* Register this */
  if (ps->ps_ota)
    epggrab_ota_service_add(map, ps->ps_ota, &svc->s_id.in_uuid, 1);

  /* For each associated channels */
  LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (!ch->ch_enabled || ch->ch_epg_parent) continue;
    save |= _psip_eit_callback_channel(ps, ch, ptr, len, count);
  }

  if (save)
    epg_updated();

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
complete:

  return r;
}

static int
_psip_ett_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r;
  int sect, last, ver;
  int save = 0, found = 0;
  uint16_t tsid;
  uint32_t extraid, sourceid, eventid;
  int isevent;
  mpegts_mux_t         *mm  = mt->mt_mux;
  psip_status_t        *ps  = mt->mt_opaque;
  epggrab_ota_map_t    *map = ps->ps_map;
  epggrab_module_t     *mod = (epggrab_module_t *)map->om_module;
  mpegts_service_t     *svc;
  mpegts_psi_table_state_t *st;
  th_subscription_t    *ths;
  lang_str_t *description;
  idnode_list_mapping_t *ilm;
  channel_t            *ch;
  epg_changes_t         changes;

  /* Validate */
  if (tableid != 0xcc) return -1;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mm, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver, 0);
  if (r == 0) goto complete;
  if (r != 1) return r;

  sourceid = (ptr[6] << 8) | ptr[7];
  eventid = (ptr[8] << 6) | ((ptr[9] >> 2) & 0x3f);
  isevent = (ptr[9] & 0x2) >> 1;

  /* Look up channel based on the source id */
  LIST_FOREACH(svc, &mm->mm_services, s_dvb_mux_link) {
    if (svc->s_atsc_source_id == sourceid)
      break;
  }
  if (!svc) {
    tvhtrace(LS_PSIP, "ETT with no associated channel found (sourceid 0x%04x)", sourceid);
    return -1;
  }

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  if (!isevent) {
    tvhtrace(LS_PSIP, "0x%04x: channel ETT tableid 0x%04X [%s], ver %d", mt->mt_pid, tsid, svc->s_dvb_svcname, ver);
  } else {
    found = 1;
    description = atsc_get_string(ptr+10, len-10);
    LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
      ch = (channel_t *)ilm->ilm_in2;
      epg_broadcast_t *ebc;
      changes = 0;
      ebc = epg_broadcast_find_by_eid(ch, eventid);
      if (ebc && ebc->grabber == mod) {
        save |= epg_broadcast_set_description(ebc, description, &changes);
        tvhtrace(LS_PSIP, "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d) ['%s'], ver %d",
                 mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid,
                 lang_str_get(ebc->title, "eng"), ver);
      } else {
        found = 0;
      }
    }
    if (found == 0) {
      tvhtrace(LS_PSIP, "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d), ver %d - no matching broadcast found [%.80s]",
               mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid, ver, lang_str_get(description, NULL));
    }
    lang_str_destroy(description);
  }

  if (save)
    epg_updated();

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);

  /* Reset version caching for ETT tables. */
  dvb_table_reset((mpegts_psi_table_t *)mt);

complete:
  return r;
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
  psip_status_t        *ps  = mt->mt_opaque;
  mpegts_psi_table_state_t *st;
  th_subscription_t    *ths;

  /* Validate */
  if (tableid != 0xc7) return -1;

  /* Statistics */
  ths = mpegts_mux_find_subscription_by_name(mm, "epggrab");
  if (ths) {
    subscription_add_bytes_in(ths, len);
    subscription_add_bytes_out(ths, len);
  }

  /* Extra ID */
  tsid    = ptr[0] << 8 | ptr[1];
  extraid = tsid;

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver, 0);
  if (r != 1) return r;

  /* # tables */
  count = ptr[6] << 8 | ptr[7];
  ptr  += 8;
  len  -= 8;

  tvhtrace(LS_PSIP, "0x%04x: MGT tsid %04X (%d), ver %d, count %d", mt->mt_pid, tsid, tsid, ver, count);

  for (i = 0; i < count && len >= 11; i++) {
    unsigned int type, tablepid, tablever, tablesize;
    unsigned int dlen;
    dlen = ((ptr[9] & 0x3) << 8) | ptr[10];
    if (dlen + 11 > len) return -1;

    type = ptr[0] << 8 | ptr[1];
    tablepid = (ptr[2] & 0x1f) << 8 | ptr[3];
    tablever = ptr[4] & 0x1f;
    tablesize = ptr[5] << 24 | ptr[6] << 16 | ptr[7] << 8 | ptr[8];

    tvhdebug(LS_PSIP, "table %d - type 0x%04X, pid 0x%04X, ver 0x%04X, size 0x%08X",
             i, type, tablepid, tablever, tablesize);

    psip_update_table(ps, tablepid, type);

    /* Move on */
    ptr += dlen + 11;
    len -= dlen + 11;
  }

  psip_reschedule_tables(ps);

  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static int _psip_start
  ( epggrab_ota_map_t *map, mpegts_mux_t *dm )
{
  mpegts_table_t *mt;
  psip_status_t *ps;

  ps = calloc(1, sizeof(*ps));
  TAILQ_INIT(&ps->ps_tables);
  ps->ps_mod      = map->om_module;
  ps->ps_map      = map;
  ps->ps_mm       = dm;

  /* Listen for Master Guide Table */
  mt = mpegts_table_add(dm, DVB_ATSC_MGT_BASE, DVB_ATSC_MGT_MASK,
                        _psip_mgt_callback, ps, "mgt", LS_TBL_EIT,
                        MT_CRC | MT_RECORD,
                        DVB_ATSC_MGT_PID, MPS_WEIGHT_MGT);
  if (mt && !mt->mt_destroy) {
    ps->ps_refcount++;
    mt->mt_destroy = psip_status_destroy;
    tvhdebug(mt->mt_subsys, "%s: installed table handlers", mt->mt_name);
  }
  if (!ps->ps_refcount)
    free(ps);

  return 0;
}

static int _psip_stop
  ( epggrab_ota_map_t *map, mpegts_mux_t *dm )
{
  tvhdebug(LS_PSIP, "Calling _psip_stop");

  return 0;
}

static void _psip_done (void *x)
{
  tvhdebug(LS_PSIP, "Calling _psip_done");
}

static int _psip_tune
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
  // FIXME: copied from the eit module - is this a good idea here?
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

htsmsg_t *psip_module_id_list( const char *lang )
{
  htsmsg_t *e, *l = htsmsg_create_list();
  e = htsmsg_create_key_val("psip", "PSIP: ATSC Grabber");
  htsmsg_add_msg(l, NULL, e);
  return l;
}

const char *psip_check_module_id ( const char *id )
{
  if (id && strcmp(id, "psip") == 0)
    return "psip";
  return NULL;
}

void psip_init ( void )
{
  static epggrab_ota_module_ops_t ops = {
    .start = _psip_start,
    .stop  = _psip_stop,
    .done  = _psip_done,
    .tune  = _psip_tune,
  };

  epggrab_module_ota_create(NULL, "psip", LS_PSIP, NULL, "PSIP: ATSC Grabber",
                            1, NULL, &ops);
}
