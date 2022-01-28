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

#ifndef __EN50221_H__
#define __EN50221_H__

#include "queue.h"
#include "htsmsg.h"
#include "sbuf.h"

/* Session resource IDs */
#define CICAM_RI_RESOURCE_MANAGER            0x00010041
#define CICAM_RI_APPLICATION_INFORMATION     0x00020041
#define CICAM_RI_CONDITIONAL_ACCESS_SUPPORT  0x00030041
#define CICAM_RI_HOST_CONTROL                0x00200041
#define CICAM_RI_DATE_TIME                   0x00240041
#define CICAM_RI_MMI                         0x00400041

#define CICAM_RI_DUMMY_CAPMT                 0x00ffffff

/* Application tags */
#define CICAM_AOT_NONE                       0x000000
#define CICAM_AOT_PROFILE_ENQ                0x9F8010
#define CICAM_AOT_PROFILE                    0x9F8011
#define CICAM_AOT_PROFILE_CHANGE             0x9F8012
#define CICAM_AOT_APPLICATION_INFO_ENQ       0x9F8020
#define CICAM_AOT_APPLICATION_INFO           0x9F8021
#define CICAM_AOT_ENTER_MENU                 0x9F8022
#define CICAM_AOT_PCMCIA_DATA_RATE           0x9F8024
#define CICAM_AOT_CA_INFO_ENQ                0x9F8030
#define CICAM_AOT_CA_INFO                    0x9F8031
#define CICAM_AOT_CA_PMT                     0x9F8032
#define CICAM_AOT_CA_PMT_REPLY               0x9F8033
#define CICAM_AOT_CA_UPDATE                  0x9F8034
#define CICAM_AOT_TUNE                       0x9F8400
#define CICAM_AOT_REPLACE                    0x9F8401
#define CICAM_AOT_CLEAR_REPLACE              0x9F8402
#define CICAM_AOT_ASK_RELEASE                0x9F8403
#define CICAM_AOT_DATE_TIME_ENQ              0x9F8440
#define CICAM_AOT_DATE_TIME                  0x9F8441
#define CICAM_AOT_CLOSE_MMI                  0x9F8800
#define CICAM_AOT_DISPLAY_CONTROL            0x9F8801
#define CICAM_AOT_DISPLAY_REPLY              0x9F8802
#define CICAM_AOT_TEXT_LAST                  0x9F8803
#define CICAM_AOT_TEXT_MORE                  0x9F8804
#define CICAM_AOT_KEYPAD_CONTROL             0x9F8805
#define CICAM_AOT_KEYPRESS                   0x9F8806
#define CICAM_AOT_ENQ                        0x9F8807
#define CICAM_AOT_ANSW                       0x9F8808
#define CICAM_AOT_MENU_LAST                  0x9F8809
#define CICAM_AOT_MENU_MORE                  0x9F880A
#define CICAM_AOT_MENU_ANSW                  0x9F880B
#define CICAM_AOT_LIST_LAST                  0x9F880C
#define CICAM_AOT_LIST_MORE                  0x9F880D
#define CICAM_AOT_SUBTITLE_SEGMENT_LAST      0x9F880E
#define CICAM_AOT_SUBTITLE_SEGMENT_MORE      0x9F880F
#define CICAM_AOT_DISPLAY_MESSAGE            0x9F8810
#define CICAM_AOT_SCENE_END_MARK             0x9F8811
#define CICAM_AOT_SCENE_DONE                 0x9F8812
#define CICAM_AOT_SCENE_CONTROL              0x9F8813
#define CICAM_AOT_SUBTITLE_DOWNLOAD_LAST     0x9F8814
#define CICAM_AOT_SUBTITLE_DOWNLOAD_MORE     0x9F8815
#define CICAM_AOT_FLUSH_DOWNLOAD             0x9F8816
#define CICAM_AOT_DOWNLOAD_REPLY             0x9F8817
#define CICAM_AOT_COMMS_CMD                  0x9F8C00
#define CICAM_AOT_CONNECTION_DESCRIPTOR      0x9F8C01
#define CICAM_AOT_COMMS_REPLY                0x9F8C02
#define CICAM_AOT_COMMS_SEND_LAST            0x9F8C03
#define CICAM_AOT_COMMS_SEND_MORE            0x9F8C04
#define CICAM_AOT_COMMS_RCV_LAST             0x9F8C05
#define CICAM_AOT_COMMS_RCV_MORE             0x9F8C06

typedef struct en50221_app_prop en50221_app_prop_t;
typedef struct en50221_app en50221_app_t;
typedef struct en50221_slot en50221_slot_t;
typedef struct en50221_slot_wmsg en50221_slot_wmsg_t;
typedef struct en50221_transport en50221_transport_t;
typedef struct en50221_ops en50221_ops_t;

struct en50221_app_prop {
  LIST_ENTRY(en50221_app_prop) ciap_link;
  const char *ciap_name;
  uint32_t ciap_resource_id;
  size_t ciap_struct_size;
  void (*ciap_destroy)(void *self);
  int (*ciap_open)(void *self);
  int (*ciap_handle)(void *self, uint8_t status, uint32_t atag,
                     const uint8_t *data, size_t datalen);
  int (*ciap_monitor)(void *self, int64_t clock);
};

struct en50221_app {
  LIST_ENTRY(en50221_app) cia_link;
  en50221_slot_t *cia_slot;
  char *cia_name;
  uint32_t cia_resource_id;
  uint16_t cia_session_id;
  en50221_app_prop_t *cia_prop;
};

struct en50221_slot_wmsg {
  TAILQ_ENTRY(en50221_slot_wmsg) link;
  uint8_t tag;
  uint8_t reply_is_expected;
  size_t len;
  uint8_t data[0];
};

struct en50221_slot {
  LIST_ENTRY(en50221_slot) cil_link;
  en50221_transport_t *cil_transport;
  char *cil_name;
  uint8_t cil_number;
  uint8_t cil_tcnum;
  uint8_t cil_enabled;
  uint8_t cil_ready;
  uint8_t cil_caid_list;
  uint8_t cil_apdu_only;
  uint8_t cil_initiate_connection;
  uint8_t cil_closing;
  uint16_t cil_last_session_number;
  int64_t cil_monitor_read;
  TAILQ_HEAD(,en50221_slot_wmsg) cil_write_queue;
  LIST_HEAD(, en50221_app) cil_apps;
};

struct en50221_transport {
  LIST_HEAD(, en50221_slot) cit_slots;
  en50221_ops_t *cit_ops;
  void *cit_ops_aux;
  uint8_t cit_broken;
  int cit_slot_count;
  char *cit_name;
  sbuf_t cit_rmsg;
};

struct en50221_ops {
  /* hw interface */
  int (*cihw_reset)(void *aux);
  int (*cihw_cam_is_ready)(void *aux, en50221_slot_t *slot);
  int (*cihw_pdu_write)(void *aux, en50221_slot_t *slot, uint8_t tcnum,
                        const uint8_t *data, size_t datalen);
  int (*cihw_apdu_write)(void *aux, en50221_slot_t *slot,
                         const uint8_t *data, size_t datalen);
  /* software level ops */
  int (*cisw_appinfo)(void *aux, en50221_slot_t *slot, uint8_t ver,
                      char *name, uint8_t type, uint16_t manufacturer, uint16_t code);
  int (*cisw_pcmcia_data_rate)(void *aux, en50221_slot_t *slot, uint8_t *rate);
  int (*cisw_caids)(void *aux, en50221_slot_t *slot, uint16_t *list, int listcount);
  int (*cisw_ca_close)(void *aux, en50221_slot_t *slot);
  int (*cisw_menu)(void *aux, en50221_slot_t *slot, htsmsg_t *menu);
  int (*cisw_enquiry)(void *aux, en50221_slot_t *slot, htsmsg_t *enq);
  int (*cisw_close)(void *aux, en50221_slot_t *slot, int delay);
};

/*
 *
 */
int en50221_create_transport(en50221_ops_t *ops, void *ops_aux, int slots,
                             const char *name, en50221_transport_t **cit);
void en50221_transport_destroy(en50221_transport_t *cit);
en50221_slot_t *en50221_transport_find_slot(en50221_transport_t *cit, uint8_t slotnum);
int en50221_transport_reset(en50221_transport_t *cit);
int en50221_transport_monitor(en50221_transport_t *cit, int64_t clock);
int en50221_transport_read(en50221_transport_t *cit,
                           uint8_t slotnum, uint8_t tcnum,
                           const uint8_t *data, size_t datalen);

/*
 *
 */
en50221_app_t *
en50221_slot_find_application(en50221_slot_t *cil,
                              uint32_t resource_id, uint32_t mask);
int en50221_slot_enable(en50221_slot_t *cil);
int en50221_slot_disable(en50221_slot_t *cil);

/*
 *
 */
void en50221_register_app(en50221_app_prop_t *prop);
int en50221_app_pdu_send(en50221_app_t *app, uint32_t atag,
                         const uint8_t *data, size_t datalen,
                         int reply_is_expected);
void en50221_register_apps(void);

#define CICAM_CALL_APP_CB(app, name, ...) ({ \
  en50221_slot_t *__s = app->cia_slot; \
  en50221_transport_t *__t = __s->cil_transport; \
  int __r = __t->cit_ops ? \
    __t->cit_ops->name(__t->cit_ops_aux, __s, ##__VA_ARGS__) : 0; \
  __r; \
})

/*
 *
 */
int en50221_extract_len
  (const uint8_t *data, size_t datalen, const uint8_t **ptr, size_t *len,
   const char *prefix, const char *pdu_name);

/*
 * random public functions
 */
int en50221_send_capmt
  (en50221_slot_t *slot, const uint8_t *capmt, size_t capmtlen);
int en50221_pcmcia_data_rate(en50221_slot_t *slot, uint8_t rate);
int en50221_mmi_answer
  (en50221_slot_t *slot, const uint8_t *data, size_t datalen);
int en50221_mmi_close(en50221_slot_t *slot);

#endif /* EN50221_H */
