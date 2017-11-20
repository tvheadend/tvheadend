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
#include "input/mpegts/tsdemux.h"
#include "input/mpegts/linuxdvb/linuxdvb_private.h"

#if ENABLE_LINUXDVB_CA

#define CAIDS_PER_CA_SLOT   16
#define MAX_ECM_PIDS        16   // max opened ECM PIDs

typedef struct dvbcam_active_cam {
  TAILQ_ENTRY(dvbcam_active_cam) global_link;
  uint16_t             caids[CAIDS_PER_CA_SLOT];
  int                  num_caids;
  linuxdvb_ca_t       *ca;
  uint8_t              slot;
  int                  active_programs;
} dvbcam_active_cam_t;

typedef struct dvbcam_ecm_pids {
  uint16_t             pids[MAX_ECM_PIDS];
  int                  num_pids;
} dvbcam_ecm_pids_t;

typedef struct dvbcam_active_service {
  th_descrambler_t;
  TAILQ_ENTRY(dvbcam_active_service) global_link;
  LIST_ENTRY(dvbcam_active_service)  dvbcam_link;
  uint8_t             *last_pmt;
  int                  last_pmt_len;
  uint16_t             caid;
  dvbcam_active_cam_t *ac;
  dvbcam_ecm_pids_t    ecm_open;
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

#if ENABLE_DDCI
static void
dvbcam_unregister_ddci(dvbcam_active_cam_t *ac, dvbcam_active_service_t *as)
{
  if (ac && ac->ca->lddci) {
    th_descrambler_t *td = (th_descrambler_t *)as;
    service_t *t = td->td_service;
    th_descrambler_runtime_t *dr = t->s_descramble;

    /* unassign the service from the DD CI CAM */
    linuxdvb_ddci_assign(ac->ca->lddci, NULL);
    if (dr) {
      dr->dr_descrambler = NULL;
      dr->dr_descramble = NULL;
    }
  }
}

int
dvbcam_is_ddci(struct service *t)
{
  th_descrambler_runtime_t  *dr = t->s_descramble;
  int ret = 0;

  if (dr) {
    dvbcam_active_service_t  *as = (dvbcam_active_service_t *)dr->dr_descrambler;

    if (as && as->ac) {
      linuxdvb_ddci_t        *lddci = as->ac->ca->lddci;
      ret = lddci != NULL;
    }
  }
  return ret;
}
#endif

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
        if (as->ac == ac) {
#if ENABLE_DDCI
          dvbcam_unregister_ddci(ac, as);
#endif
          as->ac = NULL;
        }
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

#if ENABLE_DDCI
  if (!ac->ca->lddci)
#endif
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

#if ENABLE_DDCI
  dvbcam_unregister_ddci(ac, as);
#endif

  LIST_REMOVE(as, dvbcam_link);
  LIST_REMOVE(td, td_service_link);
  TAILQ_REMOVE(&dvbcam_active_services, as, global_link);
  if (do_active_programs) {
    TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link)
      if (as->ac == ac)
        ac->active_programs--;
  }
  free(as);
  pthread_mutex_unlock(&dvbcam_mutex);
}

#if ENABLE_DDCI
static int
dvbcam_descramble_ddci(service_t *t, elementary_stream_t *st, const uint8_t *tsb, int len)
{
  th_descrambler_runtime_t  *dr = ((mpegts_service_t *)t)->s_descramble;
  dvbcam_active_service_t   *as = (dvbcam_active_service_t *)dr->dr_descrambler;

  if (as->ac != NULL)
    linuxdvb_ddci_put(as->ac->ca->lddci, tsb, len);

  return 1;
}

static void
dvbcam_ecm_pid_update
  (dvbcam_active_service_t *as, service_t *t, dvbcam_ecm_pids_t *new_ecm_pids)
{
  mpegts_mux_t *mm;
  mpegts_input_t *mi;
  int i, j, pid;
  dvbcam_ecm_pids_t ecm_pids_to_open;
  dvbcam_ecm_pids_t ecm_pids_to_close;

  if (new_ecm_pids->num_pids == 0) return;

  /* due to the mutex lock order, we need two helper arrays
   * to be filled with locked dvbcam mutex and executed at the end with
   * unlocked dvbcam  mutex
   */
  memset(&ecm_pids_to_open, 0, sizeof(ecm_pids_to_open));
  memset(&ecm_pids_to_close, 0, sizeof(ecm_pids_to_close));

  pthread_mutex_lock(&dvbcam_mutex);

  for (i = 0; i < new_ecm_pids->num_pids; i++) {
    pid = new_ecm_pids->pids[i];

    /* Clear out already opened PIDs, so that they don't get closed in
     * the next step
     */
    for (j = 0; j < as->ecm_open.num_pids; j++)
      if (as->ecm_open.pids[j] == pid) {
        as->ecm_open.pids[j] = 0;
        break;
      }

    /* Not found -> New PID */
    if (j == as->ecm_open.num_pids)
      ecm_pids_to_open.pids[ecm_pids_to_open.num_pids++] = pid;
  }

  /* close old PIDs (list contains only no longer used PIDs) */
  for (i = 0; i < as->ecm_open.num_pids; i++) {
    pid = as->ecm_open.pids[i];
    if (pid)
      ecm_pids_to_close.pids[ecm_pids_to_close.num_pids++] = pid;
  }

  /* save new open PIDs list */
  as->ecm_open = *new_ecm_pids;

  pthread_mutex_unlock(&dvbcam_mutex);

  mm = ((mpegts_service_t *)t)->s_dvb_mux;;
  mi = mm->mm_active ? mm->mm_active->mmi_input : NULL;
  if (mi) {
    pthread_mutex_lock(&mi->mi_output_lock);
    pthread_mutex_lock(&t->s_stream_mutex);

    for (i = 0; i < ecm_pids_to_open.num_pids; i++)
      mpegts_input_open_pid(mi, mm, ecm_pids_to_open.pids[i], MPS_SERVICE,
                           MPS_WEIGHT_CAT, t, 0);
    for (i = 0; i < ecm_pids_to_close.num_pids; i++)
      mpegts_input_close_pid(mi, mm, ecm_pids_to_close.pids[i], MPS_SERVICE,
                             MPS_WEIGHT_CAT, t);

    pthread_mutex_unlock(&t->s_stream_mutex);
    pthread_mutex_unlock(&mi->mi_output_lock);
  }
}

#if 0
static void
dvbcam_caid_change_ddci(th_descrambler_t *td)
{
  tvhwarning(LS_DVBCAM, "FIXME: implement dvbcam_caid_change_ddci");

}
#endif

#endif /* ENABLE_DDCI */

/*
 * This routine is called from two places
 * a) start a new service
 * b) restart a running service with possible caid changes
 */
static void
dvbcam_service_start(caclient_t *cac, service_t *t)
{
  dvbcam_t *dc = (dvbcam_t *)cac;
  dvbcam_active_service_t *as;
  dvbcam_active_cam_t *ac = NULL;
  th_descrambler_t *td;
  elementary_stream_t *st;
  th_descrambler_runtime_t *dr;
  caid_t *c = NULL;
  char buf[128];
#if ENABLE_DDCI
  mpegts_input_t *mi;
  mpegts_mux_t *mm;
  int ddci_cam = 0;
  dvbcam_ecm_pids_t new_ecm_pids;
#endif

  if (!cac->cac_enabled)
    return;

  tvhtrace(LS_DVBCAM, "start service %p", t);

#if ENABLE_DDCI
  memset(&new_ecm_pids,0,sizeof(new_ecm_pids));
#endif

  pthread_mutex_lock(&t->s_stream_mutex);
  pthread_mutex_lock(&dvbcam_mutex);

  /* is there already a CAM associated to the service? */
  TAILQ_FOREACH(as, &dvbcam_active_services, global_link) {
    if (as->td_service == t)
      goto update_pid;
  }

  /* FIXME: The limit check needs to be done per CAM instance.
   *        Have to check if dvbcam_t struct is created per CAM
   *        or once per class before we can implement this correctly.

   * FIXME: This might be removed or implemented differently in case of
   *        MCD/MTD. VDR asks the CAM with a query if the CAM can decode another
   *        PID.
   *        Note: A CAM has decoding slots, which are used up by each PID which
   *              gets decoded. This are Audio, Video, Teletext, ..., depending
   *              of the decoded program this can be more or less PIDs,
   *              resulting in more or less occupied CAM decoding slots. So the
   *              simple limit approach might not work or some decoding slots
   *              remain unused.

  if (dc->limit > 0 && dc->limit <= count)
    goto end;

  */

  /* check all elementary streams for CAIDs and find CAM */
  TAILQ_FOREACH(st, &t->s_filt_components, es_link)
    LIST_FOREACH(c, &st->es_caids, link)
      if (c->use)
        TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link)
          if (dvbcam_ca_lookup(ac, ((mpegts_service_t *)t)->s_dvb_active_input, c->caid)) {
            /* FIXME: The limit check needs to be per CAM, so we need to find
             *        another CAM if the fund one is on limit.
             */

#if ENABLE_DDCI
            /* currently we allow only one service per DD CI */
            if (ac->ca->lddci && linuxdvb_ddci_is_assigned(ac->ca->lddci)) {
              continue;
            }
#endif
            goto end_of_search_for_cam;
          }

  end_of_search_for_cam:
  if (ac == NULL) {
    service_set_streaming_status_flags(t, TSS_NO_DESCRAMBLER);
    goto end;
  }

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
  dr = t->s_descramble;
  dr->dr_descrambler = td;
  dr->dr_descramble = descrambler_pass;
#if ENABLE_DDCI
  if (ac->ca->lddci) {
    /* assign the service to the DD CI CAM */
    linuxdvb_ddci_assign(ac->ca->lddci, t);
    dr->dr_descramble = dvbcam_descramble_ddci;

    /* add ECM handler */
    // td->td_caid_change = dvbcam_caid_change_ddci;

    ddci_cam = 1;
  }
#endif
  descrambler_change_keystate(td, DS_READY, 0);

  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&dc->services, as, dvbcam_link);
  TAILQ_INSERT_TAIL(&dvbcam_active_services, as, global_link);

update_pid:
#if ENABLE_DDCI
  /* open all ECM PIDs */
  TAILQ_FOREACH(st, &t->s_filt_components, es_link)
    LIST_FOREACH(c, &st->es_caids, link) {
      if (c->use == 0) continue;
      if (st->es_type != SCT_CA) continue;
      if (as->caid != c->caid) continue;
      if (!DESCRAMBLER_ECM_PID(st->es_pid)) continue;
      if (new_ecm_pids.num_pids < MAX_ECM_PIDS) {
        new_ecm_pids.pids[new_ecm_pids.num_pids++] = st->es_pid;
      }
      else
        tvhwarn(LS_DVBCAM, "Table for new ECM PIDs too small!");
  }
#endif

end:
  pthread_mutex_unlock(&dvbcam_mutex);
  pthread_mutex_unlock(&t->s_stream_mutex);

#if ENABLE_DDCI

  dvbcam_ecm_pid_update(as, t, &new_ecm_pids);

  if (ddci_cam) {
    mm = ((mpegts_service_t *)t)->s_dvb_mux;
    mi = mm->mm_active ? mm->mm_active->mmi_input : NULL;
    if (mi) {
      pthread_mutex_lock(&mi->mi_output_lock);
      pthread_mutex_lock(&t->s_stream_mutex);
      mpegts_input_open_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, MPS_WEIGHT_CAT, t, 0);
      ((mpegts_service_t *)t)->s_cat_opened = 1;
      pthread_mutex_unlock(&t->s_stream_mutex);
      pthread_mutex_unlock(&mi->mi_output_lock);
      mpegts_input_open_cat_monitor(mm, (mpegts_service_t *)t);
      mpegts_mux_update_pids(mm);
    }
  }
#endif
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
