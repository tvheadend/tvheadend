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

typedef struct dvbcam_active_caid {
  TAILQ_ENTRY(dvbcam_active_caid) link;
  uint16_t             caids[CAIDS_PER_CA_SLOT];
  int                  num_caids;
  linuxdvb_ca_t       *ca;
  uint8_t              slot;
} dvbcam_active_caid_t;

TAILQ_HEAD(,dvbcam_active_service) dvbcam_active_services;
TAILQ_HEAD(,dvbcam_active_caid) dvbcam_active_caids;

pthread_mutex_t dvbcam_mutex;

void
dvbcam_register_cam(linuxdvb_ca_t * lca, uint8_t slot, uint16_t * caids,
                    int num_caids)
{
  dvbcam_active_caid_t *ac;

  num_caids = MIN(CAIDS_PER_CA_SLOT, num_caids);

  if ((ac = malloc(sizeof(*ac))) == NULL)
  	return;

  ac->ca = lca;
  ac->slot = slot;
  memcpy(ac->caids, caids, num_caids * sizeof(uint16_t));
  ac->num_caids = num_caids;

  pthread_mutex_lock(&dvbcam_mutex);

  TAILQ_INSERT_TAIL(&dvbcam_active_caids, ac, link);

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_unregister_cam(linuxdvb_ca_t * lca, uint8_t slot)
{
  dvbcam_active_caid_t *ac, *ac_tmp;
  dvbcam_active_service_t *as;

  pthread_mutex_lock(&dvbcam_mutex);

  /* remove pointer to this CAM in all active services */
  TAILQ_FOREACH(as, &dvbcam_active_services, link)
    if (as->ca == lca && as->slot == slot)
      as->ca = NULL;

  /* delete entry */
  for (ac = TAILQ_FIRST(&dvbcam_active_caids); ac != NULL; ac = ac_tmp) {
    ac_tmp = TAILQ_NEXT(ac, link);
    if(ac && ac->ca == lca && ac->slot == slot) {
      TAILQ_REMOVE(&dvbcam_active_caids, ac, link);
      free(ac);
    }
  }

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_pmt_data(mpegts_service_t *s, const uint8_t *ptr, int len)
{
  linuxdvb_frontend_t *lfe;
  dvbcam_active_caid_t *ac;
  dvbcam_active_service_t *as = NULL, *as2;
  elementary_stream_t *st;
  caid_t *c;
  int i;

	lfe = (linuxdvb_frontend_t*) s->s_dvb_active_input;

  if (!lfe)
    return;

  pthread_mutex_lock(&dvbcam_mutex);

  TAILQ_FOREACH(as2, &dvbcam_active_services, link)
    if (as2->t == (service_t *) s) {
      as = as2;
      break;
    }

  pthread_mutex_unlock(&dvbcam_mutex);

  if (!as)
  	return;

  if(as->last_pmt)
    free(as->last_pmt);

  as->last_pmt = malloc(len + 3);
  memcpy(as->last_pmt, ptr-3, len + 3);
  as->last_pmt_len = len + 3;
  as->ca = NULL;

  pthread_mutex_lock(&dvbcam_mutex);

  /* check all ellementary streams for CAIDs, if any send PMT to CAM */
  TAILQ_FOREACH(st, &s->s_components, es_link) {
    LIST_FOREACH(c, &st->es_caids, link) {
      TAILQ_FOREACH(ac, &dvbcam_active_caids, link) {
        for(i=0;i<ac->num_caids;i++) {
          if(ac->ca && ac->ca->lca_adapter == lfe->lfe_adapter &&
             ac->caids[i] == c->caid)
          {
            as->ca = ac->ca;
            as->slot = ac->slot;
            break;
          }
        }
      }
    }
  }

  pthread_mutex_unlock(&dvbcam_mutex);

  /* this service doesn't have assigned CAM */
  if (!as->ca)
    return;

  linuxdvb_ca_send_capmt(as->ca, as->slot, as->last_pmt, as->last_pmt_len);
}

void
dvbcam_service_start(service_t *t)
{
  dvbcam_active_service_t *as;

  TAILQ_FOREACH(as, &dvbcam_active_services, link)
    if (as->t == t)
      return;

  if ((as = malloc(sizeof(*as))) == NULL)
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

  pthread_mutex_lock(&dvbcam_mutex);

  for (as = TAILQ_FIRST(&dvbcam_active_services); as != NULL; as = as_tmp) {
    as_tmp = TAILQ_NEXT(as, link);
    if(as && as->t == t) {
      TAILQ_REMOVE(&dvbcam_active_services, as, link);
      if(as->last_pmt)
        free(as->last_pmt);
      free(as);
    }
  }

  pthread_mutex_unlock(&dvbcam_mutex);
}

void
dvbcam_init(void)
{
  pthread_mutex_init(&dvbcam_mutex, NULL);
  TAILQ_INIT(&dvbcam_active_services);
  TAILQ_INIT(&dvbcam_active_caids);
}

#endif /* ENABLE_LINUXDVB_CA */
