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

int  mpegts_service_enabled (service_t *);
void mpegts_service_enlist  (service_t *, struct service_instance_list*);
int  mpegts_service_start   (service_t *, int);
void mpegts_service_stop    (service_t *);
void mpegts_service_refresh (service_t *);
void mpegts_service_setsourceinfo (service_t *, source_info_t *);

/*
 * Check the service is enabled
 */
int
mpegts_service_enabled(service_t *t)
{
  mpegts_service_t      *s = (mpegts_service_t*)t;
#if 0
  mpegts_mux_t          *m = t->s_dvb_mux;
  mpegts_mux_instance_t *mi;
#endif
  assert(s->s_source_type == S_MPEG_TS);
  return 1; // TODO: check heirarchy
}

/*
 * Service instance list
 */
void
mpegts_service_enlist(service_t *t, struct service_instance_list *sil)
{
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mi;

  assert(s->s_source_type == S_MPEG_TS);

  LIST_FOREACH(mi, &m->mm_instances, mmi_mux_link) {
    if (mi->mmi_tune_failed)
      continue;
    // TODO: check the instance is enabled

    service_instance_add(sil, t, mi->mmi_input->mi_instance, 
                         //TODO: fix below,
                         100, 0);
                         //mpegts_mux_instance_weight(mi));
  }
}

/*
 * Start service
 */
int
mpegts_service_start(service_t *t, int instance)
{
  int r;
  mpegts_service_t      *s = (mpegts_service_t*)t;
  mpegts_mux_t          *m = s->s_dvb_mux;
  mpegts_mux_instance_t *mi;

  /* Validate */
  assert(s->s_status      == SERVICE_IDLE);
  assert(s->s_source_type == S_MPEG_TS);
  lock_assert(&global_lock);

  /* Find */
  LIST_FOREACH(mi, &m->mm_instances, mmi_mux_link)
    if (mi->mmi_input->mi_instance == instance)
      break;
  assert(mi != NULL);
  if (mi == NULL)
    return SM_CODE_UNDEFINED_ERROR;

  /* Start */
  if (!(r = mi->mmi_input->mi_start_mux(mi->mmi_input, mi))) {
    pthread_mutex_lock(&mi->mmi_input->mi_delivery_mutex);
    LIST_INSERT_HEAD(&mi->mmi_input->mi_transports, t, s_active_link);
    s->s_dvb_active_input = mi->mmi_input;
    pthread_mutex_unlock(&mi->mmi_input->mi_delivery_mutex);
    mi->mmi_input->mi_open_service(mi->mmi_input, s);
    // TODO: revisit this
  }

  return r;
}

/*
 * Stop service
 */
void
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
void
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
void
mpegts_service_setsourceinfo(service_t *t, source_info_t *si)
{
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

  si->si_mux = strdup("TODO:mpegts_mux_nicename(m)");

  if(s->s_dvb_provider != NULL)
    si->si_provider = strdup(s->s_dvb_provider);

  if(s->s_dvb_svcname != NULL)
    si->si_service = strdup(s->s_dvb_svcname);
}
