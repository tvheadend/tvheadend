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
#include "atomic.h"
#include "tvhpoll.h"
#include "streaming.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

#include "descrambler/dvbcam.h"

typedef enum {
  CA_SLOT_STATE_EMPTY = 0,
  CA_SLOT_STATE_MODULE_PRESENT,
  CA_SLOT_STATE_MODULE_READY
} ca_slot_state_t;

const static char *
ca_slot_state2str(ca_slot_state_t v)
{
  switch(v) {
    case CA_SLOT_STATE_EMPTY:           return "EMPTY";
    case CA_SLOT_STATE_MODULE_PRESENT:  return "MODULE_PRESENT";
    case CA_SLOT_STATE_MODULE_READY:    return "MODULE_READY";
	};
  return "UNKNOWN";
}

static uint32_t
resource_ids[] = { EN50221_APP_RM_RESOURCEID,
                   EN50221_APP_MMI_RESOURCEID,
                   EN50221_APP_DVB_RESOURCEID,
                   EN50221_APP_CA_RESOURCEID,
                   EN50221_APP_DATETIME_RESOURCEID,
                   EN50221_APP_AI_RESOURCEID};

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
        *callback_out = (en50221_sl_resource_callback) en50221_app_ai_message;
        *arg_out = lca->lca_ai_resource;
        *connected_rid = EN50221_APP_AI_RESOURCEID;
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
      default:
        tvhtrace("en50221", "lookup cb for unknown resource id %u on slot %u",
                 requested_rid, slot_id);
    		return -1;
    }
    return 0;
}

static int
linuxdvb_ca_session_cb (void *arg, int reason, uint8_t slot_id,
                        uint16_t session_num, uint32_t rid)
{
    linuxdvb_ca_t * lca = arg;

	switch(reason) {
		case S_SCALLBACK_REASON_CAMCONNECTING:
			break;
		case S_SCALLBACK_REASON_CAMCONNECTED:
      if (rid == EN50221_APP_RM_RESOURCEID) {
          en50221_app_rm_enq(lca->lca_rm_resource, session_num);
      } else if (rid == EN50221_APP_AI_RESOURCEID) {
          en50221_app_ai_enquiry(lca->lca_ai_resource, session_num);
      } else if (rid == EN50221_APP_CA_RESOURCEID) {
          en50221_app_ca_info_enq(lca->lca_ca_resource, session_num);
          lca->lca_ca_session_number = session_num;
      }
			break;
    case S_SCALLBACK_REASON_CLOSE:
      if (rid == EN50221_APP_CA_RESOURCEID)
        dvbcam_unregister_cam(lca, 0);
      break;
    case S_SCALLBACK_REASON_TC_CONNECT:
      break;
		default:
		  tvhtrace("en50221", "unhandled sessopn callback - "
		  	       "reason %d slot_id %u session_num %u resource_id %x",
		           reason, slot_id, session_num, rid);
	}
    return 0;
}

static int
linuxdvb_ca_rm_enq_cb(void *arg, uint8_t slot_id, uint16_t session_num)
{
    linuxdvb_ca_t * lca = arg;

    tvhtrace("en50221", "rm enq callback received for slot %d", slot_id);

    if (en50221_app_rm_reply(lca->lca_rm_resource, session_num,
    	  sizeof(resource_ids)/4, resource_ids)) {
        tvherror("en50221", "failed to send rm reply to slot %u session %u",
                 slot_id, session_num);
    }

    return 0;
}

static int
linuxdvb_ca_rm_reply_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                        uint32_t resource_id_count, uint32_t *_resource_ids)
{
    linuxdvb_ca_t * lca = arg;

    tvhtrace("en50221", "rm reply cb received for slot %d", slot_id);

    uint32_t i;
    for(i=0; i< resource_id_count; i++) {
        tvhtrace("en50221", "CAM provided resource id: %08x", _resource_ids[i]);
    }

    if (en50221_app_rm_changed(lca->lca_rm_resource, session_num)) {
        tvherror("en50221", "failed to send RM reply on slot %u", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_rm_changed_cb(void *arg, uint8_t slot_id,
                          uint16_t session_num)
{
    linuxdvb_ca_t * lca = arg;
    tvhtrace("en50221", "rm changed cb received for slot %d", slot_id);

    if (en50221_app_rm_enq(lca->lca_rm_resource, session_num)) {
        tvherror("en50221", "failed to send ENQ to slot %d", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_dt_enquiry_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                          uint8_t response_int)
{
    linuxdvb_ca_t * lca = arg;
    tvhtrace("en50221", "datetime enquiry cb received for slot %d", slot_id);

    if (en50221_app_datetime_send(lca->lca_dt_resource, session_num, time(NULL), -1)) {
        tvherror("en50221", "Failed to send datetime to slot %d", slot_id);
    }

    return 0;
}

static int
linuxdvb_ca_ai_callback(void *arg, uint8_t slot_id, uint16_t session_num,
                     uint8_t app_type, uint16_t app_manufacturer,
                     uint16_t manufacturer_code, uint8_t menu_string_len,
                     uint8_t *menu_string)
{
    tvhinfo("en50221", "slot %u: Application type: %02x", slot_id, app_type);
    tvhinfo("en50221", "slot %u: Application manufacturer: %04x",slot_id, app_manufacturer);
    tvhinfo("en50221", "slot %u: Manufacturer code: %04x", slot_id, manufacturer_code);
    tvhinfo("en50221", "slot %u: Menu string: %.*s", slot_id, menu_string_len, menu_string);
    return 0;
}

static int
linuxdvb_ca_ca_info_callback(void *arg, uint8_t slot_id, uint16_t session_num, uint32_t ca_id_count, uint16_t *ca_ids)
{
    linuxdvb_ca_t * lca = arg;
    uint32_t i;


    dvbcam_unregister_cam(lca, 0);
    dvbcam_register_cam(lca, 0, ca_ids, ca_id_count);


    for(i=0; i< ca_id_count; i++) {
        tvhinfo("en50221", "slot %d supported CAID: %04x (%s)", slot_id,
        	      ca_ids[i], descrambler_caid2name(ca_ids[i]));
    }

    //ca_connected = 1;
    return 0;
}

static int
linuxdvb_ca_ca_pmt_reply_cb(void *arg, uint8_t slot_id, uint16_t session_num,
                            struct en50221_app_pmt_reply *reply,
                            uint32_t reply_size)
{
    tvhtrace("en50221", "ca pmt reply callback received for slot %d", slot_id);

    return 0;
}

static void *
linuxdvb_ca_en50221_thread ( void *aux )
{
  linuxdvb_ca_t *lca = aux;
  int slot_id, lasterror = 0;

  lca->lca_tl = en50221_tl_create(5, 32);
  if (!lca->lca_tl) {
    tvherror("en50221", "failed to create transport layer");
    return NULL;
  }

  slot_id = en50221_tl_register_slot(lca->lca_tl, lca->lca_ca_fd, 0, 1000, 100);
  if (slot_id < 0) {
    tvherror("en50221", "slot registration failed");
    return NULL;
  }

  lca->lca_sl = en50221_sl_create(lca->lca_tl, 256);
  if (lca->lca_sl == NULL) {
    tvherror("en50221", "failed to create session layer");
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

  en50221_sl_register_lookup_callback(lca->lca_sl, linuxdvb_ca_lookup_cb, lca);
  en50221_sl_register_session_callback(lca->lca_sl, linuxdvb_ca_session_cb, lca);

  lca->lca_tc = en50221_tl_new_tc(lca->lca_tl, slot_id);

  while (tvheadend_running & lca->lca_en50221_thread_running) {
        int error;
        if ((error = en50221_tl_poll(lca->lca_tl)) != 0) {
            if (error != lasterror) {
                tvherror("en50221", "poll error on slot %d [error:%i]",
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

	csi.num = 0;

	if (lca->lca_ca_fd > 0) {

		if ((ioctl(lca->lca_ca_fd, CA_GET_SLOT_INFO, &csi)) != 0) {
      tvherror("linuxdvb", "failed to get ca%u slot info [e=%s]",
      	       lca->lca_number, strerror(errno));
    }
    if (csi.flags & CA_CI_MODULE_READY)
    	state = CA_SLOT_STATE_MODULE_READY;
    else if (csi.flags & CA_CI_MODULE_PRESENT)
  	  state = CA_SLOT_STATE_MODULE_PRESENT;
    else
    	state = CA_SLOT_STATE_EMPTY;

    if (lca->lca_state != state) {
	    tvhlog(LOG_INFO, "linuxdvb", "ca%u slot %u status changed to %s",
	  	       lca->lca_number, csi.num, ca_slot_state2str(state));

      if (state == CA_SLOT_STATE_MODULE_READY) {
      	lca->lca_en50221_thread_running = 1;
        tvhthread_create(&lca->lca_en50221_thread, NULL,
                         linuxdvb_ca_en50221_thread, lca);

	    } else {
	    	lca->lca_en50221_thread_running = 0;
	    	pthread_join(lca->lca_en50221_thread, NULL);
	    }

		  lca->lca_state = state;
	  }
	}

  gtimer_arm_ms(&lca->lca_monitor_timer, linuxdvb_ca_monitor, lca, 250);
}

linuxdvb_ca_t *
linuxdvb_ca_create
  ( linuxdvb_adapter_t *la, int number, const char *ca_path)
{
	linuxdvb_ca_t *lca;
	char id[6];

  /* Internal config ID */
  snprintf(id, sizeof(id), "CA #%d", number);

	lca = calloc(1, sizeof(linuxdvb_frontend_t));
  lca->lca_number = number;
  lca->lca_ca_path  = strdup(ca_path);

  /* Adapter link */
  lca->lca_adapter = la;
  LIST_INSERT_HEAD(&la->la_cas, lca, lca_link);

  lca->lca_ca_fd = tvh_open(lca->lca_ca_path, O_RDWR | O_NONBLOCK, 0);
    tvhtrace("linuxdvb", "opening CA%u %s (fd %d)",
             lca->lca_number, lca->lca_ca_path, lca->lca_ca_fd);

  gtimer_arm_ms(&lca->lca_monitor_timer, linuxdvb_ca_monitor, lca, 250);

	return lca;
}

void
linuxdvb_ca_send_capmt(linuxdvb_ca_t *lca, uint8_t slot, const uint8_t *ptr, int len)
{
  struct section *section;
  struct section_ext *result;
  struct mpeg_pmt_section *pmt;
  uint8_t *buffer;
  uint8_t capmt[4096];
  int size;

  buffer = malloc(len);
  if (!buffer)
    return;

  memcpy(buffer, ptr, len);

  section = section_codec(buffer, len);
  if (!section){
    tvherror("en50221", "failed to decode PMT section");
    goto fail;
  }

  result = section_ext_decode(section, 0);
  if (!result){
    tvherror("en50221", "failed to decode PMT ext_section");
    goto fail;
  }

  pmt = mpeg_pmt_section_codec(result);
  if (!pmt){
    tvherror("en50221", "failed to decode PMT");
    goto fail;
  }

  size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt),
                               CA_LIST_MANAGEMENT_ONLY, slot,
                               CA_PMT_CMD_ID_OK_DESCRAMBLING);
  if (size < 0) {
    tvherror("en50221", "Failed to format CAPMT");
  }
  if (en50221_app_ca_pmt(lca->lca_ca_resource, lca->lca_ca_session_number,
                         capmt, size)) {
        tvherror("en50221", "Failed to send CAPMT");
  }

  tvhtrace("en50221", "CAPMT sent");

fail:
  free(buffer);
}
