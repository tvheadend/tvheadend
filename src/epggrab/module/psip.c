/*
 *  Electronic Program Guide - psip grabber
 *  Copyright (C) 2012 Adam Sutton
 *                2014 Lauri Mylläri
 *                2015 Jaroslav Kysela
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

#define PSIP_EPG_TABLE_LIMIT 5

static int _psip_eit_callback(mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid);
static int _psip_ett_callback(mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid);

/* ************************************************************************
 * Status handling
 * ***********************************************************************/

typedef struct psip_table {
  TAILQ_ENTRY(psip_table) pt_link;
  mpegts_table_t         *pt_table;
  time_t                  pt_start;
  uint16_t                pt_pid;
  uint16_t                pt_type;
  uint8_t                 pt_complete;
} psip_table_t;

typedef struct psip_desc {
  RB_ENTRY(psip_desc)     pd_link;
  uint16_t                pd_eventid;
  uint8_t                *pd_data;
  uint32_t                pd_datalen;
} psip_desc_t;

typedef struct psip_status {
  epggrab_module_ota_t    *ps_mod;
  epggrab_ota_map_t       *ps_map;
  int                      ps_refcount;
  epggrab_ota_mux_t       *ps_ota;
  mpegts_mux_t            *ps_mm;
  TAILQ_HEAD(, psip_table) ps_tables;
  RB_HEAD(, psip_desc)     ps_descs;
  gtimer_t                 ps_reschedule_timer;
  uint8_t                  ps_complete;
  uint8_t                  ps_armed;
} psip_status_t;

static void
psip_status_destroy ( mpegts_table_t *mt )
{
  psip_status_t *st = mt->mt_opaque;
  psip_table_t *pt;
  psip_desc_t *pd;

  lock_assert(&global_lock);
  assert(st->ps_refcount > 0);
  --st->ps_refcount;
  if (!st->ps_refcount) {
    while ((pt = TAILQ_FIRST(&st->ps_tables)) != NULL) {
      TAILQ_REMOVE(&st->ps_tables, pt, pt_link);
      free(pt);
    }
    while ((pd = RB_FIRST(&st->ps_descs)) != NULL) {
      RB_REMOVE(&st->ps_descs, pd, pd_link);
      free(pd->pd_data);
      free(pd);
    }
    gtimer_disarm(&st->ps_reschedule_timer);
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
#define IS_ETT(t) ((t) == 0x04 || ((t) >= 0x200 && (t) <= 0x27f))

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
      ps->ps_refcount--;
      goto assign;
    }
    return pt;
  }
  pt = calloc(1, sizeof(*pt));
  TAILQ_INSERT_TAIL(&ps->ps_tables, pt, pt_link);
  pt->pt_pid = pid;
assign:
  pt->pt_type = type;
  pt->pt_complete = 0;
  return pt;
}

static mpegts_table_t *
psip_activate_table(psip_status_t *ps, psip_table_t *pt)
{
  mpegts_table_t *mt;

  if (IS_EIT(pt->pt_type)) {
    /* This is an EIT table */
    mt =  mpegts_table_add(ps->ps_mm, DVB_ATSC_EIT_BASE, DVB_ATSC_EIT_MASK,
                          _psip_eit_callback, ps, "aeit",
                          MT_CRC | MT_RECORD, pt->pt_pid,
                          MPS_WEIGHT_EIT);
  } else if (IS_ETT(pt->pt_type)) {
    /* This is an ETT table */
    mt = mpegts_table_add(ps->ps_mm, DVB_ATSC_ETT_BASE, DVB_ATSC_ETT_MASK,
                          _psip_ett_callback, ps, "ett",
                          MT_CRC | MT_RECORD, pt->pt_pid,
                          MPS_WEIGHT_ETT);
  } else {
    abort();
  }
  ps->ps_refcount++;
  mt->mt_destroy = psip_status_destroy;
  pt->pt_start = dispatch_clock;
  pt->pt_table = mt;
  tvhtrace("psip", "table activated - pid 0x%04X type 0x%04X", mt->mt_pid, pt->pt_type);
  return mt;
}

#define safecmp(a, b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

static int
_reschedule_cmp(const void *_a, const void *_b)
{
  const psip_table_t *a = *(psip_table_t **)_a;
  const psip_table_t *b = *(psip_table_t **)_b;
  int res = 0, va, vb;
  if (a->pt_table && b->pt_table)
    res = safecmp(a->pt_start, b->pt_start);
  else if (a->pt_table)
    res = -1;
  else {
    res = safecmp(a->pt_start, b->pt_start);
    if (res == 0) {
      va = (IS_ETT(a->pt_type) ? 0x10000 : 0) | a->pt_pid;
      vb = (IS_ETT(b->pt_type) ? 0x10000 : 0) | b->pt_pid;
      res = va - vb;
    }
  }
  return res;
}

static void
psip_reschedule_tables(psip_status_t *ps)
{
  psip_table_t **tables, *pt;
  int total, i;

  ps->ps_armed = 0;

  total = 0;
  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link) {
    total++;
    if (pt->pt_table && pt->pt_start + 10 < dispatch_clock) {
      tvhtrace("psip", "table late: pid = 0x%04X, type = 0x%04X\n", pt->pt_pid, pt->pt_type);
      mpegts_table_destroy(pt->pt_table);
      pt->pt_table = NULL;
    }
  }
  tables = malloc(total * sizeof(psip_table_t *));

  tvhtrace("psip", "reschedule tables, total %d", total);

  i = 0;
  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link)
    tables[i++] = pt;

  qsort(tables, total, sizeof(psip_table_t *), _reschedule_cmp);

#if 0
  for (i = 0; i < total; i++) {
    pt = tables[i];
    tvhtrace("psip", "sorted: pid = 0x%04X, type = 0x%04X, time = %"PRItime_t", complete %d\n",
             pt->pt_pid, pt->pt_type, pt->pt_start, pt->pt_complete);
  }
#endif

  for (i = 0; i < total && i < PSIP_EPG_TABLE_LIMIT; i++) {
    pt = tables[i];
    if (pt->pt_table)
      continue;
    psip_activate_table(ps, pt);
  }

  free(tables);
}

static void
psip_reschedule_tables_cb(void *aux)
{
  psip_reschedule_tables((psip_status_t *)aux);
}

static void
psip_complete_table(psip_status_t *ps, mpegts_table_t *mt)
{
  psip_table_t *pt = NULL;

  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link)
    if (pt->pt_table == mt) {
      pt->pt_table = NULL;
      pt->pt_complete = 1;
      break;
    }

  tvhtrace("psip", "pid 0x%04X completed, found = %d", mt->mt_pid, pt != NULL);

  mpegts_table_destroy(mt);

  if (pt)
    gtimer_arm(&ps->ps_reschedule_timer, psip_reschedule_tables_cb, ps, 0);

  if (ps->ps_ota == NULL)
    return;

  if (ps->ps_complete) {
    if (!ps->ps_armed) {
      ps->ps_armed = 1;
      gtimer_arm(&ps->ps_reschedule_timer, psip_reschedule_tables_cb, ps, 10);
    }
    return;
  }

  TAILQ_FOREACH(pt, &ps->ps_tables, pt_link)
    if (!pt->pt_complete)
      return;

  ps->ps_complete = 1;
  epggrab_ota_complete(ps->ps_mod, ps->ps_ota);
}

/* ************************************************************************
 * Description routines
 * ***********************************************************************/

/* Compare language codes */
static int _desc_cmp ( void *a, void *b )
{
  return (int)(((psip_desc_t*)a)->pd_eventid) -
         (int)(((psip_desc_t*)b)->pd_eventid);
}

static psip_desc_t *psip_find_desc(psip_status_t *ps, uint16_t eventid)
{
  psip_desc_t *pd, _tmp;

  _tmp.pd_eventid = eventid;
  pd = RB_FIND(&ps->ps_descs, &_tmp, pd_link, _desc_cmp);
  return pd;
}

static void psip_remove_desc(psip_status_t *ps, uint16_t eventid)
{
  psip_desc_t *pd = psip_find_desc(ps, eventid);
  if (pd) {
    RB_REMOVE(&ps->ps_descs, pd, pd_link);
    free(pd->pd_data);
    free(pd);
  }
}

static void psip_add_desc
  (psip_status_t *ps, uint16_t eventid, const uint8_t *data, uint32_t datalen)
{
  psip_desc_t *pc, *pd = calloc(1, sizeof(*pd));

  pd->pd_eventid = eventid;
  pc = RB_INSERT_SORTED(&ps->ps_descs, pd, pd_link, _desc_cmp);
  if (pc) {
    free(pd);
    pd = pc;
    if (pd->pd_datalen == datalen && memcmp(pd->pd_data, data, datalen) == 0)
      return;
    free(pd->pd_data);
  }
  pd->pd_data = malloc(datalen);
  memcpy(pd->pd_data, data, datalen);
  pd->pd_datalen = datalen;
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
  epg_episode_t *ee;
  lang_str_t *title, *description;
  psip_desc_t *pd;
  epggrab_module_t *mod = (epggrab_module_t *)ps->ps_mod;

  for (i = 0; len >= 12 && i < count; len -= size, ptr += size, i++) {
    eventid = (ptr[0] & 0x3f) << 8 | ptr[1];
    starttime = ptr[2] << 24 | ptr[3] << 16 | ptr[4] << 8 | ptr[5];
    start = atsc_convert_gpstime(starttime);
#if 0
    {
      static time_t debug_start = 0;
      static time_t debug_off;
      if (debug_start == 0) {
        debug_start = dispatch_clock;
        debug_off = start - (10 * 24 * 3600ULL);
      }
      start = debug_start + (start - debug_off);
    }
#endif
    length = (ptr[6] & 0x0f) << 16 | ptr[7] << 8 | ptr[8];
    stop = start + length;
    titlelen = ptr[9];

    if (12 + titlelen > len) break;

    dlen = ((ptr[10+titlelen] & 0x0f) << 8) | ptr[11+titlelen];
    size = titlelen + dlen + 12;
    tvhtrace("psip", "  %03d: titlelen %d, dlen %d", i, titlelen, dlen);

    if (size > len) break;

    title = atsc_get_string(ptr+10, titlelen);
    if (title == NULL) continue;

    tvhtrace("psip", "  %03d: [%s] eventid 0x%04x at %"PRItime_t", duration %d, title: '%s' (%d bytes)",
             i, ch ? channel_get_name(ch) : "(null)", eventid, start, length,
             lang_str_get(title, NULL), titlelen);

    ebc = epg_broadcast_find_by_time(ch, start, stop, eventid, 1, &save2);
    tvhtrace("psip", "  eid=%5d, start=%"PRItime_t", stop=%"PRItime_t", ebc=%p",
             eventid, start, stop, ebc);
    if (!ebc) goto next;
    save |= save2;

    pd = psip_find_desc(ps, eventid);
    if (pd) {
      description = atsc_get_string(pd->pd_data, pd->pd_datalen);
      if (description) {
        save |= epg_broadcast_set_description2(ebc, description, mod);
        lang_str_destroy(description);
      }
    }

    ee = epg_broadcast_get_episode(ebc, 1, &save2);
    save |= epg_episode_set_title2(ee, title, mod);

next:
    lang_str_destroy(title);
  }
  return save;
}

static void
_psip_eit_callback_cleanup
  (psip_status_t *ps, const uint8_t *ptr, int len, int count)
{
  uint16_t eventid;
  int i, size;
  uint8_t titlelen;
  unsigned int dlen;

  for (i = 0; len >= 12 && i < count; len -= size, ptr += size, i++) {
    eventid = (ptr[0] & 0x3f) << 8 | ptr[1];
    titlelen = ptr[9];
    dlen = ((ptr[10+titlelen] & 0x0f) << 8) | ptr[11+titlelen];
    size = titlelen + dlen + 12;
    if (size > len) break;
    psip_remove_desc(ps, eventid);
  }
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
  char ubuf[UUID_HEX_SIZE];

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
    tvhtrace("psip", "EIT with no associated channel found (tsid 0x%04x)", tsid);
    return -1;
  }

  /* Register interest */
  if (ps->ps_ota == NULL)
    ps->ps_ota = epggrab_ota_register((epggrab_module_ota_t*)mod, NULL, mm);

  /* Begin */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len, tableid, extraid, 7,
                      &st, &sect, &last, &ver);
  if (r == 0) goto complete;
  if (r != 1) return r;
  tvhtrace("psip", "0x%04x: EIT tsid %04X (%s), ver %d",
		  mt->mt_pid, tsid, svc->s_dvb_svcname, ver);

  /* # events */
  count = ptr[6];
  tvhtrace("psip", "event count %d data len %d", count, len);
  ptr  += 7;
  len  -= 7;

  /* Sanity check */
  if (len < 12) goto done;

  /* Register this */
  if (ps->ps_ota)
    epggrab_ota_service_add(map, ps->ps_ota,
                            idnode_uuid_as_str(&svc->s_id, ubuf), 1);

  /* For each associated channels */
  LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (!ch->ch_enabled || ch->ch_epg_parent) continue;
    save |= _psip_eit_callback_channel(ps, ch, ptr, len, count);
  }
  _psip_eit_callback_cleanup(ps, ptr, len, count);

  if (save)
    epg_updated();

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
complete:
  if (!r)
    psip_complete_table(ps, mt);

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
                      &st, &sect, &last, &ver);
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
    tvhtrace("psip", "ETT with no associated channel found (sourceid 0x%04x)", sourceid);
    return -1;
  }

  /* No point processing */
  if (!LIST_FIRST(&svc->s_channels))
    goto done;

  if (!isevent) {
    tvhtrace("psip", "0x%04x: channel ETT tableid 0x%04X [%s], ver %d", mt->mt_pid, tsid, svc->s_dvb_svcname, ver);
  } else {
    found = 1;
    description = atsc_get_string(ptr+10, len-10);
    LIST_FOREACH(ilm, &svc->s_channels, ilm_in1_link) {
      ch = (channel_t *)ilm->ilm_in2;
      epg_broadcast_t *ebc;
      ebc = epg_broadcast_find_by_eid(ch, eventid);
      if (ebc) {
        save |= epg_broadcast_set_description2(ebc, description, mod);
        tvhtrace("psip", "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d) ['%s'], ver %d",
                 mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid,
                 lang_str_get(ebc->episode->title, "eng"), ver);
      } else {
        found = 0;
      }
    }
    if (found == 0) {
      tvhtrace("psip", "0x%04x: ETT tableid 0x%04X [%s], eventid 0x%04X (%d), ver %d - no matching broadcast found [%.80s]",
               mt->mt_pid, tsid, svc->s_dvb_svcname, eventid, eventid, ver, lang_str_get(description, NULL));
      psip_add_desc(ps, eventid, ptr+10, len-10);
    }
    lang_str_destroy(description);
  }

  if (save)
    epg_updated();

done:
  r = dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
complete:
  if (!r)
    psip_complete_table(ps, mt);
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
                      &st, &sect, &last, &ver);
  if (r != 1) return r;

  /* # tables */
  count = ptr[6] << 9 | ptr[7];
  ptr  += 8;
  len  -= 8;

  tvhtrace("psip", "0x%04x: MGT tsid %04X (%d), ver %d, count %d", mt->mt_pid, tsid, tsid, ver, count);

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
  epggrab_module_ota_t *m = map->om_module;
  mpegts_table_t *mt;
  psip_status_t *ps;

  /* Disabled */
  if (!m->enabled && !map->om_forced) return -1;

  ps = calloc(1, sizeof(*ps));
  TAILQ_INIT(&ps->ps_tables);
  ps->ps_mod      = map->om_module;
  ps->ps_map      = map;
  ps->ps_mm       = dm;

  /* Listen for Master Guide Table */
  mt = mpegts_table_add(dm, DVB_ATSC_MGT_BASE, DVB_ATSC_MGT_MASK,
                        _psip_mgt_callback, ps, "mgt",
                        MT_CRC | MT_QUICKREQ | MT_RECORD,
                        DVB_ATSC_MGT_PID, MPS_WEIGHT_MGT);
  if (mt && !mt->mt_destroy) {
    ps->ps_refcount++;
    mt->mt_destroy = psip_status_destroy;
    tvhlog(LOG_DEBUG, m->id, "installed table handlers");
  }
  if (!ps->ps_refcount)
    free(ps);

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
