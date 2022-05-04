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
#include "streaming.h"
#include "access.h"
#include "notify.h"
#include "dbus.h"
#include "memoryinfo.h"

memoryinfo_t mpegts_input_queue_memoryinfo = { .my_name = "MPEG-TS input queue" };
memoryinfo_t mpegts_input_table_memoryinfo = { .my_name = "MPEG-TS table queue" };

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

static void
mpegts_input_class_get_title
  ( idnode_t *in, const char *lang, char *dst, size_t dstsize )
{
  mpegts_input_t *mi = (mpegts_input_t*)in;
  mi->mi_display_name(mi, dst, dstsize);
}

const void *
mpegts_input_class_active_get ( void *obj )
{
  static int active;
  mpegts_input_t *mi = obj;
  active = mi->mi_enabled ? 1 : 0;
  return &active;
}

const void *
mpegts_input_class_network_get ( void *obj )
{
  mpegts_network_link_t *mnl;  
  mpegts_input_t *mi = obj;
  htsmsg_t       *l  = htsmsg_create_list();

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link)
    htsmsg_add_uuid(l, NULL, &mnl->mnl_network->mn_id.in_uuid);

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

  if (!obj)
    return NULL;

  p = htsmsg_create_map();
  htsmsg_add_uuid(p, "uuid",    &((idnode_t*)obj)->in_uuid);
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
  mpegts_network_t *mn;
  mpegts_input_t *mi = obj;
  htsmsg_t        *l = htsmsg_create_list();
  char buf[384];

  LIST_FOREACH(mnl, &mi->mi_networks, mnl_mi_link) {
    mn = mnl->mnl_network;
    htsmsg_add_str(l, NULL, idnode_get_title(&mn->mn_id, lang, buf, sizeof(buf)));
  }

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

static htsmsg_t *
mpegts_input_class_linked_enum( void * self, const char *lang )
{
  mpegts_input_t *mi = self, *mi2;
  tvh_input_t *ti;
  char ubuf[UUID_HEX_SIZE], buf[384];
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e = htsmsg_create_key_val("", tvh_gettext_lang(lang, N_("Not linked")));
  htsmsg_add_msg(m, NULL, e);
  TVH_INPUT_FOREACH(ti)
    if (idnode_is_instance(&ti->ti_id, &mpegts_input_class)) {
      mi2 = (mpegts_input_t *)ti;
      if (mi2 != mi) {
        e = htsmsg_create_key_val(idnode_uuid_as_str(&ti->ti_id, ubuf),
                                  idnode_get_title(&mi2->ti_id, lang, buf, sizeof(buf)));
        htsmsg_add_msg(m, NULL, e);
      }
  }
  return m;
}

PROP_DOC(priority)
PROP_DOC(streaming_priority)

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
      .id       = "active",
      .name     = N_("Active"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
      .get      = mpegts_input_class_active_get,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable tuner/adapter."),
      .off      = offsetof(mpegts_input_t, mi_enabled),
      .notify   = mpegts_input_enabled_notify,
      .def.i    = 1,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .desc     = N_("The tuner priority value (a higher value means to "
                     "use this tuner out of preference). See Help for details."),
      .doc      = prop_doc_priority,
      .off      = offsetof(mpegts_input_t, mi_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming priority"),
      .desc     = N_("The tuner priority value for streamed channels "
                     "through HTTP or HTSP (a higher value means to use "
                     "this tuner out of preference). If not set (zero), "
                     "the standard priority value is used. See Help for details."),
      .doc      = prop_doc_streaming_priority,
      .off      = offsetof(mpegts_input_t, mi_streaming_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = N_("Name"),
      .desc     = N_("Name of the tuner/adapter."),
      .off      = offsetof(mpegts_input_t, mi_name),
      .notify   = idnode_notify_title_changed_lang,
    },
    {
      .type     = PT_BOOL,
      .id       = "ota_epg",
      .name     = N_("Over-the-air EPG"),
      .desc     = N_("Enable over-the-air program guide (EPG) scanning "
                     "on this input device."),
      .off      = offsetof(mpegts_input_t, mi_ota_epg),
      .def.i    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "initscan",
      .name     = N_("Initial scan"),
      .desc     = N_("Allow the initial scan tuning on this device "
                     "(scan when Tvheadend starts or when a new multiplex "
                     "is added automatically). At least one tuner or input "
                     "should have this settings turned on. "
                     "See also 'Skip Startup Scan' in the network settings "
                     "for further details."),
      .off      = offsetof(mpegts_input_t, mi_initscan),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "idlescan",
      .name     = N_("Idle scan"),
      .desc     = N_("Allow idle scan tuning on this device."),
      .off      = offsetof(mpegts_input_t, mi_idlescan),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_U32,
      .id       = "free_weight",
      .name     = N_("Free subscription weight"),
      .desc     = N_("If the subscription weight for the input is below "
                     "the specified threshold, the tuner is handled as free "
                     "(according the priority settings). Otherwise, the next "
                     "tuner (without any subscriptions) is used. Set this value "
                     "to 10, if you are willing to override scan and epggrab "
                     "subscriptions."),
      .off      = offsetof(mpegts_input_t, mi_free_weight),
      .def.i    = 1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "remove_scrambled",
      .name     = N_("Remove scrambled bits"),
      .desc     = N_("The scrambled bits in MPEG-TS packets are always cleared. "
                     "It is a workaround for the special streams which are "
                     "descrambled, but these bits are not touched."),
      .off      = offsetof(mpegts_input_t, mi_remove_scrambled_bits),
      .def.i    = 1,
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = N_("Networks"),
      .desc     = N_("Associate this device with one or more networks."),
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
      .desc     = N_("Wake up the linked input whenever this adapter "
                     "is used. The subscriptions are named as \"keep\". "
                     "Note that this isn't normally needed, and is here "
                     "simply as a workaround to driver bugs in certain "
                     "dual tuner cards that otherwise lock the second tuner."),
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
mpegts_input_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  if ((flags & SUBSCRIPTION_EPG) != 0 && !mi->mi_ota_epg)
    return MI_IS_ENABLED_NEVER;
  if ((flags & SUBSCRIPTION_USERSCAN) == 0) {
    if ((flags & SUBSCRIPTION_INITSCAN) != 0 && !mi->mi_initscan)
      return MI_IS_ENABLED_NEVER;
    if ((flags & SUBSCRIPTION_IDLESCAN) != 0 && !mi->mi_idlescan)
      return MI_IS_ENABLED_NEVER;
  }
  return mi->mi_enabled ? MI_IS_ENABLED_OK : MI_IS_ENABLED_NEVER;
}

void
mpegts_input_set_enabled ( mpegts_input_t *mi, int enabled )
{
  enabled = !!enabled;
  if (mi->mi_enabled != enabled) {
    htsmsg_t *conf = htsmsg_create_map();
    htsmsg_add_bool(conf, "enabled", enabled);
    idnode_update(&mi->ti_id, conf);
    htsmsg_destroy(conf);
  }
}

static void
mpegts_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  if (mi->mi_name) {
    strlcpy(buf, mi->mi_name, len);
  } else {
    *buf = 0;
  }
}

int
mpegts_input_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  const mpegts_mux_instance_t *mmi;
  const service_t *s;
  const th_subscription_t *ths;
  int w = 0, count = 0;

  /* Service subs */
  tvh_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link)
    LIST_FOREACH(s, &mmi->mmi_mux->mm_transports, s_active_link)
      LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link) {
        w = MAX(w, ths->ths_weight);
        count++;
      }
  tvh_mutex_unlock(&mi->mi_output_lock);
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

int
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
  const int mask = MPS_SERVICE;
  if ((a->mps_type & mask) != (b->mps_type & mask))
    return (a->mps_type & mask) ? 1 : -1;
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
      mpegts_input_close_pid(mi, mm, pid, mps->mps_type, mps->mps_owner);
    }
  for (mp = RB_FIRST(&mm->mm_pids); mp; mp = mp_next) {
    mp_next = RB_NEXT(mp, mp_link);
    for (mps = RB_FIRST(&mp->mp_subs); mps; mps = mps_next) {
      mps_next = RB_NEXT(mps, mps_link);
      if (mps->mps_owner != owner) continue;
      mpegts_input_close_pid(mi, mm, mp->mp_pid, mps->mps_type, mps->mps_owner);
    }
  }
}

mpegts_pid_t *
mpegts_input_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight,
    void *owner, int reopen )
{
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
        tvhdebug(LS_MPEGTS, "%s - open PID %s subscription [%04x/%p]",
                 mm->mm_nicename, (type & MPS_TABLES) ? "tables" : "fullmux", type, owner);
        mm->mm_update_pids_flag = 1;
      } else {
        if (!reopen) {
          tvherror(LS_MPEGTS,
                   "%s - open PID %04x (%d) failed, dupe sub (owner %p)",
                   mm->mm_nicename, mp->mp_pid, mp->mp_pid, owner);
        }
        free(mps);
        mp = NULL;
      }
    } else if (!RB_INSERT_SORTED(&mp->mp_subs, mps, mps_link, mpegts_mps_cmp)) {
      mp->mp_type |= type;
      if (type & MPS_RAW)
        LIST_INSERT_HEAD(&mp->mp_raw_subs, mps, mps_raw_link);
      if (type & MPS_SERVICE)
        LIST_INSERT_HEAD(&mp->mp_svc_subs, mps, mps_svcraw_link);
      tvhdebug(LS_MPEGTS, "%s - open PID %04X (%d) [%d/%p]",
               mm->mm_nicename, mp->mp_pid, mp->mp_pid, type, owner);
      mm->mm_update_pids_flag = 1;
    } else {
      if (!reopen) {
        tvherror(LS_MPEGTS, "%s - open PID %04x (%d) failed, dupe sub (owner %p)",
                 mm->mm_nicename, mp->mp_pid, mp->mp_pid, owner);
      }
      free(mps);
      mp = NULL;
    }
  }
  return mp;
}

int
mpegts_input_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  mpegts_pid_sub_t *mps, skel;
  mpegts_pid_t *mp;
  int mask;
  assert(owner != NULL);
  lock_assert(&mi->mi_output_lock);
  if (!(mp = mpegts_mux_find_pid(mm, pid, 0)))
    return -1;
  if (pid == MPEGTS_FULLMUX_PID || pid == MPEGTS_TABLES_PID) {
    LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
      if (mps->mps_owner == owner) break;
    if (mps == NULL) return -1;
    tvhdebug(LS_MPEGTS, "%s - close PID %s subscription [%04x/%p]",
             mm->mm_nicename, pid == MPEGTS_TABLES_PID ? "tables" : "fullmux",
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
    skel.mps_weight = -1;
    skel.mps_owner  = owner;
    mps = RB_FIND(&mp->mp_subs, &skel, mps_link, mpegts_mps_cmp);
    if (pid == mm->mm_last_pid) {
      mm->mm_last_pid = -1;
      mm->mm_last_mp = NULL;
    }
    if (mps) {
      tvhdebug(LS_MPEGTS, "%s - close PID %04X (%d) [%d/%p]",
               mm->mm_nicename, mp->mp_pid, mp->mp_pid, type, owner);
      if (type & MPS_RAW)
        LIST_REMOVE(mps, mps_raw_link);
      if (type & MPS_SERVICE)
        LIST_REMOVE(mps, mps_svcraw_link);
      RB_REMOVE(&mp->mp_subs, mps, mps_link);
      free(mps);
      mm->mm_update_pids_flag = 1;
    }
  }
  if (!RB_FIRST(&mp->mp_subs)) {
    if (mm->mm_last_pid == mp->mp_pid) {
      mm->mm_last_pid = -1;
      mm->mm_last_mp = NULL;
    }
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

mpegts_pid_t *
mpegts_input_update_pid_weight
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, int weight,
    void *owner )
{
  mpegts_pid_sub_t *mps, skel;
  mpegts_pid_t *mp;
  assert(owner != NULL);
  lock_assert(&mi->mi_output_lock);
  if (!(mp = mpegts_mux_find_pid(mm, pid, 0)))
    return NULL;
  if (pid == MPEGTS_FULLMUX_PID || pid == MPEGTS_TABLES_PID) {
    LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
      if (mps->mps_owner == owner) break;
    if (mps == NULL) return NULL;
    tvhdebug(LS_MPEGTS, "%s - update PID %s weight %d subscription [%04x/%p]",
             mm->mm_nicename, pid == MPEGTS_TABLES_PID ? "tables" : "fullmux",
             weight, type, owner);
    mps->mps_weight = weight;
  } else {
    skel.mps_type   = type;
    skel.mps_weight = -1;
    skel.mps_owner  = owner;
    mps = RB_FIND(&mp->mp_subs, &skel, mps_link, mpegts_mps_cmp);
    if (mps == NULL) return NULL;
    tvhdebug(LS_MPEGTS, "%s - update PID %04X (%d) weight %d [%d/%p]",
             mm->mm_nicename, mp->mp_pid, mp->mp_pid, weight, type, owner);
    mps->mps_weight = weight;
    mm->mm_update_pids_flag = 1;
  }
  return mp;
}

elementary_stream_t *
mpegts_input_open_service_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm,
    service_t *s, streaming_component_type_t stype,
    int pid, int weight, int create )
{
  elementary_stream_t *es;

  lock_assert(&s->s_stream_mutex);

  es = NULL;
  if (elementary_stream_find(&s->s_components, pid) == NULL) {
    if (!create)
      return NULL;
    es = elementary_stream_create(&s->s_components, pid, stype);
    es->es_pid_opened = 1;
  }
  if (es && mm->mm_active) {
    mpegts_input_open_pid(mi, mm, pid,
                          MPS_SERVICE, weight, s, 0);
  }
  return es;
}

static void
mpegts_input_update_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  /* nothing - override */
}

int mpegts_mps_weight(elementary_stream_t *st)
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

typedef struct __cat_pass_aux {
  mpegts_input_t *mi;
  mpegts_mux_t *mm;
  service_t *service;
} __cat_pass_aux_t;

static void
mpegts_input_cat_pass_entry
  (void *_aux, uint16_t caid, uint32_t prov, uint16_t pid)
{
  __cat_pass_aux_t *aux = _aux;
  elementary_stream_t *es;
  caid_t *c;

  tvhdebug(LS_TBL_BASE, "cat:  pass: caid %04X (%d) pid %04X (%d)",
           (uint16_t)caid, (uint16_t)caid, pid, pid);
  es = mpegts_input_open_service_pid(aux->mi, aux->mm, aux->service,
                                     SCT_CAT, pid, MPS_WEIGHT_CAT, 1);
  if (es) {
    LIST_FOREACH(c, &es->es_caids, link) {
      if (c->pid == pid) {
        c->caid = caid;
        c->delete_me = 0;
        es->es_delete_me = 0;
        break;
      }
    }
    if (c == NULL) {
      c = malloc(sizeof(caid_t));
      c->caid = caid;
      c->providerid = 0;
      c->use = 1;
      c->pid = pid;
      c->delete_me = 0;
      c->filter = 0;
      LIST_INSERT_HEAD(&es->es_caids, c, link);
      es->es_delete_me = 0;
    }
  }
}

static int
mpegts_input_cat_pass_callback
  (mpegts_table_t *mt, const uint8_t *ptr, int len, int tableid)
{
  int r, sect, last, ver;
  mpegts_mux_t             *mm  = mt->mt_mux;
  mpegts_psi_table_state_t *st  = NULL;
  service_t                *s   = mt->mt_opaque;
  mpegts_input_t           *mi;
  elementary_stream_t      *es, *next;
  caid_t                   *c, *cn;
  __cat_pass_aux_t aux;

  /* Start */
  r = dvb_table_begin((mpegts_psi_table_t *)mt, ptr, len,
                      tableid, 0, 5, &st, &sect, &last, &ver, 0);
  if (r != 1) return r;
  ptr += 5;
  len -= 5;

  /* Send CAT data for descramblers */
  descrambler_cat_data(mm, ptr, len);

  mi = mm->mm_active ? mm->mm_active->mmi_input : NULL;
  if (mi == NULL) goto fin;

  tvh_mutex_lock(&mi->mi_output_lock);
  tvh_mutex_lock(&s->s_stream_mutex);

  TAILQ_FOREACH(es, &s->s_components.set_all, es_link) {
    if (es->es_type != SCT_CAT) continue;
    es->es_delete_me = 1;
    LIST_FOREACH(c, &es->es_caids, link)
      c->delete_me = 1;
  }

  aux.mi = mi;
  aux.mm = mm;
  aux.service = s;
  dvb_cat_decode(ptr, len, mpegts_input_cat_pass_entry, &aux);

  for (es = TAILQ_FIRST(&s->s_components.set_all); es != NULL; es = next) {
    next = TAILQ_NEXT(es, es_link);
    if (es->es_type != SCT_CAT) continue;
    for (c = LIST_FIRST(&es->es_caids); c != NULL; c = cn) {
      cn = LIST_NEXT(c, link);
      if (c->delete_me) {
        LIST_REMOVE(c, link);
        free(c);
      }
    }
    if (es->es_delete_me)
      elementary_set_stream_destroy(&s->s_components, es);
  }

  tvh_mutex_unlock(&s->s_stream_mutex);
  tvh_mutex_unlock(&mi->mi_output_lock);

  /* Finish */
fin:
  return dvb_table_end((mpegts_psi_table_t *)mt, st, sect);
}

void
mpegts_input_open_pmt_monitor
  ( mpegts_mux_t *mm, mpegts_service_t *s )
{
    if (s->s_pmt_mon)
      mpegts_table_destroy(s->s_pmt_mon);
    s->s_pmt_mon =
      mpegts_table_add(mm, DVB_PMT_BASE, DVB_PMT_MASK,
                       dvb_pmt_callback, s, "pmt", LS_TBL_BASE,
                       MT_CRC, s->s_components.set_pmt_pid, MPS_WEIGHT_PMT);
}

void
mpegts_input_open_cat_monitor
  ( mpegts_mux_t *mm, mpegts_service_t *s )
{
  assert(s->s_cat_mon == NULL);
  s->s_cat_mon =
    mpegts_table_add(mm, DVB_CAT_BASE, DVB_CAT_MASK,
                     mpegts_input_cat_pass_callback, s, "cat",
                     LS_TBL_BASE, MT_QUICKREQ | MT_CRC, DVB_CAT_PID,
                     MPS_WEIGHT_CAT);
}

void
mpegts_input_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int flags, int init, int weight )
{
  mpegts_mux_t *mm = s->s_dvb_mux;
  elementary_stream_t *st;
  mpegts_apids_t *pids;
  mpegts_apid_t *p;
  int i, reopen = !init;

  /* Add to list */
  tvh_mutex_lock(&mi->mi_output_lock);
  if (!s->s_dvb_active_input) {
    LIST_INSERT_HEAD(&mm->mm_transports, ((service_t*)s), s_active_link);
    s->s_dvb_active_input = mi;
  }
  /* Register PIDs */
  tvh_mutex_lock(&s->s_stream_mutex);
  if (s->s_type == STYPE_STD) {

    if (s->s_components.set_pmt_pid == SERVICE_PMT_AUTO)
      goto no_pids;

    mpegts_input_open_pid(mi, mm, s->s_components.set_pmt_pid,
                          MPS_SERVICE, MPS_WEIGHT_PMT, s, reopen);
    mpegts_input_open_pid(mi, mm, s->s_components.set_pcr_pid,
                          MPS_SERVICE, MPS_WEIGHT_PCR, s, reopen);
    if (s->s_scrambled_pass) {
      mpegts_input_open_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, MPS_WEIGHT_CAT, s, reopen);
      s->s_cat_opened = 1;
    }
    /* Open only filtered components here */
    TAILQ_FOREACH(st, &s->s_components.set_filter, es_filter_link)
      if ((s->s_scrambled_pass || st->es_type != SCT_CA) &&
          st->es_pid != s->s_components.set_pmt_pid &&
          st->es_pid != s->s_components.set_pcr_pid &&
          st->es_pid < 8192) {
        st->es_pid_opened = 1;
        mpegts_input_open_pid(mi, mm, st->es_pid, MPS_SERVICE, mpegts_mps_weight(st), s, reopen);
      }

    mpegts_service_update_slave_pids(s, NULL, 0);

  } else {
    if ((pids = s->s_pids) != NULL) {
      if (pids->all) {
        mpegts_input_open_pid(mi, mm, MPEGTS_FULLMUX_PID, MPS_RAW | MPS_ALL, MPS_WEIGHT_RAW, s, reopen);
      } else {
        for (i = 0; i < pids->count; i++) {
          p = &pids->pids[i];
          mpegts_input_open_pid(mi, mm, p->pid, MPS_RAW, p->weight, s, reopen);
        }
      }
    } else if (flags & SUBSCRIPTION_TABLES) {
      mpegts_input_open_pid(mi, mm, MPEGTS_TABLES_PID, MPS_RAW | MPS_TABLES, MPS_WEIGHT_PAT, s, reopen);
    } else if (flags & SUBSCRIPTION_MINIMAL) {
      mpegts_input_open_pid(mi, mm, DVB_PAT_PID, MPS_RAW, MPS_WEIGHT_PAT, s, reopen);
    }
  }

no_pids:
  tvh_mutex_unlock(&s->s_stream_mutex);
  tvh_mutex_unlock(&mi->mi_output_lock);

  /* Add PMT monitor */
  if (s->s_type == STYPE_STD) {
    mpegts_input_open_pmt_monitor(mm, s);
    if (s->s_scrambled_pass && (flags & SUBSCRIPTION_EMM) != 0)
      mpegts_input_open_cat_monitor(mm, s);
  }

  mpegts_mux_update_pids(mm);
}

void
mpegts_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  mpegts_mux_t *mm = s->s_dvb_mux;
  elementary_stream_t *st;

  /* Close PMT/CAT tables */
  if (s->s_type == STYPE_STD) {
    if (s->s_pmt_mon)
      mpegts_table_destroy(s->s_pmt_mon);
    if (s->s_cat_mon)
      mpegts_table_destroy(s->s_cat_mon);
  }
  s->s_pmt_mon = NULL;
  s->s_cat_mon = NULL;

  /* Remove from list */
  tvh_mutex_lock(&mi->mi_output_lock);
  if (s->s_dvb_active_input != NULL) {
    LIST_REMOVE(((service_t*)s), s_active_link);
    s->s_dvb_active_input = NULL;
  }
  /* Close PID */
  tvh_mutex_lock(&s->s_stream_mutex);
  if (s->s_type == STYPE_STD) {

    if (s->s_components.set_pmt_pid == SERVICE_PMT_AUTO)
      goto no_pids;

    mpegts_input_close_pid(mi, mm, s->s_components.set_pmt_pid, MPS_SERVICE, s);
    mpegts_input_close_pid(mi, mm, s->s_components.set_pcr_pid, MPS_SERVICE, s);
    if (s->s_cat_opened) {
      mpegts_input_close_pid(mi, mm, DVB_CAT_PID, MPS_SERVICE, s);
      s->s_cat_opened = 0;
    }
    /* Close all opened PIDs (the component filter may be changed at runtime) */
    TAILQ_FOREACH(st, &s->s_components.set_all, es_link) {
      if (st->es_pid_opened) {
        st->es_pid_opened = 0;
        mpegts_input_close_pid(mi, mm, st->es_pid, MPS_SERVICE, s);
      }
    }

    mpegts_service_update_slave_pids(s, NULL, 1);

  } else {
    mpegts_input_close_pids(mi, mm, s, 1);
  }

no_pids:
  tvh_mutex_unlock(&s->s_stream_mutex);
  tvh_mutex_unlock(&mi->mi_output_lock);

  mpegts_mux_update_pids(mm);

  /* Stop mux? */
  s->s_dvb_mux->mm_stop(s->s_dvb_mux, 0, SM_CODE_OK);
}

void
mpegts_input_create_mux_instance
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  extern const idclass_t mpegts_mux_instance_class;
  tvh_input_instance_t *tii;
  LIST_FOREACH(tii, &mi->mi_mux_instances, tii_input_link)
    if (((mpegts_mux_instance_t *)tii)->mmi_mux == mm) break;
  if (!tii)
    mpegts_mux_instance_create(mpegts_mux_instance, NULL, mi, mm);
}

static void
mpegts_input_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  mpegts_mux_t *mm = mmi->mmi_mux;

  /* Deliver first TS packets as fast as possible */
  atomic_set_s64(&mi->mi_last_dispatch, 0);

  /* Arm timer */
  if (LIST_FIRST(&mi->mi_mux_active) == NULL)
    mtimer_arm_rel(&mi->mi_status_timer, mpegts_input_status_timer, mi, sec2mono(1));

  /* Update */
  mm->mm_active = mmi;

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

  tvh_mutex_lock(&mi->mi_output_lock);
  tvh_mutex_lock(&mi->mi_input_lock);
  mmi->mmi_mux->mm_active = NULL;
  tvh_mutex_unlock(&mi->mi_input_lock);
  tvh_mutex_unlock(&mi->mi_output_lock);
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
    mtimer_disarm(&mi->mi_status_timer);

  mi->mi_display_name(mi, buf, sizeof(buf));
  tvhtrace(LS_MPEGTS, "%s - flush subscribers", buf);
  for (s = LIST_FIRST(&mm->mm_transports); s; s = s_next) {
    s_next = LIST_NEXT(s, s_active_link);
    service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);
  }
  notify_reload("input_status");
  mpegts_input_dbus_notify(mi, 0);
}

static int
mpegts_input_has_subscription ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  int ret = 0;
  const service_t *t;
  const th_subscription_t *ths;
  tvh_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(t, &mm->mm_transports, s_active_link) {
    if (t->s_type == STYPE_RAW) {
      LIST_FOREACH(ths, &t->s_subscriptions, ths_service_link)
        if (!strcmp(ths->ths_title, "keep")) break;
      if (ths) continue;
    }
    ret = 1;
    break;
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
  return ret;
}

static void
mpegts_input_error ( mpegts_input_t *mi, mpegts_mux_t *mm, int tss_flags )
{
  service_t *t, *t_next;
  tvh_mutex_lock(&mi->mi_output_lock);
  for (t = LIST_FIRST(&mm->mm_transports); t; t = t_next) {
    t_next = LIST_NEXT(t, s_active_link);
    tvh_mutex_lock(&t->s_stream_mutex);
    service_set_streaming_status_flags(t, tss_flags);
    tvh_mutex_unlock(&t->s_stream_mutex);
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static void mpegts_input_analyze_table_queue ( mpegts_input_t *mi )
{
  mpegts_table_feed_t *mtf;
  uint32_t sizes[8192];
  uint16_t counters[8192];
  uint16_t pid;

  memset(&sizes, 0, sizeof(sizes));
  memset(&counters, 0, sizeof(counters));
  TAILQ_FOREACH(mtf, &mi->mi_table_queue, mtf_link) {
    const uint8_t *tsb = mtf->mtf_tsb;
    pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
    sizes[pid] += mtf->mtf_len;
    counters[pid]++;
  }
  for (pid = 0; pid < 8192; pid++)
    if (counters[pid])
      tvhtrace(LS_MPEGTS, "table queue pid=%04X (%d) cnt %u len %u", pid, pid, counters[pid], sizes[pid]);
}

#if 0
static int data_noise ( mpegts_packet_t *mp )
{
  static uint64_t off = 0, win = 4096, limit = 2*1024*1024;
  uint8_t *data = mp->mp_data;
  uint32_t i, p, s, len = mp->mp_len;
  for (p = 0; p < len; p += 188) {
    off += 188;
    if (off >= limit && off < limit + win) {
      if ((off & 3) == 1) {
        memmove(data + p, data + p + 188, len - (p + 188));
        p -= 188;
        mp->mp_len -= 188;
        return 1;
      }
      s = ((data[2] + data[3] + data[4]) & 3) + 1;
      for (i = 0; i < 188; i += s)
        ((char *)data)[p+i] ^= data[10] + data[12] + data[i];
    } else if (off >= limit + win) {
      off = 0;
      limit = (uint64_t)data[15] * 4 * 1024;
      win   = (uint64_t)data[16] * 16;
    }
  }
  return 0;
}
#else
static inline int data_noise( mpegts_packet_t *mp ) { return 0; }
#endif

static inline int
get_pcr ( const uint8_t *tsb, int64_t *rpcr )
{
  int_fast64_t pcr;

  if (tsb[1] & 0x80) /* error bit */
    return 0;

  if ((tsb[3] & 0x20) == 0 ||
       tsb[4] <= 5 ||
      (tsb[5] & 0x10) == 0)
    return 0;

  pcr  =  (uint64_t)tsb[6] << 25;
  pcr |=  (uint64_t)tsb[7] << 17;
  pcr |=  (uint64_t)tsb[8] << 9;
  pcr |=  (uint64_t)tsb[9] << 1;
  pcr |= ((uint64_t)tsb[10] >> 7) & 0x01;
  *rpcr = pcr;
  return 1;
}

static inline int
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

static void
mpegts_input_queue_packets
  ( mpegts_mux_instance_t *mmi, mpegts_packet_t *mp )
{
  mpegts_input_t *mi = mmi->mmi_input;
  const char *id = SRCLINEID();
  int len = mp->mp_len;

  tvh_mutex_lock(&mi->mi_input_lock);
  if (mmi->mmi_mux->mm_active == mmi) {
    if (mi->mi_input_queue_size < 50*1024*1024) {
      mi->mi_input_queue_size += len;
      memoryinfo_alloc(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + len);
      mpegts_mux_grab(mp->mp_mux);
      TAILQ_INSERT_TAIL(&mi->mi_input_queue, mp, mp_link);
      tprofile_queue_add(&mi->mi_qprofile, id, len);
      tprofile_queue_set(&mi->mi_qprofile, id, mi->mi_input_queue_size);
      tvh_cond_signal(&mi->mi_input_cond, 0);
    } else {
      if (tvhlog_limit(&mi->mi_input_queue_loglimit, 10))
        tvhwarn(LS_MPEGTS, "too much queued input data (over 50MB) for %s, discarding new", mi->mi_name);
      tprofile_queue_drop(&mi->mi_qprofile, id, len);
      free(mp);
    }
  } else {
    free(mp);
  }
  tvh_mutex_unlock(&mi->mi_input_lock);
}

void
mpegts_input_recv_packets
  ( mpegts_mux_instance_t *mmi, sbuf_t *sb,
    int flags, mpegts_pcr_t *pcr )
{
  mpegts_input_t *mi = mmi->mmi_input;
  int len, len2, off;
  mpegts_packet_t *mp;
  uint8_t *tsb;
#define MIN_TS_PKT 100
#define MIN_TS_SYN (5*188)

  if (sb->sb_ptr == 0)
    return;
retry:
  len2 = 0;
  off  = 0;
  tsb  = sb->sb_data;
  len  = sb->sb_ptr;
  if (len < (MIN_TS_PKT * 188) && (flags & MPEGTS_DATA_CC_RESTART) == 0) {
    /* For slow streams, check also against the clock */
    if (monocmpfastsec(mclk(), atomic_add_s64(&mi->mi_last_dispatch, 0)))
      return;
  }
  atomic_set_s64(&mi->mi_last_dispatch, mclk());

  /* Check for sync */
  while ( (len >= MIN_TS_SYN) &&
          ((len2 = ts_sync_count(tsb, len)) < MIN_TS_SYN) ) {
    atomic_add(&mmi->tii_stats.unc, 1);
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
    if (mi->mi_remove_scrambled_bits || (flags & MPEGTS_DATA_REMOVE_SCRAMBLED) != 0) {
      uint8_t *tmp, *end;
      for (tmp = mp->mp_data, end = mp->mp_data + len2; tmp < end; tmp += 188)
        tmp[3] &= ~0xc0;
    }

    len -= len2;
    off += len2;

    if ((flags & MPEGTS_DATA_CC_RESTART) == 0 && data_noise(mp)) {
      free(mp);
      goto end;
    }

    mpegts_input_queue_packets(mmi, mp);
  }

  /* Adjust buffer */
end:
  if (len && (flags & MPEGTS_DATA_CC_RESTART) == 0) {
    sbuf_cut(sb, off); // cut off the bottom
    if (sb->sb_ptr >= MIN_TS_PKT * 188)
      goto retry;
  } else
    sb->sb_ptr = 0;    // clear
}

static void
mpegts_input_table_dispatch
  ( mpegts_mux_t *mm, const char *logprefix, const uint8_t *tsb, int tsb_len, int fast )
{
  int i, len = 0, c = 0;
  const uint8_t *tsb2, *tsb2_end;
  uint16_t pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
  mpegts_table_t *mt, **vec;

  /* Collate - tables may be removed during callbacks */
  tvh_mutex_lock(&mm->mm_tables_lock);
  i = mm->mm_num_tables;
  vec = alloca(i * sizeof(mpegts_table_t *));
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    c++;
    if (mt->mt_destroyed || !mt->mt_subscribed || mt->mt_pid != pid)
      continue;
    if (fast && (mt->mt_flags & MT_FAST) == 0)
      continue;
    if (!fast && (mt->mt_flags & MT_FAST) != 0)
      continue;
    mpegts_table_grab(mt);
    tprofile_start(&mt->mt_profile, "dispatch");
    if (len < i)
      vec[len++] = mt;
  }
  tvh_mutex_unlock(&mm->mm_tables_lock);
  if (i != c) {
    tvherror(LS_TBL, "tables count inconsistency (num %d, list %d)", i, c);
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
    tprofile_finish(&mt->mt_profile);
    mpegts_table_release(mt);
  }
}

static void
mpegts_input_table_restart
  ( mpegts_mux_t *mm, const char *logprefix, int fast )
{
  mpegts_table_t *mt;
  tvh_mutex_lock(&mm->mm_tables_lock);
  LIST_FOREACH(mt, &mm->mm_tables, mt_link) {
    if (fast && (mt->mt_flags & MT_FAST) == 0)
      continue;
    if (!fast && (mt->mt_flags & MT_FAST) != 0)
      continue;
    mt->mt_sect.ps_cc = -1;
  }
  tvh_mutex_unlock(&mm->mm_tables_lock);
}

static mpegts_table_feed_t *
mpegts_input_table_feed_create ( mpegts_input_t *mi, mpegts_mux_t *mm, uint8_t *tsb, int llen )
{
  mpegts_table_feed_t *mtf;

  mtf = malloc(sizeof(mpegts_table_feed_t) + MAX(llen, MPEGTS_MTF_ALLOC_CHUNK));
  mtf->mtf_cc_restart = 0;
  mtf->mtf_len = llen;
  memcpy(mtf->mtf_tsb, tsb, llen);
  mtf->mtf_mux = mm;
  mi->mi_table_queue_size += llen;
  memoryinfo_alloc(&mpegts_input_table_memoryinfo, sizeof(mpegts_table_feed_t) + llen);
  TAILQ_INSERT_TAIL(&mi->mi_table_queue, mtf, mtf_link);
  return mtf;
}

static void
mpegts_input_table_waiting ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  mpegts_table_t *mt;

  if (!mm || !mm->mm_active)
    return;
  tvh_mutex_lock(&mm->mm_tables_lock);
  while ((mt = TAILQ_FIRST(&mm->mm_defer_tables)) != NULL) {
    mpegts_table_consistency_check(mm);
    TAILQ_REMOVE(&mm->mm_defer_tables, mt, mt_defer_link);
    if (mt->mt_defer_cmd == MT_DEFER_OPEN_PID && !mt->mt_destroyed) {
      mt->mt_defer_cmd = 0;
      if (!mt->mt_subscribed) {
        mt->mt_subscribed = 1;
        tvh_mutex_unlock(&mm->mm_tables_lock);
        mpegts_input_open_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt->mt_weight, mt, 0);
      } else {
        tvh_mutex_unlock(&mm->mm_tables_lock);
      }
    } else if (mt->mt_defer_cmd == MT_DEFER_CLOSE_PID) {
      mt->mt_defer_cmd = 0;
      if (mt->mt_subscribed) {
        mt->mt_subscribed = 0;
        tvh_mutex_unlock(&mm->mm_tables_lock);
        mpegts_input_close_pid(mi, mm, mt->mt_pid, mpegts_table_type(mt), mt);
      } else {
        tvh_mutex_unlock(&mm->mm_tables_lock);
      }
    } else {
      tvh_mutex_unlock(&mm->mm_tables_lock);
    }
    mpegts_table_release(mt);
    tvh_mutex_lock(&mm->mm_tables_lock);
  }
  mpegts_table_consistency_check(mm);
  tvh_mutex_unlock(&mm->mm_tables_lock);
}

static int
mpegts_input_process
  ( mpegts_input_t *mi, mpegts_packet_t *mpkt )
{
  uint16_t pid, pid2;
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
  mpegts_table_feed_t *mtf;
  uint64_t tspos;

  if (mm == NULL || (mmi = mm->mm_active) == NULL)
    return 0;

  assert(mm == mmi->mmi_mux);

  if (mpkt->mp_cc_restart) {
    LIST_FOREACH(s, &mm->mm_transports, s_active_link)
      TAILQ_FOREACH(st, &s->s_components.set_all, es_link)
        st->es_cc = -1;
    RB_FOREACH(mp, &mm->mm_pids, mp_link) {
      mp->mp_cc = 0xff;
      if (mp->mp_type & MPS_FTABLE) {
        mpegts_input_table_restart(mm, mm->mm_nicename, 1);
      } else {
        mtf = mpegts_input_table_feed_create(mi, mm, NULL, 0);
        mtf->mtf_cc_restart = 1;
      }
    }
  }

  /* Process */
  tspos = mm->mm_input_pos;
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
        atomic_add(&mmi->tii_stats.te, 1);
    }

    pid &= 0x1FFF;

    /* Ignore NULL packets */
    if (pid == 0x1FFF) {
      if (tsb[4] == 'T' && tsb[5] == 'V')
        tsdebug_check_tspkt(mm, tsb, llen);
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
            tvhtrace(LS_MPEGTS, "%s: pid %04X cc err %2d != %2d", mm->mm_nicename, pid, cc, cc2);
            atomic_add(&mmi->tii_stats.cc, 1);
          }
          cc2 = (cc + 1) & 0xF;
        }
        mp->mp_cc = cc2;
      }

      type = mp->mp_type;
      
      /* Stream all PIDs */
      LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
        if ((mps->mps_type & MPS_ALL) || (type & (MPS_TABLE|MPS_FTABLE)))
          ts_recv_raw((mpegts_service_t *)mps->mps_owner, tspos, tsb, llen);

      /* Stream raw PIDs */
      if (type & MPS_RAW) {
        LIST_FOREACH(mps, &mp->mp_raw_subs, mps_raw_link)
          ts_recv_raw((mpegts_service_t *)mps->mps_owner, tspos, tsb, llen);
      }

      /* Stream service data */
      if (type & MPS_SERVICE) {
        LIST_FOREACH(mps, &mp->mp_svc_subs, mps_svcraw_link) {
          s = mps->mps_owner;
          f = (type & (MPS_TABLE|MPS_FTABLE)) ||
              (pid == s->s_components.set_pmt_pid) ||
              (pid == s->s_components.set_pcr_pid);
          ts_recv_packet1((mpegts_service_t*)s, tspos, pid, tsb, llen, f);
        }
      } else
      /* Stream table data */
      if (type & MPS_STREAM) {
        LIST_FOREACH(s, &mm->mm_transports, s_active_link) {
          if (s->s_type != STYPE_STD) continue;
          f = (type & (MPS_TABLE|MPS_FTABLE)) ||
              (pid == s->s_components.set_pmt_pid) ||
              (pid == s->s_components.set_pcr_pid);
          ts_recv_packet1((mpegts_service_t*)s, tspos, pid, tsb, llen, f);
        }
      }

      /* Table data */
      if (type & (MPS_TABLE | MPS_FTABLE)) {
        if (!(tsb[1] & 0x80)) {
          if (type & MPS_FTABLE)
            mpegts_input_table_dispatch(mm, mm->mm_nicename, tsb, llen, 1);
          if (type & MPS_TABLE) {
            if (mi->mi_table_queue_size >= 2*1024*1024) {
              if (tvhlog_limit(&mi->mi_input_queue_loglimit, 10)) {
                tvhwarn(LS_MPEGTS, "too much queued table input data (over 2MB) for %s, discarding new", mi->mi_name);
                if (tvhtrace_enabled())
                  mpegts_input_analyze_table_queue(mi);
              }
            } else {
              mtf = TAILQ_LAST(&mi->mi_table_queue, mpegts_table_feed_queue);
              if (mtf && mtf->mtf_mux == mm && mtf->mtf_len + llen <= MPEGTS_MTF_ALLOC_CHUNK) {
                pid2 = (mtf->mtf_tsb[1] << 8) | mtf->mtf_tsb[2];
                if (pid == pid2) {
                  memcpy(mtf->mtf_tsb + mtf->mtf_len, tsb, llen);
                  memoryinfo_free(&mpegts_input_table_memoryinfo, sizeof(mpegts_table_feed_t) + mtf->mtf_len);
                  mtf->mtf_len += llen;
                  mi->mi_table_queue_size += llen;
                  memoryinfo_alloc(&mpegts_input_table_memoryinfo, sizeof(mpegts_table_feed_t) + mtf->mtf_len);
                } else {
                  mtf = NULL;
                }
              } else {
                mtf = NULL;
              }
              if (mtf == NULL)
                mpegts_input_table_feed_create(mi, mm, tsb, llen);
              table_wakeup = 1;
            }
          }
        } else {
          //tvhdebug("tsdemux", "%s - SI packet had errors", name);
        }
      }

    } else {

      /* Stream to all fullmux subscribers */
      LIST_FOREACH(mps, &mm->mm_all_subs, mps_svcraw_link)
        ts_recv_raw((mpegts_service_t *)mps->mps_owner, tspos, tsb, llen);

    }

done:
    tsb += llen;
    len -= llen;
    tspos += llen;
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

  /* Wake table */
  if (table_wakeup)
    tvh_cond_signal(&mi->mi_table_cond, 0);

  /* Bandwidth monitoring */
  llen = tsb - mpkt->mp_data;
  atomic_add(&mmi->tii_stats.bps, llen);
  mm->mm_input_pos += llen;
  return llen;
}

/*
 * Demux again data from one mpeg-ts stream.
 * Might be used for hardware descramblers.
 */
void
mpegts_input_postdemux
  ( mpegts_input_t *mi, mpegts_mux_t *mm, uint8_t *tsb, int len )
{
  uint16_t pid;
  int llen, type = 0;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;
  service_t *s;
  elementary_stream_t *st;
  mpegts_mux_instance_t *mmi;

  tvh_mutex_lock(&mi->mi_output_lock);
  if (mm == NULL || (mmi = mm->mm_active) == NULL)
    goto unlock;

  assert(mm == mmi->mmi_mux);

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

    pid &= 0x1FFF;

    /* Ignore NULL packets */
    if (pid == 0x1FFF)
      goto done;

    /* Find PID */
    if ((mp = mpegts_mux_find_pid(mm, pid, 0))) {

      type = mp->mp_type;

      if (type & MPS_NOPOSTDEMUX)
        goto done;

      /* Stream service data */
      if (type & MPS_SERVICE) {
        LIST_FOREACH(mps, &mp->mp_svc_subs, mps_svcraw_link) {
          s = mps->mps_owner;
          tvh_mutex_lock(&s->s_stream_mutex);
          st = elementary_stream_find(&s->s_components, pid);
          ts_recv_packet0((mpegts_service_t*)s, st, tsb, llen);
          tvh_mutex_unlock(&s->s_stream_mutex);
        }
      } else
      /* Stream table data */
      if (type & MPS_STREAM) {
        LIST_FOREACH(s, &mm->mm_transports, s_active_link) {
          if (s->s_type != STYPE_STD) continue;
          tvh_mutex_lock(&s->s_stream_mutex);
          ts_recv_packet0((mpegts_service_t*)s, NULL, tsb, llen);
          tvh_mutex_unlock(&s->s_stream_mutex);
        }
      }

    }

done:
    tsb += llen;
    len -= llen;
  }
unlock:
  tvh_mutex_unlock(&mi->mi_output_lock);
}

static void *
mpegts_input_thread ( void * p )
{
  mpegts_packet_t *mp;
  mpegts_input_t *mi = p;
  size_t bytes = 0;
  int update_pids;
  tprofile_t tprofile;
  char buf[256];

  tvh_mutex_lock(&global_lock);
  mi->mi_display_name(mi, buf, sizeof(buf));
  tvh_mutex_unlock(&global_lock);

  tprofile_init(&tprofile, buf);

  tvh_mutex_lock(&mi->mi_input_lock);
  while (atomic_get(&mi->mi_running)) {

    /* Wait for a packet */
    if (!(mp = TAILQ_FIRST(&mi->mi_input_queue))) {
      if (bytes) {
        tvhtrace(LS_MPEGTS, "input %s got %zu bytes", buf, bytes);
        bytes = 0;
      }
      tvh_cond_wait(&mi->mi_input_cond, &mi->mi_input_lock);
      continue;
    }
    mi->mi_input_queue_size -= mp->mp_len;
    memoryinfo_free(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + mp->mp_len);
    TAILQ_REMOVE(&mi->mi_input_queue, mp, mp_link);
    tvh_mutex_unlock(&mi->mi_input_lock);
      
    /* Process */
    tvh_mutex_lock(&mi->mi_output_lock);
    mpegts_input_table_waiting(mi, mp->mp_mux);
    if (mp->mp_mux && mp->mp_mux->mm_update_pids_flag) {
      tvh_mutex_unlock(&mi->mi_output_lock);
      tvh_mutex_lock(&global_lock);
      mpegts_mux_update_pids(mp->mp_mux);
      tvh_mutex_unlock(&global_lock);
      tvh_mutex_lock(&mi->mi_output_lock);
    }
    tprofile_start(&tprofile, "input");
    bytes += mpegts_input_process(mi, mp);
    tprofile_finish(&tprofile);
    update_pids = mp->mp_mux && mp->mp_mux->mm_update_pids_flag;
    tvh_mutex_unlock(&mi->mi_output_lock);
    if (update_pids) {
      tvh_mutex_lock(&global_lock);
      mpegts_mux_update_pids(mp->mp_mux);
      tvh_mutex_unlock(&global_lock);
    }

    /* Cleanup */
    if (mp->mp_mux)
      mpegts_mux_release(mp->mp_mux);
    free(mp);

#if ENABLE_TSDEBUG
    {
      extern void tsdebugcw_go(void);
      tsdebugcw_go();
    }
#endif

    tvh_mutex_lock(&mi->mi_input_lock);
  }

  tvhtrace(LS_MPEGTS, "input %s got %zu bytes (finish)", buf, bytes);

  /* Flush */
  while ((mp = TAILQ_FIRST(&mi->mi_input_queue))) {
    memoryinfo_free(&mpegts_input_queue_memoryinfo, sizeof(mpegts_packet_t) + mp->mp_len);
    TAILQ_REMOVE(&mi->mi_input_queue, mp, mp_link);
    if (mp->mp_mux)
      mpegts_mux_release(mp->mp_mux);
    free(mp);
  }
  mi->mi_input_queue_size = 0;
  tvh_mutex_unlock(&mi->mi_input_lock);

  tprofile_done(&tprofile);

  return NULL;
}

static void *
mpegts_input_table_thread ( void *aux )
{
  mpegts_table_feed_t *mtf;
  mpegts_input_t *mi = aux;
  mpegts_mux_t *mm = NULL;

  tvh_mutex_lock(&mi->mi_output_lock);
  while (atomic_get(&mi->mi_running)) {

    /* Wait for data */
    if (!(mtf = TAILQ_FIRST(&mi->mi_table_queue))) {
      tvh_cond_wait(&mi->mi_table_cond, &mi->mi_output_lock);
      continue;
    }
    mi->mi_table_queue_size -= mtf->mtf_len;
    memoryinfo_free(&mpegts_input_table_memoryinfo, sizeof(mpegts_table_feed_t) + mtf->mtf_len);
    TAILQ_REMOVE(&mi->mi_table_queue, mtf, mtf_link);
    tvh_mutex_unlock(&mi->mi_output_lock);

    /* Process */
    tvh_mutex_lock(&global_lock);
    if (atomic_get(&mi->mi_running)) {
      mm = mtf->mtf_mux;
      if (mm && mm->mm_active) {
        if (mtf->mtf_cc_restart)
          mpegts_input_table_restart(mm, mm->mm_nicename, 0);
        mpegts_input_table_dispatch(mm, mm->mm_nicename, mtf->mtf_tsb, mtf->mtf_len, 0);
      }
    }
    tvh_mutex_unlock(&global_lock);

    /* Cleanup */
    free(mtf);
    tvh_mutex_lock(&mi->mi_output_lock);
  }

  /* Flush */
  while ((mtf = TAILQ_FIRST(&mi->mi_table_queue)) != NULL) {
    memoryinfo_free(&mpegts_input_table_memoryinfo, sizeof(mpegts_table_feed_t) + mtf->mtf_len);
    TAILQ_REMOVE(&mi->mi_table_queue, mtf, mtf_link);
    free(mtf);
  }
  mi->mi_table_queue_size = 0;
  tvh_mutex_unlock(&mi->mi_output_lock);

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
  tvh_mutex_lock(&mi->mi_input_lock);
  TAILQ_FOREACH(mp, &mi->mi_input_queue, mp_link) {
    if (mp->mp_mux == mm) {
      mpegts_mux_release(mm);
      mp->mp_mux = NULL;
    }
  }
  tvh_mutex_unlock(&mi->mi_input_lock);

  /* Flush table Q */
  tvh_mutex_lock(&mi->mi_output_lock);
  TAILQ_FOREACH(mtf, &mi->mi_table_queue, mtf_link) {
    if (mtf->mtf_mux == mm)
      mtf->mtf_mux = NULL;
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
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
  mpegts_pid_t *mp;

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

  st->pids = mpegts_pid_alloc();
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    if (mp->mp_pid == MPEGTS_TABLES_PID)
      continue;
    if (mp->mp_pid == MPEGTS_FULLMUX_PID)
      st->pids->all = 1;
    else
      mpegts_pid_add(st->pids, mp->mp_pid, 0);
  }

  tvh_mutex_lock(&mmi->tii_stats_mutex);
  st->stats.signal = mmi->tii_stats.signal;
  st->stats.snr    = mmi->tii_stats.snr;
  st->stats.ber    = mmi->tii_stats.ber;
  st->stats.signal_scale = mmi->tii_stats.signal_scale;
  st->stats.snr_scale    = mmi->tii_stats.snr_scale;
  st->stats.ec_bit   = mmi->tii_stats.ec_bit;
  st->stats.tc_bit   = mmi->tii_stats.tc_bit;
  st->stats.ec_block = mmi->tii_stats.ec_block;
  st->stats.tc_block = mmi->tii_stats.tc_block;
  tvh_mutex_unlock(&mmi->tii_stats_mutex);
  st->stats.unc   = atomic_get(&mmi->tii_stats.unc);
  st->stats.cc    = atomic_get(&mmi->tii_stats.cc);
  st->stats.te    = atomic_get(&mmi->tii_stats.te);
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
    st->stats.unc += atomic_get(&mmi->tii_stats.unc);
    st->stats.cc += atomic_get(&mmi->tii_stats.cc);
    tvh_mutex_lock(&mmi->tii_stats_mutex);
    st->stats.te += mmi->tii_stats.te;
    st->stats.ec_block += mmi->tii_stats.ec_block;
    st->stats.tc_block += mmi->tii_stats.tc_block;
    tvh_mutex_unlock(&mmi->tii_stats_mutex);
  }
}

static void
mpegts_input_get_streams
  ( tvh_input_t *i, tvh_input_stream_list_t *isl )
{
  tvh_input_stream_t *st = NULL;
  mpegts_input_t *mi = (mpegts_input_t*)i;
  mpegts_mux_instance_t *mmi;

  tvh_mutex_lock(&mi->mi_output_lock);
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
  tvh_mutex_unlock(&mi->mi_output_lock);
}

static void
mpegts_input_clear_stats ( tvh_input_t *i )
{
  mpegts_input_t *mi = (mpegts_input_t*)i;
  tvh_input_instance_t *mmi_;
  mpegts_mux_instance_t *mmi;

  tvh_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi_, &mi->mi_mux_instances, tii_input_link) {
    mmi = (mpegts_mux_instance_t *)mmi_;
    atomic_set(&mmi->tii_stats.unc, 0);
    atomic_set(&mmi->tii_stats.cc, 0);
    tvh_mutex_lock(&mmi->tii_stats_mutex);
    mmi->tii_stats.te = 0;
    mmi->tii_stats.ec_block = 0;
    mmi->tii_stats.tc_block = 0;
    tvh_mutex_unlock(&mmi->tii_stats_mutex);
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
  notify_reload("input_status");
}

static void
mpegts_input_thread_start ( void *aux )
{
  mpegts_input_t *mi = aux;
  atomic_set(&mi->mi_running, 1);
  
  tvh_thread_create(&mi->mi_table_tid, NULL,
                    mpegts_input_table_thread, mi, "mi-table");
  tvh_thread_create(&mi->mi_input_tid, NULL,
                    mpegts_input_thread, mi, "mi-main");
}

static void
mpegts_input_thread_stop ( mpegts_input_t *mi )
{
  atomic_set(&mi->mi_running, 0);
  mtimer_disarm(&mi->mi_input_thread_start);

  /* Stop input thread */
  tvh_mutex_lock(&mi->mi_input_lock);
  tvh_cond_signal(&mi->mi_input_cond, 0);
  tvh_mutex_unlock(&mi->mi_input_lock);

  /* Stop table thread */
  tvh_mutex_lock(&mi->mi_output_lock);
  tvh_cond_signal(&mi->mi_table_cond, 0);
  tvh_mutex_unlock(&mi->mi_output_lock);

  /* Join threads (relinquish lock due to potential deadlock) */
  tvh_mutex_unlock(&global_lock);
  if (mi->mi_input_tid)
    pthread_join(mi->mi_input_tid, NULL);
  if (mi->mi_table_tid)
    pthread_join(mi->mi_table_tid, NULL);
  tvh_mutex_lock(&global_lock);
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

  tvh_mutex_lock(&mi->mi_output_lock);
  LIST_FOREACH(mmi, &mi->mi_mux_active, mmi_active_link) {
    memset(&st, 0, sizeof(st));
    mpegts_input_stream_status(mmi, &st);
    e = tvh_input_stream_create_msg(&st);
    htsmsg_add_u32(e, "update", 1);
    notify_by_msg("input_status", e, 0);
    subs += st.subs_count;
    tvh_input_stream_destroy(&st);
  }
  tvh_mutex_unlock(&mi->mi_output_lock);
  mtimer_arm_rel(&mi->mi_status_timer, mpegts_input_status_timer, mi, sec2mono(1));
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
  char buf[32];

  if (idnode_insert(&mi->ti_id, uuid, class, 0)) {
    if (uuid)
      tvherror(LS_MPEGTS, "invalid input uuid '%s'", uuid);
    free(mi);
    return NULL;
  }
  snprintf(buf, sizeof(buf), "input %p", mi);
  tprofile_queue_init(&mi->mi_qprofile, buf);
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
  mi->mi_error                = mpegts_input_error;
  mi->ti_get_streams          = mpegts_input_get_streams;
  mi->ti_clear_stats          = mpegts_input_clear_stats;

  /* Index */
  mi->mi_instance             = ++mpegts_input_idx;

  /* Init input/output structures */
  tvh_mutex_init(&mi->mi_input_lock, NULL);
  tvh_cond_init(&mi->mi_input_cond, 1);
  TAILQ_INIT(&mi->mi_input_queue);

  tvh_mutex_init(&mi->mi_output_lock, NULL);
  tvh_cond_init(&mi->mi_table_cond, 1);
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
  mtimer_arm_rel(&mi->mi_input_thread_start, mpegts_input_thread_start, mi, 0);

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
  atomic_set(&mi->mi_running, 0);

  idnode_save_check(&mi->ti_id, delconf);

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

  tprofile_queue_done(&mi->mi_qprofile);
  tvh_mutex_destroy(&mi->mi_output_lock);
  tvh_cond_destroy(&mi->mi_table_cond);
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
