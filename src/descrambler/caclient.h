/*
 *  tvheadend, Conditional Access Client
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef __TVH_CACLIENT_H__
#define __TVH_CACLIENT_H__

#include "tvheadend.h"
#include "idnode.h"

struct service;
struct mpegts_mux;

extern const idclass_t caclient_class;
extern const idclass_t caclient_dvbcam_class;
extern const idclass_t caclient_cc_class;
extern const idclass_t caclient_cwc_class;
extern const idclass_t caclient_cccam_class;
extern const idclass_t caclient_capmt_class;
extern const idclass_t caclient_ccw_csa_cbc_class;
extern const idclass_t caclient_ccw_des_ncb_class;
extern const idclass_t caclient_ccw_aes_ecb_class;
extern const idclass_t caclient_ccw_aes128_ecb_class;

TAILQ_HEAD(caclient_entry_queue, caclient);

extern struct caclient_entry_queue caclients;

extern const idclass_t *caclient_classes[];

typedef enum caclient_status {
  CACLIENT_STATUS_NONE,
  CACLIENT_STATUS_READY,
  CACLIENT_STATUS_CONNECTED,
  CACLIENT_STATUS_DISCONNECTED
} caclient_status_t;

typedef struct caclient {
  idnode_t cac_id;
  TAILQ_ENTRY(caclient) cac_link;

  int cac_save;
  int cac_index;
  int cac_enabled;
  char *cac_name;
  char *cac_comment;
  int cac_status;

  void (*cac_free)(struct caclient *cac);
  void (*cac_start)(struct caclient *cac, struct service *t);
  void (*cac_conf_changed)(struct caclient *cac);
  void (*cac_cat_update)(struct caclient *cac,
                         struct mpegts_mux *mux,
                         const uint8_t *data, int len);
  void (*cac_caid_update)(struct caclient *cac,
                          struct mpegts_mux *mux,
                          uint16_t caid, uint32_t prov,
                          uint16_t pid, int valid);
} caclient_t;

caclient_t *caclient_create
  (const char *uuid, htsmsg_t *conf, int save);

void caclient_start( struct service *t );
void caclient_caid_update(struct mpegts_mux *mux,
                          uint16_t caid, uint32_t prov,
                          uint16_t pid, int valid);
void caclient_cat_update(struct mpegts_mux *mux,
                         const uint8_t *data, int len);

void caclient_set_status(caclient_t *cac, caclient_status_t status);
const char *caclient_get_status(caclient_t *cac);

void caclient_foreach(void (*cb)(caclient_t *));

void caclient_init(void);
void caclient_done(void);

void tsdebugcw_service_start(struct service *t);
void tsdebugcw_new_keys(struct service *t, int type, uint16_t pid, uint8_t *odd, uint8_t *even);
void tsdebugcw_go(void);
void tsdebugcw_init(void);

caclient_t *dvbcam_create(void);
caclient_t *cwc_create(void);
caclient_t *cccam_create(void);
caclient_t *capmt_create(void);
caclient_t *constcw_create(void);
caclient_t *tsdebugcw_create(void);

#endif /* __TVH_CACLIENT_H__ */
