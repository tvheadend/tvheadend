/*
 *  Tvheadend - TS file input system
 *
 *  Copyright (C) 2014 Adam Sutton
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
#include "input.h"
#include "input/mpegts/mpegts_mux_sched.h"
#include "streaming.h"
#include "settings.h"
#include "profile.h"

static void mpegts_mux_sched_timer ( void *p );
static void mpegts_mux_sched_input ( void *p, streaming_message_t *sm );

mpegts_mux_sched_list_t mpegts_mux_sched_all;

/******************************************************************************
 * Class
 *****************************************************************************/

static void
mpegts_mux_sched_set_timer ( mpegts_mux_sched_t *mms )
{
  /* Upate timer */
  if (!mms->mms_enabled) {
    if (mms->mms_sub)
      subscription_unsubscribe(mms->mms_sub, 0);
    mms->mms_sub    = NULL;
    mms->mms_active = 0;
    gtimer_disarm(&mms->mms_timer);
  } else if (mms->mms_active) {
    if (mms->mms_timeout <= 0)
      gtimer_disarm(&mms->mms_timer);
    else {
      gtimer_arm(&mms->mms_timer, mpegts_mux_sched_timer, mms,
                 mms->mms_timeout);
    }
  } else {
    time_t now, nxt;
    time(&now);
    if (!cron_next(&mms->mms_cronjob, now, &nxt)) {
      gtimer_arm_abs(&mms->mms_timer, mpegts_mux_sched_timer, mms, nxt);
    }
  }
}

static void
mpegts_mux_sched_class_save ( idnode_t *in )
{
  mpegts_mux_sched_t *mms = (mpegts_mux_sched_t*)in;

  /* Update timer */
  mpegts_mux_sched_set_timer(mms);

  /* Save */
  mpegts_mux_sched_save(mms);
}

static void
mpegts_mux_sched_class_delete ( idnode_t *in )
{
  mpegts_mux_sched_delete((mpegts_mux_sched_t*)in, 1);
}

static htsmsg_t *
mpegts_mux_sched_class_mux_list ( void *o, const char *lang )
{
  htsmsg_t *m, *p;

  p = htsmsg_create_map();
  htsmsg_add_str (p, "class", "mpegts_mux");
  htsmsg_add_bool(p, "enum",  1);

  m = htsmsg_create_map();
  htsmsg_add_str (m, "type",  "api");
  htsmsg_add_str (m, "uri",   "idnode/load");
  htsmsg_add_str (m, "event", "mpegts_mux");
  htsmsg_add_msg (m, "params", p);
  
  return m;
}

static int
mpegts_mux_sched_class_cron_set ( void *p, const void *v )
{
  mpegts_mux_sched_t *mms = p;
  const char *str = v;
  if (strcmp(str, mms->mms_cronstr ?: "")) {
    if (!cron_set(&mms->mms_cronjob, str)) {
      free(mms->mms_cronstr);
      mms->mms_cronstr = strdup(str);
      return 1;
    } else {
      tvhwarn("muxsched", "invalid cronjob spec (%s)", str);
    }
  }
  return 0;
}

const idclass_t mpegts_mux_sched_class =
{
  .ic_class      = "mpegts_mux_sched",
  .ic_caption    = N_("Mux Sched Entry"),
  .ic_event      = "mpegts_mux_sched",
  .ic_save       = mpegts_mux_sched_class_save,
  .ic_delete     = mpegts_mux_sched_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(mpegts_mux_sched_t, mms_enabled),
      .def.i    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "mux",
      .name     = N_("Mux"),
      .off      = offsetof(mpegts_mux_sched_t, mms_mux),
      .list     = mpegts_mux_sched_class_mux_list,
    },
    {
      .type     = PT_STR,
      .id       = "cron",
      .name     = N_("Cron"),
      .off      = offsetof(mpegts_mux_sched_t, mms_cronstr),
      .set      = mpegts_mux_sched_class_cron_set,
    },
    {
      .type     = PT_INT,
      .id       = "timeout",
      .name     = N_("Timout (secs)"),
      .off      = offsetof(mpegts_mux_sched_t, mms_timeout),
    },
    {
    },
  }
};

/******************************************************************************
 * Input
 *****************************************************************************/

static void
mpegts_mux_sched_input ( void *p, streaming_message_t *sm )
{
  mpegts_mux_sched_t *mms = p;

  switch (sm->sm_type) {
    case SMT_STOP:
      gtimer_arm(&mms->mms_timer, mpegts_mux_sched_timer, mms, 0);
      break;
    default:
      // ignore
      break;
  }
  streaming_msg_free(sm);
}

/******************************************************************************
 * Timer
 *****************************************************************************/

static void
mpegts_mux_sched_timer ( void *p )
{
  mpegts_mux_t *mm;
  mpegts_mux_sched_t *mms = p;
  time_t now, nxt;

  /* Not enabled (shouldn't be running) */
  if (!mms->mms_enabled)
    return;

  /* Invalid config (creating?) */
  if (!mms->mms_mux)
    return;
  
  /* Find mux */
  if (!(mm = mpegts_mux_find(mms->mms_mux))) {
    tvhdebug("muxsched", "mux has been removed, delete sched entry");
    mpegts_mux_sched_delete(mms, 1);
    return;
  }

  /* Current time */
  time(&now);

  /* Start sub */
  if (!mms->mms_active) {
    assert(mms->mms_sub == NULL);

    if (!mms->mms_prch)
      mms->mms_prch = calloc(1, sizeof(*mms->mms_prch));
    mms->mms_prch->prch_id = mm;
    mms->mms_prch->prch_st = &mms->mms_input;

    mms->mms_sub
      = subscription_create_from_mux(mms->mms_prch, NULL, mms->mms_weight,
                                     mms->mms_creator ?: "",
                                     SUBSCRIPTION_MINIMAL,
                                     NULL, NULL, NULL, NULL);

    /* Failed (try-again soon) */
    if (!mms->mms_sub) {
      gtimer_arm(&mms->mms_timer, mpegts_mux_sched_timer, mms, 60);

    /* OK */
    } else {
      mms->mms_active = 1;
      if (mms->mms_timeout > 0) {
        gtimer_arm(&mms->mms_timer, mpegts_mux_sched_timer, mms,
                   mms->mms_timeout);
      } 
    }

  /* Cancel sub */
  } else {
    if (mms->mms_sub) {
      subscription_unsubscribe(mms->mms_sub, 0);
      mms->mms_sub = NULL;
    }
    mms->mms_active = 0;

    /* Find next */
    if (cron_next(&mms->mms_cronjob, now, &nxt)) {
      tvherror("muxsched", "failed to find next event");
      return;
    }

    /* Timer */
    gtimer_arm_abs(&mms->mms_timer, mpegts_mux_sched_timer, mms, nxt);
  }
}

/******************************************************************************
 * Init / Create
 *****************************************************************************/

mpegts_mux_sched_t *
mpegts_mux_sched_create ( const char *uuid, htsmsg_t *conf )
{
  mpegts_mux_sched_t *mms;

  if (!(mms = calloc(1, sizeof(mpegts_mux_sched_t)))) {
    tvherror("muxsched", "calloc() failed");
    assert(0);
    return NULL;
  }

  /* Insert node */
  if (idnode_insert(&mms->mms_id, uuid, &mpegts_mux_sched_class, 0)) {
    if (uuid)
      tvherror("muxsched", "invalid uuid '%s'", uuid);
    free(mms);
    return NULL;
  }

  /* Add to list */
  LIST_INSERT_HEAD(&mpegts_mux_sched_all, mms, mms_link);

  /* Initialise */
  streaming_target_init(&mms->mms_input, mpegts_mux_sched_input, mms, 0);

  /* Load conf */
  if (conf)
    idnode_load(&mms->mms_id, conf);

  /* Validate */
  if (!mpegts_mux_find(mms->mms_mux ?: "") ||
      !mms->mms_cronstr) {
    mpegts_mux_sched_delete(mms, 1);
    return NULL;
  }

  /* Set timer */
  mpegts_mux_sched_set_timer(mms);

  return mms;
}

void
mpegts_mux_sched_save ( mpegts_mux_sched_t *mms )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&mms->mms_id, c);
  hts_settings_save(c, "muxsched/%s", idnode_uuid_as_str(&mms->mms_id));
  htsmsg_destroy(c);
}

void
mpegts_mux_sched_delete ( mpegts_mux_sched_t *mms, int delconf )
{
  LIST_REMOVE(mms, mms_link);
  if (delconf)
    hts_settings_remove("muxsched/%s", idnode_uuid_as_str(&mms->mms_id));
  if (mms->mms_sub)
    subscription_unsubscribe(mms->mms_sub, 0);
  gtimer_disarm(&mms->mms_timer);
  idnode_unlink(&mms->mms_id);
  free(mms->mms_cronstr);
  free(mms->mms_mux);
  free(mms->mms_creator);
  free(mms->mms_prch);
  free(mms);
}

void
mpegts_mux_sched_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Load settings */
  if ((c = hts_settings_load_r(1, "muxsched"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      mpegts_mux_sched_create(f->hmf_name, e);
    }
    htsmsg_destroy(c);
  }
}

void
mpegts_mux_sched_done ( void )
{
  mpegts_mux_sched_t *mms;
  pthread_mutex_lock(&global_lock);
  while ((mms = LIST_FIRST(&mpegts_mux_sched_all)))
    mpegts_mux_sched_delete(mms, 0);
  pthread_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
