/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015,2016,2017,2018 Jaroslav Kysela
 *
 * Tvheadend - MPEGTS debug output
 */

#include "input.h"

void
tsdebug_encode_keys
  ( uint8_t *dst, uint16_t sid, uint16_t pid,
    uint8_t keytype, uint8_t keylen, uint8_t *even, uint8_t *odd )
{
  uint32_t pos = 0, crc;

  memset(dst, 0xff, 188);
  dst[pos++] = 0x47; /* sync byte */
  dst[pos++] = 0x1f; /* PID MSB */
  dst[pos++] = 0xff; /* PID LSB */
  dst[pos++] = 0x00; /* CC */
  memcpy(dst + pos, "TVHeadendDescramblerKeys", 24);
  dst += 24;
  dst[pos++] = keytype;
  dst[pos++] = keylen;
  dst[pos++] = (sid >> 8) & 0xff;
  dst[pos++] = sid & 0xff;
  dst[pos++] = (pid >> 8) & 0xff;
  dst[pos++] = pid & 0xff;
  memcpy(dst + pos, even, keylen);
  memcpy(dst + pos + keylen, odd, keylen);
  pos += 2 * keylen;
  crc = tvh_crc32(dst, pos, 0x859aa5ba);
  dst[pos++] = (crc >> 24) & 0xff;
  dst[pos++] = (crc >> 16) & 0xff;
  dst[pos++] = (crc >> 8) & 0xff;
  dst[pos++] = crc & 0xff;
}

void
tsdebug_check_tspkt( mpegts_mux_t *mm, uint8_t *pkt, int len )
{
  void tsdebugcw_new_keys(service_t *t, int type, uint16_t pid, uint8_t *odd, uint8_t *even);
  uint32_t pos, type, keylen, sid, crc;
  uint16_t pid;
  mpegts_service_t *t;

  for ( ; len > 0; pkt += 188, len -= 188) {
    if (memcmp(pkt + 4, "TVHeadendDescramblerKeys", 24))
      continue;
    pos = 4 + 24;
    type = pkt[pos + 0];
    keylen = pkt[pos + 1];
    sid = (pkt[pos + 2] << 8) | pkt[pos + 3];
    pid = (pkt[pos + 4] << 8) | pkt[pos + 5];
    pos += 6 + 2 * keylen;
    if (pos > 184)
      return;
    crc = (pkt[pos + 0] << 24) | (pkt[pos + 1] << 16) |
          (pkt[pos + 2] << 8) | pkt[pos + 3];
    if (crc != tvh_crc32(pkt, pos, 0x859aa5ba))
      return;
    LIST_FOREACH(t, &mm->mm_services, s_dvb_mux_link)
      if (service_id16(t) == sid) break;
    if (!t)
      return;
    pos = 4 + 24 + 4;
    tvhdebug(LS_DESCRAMBLER, "Keys from MPEG-TS source (PID 0x1FFF)!");
    tsdebugcw_new_keys((service_t *)t, type, pid, pkt + pos, pkt + pos + keylen);
  }
}
