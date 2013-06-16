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

#include <pthread.h>
#include <assert.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static const char *
mpegts_input_class_get_name ( void *in )
{
  static char buf[256];
  mpegts_input_t *mi = in;
  if (mi->mi_display_name)
    mi->mi_display_name(mi, buf, sizeof(buf));
  else
    *buf = 0;
  return buf;
}

const idclass_t mpegts_input_class =
{
  .ic_class      = "mpegts_input",
  .ic_caption    = "MPEGTS Input",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(mpegts_input_t, mi_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = "Name",
      .opts     = PO_NOSAVE | PO_RDONLY,
      .str_get  = mpegts_input_class_get_name,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static const idclass_t *
mpegts_input_network_class (mpegts_input_t *mi)
{
  extern const idclass_t mpegts_network_class;
  return &mpegts_network_class;
}

static mpegts_network_t *
mpegts_input_network_create (mpegts_input_t *mi, htsmsg_t *conf)
{
  return NULL;
}

static int
mpegts_input_is_enabled ( mpegts_input_t *mi )
{
  return mi->mi_enabled;
}

static void
mpegts_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
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
mpegts_input_current_weight ( mpegts_input_t *mi )
{
  const mpegts_mux_instance_t *mmi;
  const service_t *s;
  const th_subscription_t *ths;
  int w = 0;

  // TODO: we probably need a way for mux level subs

  /* Check for scan (weight 1) */
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    if (mmi->mmi_mux->mm_initial_scan_status == MM_SCAN_CURRENT) {
      w = 1;
      break;
    }
  }

  /* Check subscriptions */
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
    LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link)
      w = MAX(w, ths->ths_weight);
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
  return w;
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

void
mpegts_input_open_service ( mpegts_input_t *mi, mpegts_service_t *s, int init )
{
  if (!init) return;
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  LIST_INSERT_HEAD(&mi->mi_transports, ((service_t*)s), s_active_link);
  s->s_dvb_active_input = mi;
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
}

void
mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  pthread_mutex_lock(&mi->mi_delivery_mutex);
  if (s->s_dvb_active_input != NULL) {
    LIST_REMOVE(((service_t*)s), s_active_link);
    s->s_dvb_active_input = NULL;
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
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
  mmi->mmi_mux->mm_active = mmi;
  LIST_INSERT_HEAD(&mi->mi_mux_active, mmi, mmi_active_link);
}

static void
mpegts_input_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  char buf[256];
  service_t *s, *t;
  mmi->mmi_mux->mm_active = NULL;
  LIST_REMOVE(mmi, mmi_active_link);

  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - flush subscribers", buf);
  s = LIST_FIRST(&mi->mi_transports);
  while (s) {
    t = s;
    s = LIST_NEXT(t, s_active_link);
    if (((mpegts_service_t*)s)->s_dvb_mux != mmi->mmi_mux)
      continue;
    service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
  }
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
  //assert(mmi->mmi_input == mi);
  assert(mm != NULL);
  assert(name != NULL);

  // TODO: get the input name
  tvhtrace("tsdemux", "%s - recv_packets tsb=%p, len=%d, pcr=%p, pcr_pid=%p",
           name, tsb, (int)len, pcr, pcr_pid);
  
  /* Not enough data */
  if (len < 188) return 0;
  
  /* Streaming - lock mutex */
  pthread_mutex_lock(&mi->mi_delivery_mutex);

  /* Raw stream */
  if (LIST_FIRST(&mmi->mmi_streaming_pad.sp_targets) != NULL) {
    streaming_message_t sm;
    pktbuf_t *pb = pktbuf_alloc(tsb, len);
    memset(&sm, 0, sizeof(sm));
    sm.sm_type = SMT_MPEGTS;
    sm.sm_data = pb;
    streaming_pad_deliver(&mmi->mmi_streaming_pad, &sm);
    pktbuf_ref_dec(pb);
  }
  
  /* Process */
  while ( len >= 188 ) {

    /* Sync */
    if ( tsb[i] == 0x47 ) {
      int pid = ((tsb[i+1] & 0x1f) << 8) | tsb[i+2];
      tvhtrace("tsdemux", "%s - recv_packet for pid %04X (%d)", name, pid, pid);

      /* SI data */
      if (mm->mm_table_filter[pid]) {
        if (!(tsb[i+1] & 0x80)) {
          mpegts_table_feed_t *mtf = malloc(sizeof(mpegts_table_feed_t));
          memcpy(mtf->mtf_tsb, tsb+i, 188);
          mtf->mtf_mux = mm;
          TAILQ_INSERT_TAIL(&mi->mi_table_feed, mtf, mtf_link);
          table_wakeup = 1;
        }
      
      /* Other */
      } else {
        service_t *s;
        int64_t *ppcr = (pcr_pid && *pcr_pid == pid) ? pcr : NULL;
        LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
          ts_recv_packet1((mpegts_service_t*)s, tsb+i, ppcr);
        }
        if (ppcr && *ppcr == PTS_UNSET)
          ts_recv_packet1(NULL, tsb+i, ppcr);
      }

      i   += 188;
      len -= 188;
    
    /* Re-sync */
    } else {
      tvhdebug("tsdemux", "%s - ts sync lost", name);
      if (ts_resync(tsb, &len, &i)) break;
      tvhdebug("tsdemux", "%s - ts sync found", name);
    }

  }

  /* Wake table */
  if (table_wakeup)
    pthread_cond_signal(&mi->mi_table_feed_cond);

  pthread_mutex_unlock(&mi->mi_delivery_mutex);

  /* Bandwidth monitoring */
  atomic_add(&mi->mi_bytes, i);
  
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
    if (!mt->mt_destroyed && mt->mt_pid == pid)
      mpegts_psi_section_reassemble(&mt->mt_sect, mtf->mtf_tsb, 0,
                                    mpegts_table_dispatch, mt);
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
    if (mtf->mtf_mux == mm)
      TAILQ_REMOVE(&mi->mi_table_feed, mtf, mtf_link);
    mtf  = next;
  }
  pthread_mutex_unlock(&mi->mi_delivery_mutex);
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
  idnode_insert(&mi->mi_id, uuid, class);
  if (c)
    idnode_load(&mi->mi_id, c);
  
  /* Defaults */
  mi->mi_is_enabled           = mpegts_input_is_enabled;
  mi->mi_display_name         = mpegts_input_display_name;
  mi->mi_is_free              = mpegts_input_is_free;
  mi->mi_current_weight       = mpegts_input_current_weight;
  mi->mi_start_mux            = mpegts_input_start_mux;
  mi->mi_stop_mux             = mpegts_input_stop_mux;
  mi->mi_open_service         = mpegts_input_open_service;
  mi->mi_close_service        = mpegts_input_close_service;
  mi->mi_network_class        = mpegts_input_network_class;
  mi->mi_network_create       = mpegts_input_network_create;
  mi->mi_create_mux_instance  = mpegts_input_create_mux_instance;
  mi->mi_started_mux          = mpegts_input_started_mux;
  mi->mi_stopped_mux          = mpegts_input_stopped_mux;

  /* Index */
  mi->mi_instance       = ++mpegts_input_idx;

  /* Init mutex */
  pthread_mutex_init(&mi->mi_delivery_mutex, NULL);
  
  /* Table input */
  TAILQ_INIT(&mi->mi_table_feed);
  pthread_cond_init(&mi->mi_table_feed_cond, NULL);

  /* Init input thread control */
  mi->mi_thread_pipe.rd = mi->mi_thread_pipe.wr = -1;

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_input_all, mi, mi_global_link);

  return mi;
}

void
mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *m )
{
  idnode_save(&mi->mi_id, m);
}

void
mpegts_input_set_network ( mpegts_input_t *mi, mpegts_network_t *mn )
{
  char buf1[256], buf2[265];
  if (mi->mi_network == mn) return;
  *buf1 = *buf2 = 0;
  if (mi->mi_display_name)
    mi->mi_display_name(mi, buf1, sizeof(buf1));
  if (mi->mi_network) {
    mi->mi_network->mn_display_name(mi->mi_network, buf2, sizeof(buf2));
    LIST_REMOVE(mi, mi_network_link);
    tvhdebug("mpegts", "%s - remove network %s", buf1, buf2);
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
