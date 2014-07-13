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

#include "input.h"
#include "tsdemux.h"
#include "packet.h"
#include "streaming.h"
#include "subscriptions.h"
#include "atomic.h"
#include "notify.h"
#include "idnode.h"

#include <pthread.h>
#include <assert.h>

SKEL_DECLARE(mpegts_pid_sub_skel, mpegts_pid_sub_t);

static void
mpegts_input_del_network ( mpegts_network_link_t *mnl );

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static const char *
mpegts_input_class_get_title ( idnode_t *in )
{
  static char buf[512];
  mpegts_input_t *mi = (mpegts_input_t*)in;
  mi->mi_display_name(mi, buf, sizeof(buf));
  return buf;
}

const void *
mpegts_input_class_network_get ( void *obj )
{
  mpegts_network_link_t *mnl;  
  mpegts_input_t *mi = obj;
  htsmsg_t       *l  = htsmsg_create_list();

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    htsmsg_add_str(l, NULL, idnode_uuid_as_str(&mnl->mnl_network->mn_id));

  return l;
}

int
mpegts_input_class_network_set ( void *obj, const void *p )
{
  return mpegts_input_set_networks(obj, (htsmsg_t*)p);
}

htsmsg_t *
mpegts_input_class_network_enum ( void *obj )
{
  htsmsg_t *p, *m;

  p = htsmsg_create_map();
  htsmsg_add_str (p, "uuid",    idnode_uuid_as_str((idnode_t*)obj));
  htsmsg_add_bool(p, "enum",    1);

  m = htsmsg_create_map();
  htsmsg_add_str (m, "type",    "api");
  htsmsg_add_str (m, "uri",     "mpegts/input/network_list");
  htsmsg_add_str (m, "event",   "mpegts_network");
  htsmsg_add_msg (m, "params",  p);
  return m;
}

char *
mpegts_input_class_network_rend ( void *obj )
{
  char *str;
  mpegts_network_link_t *mnl;  
  mpegts_input_t *mi = obj;
  htsmsg_t        *l = htsmsg_create_list();

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    htsmsg_add_str(l, NULL, idnode_get_title(&mnl->mnl_network->mn_id));

  str = htsmsg_list_2_csv(l);
  htsmsg_destroy(l);

  return str;
}

static void
mpegts_input_enabled_notify ( void *p )
{
  mpegts_input_t *mi = p;
  mpegts_mux_instance_t *mmi;

  /* Stop */
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1);

  /* Alert */
  if (mi->mi_enabled_updated)
    mi->mi_enabled_updated(mi);
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
      .notify   = mpegts_input_enabled_notify,
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
      .off      = offsetof(mpegts_input_t, mi_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_BOOL,
      .id       = "ota_epg",
      .name     = "Over-the-air EPG",
      .off      = offsetof(mpegts_input_t, mi_ota_epg),
      .def.i    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = "Networks",
      .islist   = 1,
      .set      = mpegts_input_class_network_set,
      .get      = mpegts_input_class_network_get,
      .list     = mpegts_input_class_network_enum,
      .rend     = mpegts_input_class_network_rend,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

int
mpegts_input_is_enabled ( mpegts_input_t *mi, mpegts_mux_t *mm,
                          const char *reason )
{
  if (!strcmp(reason, "epggrab") && !mi->mi_ota_epg)
    return 0;
  return mi->mi_enabled;
}

static void
mpegts_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  if (mi->mi_name)
    strncpy(buf, mi->mi_name, len);
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
  int w = 0, count = 0;

  /* Direct subs */
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    LIST_FOREACH(ths, &mmi->mmi_subs, ths_mmi_link) {
      w = MAX(w, ths->ths_weight);
      count++;
    }
  }

  /* Service subs */
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
    LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
      w = MAX(w, ths->ths_weight);
      count++;
    }
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  return w > 0 ? w + count - 1 : 0;
}

int
mpegts_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm )
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
  if (a->mps_type != b->mps_type) {
    if (a->mps_type & MPS_STREAM)
      return 1;
    else
      return -1;
  }
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
    SKEL_ALLOC(mpegts_pid_sub_skel);
    mpegts_pid_sub_skel->mps_type  = type;
    mpegts_pid_sub_skel->mps_owner = owner;
    if (!RB_INSERT_SORTED(&mp->mp_subs, mpegts_pid_sub_skel, mps_link, mps_cmp)) {
      mpegts_mux_nice_name(mm, buf, sizeof(buf));
      tvhdebug("mpegts", "%s - open PID %04X (%d) [%d/%p]",
               buf, mp->mp_pid, mp->mp_pid, type, owner);
      SKEL_USED(mpegts_pid_sub_skel);
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
  if (pid == mm->mm_last_pid) {
    mm->mm_last_pid = -1;
    mm->mm_last_mp = NULL;
  }
  if (mps) {
    RB_REMOVE(&mp->mp_subs, mps, mps_link);
    free(mps);

    if (!RB_FIRST(&mp->mp_subs)) {
      RB_REMOVE(&mm->mm_pids, mp, mp_link);
      if (mp->mp_fd != -1) {
        mpegts_mux_nice_name(mm, buf, sizeof(buf));
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
  pthread_mutex_lock(&mi->mi_output_lock);
  if (!s->s_dvb_active_input) {
    LIST_INSERT_HEAD(&mi->mi_transports, ((service_t*)s), s_active_link);
    s->s_dvb_active_input = mi;
  }

  /* Register PIDs */
  pthread_mutex_lock(&s->s_stream_mutex);
  mi->mi_open_pid(mi, s->s_dvb_mux, s->s_pmt_pid, MPS_STREAM, s);
  mi->mi_open_pid(mi, s->s_dvb_mux, s->s_pcr_pid, MPS_STREAM, s);
  /* Open only filtered components here */
  TAILQ_FOREACH(st, &s->s_filt_components, es_filt_link) {
    if (st->es_type != SCT_CA) {
      st->es_pid_opened = 1;
      mi->mi_open_pid(mi, s->s_dvb_mux, st->es_pid, MPS_STREAM, s);
    }
  }

  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_output_lock);

   /* Add PMT monitor */
  s->s_pmt_mon =
    mpegts_table_add(s->s_dvb_mux, DVB_PMT_BASE, DVB_PMT_MASK,
                     dvb_pmt_callback, s, "pmt",
                     MT_CRC, s->s_pmt_pid);
}

void
mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  elementary_stream_t *st;

  /* Close PMT table */
  if (s->s_pmt_mon)
    mpegts_table_destroy(s->s_pmt_mon);
  s->s_pmt_mon = NULL;

  /* Remove from list */
  pthread_mutex_lock(&mi->mi_output_lock);
  if (s->s_dvb_active_input != NULL) {
    LIST_REMOVE(((service_t*)s), s_active_link);
    s->s_dvb_active_input = NULL;
  }
  
  /* Close PID */
  pthread_mutex_lock(&s->s_stream_mutex);
  mi->mi_close_pid(mi, s->s_dvb_mux, s->s_pmt_pid, MPS_STREAM, s);
  mi->mi_close_pid(mi, s->s_dvb_mux, s->s_pcr_pid, MPS_STREAM, s);
  /* Close all opened PIDs (the component filter may be changed at runtime) */
  TAILQ_FOREACH(st, &s->s_components, es_link) {
    if (st->es_pid_opened) {
      st->es_pid_opened = 0;
      mi->mi_close_pid(mi, s->s_dvb_mux, st->es_pid, MPS_STREAM, s);
    }
  }


  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_output_lock);

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
  /* Deliver first TS packets as fast as possible */
  mi->mi_last_dispatch = 0;

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
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(t, &mi->mi_transports, s_active_link) {
    if (((mpegts_service_t*)t)->s_dvb_mux == mm) {
      ret = 1;
      break;
    }
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  return ret;
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static int inline
ts_sync_count ( const uint8_t *tsb, int len )
{
  int i = 0;
  while (len >= 188 && *tsb == 0x47) {
    ++i;
    len -= 188;
    tsb += 188;
  }
  return i;
}

void
mpegts_input_recv_packets
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, sbuf_t *sb,
    int64_t *pcr, uint16_t *pcr_pid )
{
  int i, p = 0, len2, off = 0;
  mpegts_packet_t *mp;
  uint8_t *tsb = sb->sb_data;
  int     len  = sb->sb_ptr;
#define MIN_TS_PKT 100
#define MIN_TS_SYN 5

  if (len < (MIN_TS_PKT * 188)) {
    /* For slow streams, check also against the clock */
    if (dispatch_clock == mi->mi_last_dispatch)
      return;
  }
  mi->mi_last_dispatch = dispatch_clock;

  /* Check for sync */
// could be a bit more efficient
  while ( (len >= (MIN_TS_SYN * 188)) &&
          ((p = ts_sync_count(tsb, len)) < MIN_TS_SYN) ) {
    mmi->mmi_stats.unc++;
    --len;
    ++tsb;
    ++off;
  }

  // Note: we check for sync here so that the buffer can always be
  //       processed in its entirety inside the processing thread
  //       without the potential need to buffer data (since that would
  //       require per mmi buffers, where this is generally not required)

  /* Extract PCR (used for tsfile playback) */
  if (pcr && pcr_pid) {
    uint8_t *tmp = tsb;
    for (i = 0; i < p; i++, tmp += 188) {
      uint16_t pid = ((tmp[1] & 0x1f) << 8) | tmp[2];
      if (*pcr_pid == MPEGTS_PID_NONE || *pcr_pid == pid) {
        ts_recv_packet1(NULL, tmp, pcr, 0);
        if (*pcr != PTS_UNSET) *pcr_pid = pid;
      }
    }
  }

  /* Pass */
  if (p >= MIN_TS_SYN) {
    len2 = p * 188;
    
    mp = malloc(sizeof(mpegts_packet_t) + len2);
    mp->mp_mux  = mmi->mmi_mux;
    mp->mp_len  = len2;
    memcpy(mp->mp_data, tsb, len2);

    len -= len2;
    off += len2;

    pthread_mutex_lock(&mi->mi_input_lock);
    if (TAILQ_FIRST(&mi->mi_input_queue) == NULL)
      pthread_cond_signal(&mi->mi_input_cond);
    TAILQ_INSERT_TAIL(&mi->mi_input_queue, mp, mp_link);
    pthread_mutex_unlock(&mi->mi_input_lock);
  }

  /* Adjust buffer */
  if (len)
    sbuf_cut(sb, off); // cut off the bottom
  else
    sb->sb_ptr = 0;    // clear
}

static void
mpegts_input_table_dispatch ( mpegts_mux_t *mm, const uint8_t *tsb )
{
  int i, len = 0;
  uint16_t pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
  uint8_t  cc  = (tsb[3] & 0x0f);
  mpegts_table_t *mt, **vec;

  /* Collate - tables may be removed during callbacks */
  pthread_mutex_lock(&mm->mm_tables_lock);
  i = mm->mm_num_tables;
  vec = alloca(i * sizeof(mpegts_table_t *));
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (mt->mt_destroyed || !mt->mt_subscribed)
      continue;
    mpegts_table_grab(mt);
    if (len < i)
      vec[len++] = mt;
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);

  /* Process */
  for (i = 0; i < len; i++) {
    mt = vec[i];
    if (!mt->mt_destroyed && mt->mt_pid == pid) {
      if (tsb[3] & 0x10) {
        int ccerr = 0;
        if (mt->mt_cc != -1 && mt->mt_cc != cc) {
          ccerr = 1;
          /* Ignore dupes (shouldn't have payload set, but some seem to) */
          //if (((mt->mt_cc + 15) & 0xf) != cc)
          tvhdebug("psi", "PID %04X CC error %d != %d", pid, cc, mt->mt_cc);
         }
        mt->mt_cc = (cc + 1) & 0xF;
        mpegts_psi_section_reassemble(&mt->mt_sect, tsb, 0, ccerr,
                                      mpegts_table_dispatch, mt);
      }
    }
    mpegts_table_release(mt);
  }
}

static void
mpegts_input_table_waiting ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_table_t *mt;
  int type;

  pthread_mutex_lock(&mm->mm_tables_lock);
  while ((mt = LIST_FIRST(&mm->mm_defer_tables)) != NULL) {
    LIST_REMOVE(mt, mt_defer_link);
    if (mt->mt_destroyed)
      continue;
    type = 0;
    if (mt->mt_flags & MT_FAST) type |= MPS_FTABLE;
    if (mt->mt_flags & MT_SLOW) type |= MPS_TABLE;
    if (mt->mt_flags & MT_RECORD) type |= MPS_STREAM;
    if ((type & (MPS_FTABLE | MPS_TABLE)) == 0) type |= MPS_TABLE;
    if (mt->mt_defer_cmd == 1) {
      mt->mt_defer_cmd = 0;
      mt->mt_defer_reg = 1;
      LIST_INSERT_HEAD(&mm->mm_tables, mt, mt_link);
      mm->mm_num_tables++;
      if (!mt->mt_subscribed) {
        mt->mt_subscribed = 1;
        pthread_mutex_unlock(&mm->mm_tables_lock);
        mi->mi_open_pid(mi, mm, mt->mt_pid, type, mt);
      }
    } else if (mt->mt_defer_cmd == 2) {
      mt->mt_defer_cmd = 0;
      mt->mt_defer_reg = 0;
      LIST_REMOVE(mt, mt_link);
      mm->mm_num_tables--;
      if (mt->mt_subscribed) {
        mt->mt_subscribed = 0;
        pthread_mutex_unlock(&mm->mm_tables_lock);
        mi->mi_close_pid(mi, mm, mt->mt_pid, type, mt);
      }
    } else {
      pthread_mutex_unlock(&mm->mm_tables_lock);
    }
    mpegts_table_release(mt);
    pthread_mutex_lock(&mm->mm_tables_lock);
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);
}

static void
mpegts_input_process
  ( mpegts_input_t *mi, mpegts_packet_t *mpkt )
{
  uint16_t pid;
  uint8_t cc;
  uint8_t *tsb = mpkt->mp_data;
  int len = mpkt->mp_len;
  int table, stream, f;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;
  service_t *s;
  int table_wakeup = 0;
  uint8_t *end = mpkt->mp_data + len;
  mpegts_mux_t          *mm  = mpkt->mp_mux;
  mpegts_mux_instance_t *mmi = mm->mm_active;
  mpegts_pid_t *last_mp = NULL;

  /* Process */
  assert((len % 188) == 0);
  while ( tsb < end ) {
    pid = (tsb[1] << 8) | tsb[2];
    cc  = tsb[3];

    /* Transport error */
    if (pid & 0x8000) {
      if ((pid & 0x1FFF) != 0x1FFF)
        ++mmi->mmi_stats.te;
    }
    
    pid &= 0x1FFF;
    
    /* Ignore NUL packets */
    if (pid == 0x1FFF) goto done;

    /* Find PID */
    if ((mp = mpegts_mux_find_pid(mm, pid, 0))) {

      /* Low level CC check */
      if (cc & 0x10) {
        cc  &= 0x0f;
        if (mp->mp_cc != -1 && mp->mp_cc != cc) {
          tvhtrace("mpegts", "pid %04X cc err %2d != %2d", pid, cc, mp->mp_cc);
          ++mmi->mmi_stats.cc;
        }
        mp->mp_cc = (cc + 1) & 0xF;
      }

      // Note: there is a minor danger this caching will get things
      //       wrong for a brief period of time if the registrations on
      //       the PID change
      if (mp != last_mp) {
        if (pid == 0) {
          stream = MPS_STREAM;
          table  = MPS_TABLE;
        } else {
          stream = table = 0;

          /* Determine PID type */
          RB_FOREACH(mps, &mp->mp_subs, mps_link) {
            stream |= mps->mps_type & MPS_STREAM;
            table  |= mps->mps_type & (MPS_TABLE | MPS_FTABLE);
            if (table == (MPS_TABLE|MPS_FTABLE) && stream) break;
          }

          /* Special case streams */
          LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
            if (((mpegts_service_t*)s)->s_dvb_mux != mmi->mmi_mux) continue;
                 if (pid == s->s_pmt_pid) stream = MPS_STREAM;
            else if (pid == s->s_pcr_pid) stream = MPS_STREAM;
          }
        }
      }
    
      /* Stream data */
      if (stream) {
        LIST_FOREACH(s, &mi->mi_transports, s_active_link) {
          if (((mpegts_service_t*)s)->s_dvb_mux != mmi->mmi_mux) continue;
          f = table || (pid == s->s_pmt_pid) || (pid == s->s_pcr_pid);
          ts_recv_packet1((mpegts_service_t*)s, tsb, NULL, f);
        }
      }

      /* Table data */
      if (table) {
        if (!(tsb[1] & 0x80)) {
          if (table & MPS_FTABLE)
            mpegts_input_table_dispatch(mm, tsb);
          if (table & MPS_TABLE) {
            // TODO: might be able to optimise this a bit by having slightly
            //       larger buffering and trying to aggregate data (if we get
            //       same PID multiple times in the loop)
            mpegts_table_feed_t *mtf = malloc(sizeof(mpegts_table_feed_t));
            memcpy(mtf->mtf_tsb, tsb, 188);
            mtf->mtf_mux   = mm;
            TAILQ_INSERT_TAIL(&mi->mi_table_queue, mtf, mtf_link);
            table_wakeup = 1;
          }
        } else {
          //tvhdebug("tsdemux", "%s - SI packet had errors", name);
        }
      }
    }

done:
    tsb += 188;
  }

  /* Raw stream */
  if (tsb != mpkt->mp_data &&
      LIST_FIRST(&mmi->mmi_streaming_pad.sp_targets) != NULL) {
    streaming_message_t sm;
    pktbuf_t *pb = pktbuf_alloc(mpkt->mp_data, tsb - mpkt->mp_data);
    memset(&sm, 0, sizeof(sm));
    sm.sm_type = SMT_MPEGTS;
    sm.sm_data = pb;
    streaming_pad_deliver(&mmi->mmi_streaming_pad, &sm);
    pktbuf_ref_dec(pb);
  }

  /* Wake table */
  if (table_wakeup)
    pthread_cond_signal(&mi->mi_table_cond);

  /* Bandwidth monitoring */
  atomic_add(&mmi->mmi_stats.bps, tsb - mpkt->mp_data);
}

static void *
mpegts_input_thread ( void * p )
{
  mpegts_packet_t *mp;
  mpegts_input_t  *mi = p;

  pthread_mutex_lock(&mi->mi_input_lock);
  while (mi->mi_running) {

    /* Wait for a packet */
    if (!(mp = TAILQ_FIRST(&mi->mi_input_queue))) {
      pthread_cond_wait(&mi->mi_input_cond, &mi->mi_input_lock);
      continue;
    }
    TAILQ_REMOVE(&mi->mi_input_queue, mp, mp_link);
    pthread_mutex_unlock(&mi->mi_input_lock);
      
    /* Process */
    pthread_mutex_lock(&mi->mi_output_lock);
    if (mp->mp_mux && mp->mp_mux->mm_active) {
      mpegts_input_table_waiting(mi, mp->mp_mux);
      mpegts_input_process(mi, mp);
    }
    pthread_mutex_unlock(&mi->mi_output_lock);

    /* Cleanup */
    free(mp);
    pthread_mutex_lock(&mi->mi_input_lock);
  }

  /* Flush */
  while ((mp = TAILQ_FIRST(&mi->mi_input_queue))) {
    TAILQ_REMOVE(&mi->mi_input_queue, mp, mp_link);
    free(mp);
  }
  pthread_mutex_unlock(&mi->mi_input_lock);

  return NULL;
}

static void *
mpegts_input_table_thread ( void *aux )
{
  mpegts_table_feed_t   *mtf;
  mpegts_input_t        *mi = aux;

  pthread_mutex_lock(&mi->mi_output_lock);
  while (mi->mi_running) {

    /* Wait for data */
    if (!(mtf = TAILQ_FIRST(&mi->mi_table_queue))) {
      pthread_cond_wait(&mi->mi_table_cond, &mi->mi_output_lock);
      continue;
    }
    TAILQ_REMOVE(&mi->mi_table_queue, mtf, mtf_link);
    pthread_mutex_unlock(&mi->mi_output_lock);
    
    /* Process */
    if (mtf->mtf_mux) {
      pthread_mutex_lock(&global_lock);
      mpegts_input_table_dispatch(mtf->mtf_mux, mtf->mtf_tsb);
      pthread_mutex_unlock(&global_lock);
    }

    /* Cleanup */
    free(mtf);
    pthread_mutex_lock(&mi->mi_output_lock);
  }

  /* Flush */
  while ((mtf = TAILQ_FIRST(&mi->mi_table_queue)) != NULL) {
    TAILQ_REMOVE(&mi->mi_table_queue, mtf, mtf_link);
    free(mtf);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);

  return NULL;
}

void
mpegts_input_flush_mux
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_table_feed_t *mtf;
  mpegts_packet_t *mp;

  // Note: to avoid long delays in here, rather than actually
  //       remove things from the Q, we simply invalidate by clearing
  //       the mux pointer and allow the threads to deal with the deletion

  /* Flush input Q */
  pthread_mutex_lock(&mi->mi_input_lock);
  TAILQ_FOREACH(mp, &mi->mi_input_queue, mp_link) {
    if (mp->mp_mux == mm)
      mp->mp_mux = NULL;
  }
  pthread_mutex_unlock(&mi->mi_input_lock);

  /* Flush table Q */
  pthread_mutex_lock(&mi->mi_output_lock);
  TAILQ_FOREACH(mtf, &mi->mi_table_queue, mtf_link) {
    if (mtf->mtf_mux == mm)
      mtf->mtf_mux = NULL;
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
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

  st->uuid        = strdup(idnode_uuid_as_str(&mmi->mmi_id));
  mi->mi_display_name(mi, buf, sizeof(buf));
  st->input_name  = strdup(buf);
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
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

  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    st = calloc(1, sizeof(tvh_input_stream_t));
    mpegts_input_stream_status(mmi, st);
    LIST_INSERT_HEAD(isl, st, link);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
}

static void
mpegts_input_thread_start ( mpegts_input_t *mi )
{
  mi->mi_running = 1;
  
  tvhthread_create(&mi->mi_table_tid, NULL,
                   mpegts_input_table_thread, mi);
  tvhthread_create(&mi->mi_input_tid, NULL,
                   mpegts_input_thread, mi);
}

static void
mpegts_input_thread_stop ( mpegts_input_t *mi )
{
  mi->mi_running = 0;

  /* Stop input thread */
  pthread_mutex_lock(&mi->mi_input_lock);
  pthread_cond_signal(&mi->mi_input_cond);
  pthread_mutex_unlock(&mi->mi_input_lock);

  /* Stop table thread */
  pthread_mutex_lock(&mi->mi_output_lock);
  pthread_cond_signal(&mi->mi_table_cond);
  pthread_mutex_unlock(&mi->mi_output_lock);

  /* Join threads (relinquish lock due to potential deadlock) */
  pthread_mutex_unlock(&global_lock);
  pthread_join(mi->mi_input_tid, NULL);
  pthread_join(mi->mi_table_tid, NULL);
  pthread_mutex_lock(&global_lock);
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
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    memset(&st, 0, sizeof(st));
    mpegts_input_stream_status(mmi, &st);
    e = tvh_input_stream_create_msg(&st);
    htsmsg_add_u32(e, "update", 1);
    notify_by_msg("input_status", e);
    tvh_input_stream_destroy(&st);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
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

  /* Init input/output structures */
  pthread_mutex_init(&mi->mi_input_lock, NULL);
  pthread_cond_init(&mi->mi_input_cond, NULL);
  TAILQ_INIT(&mi->mi_input_queue);

  pthread_mutex_init(&mi->mi_output_lock, NULL);
  pthread_cond_init(&mi->mi_table_cond, NULL);
  TAILQ_INIT(&mi->mi_table_queue);

  /* Defaults */
  mi->mi_ota_epg = 1;

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_input_all, mi, mi_global_link);

  /* Load config */
  if (c)
    idnode_load(&mi->ti_id, c);

  /* Start threads */
  mpegts_input_thread_start(mi);

  return mi;
}

void
mpegts_input_stop_all ( mpegts_input_t *mi )
{
  mpegts_mux_instance_t *mmi;
  while ((mmi = LIST_FIRST(&mi->mi_mux_active)))
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1);
}

void
mpegts_input_delete ( mpegts_input_t *mi, int delconf )
{
  mpegts_network_link_t *mnl;
  mpegts_mux_instance_t *mmi, *mmi_next;

  /* Remove networks */
  while ((mnl = LIST_FIRST(&mi->mi_networks)))
    mpegts_input_del_network(mnl);

  /* Remove mux instances assigned to this input */
  mmi = LIST_FIRST(&mi->mi_mux_instances);
  while (mmi) {
    mmi_next = LIST_NEXT(mmi, mmi_input_link);
    if (mmi->mmi_input == mi)
      mmi->mmi_delete(mmi);
    mmi = mmi_next;
  }

  /* Remove global refs */
  idnode_unlink(&mi->ti_id);
  LIST_REMOVE(mi, ti_link);
  LIST_REMOVE(mi, mi_global_link);

  /* Stop threads (will unlock global_lock to join) */
  mpegts_input_thread_stop(mi);

  pthread_mutex_destroy(&mi->mi_output_lock);
  pthread_cond_destroy(&mi->mi_table_cond);
  free(mi->mi_name);
  free(mi);
}

void
mpegts_input_save ( mpegts_input_t *mi, htsmsg_t *m )
{
  idnode_save(&mi->ti_id, m);
}

int
mpegts_input_add_network ( mpegts_input_t *mi, mpegts_network_t *mn )
{
  mpegts_network_link_t *mnl;

  /* Find existing */
  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    if (mnl->mnl_network == mn)
      break;

  /* Clear mark */
  if (mnl) {
    mnl->mnl_mark = 0;
    return 0;
  }

  /* Create new */
  mnl = calloc(1, sizeof(mpegts_network_link_t));
  mnl->mnl_input    = mi;
  mnl->mnl_network  = mn;
  LIST_INSERT_HEAD(&mi->mi_networks, mnl, mnl_mi_link);
  LIST_INSERT_HEAD(&mn->mn_inputs,   mnl, mnl_mn_link);
  idnode_notify_simple(&mnl->mnl_network->mn_id);
  return 1;
}

static void
mpegts_input_del_network ( mpegts_network_link_t *mnl )
{
  idnode_notify_simple(&mnl->mnl_network->mn_id);
  LIST_REMOVE(mnl, mnl_mn_link);
  LIST_REMOVE(mnl, mnl_mi_link);
  free(mnl);
}

int
mpegts_input_set_networks ( mpegts_input_t *mi, htsmsg_t *msg )
{
  int save = 0;
  const char *str;
  htsmsg_field_t *f;
  mpegts_network_t *mn;
  mpegts_network_link_t *mnl, *nxt;

  /* Mark for deletion */
  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    mnl->mnl_mark = 1;

  /* Link */
  HTSMSG_FOREACH(f, msg) {
    if (!(str = htsmsg_field_get_str(f))) continue;
    if (!(mn = mpegts_network_find(str))) continue;
    save |= mpegts_input_add_network(mi, mn);
  }

  /* Unlink */
  for (mnl = LIST_FIRST(&mi->mi_networks); mnl != NULL; mnl = nxt) {
    nxt = LIST_NEXT(mnl, mnl_mi_link);
    if (mnl->mnl_mark) {
      mpegts_input_del_network(mnl);
      save = 1;
    }
  }
  
  return save;
}

int mpegts_input_grace( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  /* Get timeout */
  int t = 0;
  if (mi && mi->mi_get_grace)
    t = mi->mi_get_grace(mi, mm);
  if (t < 5) t = 5; // lower bound
  return t;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
