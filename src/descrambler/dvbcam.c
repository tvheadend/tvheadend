/*
 *  tvheadend, DVB CAM interface
 *  Copyright (C) 2014 Damjan Marion
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

#include <ctype.h>
#include "tvheadend.h"
#include "caclient.h"
#include "descrambler.h"
#include "service.h"
#include "input.h"

#include "dvbcam.h"
#include "input/mpegts/linuxdvb/linuxdvb_private.h"

#if ENABLE_LINUXDVB_CA

#define CAIDS_PER_CA_SLOT	16

typedef struct dvbcam_active_service {
  TAILQ_ENTRY(dvbcam_active_service) link;
  service_t            *t;
  uint8_t              *last_pmt;
  int                  last_pmt_len;
  linuxdvb_ca_t        *ca;
  uint8_t              slot;
} dvbcam_active_service_t;

typedef struct dvbcam_active_cam {
  TAILQ_ENTRY(dvbcam_active_cam) link;
  uint16_t             caids[CAIDS_PER_CA_SLOT];
  int                  num_caids;
  linuxdvb_ca_t       *ca;
  uint8_t              slot;
  int                  active_programs;
} dvbcam_active_cam_t;

TAILQ_HEAD(,dvbcam_active_service) dvbcam_active_services;
TAILQ_HEAD(,dvbcam_active_cam) dvbcam_active_cams;

pthread_mutex_t dvbcam_mutex;

void
dvbcam_register_cam(linuxdvb_ca_t * lca, uint8_t slot, uint16_t * caids,
                    int num_caids)
{
  dvbcam_active_cam_t *ac;

  tvhtrace(LS_DVBCAM, "register cam ca %p slot %u num_caids %u",
           lca, slot, num_caids);

  num_caids = MIN(CAIDS_PER_CA_SLOT, num_caids);

  if ((ac = calloc(1, sizeof(*ac))) == NULL)
  	return;

  ac->ca = lca;
  ac->slot = slot;
  memcpy(ac->caids, caids, num_caids * sizeof(uint16_t));
  ac->num_caids = num_caids;

  pthread_mutex_lock(&dvbcam_mutex);

  TAILQ_INSERT_TAIL(&dvbcam_active_cams, ac, link);

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_unregister_cam(linuxdvb_ca_t * lca, uint8_t slot)
{
  dvbcam_active_cam_t *ac, *ac_tmp;
  dvbcam_active_service_t *as;

  tvhtrace(LS_DVBCAM, "unregister cam lca %p slot %u", lca, slot);

  pthread_mutex_lock(&dvbcam_mutex);

  /* remove pointer to this CAM in all active services */
  TAILQ_FOREACH(as, &dvbcam_active_services, link)
    if (as->ca == lca && as->slot == slot)
      as->ca = NULL;

  /* delete entry */
  for (ac = TAILQ_FIRST(&dvbcam_active_cams); ac != NULL; ac = ac_tmp) {
    ac_tmp = TAILQ_NEXT(ac, link);
    if(ac && ac->ca == lca && ac->slot == slot) {
      TAILQ_REMOVE(&dvbcam_active_cams, ac, link);
      free(ac);
    }
  }

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_pmt_data(mpegts_service_t *s, const uint8_t *ptr, int len)
{
  linuxdvb_frontend_t *lfe;
  dvbcam_active_cam_t *ac = NULL, *ac2;
  dvbcam_active_service_t *as = NULL, *as2;
  elementary_stream_t *st;
  caid_t *c;
  uint8_t list_mgmt;
  int is_update = 0;
  int i;

  lfe = (linuxdvb_frontend_t*) s->s_dvb_active_input;

  if (!lfe)
    return;

  pthread_mutex_lock(&dvbcam_mutex);

  /* find this service in the list of active services */
  TAILQ_FOREACH(as2, &dvbcam_active_services, link)
    if (as2->t == (service_t *) s) {
      as = as2;
      break;
    }

  if (!as) {
    tvhtrace(LS_DVBCAM, "cannot find active service entry");
  	goto done;
  }

  if(as->last_pmt) {
    free(as->last_pmt);
    is_update = 1;
  }

  as->last_pmt = malloc(len);
  memcpy(as->last_pmt, ptr, len);
  as->last_pmt_len = len;

  /*if this is update just send updated CAPMT to CAM */
  if (is_update) {

    tvhtrace(LS_DVBCAM, "CAPMT sent to CAM (update)");
    list_mgmt = CA_LIST_MANAGEMENT_UPDATE;
    goto enqueue;
  }

  /* check all ellementary streams for CAIDs and find CAM */
  TAILQ_FOREACH(st, &s->s_components, es_link) {
    LIST_FOREACH(c, &st->es_caids, link) {
      TAILQ_FOREACH(ac2, &dvbcam_active_cams, link) {
        for(i=0;i<ac2->num_caids;i++) {
          if(ac2->ca && ac2->ca->lca_adapter == lfe->lfe_adapter &&
             ac2->caids[i] == c->caid)
          {
            as->ca = ac2->ca;
            as->slot = ac2->slot;
            ac = ac2;
            goto end_of_search_for_cam;
          }
        }
      }
    }
  }

end_of_search_for_cam:

  if (!ac) {
    tvhtrace(LS_DVBCAM, "cannot find active cam entry");
    goto done;
  }
  tvhtrace(LS_DVBCAM, "found active cam entry");

  if (ac->active_programs++)
    list_mgmt = CA_LIST_MANAGEMENT_ADD;
  else
    list_mgmt = CA_LIST_MANAGEMENT_ONLY;

enqueue:
  if (as->ca) {
    pthread_mutex_lock(&s->s_stream_mutex);
    descrambler_external((service_t *)s, 1);
    pthread_mutex_unlock(&s->s_stream_mutex);
    linuxdvb_ca_enqueue_capmt(as->ca, as->slot, as->last_pmt, as->last_pmt_len,
                              list_mgmt, CA_PMT_CMD_ID_OK_DESCRAMBLING);
  }
done:
  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_service_start(service_t *t)
{
  dvbcam_active_service_t *as;

  tvhtrace(LS_DVBCAM, "start service %p", t);

  TAILQ_FOREACH(as, &dvbcam_active_services, link)
    if (as->t == t)
      return;

  if ((as = calloc(1, sizeof(*as))) == NULL)
    return;

  as->t = t;
  as->last_pmt = NULL;
  as->last_pmt_len = 0;

  pthread_mutex_lock(&dvbcam_mutex);
  TAILQ_INSERT_TAIL(&dvbcam_active_services, as, link);
  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_service_stop(service_t *t)
{
  dvbcam_active_service_t *as, *as_tmp;
  linuxdvb_ca_t *ca = NULL;
  dvbcam_active_cam_t *ac2;
  uint8_t slot = -1;

  tvhtrace(LS_DVBCAM, "stop service %p", t);

  pthread_mutex_lock(&dvbcam_mutex);

  for (as = TAILQ_FIRST(&dvbcam_active_services); as != NULL; as = as_tmp) {
    as_tmp = TAILQ_NEXT(as, link);
    if(as && as->t == t) {
      if(as->last_pmt) {
        linuxdvb_ca_enqueue_capmt(as->ca, as->slot, as->last_pmt,
                                  as->last_pmt_len,
                                  CA_LIST_MANAGEMENT_UPDATE,
                                  CA_PMT_CMD_ID_NOT_SELECTED);
        free(as->last_pmt);
      }
      slot = as->slot;
      ca = as->ca;
      TAILQ_REMOVE(&dvbcam_active_services, as, link);
      free(as);
    }
  }

  if (ca)
    TAILQ_FOREACH(ac2, &dvbcam_active_cams, link)
      if (ac2->slot == slot && ac2->ca == ca) {
        ac2->active_programs--;
      }

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_init(void)
{
  pthread_mutex_init(&dvbcam_mutex, NULL);
  TAILQ_INIT(&dvbcam_active_services);
  TAILQ_INIT(&dvbcam_active_cams);
}

#endif /* ENABLE_LINUXDVB_CA */
