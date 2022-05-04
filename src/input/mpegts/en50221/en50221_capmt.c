/*
 *  Tvheadend - CI CAM (EN50221) generic interface
 *  Copyright (C) 2017 Jaroslav Kysela
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
#include "en50221_capmt.h"
#include "esstream.h"
#include "input.h"

#define EN50221_CAPMT_CMD_OK       1
#define EN50221_CAPMT_CMD_OK_MMI   2
#define EN50221_CAPMT_CMD_QUERY    3
#define EN50221_CAPMT_CMD_NOTSEL   4

#define EN50221_CAPMT_LM_MORE      0
#define EN50221_CAPMT_LM_FIRST     1
#define EN50221_CAPMT_LM_LAST      2
#define EN50221_CAPMT_LM_ONLY      3
#define EN50221_CAPMT_LM_ADD       4
#define EN50221_CAPMT_LM_UPDATE    5

/*
 *
 */

static inline uint16_t
extract_2byte(const uint8_t *ptr)
{
  return (((uint16_t)ptr[0]) << 8) | ptr[1];
}

static inline uint16_t
extract_pid(const uint8_t *ptr)
{
  return ((ptr[0] & 0x1f) << 8) | ptr[1];
}

static inline uint16_t
extract_len12(const uint8_t *ptr)
{
  return ((ptr[0] & 0xf) << 8) | ptr[1];
}

static inline void
put_2byte(uint8_t *dst, uint16_t val)
{
  dst[0] = val >> 8;
  dst[1] = val;
}

static inline void
put_len12(uint8_t *dst, uint16_t val)
{
  dst[0] &= 0xf0;
  dst[0] |= (val >> 8) & 0x0f;
  dst[1] = val;
}

/*
 *
 */

static int en50221_capmt_check_pid
  (mpegts_service_t *s, uint16_t pid )
{
  elementary_stream_t *st;

  TAILQ_FOREACH(st, &s->s_components.set_filter, es_filter_link)
    if (st->es_pid == pid) return 1;
  return 0;
}

static int en50221_capmt_check_caid
  (mpegts_service_t *s, uint16_t pid, uint16_t caid,
   const uint16_t *caids, int caids_count)
{
  elementary_stream_t *st;
  caid_t *c;

  for (; caids_count > 0; caids++, caids_count--)
    if (caid == *caids)
      break;

  if (caids_count == 0)
    return 0;

  TAILQ_FOREACH(st, &s->s_components.set_filter, es_filter_link) {
    if (st->es_type != SCT_CA) continue;
    LIST_FOREACH(c, &st->es_caids, link) {
      if (!c->use) continue;
      if (c->caid == caid && c->pid == pid) return 1;
    }
  }
  return 0;
}

/*
 *
 */

int en50221_capmt_build
  (mpegts_service_t *s, int bcmd, uint16_t svcid,
   const uint16_t *caids, int caids_count,
   const uint8_t *pmt, size_t pmtlen,
   uint8_t **capmt, size_t *capmtlen)
{
  uint8_t *d, *x, *y, dtag, dlen, cmd_id;
  const uint8_t *p;
  uint16_t l, caid, pid;
  size_t tl;
  int first = 0;

  if (pmtlen < 9)
    return -EINVAL;
  d = malloc(pmtlen * 2);
  if (d == NULL)
    return -ENOMEM;

  cmd_id = EN50221_CAPMT_CMD_OK;
  switch (bcmd) {
  case EN50221_CAPMT_BUILD_ONLY:   d[0] = 3; break;
  case EN50221_CAPMT_BUILD_ADD:    d[0] = 4; break;
  case EN50221_CAPMT_BUILD_DELETE:
    cmd_id = EN50221_CAPMT_CMD_NOTSEL;
    /* fallthru */
  case EN50221_CAPMT_BUILD_UPDATE: d[0] = 5; break;
  default:
    goto reterr;
  }

  put_2byte(d + 1, svcid);
  d[3] = pmt[2]; /* version + current_next_indicator */
  d[4] = 0xf0;

  /* common descriptors */
  x = d + 6;
  l = extract_len12(pmt + 7);
  p  = pmt + 9;
  tl = pmtlen - 9 - l;
  first = 1;
  if (9 + l > pmtlen)
    goto reterr;
  while (l > 1) {
    dtag = p[0];
    dlen = p[1];
    if (dtag == DVB_DESC_CA && dlen >= 4) {
      caid = extract_2byte(p + 2);
      pid  = extract_pid(p + 4);
      if (en50221_capmt_check_caid(s, pid, caid, caids, caids_count)) {
        if (first) {
          *x++ = cmd_id;
          first = 0;
        }
        memcpy(x, p, dlen + 2);
        x += dlen + 2;
      }
    }
    p  += 2 + dlen;
    l  -= 2 + dlen;
  }
  if (l)
    goto reterr;
  d[4] = 0xf0;
  put_len12(d + 4, x - d - 6); /* update length */

  while (tl >= 5) {
    l = extract_len12(p + 3);
    if (l + 5 > tl)
      goto reterr;
    pid = extract_pid(p + 1);
    p  += 5;
    tl -= 5;
    if (en50221_capmt_check_pid(s, pid)) {
      memcpy(y = x, p - 5, 3); /* stream type, PID */
      x += 5;
      first = 1;
      while (l > 1) {
        dtag = p[0];
        dlen = p[1];
        if (dtag == DVB_DESC_CA && dlen >= 4) {
          caid = extract_2byte(p + 2);
          pid  = extract_pid(p + 4);
          if (en50221_capmt_check_caid(s, pid, caid, caids, caids_count)) {
            if (first) {
              *x++ = cmd_id;
              first = 0;
            }
            memcpy(x, p, dlen + 2);
            x += dlen + 2;
          }
        }
        p  += 2 + dlen;
        l  -= 2 + dlen;
        tl -= 2 + dlen;
      }
      if (l)
        goto reterr;
      y[3] = 0xf0;
      put_len12(y + 3, x - y - 5);
    } else {
      p  += l;
      tl -= l;
    }
  }

  if (tl == 0) {
    *capmt = d;
    *capmtlen = x - d;
    return 0;
  }

reterr:
  free(d);
  return -EINVAL;
}

/*
 * rewrite capmt cmd_id - set it to 'query'
 */

int en50221_capmt_build_query
  (const uint8_t *capmt, size_t capmtlen,
   uint8_t **dst, size_t *dstlen)
{
  uint8_t *d, *x;
  uint16_t l;
  size_t tl;
  const uint8_t *p, *n;

  if (capmtlen < 6)
    return -EINVAL;
  l = extract_len12(capmt + 4);
  if (l + 6 > capmtlen)
    return -EINVAL;

  d = malloc(capmtlen);
  memcpy(d, capmt, capmtlen);
  
  p  = capmt + 6;
  n  = p + l;
  tl = capmtlen - l - 6;

  if (l > 0) {
    x = d + (p - capmt);
    x[0] = EN50221_CAPMT_CMD_QUERY;
  }

  while (tl >= 5) {
    l = extract_len12(n + 3);
    if (l + 5 > tl)
      goto reterr;
    if (l > 0) {
      if (l < 7)
        goto reterr;
      x = d + (n - capmt);
      x[5] = EN50221_CAPMT_CMD_QUERY;
    }
    n  += 5 + l;
    tl -= 5 + l;
  }

  if (tl == 0) {
    *dst = d;
    *dstlen = capmtlen;
    return 0;
  }

reterr:
  free(d);
  return -EINVAL;
}

/*
 *
 */

void en50221_capmt_dump
  (int subsys, const char *prefix, const uint8_t *capmt, size_t capmtlen)
{
  uint16_t l, caid, pid;
  uint8_t dtag, dlen, stype;
  const uint8_t *p;
  char hbuf[257];

  if (capmtlen < 6) {
    tvhtrace(subsys, "%s: CAPMT short length %zd", prefix, capmtlen);
    return;
  }
  tvhtrace(subsys, "%s: CAPMT list_management %02x sid %04x ver %02x",
                   prefix, capmt[0], ((int)capmt[1]) << 8 | capmt[2], capmt[3]);
  l = extract_len12(capmt + 4);
  p = capmt + 6;
  capmt = p + l;
  capmtlen -= l + 6;
  if (l > 0) {
    tvhtrace(subsys, "%s: CAPMT cmd_id %02x", prefix, p[0]);
    p++;
    l--;
    while (l > 0) {
      if (l < 6) {
        tvhtrace(subsys, "%s:   CAPMT wrong CA descriptor length %d", prefix, l);
        break;
      }
      dtag = p[0];
      dlen = p[1];
      if (dtag != DVB_DESC_CA) {
        tvhtrace(subsys, "%s:   CAPMT ES wrong dtag %02x", prefix, dtag);
      } else if (dlen < 4) {
        tvhtrace(subsys, "%s:   CAPMT wrong CA descriptor length %d (2)", prefix, l);
      } else {
        caid = extract_2byte(p + 2);
        pid  = extract_pid(p + 4);
        tvhtrace(subsys, "%s:   CAPMT CA descriptor caid %04X pid %04x length %d (%s)",
                         prefix, caid, pid, dlen, bin2hex(hbuf, sizeof(hbuf), p + 6, dlen - 4));
      }
      p += dlen + 2;
      l -= dlen + 2;
    }
  }
  while (capmtlen >= 5) {
    stype = capmt[0];
    pid   = extract_pid(capmt + 1);
    tvhtrace(subsys, "%s: CAPMT ES stype %02x pid %04x", prefix, stype, pid);
    l     = extract_len12(capmt + 3);
    p     = capmt + 5;
    capmt += l + 5;
    capmtlen -= l + 5;
    if (l > 0) {
      tvhtrace(subsys, "%s:   CAPMT ES cmd_id %02x", prefix, p[0]);
      p++;
      l--;
      while (l > 2) {
        dtag = p[0];
        dlen = p[1];
        if (dlen + 2 > l) {
          tvhtrace(subsys, "%s:     CAPMT ES wrong tag length (%d)", prefix, dlen);
        } else if (dtag != DVB_DESC_CA) {
          tvhtrace(subsys, "%s:     CAPMT ES wrong dtag %02x", prefix, dtag);
        } else if (dlen < 4) {
          tvhtrace(subsys, "%s:     CAPMT ES wrong CA descriptor length %d (2)", prefix, l);
        } else {
          caid = extract_2byte(p + 2);
          pid  = extract_pid(p + 4);
          tvhtrace(subsys, "%s:     CAPMT ES CA descriptor caid %04X pid %04x length %d (%s)",
                           prefix, caid, pid, dlen, bin2hex(hbuf, sizeof(hbuf), p + 6, dlen - 4));
        }
        p += dlen + 2;
        l -= dlen + 2;
      }
      if (l > 0)
        tvhtrace(subsys, "%s:   CAPMT ES wrong length (%d)", prefix, l);
    }
  }
  if (capmtlen)
    tvhtrace(subsys, "%s: CAPMT wrong main length (%zd)", prefix, capmtlen);
}
