/*
 *  Tvheadend - MPEGTS input source
 *  Copyright (C) 2013 Adam Sutton
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

#include "input/mpegts.h"
#include "tsdemux.h"
#include "packet.h"
#include "streaming.h"
#include "subscriptions.h"
#include "atomic.h"
#include "notify.h"

#include <pthread.h>
#include <assert.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static const char *
mpegts_input_class_get_title ( idnode_t *in )
{
  mpegts_input_t *mi = (mpegts_input_t*)in;
  return mi->mi_displayname;
}

const idclass_t mpegts_input_class =
{
  .ic_class      = "mpegts_input",
  .ic_caption    = "MPEGTS Input",
  .ic_get_title  = mpegts_input_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(mpegts_input_t, mi_enabled),
      .def.i    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = "Priority",
      .off      = offsetof(mpegts_input_t, mi_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = "Name",
      .off      = offsetof(mpegts_input_t, mi_displayname),
      .notify   = idnode_notify_title_changed,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
mpegts_input_is_enabled ( mpegts_input_t *mi )
{
  return mi->mi_enabled;
}

static void
mpegts_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  if (mi->mi_displayname)
    strncpy(buf, mi->mi_displayname, len);
  else
    *buf = 0;
}

int
mpegts_input_is_free ( mpegts_input_t *mi )
{
  char buf[256];
  mpegts_mux_instance_t *mmi = LIST_FIRST(&mi->mi_mux_active);
  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - is free? %d", buf, mmi == NULL);
  return mmi ? 0 : 1;
}

int
mpegts_input_get_weight ( mpegts_input_t *mi )
{
  const mpegts_mux_instance_t *mmi;
  const service_t *s;
  const th_subscription_t *ths;
  int w = 0;

  /* Direct subs */
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    LIST_FOREACH(ths, &mmi->mmi_subs, ths_mmi_link) {
      w = MAX(w, ths->ths_weight);
    }
  }

  /* Service subs */
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
    LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
      w = MAX(w, ths->ths_weight);
    }
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
  return w;
}

int
mpegts_input_get_priority ( mpegts_input_t *mi )
{
  return mi->mi_priority;
}

static int
mpegts_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  return SM_CODE_TUNING_FAILED;
}

static void
mpegts_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
}

static int
mps_cmp ( mpegts_pid_sub_t *a, mpegts_pid_sub_t *b )
{
  if (a->mps_type != b->mps_type)
    return a->mps_type - b->mps_type;
  if (a->mps_owner < b->mps_owner) return -1;
  if (a->mps_owner > b->mps_owner) return 1;
  return 0;
}

mpegts_pid_t *
mpegts_input_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  char buf[512];
  mpegts_pid_t *mp;
  assert(owner != NULL);
  if ((mp = mpegts_mux_find_pid(mm, pid, 1))) {
    static mpegts_pid_sub_t *skel = NULL;
    if (!skel)
      skel = calloc(1, sizeof(mpegts_pid_sub_t));
    skel->mps_type  = type;
    skel->mps_owner = owner;
    if (!RB_INSERT_SORTED(&mp->mp_subs, skel, mps_link, mps_cmp)) {
      mm->mm_display_name(mm, buf, sizeof(buf));
      tvhdebug("mpegts", "%s - open PID %04X (%d) [%d/%p]",
               buf, mp->mp_pid, mp->mp_pid, type, owner);
      skel = NULL;
    }
  }
  return mp;
}

void
mpegts_input_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  char buf[512];
  mpegts_pid_sub_t *mps, skel;
  mpegts_pid_t *mp;
  assert(owner != NULL);
  if (!(mp = mpegts_mux_find_pid(mm, pid, 1)))
    return;
  skel.mps_type  = type;
  skel.mps_owner = owner;
  mps = RB_FIND(&mp->mp_subs, &skel, mps_link, mps_cmp);
  if (mps) {
    RB_REMOVE(&mp->mp_subs, mps, mps_link);
    free(mps);

    if (!RB_FIRST(&mp->mp_subs)) {
      RB_REMOVE(&mm->mm_pids, mp, mp_link);
      if (mp->mp_fd != -1) {
        mm->mm_display_name(mm, buf, sizeof(buf));
        tvhdebug("mpegts", "%s - close PID %04X (%d) [%d/%p]",
               buf, mp->mp_pid, mp->mp_pid, type, owner);
        close(mp->mp_fd);
      }
      free(mp);
    }
  }
}

void
mpegts_input_open_service ( mpegts_input_t *mi, mpegts_service_t *s, int init )
{
  elementary_stream_t *st;

  /* Add to list */
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  if (!s->s_dvb_active_input) {
    LIST_INSERT_HEAD(&mi->mi_transports, ((service_t*)s), s_active_link);
    s->s_dvb_active_input = mi;
  }


  /* Register PIDs */
  pthread_mutex_lock(&s->s_stream_mutex);
  TAILQ_FOREACH(st, &s->s_components, es_link)
    mi->mi_open_pid(mi, s->s_dvb_mux, st->es_pid, MPS_STREAM, s);

  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

void
mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  elementary_stream_t *st;

  /* Remove from list */
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  if (s->s_dvb_active_input != NULL) {
    LIST_REMOVE(((service_t*)s), s_active_link);
    s->s_dvb_active_input = NULL;
  }
  
  /* Close PID */
  pthread_mutex_lock(&s->s_stream_mutex);
  TAILQ_FOREACH(st, &s->s_components, es_link)
    mi->mi_close_pid(mi, s->s_dvb_mux, st->es_pid, MPS_STREAM, s);

  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_delivery_mutex);

  /* Stop mux? */
  s->s_dvb_mux->mm_stop(s->s_dvb_mux, 0);
}

static void
mpegts_input_create_mux_instance
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  extern const idclass_t mpegts_mux_instance_class;
  mpegts_mux_instance_t *mmi;
  LIST_FOREACH(mmi, &mi->mi_mux_instances, mmi_input_link)
    if (mmi->mmi_mux == mm) break;
  if (!mmi)
    (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL, mi, mm);
}

static void
mpegts_input_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  /* Arm timer */
  if (LIST_FIRST(&mi->mi_mux_active) == NULL)
    gtimer_arm(&mi->mi_status_timer, mpegts_input_status_timer,
               mi, 1);

  /* Update */
  mmi->mmi_mux->mm_active = mmi;
  LIST_INSERT_HEAD(&mi->mi_mux_active, mmi, mmi_active_link);
  notify_reload("input_status");
}

static void
mpegts_input_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  char buf[256];
  service_t *s;
  mmi->mmi_mux->mm_active = NULL;
  LIST_REMOVE(mmi, mmi_active_link);

  /* Disarm timer */
  if (LIST_FIRST(&mi->mi_mux_active) == NULL)
    gtimer_disarm(&mi->mi_status_timer);

  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - flush subscribers", buf);
  s = LIST_FIRST(&mi->mi_transports);
  while (s) {
    if (((mpegts_service_t*)s)->s_dvb_mux == mmi->mmi_mux)
      service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
    s = LIST_NEXT(s, s_active_link);
  }
  notify_reload("input_status");
}

static int
mpegts_input_has_subscription ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int ret = 0;
  service_t *t;
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_FOREACH(t, &mi->mi_transports, s_active_link) {
    if (((mpegts_service_t*)t)->s_dvb_mux == mm) {
      ret = 1;
      break;
    }
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
  return ret;
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

size_t
mpegts_input_recv_packets
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi,
    uint8_t *tsb, size_t l, int64_t *pcr, uint16_t *pcr_pid,
    const char *name )
{
  int len = l;
  int i = 0, table_wakeup = 0;
  mpegts_mux_t *mm = mmi->mmi_mux;
  assert(mm != NULL);
  assert(name != NULL);

  // TODO: get the input name
  tvhtrace("tsdemux", "%s - recv_packets tsb=%p, len=%d, pcr=%p, pcr_pid=%p",
           name, tsb, (int)len, pcr, pcr_pid);
  
  /* Not enough data */
  if (len < 188) return len;
  
  /* Streaming - lock mutex */
  pthread_mutex_lock(&mi->mi_delivery_mutex);

  /* Process */
  while ( len >= 188 ) {

    /* Sync */
    if ( tsb[i] == 0x47 ) {
      mpegts_pid_t *mp;
      mpegts_pid_sub_t *mps;
      service_t *s;
      int     pid   = ((tsb[i+1] & 0x1f) << 8) | tsb[i+2];
      int64_t *ppcr = (pcr_pid && *pcr_pid == pid) ? pcr : NULL;
      tvhtrace("tsdemux", "%s - recv_packet for pid %04X (%d)", name, pid, pid);

      /* Find PID */
      if ((mp = mpegts_mux_find_pid(mm, pid, 0))) {

        /* Stream takes pref. */
        RB_FOREACH(mps, &mp->mp_subs, mps_link)
          if (mps->mps_type == MPS_STREAM)
            break;
  
        /* Stream data */
        if (mps) {
          LIST_FOREACH(s, &mi->mi_transports, s_active_link)
            ts_recv_packet1((mpegts_service_t*)s, tsb+i, ppcr);

        /* Table data */
        } else {
          if (!(tsb[i+1] & 0x80)) {
            mpegts_table_feed_t *mtf = malloc(sizeof(mpegts_table_feed_t));
            memcpy(mtf->mtf_tsb, tsb+i, 188);
            mtf->mtf_mux = mm;
            TAILQ_INSERT_TAIL(&mi->mi_table_feed, mtf, mtf_link);
            table_wakeup = 1;
          } else {
            tvhdebug("tsdemux", "%s - SI packet had errors", name);
          }
        }
      }

      /* Force PCR extraction for tsfile */
      if (ppcr && *ppcr == PTS_UNSET)
        ts_recv_packet1(NULL, tsb+i, ppcr);

      i   += 188;
      len -= 188;
    
    /* Re-sync */
    } else {
      tvhdebug("tsdemux", "%s - ts sync lost", name);
      if (ts_resync(tsb, &len, &i)) break;
      tvhdebug("tsdemux", "%s - ts sync found", name);
    }

  }

  /* Raw stream */
  // Note: this will include unsynced data if that's what is received
  if (i > 0 && LIST_FIRST(&mmi->mmi_streaming_pad.sp_targets) != NULL) {
    streaming_message_t sm;
    pktbuf_t *pb = pktbuf_alloc(tsb, i);
    memset(&sm, 0, sizeof(sm));
    sm.sm_type = SMT_MPEGTS;
    sm.sm_data = pb;
    streaming_pad_deliver(&mmi->mmi_streaming_pad, &sm);
    pktbuf_ref_dec(pb);
  }

  /* Wake table */
  if (table_wakeup)
    pthread_cond_signal(&mi->mi_table_feed_cond);

  pthread_mutex_unlock(&mi->mi_delivery_mutex);

  /* Bandwidth monitoring */
  atomic_add(&mmi->mmi_stats.bps, i);
  
  /* Reset buffer */
  if (len) memmove(tsb, tsb+i, len);

  return len;
}

static void
mpegts_input_table_dispatch ( mpegts_mux_t *mm, mpegts_table_feed_t *mtf )
{
  int      i   = 0;
  int      len = mm->mm_num_tables;
  uint16_t pid = ((mtf->mtf_tsb[1] & 0x1f) << 8) | mtf->mtf_tsb[2];
  uint8_t  cc  = (mtf->mtf_tsb[3] & 0x0f);
  mpegts_table_t *mt, *vec[len];

  /* Collate - tables may be removed during callbacks */
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    vec[i++] = mt;
    mt->mt_refcount++;
  }
  assert(i == len);

  /* Process */
  for (i = 0; i < len; i++) {
    mt = vec[i];
    if (!mt->mt_destroyed && mt->mt_pid == pid) {
      if (mt->mt_cc != -1 && mt->mt_cc != cc)
        tvhdebug("psi", "pid %04X cc error %d != %d", pid, mt->mt_cc, cc);
      mt->mt_cc = (cc + 1) % 16;
      mpegts_psi_section_reassemble(&mt->mt_sect, mtf->mtf_tsb, 0,
                                    mpegts_table_dispatch, mt);
    }
    mpegts_table_release(mt);
  }
}

void *
mpegts_input_table_thread ( void *aux )
{
  mpegts_table_feed_t   *mtf;
  mpegts_input_t        *mi = aux;

  while (1) {

    /* Wait for data */
    pthread_mutex_lock(&mi->mi_delivery_mutex);
    while(!(mtf = TAILQ_FIRST(&mi->mi_table_feed)))
      pthread_cond_wait(&mi->mi_table_feed_cond, &mi->mi_delivery_mutex);
    TAILQ_REMOVE(&mi->mi_table_feed, mtf, mtf_link);
    pthread_mutex_unlock(&mi->mi_delivery_mutex);

    /* Process */
    pthread_mutex_lock(&global_lock);
    mpegts_input_table_dispatch(mtf->mtf_mux, mtf);
    pthread_mutex_unlock(&global_lock);
    free(mtf);
  }
  return NULL;
}

void
mpegts_input_flush_mux
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_table_feed_t *mtf, *next;

  pthread_mutex_lock(&mi->mi_delivery_mutex);
  mtf = TAILQ_FIRST(&mi->mi_table_feed);
  while (mtf) {
    next = TAILQ_NEXT(mtf, mtf_link);
    if (mtf->mtf_mux == mm) {
      TAILQ_REMOVE(&mi->mi_table_feed, mtf, mtf_link);
      free(mtf);
    }
    mtf  = next;
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

static void
mpegts_input_stream_status
  ( mpegts_mux_instance_t *mmi, tvh_input_stream_t *st )
{
  int s = 0, w = 0;
  char buf[512];
  th_subscription_t *sub;
  mpegts_mux_t *mm = mmi->mmi_mux;
  mpegts_input_t *mi = mmi->mmi_input;

  /* Get number of subs */
  // Note: this is a bit of a mess
  LIST_FOREACH(sub, &mmi->mmi_subs, ths_mmi_link) {
    s++;
    w = MAX(w, sub->ths_weight);
  }
  // Note: due to satconf acting as proxy for input we can't always
  //       use mi_transports, so we can in via a convoluted route
  LIST_FOREACH(sub, &subscriptions, ths_global_link) {
    if (!sub->ths_service) continue;
    if (!idnode_is_instance(&sub->ths_service->s_id,
                            &mpegts_service_class)) continue;
    mpegts_service_t *ms = (mpegts_service_t*)sub->ths_service;
    if (ms->s_dvb_mux == mm) {
      s++;
      w = MAX(w, sub->ths_weight);
    }
  }

  mm->mm_display_name(mm, buf, sizeof(buf));
  st->uuid        = strdup(idnode_uuid_as_str(&mmi->mmi_id));
  st->input_name  = strdup(mi->mi_displayname?:"");
  st->stream_name = strdup(buf);
  st->subs_count  = s;
  st->max_weight  = w;
  st->stats       = mmi->mmi_stats;
  st->stats.bps   = atomic_exchange(&mmi->mmi_stats.bps, 0) * 8;
}

static void
mpegts_input_get_streams
  ( tvh_input_t *i, tvh_input_stream_list_t *isl )
{
  tvh_input_stream_t *st;
  mpegts_input_t *mi = (mpegts_input_t*)i;
  mpegts_mux_instance_t *mmi;

  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    st = calloc(1, sizeof(tvh_input_stream_t));
    mpegts_input_stream_status(mmi, st);
    LIST_INSERT_HEAD(isl, st, link);
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

/* **************************************************************************
 * Status monitoring
 * *************************************************************************/

void
mpegts_input_status_timer ( void *p )
{
  tvh_input_stream_t st;
  mpegts_input_t *mi = p;
  mpegts_mux_instance_t *mmi;
  htsmsg_t *e;
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    memset(&st, 0, sizeof(st));
    mpegts_input_stream_status(mmi, &st);
    e = tvh_input_stream_create_msg(&st);
    htsmsg_add_u32(e, "update", 1);
    notify_by_msg("input_status", e);
    tvh_input_stream_destroy(&st);
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
  gtimer_arm(&mi->mi_status_timer, mpegts_input_status_timer, mi, 1);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

static int mpegts_input_idx = 0;
mpegts_input_list_t mpegts_input_all;

mpegts_input_t*
mpegts_input_create0  
  ( mpegts_input_t *mi, const idclass_t *class, const char *uuid,
    htsmsg_t *c )
{
  idnode_insert(&mi->ti_id, uuid, class);
  LIST_INSERT_HEAD(&tvh_inputs, (tvh_input_t*)mi, ti_link);
  
  /* Defaults */
  mi->mi_is_enabled           = mpegts_input_is_enabled;
  mi->mi_display_name         = mpegts_input_display_name;
  mi->mi_is_free              = mpegts_input_is_free;
  mi->mi_get_weight           = mpegts_input_get_weight;
  mi->mi_get_priority         = mpegts_input_get_priority;
  mi->mi_start_mux            = mpegts_input_start_mux;
  mi->mi_stop_mux             = mpegts_input_stop_mux;
  mi->mi_open_service         = mpegts_input_open_service;
  mi->mi_close_service        = mpegts_input_close_service;
  mi->mi_open_pid             = mpegts_input_open_pid;
  mi->mi_close_pid            = mpegts_input_close_pid;
  mi->mi_create_mux_instance  = mpegts_input_create_mux_instance;
  mi->mi_started_mux          = mpegts_input_started_mux;
  mi->mi_stopped_mux          = mpegts_input_stopped_mux;
  mi->mi_has_subscription     = mpegts_input_has_subscription;
  mi->ti_get_streams          = mpegts_input_get_streams;

  /* Index */
  mi->mi_instance             = ++mpegts_input_idx;

  /* Init mutex */
  pthread_mutex_init(&mi->mi_delivery_mutex, NULL);
  
  /* Table input */
  TAILQ_INIT(&mi->mi_table_feed);
  pthread_cond_init(&mi->mi_table_feed_cond, NULL);

  /* Init input thread control */
  mi->mi_thread_pipe.rd = mi->mi_thread_pipe.wr = -1;

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_input_all, mi, mi_global_link);

  /* Load config */
  if (c)
    idnode_load(&mi->ti_id, c);

  return mi;
}

void
mpegts_input_delete ( mpegts_input_t *mi )
{
  idnode_unlink(&mi->ti_id);
  pthread_mutex_destroy(&mi->mi_delivery_mutex);
  pthread_cond_destroy(&mi->mi_table_feed_cond);
  tvh_pipe_close(&mi->mi_thread_pipe);
  LIST_REMOVE(mi, ti_link);
  LIST_REMOVE(mi, mi_global_link);
  free(mi);
}

void
mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *m )
{
  idnode_save(&mi->ti_id, m);
}

void
mpegts_input_set_network ( mpegts_input_t *mi, mpegts_network_t *mn )
{
  char buf1[256], buf2[265];
  if (mi->mi_network == mn) return;
  *buf1 = *buf2 = 0;
  if (mi->mi_display_name) {
    mi->mi_display_name(mi, buf1, sizeof(buf1));
  }
  if (mi->mi_network) {
    mi->mi_network->mn_display_name(mi->mi_network, buf2, sizeof(buf2));
    LIST_REMOVE(mi, mi_network_link);
    tvhdebug("mpegts", "%s - remove network %s", buf1, buf2);
    mi->mi_network = NULL;
  }
  if (mn) {
    mn->mn_display_name(mn, buf2, sizeof(buf2));
    mi->mi_network = mn;
    LIST_INSERT_HEAD(&mn->mn_inputs, mi, mi_network_link);
    tvhdebug("mpegts", "%s - added network %s", buf1, buf2);
  }
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
