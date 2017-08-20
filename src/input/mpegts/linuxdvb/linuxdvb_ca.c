 /*
 *  Tvheadend - Linux DVB CA
 *
 *  Copyright (C) 2015 Damjan Marion
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
#include "linuxdvb_private.h"
#include "notify.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

#include "descrambler/caid.h"
#include "descrambler/dvbcam.h"

static void linuxdvb_ca_monitor ( void *aux );

typedef enum {
  CA_SLOT_STATE_EMPTY = 0,
  CA_SLOT_STATE_MODULE_PRESENT,
  CA_SLOT_STATE_MODULE_READY
} ca_slot_state_t;

const static char *
ca_slot_state2str(ca_slot_state_t v)
{
  switch(v) {
    case CA_SLOT_STATE_EMPTY:           return "slot empty";
    case CA_SLOT_STATE_MODULE_PRESENT:  return "module present";
    case CA_SLOT_STATE_MODULE_READY:    return "module ready";
  };
  return "UNKNOWN";
}

const static char *
ca_pmt_list_mgmt2str(uint8_t v)
{
  switch(v) {
    case CA_LIST_MANAGEMENT_MORE:   return "more";
    case CA_LIST_MANAGEMENT_FIRST:  return "first";
    case CA_LIST_MANAGEMENT_LAST:   return "last";
    case CA_LIST_MANAGEMENT_ONLY:   return "only";
    case CA_LIST_MANAGEMENT_ADD:    return "add";
    case CA_LIST_MANAGEMENT_UPDATE: return "update";
  }
  return "UNKNOWN";
}

const static char *
ca_pmt_cmd_id2str(uint8_t v)
{
  switch(v) {
    case CA_PMT_CMD_ID_OK_DESCRAMBLING: return "ok_descrambling";
    case CA_PMT_CMD_ID_OK_MMI:          return "ok_mmi";
    case CA_PMT_CMD_ID_QUERY:           return "query";
    case CA_PMT_CMD_ID_NOT_SELECTED:    return "not_selected";
  }
  return "UNKNOWN";
}

struct linuxdvb_ca_capmt {
  TAILQ_ENTRY(linuxdvb_ca_capmt)  lcc_link;
  int      len;
  uint8_t *data;
  uint8_t  slot;
  uint8_t  list_mgmt;
  uint8_t  cmd_id;
};

/*
 *  ts101699 and CI+ 1.3 definitions
 */
#define TS101699_APP_AI_RESOURCEID  MKRID(2, 1, 2)
#define CIPLUS13_APP_AI_RESOURCEID  MKRID(2, 1, 3)

typedef enum {
  CIPLUS13_DATA_RATE_72_MBPS = 0,
  CIPLUS13_DATA_RATE_96_MBPS = 1,
} ciplus13_data_rate_t;

static int
ciplus13_app_ai_data_rate_info(linuxdvb_ca_t *lca, ciplus13_data_rate_t rate)
{
  uint8_t data[] = {0x9f, 0x80, 0x24, 0x01, (uint8_t) rate};

  /* only version 3 (CI+ 1.3) supports data_rate_info */
  if (lca->lca_ai_version != 3)
    return 0;

  tvhinfo(LS_EN50221, "setting CI+ CAM data rate to %s Mbps", rate ? "96":"72");

  return en50221_sl_send_data(lca->lca_sl, lca->lca_ai_session_number, data, sizeof(data));
}

static void
linuxdvb_ca_class_changed ( idnode_t *in )
{
  linuxdvb_adapter_t *la = ((linuxdvb_ca_t*)in)->lca_adapter;
  linuxdvb_adapter_changed(la);
}

static void
linuxdvb_ca_class_enabled_notify ( void *p, const char *lang )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) p;

  if (lca->lca_enabled) {
    if (lca->lca_ca_fd < 0) {
      lca->lca_ca_fd = tvh_open(lca->lca_ca_path, O_RDWR | O_NONBLOCK, 0);
      tvhtrace(LS_LINUXDVB, "opening ca%u %s (fd %d)",
               lca->lca_number, lca->lca_ca_path, lca->lca_ca_fd);
      if (lca->lca_ca_fd >= 0)
        mtimer_arm_rel(&lca->lca_monitor_timer, linuxdvb_ca_monitor, lca, ms2mono(250));
    }
  } else {
    tvhtrace(LS_LINUXDVB, "closing ca%u %s (fd %d)",
             lca->lca_number, lca->lca_ca_path, lca->lca_ca_fd);

    mtimer_disarm(&lca->lca_monitor_timer);

    if (lca->lca_en50221_thread_running) {
      lca->lca_en50221_thread_running = 0;
      pthread_join(lca->lca_en50221_thread, NULL);
    }

    if (lca->lca_ca_fd >= 0) {
      if (ioctl(lca->lca_ca_fd, CA_RESET, NULL))
        tvherror(LS_LINUXDVB, "unable to reset ca%u %s",
                 lca->lca_number, lca->lca_ca_path);

      close(lca->lca_ca_fd);
      lca->lca_ca_fd = -1;
    }

    idnode_notify_title_changed(&lca->lca_id, lang);
  }
}

static void
linuxdvb_ca_class_high_bitrate_notify ( void *p, const char *lang )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) p;
  ciplus13_app_ai_data_rate_info(lca, lca->lca_high_bitrate_mode ?
                                 CIPLUS13_DATA_RATE_96_MBPS :
                                 CIPLUS13_DATA_RATE_72_MBPS);
}

static const char *
linuxdvb_ca_class_get_title ( idnode_t *in, const char *lang )
{
  linuxdvb_ca_t *lca = (linuxdvb_ca_t *) in;
  static char buf[256];
  if (!lca->lca_enabled)
    snprintf(buf, sizeof(buf), "ca%u: disabled", lca->lca_number);
  else if (lca->lca_state == CA_SLOT_STATE_EMPTY)
    snprintf(buf, sizeof(buf), "ca%u: slot empty", lca->lca_number);
  else
    snprintf(buf, sizeof(buf), "ca%u: %s (%s)", lca->lca_number,
           lca->lca_cam_menu_string, lca->lca_state_str);

  return buf;
}

static const void *
linuxdvb_ca_class_active_get ( void *obj )
{
  static int active;
  linuxdvb_ca_t *lca = (linuxdvb_ca_t*)obj;
  active = !!lca->lca_enabled;
  return &active;
}

const idclass_t linuxdvb_ca_class =
{
  .ic_class      = "linuxdvb_ca",
  .ic_caption    = N_("Linux DVB CA"),
  .ic_changed    = linuxdvb_ca_class_changed,
  .ic_get_title  = linuxdvb_ca_class_get_title,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "active",
      .name     = N_("Active"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
      .get      = linuxdvb_ca_class_active_get,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the device."),
      .off      = offsetof(linuxdvb_ca_t, lca_enabled),
      .notify   = linuxdvb_ca_class_enabled_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "high_bitrate_mode",
      .name     = N_("High bitrate mode (CI+ CAMs only)"),
      .desc     = N_("Allow high bitrate mode (CI+ CAMs only)."),
      .off      = offsetof(linuxdvb_ca_t, lca_high_bitrate_mode),
      .notify   = linuxdvb_ca_class_high_bitrate_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "pin_reply",
      .name     = N_("Reply to CAM PIN inquiries"),
      .desc     = N_("Reply to PIN inquiries."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_reply),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "pin",
      .name     = N_("PIN"),
      .desc     = N_("The PIN to use."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_str),
      .opts     = PO_ADVANCED | PO_PASSWORD,
      .def.s    = "1234",
    },
    {
      .type     = PT_STR,
      .id       = "pin_match",
      .name     = N_("PIN inquiry match string"),
      .desc     = N_("PIN inquiry match string."),
      .off      = offsetof(linuxdvb_ca_t, lca_pin_match_str),
      .opts     = PO_ADVANCED,
      .def.s    = "PIN",
    },
    {
      .type     = PT_INT,
      .id       = "capmt_interval",
      .name     = N_("CAPMT interval (ms)"),
      .desc     = N_("CAPMT interval (in ms)."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_interval),
      .opts     = PO_ADVANCED,
      .def.i    = 100,
    },
    {
      .type     = PT_INT,
      .id       = "capmt_query_interval",
      .name     = N_("CAPMT query interval (ms)"),
      .desc     = N_("CAPMT query interval (ms)."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_query_interval),
      .opts     = PO_ADVANCED,
      .def.i    = 1200,
    },
    {
      .type     = PT_BOOL,
      .id       = "query_before_ok_descrambling",
      .name     = N_("Send CAPMT query"),
      .desc     = N_("Send CAPMT OK query before descrambling."),
      .off      = offsetof(linuxdvb_ca_t, lca_capmt_query),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "ca_path",
      .name     = N_("Device path"),
      .desc     = N_("Path used by the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_ca_t, lca_ca_path),
    },
    {
      .type     = PT_STR,
      .id       = "slot_state",
      .name     = N_("Slot state"),
      .desc     = N_("The CAM slot status."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_ca_t, lca_state_str),
    },
    {}
  }
};

static uint32_t
resource_ids[] = { EN50221_APP_RM_RESOURCEID,
                   EN50221_APP_MMI_RESOURCEID,
                   EN50221_APP_DVB_RESOURCEID,
                   EN50221_APP_CA_RESOURCEID,
                   EN50221_APP_DATETIME_RESOURCEID,
                   EN50221_APP_AI_RESOURCEID,
                   TS101699_APP_AI_RESOURCEID,
                   CIPLUS13_APP_AI_RESOURCEID};

static int
en50221_app_unknown_message(void *arg, uint8_t slot_id,
                            uint16_t session_num, uint32_t resource_id,
                            uint8_t *data, uint32_t data_length)
{
  tvhtrace(LS_EN50221, "unknown message slot_id %u, session_num %u, resource_id %x",
           slot_id, session_num, resource_id);
  tvhlog_hexdump(LS_EN50221, data, data_length);
  return 0;
}

static int
linuxdvb_ca_lookup_cb (void * arg, uint8_t slot_id, uint32_t requested_rid,
                       en50221_sl_resource_callback *callback_out,
                      void **arg_out, uint32_t *connected_rid)
{
    linuxdvb_ca_t * lca = arg;

    switch (requested_rid) {
      case EN50221_APP_RM_RESOURCEID:
        *callback_out = (en50221_sl_resource_callback) en50221_app_rm_message;
        *arg_out = lca->lca_rm_resource;
        *connected_rid = EN50221_APP_RM_RESOURCEID;
        break;
      case EN50221_APP_AI_RESOURCEID:
      case TS101699_APP_AI_RESOURCEID:
      case CIPLUS13_APP_AI_RESOURCEID:
        *callback_out = (en50221_sl_resource_callback) en50221_app_ai_message;
        *arg_out = lca->lca_ai_resource;
        *connected_rid = requested_rid;
        break;
      case EN50221_APP_CA_RESOURCEID:
        *callback_out = (en50221_sl_resource_callback) en50221_app_ca_message;
        *arg_out = lca->lca_ca_resource;
        *connected_rid = EN50221_APP_CA_RESOURCEID;
        break;
      case EN50221_APP_DATETIME_RESOURCEID:
        *callback_out = (en50221_sl_resource_callback) en50221_app_datetime_message;
        *arg_out = lca->lca_dt_resource;
        *connected_rid = EN50221_APP_DATETIME_RESOURCEID;
        break;
      case EN50221_APP_MMI_RESOURCEID:
        *callback_out = (en50221_sl_resource_callback) en50221_app_mmi_message;
        *arg_out = lca->lca_mmi_resource;
        *connected_rid = EN50221_APP_MMI_RESOURCEID;
        break;
      default:
        tvhtrace(LS_EN50221, "lookup cb for unknown resource id %x on slot %u",
                 requested_rid, slot_id);
        *callback_out = (en50221_sl_resource_callback) en50221_app_unknown_message;
        *arg_out = lca;
        *connected_rid = requested_rid;
    }
    return 0;
}

static int
linuxdvb_ca_session_cb (void *arg, int reason, uint8_t slot_id,
                        uint16_t session_num, uint32_t rid)
{
  linuxdvb_ca_t * lca = arg;

  if (reason == S_SCALLBACK_REASON_CAMCONNECTING) {
    tvhtrace(LS_EN50221, "0x%08x session %u connecting", rid, session_num);
    return 0;
  }

  if (reason == S_SCALLBACK_REASON_CAMCONNECTED) {
    tvhtrace(LS_EN50221, "0x%08x session %u connected", rid, session_num);
    switch(rid) {
      case EN50221_APP_RM_RESOURCEID:
        en50221_app_rm_enq(lca->lca_rm_resource, session_num);
        break;
      case EN50221_APP_AI_RESOURCEID:
      case TS101699_APP_AI_RESOURCEID:
      case CIPLUS13_APP_AI_RESOURCEID:
        lca->lca_ai_version = rid & 0x3f;
        lca->lca_ai_session_number = session_num;
        en50221_app_ai_enquiry(lca->lca_ai_resource, session_num);
        ciplus13_app_ai_data_rate_info(lca, lca->lca_high_bitrate_mode ?
                                       CIPLUS13_DATA_RATE_96_MBPS :
                                       CIPLUS13_DATA_RATE_72_MBPS );
        break;
      case EN50221_APP_CA_RESOURCEID:
        en50221_app_ca_info_enq(lca->lca_ca_resource, session_num);
        lca->lca_ca_session_number = session_num;
        break;
      case EN50221_APP_MMI_RESOURCEID:
      case EN50221_APP_DATETIME_RESOURCEID:
        break;
      default:
        tvhtrace(LS_EN50221, "session %u with unknown rid 0x%08x is connected",
                 session_num, rid);
    }
    return 0;
  }

  if (reason == S_SCALLBACK_REASON_CLOSE) {
    tvhtrace(LS_EN50221, "0x%08x session %u close", rid, session_num);
    switch(rid) {
      case EN50221_APP_CA_RESOURCEID:
        dvbcam_unregister_cam(lca, 0);
        lca->lca_cam_menu_string[0] = 0;
    }
    return 0;
  }

  if (reason == S_SCALLBACK_REASON_TC_CONNECT) {
    tvhtrace(LS_EN50221, "0x%08x session %u tc connect", rid, session_num);
    return 0;
  }

  tvhtrace(LS_EN50221, "unhandled session callback - reason %d slot_id %u "
           "session_num %u resource_id %x", reason, slot_id, session_num, rid);
  return 0;
}

static int
linuxdvb_ca_rm_enq_cb(void *arg, uint8_t slot_id, uint16_t session_num)
{
    linuxdvb_ca_t * lca = arg;

    tvhtrace(LS_EN50221, "rm enq callback received for slot %d", slot_id);

    if (en50221_app_rm_reply(lca->lca_rm_resource, session_num,
        sizeof(resource_ids)/4, resource_ids))
    {
      tvherror(LS_EN50221, "failed to send rm reply to slot %u session %u",
               slot_id, session_num);
    }

    return 0;
}

static int
linuxdvb_ca_rm_reply_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                        uint32_t resource_id_count, uint32_t *_resource_ids)
{
    linuxdvb_ca_t * lca = arg;

    tvhtrace(LS_EN50221, "rm reply cb received for slot %d, count %u", slot_id,
             resource_id_count);

    uint32_t i;
    for(i=0; i< resource_id_count; i++) {
        tvhtrace(LS_EN50221, "CAM provided resource id: %08x", _resource_ids[i]);
    }

    if (en50221_app_rm_changed(lca->lca_rm_resource, session_num)) {
        tvherror(LS_EN50221, "failed to send RM reply on slot %u", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_rm_changed_cb(void *arg, uint8_t slot_id,
                          uint16_t session_num)
{
    linuxdvb_ca_t * lca = arg;
    tvhtrace(LS_EN50221, "rm changed cb received for slot %d", slot_id);

    if (en50221_app_rm_enq(lca->lca_rm_resource, session_num)) {
        tvherror(LS_EN50221, "failed to send ENQ to slot %d", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_dt_enquiry_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                          uint8_t response_int)
{
    linuxdvb_ca_t * lca = arg;
    tvhtrace(LS_EN50221, "datetime enquiry cb received for slot %d", slot_id);

    if (en50221_app_datetime_send(lca->lca_dt_resource, session_num, time(NULL), -1)) {
        tvherror(LS_EN50221, "Failed to send datetime to slot %d", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_ai_callback(void *arg, uint8_t slot_id, uint16_t session_num,
                     uint8_t app_type, uint16_t app_manufacturer,
                     uint16_t manufacturer_code, uint8_t menu_string_len,
                     uint8_t *menu_string)
{
    linuxdvb_ca_t * lca = arg;

    tvhinfo(LS_EN50221, "CAM slot %u: Application type: %02x, manufacturer: %04x,"
            " Manufacturer code: %04x", slot_id, app_type, app_manufacturer,
            manufacturer_code);

    tvhinfo(LS_EN50221, "CAM slot %u: Menu string: %.*s", slot_id,
            menu_string_len, menu_string);

    snprintf(lca->lca_cam_menu_string, sizeof(lca->lca_cam_menu_string),
             "%.*s", menu_string_len, menu_string);

    idnode_notify_title_changed(&lca->lca_id, NULL);

    return 0;
}

static int
linuxdvb_ca_ca_info_callback(void *arg, uint8_t slot_id, uint16_t session_num,
                             uint32_t ca_id_count, uint16_t *ca_ids)
{
    linuxdvb_ca_t * lca = arg;
    uint32_t i, j;
    char buf[256];
    size_t c;

    dvbcam_unregister_cam(lca, 0);
    dvbcam_register_cam(lca, 0, ca_ids, ca_id_count);

    for (i = 0; i < ca_id_count; ) {
      for (j = 0, c = 0; j < 4 && i < ca_id_count; i++, j++)
          tvh_strlcatf(buf, sizeof(buf), c, " %04X (%s)",
                       ca_ids[i], caid2name(ca_ids[i]));
      tvhinfo(LS_EN50221, "CAM slot %u supported CAIDs: %s", slot_id, buf);
    }

    return 0;
}

static int
linuxdvb_ca_ca_pmt_reply_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                            struct en50221_app_pmt_reply *reply,
                            uint32_t reply_size)
{
    const char *str;

    switch (reply->CA_enable) {
      case 0x01: str = "possible"; break;
      case 0x02: str = "possible under conditions (purchase dialogue)"; break;
      case 0x03: str = "possible under conditions (technical dialogue)"; break;
      case 0x71: str = "not possible (because no entitlement)"; break;
      case 0x73: str = "not possible (for technical reasons)"; break;
      default:   str = "state unknown (unknown value received)";
    }

    tvhinfo(LS_EN50221, "CAM slot %u: descrambling %s", slot_id, str);

    return 0;
}

static int
linuxdvb_ca_mmi_close_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                         uint8_t cmd_id, uint8_t delay)
{
    tvhtrace(LS_EN50221, "mmi close cb received for slot %u session_num %u "
             "cmd_id 0x%02x delay %u" , slot_id, session_num, cmd_id, delay);

    return 0;
}

static int
linuxdvb_ca_mmi_display_ctl_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                               uint8_t cmd_id, uint8_t mmi_mode)
{
    linuxdvb_ca_t * lca = arg;

    tvhtrace(LS_EN50221, "mmi display ctl cb received for slot %u session_num %u "
             "cmd_id 0x%02x mmi_mode %u" , slot_id, session_num, cmd_id, mmi_mode);

    if (cmd_id == MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE) {
        struct en50221_app_mmi_display_reply_details d;

        d.u.mode_ack.mmi_mode = mmi_mode;
        if (en50221_app_mmi_display_reply(lca->lca_mmi_resource, session_num,
                                          MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK, &d)) {
            tvherror(LS_EN50221,"Slot %u: Failed to send MMI mode ack reply", slot_id);
        }
    }

    return 0;
}

static int
linuxdvb_ca_mmi_enq_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                       uint8_t blind_answ, uint8_t exp_answ_len,
                       uint8_t *text, uint32_t text_size)
{
    linuxdvb_ca_t * lca = arg;
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "%.*s", text_size, text);

    tvhnotice(LS_EN50221, "MMI enquiry from CAM in slot %u:  %s (%s%u digits)",
              slot_id, buffer, blind_answ ? "blind " : "" , exp_answ_len);

    if (lca->lca_pin_reply &&
        (strlen((char *) lca->lca_pin_str) == exp_answ_len) &&
        strstr((char *) buffer, lca->lca_pin_match_str))
    {
      tvhtrace(LS_EN50221, "answering to PIN enquiry");
      en50221_app_mmi_answ(lca->lca_mmi_resource, session_num,
                           MMI_ANSW_ID_ANSWER, (uint8_t *) lca->lca_pin_str,
                           exp_answ_len);
    }

    en50221_app_mmi_close(lca->lca_mmi_resource, session_num,
                          MMI_CLOSE_MMI_CMD_ID_IMMEDIATE, 0);

    return 0;
}

static int
linuxdvb_ca_mmi_menu_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                        struct en50221_app_mmi_text *title,
                        struct en50221_app_mmi_text *sub_title,
                        struct en50221_app_mmi_text *bottom,
                        uint32_t item_count, struct en50221_app_mmi_text *items,
                        uint32_t item_raw_length, uint8_t *items_raw)
{
    linuxdvb_ca_t * lca = arg;

    tvhnotice(LS_EN50221, "MMI menu from CAM in the slot %u:", slot_id);
    tvhnotice(LS_EN50221, "  title:    %.*s", title->text_length, title->text);
    tvhnotice(LS_EN50221, "  subtitle: %.*s", sub_title->text_length, sub_title->text);

    uint32_t i;
    for(i=0; i< item_count; i++) {
        tvhnotice(LS_EN50221, "  item %i:   %.*s", i+1,  items[i].text_length, items[i].text);
    }
    tvhnotice(LS_EN50221, "  bottom:   %.*s", bottom->text_length, bottom->text);

    /* cancel menu */
    en50221_app_mmi_close(lca->lca_mmi_resource, session_num,
                          MMI_CLOSE_MMI_CMD_ID_IMMEDIATE, 0);
    return 0;
}

static int
linuxdvb_ca_app_mmi_list_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                            struct en50221_app_mmi_text *title,
                            struct en50221_app_mmi_text *sub_title,
                            struct en50221_app_mmi_text *bottom,
                            uint32_t item_count, struct en50221_app_mmi_text *items,
                            uint32_t item_raw_length, uint8_t *items_raw)
{
    linuxdvb_ca_t * lca = arg;

    tvhnotice(LS_EN50221, "MMI list from CAM in the slot %u:", slot_id);
    tvhnotice(LS_EN50221, "  title:    %.*s", title->text_length, title->text);
    tvhnotice(LS_EN50221, "  subtitle: %.*s", sub_title->text_length, sub_title->text);

    uint32_t i;
    for(i=0; i< item_count; i++) {
        tvhnotice(LS_EN50221, "  item %i:   %.*s", i+1,  items[i].text_length, items[i].text);
    }
    tvhnotice(LS_EN50221, "  bottom:   %.*s", bottom->text_length, bottom->text);

    /* cancel menu */
    en50221_app_mmi_close(lca->lca_mmi_resource, session_num,
                          MMI_CLOSE_MMI_CMD_ID_IMMEDIATE, 0);
    return 0;
}

static void *
linuxdvb_ca_en50221_thread ( void *aux )
{
  linuxdvb_ca_t *lca = aux;
  int slot_id, lasterror = 0;

  lca->lca_tl = en50221_tl_create(5, 32);
  if (!lca->lca_tl) {
    tvherror(LS_EN50221, "failed to create transport layer");
    return NULL;
  }

  slot_id = en50221_tl_register_slot(lca->lca_tl, lca->lca_ca_fd, 0, 1000, 100);
  if (slot_id < 0) {
    tvherror(LS_EN50221, "slot registration failed");
    return NULL;
  }

  lca->lca_sl = en50221_sl_create(lca->lca_tl, 256);
  if (lca->lca_sl == NULL) {
    tvherror(LS_EN50221, "failed to create session layer");
    return NULL;
  }

  // create the sendfuncs
  lca->lca_sf.arg        = lca->lca_sl;
  lca->lca_sf.send_data  = (en50221_send_data) en50221_sl_send_data;
  lca->lca_sf.send_datav = (en50221_send_datav) en50221_sl_send_datav;

  /* create app resources and assign callbacks */
  lca->lca_rm_resource = en50221_app_rm_create(&lca->lca_sf);
  en50221_app_rm_register_enq_callback(lca->lca_rm_resource, linuxdvb_ca_rm_enq_cb, lca);
  en50221_app_rm_register_reply_callback(lca->lca_rm_resource, linuxdvb_ca_rm_reply_cb, lca);
  en50221_app_rm_register_changed_callback(lca->lca_rm_resource, linuxdvb_ca_rm_changed_cb, lca);

  lca->lca_dt_resource = en50221_app_datetime_create(&lca->lca_sf);
  en50221_app_datetime_register_enquiry_callback(lca->lca_dt_resource, linuxdvb_ca_dt_enquiry_cb, lca);

  lca->lca_ai_resource = en50221_app_ai_create(&lca->lca_sf);
  en50221_app_ai_register_callback(lca->lca_ai_resource, linuxdvb_ca_ai_callback, lca);

  lca->lca_ca_resource = en50221_app_ca_create(&lca->lca_sf);
  en50221_app_ca_register_info_callback(lca->lca_ca_resource, linuxdvb_ca_ca_info_callback, lca);
  en50221_app_ca_register_pmt_reply_callback(lca->lca_ca_resource, linuxdvb_ca_ca_pmt_reply_cb, lca);

  lca->lca_mmi_resource = en50221_app_mmi_create(&lca->lca_sf);
  en50221_app_mmi_register_close_callback(lca->lca_mmi_resource, linuxdvb_ca_mmi_close_cb, lca);
  en50221_app_mmi_register_display_control_callback(lca->lca_mmi_resource, linuxdvb_ca_mmi_display_ctl_cb, lca);
  en50221_app_mmi_register_enq_callback(lca->lca_mmi_resource, linuxdvb_ca_mmi_enq_cb, lca);
  en50221_app_mmi_register_menu_callback(lca->lca_mmi_resource, linuxdvb_ca_mmi_menu_cb, lca);
  en50221_app_mmi_register_list_callback(lca->lca_mmi_resource, linuxdvb_ca_app_mmi_list_cb, lca);

  en50221_sl_register_lookup_callback(lca->lca_sl, linuxdvb_ca_lookup_cb, lca);
  en50221_sl_register_session_callback(lca->lca_sl, linuxdvb_ca_session_cb, lca);

  lca->lca_tc = en50221_tl_new_tc(lca->lca_tl, slot_id);

  while (tvheadend_is_running() && lca->lca_en50221_thread_running) {
        int error;
        if ((error = en50221_tl_poll(lca->lca_tl)) != 0) {
            if (error != lasterror) {
                tvherror(LS_EN50221, "poll error on slot %d [error:%i]",
                        en50221_tl_get_error_slot(lca->lca_tl),
                        en50221_tl_get_error(lca->lca_tl));
            }
            lasterror = error;
        }
  }

  dvbcam_unregister_cam(lca, 0);

  en50221_tl_destroy_slot(lca->lca_tl, slot_id);
  en50221_sl_destroy(lca->lca_sl);
  en50221_tl_destroy(lca->lca_tl);
  return 0;
}

static void
linuxdvb_ca_monitor ( void *aux )
{
  linuxdvb_ca_t *lca = aux;
  ca_slot_info_t csi;
  int state;

  if (lca->lca_ca_fd < 0)
    return;

  memset(&csi, 0, sizeof(csi));

  if ((ioctl(lca->lca_ca_fd, CA_GET_SLOT_INFO, &csi)) != 0) {
    tvherror(LS_LINUXDVB, "failed to get CAM slot %u info [e=%s]",
             csi.num, strerror(errno));
  }
  if (csi.flags & CA_CI_MODULE_READY)
    state = CA_SLOT_STATE_MODULE_READY;
  else if (csi.flags & CA_CI_MODULE_PRESENT)
    state = CA_SLOT_STATE_MODULE_PRESENT;
  else
    state = CA_SLOT_STATE_EMPTY;

  lca->lca_state_str = ca_slot_state2str(state);

  if (lca->lca_state != state) {
    tvhnotice(LS_LINUXDVB, "CAM slot %u status changed to %s",
              csi.num, lca->lca_state_str);
    idnode_notify_title_changed(&lca->lca_id, NULL);
    lca->lca_state = state;
  }

  if ((!lca->lca_en50221_thread_running) &&
      (state == CA_SLOT_STATE_MODULE_READY)) {
    lca->lca_en50221_thread_running = 1;
    tvhthread_create(&lca->lca_en50221_thread, NULL,
                     linuxdvb_ca_en50221_thread, lca, "lnxdvb-ca");
  } else if (lca->lca_en50221_thread_running &&
             (state != CA_SLOT_STATE_MODULE_READY)) {
    lca->lca_en50221_thread_running = 0;
    pthread_join(lca->lca_en50221_thread, NULL);
  }

  mtimer_arm_rel(&lca->lca_monitor_timer, linuxdvb_ca_monitor, lca, ms2mono(250));
}

linuxdvb_ca_t *
linuxdvb_ca_create
  ( htsmsg_t *conf, linuxdvb_adapter_t *la, int number, const char *ca_path)
{
  linuxdvb_ca_t *lca;
  char id[6];
  const char *uuid = NULL;

  lca = calloc(1, sizeof(linuxdvb_ca_t));
  memset(lca, 0, sizeof(linuxdvb_ca_t));
  lca->lca_number = number;
  lca->lca_ca_path  = strdup(ca_path);
  lca->lca_ca_fd = -1;
  lca->lca_capmt_interval = 100;
  lca->lca_capmt_query_interval = 1200;

  /* Internal config ID */
  snprintf(id, sizeof(id), "ca%u", number);

  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  if (idnode_insert(&lca->lca_id, uuid, &linuxdvb_ca_class, 0)) {
    free(lca);
    return NULL;
  }

  if (conf)
    idnode_load(&lca->lca_id, conf);

  /* Adapter link */
  lca->lca_adapter = la;
  LIST_INSERT_HEAD(&la->la_ca_devices, lca, lca_link);

  TAILQ_INIT(&lca->lca_capmt_queue);

  return lca;
}

static void
linuxdvb_ca_process_capmt_queue ( void *aux )
{
  linuxdvb_ca_t *lca = aux;
  linuxdvb_ca_capmt_t *lcc;
  struct section *section;
  struct section_ext *result;
  struct mpeg_pmt_section *pmt;
  uint8_t capmt[4096];
  int size, i;

  lcc = TAILQ_FIRST(&lca->lca_capmt_queue);

  if (!lcc)
    return;

  if (!(section = section_codec(lcc->data, lcc->len))){
    tvherror(LS_EN50221, "failed to decode PMT section");
    goto done;
  }

  if (!(result = section_ext_decode(section, 0))){
    tvherror(LS_EN50221, "failed to decode PMT ext_section");
    goto done;
  }

  if (!(pmt = mpeg_pmt_section_codec(result))){
    tvherror(LS_EN50221, "failed to decode PMT");
    goto done;
  }

  size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), 0,
                               lcc->list_mgmt, lcc->cmd_id);

  if (size < 0) {
    tvherror(LS_EN50221, "Failed to format CAPMT");
  }

  if (en50221_app_ca_pmt(lca->lca_ca_resource, lca->lca_ca_session_number,
                         capmt, size)) {
        tvherror(LS_EN50221, "Failed to send CAPMT");
  }

  tvhtrace(LS_EN50221, "%s CAPMT sent (%s)", ca_pmt_cmd_id2str(lcc->cmd_id),
           ca_pmt_list_mgmt2str(lcc->list_mgmt));
  tvhlog_hexdump(LS_EN50221, capmt, size);

done:
  i = (lcc->cmd_id == CA_PMT_CMD_ID_QUERY) ?
    lca->lca_capmt_query_interval : lca->lca_capmt_interval;

  TAILQ_REMOVE(&lca->lca_capmt_queue, lcc, lcc_link);

  free(lcc->data);
  free(lcc);

  if (!TAILQ_EMPTY(&lca->lca_capmt_queue)) {
    mtimer_arm_rel(&lca->lca_capmt_queue_timer,
                   linuxdvb_ca_process_capmt_queue, lca, ms2mono(i));
  }
}

void
linuxdvb_ca_enqueue_capmt(linuxdvb_ca_t *lca, uint8_t slot, const uint8_t *ptr,
                          int len, uint8_t list_mgmt, uint8_t cmd_id)
{
  linuxdvb_ca_capmt_t *lcc;
  int c = 1;

  if (!lca)
    return;

  if (lca->lca_capmt_query && cmd_id == CA_PMT_CMD_ID_OK_DESCRAMBLING)
    c = 2;

  while (c--) {
    lcc = calloc(1, sizeof(*lcc));

    if (!lcc)
      return;

    lcc->data = malloc(len);
    lcc->len = len;
    lcc->slot = slot;
    lcc->list_mgmt = list_mgmt;
    lcc->cmd_id = (c ? CA_PMT_CMD_ID_QUERY : cmd_id);
    memcpy(lcc->data, ptr, len);

    TAILQ_INSERT_TAIL(&lca->lca_capmt_queue, lcc, lcc_link);

    tvhtrace(LS_EN50221, "%s CAPMT enqueued (%s)", ca_pmt_cmd_id2str(lcc->cmd_id),
             ca_pmt_list_mgmt2str(lcc->list_mgmt));
  }

  mtimer_arm_rel(&lca->lca_capmt_queue_timer,
                 linuxdvb_ca_process_capmt_queue, lca, ms2mono(50));
}

void linuxdvb_ca_save( linuxdvb_ca_t *lca, htsmsg_t *msg )
{
  char id[8], ubuf[UUID_HEX_SIZE];
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&lca->lca_id, ubuf));
  idnode_save(&lca->lca_id, m);

  /* Add to list */
  snprintf(id, sizeof(id), "ca%u", lca->lca_number);
  htsmsg_add_msg(msg, id, m);

}
