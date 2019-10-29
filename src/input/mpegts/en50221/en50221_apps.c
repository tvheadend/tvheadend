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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tvhlog.h"
#include "tvh_string.h"
#include "en50221.h"
#include "input/mpegts/dvb.h"
#include "descrambler/caid.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define BYTE4BE(w) \
  (((w) >> 24) & 0xff), (((w) >> 16) & 0xff), (((w) >> 8) & 0xff), ((w) & 0xff)

/*
 *
 */

static inline uint8_t int2bcd(uint32_t v)
{
  return ((v / 10) << 4) + (v % 10);
}

static inline uint32_t getatag(const uint8_t *p, size_t l)
{
  if (l < 3)
    return CICAM_AOT_NONE + l;
  return (((uint32_t)p[0]) << 16) |
         (((uint32_t)p[1]) <<  8) |
                     p[2];
}

/*
 * Resource manager
 */

typedef struct en50221_app_resman {
  en50221_app_t;
  uint8_t cia_profile_sent;
} en50221_app_resman_t;

static int
en50221_app_resman_open(void *self)
{
  en50221_app_t *app = self;
  return en50221_app_pdu_send(app, CICAM_AOT_PROFILE_ENQ, NULL, 0, 1);
}

static int
en50221_app_resman_handle
  (void *self, uint8_t status, uint32_t atag, const uint8_t *data, size_t datalen)
{
  en50221_app_resman_t *app = self;
  static const uint8_t resources[] = {
    BYTE4BE(CICAM_RI_RESOURCE_MANAGER),
    BYTE4BE(CICAM_RI_MMI),
    BYTE4BE(CICAM_RI_CONDITIONAL_ACCESS_SUPPORT),
    BYTE4BE(CICAM_RI_DATE_TIME),
    BYTE4BE(CICAM_RI_APPLICATION_INFORMATION),
    BYTE4BE(CICAM_RI_APPLICATION_INFORMATION+1),
    BYTE4BE(CICAM_RI_APPLICATION_INFORMATION+2),
    BYTE4BE(CICAM_RI_APPLICATION_INFORMATION+4),
  };
  if (atag == CICAM_AOT_PROFILE_ENQ) {
    tvhtrace(LS_EN50221, "%s: profile enq reply sent", app->cia_name);
    return en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_PROFILE,
                                resources, sizeof(resources), 0);
  } else if (atag == CICAM_AOT_PROFILE) {
    if (!app->cia_profile_sent) {
      app->cia_profile_sent = 1;
      tvhtrace(LS_EN50221, "%s: profile change sent", app->cia_name);
      return en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_PROFILE_CHANGE,
                                  NULL, 0, 0);
    }
  } else {
    tvherror(LS_EN50221, "%s: unknown atag for resman 0x%06x", app->cia_name, atag);
  }
  return 0;
}

static en50221_app_prop_t en50221_app_resman = {
  .ciap_name = "resman",
  .ciap_resource_id = CICAM_RI_RESOURCE_MANAGER,
  .ciap_struct_size = sizeof(en50221_app_resman_t),
  .ciap_open = en50221_app_resman_open,
  .ciap_handle = en50221_app_resman_handle,
};

/*
 * Conditional Access
 */

#define CICAM_CAEI_POSSIBLE                  0x01
#define CICAM_CAEI_POSSIBLE_COND_PURCHASE    0x02
#define CICAM_CAEI_POSSIBLE_COND_TECHNICAL   0x03
#define CICAM_CAEI_NOT_POSSIBLE_ENTITLEMENT  0x71
#define CICAM_CAEI_NOT_POSSIBLE_TECHNICAL    0x73

#define CICAM_CA_ENABLE_FLAG                 0x80

#define CICAM_CA_ENABLE(x) \
  (((x) & CICAM_CA_ENABLE_FLAG) ? (x) & ~CICAM_CA_ENABLE_FLAG : 0)

typedef struct en50221_app_ca {
  en50221_app_t;
  uint16_t cia_caids[64];
  uint8_t cia_replies_to_query;
  uint8_t cia_ca_possible;
} en50221_app_ca_t;

static void
en50221_app_ca_destroy
  (void *self)
{
  en50221_app_ca_t *app = self;
  CICAM_CALL_APP_CB(app, cisw_ca_close);
}

static int
en50221_app_ca_open(void *self)
{
  en50221_app_t *app = self;
  return en50221_app_pdu_send(app, CICAM_AOT_CA_INFO_ENQ, NULL, 0, 1);
}

static int
en50221_app_ca_handle
  (void *self, uint8_t status, uint32_t atag, const uint8_t *data, size_t datalen)
{
  en50221_app_ca_t *app = self;
  const uint8_t *p;
  char buf[256];
  int count, i, j;
  uint16_t caid, u16;
  size_t c;

  if (atag == CICAM_AOT_CA_INFO) {
    count = 0;
    for (; count < ARRAY_SIZE(app->cia_caids) - 1 && datalen > 1;
         data += 2, datalen -= 2, count++)
      app->cia_caids[count] = (data[0] << 8) | data[1];
    if (datalen) {
      app->cia_caids[0] = 0;
      tvherror(LS_EN50221, "wrong length for conditional access info");
      return -EINVAL;
    }
    app->cia_caids[count] = 0;
    buf[0] = buf[1] = '\0';
    for (i = 0; i < count; ) {
      for (j = 0, c = 0; j < 4 && i < count; i++, j++) {
        caid = app->cia_caids[i];
        tvh_strlcatf(buf, sizeof(buf), c, " %04X (%s)", caid, caid2name(caid));
      }
      tvhinfo(LS_EN50221, "%s: CAM supported CAIDs: %s", app->cia_name, buf + 1);
    }
    app->cia_slot->cil_caid_list = count > 0;
    CICAM_CALL_APP_CB(app, cisw_caids, app->cia_caids, count);
  } else if (atag == CICAM_AOT_CA_UPDATE) {
    /* nothing */
  } else if (atag == CICAM_AOT_CA_PMT_REPLY) {
    c = 0;
    app->cia_ca_possible = 0;
    tvh_strlcatf(buf, sizeof(buf), c, "%s: PMT REPLY", app->cia_name);
    if (datalen > 1) {
      app->cia_replies_to_query = 1;
      u16 = (data[0] << 8) | data[1];
      i = datalen - 2;
      p = data + 2;
      tvh_strlcatf(buf, sizeof(buf), c, " %04X", u16);
      if (i > 0) {
        tvh_strlcatf(buf, sizeof(buf), c, " %02X", *p);
        p++;
        if (--i > 0) {
          if ((i % 3) == 0 && i > 1) {
            /* The EN50221 standard defines that the next byte is supposed
               to be the CA_enable value at programme level. However, there are
               CAMs (for instance the AlphaCrypt with firmware <= 3.05) that
               insert a two byte length field here.
               This is a workaround to skip this length field: */
            u16 = (p[0] << 8) | p[1];
            if (u16 == i - 2) {
              p += 2;
              i -= 2;
            }
          }
          tvh_strlcatf(buf, sizeof(buf), c, " %02X", *p);
          p++; i--;
          app->cia_ca_possible = CICAM_CA_ENABLE(*p) == CICAM_CAEI_POSSIBLE;
          while (i > 2) {
            u16 = (p[0] << 8) | p[1];
            if (CICAM_CA_ENABLE(p[2]) != CICAM_CAEI_POSSIBLE)
              app->cia_ca_possible = 1;
            tvh_strlcatf(buf, sizeof(buf), c, " %d=%02X", u16, p[2]);
            i -= 3;
          }
        }
      }
    }
  } else {
    tvherror(LS_EN50221, "unknown atag for conditional access 0x%06x", atag);
  }
  return 0;
}

int en50221_send_capmt
  (en50221_slot_t *slot, const uint8_t *capmt, uint8_t capmtlen)
{
  en50221_app_t *app;
  app = en50221_slot_find_application(slot, CICAM_RI_CONDITIONAL_ACCESS_SUPPORT, ~0);
  if (app)
    return en50221_app_pdu_send(app, CICAM_AOT_CA_PMT, capmt, capmtlen, 0);
  return -ENXIO;
}

static en50221_app_prop_t en50221_app_ca = {
  .ciap_name = "ca",
  .ciap_resource_id = CICAM_RI_CONDITIONAL_ACCESS_SUPPORT,
  .ciap_struct_size = sizeof(en50221_app_ca_t),
  .ciap_destroy = en50221_app_ca_destroy,
  .ciap_open = en50221_app_ca_open,
  .ciap_handle = en50221_app_ca_handle,
};

/*
 * DateTime
 */

typedef struct en50221_app_datetime {
  en50221_app_t;
  int64_t cia_update_interval;
  int64_t cia_last_clock;
} en50221_app_datetime_t;

static int
en50221_app_datetime_send(en50221_app_datetime_t *app, int64_t clock)
{
  time_t t = time(NULL);
  struct tm tm_gmt;
  struct tm tm_loc;
  uint8_t buf[7];
  int r = 0;

  if (gmtime_r(&t, &tm_gmt) && localtime_r(&t, &tm_loc)) {
    int GMTOFF = tm_loc.tm_gmtoff / 60;
    int Y      = tm_gmt.tm_year;
    int M      = tm_gmt.tm_mon + 1;
    int D      = tm_gmt.tm_mday;
    int L      = (M == 1 || M == 2) ? 1 : 0;
    int MJD    = 14956 + D + (int)((Y - L) * 365.25)
                       + (int)((M + 1 + L * 12) * 30.6001);
    buf[0] = MJD >> 8;
    buf[1] = MJD & 0xff;
    buf[2] = int2bcd(tm_gmt.tm_hour);
    buf[3] = int2bcd(tm_gmt.tm_min);
    buf[4] = int2bcd(tm_gmt.tm_sec);
    buf[5] = GMTOFF >> 8;
    buf[6] = GMTOFF & 0xff;

    r = en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_DATE_TIME, buf, 7, 0);
    if (r >= 0) {
      tvhtrace(LS_EN50221, "%s: updated datetime was sent", app->cia_name);
      app->cia_last_clock = clock;
    }
  }
  return r;
}

static int
en50221_app_datetime_handle
  (void *self, uint8_t status, uint32_t atag, const uint8_t *data, size_t datalen)
{
  int interval;
  en50221_app_datetime_t *app = self;
  if (atag == CICAM_AOT_DATE_TIME_ENQ) {
    interval = 0;
    if (datalen == 1)
      interval = data[0];
    app->cia_update_interval = sec2mono(interval);
    tvhtrace(LS_EN50221, "%s: CAM requested datetime with interval %d", app->cia_name, interval);
    en50221_app_datetime_send(app, mclk());
  } else {
    tvherror(LS_EN50221, "%s: unknown atag for datetime 0x%06x", app->cia_name, atag);
  }
  return 0;
}

static int
en50221_app_datetime_monitor(void *self, int64_t clock)
{
  en50221_app_datetime_t *app = self;
  if (app->cia_update_interval > 0 &&
      app->cia_last_clock + app->cia_update_interval < clock)
    return en50221_app_datetime_send(app, clock);
  return 0;
}

static en50221_app_prop_t en50221_app_datetime = {
  .ciap_name = "datetime",
  .ciap_resource_id = CICAM_RI_DATE_TIME,
  .ciap_struct_size = sizeof(en50221_app_datetime_t),
  .ciap_handle = en50221_app_datetime_handle,
  .ciap_monitor = en50221_app_datetime_monitor,
};

/*
 * Application information
 */

typedef struct en50221_app_appinfo {
  en50221_app_t;
  uint8_t cia_info_version;
  uint8_t cia_pcmcia_data_rate;
} en50221_app_appinfo_t;

static int
en50221_app_appinfo_open(void *self)
{
  en50221_app_t *app = self;
  return en50221_app_pdu_send(app, CICAM_AOT_APPLICATION_INFO_ENQ, NULL, 0, 1);
}

static int
en50221_app_appinfo_handle
  (void *self, uint8_t status, uint32_t atag, const uint8_t *data, size_t datalen)
{
  en50221_app_appinfo_t *app = self;
  const uint8_t *p;
  char *s;
  size_t l;
  uint16_t type, manufacturer, code;
  uint8_t rate;
  int r;

  if (atag >= CICAM_AOT_APPLICATION_INFO &&
      atag <= CICAM_AOT_APPLICATION_INFO+2) {
    if (datalen < 6) {
      tvherror(LS_EN50221, "%s: unknown length %zd for appinfo 0x%06x", app->cia_name, datalen, atag);
      return -ENXIO;
    }
    type = data[0];
    manufacturer = (data[1] << 8) | data[2];
    code = (data[3] << 8) | data[4];
    r = en50221_extract_len(data + 5, datalen - 5, &p, &l, app->cia_name, "appinfo");
    if (r < 0)
      return -ENXIO;
    s = alloca(l * 2 + 1);
    if (dvb_get_string(s, l * 2 + 1, p, l, NULL, NULL) < 0)
      return -ENXIO;
    tvhinfo(LS_EN50221, "%s: CAM INFO: %s, %02X, %04X, %04X", app->cia_name, s, type, manufacturer, code);
    app->cia_info_version = atag & 0x3f;
    CICAM_CALL_APP_CB(app, cisw_appinfo, atag & 0x3f, s, type, manufacturer, code);
    if (app->cia_info_version >= 3) /* at least CI+ v1.3 */
      if (CICAM_CALL_APP_CB(app, cisw_pcmcia_data_rate, &rate) >= 0)
        return en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_PCMCIA_DATA_RATE, &rate, 1, 0);
  } else {
    tvherror(LS_EN50221, "%s: unknown atag for appinfo 0x%06x", app->cia_name, atag);
  }
  return 0;
}

static en50221_app_prop_t en50221_app_appinfo1 = {
  .ciap_name = "appinfo1", /* EN50221 */
  .ciap_resource_id = CICAM_RI_APPLICATION_INFORMATION,
  .ciap_struct_size = sizeof(en50221_app_appinfo_t),
  .ciap_open = en50221_app_appinfo_open,
  .ciap_handle = en50221_app_appinfo_handle,
};

static en50221_app_prop_t en50221_app_appinfo2 = {
  .ciap_name = "appinfo2", /* TS101699 */
  .ciap_resource_id = CICAM_RI_APPLICATION_INFORMATION+1,
  .ciap_struct_size = sizeof(en50221_app_appinfo_t),
  .ciap_open = en50221_app_appinfo_open,
  .ciap_handle = en50221_app_appinfo_handle,
};

static en50221_app_prop_t en50221_app_appinfo3 = {
  .ciap_name = "appinfo3", /* CIPLUS13 */
  .ciap_resource_id = CICAM_RI_APPLICATION_INFORMATION+2,
  .ciap_struct_size = sizeof(en50221_app_appinfo_t),
  .ciap_open = en50221_app_appinfo_open,
  .ciap_handle = en50221_app_appinfo_handle,
};

static en50221_app_prop_t en50221_app_appinfo5 = {
  .ciap_name = "appinfo5", /* Bluebook A173-2 */
  .ciap_resource_id = CICAM_RI_APPLICATION_INFORMATION+4,
  .ciap_struct_size = sizeof(en50221_app_appinfo_t),
  .ciap_open = en50221_app_appinfo_open,
  .ciap_handle = en50221_app_appinfo_handle,
};

int en50221_pcmcia_data_rate(en50221_slot_t *slot, uint8_t rate)
{
  en50221_app_t *app;
  app = en50221_slot_find_application(slot, CICAM_RI_APPLICATION_INFORMATION, ~0x3f);
  if (app && ((en50221_app_appinfo_t *)app)->cia_info_version >= 3)
    return en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_PCMCIA_DATA_RATE, &rate, 1, 0);
  return 0;
}

/*
 * MMI
 */

/* Display Control Commands */
#define MMI_DCC_SET_MMI_MODE                              0x01
#define MMI_DCC_DISPLAY_CHARACTER_TABLE_LIST              0x02
#define MMI_DCC_INPUT_CHARACTER_TABLE_LIST                0x03
#define MMI_DCC_OVERLAY_GRAPHICS_CHARACTERISTICS          0x04
#define MMI_DCC_FULL_SCREEN_GRAPHICS_CHARACTERISTICS      0x05

/* MMI Modes */
#define MMI_MM_HIGH_LEVEL                                 0x01
#define MMI_MM_LOW_LEVEL_OVERLAY_GRAPHICS                 0x02
#define MMI_MM_LOW_LEVEL_FULL_SCREEN_GRAPHICS             0x03

/* Display Reply IDs */
#define MMI_DRI_MMI_MODE_ACK                              0x01
#define MMI_DRI_LIST_DISPLAY_CHARACTER_TABLES             0x02
#define MMI_DRI_LIST_INPUT_CHARACTER_TABLES               0x03
#define MMI_DRI_LIST_GRAPHIC_OVERLAY_CHARACTERISTICS      0x04
#define MMI_DRI_LIST_FULL_SCREEN_GRAPHIC_CHARACTERISTICS  0x05
#define MMI_DRI_UNKNOWN_DISPLAY_CONTROL_CMD               0xF0
#define MMI_DRI_UNKNOWN_MMI_MODE                          0xF1
#define MMI_DRI_UNKNOWN_CHARACTER_TABLE                   0xF2

/* Enquiry Flags */
#define MMI_EF_BLIND   0x01

/* Answer IDs */
#define MMI_AI_CANCEL  0x00
#define MMI_AI_ANSWER  0x01

typedef struct en50221_app_mi {
  en50221_app_t;
  htsmsg_t *cia_menu;
  htsmsg_t *cia_enquiry;
} en50221_app_mmi_t;

static void
en50221_app_mmi_destroy
  (void *self)
{
  en50221_app_mmi_t *app = self;
  htsmsg_destroy(app->cia_menu);
  app->cia_menu = NULL;
  htsmsg_destroy(app->cia_enquiry);
  app->cia_enquiry = NULL;
}

static int
getmenutext
  (en50221_app_mmi_t *app, const char *key, int list, const uint8_t **_p, size_t *_l)
{
  const uint8_t *p = *_p, *p2;
  size_t l = *_l, l2;
  uint32_t tag;
  char *txt;
  htsmsg_t *c;
  int r;

  if (l == 0)
    return 0;
  if (l < 3) {
    tvherror(LS_EN50221, "%s: unexpected tag length", app->cia_name);
    return 0;
  }
  tag = getatag(p, l);
  p += 3; l -= 3;
  if (tag != CICAM_AOT_TEXT_LAST) {
    tvherror(LS_EN50221, "%s: unexpected getmenutext tag %06X", app->cia_name, tag);
    return -ENXIO;
  }
  r = en50221_extract_len(p, l, &p2, &l2, app->cia_name, "menutext");
  if (r < 0)
    return r;
  if (l2 > 0) {
    txt = alloca(l2 * 2 + 1);
    if (dvb_get_string(txt, l2 * 2 + 1, p2, l2, NULL, NULL) == 0 && txt[0]) {
      if (list) {
        c = htsmsg_get_list(app->cia_menu, key);
        if (c == NULL) {
          c = htsmsg_create_list();
          c = htsmsg_add_msg(app->cia_menu, key, c);
        }
        tvhtrace(LS_EN50221, "%s:  add to %s '%s'", app->cia_name, key, txt);
        htsmsg_add_str(c, NULL, txt);
      } else {
        tvhtrace(LS_EN50221, "%s:  %s '%s'", app->cia_name, key, txt);
        htsmsg_add_str(app->cia_menu, key, txt);
      }
    }
  }
  *_p = p2 + l2;
  *_l = l - l2 - (p2 - p);
  return 0;
}

static int
en50221_app_mmi_handle
  (void *self, uint8_t status, uint32_t atag, const uint8_t *data, size_t datalen)
{
  en50221_app_mmi_t *app = self;
  const uint8_t *p;
  char *txt;
  size_t l;
  int r, id, delay;

  if (atag == CICAM_AOT_DISPLAY_CONTROL) {
    tvhtrace(LS_EN50221, "%s: display control", app->cia_name);
    if (datalen > 0) {
      if (data[0] == MMI_DCC_SET_MMI_MODE) {
        static uint8_t buf[] = { MMI_DRI_MMI_MODE_ACK, MMI_MM_HIGH_LEVEL };
        tvhtrace(LS_EN50221, "%s: display control sending reply", app->cia_name);
        return en50221_app_pdu_send((en50221_app_t *)app, CICAM_AOT_DISPLAY_REPLY,
                                    buf, ARRAY_SIZE(buf), 0);
      }
    }
  } else if (atag == CICAM_AOT_LIST_LAST || atag == CICAM_AOT_MENU_LAST) {
    tvhtrace(LS_EN50221, "%s: %s last", app->cia_name,
             atag == CICAM_AOT_LIST_LAST ? "list" : "menu");
    htsmsg_destroy(app->cia_menu);
    app->cia_menu = NULL;
    if (datalen > 0) {
      app->cia_menu = htsmsg_create_map();
      if (atag == CICAM_AOT_LIST_LAST)
        htsmsg_add_bool(app->cia_menu, "selectable", 1);
      p = data + 1; l = datalen - 1; /* ignore choice_nb */
      r = getmenutext(app, "title", 0, &p, &l);
      if (!r) r = getmenutext(app, "subtitle", 0, &p, &l);
      if (!r) r = getmenutext(app, "bottom", 0, &p, &l);
      while (!r && l > 0)
        r = getmenutext(app, "choices", 1, &p, &l);
      if (!r)
        CICAM_CALL_APP_CB(app, cisw_menu, app->cia_menu);
      return r;
    }
  } else if (atag == CICAM_AOT_ENQ) {
    tvhtrace(LS_EN50221, "%s: enquiry", app->cia_name);
    htsmsg_destroy(app->cia_enquiry);
    app->cia_enquiry = htsmsg_create_map();
    if (datalen > 1) {
      if (data[0] & MMI_EF_BLIND)
        htsmsg_add_bool(app->cia_enquiry, "blind", 1);
      if (data[1]) /* expected length */
        htsmsg_add_s64(app->cia_enquiry, "explen", data[1]);
      txt = alloca(datalen * 2 + 1);
      if (dvb_get_string(txt, datalen * 2 + 1, data + 2, datalen - 2,
                         NULL, NULL) > 0 && txt[0])
        htsmsg_add_str(app->cia_enquiry, "text", txt);
      tvhtrace(LS_EN50221, "%s: enquiry data %02x %02x '%s'",
                           app->cia_name, data[0], data[1], txt);
      CICAM_CALL_APP_CB(app, cisw_enquiry, app->cia_enquiry);
    }
  } else if (atag == CICAM_AOT_CLOSE_MMI) {
    id = -1;
    delay = 0;
    if (datalen > 0) {
      id = data[0];
      if (datalen > 1)
        delay = data[1];
    }
    tvhtrace(LS_EN50221, "%s: close mmi %d %d", app->cia_name, id, delay);
    CICAM_CALL_APP_CB(app, cisw_close, delay);
  } else {
    tvherror(LS_EN50221, "%s: unknown atag for mmi 0x%06x", app->cia_name, atag);
  }
  return 0;
}

static en50221_app_prop_t en50221_app_mmi = {
  .ciap_name = "mmi",
  .ciap_resource_id = CICAM_RI_MMI,
  .ciap_struct_size = sizeof(en50221_app_mmi_t),
  .ciap_destroy = en50221_app_mmi_destroy,
  .ciap_handle = en50221_app_mmi_handle,
};

static int en50221_mmi_basic_io
  (en50221_slot_t *slot, uint32_t ri, const uint8_t *data, size_t datalen)
{
  en50221_app_t *app;
  app = en50221_slot_find_application(slot, CICAM_RI_MMI, ~0);
  if (app)
    return en50221_app_pdu_send((en50221_app_t *)app, ri, data, datalen, 0);
  return 0;
}

int en50221_mmi_answer
  (en50221_slot_t *slot, const uint8_t *data, size_t datalen)
{
  return en50221_mmi_basic_io(slot, CICAM_AOT_ANSW, data, datalen);
}

int en50221_mmi_close(en50221_slot_t *slot)
{
  return en50221_mmi_basic_io(slot, CICAM_AOT_CLOSE_MMI, NULL, 0);
}

/*
 *
 */

void en50221_register_apps(void)
{
  en50221_register_app(&en50221_app_resman);
  en50221_register_app(&en50221_app_ca);
  en50221_register_app(&en50221_app_datetime);
  en50221_register_app(&en50221_app_appinfo1);
  en50221_register_app(&en50221_app_appinfo2);
  en50221_register_app(&en50221_app_appinfo3);
  en50221_register_app(&en50221_app_appinfo5);
  en50221_register_app(&en50221_app_mmi);
}
