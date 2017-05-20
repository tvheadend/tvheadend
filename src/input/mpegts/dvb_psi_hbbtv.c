/*
 *  tvheadend, MPEG transport stream demuxer
 *  Copyright (C) 2007 Andreas Öman
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
#include "streaming.h"
#include "parsers.h"
#include "tsdemux.h"
#include "htsmsg.h"
#include "htsmsg_binary.h"

/**
 * Extract Hbbtv
 */
void
ts_recv_hbbtv_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  static const char *visibility_table[4] = {
    "none",
    "apps",
    "reserved",
    "all",
  };
  elementary_stream_t *st = (elementary_stream_t *)mt->mt_opaque;
  service_t *t = st->es_service;
  mpegts_psi_table_state_t *tst = NULL;
  int r, sect, last, ver, l, l2, l3, dllen, dlen, flags;
  uint8_t tableid = buf[0], dtag;
  const uint8_t *p, *dlptr, *dptr;
  uint16_t app_type, app_id, protocol_id;
  uint32_t org_id;
  char title[256], name[256], location[256], *str;
  htsmsg_t *map, *apps = NULL, *titles = NULL;
  void *bin;
  size_t binlen;

  if (tableid != 0x74 || len < 16)
    return;
  app_type = (buf[3] << 8) | buf[4];
  if (app_type & 1) /* testing */
    return;

  r = dvb_table_begin(mt, buf + 3, len - 3,
                      tableid, app_type, 5, &tst, &sect, &last, &ver);
  if (r != 1) return;

  p = buf;
  l = len;

  DVB_DESC_FOREACH(mt, p, l, 8, dlptr, dllen, dtag, dlen, dptr) {
    tvhtrace(mt->mt_subsys, "%s: common dtag %02X dlen %d", mt->mt_name, dtag, dlen);
  }}

  l2 = ((p[0] << 8) | p[1]) & 0xfff;
  p += 2;
  l += 2;
  while (l2 >= 9) {
    org_id = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    app_id = (p[4] << 8) | p[5];
    tvhtrace(mt->mt_subsys, "%s: app org %08X id %04X control %02X", mt->mt_name, org_id, app_id, p[6]);
    l2 -= 9;
    title[0] = name[0] = location[0] = '\0';
    titles = NULL;
    flags = 0;
    DVB_DESC_FOREACH(mt, p, l, 7, dlptr, dllen, dtag, dlen, dptr) {
      l2 -= dlen + 2;
      tvhtrace(mt->mt_subsys, "%s:   dtag %02X dlen %d", mt->mt_name, dtag, dlen);
      switch (dtag) {
      case DVB_DESC_APP:
        l3 = *dptr++; dlen--;
        if (l3 % 5) goto dvberr;
        while (l3 >= 5) {
          tvhtrace(mt->mt_subsys, "%s:     profile %04X %d.%d.%d", mt->mt_name, (dptr[0] << 8) | dptr[1], dptr[2], dptr[3], dptr[4]);
          dptr += 5;
          dlen -= 5;
          l3 -= 5;
        }
        if (dlen < 3) goto dvberr;
        flags = dptr[0];
        tvhtrace(mt->mt_subsys, "%s:     flags %02X prio %02X", mt->mt_name, dptr[0], dptr[1]);
        dptr += 2;
        dlen -= 2;
        while (dlen-- > 0) {
          tvhtrace(mt->mt_subsys, "%s:     transport protocol label %02X", mt->mt_name, dptr[0]);
          dptr++;
        }
        break;
      case DVB_DESC_APP_NAME:
        titles = htsmsg_create_list();
        while (dlen > 4) {
          r = dvb_get_string_with_len(title, sizeof(title), dptr + 3, dlen - 3, "UTF-8", NULL);
          if (r < 0) goto dvberr;
          tvhtrace(mt->mt_subsys, "%s:      lang '%c%c%c' name '%s'", mt->mt_name, dptr[0], dptr[1], dptr[2], title);
          map = htsmsg_create_map();
          htsmsg_add_str(map, "name", title);
          memcpy(title, dptr, 3);
          title[3] = '\0';
          htsmsg_add_str(map, "lang", title);
          htsmsg_add_msg(titles, NULL, map);
          dptr += r;
          dlen -= r;
        }
        break;
      case DVB_DESC_APP_TRANSPORT:
        if (dlen < 4) goto dvberr;
        protocol_id = (dptr[0] << 8) | dptr[1];
        tvhtrace(mt->mt_subsys, "%s:     protocol_id %04X transport protocol label %02X", mt->mt_name, protocol_id, dptr[2]);
        dptr += 3; dlen -= 3;
        if (protocol_id == 0x0003) {
          r = dvb_get_string_with_len(name, sizeof(name), dptr, dlen, "UTF-8", NULL);
          if (r < 0) goto dvberr;
          tvhtrace(mt->mt_subsys, "%s:     http '%s'", mt->mt_name, name);
        } else {
          while (dlen-- > 0) {
            tvhtrace(mt->mt_subsys, "%s:     selector %02X", mt->mt_name, dptr[0]);
            dptr++;
          }
        }
        break;
      case DVB_DESC_APP_SIMPLE_LOCATION:
        r = dvb_get_string(location, sizeof(location), dptr, dlen, "UTF-8", NULL);
        if (r < 0) goto dvberr;
        tvhtrace(mt->mt_subsys, "%s:     simple location '%s'", mt->mt_name, location);
        break;
      }
    }}
    if (titles && name[0] && location[0]) {
      map = htsmsg_create_map();
      htsmsg_add_msg(map, "title", titles);
      titles = NULL;
      str = malloc(strlen(name) + strlen(location) + 1);
      strcpy(str, name);
      strcat(str, location);
      htsmsg_add_str(map, "url", str);
      free(str);
      if (apps == NULL)
        apps = htsmsg_create_list();
      htsmsg_add_str(map, "visibility", visibility_table[(flags >> 5) & 3]);
      htsmsg_add_msg(apps, NULL, map);
    } else {
      htsmsg_destroy(titles);
    }
  }
  if (l2 != 0)
    goto dvberr;

  dvb_table_end(mt, tst, sect);

  if (t->s_hbbtv == NULL)
    t->s_hbbtv = htsmsg_create_map();
  if (apps) {
    snprintf(location, sizeof(location), "%d", sect);
    htsmsg_set_msg(t->s_hbbtv, location, apps);
    apps = NULL;
    service_request_save(t, 0);
  }

  if (streaming_pad_probe_type(&t->s_streaming_pad, SMT_PACKET)) {
    parse_mpeg_ts(t, st, buf, len, 1, 0);
    if (t->s_hbbtv) {
      if (!htsmsg_binary_serialize(t->s_hbbtv, &bin, &binlen, 128*1024)) {
        parse_mpeg_ts(t, st, bin, binlen, 1, 0);
        free(bin);
      }
    }
  }

dvberr:
  htsmsg_destroy(apps);
  htsmsg_destroy(titles);
  return;
}
