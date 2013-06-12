/*
 *  MPEGTS (DVB) based service
 *
 *  Copyright (C) 2013 Andreas Öman
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

#include <assert.h>

#include "service.h"
#include "input/mpegts.h"

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t service_class;

const idclass_t mpegts_service_class =
{
  .ic_super      = &service_class,
  .ic_class      = "mpegts_service",
  .ic_caption    = "MPEGTS Service",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_U16,
      .id       = "sid",
      .name     = "Service ID",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_service_id),
    },
    {
      .type     = PT_U16,
      .id       = "lcn",
      .name     = "Local Channel Number",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_channel_num),
    },
    {
      .type     = PT_STR,
      .id       = "svcname",
      .name     = "Service Name",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_svcname),
    },
    {
      .type     = PT_STR,
      .id       = "provider",
      .name     = "Provider",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_provider),
    },
    {
      .type     = PT_STR,
      .id       = "cridauth",
      .name     = "CRID Authority",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_cridauth),
    },
    {
      .type     = PT_U16,
      .id       = "servicetype",
      .name     = "Service Type",
      .opts     = PO_RDONLY,
      .off      = offsetof(mpegts_service_t, s_dvb_servicetype),
    },
    {
      .type   = PT_STR,
      .id     = "charset",
      .name   = "Character Set",
      .off    = offsetof(mpegts_service_t, s_dvb_charset),
    },
    {},
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

/*
 * Check the service is enabled
 */
static int
mpegts_service_is_enabled(service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_mux_t *mm    = s->s_dvb_mux;
  return mm->mm_is_enabled(mm) ? s->s_enabled : 0;
}

/*
 * Save
 */
static void
mpegts_service_config_save(service_t *t)
{
}

/*
 * Service instance list
 */
static void
mpegts_service_enlist(service_t *t, struct service_instance_list *sil)
{
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mmi;

  assert(s->s_source_type == S_MPEG_TS);

  LIST_FOREACH(mmi, &m->mm_instances, mmi_mux_link) {
    if (mmi->mmi_tune_failed)
      continue;

    if (!mmi->mmi_input->mi_is_enabled(mmi->mmi_input)) continue;

    service_instance_add(sil, t, mmi->mmi_input->mi_instance,
                         mmi->mmi_input->mi_current_weight(mmi->mmi_input),
                         0/*TODO: priority */);
  }
}

/*
 * Start service
 */
static int
mpegts_service_start(service_t *t, int instance)
{
  int r;
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mmi;

  /* Validate */
  assert(s->s_status      == SERVICE_IDLE);
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Find */
  LIST_FOREACH(mmi, &m->mm_instances, mmi_mux_link)
    if (mmi->mmi_input->mi_instance == instance)
      break;
  assert(mmi != NULL);
  if (mmi == NULL)
    return SM_CODE_UNDEFINED_ERROR;

  /* Start Mux */
  r = mpegts_mux_instance_start(&mmi);

  /* Start */
  if (!r) {

    /* Add to active set */
    pthread_mutex_lock(&mmi->mmi_input->mi_delivery_mutex);
    LIST_INSERT_HEAD(&mmi->mmi_input->mi_transports, t, s_active_link);
    s->s_dvb_active_input = mmi->mmi_input;
    pthread_mutex_unlock(&mmi->mmi_input->mi_delivery_mutex);

    /* Open service */
    mmi->mmi_input->mi_open_service(mmi->mmi_input, s);
  }

  return r;
}

/*
 * Stop service
 */
static void
mpegts_service_stop(service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_input_t   *i = s->s_dvb_active_input;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  assert(i != NULL);
  lock_assert(&global_lock);

  /* Remove */
  pthread_mutex_lock(&i->mi_delivery_mutex);
  LIST_REMOVE(t, s_active_link);
  s->s_dvb_active_input = NULL;
  pthread_mutex_unlock(&i->mi_delivery_mutex);

  /* Stop */
  i->mi_close_service(i, s);
  s->s_status = SERVICE_IDLE;
}

/*
 * Refresh
 */
static void
mpegts_service_refresh(service_t *t)
{
  mpegts_service_t *s = (mpegts_service_t*)t;
  mpegts_input_t   *i = s->s_dvb_active_input;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  assert(i != NULL);
  lock_assert(&global_lock);

  /* Re-open */
  i->mi_open_service(i, s);
}

/*
 * Source info
 */
static void
mpegts_service_setsourceinfo(service_t *t, source_info_t *si)
{
  char buf[128];
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;

  /* Validate */
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Update */
  memset(si, 0, sizeof(struct source_info));
  si->si_type = S_MPEG_TS;

  if(m->mm_network->mn_network_name != NULL)
    si->si_network = strdup(m->mm_network->mn_network_name);

  m->mm_display_name(m, buf, sizeof(buf));
  si->si_mux = strdup(buf);

  if(s->s_dvb_active_input) {
    mpegts_input_t *mi = s->s_dvb_active_input;
    mi->mi_display_name(mi, buf, sizeof(buf));
    si->si_adapter = strdup(buf);
  }

  if(s->s_dvb_provider != NULL)
    si->si_provider = strdup(s->s_dvb_provider);

  if(s->s_dvb_svcname != NULL)
    si->si_service = strdup(s->s_dvb_svcname);
}

/* **************************************************************************
 * Creation/Location
 * *************************************************************************/

/*
 * Create service
 */
mpegts_service_t *
mpegts_service_create0
  ( mpegts_service_t *s, const idclass_t *class, const char *uuid,
    mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, htsmsg_t *conf )
{
  char buf[256];
  service_create0((service_t*)s, class, uuid, S_MPEG_TS, conf);

  /* Create */
  sbuf_init(&s->s_tsbuf);
  if (!conf) {
    if (sid)     s->s_dvb_service_id = sid;
    if (pmt_pid) s->s_pmt_pid        = pmt_pid;
  }
  s->s_dvb_mux        = mm;
  LIST_INSERT_HEAD(&mm->mm_services, s, s_dvb_mux_link);
  
  s->s_is_enabled     = mpegts_service_is_enabled;
  s->s_config_save    = mpegts_service_config_save;
  s->s_enlist         = mpegts_service_enlist;
  s->s_start_feed     = mpegts_service_start;
  s->s_stop_feed      = mpegts_service_stop;
  s->s_refresh_feed   = mpegts_service_refresh;
  s->s_setsourceinfo  = mpegts_service_setsourceinfo;
#if 0
  s->s_grace_period   = mpegts_service_grace_period;
#endif

  pthread_mutex_lock(&s->s_stream_mutex);
  service_make_nicename((service_t*)s);
  pthread_mutex_unlock(&s->s_stream_mutex);

  mm->mm_display_name(mm, buf, sizeof(buf));
  tvhlog(LOG_DEBUG, "mpegts", "%s - add service %04X %s", buf, s->s_dvb_service_id, s->s_dvb_svcname);
  return s;
}

/*
 * Find service
 */
mpegts_service_t *
mpegts_service_find
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid, 
    int create, int *save )
{
  mpegts_service_t *s;

  /* Validate */
  lock_assert(&global_lock);

  /* Find existing service */
  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
    if (s->s_dvb_service_id == sid) {
      if (pmt_pid && pmt_pid != s->s_pmt_pid) {
        s->s_pmt_pid = pmt_pid;
        if (save) *save = 1;
      }
      return s;
    }
  }

  /* Create */
  if (create) {
    s = mm->mm_network->mn_create_service(mm, sid, pmt_pid);
    if (save) *save = 1;
  }
  
  return s;
}

/*
 * Save
 */
void
mpegts_service_save ( mpegts_service_t *s, htsmsg_t *c )
{
  service_save((service_t*)s, c);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
