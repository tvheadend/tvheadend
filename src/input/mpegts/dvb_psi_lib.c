/*
 *  MPEG TS Program Specific Information Library code
 *  Copyright (C) 2007 - 2010 Andreas Öman
 *  Copyright (C) 2015 Jaroslav Kysela
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

#include "tvheadend.h"
#include "input.h"
#include "dvb.h"

SKEL_DECLARE(mpegts_table_state_skel, struct mpegts_table_state);

/* **************************************************************************
 * Lookup tables
 * *************************************************************************/

static const int dvb_servicetype_map[][2] = {
  { 0x01, ST_SDTV  }, /* SDTV (MPEG2) */
  { 0x02, ST_RADIO },
  { 0x11, ST_HDTV  }, /* HDTV (MPEG2) */
  { 0x16, ST_SDTV  }, /* Advanced codec SDTV */
  { 0x19, ST_HDTV  }, /* Advanced codec HDTV */
  { 0x80, ST_SDTV  }, /* NET POA - Cabo SDTV */
  { 0x91, ST_HDTV  }, /* Bell TV HDTV */
  { 0x96, ST_SDTV  }, /* Bell TV SDTV */
  { 0xA0, ST_HDTV  }, /* Bell TV tiered HDTV */
  { 0xA4, ST_HDTV  }, /* DN HDTV */
  { 0xA6, ST_HDTV  }, /* Bell TV tiered HDTV */
  { 0xA8, ST_SDTV  }, /* DN advanced SDTV */
  { 0xD3, ST_SDTV  }, /* SKY TV SDTV */
};

int
dvb_servicetype_lookup ( int t )
{
  int i;
  for (i = 0; i < ARRAY_SIZE(dvb_servicetype_map); i++) {
    if (dvb_servicetype_map[i][0] == t)
      return dvb_servicetype_map[i][1];
  }
  return -1;
}

/* **************************************************************************
 * Tables
 * *************************************************************************/

/*
 * Section assembly
 */
static int
mpegts_psi_section_reassemble0
  ( mpegts_psi_section_t *ps, const uint8_t *data,
    int len, int start, int crc,
    mpegts_psi_section_callback_t cb, void *opaque)
{
  int excess, tsize;

  if(start) {
    // Payload unit start indicator
    ps->ps_offset = 0;
    ps->ps_lock = 1;
  }

  if(!ps->ps_lock)
    return -1;

  memcpy(ps->ps_data + ps->ps_offset, data, len);
  ps->ps_offset += len;

  if(ps->ps_offset < 3) {
    /* We don't know the total length yet */
    return len;
  }

  tsize = 3 + (((ps->ps_data[1] & 0xf) << 8) | ps->ps_data[2]);

  if(ps->ps_offset < tsize)
    return len; // Not there yet

  excess = ps->ps_offset - tsize;

  if(crc && tvh_crc32(ps->ps_data, tsize, 0xffffffff))
    return -1;

  ps->ps_offset = 0;
  if (cb)
    cb(ps->ps_data, tsize - (crc ? 4 : 0), opaque);
  return len - excess;
}

/**
 *
 */
void
mpegts_psi_section_reassemble
  (mpegts_psi_section_t *ps, const uint8_t *tsb, int crc, int ccerr,
   mpegts_psi_section_callback_t cb, void *opaque)
{
  int off  = tsb[3] & 0x20 ? tsb[4] + 5 : 4;
  int pusi = tsb[1] & 0x40;
  int r;

  if (ccerr)
    ps->ps_lock = 0;

  if(off >= 188) {
    ps->ps_lock = 0;
    return;
  }

  if(pusi) {
    int len = tsb[off++];
    if(len > 0) {
      if(len > 188 - off) {
        ps->ps_lock = 0;
        return;
      }
      mpegts_psi_section_reassemble0(ps, tsb + off, len, 0, crc, cb, opaque);
      off += len;
    }
  }

  while(off < 188) {
    r = mpegts_psi_section_reassemble0(ps, tsb + off, 188 - off, pusi, crc,
        cb, opaque);
    if(r < 0) {
      ps->ps_lock = 0;
      break;
    }
    off += r;
    pusi = 0;
  }
}

/*
 *
 */

static int sect_cmp
  ( struct mpegts_table_state *a, struct mpegts_table_state *b )
{
  if (a->tableid != b->tableid)
    return a->tableid - b->tableid;
  if (a->extraid < b->extraid)
    return -1;
  if (a->extraid > b->extraid)
    return 1;
  return 0;
}

static void
mpegts_table_state_reset
  ( mpegts_table_t *mt, mpegts_table_state_t *st, int last )
{
  int i;
  mt->mt_finished = 0;
  st->complete = 0;
  st->version = 0xff;  /* invalid */
  memset(st->sections, 0, sizeof(st->sections));
  for (i = 0; i < last / 32; i++)
    st->sections[i] = 0xFFFFFFFF;
  st->sections[last / 32] = 0xFFFFFFFF << (31 - (last % 32));
}

static struct mpegts_table_state *
mpegts_table_state_find
  ( mpegts_table_t *mt, int tableid, uint64_t extraid, int last )
{
  struct mpegts_table_state *st;

  /* Find state */
  SKEL_ALLOC(mpegts_table_state_skel);
  mpegts_table_state_skel->tableid = tableid;
  mpegts_table_state_skel->extraid = extraid;
  st = RB_INSERT_SORTED(&mt->mt_state, mpegts_table_state_skel, link, sect_cmp);
  if (!st) {
    st   = mpegts_table_state_skel;
    SKEL_USED(mpegts_table_state_skel);
    mt->mt_incomplete++;
    mpegts_table_state_reset(mt, st, last);
  }
  return st;
}

/*
 * End table
 */
static int
dvb_table_complete
  (mpegts_table_t *mt)
{
  if (mt->mt_incomplete || !mt->mt_complete) {
    int total = 0;
    mpegts_table_state_t *st;
    RB_FOREACH(st, &mt->mt_state, link)
      total++;
    tvhtrace(mt->mt_name, "incomplete %d complete %d total %d",
             mt->mt_incomplete, mt->mt_complete, total);
    return 2;
  }
  if (!mt->mt_finished)
    tvhdebug(mt->mt_name, "completed pid %d table %08X / %08x", mt->mt_pid, mt->mt_table, mt->mt_mask);
  mt->mt_finished = 1;
  return 0;
}

int
dvb_table_end
  (mpegts_table_t *mt, mpegts_table_state_t *st, int sect)
{
  int sa, sb;
  uint32_t rem;
  if (st && !st->complete) {
    assert(sect >= 0 && sect <= 255);
    sa = sect / 32;
    sb = sect % 32;
    st->sections[sa] &= ~(0x1 << (31 - sb));
    if (!st->sections[sa]) {
      rem = 0;
      for (sa = 0; sa < 8; sa++)
        rem |= st->sections[sa];
      if (rem) return 1;
      tvhtrace(mt->mt_name, "  tableid %02X extraid %016" PRIx64 " completed",
               st->tableid, st->extraid);
      st->complete = 1;
      mt->mt_incomplete--;
      return dvb_table_complete(mt);
    }
  } else if (st)
    return dvb_table_complete(mt);
  return 2;
}

/*
 * Begin table
 */
int
dvb_table_begin
  (mpegts_table_t *mt, const uint8_t *ptr, int len,
   int tableid, uint64_t extraid, int minlen,
   mpegts_table_state_t **ret, int *sect, int *last, int *ver)
{
  mpegts_table_state_t *st;
  uint32_t sa, sb;

  /* Not long enough */
  if (len < minlen) {
    tvhdebug(mt->mt_name, "invalid table length %d min %d", len, minlen);
    return -1;
  }

  /* Ignore next */
  if((ptr[2] & 1) == 0)
    return -1;

  tvhtrace(mt->mt_name, "pid %02X tableid %02X extraid %016" PRIx64 " len %d",
           mt->mt_pid, tableid, extraid, len);

  /* Section info */
  if (sect && ret) {
    *sect = ptr[3];
    *last = ptr[4];
    *ver  = (ptr[2] >> 1) & 0x1F;
    *ret = st = mpegts_table_state_find(mt, tableid, extraid, *last);
    tvhtrace(mt->mt_name, "  section %d last %d ver %d (ver %d st %d incomp %d comp %d)",
             *sect, *last, *ver, st->version, st->complete, mt->mt_incomplete, mt->mt_complete);

    /* Ignore previous version */
    /* This check is for the broken PMT tables where:
     * last 0 version 21 = PCR + Audio PID 0x0044
     * last 0 version 22 = Audio PID 0x0044, PCR + Video PID 0x0045
     */
    if (*last == 0 && st->version - 1 == *ver)
      return -1;

    /* New version */
    if (st->version != *ver) {
      if (st->complete == 2)
        mt->mt_complete--;
      if (st->complete)
        mt->mt_incomplete++;
      tvhtrace(mt->mt_name, "  new version, restart");
      mpegts_table_state_reset(mt, st, *last);
      st->version = *ver;
    }

    /* Complete? */
    if (st->complete) {
      tvhtrace(mt->mt_name, "  skip, already complete (%i)", st->complete);
      if (st->complete == 1) {
        st->complete = 2;
        mt->mt_complete++;
        return dvb_table_complete(mt);
      } else if (st->complete == 2) {
        return dvb_table_complete(mt);
      }
      assert(0);
    }

    /* Already seen? */
    sa = *sect / 32;
    sb = *sect % 32;
    if (!(st->sections[sa] & (0x1 << (31 - sb)))) {
      tvhtrace(mt->mt_name, "  skip, already seen");
      return -1;
    }
  }

  tvhlog_hexdump(mt->mt_name, ptr, len);

  return 1;
}

void
dvb_table_reset(mpegts_table_t *mt)
{
  mpegts_table_state_t *st;

  tvhtrace(mt->mt_name, "pid %02X complete reset", mt->mt_pid);
  mt->mt_incomplete = 0;
  mt->mt_complete   = 0;
  while ((st = RB_FIRST(&mt->mt_state)) != NULL) {
    RB_REMOVE(&mt->mt_state, st, link);
    free(st);
  }
}
