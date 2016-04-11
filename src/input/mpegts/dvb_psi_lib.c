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

SKEL_DECLARE(mpegts_psi_table_state_skel, struct mpegts_psi_table_state);

/* **************************************************************************
 * Lookup tables
 * *************************************************************************/

static const int dvb_servicetype_map[][2] = {
  { 0x01, ST_SDTV  }, /* SDTV (MPEG2) */
  { 0x02, ST_RADIO },
  { 0x11, ST_HDTV  }, /* HDTV (MPEG2) */
  { 0x16, ST_SDTV  }, /* Advanced codec SDTV */
  { 0x17, ST_SDTV  }, /* Advanced codec SDTV - NVOD time-shifted */
  { 0x18, ST_SDTV  }, /* Advanced codec SDTV - NVOD reference */
  { 0x19, ST_HDTV  }, /* Advanced codec HDTV */
  { 0x1A, ST_HDTV  }, /* Advanced codec HDTV - NVOD time-shifted */
  { 0x1B, ST_HDTV  }, /* Advanced codec HDTV - NVOD reference */
  { 0x1C, ST_HDTV  }, /* Advanced codec HDTV - plano-stereoscopic */
  { 0x1D, ST_HDTV  }, /* Advanced codec HDTV - plano-stereoscopic - NVOD time-shifted */
  { 0x1E, ST_HDTV  }, /* Advanced codec HDTV - plano-stereoscopic - NVOID reference */
  { 0x1F, ST_UHDTV }, /* HEVC (assume all HEVC content is UHD?) */
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
  ( mpegts_psi_table_t *mt, const char *logpref,
    const uint8_t *data,
    int len, int start, int crc,
    mpegts_psi_section_callback_t cb, void *opaque)
{
  uint8_t *p = mt->mt_sect.ps_data;
  int excess, tsize;

  if(start) {
    // Payload unit start indicator
    mt->mt_sect.ps_offset = 0;
    mt->mt_sect.ps_lock = 1;
  }

  if(!mt->mt_sect.ps_lock)
    return -1;

  memcpy(p + mt->mt_sect.ps_offset, data, len);
  mt->mt_sect.ps_offset += len;

  if(mt->mt_sect.ps_offset < 3) {
    /* We don't know the total length yet */
    return len;
  }

  tsize = 3 + (((p[1] & 0xf) << 8) | p[2]);

  if(mt->mt_sect.ps_offset < tsize)
    return len; // Not there yet

  if(p[0] == 0x72) { /* stuffing section */
    dvb_table_reset(mt);
    cb = NULL;
    crc = 0;
  }

  if(crc && tvh_crc32(p, tsize, 0xffffffff)) {
    if (tvhlog_limit(&mt->mt_err_log, 10)) {
      tvhwarn(mt->mt_name, "%s: invalid checksum (len %i, errors %zi)",
              logpref, tsize, mt->mt_err_log.count);
    }
    return -1;
  }

  excess = mt->mt_sect.ps_offset - tsize;
  mt->mt_sect.ps_offset = 0;

  if (cb)
    cb(p, tsize - (crc ? 4 : 0), opaque);

  return len - excess;
}

/**
 *
 */
void
mpegts_psi_section_reassemble
  (mpegts_psi_table_t *mt, const char *logprefix,
   const uint8_t *tsb, int crc,
   mpegts_psi_section_callback_t cb, void *opaque)
{
  int pusi   = tsb[1] & 0x40;
  uint8_t cc = tsb[3];
  int off    = cc & 0x20 ? tsb[4] + 5 : 4;
  int r;

  if (cc & 0x10) {
    if (mt->mt_sect.ps_cc != -1 && mt->mt_sect.ps_cc != (cc & 0x0f)) {
      uint16_t pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
      tvhdebug(mt->mt_name, "%s: PID %04X CC error %d != %d",
               logprefix, pid, cc & 0x0f, mt->mt_sect.ps_cc);
      mt->mt_sect.ps_lock = 0;
    }
    mt->mt_sect.ps_cc = (cc + 1) & 0x0f;
  }

  if(off >= 188) {
    mt->mt_sect.ps_lock = 0;
    return;
  }

  if(pusi) {
    int len = tsb[off++];
    if(len > 0) {
      if(len > 188 - off) {
        mt->mt_sect.ps_lock = 0;
        return;
      }
      mpegts_psi_section_reassemble0(mt, logprefix, tsb + off, len, 0, crc, cb, opaque);
      off += len;
    }
  }

  while(off < 188) {
    r = mpegts_psi_section_reassemble0(mt, logprefix, tsb + off, 188 - off, pusi, crc,
        cb, opaque);
    if(r < 0) {
      mt->mt_sect.ps_lock = 0;
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
  ( mpegts_psi_table_state_t *a, mpegts_psi_table_state_t *b )
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
  ( mpegts_psi_table_t *mt, mpegts_psi_table_state_t *st, int last )
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

static mpegts_psi_table_state_t *
mpegts_table_state_find
  ( mpegts_psi_table_t *mt, int tableid, uint64_t extraid, int last )
{
  mpegts_psi_table_state_t *st;

  /* Find state */
  SKEL_ALLOC(mpegts_psi_table_state_skel);
  mpegts_psi_table_state_skel->tableid = tableid;
  mpegts_psi_table_state_skel->extraid = extraid;
  st = RB_INSERT_SORTED(&mt->mt_state, mpegts_psi_table_state_skel, link, sect_cmp);
  if (!st) {
    st   = mpegts_psi_table_state_skel;
    SKEL_USED(mpegts_psi_table_state_skel);
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
  (mpegts_psi_table_t *mt)
{
  if (mt->mt_incomplete || !mt->mt_complete) {
    int total = 0;
    mpegts_psi_table_state_t *st;
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
  (mpegts_psi_table_t *mt, mpegts_psi_table_state_t *st, int sect)
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
  (mpegts_psi_table_t *mt, const uint8_t *ptr, int len,
   int tableid, uint64_t extraid, int minlen,
   mpegts_psi_table_state_t **ret, int *sect, int *last, int *ver)
{
  mpegts_psi_table_state_t *st;
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

#if 0 // FIXME, cannot be enabled in this form
    /* Ignore previous version */
    /* This check is for the broken PMT tables where:
     * last 0 version 21 = PCR + Audio PID 0x0044
     * last 0 version 22 = Audio PID 0x0044, PCR + Video PID 0x0045
     */
    if (*last == 0 && st->version - 1 == *ver)
      return -1;
#endif

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
dvb_table_reset(mpegts_psi_table_t *mt)
{
  mpegts_psi_table_state_t *st;

  tvhtrace(mt->mt_name, "pid %02X complete reset", mt->mt_pid);
  mt->mt_incomplete = 0;
  mt->mt_complete   = 0;
  while ((st = RB_FIRST(&mt->mt_state)) != NULL) {
    RB_REMOVE(&mt->mt_state, st, link);
    free(st);
  }
}

void
dvb_table_release(mpegts_psi_table_t *mt)
{
  mpegts_psi_table_state_t *st;

  while ((st = RB_FIRST(&mt->mt_state))) {
    RB_REMOVE(&mt->mt_state, st, link);
    free(st);
  }
}

/*
 *
 */

void dvb_table_parse_init
  ( mpegts_psi_table_t *mt, const char *name, int pid, void *opaque )
{
  memset(mt, 0, sizeof(*mt));
  mt->mt_name = strdup(name);
  mt->mt_opaque = opaque;
  mt->mt_pid = pid;
  mt->mt_sect.ps_cc = -1;
}

void dvb_table_parse_done( mpegts_psi_table_t *mt )
{
  free(mt->mt_name);
}

struct psi_parse {
  mpegts_psi_table_t *mt;
  mpegts_psi_parse_callback_t cb;
  int full;
};

static void
dvb_table_parse_cb( const uint8_t *sec, size_t len, void *opaque )
{
  struct psi_parse *parse = opaque;

  parse->cb(parse->mt, sec + parse->full, len - parse->full);
}

void dvb_table_parse
  (mpegts_psi_table_t *mt, const char *logprefix,
   const uint8_t *tsb, int len,
   int crc, int full, mpegts_psi_parse_callback_t cb)
{
  const uint8_t *end;
  struct psi_parse parse;

  parse.mt   = mt;
  parse.cb   = cb;
  parse.full = full ? 3 : 0;

  for (end = tsb + len; tsb < end; tsb += 188)
    mpegts_psi_section_reassemble(mt, logprefix, tsb, crc,
                                  dvb_table_parse_cb, &parse);
}

int dvb_table_append_crc32(uint8_t *dst, int off, int maxlen)
{
  if (off + 4 > maxlen)
    return -1;

  uint32_t crc = tvh_crc32(dst, off, 0xffffffff);
  dst[off++] = crc >> 24;
  dst[off++] = crc >> 16;
  dst[off++] = crc >> 8;
  dst[off++] = crc;
  return off;
}

int dvb_table_remux
  (mpegts_psi_table_t *mt, const uint8_t *buf, int len, uint8_t **out)
{
  uint8_t *obuf, pusi;
  int ol = 0, l, m;

  /* try to guess the output size here */
  obuf = malloc(((len + 183 + 1 /* pusi */) / 184) * 188);

  pusi = 0x40;
  while (len > 0) {
    obuf[ol++] = 0x47;
    obuf[ol++] = pusi | ((mt->mt_pid >> 8) & 0x1f);
    obuf[ol++] = mt->mt_pid & 0xff;
    obuf[ol++] = 0x10 | ((mt->mt_sect.ps_cco++) & 0x0f);
    if (pusi) {
      obuf[ol++] = 0;
      m = 183;
      pusi = 0;
    } else {
      m = 184;
    }
    l = MIN(m, len);
    memcpy(obuf + ol, buf, l);
    if (l < m)
      memset(obuf + ol + l, 0xff, m - l);
    ol  += m;
    len -= l;
    buf += l;
  }

  if (ol <= 0) {
    free(obuf);
    return 0;
  }
  *out = obuf;
  return ol;
}
