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
#include "tvhlog.h"
#include "queue.h"
#include "sbuf.h"
#include "en50221.h"

/*
 * Implementation note: For simplicity, the session is tied with
 * the application structure and the connection is tied with the
 * slot structure, because there is 1:1 mapping.
 */

static int  en50221_slot_create(en50221_transport_t* cit, uint8_t number, en50221_slot_t** cil);
static void en50221_slot_destroy(en50221_slot_t* cil);
static int  en50221_slot_connection_initiated(en50221_slot_t* cil);
static void en50221_slot_reset(en50221_slot_t* cil);
static int  en50221_slot_trigger_write_queue(en50221_slot_t* cil);
#if 0
static int en50221_slot_session_create
  (en50221_slot_t *cil, uint32_t resource_id);
#endif
static int  en50221_slot_spdu_recv(en50221_slot_t* cil, const uint8_t* data, size_t datalen);
static int  en50221_slot_monitor(en50221_slot_t* cil, int64_t clock);
static int  en50221_create_app(en50221_slot_t* cil,
     uint32_t                                  resource_id,
     uint16_t                                  session_id,
     en50221_app_t**                           cia);
static void en50221_app_destroy(en50221_app_t* cia);
static int
en50221_app_handle(en50221_app_t* cia, uint8_t status, const uint8_t* data, size_t datalen);
static int en50221_app_open(en50221_app_t* cia);
static int en50221_app_monitor(en50221_app_t* cia, int64_t clock);

/*
 * Misc
 */

static inline uint32_t get32(const uint8_t* p) {
  return (((uint32_t)p[0]) << 24) | (((uint32_t)p[1]) << 16) | (((uint32_t)p[2]) << 8) | p[3];
}

static inline uint32_t getatag(const uint8_t* p, size_t l) {
  if (l < 3)
    return CICAM_AOT_NONE + l;
  return (((uint32_t)p[0]) << 16) | (((uint32_t)p[1]) << 8) | p[2];
}

/*
 * Transport layer
 */

#define CICAM_TDPU_SIZE      4096
#define CICAM_TDPU_DATA_SIZE (CICAM_TDPU_SIZE - 7)

/* EN50221, Annex A.4.1.13 (pg70) */
#define CICAM_T_SB         0x80 /* SB                             h<--m */
#define CICAM_T_RCV        0x81 /* receive                        h-->m */
#define CICAM_T_CREATE_TC  0x82 /* create transport connection    h-->m */
#define CICAM_T_CTC_REPLY  0x83 /* ctc reply                      h<--m */
#define CICAM_T_DELETE_TC  0x84 /* delete tc                      h<->m */
#define CICAM_T_DTC_REPLY  0x85 /* delete tc reply                h<->m */
#define CICAM_T_REQUEST_TC 0x86 /* request transport connection   h<--m */
#define CICAM_T_NEW_TC     0x87 /* new tc (reply to request)      h-->m */
#define CICAM_T_TC_ERROR   0x88 /* error creating tc              h-->m */
#define CICAM_T_DATA_LAST  0xA0 /* data transfer                  h<->m */
#define CICAM_T_DATA_MORE  0xA1 /* data transfer                  h<->m */

/* Session tags */
#define CICAM_ST_SESSION_NUMBER          0x90
#define CICAM_ST_OPEN_SESSION_REQUEST    0x91
#define CICAM_ST_OPEN_SESSION_RESPONSE   0x92
#define CICAM_ST_CREATE_SESSION          0x93
#define CICAM_ST_CREATE_SESSION_RESPONSE 0x94
#define CICAM_ST_CLOSE_SESSION_REQUEST   0x95
#define CICAM_ST_CLOSE_SESSION_RESPONSE  0x96

/* Session state codes */
#define CICAM_SS_OK            0x00
#define CICAM_SS_NOT_ALLOCATED 0xF0

int en50221_create_transport(en50221_ops_t* ops,
    void*                                   ops_aux,
    int                                     slots,
    const char*                             name,
    en50221_transport_t**                   _cit) {
  en50221_transport_t* cit;

  cit = calloc(1, sizeof(*cit));
  if (cit == NULL)
    return -ENOMEM;
  cit->cit_ops        = ops;
  cit->cit_ops_aux    = ops_aux;
  cit->cit_name       = strdup(name);
  cit->cit_slot_count = slots;
  sbuf_init(&cit->cit_rmsg);
  *_cit = cit;
  return 0;
}

static void en50221_transport_destroy_slots(en50221_transport_t* cit) {
  en50221_slot_t* slot;

  while ((slot = LIST_FIRST(&cit->cit_slots)) != NULL)
    en50221_slot_destroy(slot);
}

void en50221_transport_destroy(en50221_transport_t* cit) {
  if (cit == NULL)
    return;
  en50221_transport_destroy_slots(cit);
  sbuf_free(&cit->cit_rmsg);
  free(cit->cit_name);
  free(cit);
}

static int en50221_transport_build_slots(en50221_transport_t* cit) {
  int i, r;
  for (i = 0; i < cit->cit_slot_count; i++) {
    r = en50221_slot_create(cit, i, NULL);
    if (r < 0)
      return r;
  }
  return 0;
}

int en50221_transport_reset(en50221_transport_t* cit) {
  int r;

  r = cit->cit_ops->cihw_reset(cit->cit_ops_aux);
  if (r == 0) {
    en50221_transport_destroy_slots(cit);
    r = en50221_transport_build_slots(cit);
    if (r < 0)
      return r;
    cit->cit_broken = 0;
  }
  return r;
}

static int en50221_transport_pdu_write(en50221_transport_t* cit,
    en50221_slot_t*                                         cil,
    uint8_t                                                 tcnum,
    uint8_t                                                 tag,
    const uint8_t*                                          data,
    size_t                                                  datalen) {
  uint8_t* buf = alloca(8 + datalen);
  size_t   hlen;

  buf[0] = tag;
  datalen++;
  if (datalen < 0x80) {
    buf[1] = datalen & 0x7f;
    hlen   = 2;
  } else if (datalen < 0x100) {
    buf[1] = 0x81;
    buf[2] = datalen & 0xff;
    hlen   = 3;
  } else if (datalen < 0x10000) {
    buf[1] = 0x82;
    buf[2] = datalen >> 8;
    buf[3] = datalen & 0xff;
    hlen   = 4;
  } else if (datalen < 0x1000000) {
    buf[1] = 0x83;
    buf[2] = datalen >> 16;
    buf[3] = (datalen >> 8) & 0xff;
    buf[4] = datalen & 0xff;
    hlen   = 5;
  } else {
    buf[1] = 0x84;
    buf[2] = datalen >> 24;
    buf[3] = (datalen >> 16) & 0xff;
    buf[4] = (datalen >> 8) & 0xff;
    buf[5] = datalen & 0xff;
    hlen   = 6;
  }
  buf[hlen++] = tcnum;
  if (datalen > 1)
    memcpy(buf + hlen, data, datalen - 1);
  return cit->cit_ops->cihw_pdu_write(cit->cit_ops_aux, cil, tcnum, buf, datalen + hlen - 1);
}

en50221_slot_t* en50221_transport_find_slot(en50221_transport_t* cit, uint8_t slotnum) {
  en50221_slot_t* slot;

  LIST_FOREACH (slot, &cit->cit_slots, cil_link)
    if (slot->cil_number == slotnum)
      return slot;
  return NULL;
}

static int en50221_transport_broken(en50221_transport_t* cit, const uint8_t* data, size_t datalen) {
  cit->cit_broken = 1;
  tvhlog_hexdump(LS_EN50221, data, datalen);
  return -ENXIO;
}

int en50221_transport_read(en50221_transport_t* cit,
    uint8_t                                     slotnum,
    uint8_t                                     tcnum,
    const uint8_t*                              data,
    size_t                                      datalen) {
  const uint8_t * p, *end;
  uint8_t         tag;
  en50221_slot_t* slot;
  size_t          l;
  int             r;

  if (cit->cit_broken)
    return -ENXIO;

  if (slotnum + 1 != tcnum) {
    tvherror(LS_EN50221,
        "%s: invalid slot number 0x%02x and "
        "transport connection id (0x%02x)",
        cit->cit_name,
        slotnum,
        tcnum);
    return en50221_transport_broken(cit, data, datalen);
  }
  slot = en50221_transport_find_slot(cit, slotnum);
  if (slot == NULL) {
    tvherror(LS_EN50221, "%s: invalid slot number 0x%02x", cit->cit_name, slotnum);
    return en50221_transport_broken(cit, data, datalen);
  }
  if (datalen > 0)
    slot->cil_monitor_read = mclk();

  p   = data;
  end = data + datalen;
  while (p + 3 <= end) {
    tag = *p++;
    r   = en50221_extract_len(p, end - p, &p, &l, cit->cit_name, "tpdu");
    if (r < 0)
      return en50221_transport_broken(cit, data, datalen);
    if (l == 0) {
      tvherror(LS_EN50221, "%s: invalid zero length", cit->cit_name);
      return en50221_transport_broken(cit, data, datalen);
    }
    if (end - p < l) {
      tvhtrace(LS_EN50221, "%s: invalid data received (%zd)", cit->cit_name, (size_t)(end - p));
      break;
    }
    switch (tag) {
      case CICAM_T_SB:
        if (slot->cil_closing) {
          r = en50221_slot_trigger_write_queue(slot);
          if (r < 0)
            return en50221_transport_broken(cit, data, datalen);
          break;
        }
        if (slot->cil_ready == 0) {
          tvhwarn(LS_EN50221, "%s: received sb but slot is not ready", cit->cit_name);
          break;
        }
        /* data are waiting? tell CAM that we can receive them or flush the write queue */
        if (l == 2) {
          if ((p[1] & 0x80) != 0) {
            r = en50221_transport_pdu_write(cit, slot, tcnum, CICAM_T_RCV, NULL, 0);
          } else {
            r = en50221_slot_trigger_write_queue(slot);
          }
          if (r < 0)
            return en50221_transport_broken(cit, data, datalen);
        }
        break;
      case CICAM_T_CTC_REPLY:
        if (l != 1) {
          tvherror(LS_EN50221, "%s: invalid length %zd for tag 0x%02x", cit->cit_name, l, tag);
          return en50221_transport_broken(cit, data, datalen);
        }
        slot->cil_ready = 1;
        if (slot->cil_initiate_connection)
          en50221_slot_connection_initiated(slot);
        break;
      case CICAM_T_DATA_MORE:
      case CICAM_T_DATA_LAST:
        if (slot->cil_ready == 0) {
          tvhwarn(LS_EN50221, "%s: received data but slot is not ready", cit->cit_name);
          break;
        }
        if (l > 1) {
          p++;
          l--; /* what's this byte? tcnum? */
          if (tag == CICAM_T_DATA_LAST) {
            if (cit->cit_rmsg.sb_ptr == 0) {
              r = en50221_slot_spdu_recv(slot, p, l);
            } else {
              sbuf_append(&cit->cit_rmsg, p, l);
              r = en50221_slot_spdu_recv(slot, cit->cit_rmsg.sb_data, cit->cit_rmsg.sb_ptr);
              sbuf_reset(&cit->cit_rmsg, CICAM_TDPU_SIZE);
            }
            if (r < 0)
              return en50221_transport_broken(cit, data, datalen);
          } else {
            sbuf_append(&cit->cit_rmsg, p, l);
          }
        }
        break;
      default:
        tvherror(LS_EN50221, "%s: unknown transport tag 0x%02x", cit->cit_name, tag);
        return en50221_transport_broken(cit, data, datalen);
    }
    p += l;
  }
  return 0;
}

int en50221_transport_monitor(en50221_transport_t* cit, int64_t clock) {
  en50221_slot_t* slot;
  int             r;

  if (LIST_EMPTY(&cit->cit_slots) && cit->cit_slot_count > 0) {
    r = en50221_transport_build_slots(cit);
    if (r < 0)
      return r;
  }
  LIST_FOREACH (slot, &cit->cit_slots, cil_link) {
    if (cit->cit_ops->cihw_cam_is_ready(cit->cit_ops_aux, slot) > 0) {
      en50221_slot_enable(slot);
      r = en50221_slot_monitor(slot, clock);
      if (r < 0)
        return r;
    } else {
      en50221_slot_disable(slot);
    }
  }
  return 0;
}

/*
 *  Slot/connection layer
 */
static int en50221_slot_create(en50221_transport_t* cit, uint8_t number, en50221_slot_t** cil) {
  en50221_slot_t* slot;
  char            buf[128];

  slot = calloc(1, sizeof(*slot));
  if (slot == NULL)
    return -ENOMEM;
  snprintf(buf, sizeof(buf), "%s-slot%d", cit->cit_name, number);
  slot->cil_name         = strdup(buf);
  slot->cil_transport    = cit;
  slot->cil_number       = number;
  slot->cil_monitor_read = mclk();
  TAILQ_INIT(&slot->cil_write_queue);
  LIST_INSERT_HEAD(&cit->cit_slots, slot, cil_link);
  if (cil)
    *cil = slot;
  return 0;
}

static void en50221_slot_applications_destroy(en50221_slot_t* cil) {
  en50221_app_t* app;

  while ((app = LIST_FIRST(&cil->cil_apps)) != NULL)
    en50221_app_destroy(app);
}

static void en50221_slot_clear(en50221_slot_t* cil) {
  en50221_slot_wmsg_t* wmsg;

  if (cil == NULL)
    return;
  en50221_slot_applications_destroy(cil);
  while ((wmsg = TAILQ_FIRST(&cil->cil_write_queue)) != NULL) {
    TAILQ_REMOVE(&cil->cil_write_queue, wmsg, link);
    free(wmsg);
  }
}

static void en50221_slot_destroy(en50221_slot_t* cil) {
  en50221_slot_clear(cil);
  LIST_REMOVE(cil, cil_link);
  free(cil->cil_name);
  free(cil);
}

static void en50221_slot_reset(en50221_slot_t* cil) {
  en50221_slot_clear(cil);
  cil->cil_ready               = 0;
  cil->cil_initiate_connection = 0;
  cil->cil_closing             = 0;
  cil->cil_tcnum               = 0;
}

static inline int en50221_slot_pdu_send(en50221_slot_t* cil,
    uint8_t                                             tag,
    const uint8_t*                                      data,
    size_t                                              datalen,
    int                                                 reply_is_expected) {
  en50221_slot_wmsg_t* wmsg;

  wmsg                    = malloc(sizeof(*wmsg) + datalen);
  wmsg->tag               = tag;
  wmsg->len               = datalen;
  wmsg->reply_is_expected = reply_is_expected;
  if (data && datalen > 0)
    memcpy(wmsg->data, data, datalen);
  TAILQ_INSERT_TAIL(&cil->cil_write_queue, wmsg, link);
  return 0;
}

static int en50221_slot_trigger_write_queue(en50221_slot_t* cil) {
  en50221_slot_wmsg_t* wmsg;
  int                  r = 0;

  wmsg = TAILQ_FIRST(&cil->cil_write_queue);
  if (wmsg) {
    TAILQ_REMOVE(&cil->cil_write_queue, wmsg, link);
    r = en50221_transport_pdu_write(cil->cil_transport,
        cil,
        cil->cil_tcnum,
        wmsg->tag,
        wmsg->data,
        wmsg->len);
    free(wmsg);
  } else {
    if (cil->cil_ready) {
      r = en50221_transport_pdu_write(cil->cil_transport,
          cil,
          cil->cil_tcnum,
          CICAM_T_DATA_LAST,
          NULL,
          0);
    }
  }
  if (cil->cil_closing && TAILQ_EMPTY(&cil->cil_write_queue))
    cil->cil_closing = 0;
  return r;
}

static int en50221_slot_initiate_connection(en50221_slot_t* cil) {
  en50221_app_t* app;
  int            r;

  if (cil->cil_apdu_only) {
    /* create dummy conditional access application */
    r = en50221_create_app(cil, CICAM_RI_CONDITIONAL_ACCESS_SUPPORT, 1, &app);
    if (r < 0)
      return r;
    r = en50221_app_pdu_send(app, CICAM_AOT_APPLICATION_INFO_ENQ, NULL, 0, 1);
    if (r < 0)
      return r;
    return 0;
  }
  /* static connection ID allocation */
  cil->cil_tcnum = cil->cil_number + 1;
  r              = en50221_slot_pdu_send(cil, CICAM_T_CREATE_TC, NULL, 0, 1);
  if (r < 0)
    return r;
  cil->cil_initiate_connection = 1;
  return en50221_slot_trigger_write_queue(cil);
}

int en50221_slot_enable(en50221_slot_t* cil) {
  if (!cil->cil_enabled) {
    en50221_slot_reset(cil);
    cil->cil_enabled = 1;
    return en50221_slot_initiate_connection(cil);
  }
  return 0;
}

int en50221_slot_disable(en50221_slot_t* cil) {
  if (cil->cil_enabled) {
    en50221_slot_reset(cil);
    cil->cil_enabled = 0;
    if (!cil->cil_apdu_only) {
      en50221_slot_pdu_send(cil, CICAM_T_DELETE_TC, NULL, 0, 1);
      cil->cil_closing = 1;
      tvhtrace(LS_EN50221, "%s: closing", cil->cil_name);
    }
  }
  return 0;
}

static int en50221_slot_connection_initiated(en50221_slot_t* cil) {
  cil->cil_initiate_connection = 0;
  return 0;
}

static en50221_app_t* en50221_slot_find_session(en50221_slot_t* cil, uint16_t session_id) {
  en50221_app_t* cia;

  LIST_FOREACH (cia, &cil->cil_apps, cia_link)
    if (cia->cia_session_id == session_id)
      return cia;
  return NULL;
}

en50221_app_t*
en50221_slot_find_application(en50221_slot_t* cil, uint32_t resource_id, uint32_t mask) {
  en50221_app_t* cia;

  resource_id &= mask;
  LIST_FOREACH (cia, &cil->cil_apps, cia_link) {
    if ((cia->cia_resource_id & mask) == resource_id)
      return cia;
  }
  return NULL;
}

#if 0
static int en50221_slot_session_create
  (en50221_slot_t *cil, uint32_t resource_id)
{
  en50221_app_t *app;
  uint16_t session;
  uint8_t buf[8];
  int r;

  session = ++cil->cil_last_session_number;
  if (session == 0)
    session = ++cil->cil_last_session_number;
  r = en50221_create_app(cil, resource_id, session, &app);
  if (r < 0)
    return r;
  buf[0] = CICAM_ST_CREATE_SESSION;
  buf[1] = 6;
  buf[2] = resource_id >> 24;
  buf[3] = resource_id >> 16;
  buf[4] = resource_id >> 8;
  buf[5] = resource_id;
  buf[6] = session >> 8;
  buf[7] = session;
  return en50221_slot_pdu_send(cil, CICAM_T_DATA_LAST, buf, 8);
}
#endif

static int en50221_slot_spdu_recv(en50221_slot_t* cil, const uint8_t* data, size_t datalen) {
  en50221_app_t* app;
  uint16_t       session;
  uint32_t       ri;
  uint8_t        buf[9];
  int            r;

  switch (data[0]) {
    case CICAM_ST_SESSION_NUMBER:
      if (datalen <= 4) {
        tvherror(LS_EN50221, "%s: unhandled session number length %zd", cil->cil_name, datalen);
        return -EINVAL;
      }
      session = (data[2] << 8) | data[3];
      app     = en50221_slot_find_session(cil, session);
      if (app == NULL) {
        tvherror(LS_EN50221,
            "%s: unhandled session message length %zd "
            "session number 0x%04x",
            cil->cil_name,
            datalen,
            session);
        return -EINVAL;
      }
      return en50221_app_handle(app, data[1], data + 4, datalen - 4);
    case CICAM_ST_OPEN_SESSION_REQUEST:
      if (datalen != 6 || data[1] != 4) {
        tvherror(LS_EN50221,
            "%s: unhandled open session request length %zd "
            "data1 0x%02x",
            cil->cil_name,
            datalen,
            data[1]);
        return -EINVAL;
      }
      ri  = get32(data + 2);
      app = en50221_slot_find_application(cil, ri, ~0);
      if (app == NULL) {
        session = ++cil->cil_last_session_number;
        if (session == 0)
          session = ++cil->cil_last_session_number;
        r = en50221_create_app(cil, ri, session, &app);
        if (r < 0)
          return r;
      } else {
        session = app->cia_session_id;
      }
      buf[0] = CICAM_ST_OPEN_SESSION_RESPONSE;
      buf[1] = 7;
      buf[2] = app->cia_prop ? CICAM_SS_OK : CICAM_SS_NOT_ALLOCATED;
      memcpy(buf + 3, data + 2, 4); /* ri */
      buf[7] = session >> 8;
      buf[8] = session & 0xff;
      r      = en50221_slot_pdu_send(cil, CICAM_T_DATA_LAST, buf, 9, 0);
      if (r < 0)
        return r;
      return en50221_app_open(app);
    case CICAM_ST_CREATE_SESSION_RESPONSE:
      if (datalen != 9 || data[1] != 7) {
        tvherror(LS_EN50221,
            "%s: unhandled create session response length %zd "
            "data1 0x%02x",
            cil->cil_name,
            datalen,
            data[1]);
        return -EINVAL;
      }
      ri      = get32(data + 3);
      session = (data[7] << 8) | data[8];
      if (data[2] != CICAM_SS_OK) {
        tvherror(LS_EN50221, "%s: CAM rejected to create session 0x%04x", cil->cil_name, session);
        return -EINVAL;
      }
      app = en50221_slot_find_session(cil, session);
      if (app == NULL) {
        r = en50221_create_app(cil, ri, session, &app);
        if (r < 0)
          return r;
      }
      break;
    case CICAM_ST_CLOSE_SESSION_REQUEST:
      if (datalen != 4 || data[1] != 2) {
        tvherror(LS_EN50221,
            "%s: unhandled close session request length %zd "
            "data1 0x%02x",
            cil->cil_name,
            datalen,
            data[1]);
        return -EINVAL;
      }
      session = (data[2] << 8) | data[3];
      app     = en50221_slot_find_session(cil, session);
      if (app == NULL) {
        tvherror(LS_EN50221,
            "%s: unhandled close session request, "
            "session number 0x%04x not found",
            cil->cil_name,
            session);
        return -EINVAL;
      }
      en50221_app_destroy(app);
      buf[0] = CICAM_ST_CLOSE_SESSION_RESPONSE;
      buf[1] = 3;
      buf[2] = CICAM_SS_OK;
      buf[3] = session >> 8;
      buf[4] = session & 0xff;
      return en50221_slot_pdu_send(cil, CICAM_T_DATA_LAST, buf, 5, 0);
    case CICAM_ST_CLOSE_SESSION_RESPONSE:
      if (datalen != 5 || data[1] != 3 || data[2]) {
        tvherror(LS_EN50221,
            "%s: unhandled close session response length %zd "
            "data1 0x%02x data2 0x%02x",
            cil->cil_name,
            datalen,
            data[1],
            data[2]);
        return -EINVAL;
      }
      session = (data[3] << 8) | data[4];
      app     = en50221_slot_find_session(cil, session);
      if (app == NULL) {
        tvherror(LS_EN50221,
            "%s: unhandled close session response, "
            "session number 0x%04x not found",
            cil->cil_name,
            session);
        return -EINVAL;
      }
      en50221_app_destroy(app);
      break;
    default:
      tvherror(LS_EN50221, "%s: unknown session tag 0x%02x", cil->cil_name, data[0]);
      return -EINVAL;
  }
  return 0;
}

static int en50221_slot_monitor(en50221_slot_t* cil, int64_t clock) {
  en50221_app_t* cia;
  int            r;

  LIST_FOREACH (cia, &cil->cil_apps, cia_link) {
    if (cil->cil_monitor_read + ms2mono(500) < mclk()) {
      tvherror(LS_EN50221, "%s: communication stalled for more than 500ms", cil->cil_name);
      return -ENXIO;
    }
    r = en50221_app_monitor(cia, clock);
    if (r < 0)
      return r;
  }
  return 0;
}

/*
 *  Application/session layer
 */
static LIST_HEAD(, en50221_app_prop) en50221_app_props;

void en50221_register_app(en50221_app_prop_t* prop) {
  LIST_INSERT_HEAD(&en50221_app_props, prop, ciap_link);
}

static int en50221_create_app(en50221_slot_t* cil,
    uint32_t                                  resource_id,
    uint16_t                                  session_id,
    en50221_app_t**                           cia) {
  en50221_app_prop_t* prop;
  en50221_app_t*      app;
  char                buf[128];

  LIST_FOREACH (prop, &en50221_app_props, ciap_link)
    if (prop->ciap_resource_id == resource_id)
      break;
  if (prop == NULL)
    tvherror(LS_EN50221, "%s: unknown resource id %08x", cil->cil_name, resource_id);
  app = calloc(1, prop ? prop->ciap_struct_size : 0);
  if (app == NULL)
    return -ENOMEM;
  snprintf(buf, sizeof(buf), "%s-app%08x/%04X", cil->cil_name, resource_id, session_id);
  app->cia_name        = strdup(buf);
  app->cia_slot        = cil;
  app->cia_resource_id = resource_id;
  app->cia_session_id  = session_id;
  app->cia_prop        = prop;
  LIST_INSERT_HEAD(&cil->cil_apps, app, cia_link);
  if (cia)
    *cia = app;
  tvhtrace(LS_EN50221, "%s: registered (%s)", app->cia_name, prop ? prop->ciap_name : "???");
  return 0;
}

static void en50221_app_destroy(en50221_app_t* cia) {
  en50221_app_prop_t* prop;

  if (cia == NULL)
    return;
  prop = cia->cia_prop;
  LIST_REMOVE(cia, cia_link);
  if (prop && prop->ciap_destroy)
    prop->ciap_destroy(cia);
  tvhtrace(LS_EN50221, "%s: destroyed (%s)", cia->cia_name, prop ? prop->ciap_name : "???");
  free(cia->cia_name);
  free(cia);
}

int en50221_app_pdu_send(en50221_app_t* app,
    uint32_t                            atag,
    const uint8_t*                      data,
    size_t                              datalen,
    int                                 reply_is_expected) {
  en50221_slot_t*      slot = app->cia_slot;
  en50221_transport_t* cit  = slot->cil_transport;
  uint8_t *            buf  = alloca(datalen + 12), *p, tag;
  size_t               hlen, buflen, tlen;
  int                  r;

  buf[0] = CICAM_ST_SESSION_NUMBER;
  buf[1] = 2;
  buf[2] = app->cia_session_id >> 8;
  buf[3] = app->cia_session_id & 0xff;
  buf[4] = atag >> 16;
  buf[5] = atag >> 8;
  buf[6] = atag & 0xff;
  if (datalen < 0x80) {
    buf[7] = datalen & 0x7f;
    hlen   = 8;
  } else if (datalen < 0x100) {
    buf[7] = 0x81;
    buf[8] = datalen & 0xff;
    hlen   = 9;
  } else if (datalen < 0x10000) {
    buf[7] = 0x82;
    buf[8] = datalen >> 8;
    buf[9] = datalen & 0xff;
    hlen   = 10;
  } else if (datalen < 0x1000000) {
    buf[7]  = 0x83;
    buf[8]  = datalen >> 16;
    buf[9]  = (datalen >> 8) & 0xff;
    buf[10] = datalen & 0xff;
    hlen    = 11;
  } else {
    buf[7]  = 0x84;
    buf[8]  = datalen >> 24;
    buf[9]  = (datalen >> 16) & 0xff;
    buf[10] = (datalen >> 8) & 0xff;
    buf[11] = datalen & 0xff;
    hlen    = 12;
  }
  if (datalen > 0)
    memcpy(buf + hlen, data, datalen);
  buflen = datalen + hlen;
  if (slot->cil_apdu_only)
    return cit->cit_ops->cihw_apdu_write(cit->cit_ops_aux, slot, buf + 4, buflen - 4);
  p = buf;
  while (buflen > 0) {
    if (buflen > CICAM_TDPU_DATA_SIZE) {
      tlen = CICAM_TDPU_DATA_SIZE;
      tag  = CICAM_T_DATA_MORE;
    } else {
      tlen = buflen;
      tag  = CICAM_T_DATA_LAST;
    }
    r = en50221_slot_pdu_send(slot, tag, p, tlen, reply_is_expected);
    if (r < 0)
      return r;
    buflen -= tlen;
    p += tlen;
  }
  return 0;
}

static int
en50221_app_handle(en50221_app_t* cia, uint8_t status, const uint8_t* data, size_t datalen) {
  en50221_app_prop_t* prop = cia->cia_prop;
  uint32_t            atag;
  const uint8_t*      p;
  size_t              l;
  int                 r;

  if (prop && prop->ciap_handle) {
    if (datalen < 4)
      return -ENXIO;
    r = en50221_extract_len(data + 3, datalen - 3, &p, &l, cia->cia_name, "apdu");
    if (r < 0)
      return -ENXIO;
    atag = getatag(data, datalen);
    if (atag < 8) {
      tvherror(LS_EN50221, "%s: invalid atag received 0x%06x", cia->cia_name, atag);
      return -ENXIO;
    }
    return prop->ciap_handle(cia, status, atag, p, l);
  }
  return 0;
}

static int en50221_app_open(en50221_app_t* cia) {
  en50221_app_prop_t* prop = cia->cia_prop;
  if (prop && prop->ciap_open)
    return prop->ciap_open(cia);
  return 0;
}

static int en50221_app_monitor(en50221_app_t* cia, int64_t clock) {
  if (cia->cia_prop && cia->cia_prop->ciap_monitor)
    return cia->cia_prop->ciap_monitor(cia, clock);
  return 0;
}

/*
 *
 */

int en50221_extract_len(const uint8_t* data,
    size_t                             datalen,
    const uint8_t**                    ptr,
    size_t*                            len,
    const char*                        prefix,
    const char*                        pdu_name) {
  const uint8_t *p, *end;
  uint32_t       j;
  size_t         l;

  l = data[0];
  p = data + 1;
  if (l < 0x80) {
  } else {
    j   = l & 0x7f;
    end = data + datalen;
    if (j > 3) {
      tvherror(LS_EN50221, "%s: invalid %s length 0x%02zx", prefix, pdu_name, l);
      return -ENXIO;
    }
    for (l = 0; j > 0 && p < end; j--) {
      l <<= 8;
      l |= *p++;
    }
    if (j > 0 || p >= end || l + (p - data) > datalen) {
      tvherror(LS_EN50221, "%s: invalid %s length 0x%06zx", prefix, pdu_name, l);
      return -ENXIO;
    }
  }
  *ptr = p;
  *len = l;
  return 0;
}
