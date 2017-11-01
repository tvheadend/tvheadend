/*
 *  tvheadend, DVB CAM interface
 *  Copyright (C) 2014 Damjan Marion
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

typedef struct dvbcam_active_cam {
  TAILQ_ENTRY(dvbcam_active_cam) global_link;
  uint16_t             caids[CAIDS_PER_CA_SLOT];
  int                  num_caids;
  linuxdvb_ca_t       *ca;
  uint8_t              slot;
  int                  active_programs;
} dvbcam_active_cam_t;

typedef struct dvbcam_active_service {
  th_descrambler_t;
  TAILQ_ENTRY(dvbcam_active_service) global_link;
  LIST_ENTRY(dvbcam_active_service)  dvbcam_link;
  uint8_t             *last_pmt;
  int                  last_pmt_len;
  uint16_t             caid;
  dvbcam_active_cam_t *ac;
} dvbcam_active_service_t;

typedef struct dvbcam {
  caclient_t;
  LIST_HEAD(,dvbcam_active_service) services;
  int limit;
} dvbcam_t;

TAILQ_HEAD(,dvbcam_active_service) dvbcam_active_services;
TAILQ_HEAD(,dvbcam_active_cam) dvbcam_active_cams;

pthread_mutex_t dvbcam_mutex;

/*
 *
 */
static void
dvbcam_status_update0(caclient_t *cac)
{
  int status = CACLIENT_STATUS_NONE;

  if (TAILQ_FIRST(&dvbcam_active_cams))
    status = CACLIENT_STATUS_CONNECTED;
  caclient_set_status(cac, status);
}

/*
 *
 */
static void
dvbcam_status_update(void)
{
  caclient_foreach(dvbcam_status_update0);
}

/*
 *
 */
void
dvbcam_register_cam(linuxdvb_ca_t * lca, uint8_t slot, uint16_t * caids,
                    int num_caids)
{
  dvbcam_active_cam_t *ac, *ac_first;

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

  ac_first = TAILQ_FIRST(&dvbcam_active_cams);
  TAILQ_INSERT_TAIL(&dvbcam_active_cams, ac, global_link);

  if (ac_first == NULL)
    dvbcam_status_update();

  pthread_mutex_unlock(&dvbcam_mutex);

}

/*
 *
 */
void
dvbcam_unregister_cam(linuxdvb_ca_t * lca, uint8_t slot)
{
  dvbcam_active_cam_t *ac, *ac_next;
  dvbcam_active_service_t *as;

  tvhtrace(LS_DVBCAM, "unregister cam lca %p slot %u", lca, slot);

  pthread_mutex_lock(&dvbcam_mutex);

  /* delete entry */
  for (ac = TAILQ_FIRST(&dvbcam_active_cams); ac != NULL; ac = ac_next) {
    ac_next = TAILQ_NEXT(ac, global_link);
    if (ac->ca == lca && ac->slot == slot) {
      TAILQ_REMOVE(&dvbcam_active_cams, ac, global_link);
      /* remove pointer to this CAM in all active services */
      TAILQ_FOREACH(as, &dvbcam_active_services, global_link)
        if (as->ac == ac)
          as->ac = NULL;
      free(ac);
    }
  }

  if (TAILQ_EMPTY(&dvbcam_active_cams))
    dvbcam_status_update();

  pthread_mutex_unlock(&dvbcam_mutex);
}

/*
 *
 */
static int
dvbcam_ca_lookup(dvbcam_active_cam_t *ac, mpegts_input_t *input, uint16_t caid)
{
  extern const idclass_t linuxdvb_frontend_class;
  linuxdvb_frontend_t *lfe = NULL;
  int i;

  if (ac->ca == NULL)
    return 0;

  if (idnode_is_instance(&input->ti_id, &linuxdvb_frontend_class))
    lfe = (linuxdvb_frontend_t*)input;

  if (lfe == NULL || ac->ca->lca_adapter != lfe->lfe_adapter)
    return 0;

  for (i = 0; i < ac->num_caids; i++)
    if (ac->caids[i] == caid)
      return 1;

  return 0;
}

/*
 *
 */
void
dvbcam_pmt_data(mpegts_service_t *s, const uint8_t *ptr, int len)
{
  dvbcam_active_cam_t *ac;
  dvbcam_active_service_t *as;
  uint8_t list_mgmt;
  int is_update = 0;

  pthread_mutex_lock(&s->s_stream_mutex);
  pthread_mutex_lock(&dvbcam_mutex);

  /* find this service in the list of active services */
  TAILQ_FOREACH(as, &dvbcam_active_services, global_link)
    if (as->td_service == (service_t *)s)
      break;

  if (as == NULL) {
    tvhtrace(LS_DVBCAM, "cannot find active service entry");
    goto done;
  }

  ac = as->ac;
  if (!ac) {
    tvhtrace(LS_DVBCAM, "cannot find active cam entry");
    goto done;
  }

  if (as->last_pmt) {
    /* exact match - do nothing */
    if (as->last_pmt_len == len && memcmp(as->last_pmt, ptr, len) == 0)
      goto done;
    free(as->last_pmt);
    is_update = 1;
  }

  as->last_pmt = malloc(len);
  memcpy(as->last_pmt, ptr, len);
  as->last_pmt_len = len;

  /* if this is update just send updated CAPMT to CAM */
  if (is_update) {
    tvhtrace(LS_DVBCAM, "CAPMT sent to CAM (update)");
    list_mgmt = CA_LIST_MANAGEMENT_UPDATE;
  } else {
    list_mgmt = ac->active_programs ? CA_LIST_MANAGEMENT_ADD :
                                    CA_LIST_MANAGEMENT_ONLY;
    ac->active_programs++;
  }

  descrambler_external((service_t *)s, 1);
  linuxdvb_ca_enqueue_capmt(ac->ca, ac->slot, as->last_pmt, as->last_pmt_len,
                            list_mgmt, CA_PMT_CMD_ID_OK_DESCRAMBLING);
done:
  pthread_mutex_unlock(&dvbcam_mutex);
  pthread_mutex_unlock(&s->s_stream_mutex);
}

static void
dvbcam_service_destroy(th_descrambler_t *td)
{
  dvbcam_active_service_t *as = (dvbcam_active_service_t *)td;
  dvbcam_active_cam_t *ac;
  int do_active_programs = 0;

  pthread_mutex_lock(&dvbcam_mutex);
  ac = as->ac;
  if (as->last_pmt) {
    if (ac)
      linuxdvb_ca_enqueue_capmt(ac->ca, ac->slot, as->last_pmt,
                                as->last_pmt_len,
                                CA_LIST_MANAGEMENT_UPDATE,
                                CA_PMT_CMD_ID_NOT_SELECTED);
    free(as->last_pmt);
    do_active_programs = 1;
  }
  LIST_REMOVE(as, dvbcam_link);
  LIST_REMOVE(td, td_service_link);
  TAILQ_REMOVE(&dvbcam_active_services, as, global_link);
  if (do_active_programs) {
    TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link)
      if (as->ac == ac)
        ac->active_programs--;
  }
  pthread_mutex_unlock(&dvbcam_mutex);
}

static void
dvbcam_service_start(caclient_t *cac, service_t *t)
{
  dvbcam_t *dc = (dvbcam_t *)cac;
  dvbcam_active_service_t *as;
  dvbcam_active_cam_t *ac = NULL;
  th_descrambler_t *td;
  elementary_stream_t *st;
  caid_t *c;
  int count = 0;
  char buf[128];

  if (!cac->cac_enabled)
    return;

  tvhtrace(LS_DVBCAM, "start service %p", t);

  pthread_mutex_lock(&t->s_stream_mutex);
  pthread_mutex_lock(&dvbcam_mutex);

  TAILQ_FOREACH(as, &dvbcam_active_services, global_link) {
    if (as->td_service == t)
      goto end;
    count++;
  }

  if (dc->limit > 0 && dc->limit <= count)
    goto end;

  /* check all elementary streams for CAIDs and find CAM */
  TAILQ_FOREACH(st, &t->s_filt_components, es_link)
    LIST_FOREACH(c, &st->es_caids, link)
      if (c->use)
        TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link)
          if (dvbcam_ca_lookup(ac, ((mpegts_service_t *)t)->s_dvb_active_input, c->caid))
            goto end_of_search_for_cam;

end_of_search_for_cam:

  if (ac == NULL)
    goto end;

  if ((as = calloc(1, sizeof(*as))) == NULL)
    goto end;

  as->ac = ac;
  as->caid = c->caid;

  td = (th_descrambler_t *)as;
  snprintf(buf, sizeof(buf), "dvbcam-%i-%i-%04X",
           ac->ca->lca_number, (int)ac->slot, (int)as->caid);
  td->td_nicename = strdup(buf);
  td->td_service = t;
  td->td_stop = dvbcam_service_destroy;
  descrambler_change_keystate(td, DS_READY, 0);

  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&dc->services, as, dvbcam_link);
  TAILQ_INSERT_TAIL(&dvbcam_active_services, as, global_link);

end:
  pthread_mutex_unlock(&dvbcam_mutex);
  pthread_mutex_unlock(&t->s_stream_mutex);
}

/*
 *
 */
static void
dvbcam_free(caclient_t *cac)
{
}

/*
 *
 */
static void
dvbcam_caid_update(caclient_t *cac, mpegts_mux_t *mux, uint16_t caid, uint16_t pid, int valid)
{

}

/**
 *
 */
static void
dvbcam_conf_changed(caclient_t *cac)
{
}

/*
 *
 */
const idclass_t caclient_dvbcam_class =
{
  .ic_super      = &caclient_class,
  .ic_class      = "caclient_dvbcam",
  .ic_caption    = N_("Linux DVB CAM Client"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "limit",
      .name     = N_("Service limit"),
      .desc     = N_("Limit of concurrent descrambled services (total)."),
      .off      = offsetof(dvbcam_t, limit),
    },
    {}
  }
};

/*
 *
 */
caclient_t *dvbcam_create(void)
{
  dvbcam_t *dc = calloc(1, sizeof(*dc));

  dc->cac_free         = dvbcam_free;
  dc->cac_start        = dvbcam_service_start;
  dc->cac_conf_changed = dvbcam_conf_changed;
  dc->cac_caid_update  = dvbcam_caid_update;
  return (caclient_t *)dc;
}

/*
 *
 */
void
dvbcam_init(void)
{
  pthread_mutex_init(&dvbcam_mutex, NULL);
  TAILQ_INIT(&dvbcam_active_services);
  TAILQ_INIT(&dvbcam_active_cams);
}

#endif /* ENABLE_LINUXDVB_CA */
