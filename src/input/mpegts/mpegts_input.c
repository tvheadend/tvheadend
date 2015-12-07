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
#include "access.h"
#include "atomic.h"
#include "notify.h"
#include "idnode.h"
#include "dbus.h"

#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>


static void
mpegts_input_del_network ( mpegts_network_link_t *mnl );

/*
 * DBUS
 */

static void
mpegts_input_dbus_notify(mpegts_input_t *mi, int64_t subs)
{
#if ENABLE_DBUS_1
  char buf[256], ubuf[UUID_HEX_SIZE];
  htsmsg_t *msg;

  if (mi->mi_dbus_subs == subs)
    return;
  mi->mi_dbus_subs = subs;
  msg = htsmsg_create_list();
  mi->mi_display_name(mi, buf, sizeof(buf));
  htsmsg_add_str(msg, NULL, buf);
  htsmsg_add_s64(msg, NULL, subs);
  snprintf(buf, sizeof(buf), "/input/mpegts/%s", idnode_uuid_as_str(&mi->ti_id, ubuf));
  dbus_emit_signal(buf, "status", msg);
#endif
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static const char *
mpegts_input_class_get_title ( idnode_t *in, const char *lang )
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
  char ubuf[UUID_HEX_SIZE];

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    htsmsg_add_str(l, NULL, idnode_uuid_as_str(&mnl->mnl_network->mn_id, ubuf));

  return l;
}

int
mpegts_input_class_network_set ( void *obj, const void *p )
{
  return mpegts_input_set_networks(obj, (htsmsg_t*)p);
}

htsmsg_t *
mpegts_input_class_network_enum ( void *obj, const char *lang )
{
  htsmsg_t *p, *m;
  char ubuf[UUID_HEX_SIZE];

  if (!obj)
    return NULL;

  p = htsmsg_create_map();
  htsmsg_add_str (p, "uuid",    idnode_uuid_as_str((idnode_t*)obj, ubuf));
  htsmsg_add_bool(p, "enum",    1);

  m = htsmsg_create_map();
  htsmsg_add_str (m, "type",    "api");
  htsmsg_add_str (m, "uri",     "mpegts/input/network_list");
  htsmsg_add_str (m, "event",   "mpegts_network");
  htsmsg_add_msg (m, "params",  p);
  return m;
}

char *
mpegts_input_class_network_rend ( void *obj, const char *lang )
{
  char *str;
  mpegts_network_link_t *mnl;  
  mpegts_input_t *mi = obj;
  htsmsg_t        *l = htsmsg_create_list();

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    htsmsg_add_str(l, NULL, idnode_get_title(&mnl->mnl_network->mn_id, lang));

  str = htsmsg_list_2_csv(l, ',', 1);
  htsmsg_destroy(l);

  return str;
}

static void
mpegts_input_enabled_notify ( void *p, const char *lang )
{
  mpegts_input_t *mi = p;
  mpegts_mux_instance_t *mmi;

  /* Stop */
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1, SM_CODE_ABORTED);

  /* Alert */
  if (mi->mi_enabled_updated)
    mi->mi_enabled_updated(mi);
}

static int
mpegts_input_class_linked_set ( void *self, const void *val )
{
  mpegts_input_t *mi = self, *mi2;
  char ubuf[UUID_HEX_SIZE];

  if (strcmp(val ?: "", mi->mi_linked ?: "")) {
    mi2 = mpegts_input_find(mi->mi_linked);
    free(mi->mi_linked);
    mi->mi_linked = NULL;
    if (mi2) {
      free(mi2->mi_linked);
      mi2->mi_linked = NULL;
      mpegts_mux_unsubscribe_linked(mi2, NULL);
    }
    mpegts_mux_unsubscribe_linked(mi, NULL);
    if (val && ((char *)val)[0]) {
      mi->mi_linked = strdup((char *)val);
      mi2 = mpegts_input_find((char *)val);
      if (mi2) {
        free(mi2->mi_linked);
        mi2->mi_linked = strdup(idnode_uuid_as_str(&mi->ti_id, ubuf));
      }
    }
    if (mi2)
      idnode_changed(&mi2->ti_id);
    return 1;
  }
  return 0;
}

static const void *
mpegts_input_class_linked_get ( void *self )
{
  mpegts_input_t *mi = self;
  prop_sbuf[0] = '\0';
  if (mi->mi_linked) {
    mi = mpegts_input_find(mi->mi_linked);
    if (mi)
      idnode_uuid_as_str(&mi->ti_id, prop_sbuf);
  }
  return &prop_sbuf_ptr;
}

static void
mpegts_input_add_keyval(htsmsg_t *l, const char *key, const char *val)
{
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_str(e, "key", key);
  htsmsg_add_str(e, "val", val);
  htsmsg_add_msg(l, NULL, e);
}

static htsmsg_t *
mpegts_input_class_linked_enum( void * self, const char *lang )
{
  mpegts_input_t *mi = self, *mi2;
  tvh_input_t *ti;
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *m = htsmsg_create_list();
  mpegts_input_add_keyval(m, "", tvh_gettext_lang(lang, N_("Not linked")));
  TVH_INPUT_FOREACH(ti)
    if (idnode_is_instance(&ti->ti_id, &mpegts_input_class)) {
      mi2 = (mpegts_input_t *)ti;
      if (mi2 != mi)
        mpegts_input_add_keyval(m, idnode_uuid_as_str(&ti->ti_id, ubuf),
                                   idnode_get_title(&mi2->ti_id, lang));
  }
  return m;
}

const idclass_t mpegts_input_class =
{
  .ic_super      = &tvh_input_class,
  .ic_class      = "mpegts_input",
  .ic_caption    = N_("MPEG-TS input"),
  .ic_event      = "mpegts_input",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_get_title  = mpegts_input_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(mpegts_input_t, mi_enabled),
      .notify   = mpegts_input_enabled_notify,
      .def.i    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .off      = offsetof(mpegts_input_t, mi_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming priority"),
      .off      = offsetof(mpegts_input_t, mi_streaming_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = N_("Name"),
      .off      = offsetof(mpegts_input_t, mi_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_BOOL,
      .id       = "ota_epg",
      .name     = N_("Over-the-air EPG"),
      .off      = offsetof(mpegts_input_t, mi_ota_epg),
      .def.i    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "initscan",
      .name     = N_("Initial scan"),
      .off      = offsetof(mpegts_input_t, mi_initscan),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "idlescan",
      .name     = N_("Idle scan"),
      .off      = offsetof(mpegts_input_t, mi_idlescan),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "free_weight",
      .name     = N_("Free subscription weight"),
      .desc     = N_("If the subscription weight for this input is bellow "
                     "the specified threshold, the tuner is handled as free "
                     "(according the priority settings). Otherwise, a next "
                     "tuner (without any subscriptions) is used. Set this value "
                     "to 10, if you are willing to override scan and epggrab "
                     "subscriptions."),
      .off      = offsetof(mpegts_input_t, mi_free_weight),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = N_("Networks"),
      .islist   = 1,
      .set      = mpegts_input_class_network_set,
      .get      = mpegts_input_class_network_get,
      .list     = mpegts_input_class_network_enum,
      .rend     = mpegts_input_class_network_rend,
    },
    {
      .type     = PT_STR,
      .id       = "linked",
      .name     = N_("Linked input"),
      .set      = mpegts_input_class_linked_set,
      .get      = mpegts_input_class_linked_get,
      .list     = mpegts_input_class_linked_enum,
      .opts     = PO_ADVANCED,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

int
mpegts_input_is_enabled ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  if ((flags & SUBSCRIPTION_EPG) != 0 && !mi->mi_ota_epg)
    return 0;
  if ((flags & SUBSCRIPTION_INITSCAN) != 0 && !mi->mi_initscan)
    return 0;
  if ((flags & SUBSCRIPTION_IDLESCAN) != 0 && !mi->mi_idlescan)
    return 0;
  return mi->mi_enabled;
}

static void
mpegts_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  if (mi->mi_name) {
    strncpy(buf, mi->mi_name, len - 1);
    buf[len - 1] = '\0';
  } else
    *buf = 0;
}

int
mpegts_input_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  const mpegts_mux_instance_t *mmi;
  const service_t *s;
  const th_subscription_t *ths;
  int w = 0, count = 0;

  /* Service subs */
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    LIST_FOREACH(s, &mmi->mmi_mux->mm_transports, s_active_link)
      LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
        w = MAX(w, ths->ths_weight);
        count++;
      }
  pthread_mutex_unlock(&mi->mi_output_lock);
  return w > 0 ? w + count - 1 : 0;
}

int
mpegts_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  if (flags & SUBSCRIPTION_STREAMING) {
    if (mi->mi_streaming_priority > 0)
      return mi->mi_streaming_priority;
  }
  return mi->mi_priority;
}

static int
mpegts_input_warm_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  mpegts_mux_instance_t *cur;

  cur = LIST_FIRST(&mi->mi_mux_active);
  if (cur != NULL) {
    /* Already tuned */
    if (mmi == cur)
      return 0;

    /* Stop current */
    cur->mmi_mux->mm_stop(cur->mmi_mux, 1, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
  }
  if (LIST_FIRST(&mi->mi_mux_active))
    return SM_CODE_TUNING_FAILED;
  return 0;
}

static int
mpegts_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  return SM_CODE_TUNING_FAILED;
}

static void
mpegts_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
}

int
mpegts_mps_cmp ( mpegts_pid_sub_t *a, mpegts_pid_sub_t *b )
{
  if (a->mps_type != b->mps_type) {
    if (a->mps_type & MPS_SERVICE)
      return 1;
    else
      return -1;
  }
  if (a->mps_owner < b->mps_owner) return -1;
  if (a->mps_owner > b->mps_owner) return 1;
  return 0;
}

void
mpegts_input_close_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm, void *owner, int all )
{
  mpegts_pid_t *mp, *mp_next;
  mpegts_pid_sub_t *mps, *mps_next;
  int pid;

  if (all)
    for (mps = LIST_FIRST(&mm->mm_all_subs); mps; mps = mps_next) {
      mps_next = LIST_NEXT(mps, mps_svcraw_link);
      if (mps->mps_owner != owner) continue;
      pid = MPEGTS_FULLMUX_PID;
      if (mps->mps_type & MPS_TABLES) pid = MPEGTS_TABLES_PID;
      mpegts_input_close_pid(mi, mm, pid, mps->mps_type, mps->mps_weight, mps->mps_owner);
    }
  for (mp = RB_FIRST(&mm->mm_pids); mp; mp = mp_next) {
    mp_next = RB_NEXT(mp, mp_link);
    for (mps = RB_FIRST(&mp->mp_subs); mps; mps = mps_next) {
      mps_next = RB_NEXT(mps, mps_link);
      if (mps->mps_owner != owner) continue;
      mpegts_input_close_pid(mi, mm, mp->mp_pid, mps->mps_type, mps->mps_weight, mps->mps_owner);
    }
  }
}

mpegts_pid_t *
mpegts_input_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight, void *owner )
{
  char buf[512];
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps, *mps2;

  assert(owner != NULL);
  assert((type & (MPS_STREAM|MPS_SERVICE|MPS_RAW)) == 0 ||
         (((type & MPS_STREAM) ? 1 : 0) +
          ((type & MPS_SERVICE) ? 1 : 0) +
          ((type & MPS_RAW) ? 1 : 0)) == 1);
  lock_assert(&mi->mi_output_lock);

  if (pid == MPEGTS_FULLMUX_PID)
    mpegts_input_close_pids(mi, mm, owner, 1);

  if ((mp = mpegts_mux_find_pid(mm, pid, 1))) {
    mps = calloc(1, sizeof(*mps));
    mps->mps_type   = type;
    mps->mps_weight = weight;
    mps->mps_owner  = owner;
    if (pid == MPEGTS_FULLMUX_PID || pid == MPEGTS_TABLES_PID) {
      mp->mp_type |= type;
      LIST_FOREACH(mps2, &mm->mm_all_subs, mps_svcraw_link)
        if (mps2->mps_owner == owner) break;
      if (mps2 == NULL) {
        LIST_INSERT_HEAD(&mm->mm_all_subs, mps, mps_svcraw_link);
        mpegts_mux_nice_name(mm, buf, sizeof(buf));
        tvhdebug("mpegts", "%s - open PID %s subscription [%04x/%p]",
                 buf, (type & MPS_TABLES) ? "tables" : "fullmux", type, owner);
        mm->mm_update_pids_flag = 1;
      }
    } else if (!RB_INSERT_SORTED(&mp->mp_subs, mps, mps_link, mpegts_mps_cmp)) {
      mp->mp_type |= type;
      if (type & (MPS_SERVICE|MPS_RAW))
        LIST_INSERT_HEAD(&mp->mp_svc_subs, mps, mps_svcraw_link);
      mpegts_mux_nice_name(mm, buf, sizeof(buf));
      tvhdebug("mpegts", "%s - open PID %04X (%d) [%d/%p]",
               buf, mp->mp_pid, mp->mp_pid, type, owner);
      mm->mm_update_pids_flag = 1;
    } else {
      free(mps);
      mp = NULL;
    }
  }
  return mp;
}

int
mpegts_input_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight, void *owner )
{
  char buf[512];
  mpegts_pid_sub_t *mps, skel;
  mpegts_pid_t *mp;
  int mask;
  assert(owner != NULL);
  lock_assert(&mi->mi_output_lock);
  if (!(mp = mpegts_mux_find_pid(mm, pid, 0)))
    return -1;
  if (pid == MPEGTS_FULLMUX_PID || pid == MPEGTS_TABLES_PID) {
    mpegts_mux_nice_name(mm, buf, sizeof(buf));
    LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
      if (mps->mps_owner == owner) break;
    if (mps == NULL) return -1;
    tvhdebug("mpegts", "%s - close PID %s subscription [%04x/%p]",
             buf, pid == MPEGTS_TABLES_PID ? "tables" : "fullmux",
             type, owner);
    if (pid == MPEGTS_FULLMUX_PID)
      mpegts_input_close_pids(mi, mm, owner, 0);
    LIST_REMOVE(mps, mps_svcraw_link);
    free(mps);
    mm->mm_update_pids_flag = 1;
    mask = pid == MPEGTS_FULLMUX_PID ? MPS_ALL : MPS_TABLES;
    LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
      if (mps->mps_type & mask) break;
    if (mps) return 0;
  } else {
    skel.mps_type   = type;
    skel.mps_weight = weight;
    skel.mps_owner  = owner;
    mps = RB_FIND(&mp->mp_subs, &skel, mps_link, mpegts_mps_cmp);
    if (pid == mm->mm_last_pid) {
      mm->mm_last_pid = -1;
      mm->mm_last_mp = NULL;
    }
    if (mps) {
      mpegts_mux_nice_name(mm, buf, sizeof(buf));
      tvhdebug("mpegts", "%s - close PID %04X (%d) [%d/%p]",
               buf, mp->mp_pid, mp->mp_pid, type, owner);
      if (type & (MPS_SERVICE|MPS_RAW))
        LIST_REMOVE(mps, mps_svcraw_link);
      RB_REMOVE(&mp->mp_subs, mps, mps_link);
      free(mps);
      mm->mm_update_pids_flag = 1;
    }
  }
  if (!RB_FIRST(&mp->mp_subs)) {
    RB_REMOVE(&mm->mm_pids, mp, mp_link);
    free(mp);
    return 1;
  } else {
    type = 0;
    RB_FOREACH(mps, &mp->mp_subs, mps_link)
      type |= mps->mps_type;
    mp->mp_type = type;
  }
  return 0;
}

static void
mpegts_input_update_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  /* nothing - override */
}

static int mps_weight(elementary_stream_t *st)
{
   if (SCT_ISVIDEO(st->es_type))
     return MPS_WEIGHT_VIDEO + MIN(st->es_index, 49);
   else if (SCT_ISAUDIO(st->es_type))
     return MPS_WEIGHT_AUDIO + MIN(st->es_index, 49);
   else if (SCT_ISSUBTITLE(st->es_type))
     return MPS_WEIGHT_SUBTITLE + MIN(st->es_index, 49);
   else
     return MPS_WEIGHT_ESOTHER + MIN(st->es_index, 49);
}

static int
mpegts_input_cat_pass_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  uint8_t dtag, dlen;
  uint16_t pid;
  uintptr_t caid;
  mpegts_mux_t             *mm  = mt->mt_mux;
  mpegts_psi_table_state_t *st  = NULL;
  service_t                *s   = mt->mt_opaque;
  elementary_stream_t      *es;
  mpegts_input_t           *mi;

  /* Start */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, 0, 5, &st, &sect, &last, &ver);
  if (r != 1) return r;
  ptr += 5;
  len -= 5;

  /* Send CAT data for descramblers */
  descrambler_cat_data(mm, ptr, len);

  while(len > 2) {
    dtag = *ptr++;
    dlen = *ptr++;
    len -= 2;

    switch(dtag) {
      case DVB_DESC_CA:
        if (len >= 4 && dlen >= 4 && mm->mm_active) {
          caid = ( ptr[0]         << 8) | ptr[1];
          pid  = ((ptr[2] & 0x1f) << 8) | ptr[3];
          tvhdebug("cat", "  pass: caid %04X (%d) pid %04X (%d)",
                   (uint16_t)caid, (uint16_t)caid, pid, pid);
          pthread_mutex_lock(&s->s_stream_mutex);
          es = NULL;
          if (service_stream_find((service_t *)s, pid) == NULL) {
            es = service_stream_create(s, pid, SCT_CA);
            es->es_pid_opened = 1;
          }
          pthread_mutex_unlock(&s->s_stream_mutex);
          if (es && mm->mm_active && (mi = mm->mm_active->mmi_input) != NULL) {
            pthread_mutex_lock(&mi->mi_output_lock);
            if ((mi = mm->mm_active->mmi_input) != NULL)
              mpegts_input_open_pid(mi, mm, pid,
                                    MPS_SERVICE, MPS_WEIGHT_CAT, s);
            pthread_mutex_unlock(&mi->mi_output_lock);
          }
        }
        break;
      default:
        break;
    }

    ptr += dlen;
    len -= dlen;
  }

  /* Finish */
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

void
mpegts_input_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int flags, int init, int weight )
{
  mpegts_mux_t *mm = s->s_dvb_mux;
  elementary_stream_t *st;
  mpegts_apids_t *pids;
  mpegts_apid_t *p;
  mpegts_service_t *s2;
  int i;

  /* Add to list */
  pthread_mutex_lock(&mi->mi_output_lock);
  if (!s->s_dvb_active_input) {
    LIST_INSERT_HEAD(&mm->mm_transports, ((service_t*)s), s_active_link);
    s->s_dvb_active_input = mi;
  }

  /* Register PIDs */
  pthread_mutex_lock(&s->s_stream_mutex);
  if (s->s_type == STYPE_STD) {

    pids = mpegts_pid_alloc();

    mpegts_input_open_pid(mi, mm, s->s_pmt_pid, MPS_SERVICE, MPS_WEIGHT_PMT, s);
    mpegts_input_open_pid(mi, mm, s->s_pcr_pid, MPS_SERVICE, MPS_WEIGHT_PCR, s);
    mpegts_pid_add(pids, s->s_pmt_pid, MPS_WEIGHT_PMT);
    mpegts_pid_add(pids, s->s_pcr_pid, MPS_WEIGHT_PCR);
    if (s->s_scrambled_pass)
      mpegts_input_open_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, MPS_WEIGHT_CAT, s);
    /* Open only filtered components here */
    TAILQ_FOREACH(st, &s->s_filt_components, es_filt_link)
      if (s->s_scrambled_pass || st->es_type != SCT_CA) {
        st->es_pid_opened = 1;
        mpegts_input_open_pid(mi, mm, st->es_pid, MPS_SERVICE, mps_weight(st), s);
      }

    /* Ensure that filtered PIDs are not send in ts_recv_raw */
    TAILQ_FOREACH(st, &s->s_filt_components, es_filt_link)
      if ((s->s_scrambled_pass || st->es_type != SCT_CA) &&
          st->es_pid >= 0 && st->es_pid < 8192)
        mpegts_pid_add(pids, st->es_pid, mps_weight(st));

    LIST_FOREACH(s2, &s->s_masters, s_masters_link) {
      pthread_mutex_lock(&s2->s_stream_mutex);
      mpegts_pid_add_group(s2->s_slaves_pids, pids);
      pthread_mutex_unlock(&s2->s_stream_mutex);
    }

    mpegts_pid_destroy(&pids);

  } else {
    if ((pids = s->s_pids) != NULL) {
      if (pids->all) {
        mpegts_input_open_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW | MPS_ALL, MPS_WEIGHT_RAW, s);
      } else {
        for (i = 0; i < pids->count; i++) {
          p = &pids->pids[i];
          mpegts_input_open_pid(mi, mm, p->pid, MPS_RAW, p->weight, s);
        }
      }
    } else if (flags & SUBSCRIPTION_TABLES) {
      mpegts_input_open_pid(mi, mm, MPEGTS_TABLES_PID, MPS_RAW | MPS_TABLES, MPS_WEIGHT_PAT, s);
    } else if (flags & SUBSCRIPTION_MINIMAL) {
      mpegts_input_open_pid(mi, mm, DVB_PAT_PID, MPS_RAW, MPS_WEIGHT_PAT, s);
    }
  }

  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_output_lock);

  /* Add PMT monitor */
  if(s->s_type == STYPE_STD) {
    s->s_pmt_mon =
      mpegts_table_add(mm, DVB_PMT_BASE, DVB_PMT_MASK,
                       dvb_pmt_callback, s, "pmt",
                       MT_CRC, s->s_pmt_pid, MPS_WEIGHT_PMT);
    if (s->s_scrambled_pass && (flags & SUBSCRIPTION_EMM) != 0) {
      s->s_cat_mon =
        mpegts_table_add(mm, DVB_CAT_BASE, DVB_CAT_MASK,
                         mpegts_input_cat_pass_callback, s, "cat",
                         MT_QUICKREQ | MT_CRC, DVB_CAT_PID,
                         MPS_WEIGHT_CAT);
    }
  }

  mpegts_mux_update_pids(mm);
}

void
mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  mpegts_mux_t *mm = s->s_dvb_mux;
  elementary_stream_t *st;
  mpegts_apids_t *pids;
  mpegts_service_t *s2;

  /* Close PMT/CAT tables */
  if (s->s_type == STYPE_STD) {
    if (s->s_pmt_mon)
      mpegts_table_destroy(s->s_pmt_mon);
    if (s->s_cat_mon)
      mpegts_table_destroy(s->s_cat_mon);
  }
  s->s_pmt_mon = NULL;

  /* Remove from list */
  pthread_mutex_lock(&mi->mi_output_lock);
  if (s->s_dvb_active_input != NULL) {
    LIST_REMOVE(((service_t*)s), s_active_link);
    s->s_dvb_active_input = NULL;
  }
  
  /* Close PID */
  pthread_mutex_lock(&s->s_stream_mutex);
  if (s->s_type == STYPE_STD) {

    pids = mpegts_pid_alloc();

    mpegts_input_close_pid(mi, mm, s->s_pmt_pid, MPS_SERVICE, MPS_WEIGHT_PMT, s);
    mpegts_input_close_pid(mi, mm, s->s_pcr_pid, MPS_SERVICE, MPS_WEIGHT_PCR, s);
    mpegts_pid_del(pids, s->s_pmt_pid, MPS_WEIGHT_PMT);
    mpegts_pid_del(pids, s->s_pcr_pid, MPS_WEIGHT_PCR);
    if (s->s_scrambled_pass)
      mpegts_input_close_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, MPS_WEIGHT_CAT, s);
    /* Close all opened PIDs (the component filter may be changed at runtime) */
    TAILQ_FOREACH(st, &s->s_components, es_link) {
      if (st->es_pid_opened) {
        st->es_pid_opened = 0;
        mpegts_input_close_pid(mi, mm, st->es_pid, MPS_SERVICE, mps_weight(st), s);
      }
      if (st->es_pid >= 0 && st->es_pid < 8192)
        mpegts_pid_del(pids, st->es_pid, mps_weight(st));
    }

    LIST_FOREACH(s2, &s->s_masters, s_masters_link) {
      pthread_mutex_lock(&s2->s_stream_mutex);
      mpegts_pid_del_group(s2->s_slaves_pids, pids);
      pthread_mutex_unlock(&s2->s_stream_mutex);
    }

    mpegts_pid_destroy(&pids);

  } else {
    mpegts_input_close_pids(mi, mm, s, 1);
  }

  pthread_mutex_unlock(&s->s_stream_mutex);
  pthread_mutex_unlock(&mi->mi_output_lock);

  mpegts_mux_update_pids(mm);

  /* Stop mux? */
  s->s_dvb_mux->mm_stop(s->s_dvb_mux, 0, SM_CODE_OK);
}

static void
mpegts_input_create_mux_instance
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  extern const idclass_t mpegts_mux_instance_class;
  tvh_input_instance_t *tii;
  LIST_FOREACH(tii, &mi->mi_mux_instances, tii_input_link)
    if (((mpegts_mux_instance_t *)tii)->mmi_mux == mm) break;
  if (!tii)
    (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL, mi, mm);
}

static void
mpegts_input_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
#if ENABLE_TSDEBUG
  extern char *tvheadend_tsdebug;
  static const char *tmpdir = "/tmp/tvheadend.tsdebug/";
  char buf[128];
  char path[PATH_MAX];
  struct stat st;
  if (!tvheadend_tsdebug && !stat(tmpdir, &st) && (st.st_mode & S_IFDIR) != 0)
    tvheadend_tsdebug = (char *)tmpdir;
  if (tvheadend_tsdebug && !strcmp(tvheadend_tsdebug, tmpdir) && stat(tmpdir, &st))
    tvheadend_tsdebug = NULL;
  if (tvheadend_tsdebug) {
    mpegts_mux_nice_name(mmi->mmi_mux, buf, sizeof(buf));
    snprintf(path, sizeof(path), "%s/%s-%li-%p-mux.ts", tvheadend_tsdebug,
             buf, (long)dispatch_clock, mi);
    mmi->mmi_mux->mm_tsdebug_fd = tvh_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (mmi->mmi_mux->mm_tsdebug_fd < 0)
      tvherror("tsdebug", "unable to create file '%s' (%i)", path, errno);
    snprintf(path, sizeof(path), "%s/%s-%li-%p-input.ts", tvheadend_tsdebug,
             buf, (long)dispatch_clock, mi);
    mmi->mmi_mux->mm_tsdebug_fd2 = tvh_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (mmi->mmi_mux->mm_tsdebug_fd2 < 0)
      tvherror("tsdebug", "unable to create file '%s' (%i)", path, errno);
  } else {
    mmi->mmi_mux->mm_tsdebug_fd = -1;
    mmi->mmi_mux->mm_tsdebug_fd2 = -1;
  }
#endif

  /* Deliver first TS packets as fast as possible */
  mi->mi_last_dispatch = 0;

  /* Arm timer */
  if (LIST_FIRST(&mi->mi_mux_active) == NULL)
    gtimer_arm(&mi->mi_status_timer, mpegts_input_status_timer,
               mi, 1);

  /* Update */
  mmi->mmi_mux->mm_active = mmi;

  /* Accept packets */
  LIST_INSERT_HEAD(&mi->mi_mux_active, mmi, mmi_active_link);
  notify_reload("input_status");
  mpegts_input_dbus_notify(mi, 1);
}

static void
mpegts_input_stopping_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  assert(mmi->mmi_mux->mm_active);

  pthread_mutex_lock(&mi->mi_input_lock);
  mmi->mmi_mux->mm_active = NULL;
  pthread_mutex_unlock(&mi->mi_input_lock);
  pthread_mutex_lock(&mi->mi_output_lock);
  mmi->mmi_mux->mm_active = NULL;
  pthread_mutex_unlock(&mi->mi_output_lock);
}

static void
mpegts_input_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  char buf[256];
  service_t *s, *s_next;
  mpegts_mux_t *mm = mmi->mmi_mux;

  /* no longer active */
  LIST_REMOVE(mmi, mmi_active_link);

  /* Disarm timer */
  if (LIST_FIRST(&mi->mi_mux_active) == NULL)
    gtimer_disarm(&mi->mi_status_timer);

  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhtrace("mpegts", "%s - flush subscribers", buf);
  for (s = LIST_FIRST(&mm->mm_transports); s; s = s_next) {
    s_next = LIST_NEXT(s, s_active_link);
    service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
  }
  notify_reload("input_status");
  mpegts_input_dbus_notify(mi, 0);

#if ENABLE_TSDEBUG
  tsdebug_packet_t *tp;
  if (mm->mm_tsdebug_fd >= 0)
    close(mm->mm_tsdebug_fd);
  if (mm->mm_tsdebug_fd2 >= 0)
    close(mm->mm_tsdebug_fd2);
  mm->mm_tsdebug_fd = -1;
  mm->mm_tsdebug_fd2 = -1;
  mm->mm_tsdebug_pos = 0;
  while ((tp = TAILQ_FIRST(&mm->mm_tsdebug_packets)) != NULL) {
    TAILQ_REMOVE(&mm->mm_tsdebug_packets, tp, link);
    free(tp);
  }
#endif
}

static int
mpegts_input_has_subscription ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int ret = 0;
  const service_t *t;
  const th_subscription_t *ths;
  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(t, &mm->mm_transports, s_active_link) {
    if (t->s_type == STYPE_RAW) {
      LIST_FOREACH(ths, &t->s_subscriptions, ths_service_link)
        if (!strcmp(ths->ths_title, "keep")) break;
      if (ths) continue;
    }
    ret = 1;
    break;
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  return ret;
}

static void
mpegts_input_tuning_error ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  service_t *t, *t_next;
  pthread_mutex_lock(&mi->mi_output_lock);
  for (t = LIST_FIRST(&mm->mm_transports); t; t = t_next) {
    t_next = LIST_NEXT(t, s_active_link);
    pthread_mutex_lock(&t->s_stream_mutex);
    service_set_streaming_status_flags(t, TSS_TUNING);
    pthread_mutex_unlock(&t->s_stream_mutex);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static int inline
get_pcr ( const uint8_t *tsb, int64_t *rpcr )
{
  int_fast64_t pcr;

  if (tsb[1] & 0x80) /* error bit */
    return 0;

  if ((tsb[3] & 0x20) == 0 ||
      tsb[4] <= 5 ||
      (tsb[5] & 0x10) == 0)
    return 0;

  pcr  = (uint64_t)tsb[6] << 25;
  pcr |= (uint64_t)tsb[7] << 17;
  pcr |= (uint64_t)tsb[8] << 9;
  pcr |= (uint64_t)tsb[9] << 1;
  pcr |= ((uint64_t)tsb[10] >> 7) & 0x01;
  *rpcr = pcr;
  return 1;
}

static int inline
ts_sync_count ( const uint8_t *tsb, int len )
{
  const uint8_t *start = tsb;
  while (len >= 188) {
    if (len >= 1880 &&
        tsb[0*188] == 0x47 && tsb[1*188] == 0x47 &&
        tsb[2*188] == 0x47 && tsb[3*188] == 0x47 &&
        tsb[4*188] == 0x47 && tsb[5*188] == 0x47 &&
        tsb[6*188] == 0x47 && tsb[7*188] == 0x47 &&
        tsb[8*188] == 0x47 && tsb[9*188] == 0x47) {
      len -= 1880;
      tsb += 1880;
    } else if (*tsb == 0x47) {
      len -= 188;
      tsb += 188;
    } else {
      break;
    }
  }
  return tsb - start;
}

void
mpegts_input_recv_packets
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, sbuf_t *sb,
    int flags, mpegts_pcr_t *pcr )
{
  int len2 = 0, off = 0;
  mpegts_packet_t *mp;
  uint8_t *tsb = sb->sb_data;
  int     len  = sb->sb_ptr;
#define MIN_TS_PKT 100
#define MIN_TS_SYN (5*188)

  if (len < (MIN_TS_PKT * 188) && (flags & MPEGTS_DATA_CC_RESTART) == 0) {
    /* For slow streams, check also against the clock */
    if (dispatch_clock == mi->mi_last_dispatch)
      return;
  }
  mi->mi_last_dispatch = dispatch_clock;

  /* Check for sync */
  while ( (len >= MIN_TS_SYN) &&
          ((len2 = ts_sync_count(tsb, len)) < MIN_TS_SYN) ) {
    mmi->tii_stats.unc++;
    --len;
    ++tsb;
    ++off;
  }

  // Note: we check for sync here so that the buffer can always be
  //       processed in its entirety inside the processing thread
  //       without the potential need to buffer data (since that would
  //       require per mmi buffers, where this is generally not required)

  /* Extract PCR on demand */
  if (pcr) {
    uint8_t *tmp, *end;
    uint16_t pid;
    for (tmp = tsb, end = tsb + len2; tmp < end; tmp += 188) {
      pid = ((tmp[1] & 0x1f) << 8) | tmp[2];
      if (pcr->pcr_pid == MPEGTS_PID_NONE || pcr->pcr_pid == pid) {
        if (get_pcr(tmp, &pcr->pcr_first)) {
          pcr->pcr_pid = pid;
          break;
        }
      }
    }
    if (pcr->pcr_pid != MPEGTS_PID_NONE) {
      for (tmp = tsb + len2 - 188; tmp >= tsb; tmp -= 188) {
        pid = ((tmp[1] & 0x1f) << 8) | tmp[2];
        if (pcr->pcr_pid == pid) {
          if (get_pcr(tmp, &pcr->pcr_last)) {
            pcr->pcr_pid = pid;
            break;
          }
        }
      }
    }
  }

  /* Pass */
  if (len2 >= MIN_TS_SYN || (flags & MPEGTS_DATA_CC_RESTART)) {
    mp = malloc(sizeof(mpegts_packet_t) + len2);
    mp->mp_mux        = mmi->mmi_mux;
    mp->mp_len        = len2;
    mp->mp_cc_restart = (flags & MPEGTS_DATA_CC_RESTART) ? 1 : 0;
    memcpy(mp->mp_data, tsb, len2);

    len -= len2;
    off += len2;

    pthread_mutex_lock(&mi->mi_input_lock);
    if (mmi->mmi_mux->mm_active == mmi) {
      TAILQ_INSERT_TAIL(&mi->mi_input_queue, mp, mp_link);
      pthread_cond_signal(&mi->mi_input_cond);
    } else {
      free(mp);
    }
    pthread_mutex_unlock(&mi->mi_input_lock);
  }

  /* Adjust buffer */
  if (len && (flags & MPEGTS_DATA_CC_RESTART) == 0)
    sbuf_cut(sb, off); // cut off the bottom
  else
    sb->sb_ptr = 0;    // clear
}

static void
mpegts_input_table_dispatch
  ( mpegts_mux_t *mm, const char *logprefix, const uint8_t *tsb, int tsb_len )
{
  int i, len = 0, c = 0;
  const uint8_t *tsb2, *tsb2_end;
  uint16_t pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
  mpegts_table_t *mt, **vec;

  /* Collate - tables may be removed during callbacks */
  pthread_mutex_lock(&mm->mm_tables_lock);
  i = mm->mm_num_tables;
  vec = alloca(i * sizeof(mpegts_table_t *));
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    c++;
    if (mt->mt_destroyed || !mt->mt_subscribed || mt->mt_pid != pid)
      continue;
    mpegts_table_grab(mt);
    if (len < i)
      vec[len++] = mt;
  }
  pthread_mutex_unlock(&mm->mm_tables_lock);
  if (i != c) {
    tvherror("psi", "tables count inconsistency (num %d, list %d)", i, c);
    assert(0);
  }

  /* Process */
  for (i = 0; i < len; i++) {
    mt = vec[i];
    if (!mt->mt_destroyed && mt->mt_pid == pid)
      for (tsb2 = tsb, tsb2_end = tsb + tsb_len; tsb2 < tsb2_end; tsb2 += 188)
        mpegts_psi_section_reassemble((mpegts_psi_table_t *)mt, logprefix,
                                      tsb2, mt->mt_flags & MT_CRC,
                                      mpegts_table_dispatch, mt);
    mpegts_table_release(mt);
  }
}

static void
mpegts_input_table_waiting ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_table_t *mt;

  if (!mm || !mm->mm_active)
    return;
  pthread_mutex_lock(&mm->mm_tables_lock);
  while ((mt = TAILQ_FIRST(&mm->mm_defer_tables)) != NULL) {
    mpegts_table_consistency_check(mm);
    TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
    if (mt->mt_defer_cmd == MT_DEFER_OPEN_PID && !mt->mt_destroyed) {
      mt->mt_defer_cmd = 0;
      if (!mt->mt_subscribed) {
        mt->mt_subscribed = 1;
        pthread_mutex_unlock(&mm->mm_tables_lock);
        mpegts_input_open_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt);
      } else {
        pthread_mutex_unlock(&mm->mm_tables_lock);
      }
    } else if (mt->mt_defer_cmd == MT_DEFER_CLOSE_PID) {
      mt->mt_defer_cmd = 0;
      if (mt->mt_subscribed) {
        mt->mt_subscribed = 0;
        pthread_mutex_unlock(&mm->mm_tables_lock);
        mpegts_input_close_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt);
      } else {
        pthread_mutex_unlock(&mm->mm_tables_lock);
      }
    } else {
      pthread_mutex_unlock(&mm->mm_tables_lock);
    }
    mpegts_table_release(mt);
    pthread_mutex_lock(&mm->mm_tables_lock);
  }
  mpegts_table_consistency_check(mm);
  pthread_mutex_unlock(&mm->mm_tables_lock);
}

#if ENABLE_TSDEBUG
static void
tsdebug_check_tspkt( mpegts_mux_t *mm, uint8_t *pkt, int len )
{
  void tsdebugcw_new_keys(service_t *t, int type, uint8_t *odd, uint8_t *even);
  uint32_t pos, type, keylen, sid, crc;
  mpegts_service_t *t;

  for ( ; len > 0; pkt += 188, len -= 188) {
    if (memcmp(pkt + 4, "TVHeadendDescramblerKeys", 24))
      continue;
    pos = 4 + 24;
    type = pkt[pos + 0];
    keylen = pkt[pos + 1];
    sid = (pkt[pos + 2] << 8) | pkt[pos + 3];
    pos += 4 + 2 * keylen;
    if (pos > 184)
      return;
    crc = (pkt[pos + 0] << 24) | (pkt[pos + 1] << 16) |
          (pkt[pos + 2] << 8) | pkt[pos + 3];
    if (crc != tvh_crc32(pkt, pos, 0x859aa5ba))
      return;
    LIST_FOREACH(t, &mm->mm_services, s_dvb_mux_link)
      if (t->s_dvb_service_id == sid) break;
    if (!t)
      return;
    pos =  4 + 24 + 4;
    tvhdebug("descrambler", "Keys from MPEG-TS source (PID 0x1FFF)!");
    tsdebugcw_new_keys((service_t *)t, type, pkt + pos, pkt + pos + keylen);
  }
}
#endif

static int
mpegts_input_process
  ( mpegts_input_t *mi, mpegts_packet_t *mpkt )
{
  uint16_t pid;
  uint8_t cc, cc2;
  uint8_t *tsb = mpkt->mp_data, *tsb2, *tsb2_end;
  int len = mpkt->mp_len, llen;
  int type = 0, f;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;
  service_t *s;
  elementary_stream_t *st;
  int table_wakeup = 0;
  mpegts_mux_t *mm = mpkt->mp_mux;
  mpegts_mux_instance_t *mmi;
#if ENABLE_TSDEBUG
  off_t tsdebug_pos;
#endif
  char muxname[256];

  if (mm == NULL || (mmi = mm->mm_active) == NULL)
    return 0;

  mpegts_mux_nice_name(mm, muxname, sizeof(muxname));

  assert(mm == mmi->mmi_mux);

#if ENABLE_TSDEBUG
  tsdebug_pos = mm->mm_tsdebug_pos;
#endif

  /* Process */
  assert((len % 188) == 0);
  while (len > 0) {

    /*
     * mask
     *  0 - 0xFF - sync word 0x47
     *  1 - 0x80 - transport error
     *  1 - 0x1F - pid high
     *  2 - 0xFF - pid low
     *  3 - 0xC0 - scrambled
     *  3 - 0x10 - CC check
     */
    llen = mpegts_word_count(tsb, len, 0xFF9FFFD0);

    pid = (tsb[1] << 8) | tsb[2];

    /* Transport error */
    if (pid & 0x8000) {
      if ((pid & 0x1FFF) != 0x1FFF)
        ++mmi->tii_stats.te;
    }

    pid &= 0x1FFF;

    /* Ignore NUL packets */
    if (pid == 0x1FFF) {
#if ENABLE_TSDEBUG
      tsdebug_check_tspkt(mm, tsb, llen);
#endif
      goto done;
    }

    /* Find PID */
    if ((mp = mpegts_mux_find_pid(mm, pid, 0))) {

      /* Low level CC check */
      if (tsb[3] & 0x10) {
        for (tsb2 = tsb, tsb2_end = tsb + llen, cc2 = mp->mp_cc;
             tsb2 < tsb2_end; tsb2 += 188) {
          cc = tsb2[3] & 0x0f;
          if (cc2 != 0xff && cc2 != cc) {
            tvhtrace("mpegts", "%s: pid %04X cc err %2d != %2d", muxname, pid, cc, cc2);
            ++mmi->tii_stats.cc;
          }
          cc2 = (cc + 1) & 0xF;
        }
        mp->mp_cc = cc2;
      }

      type = mp->mp_type;
      
      /* Stream all PIDs */
      LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
        if ((mps->mps_type & MPS_ALL) || (type & (MPS_TABLE|MPS_FTABLE)))
          ts_recv_raw((mpegts_service_t *)mps->mps_owner, tsb, llen);

      /* Stream raw PIDs */
      if (type & MPS_RAW) {
        LIST_FOREACH(mps, &mp->mp_svc_subs, mps_svcraw_link)
          ts_recv_raw((mpegts_service_t *)mps->mps_owner, tsb, llen);
      }

      /* Stream service data */
      if (type & MPS_SERVICE) {
        LIST_FOREACH(mps, &mp->mp_svc_subs, mps_svcraw_link) {
          s = mps->mps_owner;
          f = (type & (MPS_TABLE|MPS_FTABLE)) ||
              (pid == s->s_pmt_pid) || (pid == s->s_pcr_pid);
          ts_recv_packet1((mpegts_service_t*)s, tsb, llen, f);
        }
      } else
      /* Stream table data */
      if (type & MPS_STREAM) {
        LIST_FOREACH(s, &mm->mm_transports, s_active_link) {
          if (s->s_type != STYPE_STD) continue;
          f = (type & (MPS_TABLE|MPS_FTABLE)) ||
              (pid == s->s_pmt_pid) || (pid == s->s_pcr_pid);
          ts_recv_packet1((mpegts_service_t*)s, tsb, llen, f);
        }
      }

      /* Table data */
      if (type & (MPS_TABLE | MPS_FTABLE)) {
        if (!(tsb[1] & 0x80)) {
          if (type & MPS_FTABLE)
            mpegts_input_table_dispatch(mm, muxname, tsb, llen);
          if (type & MPS_TABLE) {
            mpegts_table_feed_t *mtf = malloc(sizeof(mpegts_table_feed_t)+llen);
            mtf->mtf_len = llen;
            memcpy(mtf->mtf_tsb, tsb, llen);
            mtf->mtf_mux = mm;
            TAILQ_INSERT_TAIL(&mi->mi_table_queue, mtf, mtf_link);
            table_wakeup = 1;
          }
        } else {
          //tvhdebug("tsdemux", "%s - SI packet had errors", name);
        }
      }

    } else {

      /* Stream to all fullmux subscribers */
      LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
        ts_recv_raw((mpegts_service_t *)mps->mps_owner, tsb, llen);

    }

done:
    tsb += llen;
    len -= llen;
#if ENABLE_TSDEBUG
    mm->mm_tsdebug_pos += llen;
#endif
  }

  /* Raw stream */
  if (tsb != mpkt->mp_data &&
      LIST_FIRST(&mmi->mmi_streaming_pad.sp_targets) != NULL) {

    streaming_message_t sm;
    pktbuf_t *pb = pktbuf_alloc(mpkt->mp_data, tsb - mpkt->mp_data);
    memset(&sm, 0, sizeof(sm));
    sm.sm_type = SMT_MPEGTS;
    sm.sm_data = pb;
    streaming_pad_deliver(&mmi->mmi_streaming_pad, streaming_msg_clone(&sm));
    pktbuf_ref_dec(pb);
  }
#if ENABLE_TSDEBUG
  {
    tsdebug_packet_t *tp, *tp_next;
    off_t pos = 0;
    size_t used = tsb - mpkt->mp_data;
    for (tp = TAILQ_FIRST(&mm->mm_tsdebug_packets); tp; tp = tp_next) {
      tp_next = TAILQ_NEXT(tp, link);
      assert((tp->pos % 188) == 0);
      assert(tp->pos >= tsdebug_pos && tp->pos < tsdebug_pos + used);
      if (mm->mm_tsdebug_fd >= 0) {
        tvh_write(mm->mm_tsdebug_fd, mpkt->mp_data + pos, tp->pos - tsdebug_pos - pos);
        tvh_write(mm->mm_tsdebug_fd, tp->pkt, 188);
      }
      pos = tp->pos - tsdebug_pos;
      TAILQ_REMOVE(&mm->mm_tsdebug_packets, tp, link);
      free(tp);
    }
    if (pos < used && mm->mm_tsdebug_fd >= 0)
      tvh_write(mm->mm_tsdebug_fd, mpkt->mp_data + pos, used - pos);
  }
#endif

  if (mpkt->mp_cc_restart) {
    LIST_FOREACH(s, &mm->mm_transports, s_active_link)
      TAILQ_FOREACH(st, &s->s_components, es_link)
        st->es_cc = -1;
  }

  /* Wake table */
  if (table_wakeup)
    pthread_cond_signal(&mi->mi_table_cond);

  /* Bandwidth monitoring */
  llen = tsb - mpkt->mp_data;
  atomic_add(&mmi->tii_stats.bps, llen);
  return llen;
}

static void *
mpegts_input_thread ( void * p )
{
  mpegts_packet_t *mp;
  mpegts_input_t  *mi = p;
  size_t bytes = 0;
  char buf[256];

  mi->mi_display_name(mi, buf, sizeof(buf));
  pthread_mutex_lock(&mi->mi_input_lock);
  while (mi->mi_running) {

    /* Wait for a packet */
    if (!(mp = TAILQ_FIRST(&mi->mi_input_queue))) {
      if (bytes) {
        tvhtrace("mpegts", "input %s got %zu bytes", buf, bytes);
        bytes = 0;
      }
      pthread_cond_wait(&mi->mi_input_cond, &mi->mi_input_lock);
      continue;
    }
    TAILQ_REMOVE(&mi->mi_input_queue, mp, mp_link);
    pthread_mutex_unlock(&mi->mi_input_lock);
      
    /* Process */
    pthread_mutex_lock(&mi->mi_output_lock);
    mpegts_input_table_waiting(mi, mp->mp_mux);
    if (mp->mp_mux && mp->mp_mux->mm_update_pids_flag) {
      pthread_mutex_unlock(&mi->mi_output_lock);
      pthread_mutex_lock(&global_lock);
      mpegts_mux_update_pids(mp->mp_mux);
      pthread_mutex_unlock(&global_lock);
      pthread_mutex_lock(&mi->mi_output_lock);
    }
    bytes += mpegts_input_process(mi, mp);
    pthread_mutex_unlock(&mi->mi_output_lock);
    if (mp->mp_mux && mp->mp_mux->mm_update_pids_flag) {
      pthread_mutex_lock(&global_lock);
      mpegts_mux_update_pids(mp->mp_mux);
      pthread_mutex_unlock(&global_lock);
    }

    /* Cleanup */
    free(mp);

#if ENABLE_TSDEBUG
    {
      extern void tsdebugcw_go(void);
      tsdebugcw_go();
    }
#endif

    pthread_mutex_lock(&mi->mi_input_lock);
  }

  tvhtrace("mpegts", "input %s got %zu bytes (finish)", buf, bytes);

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
  mpegts_mux_t          *mm = NULL;
  char                   muxname[256];

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
    pthread_mutex_lock(&global_lock);
    if (mi->mi_running) {
      if (mm != mtf->mtf_mux) {
        mm = mtf->mtf_mux;
        if (mm)
          mpegts_mux_nice_name(mm, muxname, sizeof(muxname));
      }
      if (mm && mm->mm_active)
        mpegts_input_table_dispatch(mm, muxname, mtf->mtf_tsb, mtf->mtf_len);
    }
    pthread_mutex_unlock(&global_lock);

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

  lock_assert(&global_lock);

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
  /* mux active must be NULL here */
  /* otherwise the picked mtf might be processed after mux deactivation */
  assert(mm->mm_active == NULL);
}

static void
mpegts_input_stream_status
  ( mpegts_mux_instance_t *mmi, tvh_input_stream_t *st )
{
  int s = 0, w = 0;
  char buf[512], ubuf[UUID_HEX_SIZE];
  th_subscription_t *ths;
  const service_t *t;
  mpegts_mux_t *mm = mmi->mmi_mux;
  mpegts_input_t *mi = mmi->mmi_input;

  LIST_FOREACH(t, &mm->mm_transports, s_active_link)
    if (((mpegts_service_t *)t)->s_dvb_mux == mm)
      LIST_FOREACH(ths, &t->s_subscriptions, ths_service_link) {
        s++;
        w = MAX(w, ths->ths_weight);
      }

  st->uuid        = strdup(idnode_uuid_as_str(&mmi->tii_id, ubuf));
  mi->mi_display_name(mi, buf, sizeof(buf));
  st->input_name  = strdup(buf);
  mpegts_mux_nice_name(mm, buf, sizeof(buf));
  st->stream_name = strdup(buf);
  st->subs_count  = s;
  st->max_weight  = w;
  st->stats       = mmi->tii_stats;
  st->stats.bps   = atomic_exchange(&mmi->tii_stats.bps, 0) * 8;
}

void
mpegts_input_empty_status
  ( mpegts_input_t *mi, tvh_input_stream_t *st )
{
  char buf[512], ubuf[UUID_HEX_SIZE];
  tvh_input_instance_t *mmi_;
  mpegts_mux_instance_t *mmi;

  st->uuid        = strdup(idnode_uuid_as_str(&mi->ti_id, ubuf));
  mi->mi_display_name(mi, buf, sizeof(buf));
  st->input_name  = strdup(buf);
  LIST_FOREACH(mmi_, &mi->mi_mux_instances, tii_input_link) {
    mmi = (mpegts_mux_instance_t *)mmi_;
    st->stats.unc += mmi->tii_stats.unc;
    st->stats.cc += mmi->tii_stats.cc;
    st->stats.te += mmi->tii_stats.te;
    st->stats.ec_block += mmi->tii_stats.ec_block;
    st->stats.tc_block += mmi->tii_stats.tc_block;
  }
}

static void
mpegts_input_get_streams
  ( tvh_input_t *i, tvh_input_stream_list_t *isl )
{
  tvh_input_stream_t *st = NULL;
  mpegts_input_t *mi = (mpegts_input_t*)i;
  mpegts_mux_instance_t *mmi;

  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    st = calloc(1, sizeof(tvh_input_stream_t));
    mpegts_input_stream_status(mmi, st);
    LIST_INSERT_HEAD(isl, st, link);
  }
  if (st == NULL && mi->mi_empty_status && mi->mi_enabled) {
    st = calloc(1, sizeof(tvh_input_stream_t));
    mi->mi_empty_status(mi, st);
    LIST_INSERT_HEAD(isl, st, link);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
}

static void
mpegts_input_clear_stats ( tvh_input_t *i )
{
  mpegts_input_t *mi = (mpegts_input_t*)i;
  tvh_input_instance_t *mmi_;
  mpegts_mux_instance_t *mmi;

  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi_, &mi->mi_mux_instances, tii_input_link) {
    mmi = (mpegts_mux_instance_t *)mmi_;
    mmi->tii_stats.unc = 0;
    mmi->tii_stats.cc = 0;
    mmi->tii_stats.te = 0;
    mmi->tii_stats.ec_block = 0;
    mmi->tii_stats.tc_block = 0;
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  notify_reload("input_status");
}

static void
mpegts_input_thread_start ( mpegts_input_t *mi )
{
  mi->mi_running = 1;
  
  tvhthread_create(&mi->mi_table_tid, NULL,
                   mpegts_input_table_thread, mi, "mi-table");
  tvhthread_create(&mi->mi_input_tid, NULL,
                   mpegts_input_thread, mi, "mi-main");
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
  int64_t subs = 0;

  pthread_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    memset(&st, 0, sizeof(st));
    mpegts_input_stream_status(mmi, &st);
    e = tvh_input_stream_create_msg(&st);
    htsmsg_add_u32(e, "update", 1);
    notify_by_msg("input_status", e, 0);
    subs += st.subs_count;
    tvh_input_stream_destroy(&st);
  }
  pthread_mutex_unlock(&mi->mi_output_lock);
  gtimer_arm(&mi->mi_status_timer, mpegts_input_status_timer, mi, 1);
  mpegts_input_dbus_notify(mi, subs);
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
  if (idnode_insert(&mi->ti_id, uuid, class, 0)) {
    if (uuid)
      tvherror("mpegts", "invalid input uuid '%s'", uuid);
    free(mi);
    return NULL;
  }
  LIST_INSERT_HEAD(&tvh_inputs, (tvh_input_t*)mi, ti_link);
  
  /* Defaults */
  mi->mi_is_enabled           = mpegts_input_is_enabled;
  mi->mi_display_name         = mpegts_input_display_name;
  mi->mi_get_weight           = mpegts_input_get_weight;
  mi->mi_get_priority         = mpegts_input_get_priority;
  mi->mi_warm_mux             = mpegts_input_warm_mux;
  mi->mi_start_mux            = mpegts_input_start_mux;
  mi->mi_stop_mux             = mpegts_input_stop_mux;
  mi->mi_open_service         = mpegts_input_open_service;
  mi->mi_close_service        = mpegts_input_close_service;
  mi->mi_update_pids          = mpegts_input_update_pids;
  mi->mi_create_mux_instance  = mpegts_input_create_mux_instance;
  mi->mi_started_mux          = mpegts_input_started_mux;
  mi->mi_stopping_mux         = mpegts_input_stopping_mux;
  mi->mi_stopped_mux          = mpegts_input_stopped_mux;
  mi->mi_has_subscription     = mpegts_input_has_subscription;
  mi->mi_tuning_error         = mpegts_input_tuning_error;
  mi->ti_get_streams          = mpegts_input_get_streams;
  mi->ti_clear_stats          = mpegts_input_clear_stats;

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
  mi->mi_initscan = 1;
  mi->mi_idlescan = 1;

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
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1, SM_CODE_OK);
}

void
mpegts_input_delete ( mpegts_input_t *mi, int delconf )
{
  mpegts_network_link_t *mnl;
  tvh_input_instance_t *tii, *tii_next;

  /* Early shutdown flag */
  mi->mi_running = 0;

  /* Remove networks */
  while ((mnl = LIST_FIRST(&mi->mi_networks)))
    mpegts_input_del_network(mnl);

  /* Remove mux instances assigned to this input */
  tii = LIST_FIRST(&mi->mi_mux_instances);
  while (tii) {
    tii_next = LIST_NEXT(tii, tii_input_link);
    if (((mpegts_mux_instance_t *)tii)->mmi_input == mi)
      tii->tii_delete(tii);
    tii = tii_next;
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
  free(mi->mi_linked);
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
  idnode_notify_changed(&mnl->mnl_network->mn_id);
  return 1;
}

static void
mpegts_input_del_network ( mpegts_network_link_t *mnl )
{
  idnode_notify_changed(&mnl->mnl_network->mn_id);
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

int
mpegts_input_grace( mpegts_input_t *mi, mpegts_mux_t *mm )
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
