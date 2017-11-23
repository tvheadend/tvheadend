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

#define DVBCAM_SEL_ALL      0
#define DVBCAM_SEL_FIRST    1
#define DVBCAM_SEL_LAST     2

#define CAIDS_PER_CA_SLOT   16

typedef struct dvbcam_active_cam {
  TAILQ_ENTRY(dvbcam_active_cam) global_link;
  uint16_t             caids[CAIDS_PER_CA_SLOT];
  int                  num_caids;
  linuxdvb_ca_t       *ca;
  uint8_t              slot;
  int                  active_programs;
  int                  allocated_programs;
} dvbcam_active_cam_t;

typedef struct dvbcam_active_service {
  th_descrambler_t;
  TAILQ_ENTRY(dvbcam_active_service) global_link;
  LIST_ENTRY(dvbcam_active_service)  dvbcam_link;
  uint8_t             *last_pmt;
  int                  last_pmt_len;
  uint16_t             caid_list[32];
  dvbcam_active_cam_t *ac;
  mpegts_apids_t       ecm_pids;
  uint8_t             *cat_data;
  int                  cat_data_len;
  mpegts_apids_t       cat_pids;
#if ENABLE_DDCI
  int                  is_ddci;
#endif
} dvbcam_active_service_t;

typedef struct dvbcam {
  caclient_t;
  LIST_HEAD(,dvbcam_active_service) services;
  int limit;
  int caid_select;
  uint16_t caid_list[32];
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
#if ENABLE_DDCI
/*
 *
 */
static int
dvbcam_service_check_caid(dvbcam_active_service_t *as, uint16_t caid)
{
  uint16_t caid1;
  int i;

  for (i = 0; i < ARRAY_SIZE(as->caid_list); i++) {
    caid1 = as->caid_list[i];
    if (caid1 == 0)
      return 0;
    if (caid1 == caid)
      return 1;
  }
  return 0;
}

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
  TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link) {
    if (as->ac == ac) {
      if (do_active_programs)
        ac->active_programs--;
      ac->allocated_programs--;
      break;
    }
  }
  mpegts_pid_done(&as->ecm_pids);
  mpegts_pid_done(&as->cat_pids);
  free(as->cat_data);
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
  int i, j;
#if ENABLE_DDCI
  mpegts_input_t *mi;
  mpegts_mux_t *mm;
  mpegts_apids_t ecm_pids;
  mpegts_apids_t ecm_to_open;
  mpegts_apids_t ecm_to_close;
#endif

  if (!cac->cac_enabled)
    return;

  tvhtrace(LS_DVBCAM, "start service %p", t);

#if ENABLE_DDCI
  mpegts_pid_init(&ecm_pids);
  mpegts_pid_init(&ecm_to_open);
  mpegts_pid_init(&ecm_to_close);
#endif

  pthread_mutex_lock(&t->s_stream_mutex);
  pthread_mutex_lock(&dvbcam_mutex);

  /* is there already a CAM associated to the service? */
  TAILQ_FOREACH(as, &dvbcam_active_services, global_link) {
    if (as->td_service == t)
      goto update_pid;
  }

  /*
   * FIXME: Service limit for DDCI.
   *        This might be removed or implemented differently in case of
   *        MCD/MTD. VDR asks the CAM with a query if the CAM can decode another
   *        PID.
   *        Note: A CAM has decoding slots, which are used up by each PID which
   *              gets decoded. This are Audio, Video, Teletext, ..., depending
   *              of the decoded program this can be more or less PIDs,
   *              resulting in more or less occupied CAM decoding slots. So the
   *              simple limit approach might not work or some decoding slots
   *              remain unused.
   */

  /* check all or filtered elementary streams for CAIDs and find CAM */
  for (i = 0; i < ARRAY_SIZE(dc->caid_list); i++) {
    if (dc->caid_list[0]) {
      if (dc->caid_list[i] == 0)
        break;
    } else {
      if (i > 0)
        break;
    }
    TAILQ_FOREACH(st, &t->s_filt_components, es_link) {
      if (st->es_type != SCT_CA) continue;
      LIST_FOREACH(c, &st->es_caids, link) {
        if (!c->use) continue;
        if (t->s_dvb_forcecaid && t->s_dvb_forcecaid != c->caid) continue;
        if (dc->caid_list[0] && dc->caid_list[i] != c->caid) continue;
        TAILQ_FOREACH(ac, &dvbcam_active_cams, global_link) {
          if (dvbcam_ca_lookup(ac,
                               ((mpegts_service_t *)t)->s_dvb_active_input,
                               c->caid)) {
            /* limit the concurrent service decoders per CAM */
            if (dc->limit > 0 && ac->allocated_programs >= dc->limit)
              continue;
  #if ENABLE_DDCI
            /* currently we allow only one service per DD CI */
            if (ac->ca->lddci && linuxdvb_ddci_is_assigned(ac->ca->lddci))
              continue;
  #endif
            tvhtrace(LS_DVBCAM, "%s/%p: match CAID %04X PID %d (%04X)",
                                ac->ca->lca_name, t, c->caid, c->pid, c->pid);
            goto end_of_search_for_cam;
          }
        }
      }
    }
  }

end_of_search_for_cam:
  if (ac == NULL)
    goto end;

  if ((as = calloc(1, sizeof(*as))) == NULL)
    goto end;

  ac->allocated_programs++;

  as->ac = ac;

  if (dc->caid_select == DVBCAM_SEL_FIRST ||
      t->s_dvb_forcecaid) {
    as->caid_list[0] = c->caid;
    as->caid_list[1] = 0;
    tvhtrace(LS_DVBCAM, "%s/%p: first CAID %04X selected",
                        ac->ca->lca_name, t, as->caid_list[0]);
  } else {
    for (i = j = 0; i < ARRAY_SIZE(dc->caid_list); i++) {
      if (dc->caid_list[0]) {
        if (dc->caid_list[i] == 0)
          break;
      } else {
        if (i > 0)
          break;
      }
      TAILQ_FOREACH(st, &t->s_filt_components, es_link) {
        if (st->es_type != SCT_CA) continue;
        LIST_FOREACH(c, &st->es_caids, link) {
          if (i >= ARRAY_SIZE(as->caid_list)) {
            tvherror(LS_DVBCAM, "%s/%p: CAID service list overflow",
                                ac->ca->lca_name, t);
            break;
          }
          if (!c->use) continue;
          if (dc->caid_list[0] && c->caid != dc->caid_list[i]) continue;
          if (!dvbcam_ca_lookup(ac,
                                ((mpegts_service_t *)t)->s_dvb_active_input,
                                c->caid)) continue;
          if (dc->caid_select != DVBCAM_SEL_LAST)
            tvhtrace(LS_DVBCAM, "%s/%p: add CAID %04X to selection",
                                ac->ca->lca_name, t, c->caid);
          as->caid_list[j++] = c->caid;
        }
      }
    }
    if (j < ARRAY_SIZE(as->caid_list))
      as->caid_list[j] = 0;
    if (dc->caid_select == DVBCAM_SEL_LAST && j > 0) {
      as->caid_list[0] = as->caid_list[j-1];
      as->caid_list[1] = 0;
      tvhtrace(LS_DVBCAM, "%s/%p: last CAID %04X selected",
                          ac->ca->lca_name, t, as->caid_list[0]);
    }
  }

  mpegts_pid_init(&as->ecm_pids);
  mpegts_pid_init(&as->cat_pids);

  td = (th_descrambler_t *)as;
  snprintf(buf, sizeof(buf), "dvbcam-%i-%i-%04X",
           ac->ca->lca_number, (int)ac->slot, (int)as->caid_list[0]);
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
    as->is_ddci = 1;
  }
#endif
  descrambler_change_keystate(td, DS_READY, 0);

  LIST_INSERT_HEAD(&t->s_descramblers, td, td_service_link);

  LIST_INSERT_HEAD(&dc->services, as, dvbcam_link);
  TAILQ_INSERT_TAIL(&dvbcam_active_services, as, global_link);

update_pid:
#if ENABLE_DDCI
  /* open selected ECM PIDs */
  TAILQ_FOREACH(st, &t->s_filt_components, es_link) {
    if (st->es_type != SCT_CA) continue;
    LIST_FOREACH(c, &st->es_caids, link) {
      if (!c->use) continue;
      if (dvbcam_service_check_caid(as, c->caid)) {
        mpegts_pid_add(&ecm_pids, st->es_pid, 0);
        tvhtrace(LS_DVBCAM, "%s/%p: add ECM PID %d (%04X) for CAID %04X",
                            ac->ca->lca_name, t, st->es_pid, st->es_pid, c->caid);
      }
    }
  }
  mpegts_pid_compare(&ecm_pids, &as->ecm_pids, &ecm_to_open, &ecm_to_close);
  mpegts_pid_copy(&as->ecm_pids, &ecm_pids);
#endif

  pthread_mutex_unlock(&dvbcam_mutex);
  pthread_mutex_unlock(&t->s_stream_mutex);

#if ENABLE_DDCI
  if (as && as->is_ddci) {
    mm = ((mpegts_service_t *)t)->s_dvb_mux;
    mi = mm->mm_active ? mm->mm_active->mmi_input : NULL;
    if (mi) {
      pthread_mutex_lock(&mi->mi_output_lock);
      pthread_mutex_lock(&t->s_stream_mutex);
      mpegts_input_open_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, MPS_WEIGHT_CAT, t, 0);
      ((mpegts_service_t *)t)->s_cat_opened = 1;
      for (i = 0; i < ecm_to_open.count; i++)
        mpegts_input_open_pid(mi, mm, ecm_to_open.pids[i].pid, MPS_SERVICE,
                             MPS_WEIGHT_CA, t, 0);
      for (i = 0; i < ecm_to_close.count; i++)
        mpegts_input_close_pid(mi, mm, ecm_to_close.pids[i].pid, MPS_SERVICE,
                               MPS_WEIGHT_CA, t);
      pthread_mutex_unlock(&t->s_stream_mutex);
      pthread_mutex_unlock(&mi->mi_output_lock);
      mpegts_mux_update_pids(mm);
    }
  }
  mpegts_pid_done(&ecm_to_close);
  mpegts_pid_done(&ecm_to_open);
  mpegts_pid_done(&ecm_pids);
#endif
  return;

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
typedef struct __cat_update {
  mpegts_service_t *service;
  mpegts_apids_t to_open;
  mpegts_apids_t to_close;
} __cat_update_t;

/*
 *
 */
static void
dvbcam_cat_update(caclient_t *cac, mpegts_mux_t *mux, const uint8_t *data, int len)
{
#if ENABLE_DDCI
  __cat_update_t services[32], *sp;
  int i, services_count;
  dvbcam_active_service_t *as;
  mpegts_apids_t pids;
  const uint8_t *data1;
  int len1;
  uint8_t dtag;
  uint8_t dlen;
  uint16_t caid;
  uint16_t pid;
  mpegts_input_t *mi;

  if (len <= 0)
    return;

  services_count = 0;
  pthread_mutex_lock(&dvbcam_mutex);
  /* look for CAID in all services */
  TAILQ_FOREACH(as, &dvbcam_active_services, global_link) {
    if (!as->is_ddci) continue;
    if (as->caid_list[0] == 0) continue;
    if (((mpegts_service_t *)as->td_service)->s_dvb_mux != mux) continue;
    if (len == as->cat_data_len &&
        memcmp(data, as->cat_data, len) == 0)
      continue;
    free(as->cat_data);
    as->cat_data = malloc(len);
    memcpy(as->cat_data, data, len);
    as->cat_data_len = len;
    if (services_count < ARRAY_SIZE(services)) {
      sp = &services[services_count++];
      sp->service = (mpegts_service_t *)as->td_service;
      mpegts_pid_init(&sp->to_open);
      mpegts_pid_init(&sp->to_close);
      mpegts_pid_init(&pids);
      data1 = data;
      len1  = len;
      while (len1 > 2) {
        dtag = *data1++;
        dlen = *data1++;
        len1 -= 2;
        if (dtag == DVB_DESC_CA && len1 >= 4 && dlen >= 4) {
          caid =  (data1[0] << 8) | data1[1];
          pid  = ((data1[2] << 8) | data1[3]) & 0x1fff;
          if (dvbcam_service_check_caid(as, caid) && pid != 0) {
            tvhtrace(LS_DVBCAM, "%p: add EMM PID %d (%04X) for CAID %04X",
                                sp->service, pid, pid, caid);
            mpegts_pid_add(&pids, pid, 0);
          }
        }
        data1 += dlen;
        len1  -= dlen;
      }
      mpegts_pid_compare(&pids, &as->cat_pids, &sp->to_open, &sp->to_close);
      mpegts_pid_copy(&as->cat_pids, &pids);
      mpegts_pid_done(&pids);
    } else
      tvherror(LS_DVBCAM, "CAT update error (array overflow)");
  }
  pthread_mutex_unlock(&dvbcam_mutex);

  if (services_count > 0) {
    mi = mux->mm_active ? mux->mm_active->mmi_input : NULL;
    if (mi) {
      pthread_mutex_lock(&mi->mi_output_lock);
      pthread_mutex_lock(&sp->service->s_stream_mutex);
      for (i = 0; i < services_count; i++) {
        sp = &services[i];
        for (i = 0; i < sp->to_open.count; i++)
          mpegts_input_open_pid(mi, mux, sp->to_open.pids[i].pid, MPS_SERVICE,
                               MPS_WEIGHT_CAT, sp->service, 0);
        for (i = 0; i < sp->to_close.count; i++)
          mpegts_input_close_pid(mi, mux, sp->to_close.pids[i].pid, MPS_SERVICE,
                                 MPS_WEIGHT_CAT, sp->service);
        mpegts_pid_done(&sp->to_open);
        mpegts_pid_done(&sp->to_close);
      }
      pthread_mutex_unlock(&sp->service->s_stream_mutex);
      pthread_mutex_unlock(&mi->mi_output_lock);
      mpegts_mux_update_pids(mux);
    }
  }
#endif
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
static htsmsg_t *
caclient_dvbcam_class_caid_selection_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("All CAIDs"),           DVBCAM_SEL_ALL },
    { N_("First hit"),           DVBCAM_SEL_FIRST },
    { N_("Last hit"),            DVBCAM_SEL_LAST },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static int
caclient_dvbcam_class_caid_list_set( void *obj, const void *p )
{
  dvbcam_t *dc = obj;
  htsmsg_t *list = htsmsg_csv_2_list((const char *)p, ',');
  const char *s;
  htsmsg_field_t *f;
  uint16_t caid_list[ARRAY_SIZE(dc->caid_list)];
  int caid, idx, index = 0, change = 0;

  for (idx = 0; dc->caid_list[idx] && idx < ARRAY_SIZE(dc->caid_list); idx++);

  if (list == NULL)
    return 0;
  HTSMSG_FOREACH(f, list) {
    if (index >= ARRAY_SIZE(dc->caid_list)) {
      tvherror(LS_DVBCAM, "CAID list overflow");
      break;
    }
    s = htsmsg_field_get_str(f);
    if (s) {
      caid = strtol(s, NULL, 16);
      if (caid > 0 && caid < 0xFFFF) {
        if (caid != dc->caid_list[index])
          change = 1;
        caid_list[index++] = caid;
      }
    }
  }
  htsmsg_destroy(list);
  if (index < ARRAY_SIZE(dc->caid_list))
    dc->caid_list[index] = 0;
  if (change)
    memcpy(dc->caid_list, caid_list, index * sizeof(caid_list[0]));
  return change || index != idx;
}

static const void *
caclient_dvbcam_class_caid_list_get( void *obj )
{
  dvbcam_t *dc = obj;
  size_t l = 0;
  int index;

  prop_sbuf[0] = '\0';
  for (index = 0; index < ARRAY_SIZE(dc->caid_list); index++) {
    if (dc->caid_list[index] == 0)
      break;
    tvh_strlcatf(prop_sbuf, PROP_SBUF_LEN, l, "%s%04X",
                 index > 0 ? "," : "", dc->caid_list[index]);
  }
  return &prop_sbuf_ptr;
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
      .desc     = N_("Limit of concurrent descrambled services (per one CAM)."),
      .off      = offsetof(dvbcam_t, limit),
    },
    {
      .type     = PT_INT,
      .id       = "caid_select",
      .name     = N_("CAID selection"),
      .desc     = N_("Selection method for CAID"),
      .list     = caclient_dvbcam_class_caid_selection_list,
      .off      = offsetof(dvbcam_t, caid_select),
    },
    {
      .type     = PT_STR,
      .id       = "caid_list",
      .name     = N_("CAID filter list"),
      .desc     = N_("A list of allowed CAIDs (hexa format, comma separated). "
                     "E.g. '0D00,0F00,0100'."),
      .set      = caclient_dvbcam_class_caid_list_set,
      .get      = caclient_dvbcam_class_caid_list_get,
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
  dc->cac_cat_update   = dvbcam_cat_update;
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
