/*
 *  Electronic Program Guide - Common functions
 *  Copyright (C) 2012 Adam Sutton
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include "tvheadend.h"
#include "config.h"
#include "queue.h"
#include "channels.h"
#include "settings.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "epggrab.h"
#include "imagecache.h"
#include "notify.h"
#include "string_list.h"

/* Broadcast hashing */
#define EPG_HASH_WIDTH 1024
#define EPG_HASH_MASK  (EPG_HASH_WIDTH - 1)

/* Objects tree */
epg_object_tree_t epg_objects[EPG_HASH_WIDTH];

/* URI lists */
epg_set_tree_t epg_serieslinks;
epg_set_tree_t epg_episodelinks;

/* Other special case lists */
epg_object_list_t epg_object_unref;
epg_object_list_t epg_object_updated;

int epg_in_load;

/* Global counter */
static uint32_t _epg_object_idx    = 0;

/*
 *
 */
static inline epg_object_tree_t *epg_id_tree( epg_object_t *eo )
{
  return &epg_objects[eo->id & EPG_HASH_MASK];
}

/* **************************************************************************
 * Comparators / Ordering
 * *************************************************************************/

static int _id_cmp ( const void *a, const void *b )
{
  return ((epg_object_t*)a)->id - ((epg_object_t*)b)->id;
}

static int _ebc_start_cmp ( const void *a, const void *b )
{
  return ((epg_broadcast_t*)a)->start - ((epg_broadcast_t*)b)->start;
}

static int _ebc_xmltv_cmp ( const void *a, const void *b )
{

  //Sometimes, nulls are passed to this function and the strcmp() crashes.
  if(!((epg_broadcast_t*)a)->xmltv_eid)
  {
    return -1;
  }
  if(!((epg_broadcast_t*)b)->xmltv_eid)
  {
    return 1;
  }

  return strcmp(((epg_broadcast_t*)a)->xmltv_eid, ((epg_broadcast_t*)b)->xmltv_eid);
}

void epg_updated ( void )
{
  epg_object_t *eo;

  lock_assert(&global_lock);

  /* Remove unref'd */
  while ((eo = LIST_FIRST(&epg_object_unref))) {
    tvhtrace(LS_EPG,
             "unref'd object %u created during update", eo->id);
    LIST_REMOVE(eo, un_link);
    eo->ops->destroy(eo);
  }
  // Note: we do things this way around since unref'd objects are not likely
  //       to be useful to DVR since they will relate to episodes
  //       with no valid broadcasts etc..

  /* Update updated */
  while ((eo = LIST_FIRST(&epg_object_updated))) {
    eo->ops->update(eo);
    LIST_REMOVE(eo, up_link);
    eo->_updated = 0;
    eo->_created = 1;
  }
}

/* **************************************************************************
 * Object (Generic routines)
 * *************************************************************************/

static void _epg_object_destroy
  ( epg_object_t *eo, epg_object_tree_t *tree )
{
  assert(eo->refcount == 0);
  tvhtrace(LS_EPG, "eo [%p, %u, %d] destroy",
           eo, eo->id, eo->type);
  if (eo->_updated) LIST_REMOVE(eo, up_link);
  RB_REMOVE(epg_id_tree(eo), eo, id_link);
}

static void _epg_object_getref ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace(LS_EPG, "eo [%p, %u, %d] getref %d",
           eo, eo->id, eo->type, eo->refcount+1);
  if (eo->refcount == 0) LIST_REMOVE(eo, un_link);
  eo->refcount++;
}

static int _epg_object_putref ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace(LS_EPG, "eo [%p, %u, %d] putref %d",
           eo, eo->id, eo->type, eo->refcount-1);
  assert(eo->refcount>0);
  eo->refcount--;
  if (!eo->refcount) {
    eo->ops->destroy(eo);
    return 1;
  }
  return 0;
}

static void _epg_object_set_updated0 ( void *o )
{
  epg_object_t *eo = o;
  tvhtrace(LS_EPG, "eo [%p, %u, %d] updated",
           eo, eo->id, eo->type);
  eo->_updated = 1;
  eo->updated  = gclk();
  LIST_INSERT_HEAD(&epg_object_updated, eo, up_link);
}

static inline void _epg_object_set_updated ( void *o )
{
  if (!((epg_object_t *)o)->_updated)
    _epg_object_set_updated0(o);
}

static int _epg_object_can_remove ( void *_old, void *_new )
{
  epggrab_module_t *ograb, *ngrab;
  ngrab = ((epg_object_t *)_new)->grabber;
  if (ngrab == NULL) return 0;
  ograb = ((epg_object_t *)_old)->grabber;
  if (ograb == NULL || ograb == ngrab) return 1;
  if (ngrab->priority > ograb->priority) return 1;
  return 0;
}

static int _epg_object_set_grabber ( void *o, epggrab_module_t *ngrab )
{
  epggrab_module_t *ograb;
  if (!ngrab) return 1; // grab=NULL is override
  ograb = ((epg_object_t *)o)->grabber;
  if (ograb == ngrab) return 1;
  if (ograb && ograb->priority >= ngrab->priority) return 0;
  ((epg_object_t *)o)->grabber = ngrab;
  return 1;
}

static void _epg_object_create ( void *o )
{
  epg_object_t *eo = o;
  uint32_t id = eo->id;
  if (!id) eo->id = ++_epg_object_idx;
  if (!eo->id) eo->id = ++_epg_object_idx;
  tvhtrace(LS_EPG, "eo [%p, %u, %d] created",
           eo, eo->id, eo->type);
  _epg_object_set_updated(eo);
  LIST_INSERT_HEAD(&epg_object_unref, eo, un_link);
  while (1) {
    if (!RB_INSERT_SORTED(epg_id_tree(eo), eo, id_link, _id_cmp))
      break;
    if (id) {
      tvherror(LS_EPG, "fatal error, duplicate EPG ID");
      abort();
    }
    eo->id = ++_epg_object_idx;
    if (!eo->id) eo->id = ++_epg_object_idx;
  }
}

epg_object_t *epg_object_find_by_id ( uint32_t id, epg_object_type_t type )
{
  epg_object_t *eo, temp;
  temp.id = id;
  eo = RB_FIND(epg_id_tree(&temp), &temp, id_link, _id_cmp);
  if (eo && eo->type == type)
    return eo;
  return NULL;
}

static htsmsg_t * _epg_object_serialize ( void *o )
{
  htsmsg_t *m;
  epg_object_t *eo = o;
  tvhtrace(LS_EPG, "eo [%p, %u, %d] serialize",
           eo, eo->id, eo->type);
  if (!eo->id || !eo->type) return NULL;
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "id", eo->id);
  htsmsg_add_u32(m, "tp", eo->type);
  if (eo->grabber)
    htsmsg_add_str(m, "gr", eo->grabber->id);
  htsmsg_add_s64(m, "up", eo->updated);
  return m;
}

static epg_object_t *_epg_object_deserialize ( htsmsg_t *m, epg_object_t *eo )
{
  int64_t s64;
  uint32_t u32;
  const char *s;
  if (htsmsg_get_u32(m, "id", &eo->id)) return NULL;
  if (htsmsg_get_u32(m, "tp", &u32))    return NULL;
  if (u32 != eo->type)                    return NULL;
  if ((s = htsmsg_get_str(m, "gr")))
    eo->grabber = epggrab_module_find_by_id(s);
  if (!htsmsg_get_s64(m, "up", &s64)) {
    _epg_object_set_updated(eo);
    eo->updated = s64;
  }
  tvhtrace(LS_EPG, "eo [%p, %u, %d, %s, %s] deserialize",
           eo, eo->id, eo->type, s, eo->grabber ? eo->grabber->id : NULL);
  return eo;
}

static int _epg_object_set_str
  ( void *o, char **old, const char *newstr,
    epg_changes_t *changed, epg_changes_t cflag )
{
  int save = 0;
  epg_object_t *eo = o;
  if (!eo) return 0;
  if (changed) *changed |= cflag;
  if (!*old && !newstr) return 0;
  if (!*old || !newstr || strcmp(*old, newstr)) {
    free(*old);
    *old = newstr ? strdup(newstr) : NULL;
    _epg_object_set_updated(eo);
    save = 1;
  }
  return save;
}

/* "Template" for setting objects. */
#define EPG_OBJECT_SET_FN(FNNAME,TYPE,DESTROY,COMPARE,COPY) \
static int FNNAME \
  ( void *o, TYPE **old, const TYPE *new, \
    epg_changes_t *changed, epg_changes_t cflag ) \
{ \
  if (!o) return 0; \
  if (changed) *changed |= cflag; \
  if (!*old) { \
    if (!new) \
      return 0; \
  } \
  if (!new) { \
    DESTROY(*old); \
    *old = NULL; \
    _epg_object_set_updated(o); \
    return 1; \
  } \
  if (COMPARE(*old, new)) { \
    DESTROY(*old); \
    *old = COPY(new); \
    _epg_object_set_updated(o); \
    return 1; \
  } \
  return 0; \
}

EPG_OBJECT_SET_FN(_epg_object_set_lang_str, lang_str_t,
                  lang_str_destroy, lang_str_compare, lang_str_copy)
EPG_OBJECT_SET_FN(_epg_object_set_string_list, string_list_t,
                  string_list_destroy, string_list_cmp, string_list_copy)
EPG_OBJECT_SET_FN(_epg_object_set_htsmsg, htsmsg_t,
                  htsmsg_destroy, htsmsg_cmp, htsmsg_copy)
#undef EPG_OBJECT_SET_FN

#define EPG_OBJECT_SET_FN(FNNAME,TYPE) \
static int FNNAME \
  ( void *o, TYPE *old, const TYPE nval, \
    epg_changes_t *changed, epg_changes_t cflag ) \
{ \
  int save; \
  if (!o) return 0; \
  if (changed) *changed |= cflag; \
  if ((save = (*old != nval)) != 0) { \
    *old = nval; \
    _epg_object_set_updated(o); \
  } \
  return save; \
}

EPG_OBJECT_SET_FN(_epg_object_set_u8, uint8_t)
EPG_OBJECT_SET_FN(_epg_object_set_u16, uint16_t)
#undef EPG_OBJECT_SET_FN

htsmsg_t *epg_object_serialize ( epg_object_t *eo )
{
  if (!eo) return NULL;
  switch (eo->type) {
    case EPG_BROADCAST:
      return epg_broadcast_serialize((epg_broadcast_t*)eo);
    default:
      return NULL;
  }
}

epg_object_t *epg_object_deserialize ( htsmsg_t *msg, int create, int *save )
{
  uint32_t type;
  if (!msg) return NULL;
  type = htsmsg_get_u32_or_default(msg, "type", 0);
  switch (type) {
    case EPG_BROADCAST:
      return (epg_object_t*)epg_broadcast_deserialize(msg, create, save);
  }
  return NULL;
}

/* **************************************************************************
 * Episode
 * *************************************************************************/

htsmsg_t *epg_episode_epnum_serialize ( epg_episode_num_t *num )
{
  htsmsg_t *m;
  if (!num) return NULL;
  if (!num->e_num && !num->e_cnt &&
      !num->s_num && !num->s_cnt &&
      !num->p_num && !num->p_cnt &&
      !num->text) return NULL;
  m = htsmsg_create_map();
  if (num->e_num)
    htsmsg_add_u32(m, "enum", num->e_num);
  if (num->e_cnt)
    htsmsg_add_u32(m, "ecnt", num->e_cnt);
  if (num->s_num)
    htsmsg_add_u32(m, "snum", num->s_num);
  if (num->s_cnt)
    htsmsg_add_u32(m, "scnt", num->s_cnt);
  if (num->p_num)
    htsmsg_add_u32(m, "pnum", num->p_num);
  if (num->p_cnt)
    htsmsg_add_u32(m, "pcnt", num->p_cnt);
  if (num->text)
    htsmsg_add_str(m, "text", num->text);
  return m;
}

void epg_episode_epnum_deserialize
  ( htsmsg_t *m, epg_episode_num_t *num )
{
  const char *str;
  uint32_t u32;
  assert(m && num);

  memset(num, 0, sizeof(epg_episode_num_t));

  if (!htsmsg_get_u32(m, "enum", &u32) ||
      !htsmsg_get_u32(m, "e_num", &u32))
    num->e_num = u32;
  if (!htsmsg_get_u32(m, "ecnt", &u32) ||
      !htsmsg_get_u32(m, "e_cnt", &u32))
    num->e_cnt = u32;
  if (!htsmsg_get_u32(m, "snum", &u32) ||
      !htsmsg_get_u32(m, "s_num", &u32))
    num->s_num = u32;
  if (!htsmsg_get_u32(m, "scnt", &u32) ||
      !htsmsg_get_u32(m, "s_cnt", &u32))
    num->s_cnt = u32;
  if (!htsmsg_get_u32(m, "pnum", &u32) ||
      !htsmsg_get_u32(m, "p_num", &u32))
    num->p_num = u32;
  if (!htsmsg_get_u32(m, "pcnt", &u32) ||
      !htsmsg_get_u32(m, "p_cnt", &u32))
    num->p_cnt = u32;
  if ((str = htsmsg_get_str(m, "text")))
    num->text = strdup(str);
}

size_t epg_episode_num_format
  ( epg_episode_num_t *epnum, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt )
{
  size_t i = 0;
  if (!epnum || !buf || !len) return 0;
  buf[0] = '\0';
  if (epnum->e_num || epnum->s_num) {
    if (pre) tvh_strlcatf(buf, len, i, "%s", pre);
    if (sfmt && epnum->s_num) {
      tvh_strlcatf(buf, len, i, sfmt, epnum->s_num);
      if (cfmt && epnum->s_cnt)
        tvh_strlcatf(buf, len, i, cfmt, epnum->s_cnt);
      if (sep && efmt && epnum->e_num) tvh_strlcatf(buf, len, i, "%s", sep);
    }
    if (efmt && epnum->e_num)
      tvh_strlcatf(buf, len, i, efmt, epnum->e_num);
    if (cfmt && epnum->e_cnt)
      tvh_strlcatf(buf, len, i, cfmt, epnum->e_cnt);
  } else if (epnum->text) {
    if (pre) tvh_strlcatf(buf, len, i, "%s", pre);
    tvh_strlcatf(buf, len, i, "%s", epnum->text);
  }
  return i;
}

int epg_episode_number_cmp ( const epg_episode_num_t *a, const epg_episode_num_t *b )
{
  if (a->e_num) {
    if (a->s_num != b->s_num) {
      return a->s_num - b->s_num;
    } else if (a->e_num != b->e_num) {
      return a->e_num - b->e_num;
    } else {
      return a->p_num - b->p_num;
    }
  } else if (a->text && b->text) {
    return strcmp(a->text, b->text);
  }
  return 0;
}

int epg_episode_number_cmpfull ( const epg_episode_num_t *a, const epg_episode_num_t *b )
{
  int i = a->s_cnt - b->s_cnt;
  if (i) return i;
  i = a->s_num - b->s_num;
  if (i) return i;
  i = a->e_cnt - b->e_cnt;
  if (i) return i;
  i = a->e_num - b->e_num;
  if (i) return i;
  i = a->p_cnt - b->p_cnt;
  if (i) return i;
  i = a->p_num - b->p_num;
  if (i) return i;
  return strcasecmp(a->text ?: "", b->text ?: "");
}

/* **************************************************************************
 * Channel
 * *************************************************************************/

int epg_channel_ignore_broadcast(channel_t *ch, time_t start)
{
  if (ch->ch_epg_limit && start >= gclk() + ch->ch_epg_limit * 3600 * 24)
    return 1;
  return 0;
}

static void _epg_channel_rem_broadcast
  ( channel_t *ch, epg_broadcast_t *ebc, epg_broadcast_t *ebc_new )
{
  RB_REMOVE(&ch->ch_epg_schedule, ebc, sched_link);
  if (ch->ch_epg_now  == ebc) ch->ch_epg_now  = NULL;
  if (ch->ch_epg_next == ebc) ch->ch_epg_next = NULL;
  if (ebc_new) {
    dvr_event_replaced(ebc, ebc_new);
  } else {
    dvr_event_removed(ebc);
  }
  _epg_object_putref(ebc);
}

static void _epg_channel_timer_callback ( void *p )
{
  time_t next = 0;
  epg_broadcast_t *ebc, *cur, *nxt;
  channel_t *ch = (channel_t*)p;
  char tm1[32];

  /* Clear now/next */
  if ((cur = ch->ch_epg_now)) {
    if (cur->running != EPG_RUNNING_STOP && cur->running != EPG_RUNNING_NOTSET) {
      /* running? don't do anything */
      gtimer_arm_rel(&ch->ch_epg_timer, _epg_channel_timer_callback, ch, 2);
      return;
    }
    cur->ops->getref(cur);
  }
  if ((nxt = ch->ch_epg_next))
    nxt->ops->getref(nxt);
  ch->ch_epg_now = ch->ch_epg_next = NULL;

  /* Check events */
  while ((ebc = RB_FIRST(&ch->ch_epg_schedule))) {

    /* Expire */
    if ( ebc->stop <= gclk() ) {
      tvhdebug(LS_EPG, "expire event %u (%s) from %s",
               ebc->id, epg_broadcast_get_title(ebc, NULL),
               channel_get_name(ch, channel_blank_name));
      _epg_channel_rem_broadcast(ch, ebc, NULL);
      continue; // skip to next

    /* No now */
    } else if (ebc->start > gclk()) {
      ch->ch_epg_next = ebc;
      next            = ebc->start;

    /* Now/Next */
    } else {
      ch->ch_epg_now  = ebc;
      ch->ch_epg_next = RB_NEXT(ebc, sched_link);
      next            = ebc->stop;
    }
    break;
  }

  /* Change (update HTSP) */
  if (cur != ch->ch_epg_now || nxt != ch->ch_epg_next) {
    tvhdebug(LS_EPG, "now/next %u/%u set on %s",
             ch->ch_epg_now  ? ch->ch_epg_now->id : 0,
             ch->ch_epg_next ? ch->ch_epg_next->id : 0,
             channel_get_name(ch, channel_blank_name));
    tvhdebug(LS_EPG, "inform HTSP of now event change on %s",
             channel_get_name(ch, channel_blank_name));
    htsp_channel_update_nownext(ch);
  }

  /* re-arm */
  if (next) {
    tvhdebug(LS_EPG, "arm channel timer @ %s for %s",
             gmtime2local(next, tm1, sizeof(tm1)), channel_get_name(ch, channel_blank_name));
    gtimer_arm_absn(&ch->ch_epg_timer, _epg_channel_timer_callback, ch, next);
  }

  /* Remove refs */
  if (cur) cur->ops->putref(cur);
  if (nxt) nxt->ops->putref(nxt);
}

static epg_broadcast_t *_epg_channel_add_broadcast
  ( channel_t *ch, epg_broadcast_t **bcast, epggrab_module_t *src,
    int create, int *save, epg_changes_t *changed )
{
  int timer = 0;
  epg_broadcast_t *ebc, *ret;
  char tm1[32], tm2[32];

  if (!src) {
    tvherror(LS_EPG, "skipped event (!grabber) %u (%s) on %s @ %s to %s",
             (*bcast)->id, epg_broadcast_get_title(*bcast, NULL),
             channel_get_name(ch, channel_blank_name),
             gmtime2local((*bcast)->start, tm1, sizeof(tm1)),
             gmtime2local((*bcast)->stop, tm2, sizeof(tm2)));
    return NULL;
  }

  /* Set channel */
  (*bcast)->channel = ch;

  /* Find (only) */
  if ( !create ) {
    if((*bcast)->xmltv_eid)
    {
      return RB_FIND(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_xmltv_cmp);
    }
    else
    {
      return RB_FIND(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_start_cmp);
    }

  /* Find/Create */
  } else {
    if((*bcast)->xmltv_eid)
    {
      ret = RB_INSERT_SORTED(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_xmltv_cmp);
    }
    else
    {
      ret = RB_INSERT_SORTED(&ch->ch_epg_schedule, *bcast, sched_link, _ebc_start_cmp);
    }

    /* New */
    if (!ret) {
      if (changed) *changed |= EPG_CHANGED_CREATE;
      *save  = 1;
      ret    = *bcast;
      *bcast = NULL;
      _epg_object_create(ret);
      // Note: sets updated
      _epg_object_getref(ret);
      ret->grabber = src;
      tvhtrace(LS_EPG, "added event %u (%s) on %s @ %s to %s (grabber %s)",
               ret->id, epg_broadcast_get_title(ret, NULL),
               channel_get_name(ch, channel_blank_name),
               gmtime2local(ret->start, tm1, sizeof(tm1)),
               gmtime2local(ret->stop, tm2, sizeof(tm2)),
               src->id);

    /* Existing */
    } else {
      if (!_epg_object_set_grabber(ret, src))
        return NULL;

      /* No time change */
      if (ret->stop == (*bcast)->stop) {
        return ret;

      /* Extend in time */
      } else {
        ret->stop = (*bcast)->stop;
        _epg_object_set_updated(ret);
        tvhtrace(LS_EPG, "updated event %u (%s) on %s @ %s to %s (grabber %s)",
                 ret->id, epg_broadcast_get_title(ret, NULL),
                 channel_get_name(ch, channel_blank_name),
                 gmtime2local(ret->start, tm1, sizeof(tm1)),
                 gmtime2local(ret->stop, tm2, sizeof(tm2)),
                 src->id);
      }
    }
  }

  /* Changed */
  *save |= 1;

  /* Remove overlapping (before) */
  while ((ebc = RB_PREV(ret, sched_link)) != NULL) {
    if (ebc->stop <= ret->start) break;
    if (!_epg_object_can_remove(ebc, ret)) {
      tvhtrace(LS_EPG, "grabber %s for event %u has higher priority than overlap (b), removing added",
               ebc->grabber->id, ebc->id);
      _epg_channel_rem_broadcast(ch, ret, NULL);
      return NULL;
    }
    if (config.epg_cut_window && ebc->stop - ebc->start > config.epg_cut_window * 2 &&
        ebc->stop - ret->start <= config.epg_cut_window) {
      tvhtrace(LS_EPG, "cut stop for overlap (b) event %u (%s) on %s @ %s to %s",
               ebc->id, epg_broadcast_get_title(ebc, NULL),
               channel_get_name(ch, channel_blank_name),
               gmtime2local(ebc->start, tm1, sizeof(tm1)),
               gmtime2local(ebc->stop, tm2, sizeof(tm2)));
      ebc->stop = ret->start;
      _epg_object_set_updated(ebc);
      continue;
    }
    tvhtrace(LS_EPG, "remove overlap (b) event %u (%s) on %s @ %s to %s",
             ebc->id, epg_broadcast_get_title(ebc, NULL),
             channel_get_name(ch, channel_blank_name),
             gmtime2local(ebc->start, tm1, sizeof(tm1)),
             gmtime2local(ebc->stop, tm2, sizeof(tm2)));
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Remove overlapping (after) */
  while ((ebc = RB_NEXT(ret, sched_link)) != NULL) {
    if (ebc->start >= ret->stop) break;
    if (!_epg_object_can_remove(ebc, ret)) {
      tvhtrace(LS_EPG, "grabber %s for event %u has higher priority than overlap (a), removing added",
               ebc->grabber->id, ebc->id);
      _epg_channel_rem_broadcast(ch, ret, NULL);
      return NULL;
    }
    if (config.epg_cut_window && ret->stop - ret->start > config.epg_cut_window * 2 &&
        ret->stop - ebc->start <= config.epg_cut_window) {
      tvhtrace(LS_EPG, "cut stop for overlap (a) event %u (%s) on %s @ %s to %s",
               ebc->id, epg_broadcast_get_title(ebc, NULL),
               channel_get_name(ch, channel_blank_name),
               gmtime2local(ebc->start, tm1, sizeof(tm1)),
               gmtime2local(ebc->stop, tm2, sizeof(tm2)));
      ret->stop = ebc->start;
      _epg_object_set_updated(ebc);
      continue;
    }
    tvhtrace(LS_EPG, "remove overlap (a) event %u (%s) on %s @ %s to %s",
             ebc->id, epg_broadcast_get_title(ebc, NULL),
             channel_get_name(ch, channel_blank_name),
             gmtime2local(ebc->start, tm1, sizeof(tm1)),
             gmtime2local(ebc->stop, tm2, sizeof(tm2)));
    _epg_channel_rem_broadcast(ch, ebc, ret);
  }

  /* Check now/next change */
  if (RB_FIRST(&ch->ch_epg_schedule) == ret) {
    timer = 1;
  } else if (ch->ch_epg_now &&
             RB_NEXT(ch->ch_epg_now, sched_link) == ret) {
    timer = 1;
  }

  /* Reset timer - it might free return event! */
  ret->ops->getref(ret);
  if (timer) _epg_channel_timer_callback(ch);
  if (ret->ops->putref(ret)) return NULL;
  return ret;
}// END _epg_channel_add_broadcast

void epg_channel_unlink ( channel_t *ch )
{
  epg_broadcast_t *ebc;
  while ((ebc = RB_FIRST(&ch->ch_epg_schedule)))
    _epg_channel_rem_broadcast(ch, ebc, NULL);
  gtimer_disarm(&ch->ch_epg_timer);
}

static int epg_match_event_fuzzy(epg_broadcast_t *a, epg_broadcast_t *b)
{
  time_t t1, t2;
  const char *title1, *title2;
  epg_episode_num_t num1, num2;

  if (a == NULL || b == NULL)
    return 0;

  /* Matching ID */
  if (a->dvb_eid) {
    if (b->dvb_eid && a->dvb_eid == b->dvb_eid)
      return 1;
    return 0;
  }

  /* Wrong length (+/-20%) */
  t1 = a->stop - a->start;
  t2 = b->stop - b->start;
  if (labs((long)(t2 - t1)) > (t1 / 5))
    return 0;

  /* No title */
  if (!(title1 = epg_broadcast_get_title(a, NULL)))
    return 0;
  if (!(title2 = epg_broadcast_get_title(b, NULL)))
    return 0;

  /* Outside of window */
  if ((int64_t)llabs(b->start - a->start) > config.epg_update_window)
    return 0;

  /* Title match (or contains?) */
  if (strcasecmp(title1, title2))
    return 0;

  /* episode check */
  epg_broadcast_get_epnum(a, &num1);
  epg_broadcast_get_epnum(b, &num2);
  if (epg_episode_number_cmp(&num1, &num2) == 0)
    return 1;

  return 0;
}

epg_broadcast_t *epg_match_now_next ( channel_t *ch, epg_broadcast_t *ebc )
{
  epg_broadcast_t *ret;

  if (epg_match_event_fuzzy(ch->ch_epg_now, ebc))
    ret = ch->ch_epg_now;
  else if (epg_match_event_fuzzy(ch->ch_epg_next, ebc))
    ret = ch->ch_epg_next;
  else
    return NULL;
  /* update eid for further lookups */
  if (ret->dvb_eid != ebc->dvb_eid && ret->dvb_eid == 0 && ebc->dvb_eid)
    ret->dvb_eid = ebc->dvb_eid;
  return ret;
}

/* **************************************************************************
 * Broadcast set
 * *************************************************************************/

static int _set_uri_cmp(const void *_a, const void *_b)
{
  const epg_set_t *a = _a, *b = _b;
  return strcmp(a->uri, b->uri);
}

epg_set_t *epg_set_broadcast_find_by_uri
  (epg_set_tree_t *tree, const char *uri)
{
  epg_set_t *dummy;

  if (tree == NULL || uri == NULL || uri[0] == '\0')
    return NULL;
  dummy = (epg_set_t *)(uri - offsetof(epg_set_t, uri));
  return RB_FIND(tree, dummy, set_link, _set_uri_cmp);
}


epg_set_t *epg_set_broadcast_insert
  (epg_set_tree_t *tree, epg_broadcast_t *b, const char *uri)
{
  epg_set_t *es;
  epg_set_item_t *item;

  if (tree == NULL || b == NULL)
    return NULL;
  if (uri == NULL || uri[0] == '\0')
    return NULL;
  es = epg_set_broadcast_find_by_uri(tree, uri);
  if (!es) {
    es = malloc(sizeof(*es) + strlen(uri) + 1);
    LIST_INIT(&es->broadcasts);
    strcpy(es->uri, uri);
    RB_INSERT_SORTED(tree, es, set_link, _set_uri_cmp);
  }
  item = malloc(sizeof(*item));
  item->broadcast = b;
  LIST_INSERT_HEAD(&es->broadcasts, item, item_link);
  return es;
}

void epg_set_broadcast_remove
  (epg_set_tree_t *tree, epg_set_t *set, epg_broadcast_t *b)
{
  epg_set_item_t *item;

  if (tree == NULL || set == NULL)
    return;
  LIST_FOREACH(item, &set->broadcasts, item_link)
    if (item->broadcast == b) {
      LIST_REMOVE(item, item_link);
      free(item);
      if (LIST_EMPTY(&set->broadcasts)) {
        RB_REMOVE(tree, set, set_link);
        free(set);
      }
      return;
    }
  assert(0);
}

/* **************************************************************************
 * Broadcast
 * *************************************************************************/

static void _epg_broadcast_destroy ( void *eo )
{
  epg_broadcast_t *ebc = eo;
  epg_genre_t *eg;
  char id[16];

  if (ebc->_created) {
    htsp_event_delete(ebc);
    snprintf(id, sizeof(id), "%u", ebc->id);
    notify_delayed(id, "epg", "delete");
  }
  if (ebc->title)       lang_str_destroy(ebc->title);
  if (ebc->subtitle)    lang_str_destroy(ebc->subtitle);
  if (ebc->summary)     lang_str_destroy(ebc->summary);
  if (ebc->description) lang_str_destroy(ebc->description);
  while ((eg = LIST_FIRST(&ebc->genre))) {
    LIST_REMOVE(eg, link);
    free(eg);
  }
  free(ebc->image);
  free(ebc->epnum.text);
  if (ebc->credits)     htsmsg_destroy(ebc->credits);
  if (ebc->credits_cached) lang_str_destroy(ebc->credits_cached);
  if (ebc->category)    string_list_destroy(ebc->category);
  if (ebc->keyword)     string_list_destroy(ebc->keyword);
  if (ebc->keyword_cached) lang_str_destroy(ebc->keyword_cached);
  epg_set_broadcast_remove(&epg_serieslinks, ebc->serieslink, ebc);
  epg_set_broadcast_remove(&epg_episodelinks, ebc->episodelink, ebc);
  _epg_object_destroy(eo, NULL);
  assert(LIST_EMPTY(&ebc->dvr_entries));
  free(ebc);
}

static void _epg_broadcast_update_running ( epg_broadcast_t *broadcast )
{
  channel_t *ch;
  epg_broadcast_t *now;
  int orunning = broadcast->running;

  broadcast->running = broadcast->update_running;
  ch = broadcast->channel;
  now = ch ? ch->ch_epg_now : NULL;
  if (broadcast->update_running == EPG_RUNNING_STOP) {
    if (now == broadcast && orunning == broadcast->running)
      broadcast->stop = gclk() - 1;
  } else {
    if (broadcast != now && now) {
      now->running = EPG_RUNNING_STOP;
      dvr_event_running(now, EPG_RUNNING_STOP);
    }
  }
  dvr_event_running(broadcast, broadcast->running);
  broadcast->update_running = EPG_RUNNING_NOTSET;
}

static void _epg_broadcast_updated ( void *eo )
{
  epg_broadcast_t *ebc = eo;
  char id[16];

  if (!epg_in_load)
    snprintf(id, sizeof(id), "%u", ebc->id);
  else
    id[0] = '\0';

  if (ebc->_created) {
    htsp_event_update(eo);
    notify_delayed(id, "epg", "update");
  } else {
    htsp_event_add(eo);
    notify_delayed(id, "epg", "create");
  }
  if (ebc->channel) {
    dvr_event_updated(eo);
    if (ebc->update_running != EPG_RUNNING_NOTSET)
      _epg_broadcast_update_running(ebc);
    dvr_autorec_check_event(eo);
    channel_event_updated(eo);
  }
}

static epg_object_ops_t _epg_broadcast_ops = {
  .getref  = _epg_object_getref,
  .putref  = _epg_object_putref,
  .destroy = _epg_broadcast_destroy,
  .update  = _epg_broadcast_updated,
};

static epg_broadcast_t **_epg_broadcast_skel ( void )
{
  static epg_broadcast_t *skel = NULL;
  if (!skel) {
    skel = calloc(1, sizeof(epg_broadcast_t));
    skel->type = EPG_BROADCAST;
    skel->ops  = &_epg_broadcast_ops;
  }
  return &skel;
}

//Prepare an EPG struct to search for an extant event
//using the XMLTV unique ID.
epg_broadcast_t *epg_broadcast_find_by_xmltv_eid
  ( channel_t *channel, epggrab_module_t *src,
    time_t start, time_t stop, int create,
    int *save, epg_changes_t *changed, const char *xmltv_eid)
{
  epg_broadcast_t **ebc;
  int             ret = 0;
  if (!channel || !start || !stop || !xmltv_eid) return NULL;
  if (stop <= start) return NULL;
  if (stop <= gclk()) return NULL;

  ebc = _epg_broadcast_skel();
  (*ebc)->start         = start;
  (*ebc)->stop          = stop;

  if((*ebc)->xmltv_eid)
  {
    free((*ebc)->xmltv_eid);
    (*ebc)->xmltv_eid     = NULL;
  }
  
  ret = epg_broadcast_set_xmltv_eid(*ebc, xmltv_eid, changed);

  //If the XMLTV ID was not set, exit.
  if(!ret){
    tvherror(LS_EPG, "Unable to set '%s' result '%d'", xmltv_eid, ret);
    return NULL;
  }

  return _epg_channel_add_broadcast(channel, ebc, src, create, save, changed);
}

epg_broadcast_t *epg_broadcast_find_by_time
  ( channel_t *channel, epggrab_module_t *src,
    time_t start, time_t stop, int create, int *save, epg_changes_t *changed )
{
  epg_broadcast_t **ebc;
  if (!channel || !start || !stop) return NULL;
  if (stop <= start) return NULL;
  if (stop <= gclk()) return NULL;

  ebc = _epg_broadcast_skel();
  (*ebc)->start   = start;
  (*ebc)->stop    = stop;
  (*ebc)->xmltv_eid = NULL;

  return _epg_channel_add_broadcast(channel, ebc, src, create, save, changed);
}

int epg_broadcast_change_finish
  ( epg_broadcast_t *broadcast, epg_changes_t changes, int merge )
{
  int save = 0;
  if (merge) return 0;
  if (changes & EPG_CHANGED_CREATE) return 0;
  if (!(changes & EPG_CHANGED_SERIESLINK))
    save |= epg_broadcast_set_serieslink_uri(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_EPISODE))
    save |= epg_broadcast_set_episodelink_uri(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_DVB_EID))
    {
      save |= epg_broadcast_set_dvb_eid(broadcast, 0, NULL);
      save |= epg_broadcast_set_xmltv_eid(broadcast, NULL, NULL);
    }
  if (!(changes & EPG_CHANGED_IS_WIDESCREEN))
    save |= epg_broadcast_set_is_widescreen(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_IS_HD))
    save |= epg_broadcast_set_is_hd(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_LINES))
    save |= epg_broadcast_set_lines(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_ASPECT))
    save |= epg_broadcast_set_aspect(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_DEAFSIGNED))
    save |= epg_broadcast_set_is_deafsigned(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_SUBTITLED))
    save |= epg_broadcast_set_is_subtitled(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_AUDIO_DESC))
    save |= epg_broadcast_set_is_audio_desc(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_IS_NEW))
    save |= epg_broadcast_set_is_new(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_IS_REPEAT))
    save |= epg_broadcast_set_is_repeat(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_IS_BW))
    save |= epg_broadcast_set_is_bw(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_STAR_RATING))
    save |= epg_broadcast_set_star_rating(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_AGE_RATING))
    save |= epg_broadcast_set_age_rating(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_RATING_LABEL))
    save |= epg_broadcast_set_rating_label(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_IMAGE))
    save |= epg_broadcast_set_image(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_GENRE))
    save |= epg_broadcast_set_genre(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_TITLE))
    save |= epg_broadcast_set_title(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_SUBTITLE))
    save |= epg_broadcast_set_subtitle(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_SUMMARY))
    save |= epg_broadcast_set_summary(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_DESCRIPTION))
    save |= epg_broadcast_set_description(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_EPSER_NUM))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.s_num, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPSER_CNT))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.s_cnt, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPNUM_NUM))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.e_num, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPNUM_CNT))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.e_cnt, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPPAR_NUM))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.p_num, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPPAR_CNT))
    save |= _epg_object_set_u16(broadcast, &broadcast->epnum.p_cnt, 0, NULL, 0);
  if (!(changes & EPG_CHANGED_EPTEXT))
    save |= _epg_object_set_str(broadcast, &broadcast->epnum.text, NULL, NULL, 0);
  if (!(changes & EPG_CHANGED_FIRST_AIRED))
    save |= epg_broadcast_set_first_aired(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_COPYRIGHT_YEAR))
    save |= epg_broadcast_set_copyright_year(broadcast, 0, NULL);
  if (!(changes & EPG_CHANGED_CREDITS))
    save |= epg_broadcast_set_credits(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_CATEGORY))
    save |= epg_broadcast_set_category(broadcast, NULL, NULL);
  if (!(changes & EPG_CHANGED_KEYWORD))
    save |= epg_broadcast_set_keyword(broadcast, NULL, NULL);
  return save;
}

epg_broadcast_t *epg_broadcast_clone
  ( channel_t *channel, epg_broadcast_t *src, int *save )
{
  epg_broadcast_t *ebc;
  epg_changes_t changes = 0;

  if (!src) return NULL;
  ebc = epg_broadcast_find_by_time(channel, src->grabber,
                                   src->start, src->stop,
                                   1, save, &changes);
  if (ebc) {
    /* Copy metadata */
    *save |= epg_broadcast_set_xmltv_eid(ebc, src->xmltv_eid, &changes);
    *save |= epg_broadcast_set_is_widescreen(ebc, src->is_widescreen, &changes);
    *save |= epg_broadcast_set_is_hd(ebc, src->is_hd, &changes);
    *save |= epg_broadcast_set_is_bw(ebc, src->is_bw, &changes);
    *save |= epg_broadcast_set_lines(ebc, src->lines, &changes);
    *save |= epg_broadcast_set_aspect(ebc, src->aspect, &changes);
    *save |= epg_broadcast_set_is_deafsigned(ebc, src->is_deafsigned, &changes);
    *save |= epg_broadcast_set_is_subtitled(ebc, src->is_subtitled, &changes);
    *save |= epg_broadcast_set_is_audio_desc(ebc, src->is_audio_desc, &changes);
    *save |= epg_broadcast_set_is_new(ebc, src->is_new, &changes);
    *save |= epg_broadcast_set_is_repeat(ebc, src->is_repeat, &changes);
    *save |= epg_broadcast_set_star_rating(ebc, src->star_rating, &changes);
    *save |= epg_broadcast_set_age_rating(ebc, src->age_rating, &changes);
    *save |= epg_broadcast_set_rating_label(ebc, src->rating_label, &changes);
    *save |= epg_broadcast_set_image(ebc, src->image, &changes);
    *save |= epg_broadcast_set_genre(ebc, &src->genre, &changes);
    *save |= epg_broadcast_set_title(ebc, src->title, &changes);
    *save |= epg_broadcast_set_subtitle(ebc, src->subtitle, &changes);
    *save |= epg_broadcast_set_summary(ebc, src->summary, &changes);
    *save |= epg_broadcast_set_description(ebc, src->description, &changes);
    *save |= epg_broadcast_set_epnum(ebc, &src->epnum, &changes);
    *save |= epg_broadcast_set_credits(ebc, src->credits, &changes);
    *save |= epg_broadcast_set_category(ebc, src->category, &changes);
    *save |= epg_broadcast_set_keyword(ebc, src->keyword, &changes);
    *save |= epg_broadcast_set_serieslink_uri
               (ebc, src->serieslink ? src->serieslink->uri : NULL, &changes);
    *save |= epg_broadcast_set_episodelink_uri
               (ebc, src->episodelink ? src->episodelink->uri : NULL, &changes);
    *save |= epg_broadcast_set_first_aired(ebc, src->first_aired, &changes);
    *save |= epg_broadcast_set_copyright_year(ebc, src->copyright_year, &changes);
    _epg_object_set_grabber(ebc, src->grabber);
    *save |= epg_broadcast_change_finish(ebc, changes, 0);
  }
  return ebc;
}

epg_broadcast_t *epg_broadcast_find_by_id ( uint32_t id )
{
  return (epg_broadcast_t*)epg_object_find_by_id(id, EPG_BROADCAST);
}

epg_broadcast_t *epg_broadcast_find_by_eid ( channel_t *ch, uint16_t eid )
{
  epg_broadcast_t *e;
  RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
    if (e->dvb_eid == eid && e->stop > gclk()) return e;
  }
  return NULL;
}

int epg_broadcast_set_running
  ( epg_broadcast_t *broadcast, epg_running_t running )
{
  int save = 0;
  if (running != broadcast->running) {
    broadcast->update_running = running;
    _epg_object_set_updated(broadcast);
    save = 1;
  }
  return save;
}

static int _epg_broadcast_set_set
  ( epg_broadcast_t *ebc, const char *uri,
    epg_set_tree_t *tree, epg_set_t **set )
{
  if (*set == NULL) {
    if (uri == NULL || uri[0] == '\0')
      return 0;
  } else if (strcmp((*set)->uri, uri ?: "")) {
    epg_set_broadcast_remove(tree, *set, ebc);
  } else {
    return 0;
  }
  *set = epg_set_broadcast_insert(tree, ebc, uri);
  return 1;
}


int epg_broadcast_set_serieslink_uri
  ( epg_broadcast_t *ebc, const char *uri, epg_changes_t *changed )
{
  if (!ebc) return 0;
  if (changed) *changed |= EPG_CHANGED_SERIESLINK;
  return _epg_broadcast_set_set(ebc, uri, &epg_serieslinks, &ebc->serieslink);
}

int epg_broadcast_set_episodelink_uri
  ( epg_broadcast_t *ebc, const char *uri, epg_changes_t *changed )
{
  if (!ebc) return 0;
  if (changed) *changed |= EPG_CHANGED_EPISODE;
  return _epg_broadcast_set_set(ebc, uri, &epg_episodelinks, &ebc->episodelink);
}

int epg_broadcast_set_dvb_eid
  ( epg_broadcast_t *b, uint16_t dvb_eid, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->dvb_eid, dvb_eid,
                             changed, EPG_CHANGED_DVB_EID);
}

int epg_broadcast_set_xmltv_eid
  ( epg_broadcast_t *b, const char *xmltv_eid, epg_changes_t *changed )
{
  int save;
  if (!b) return 0;
  save = _epg_object_set_str(b, &b->xmltv_eid, xmltv_eid,
                             changed, EPG_CHANGED_DVB_EID);

  return save;
}

int epg_broadcast_set_is_widescreen
  ( epg_broadcast_t *b, uint8_t ws, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_widescreen, ws,
                            changed, EPG_CHANGED_IS_WIDESCREEN);
}

int epg_broadcast_set_is_hd
  ( epg_broadcast_t *b, uint8_t hd, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_hd, hd,
                            changed, EPG_CHANGED_IS_HD);
}

int epg_broadcast_set_lines
  ( epg_broadcast_t *b, uint16_t lines, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->lines, lines,
                             changed, EPG_CHANGED_LINES);
}

int epg_broadcast_set_aspect
  ( epg_broadcast_t *b, uint16_t aspect, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->aspect, aspect,
                             changed, EPG_CHANGED_ASPECT);
}

int epg_broadcast_set_is_deafsigned
  ( epg_broadcast_t *b, uint8_t ds, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_deafsigned, ds,
                            changed, EPG_CHANGED_DEAFSIGNED);
}

int epg_broadcast_set_is_subtitled
  ( epg_broadcast_t *b, uint8_t st, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_subtitled, st,
                            changed, EPG_CHANGED_SUBTITLED);
}

int epg_broadcast_set_is_audio_desc
  ( epg_broadcast_t *b, uint8_t ad, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_audio_desc, ad,
                            changed, EPG_CHANGED_AUDIO_DESC);
}

int epg_broadcast_set_is_new
  ( epg_broadcast_t *b, uint8_t n, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_new, n,
                            changed, EPG_CHANGED_IS_NEW);
}

int epg_broadcast_set_is_repeat
  ( epg_broadcast_t *b, uint8_t r, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_repeat, r,
                            changed, EPG_CHANGED_IS_REPEAT);
}

int epg_broadcast_set_is_bw
  ( epg_broadcast_t *b, uint8_t bw, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->is_bw, bw,
                            changed, EPG_CHANGED_IS_BW);
}

int epg_broadcast_set_star_rating
  ( epg_broadcast_t *b, uint8_t stars, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->star_rating, stars,
                            changed, EPG_CHANGED_STAR_RATING);
}

int epg_broadcast_set_title
  ( epg_broadcast_t *b, const lang_str_t *title, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->title, title,
                                  changed, EPG_CHANGED_TITLE);
}

int epg_broadcast_set_subtitle
  ( epg_broadcast_t *b, const lang_str_t *subtitle, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->subtitle,
                                  subtitle, changed, EPG_CHANGED_SUBTITLE);
}

int epg_broadcast_set_summary
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->summary, str,
                                  changed, EPG_CHANGED_SUMMARY);
}

int epg_broadcast_set_description
  ( epg_broadcast_t *b, const lang_str_t *str, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_lang_str(b, &b->description, str,
                                  changed, EPG_CHANGED_DESCRIPTION);
}

int epg_broadcast_set_credits
( epg_broadcast_t *b, const htsmsg_t *credits, epg_changes_t *changed )
{
  if (!b) return 0;
  const int mod = _epg_object_set_htsmsg(b, &b->credits, credits, changed, EPG_CHANGED_CREDITS);
  if (mod) {
      /* Copy in to cached csv for regex searching in autorec/GUI.
       * We use just one string (rather than regex across each entry
       * separately) so you could do a regex of "Douglas.*Stallone"
       * to match the movies with the two actors.
       */
      if (!b->credits_cached) {
          b->credits_cached = lang_str_create();
      }
      lang_str_set(&b->credits_cached, "", NULL);

      if (b->credits) {
        int add_sep = 0;
        htsmsg_field_t *f;
        HTSMSG_FOREACH(f, b->credits) {
          if (add_sep) {
            lang_str_append(b->credits_cached, ", ", NULL);
          } else {
            add_sep = 1;
          }
          lang_str_append(b->credits_cached, htsmsg_field_name(f), NULL);
        }
      } else {
        if (b->credits_cached) {
          lang_str_destroy(b->credits_cached);
          b->credits_cached = NULL;
        }
      }
  }
  return mod;
}

int epg_broadcast_set_category
( epg_broadcast_t *b, const string_list_t *msg, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_string_list(b, &b->category, msg, changed, EPG_CHANGED_CATEGORY);
}

int epg_broadcast_set_keyword
( epg_broadcast_t *b, const string_list_t *msg, epg_changes_t *changed )
{
  if (!b) return 0;
  const int mod = _epg_object_set_string_list(b, &b->keyword, msg, changed, EPG_CHANGED_KEYWORD);
  if (mod) {
    /* Copy in to cached csv for regex searching in autorec/GUI. */
    if (msg) {
      /* 1==>human readable */
      char *str = string_list_2_csv(msg, ',', 1);
      lang_str_set(&b->keyword_cached, str, NULL);
      free(str);
    } else {
      if (b->keyword_cached) {
        lang_str_destroy(b->keyword_cached);
        b->keyword_cached = NULL;
      }
    }
  }
  return mod;
}

int epg_broadcast_set_image
  ( epg_broadcast_t *b, const char *image, epg_changes_t *changed )
{
  int save;
  if (!b) return 0;
  save = _epg_object_set_str(b, &b->image, image,
                             changed, EPG_CHANGED_IMAGE);
  if (save)
    imagecache_get_id(image);
  return save;
}

int epg_broadcast_set_epnumber
  ( epg_broadcast_t *b, uint16_t number, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->epnum.e_num, number,
                             changed, EPG_CHANGED_EPNUM_NUM);
}

int epg_broadcast_set_eppart
  ( epg_broadcast_t *b, uint16_t part, uint16_t count,
    epg_changes_t *changed )
{
  int save = 0;
  if (!b) return 0;
  save |= _epg_object_set_u16(b, &b->epnum.p_num, part,
                              changed, EPG_CHANGED_EPPAR_NUM);
  save |= _epg_object_set_u16(b, &b->epnum.p_cnt, count,
                              changed, EPG_CHANGED_EPPAR_CNT);
  return save;
}

int epg_broadcast_set_epnum
  ( epg_broadcast_t *b, epg_episode_num_t *num, epg_changes_t *changed )
{
  int save = 0;
  static epg_episode_num_t _zero = { 0 };
  if (!b)
    return 0;
  if (!num)
    num = &_zero;
  if (num->s_num)
    save |= _epg_object_set_u16(b, &b->epnum.s_num,
                                num->s_num, changed, EPG_CHANGED_EPSER_NUM);
  if (num->s_cnt)
    save |= _epg_object_set_u16(b, &b->epnum.s_cnt,
                                num->s_cnt, changed, EPG_CHANGED_EPSER_CNT);
  if (num->e_num)
    save |= _epg_object_set_u16(b, &b->epnum.e_num,
                                num->e_num, changed, EPG_CHANGED_EPNUM_NUM);
  if (num->e_cnt)
    save |= _epg_object_set_u16(b, &b->epnum.e_cnt,
                                num->e_cnt, changed, EPG_CHANGED_EPNUM_CNT);
  if (num->p_num)
    save |= _epg_object_set_u16(b, &b->epnum.p_num,
                                num->p_num, changed, EPG_CHANGED_EPPAR_NUM);
  if (num->p_cnt)
    save |= _epg_object_set_u16(b, &b->epnum.p_cnt,
                                num->p_cnt, changed, EPG_CHANGED_EPPAR_CNT);
  if (num->text)
    save |= _epg_object_set_str(b, &b->epnum.text,
                                num->text, changed, EPG_CHANGED_EPTEXT);
  return save;
}

int epg_broadcast_set_genre
  ( epg_broadcast_t *b, epg_genre_list_t *genre, epg_changes_t *changed )
{
  int save = 0;
  epg_genre_t *g1, *g2;

  if (!b) return 0;

  if (changed) *changed |= EPG_CHANGED_GENRE;

  g1 = LIST_FIRST(&b->genre);

  /* Remove old */
  while (g1) {
    g2 = LIST_NEXT(g1, link);
    if (!epg_genre_list_contains(genre, g1, 0)) {
      LIST_REMOVE(g1, link);
      free(g1);
      save = 1;
    }
    g1 = g2;
  }

  /* Insert all entries */
  if (genre) {
    LIST_FOREACH(g1, genre, link)
      save |= epg_genre_list_add(&b->genre, g1);
  }

  return save;
}

int epg_broadcast_set_copyright_year
  ( epg_broadcast_t *b, uint16_t year, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u16(b, &b->copyright_year, year,
                             changed, EPG_CHANGED_COPYRIGHT_YEAR);
}

int epg_broadcast_set_age_rating
  ( epg_broadcast_t *b, uint8_t age, epg_changes_t *changed )
{
  if (!b) return 0;
  return _epg_object_set_u8(b, &b->age_rating, age,
                            changed, EPG_CHANGED_AGE_RATING);
}

int epg_broadcast_set_rating_label
  ( epg_broadcast_t *b, ratinglabel_t *rating_label, epg_changes_t *changed )
{
  if (!b || !rating_label) return 0;

  if(rating_label != b->rating_label){
    b->rating_label = rating_label;
    if (changed) *changed |= EPG_CHANGED_RATING_LABEL;
    return 1;
  }

  return 0;
}

int epg_broadcast_set_first_aired
  ( epg_broadcast_t *b, time_t aired, epg_changes_t *changed )
{
  if (!b) return 0;
  if (changed) *changed |= EPG_CHANGED_FIRST_AIRED;
  if (b->first_aired != aired) {
    b->first_aired = aired;
    _epg_object_set_updated(b);
    return 1;
  }
  return 0;
}

epg_broadcast_t *epg_broadcast_get_prev ( epg_broadcast_t *b )
{
  if (!b) return NULL;
  return RB_PREV(b, sched_link);
}

epg_broadcast_t *epg_broadcast_get_next ( epg_broadcast_t *b )
{
  if (!b) return NULL;
  return RB_NEXT(b, sched_link);
}

const char *epg_broadcast_get_title ( const epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->title) return NULL;
  return lang_str_get(b->title, lang);
}

const char *epg_broadcast_get_subtitle ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->subtitle) return NULL;
  return lang_str_get(b->subtitle, lang);
}
const ratinglabel_t *epg_broadcast_get_rating_label ( epg_broadcast_t *b )
{
  if (!b || !b->rating_label) return NULL;
  return b->rating_label;
}

const char *epg_broadcast_get_summary ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->summary) return NULL;
  return lang_str_get(b->summary, lang);
}

const char *epg_broadcast_get_credits_cached ( epg_broadcast_t *b, const char *lang)
{
  if (!b || !b->credits_cached) return NULL;
  return lang_str_get(b->credits_cached, lang);
}

const char *epg_broadcast_get_keyword_cached ( epg_broadcast_t *b, const char *lang)
{
  if (!b || !b->keyword_cached) return NULL;
  return lang_str_get(b->keyword_cached, lang);
}

const char *epg_broadcast_get_description ( epg_broadcast_t *b, const char *lang )
{
  if (!b || !b->description) return NULL;
  return lang_str_get(b->description, lang);
}

/**
 * Take all of the string fields from an EPG record and concatenate
 * them into a monolithic merged string.
 *
 * Used for Autorec creation and interactive EPG search.
 *
 * [0x01]<TITLE_LANG1>[0x09]<TITLE_TEXT1>[0x09]<TITLE_LANG2><TITLE_TEXT2>[0x02]<SHORT_DESC_LANG1>[0x09]<SHORT_DESC_TEXT1>[0x09]<SHORT_DESC_LANG2>[0x09]<SHORT_DESCT_EXT2>[0x03][0x04][0x05][0x06][0x07]
 *
 * 0x01 = Title
 * 0x02 = Subtitle (Short Description)
 * 0x03 = Summary
 * 0x04 = Description
 * 0x05 = Credits
 * 0x06 = Keywords
 * 0x07 = Terminator
 *
 * 0x09 = Field separator (Tab)
 * 
 */
char* epg_broadcast_get_merged_text ( epg_broadcast_t *b )
{

  if (!b) return NULL;

  size_t            string_size = 8;  //Allow for a field mark for each field, even if null.
  lang_str_ele_t    *ls;
  char              *mergedtext = NULL;
  size_t            output_pos = 0;

  lang_str_t *fields[] = {
    b->title, b->subtitle, b->summary, b->description, b->credits_cached, b->keyword_cached
  };

  //First work out the concatenated string length
  int i = 0;  //Some older compiler versions don't like the variable declaration at the start of the for loop.
  for (i = 0; i < 6; i++) {
    if (fields[i]) {
      RB_FOREACH(ls, fields[i], link) {
        string_size += strlen(ls->str) + strlen(ls->lang) + 2; // 2 separators
      }
    }
  }

  //Now allocate a string big enough to hold the merged EPG fields.
  mergedtext = calloc(string_size, 1);
  if (!mergedtext) {
    tvhinfo(LS_EPG, "Unable to allocate string size '%zu' for merged text search.  Skipping search.", string_size);
    return NULL;
  }

  //Concatenate all of the EPG strings.
  for (i = 0; i < 6; i++) {
    mergedtext[output_pos++] = i + 1; // Field codes 0x01 to 0x06
    if (fields[i]) {
      RB_FOREACH(ls, fields[i], link) {
        mergedtext[output_pos++] = 0x09;
        size_t lang_len = strlen(ls->lang);
        memcpy(mergedtext + output_pos, ls->lang, lang_len);
        output_pos += lang_len;
        mergedtext[output_pos++] = 0x09;
        size_t str_len = strlen(ls->str);
        memcpy(mergedtext + output_pos, ls->str, str_len);
        output_pos += str_len;
      }
    }
  }

  mergedtext[output_pos++] = 0x07; //Add a terminator

  return mergedtext;
}//END epg_broadcast_get_merged_text

void epg_broadcast_get_epnum ( const epg_broadcast_t *b, epg_episode_num_t *num )
{
  if (!b || !num) {
    if (num)
      memset(num, 0, sizeof(*num));
    return;
  }
  *num = b->epnum;
}

size_t epg_broadcast_epnumber_format
  ( epg_broadcast_t *b, char *buf, size_t len,
    const char *pre,  const char *sfmt,
    const char *sep,  const char *efmt,
    const char *cfmt )
{
  if (!b) return 0;
  epg_episode_num_t num;
  epg_broadcast_get_epnum(b, &num);
  return epg_episode_num_format(&num, buf, len, pre,
                                sfmt, sep, efmt, cfmt);
}

htsmsg_t *epg_broadcast_serialize ( epg_broadcast_t *broadcast )
{
  htsmsg_t *m, *a;
  epg_genre_t *eg;
  char ubuf[UUID_HEX_SIZE];
  if (!broadcast) return NULL;
  if (!(m = _epg_object_serialize((epg_object_t*)broadcast))) return NULL;
  htsmsg_add_s64(m, "start", broadcast->start);
  htsmsg_add_s64(m, "stop", broadcast->stop);
  if (broadcast->channel)
    htsmsg_add_str(m, "ch", channel_get_uuid(broadcast->channel, ubuf));
  if (broadcast->dvb_eid)
    htsmsg_add_u32(m, "eid", broadcast->dvb_eid);
  if (broadcast->xmltv_eid)
    htsmsg_add_str(m, "xeid", broadcast->xmltv_eid);
  if (broadcast->is_widescreen)
    htsmsg_add_u32(m, "is_wd", 1);
  if (broadcast->is_hd)
    htsmsg_add_u32(m, "is_hd", 1);
  if (broadcast->is_bw)
    htsmsg_add_u32(m, "is_bw", 1);
  if (broadcast->lines)
    htsmsg_add_u32(m, "lines", broadcast->lines);
  if (broadcast->aspect)
    htsmsg_add_u32(m, "aspect", broadcast->aspect);
  if (broadcast->is_deafsigned)
    htsmsg_add_u32(m, "is_de", 1);
  if (broadcast->is_subtitled)
    htsmsg_add_u32(m, "is_st", 1);
  if (broadcast->is_audio_desc)
    htsmsg_add_u32(m, "is_ad", 1);
  if (broadcast->is_new)
    htsmsg_add_u32(m, "is_n", 1);
  if (broadcast->is_repeat)
    htsmsg_add_u32(m, "is_r", 1);
  if (broadcast->star_rating)
    htsmsg_add_u32(m, "star", broadcast->star_rating);
  if (broadcast->age_rating)
    htsmsg_add_u32(m, "age", broadcast->age_rating);
  if (broadcast->rating_label)
    {
      htsmsg_add_str(m, "ratlab", idnode_uuid_as_str((idnode_t *)(broadcast->rating_label), ubuf));
    }
  if (broadcast->image)
    htsmsg_add_str(m, "img", broadcast->image);
  if (broadcast->title)
    lang_str_serialize(broadcast->title, m, "tit");
  if (broadcast->subtitle)
    lang_str_serialize(broadcast->subtitle, m, "sti");
  if (broadcast->summary)
    lang_str_serialize(broadcast->summary, m, "sum");
  if (broadcast->description)
    lang_str_serialize(broadcast->description, m, "des");
  if ((a = epg_episode_epnum_serialize(&broadcast->epnum)) != NULL)
    htsmsg_add_msg(m, "epn", a);
  a = NULL;
  LIST_FOREACH(eg, &broadcast->genre, link) {
    if (!a) a = htsmsg_create_list();
    htsmsg_add_u32(a, NULL, eg->code);
  }
  if (a) htsmsg_add_msg(m, "genre", a);
  if (broadcast->copyright_year)
    htsmsg_add_u32(m, "cyear", broadcast->copyright_year);
  if (broadcast->first_aired)
    htsmsg_add_s64(m, "fair", broadcast->first_aired);
  if (broadcast->credits)
    htsmsg_add_msg(m, "cred", htsmsg_copy(broadcast->credits));
  /* No need to serialize credits_cached since it is rebuilt from credits. */
  if (broadcast->category)
    string_list_serialize(broadcast->category, m, "cat");
  if (broadcast->keyword)
    string_list_serialize(broadcast->keyword, m, "key");
  /* No need to serialize keyword_cached since it is rebuilt from keyword */
  if (broadcast->serieslink)
    htsmsg_add_str(m, "slink", broadcast->serieslink->uri);
  if (broadcast->episodelink)
    htsmsg_add_str(m, "elink", broadcast->episodelink->uri);
  return m;
}

epg_broadcast_t *epg_broadcast_deserialize
  ( htsmsg_t *m, int create, int *save )
{
  channel_t *ch = NULL;
  epg_broadcast_t *ebc, **skel = _epg_broadcast_skel();
  lang_str_t *ls;
  htsmsg_t *hm;
  htsmsg_field_t *f;
  string_list_t *sl;
  const char *str;
  uint32_t eid, u32;
  epg_changes_t changes = 0;
  int64_t start, stop, s64;
  epg_episode_num_t num;

  if (htsmsg_get_s64(m, "start", &start)) return NULL;
  if (htsmsg_get_s64(m, "stop", &stop)) return NULL;
  if (!start || !stop) return NULL;
  if (stop <= start) return NULL;
  if (stop <= gclk()) return NULL;

  _epg_object_deserialize(m, (epg_object_t*)*skel);

  /* Set properties */
  (*skel)->start   = start;
  (*skel)->stop    = stop;

  /* Get channel */
  if ((str = htsmsg_get_str(m, "ch")))
    ch = channel_find(str);
  if (!ch) return NULL;

  /* Create */
  ebc = _epg_channel_add_broadcast(ch, skel, (*skel)->grabber, create, save, &changes);
  if (!ebc) return NULL;

  /* Get metadata */
  if (!htsmsg_get_u32(m, "eid", &eid))
    *save |= epg_broadcast_set_dvb_eid(ebc, eid, &changes);
  if ((str = htsmsg_get_str(m, "xeid")))
    *save |= epg_broadcast_set_xmltv_eid(ebc, str, &changes);
  if (!htsmsg_get_u32(m, "is_wd", &u32))
    *save |= epg_broadcast_set_is_widescreen(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_hd", &u32))
    *save |= epg_broadcast_set_is_hd(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_bw", &u32))
    *save |= epg_broadcast_set_is_bw(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "lines", &u32))
    *save |= epg_broadcast_set_lines(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "aspect", &u32))
    *save |= epg_broadcast_set_aspect(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_de", &u32))
    *save |= epg_broadcast_set_is_deafsigned(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_st", &u32))
    *save |= epg_broadcast_set_is_subtitled(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_ad", &u32))
    *save |= epg_broadcast_set_is_audio_desc(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_n", &u32))
    *save |= epg_broadcast_set_is_new(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "is_r", &u32))
    *save |= epg_broadcast_set_is_repeat(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "star", &u32))
    *save |= epg_broadcast_set_star_rating(ebc, u32, &changes);
  if (!htsmsg_get_u32(m, "age", &u32))
    *save |= epg_broadcast_set_age_rating(ebc, u32, &changes);
  if ((str = htsmsg_get_str(m, "ratlab")))
  {
    //Convert the UUID string saved on disk into an idnode ID
    //and then fetch the ratinglabel object.
    *save |= epg_broadcast_set_rating_label(ebc, ratinglabel_find_from_uuid(str), &changes);
  }

  if ((str = htsmsg_get_str(m, "img")))
    *save |= epg_broadcast_set_image(ebc, str, &changes);

  if ((hm = htsmsg_get_list(m, "genre"))) {
    epg_genre_list_t *egl = calloc(1, sizeof(epg_genre_list_t));
    HTSMSG_FOREACH(f, hm) {
      epg_genre_t genre;
      genre.code = (uint8_t)f->hmf_s64;
      epg_genre_list_add(egl, &genre);
    }
    *save |= epg_broadcast_set_genre(ebc, egl, &changes);
    epg_genre_list_destroy(egl);
  }

  if ((ls = lang_str_deserialize(m, "tit"))) {
    *save |= epg_broadcast_set_title(ebc, ls, &changes);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "sti"))) {
    *save |= epg_broadcast_set_subtitle(ebc, ls, &changes);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "sum"))) {
    *save |= epg_broadcast_set_summary(ebc, ls, &changes);
    lang_str_destroy(ls);
  }
  if ((ls = lang_str_deserialize(m, "des"))) {
    *save |= epg_broadcast_set_description(ebc, ls, &changes);
    lang_str_destroy(ls);
  }

  if ((hm = htsmsg_get_map(m, "epn"))) {
    epg_episode_epnum_deserialize(hm, &num);
    *save |= epg_broadcast_set_epnum(ebc, &num, &changes);
    if (num.text) free(num.text);
  }

  if (!htsmsg_get_u32(m, "cyear", &u32))
    *save |= epg_broadcast_set_copyright_year(ebc, u32, &changes);
  if (!htsmsg_get_s64(m, "fair", &s64))
    *save |= epg_broadcast_set_first_aired(ebc, (time_t)s64, &changes);

  if ((hm = htsmsg_get_map(m, "cred")))
    *save |= epg_broadcast_set_credits(ebc, hm, &changes);

  if ((sl = string_list_deserialize(m, "key"))) {
    *save |= epg_broadcast_set_keyword(ebc, sl, &changes);
    string_list_destroy(sl);
  }
  if ((sl = string_list_deserialize(m, "cat"))) {
    *save |= epg_broadcast_set_category(ebc, sl, &changes);
    string_list_destroy(sl);
  }

  /* Series link */
  if ((str = htsmsg_get_str(m, "slink")))
    *save |= epg_broadcast_set_serieslink_uri(ebc, str, &changes);
  if ((str = htsmsg_get_str(m, "elink")))
    *save |= epg_broadcast_set_episodelink_uri(ebc, str, &changes);

  *save |= epg_broadcast_change_finish(ebc, changes, 0);

  return ebc;
}

/* **************************************************************************
 * Genre
 * *************************************************************************/

// TODO: make this configurable
// FULL(ish) list from EN 300 468, I've excluded the last category
// that relates more to broadcast content than what I call a "genre"
// these will be handled elsewhere as broadcast metadata

// Reference (Sept 2016):
// http://www.etsi.org/deliver/etsi_en/300400_300499/300468/01.11.01_60/en_300468v011101p.pdf

#define C_ (const char *[])
static const char **_epg_genre_names[16][16] = {
  { /* 00 */
    C_{ "", NULL  }
  },
  { /* 01 */
    C_{ N_("Movie"), N_("Drama"), NULL },
    C_{ N_("Detective"), N_("Thriller"), NULL },
    C_{ N_("Adventure"), N_("Western"), N_("War"), NULL },
    C_{ N_("Science fiction"), N_("Fantasy"), N_("Horror"), NULL },
    C_{ N_("Comedy"), NULL },
    C_{ N_("Soap"), N_("Melodrama"), N_("Folkloric"), NULL },
    C_{ N_("Romance"), NULL },
    C_{ N_("Serious"), N_("Classical"), N_("Religious"), N_("Historical movie"), N_("Drama"), NULL },
    C_{ N_("Adult movie"), N_("Drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
    C_{ N_("Movie / drama"), NULL },
  },
  { /* 02 */
    C_{ N_("News"), N_("Current affairs"), NULL },
    C_{ N_("News"), N_("Weather report"), NULL },
    C_{ N_("News magazine"), NULL },
    C_{ N_("Documentary"), NULL },
    C_{ N_("Discussion"), N_("Interview"), N_("Debate"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
    C_{ N_("News / Current Affairs"), NULL },
  },
  { /* 03 */
    C_{ N_("Show"), N_("Game show"), NULL },
    C_{ N_("Game show"), N_("Quiz"), N_("Contest"), NULL },
    C_{ N_("Variety show"), NULL },
    C_{ N_("Talk show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
    C_{ N_("Show / Game show"), NULL },
  },
  { /* 04 */
    C_{ N_("Sports"), NULL },
    C_{ N_("Special events (Olympic Games, World Cup, etc.)"), NULL },
    C_{ N_("Sports magazines"), NULL },
    C_{ N_("Football"), N_("Soccer"), NULL },
    C_{ N_("Tennis"), N_("Squash"), NULL },
    C_{ N_("Team sports (excluding football)"), NULL },
    C_{ N_("Athletics"), NULL },
    C_{ N_("Motor sport"), NULL },
    C_{ N_("Water sport"), NULL },
    C_{ N_("Winter sports"), NULL },
    C_{ N_("Equestrian"), NULL },
    C_{ N_("Martial sports"), NULL },
    C_{ N_("Sports"), NULL },
    C_{ N_("Sports"), NULL },
    C_{ N_("Sports"), NULL },
    C_{ N_("Sports"), NULL },
  },
  { /* 05 */
    C_{ N_("Children's / Youth programs"), NULL },
    C_{ N_("Pre-school children's programs"), NULL },
    C_{ N_("Entertainment programs for 6 to 14"), NULL },
    C_{ N_("Entertainment programs for 10 to 16"), NULL },
    C_{ N_("Informational"), N_("Educational"), N_("School programs"), NULL },
    C_{ N_("Cartoons"), N_("Puppets"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
    C_{ N_("Children's / Youth Programs"), NULL },
  },
  { /* 06 */
    C_{ N_("Music"), N_("Ballet"), N_("Dance"), NULL },
    C_{ N_("Rock"), N_("Pop"), NULL },
    C_{ N_("Serious music"), N_("Classical music"), NULL },
    C_{ N_("Folk"), N_("Traditional music"), NULL },
    C_{ N_("Jazz"), NULL },
    C_{ N_("Musical"), N_("Opera"), NULL },
    C_{ N_("Ballet"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
    C_{ N_("Music / Ballet / Dance"), NULL },
  },
  { /* 07 */
    C_{ N_("Arts"), N_("Culture (without music)"), NULL },
    C_{ N_("Performing arts"), NULL },
    C_{ N_("Fine arts"), NULL },
    C_{ N_("Religion"), NULL },
    C_{ N_("Popular culture"), N_("Traditional arts"), NULL },
    C_{ N_("Literature"), NULL },
    C_{ N_("Film"), N_("Cinema"), NULL },
    C_{ N_("Experimental film"), N_("Video"), NULL },
    C_{ N_("Broadcasting"), N_("Press"), NULL },
    C_{ N_("New media"), NULL },
    C_{ N_("Arts magazines"), N_("Culture magazines"), NULL },
    C_{ N_("Fashion"), NULL },
    C_{ N_("Arts / Culture (without music)"), NULL },
    C_{ N_("Arts / Culture (without music)"), NULL },
    C_{ N_("Arts / Culture (without music)"), NULL },
    C_{ N_("Arts / Culture (without music)"), NULL },
  },
  { /* 08 */
    C_{ N_("Social"), N_("Political issues"), N_("Economics"), NULL },
    C_{ N_("Magazines"), N_("Reports"), N_("Documentary"), NULL },
    C_{ N_("Economics"), N_("Social advisory"), NULL },
    C_{ N_("Remarkable people"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
    C_{ N_("Social / Political issues / Economics"), NULL },
  },
  { /* 09 */
    C_{ N_("Education"), N_("Science"), N_("Factual topics"), NULL },
    C_{ N_("Nature"), N_("Animals"), N_("Environment"), NULL },
    C_{ N_("Technology"), N_("Natural sciences"), NULL },
    C_{ N_("Medicine"), N_("Physiology"), N_("Psychology"), NULL },
    C_{ N_("Foreign countries"), N_("Expeditions"), NULL },
    C_{ N_("Social"), N_("Spiritual sciences"), NULL },
    C_{ N_("Further education"), NULL },
    C_{ N_("Languages"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
    C_{ N_("Education / Science / Factual topics"), NULL },
  },
  { /* 10 */
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Tourism / Travel"), NULL },
    C_{ N_("Handicraft"), NULL },
    C_{ N_("Motoring"), NULL },
    C_{ N_("Fitness and health"), NULL },
    C_{ N_("Cooking"), NULL },
    C_{ N_("Advertisement / Shopping"), NULL },
    C_{ N_("Gardening"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
    C_{ N_("Leisure hobbies"), NULL },
  }
};

static const char *_genre_get_name(int a, int b, const char *lang)
{
  static __thread char name[64];
  size_t l = 0;
  const char **p = _epg_genre_names[a][b];
  name[0] = '\0';
  if (p == NULL)
    return NULL;
  for ( ; *p; p++)
    tvh_strlcatf(name, sizeof(name), l, "%s%s", l ? " / " : "",
                 lang ? tvh_gettext_lang(lang, *p) : *p);
  return name;
}

// match strings, ignoring case and whitespace
// Note: | 0x20 is a cheats (fast) way of lowering case
static int _genre_str_match ( const char *a, const char *b )
{
  if (!a || !b) return 0;
  while (*a != '\0' || *b != '\0') {
    while (*a == ' ') a++;
    while (*b == ' ') b++;
    if ((*a | 0x20) != (*b | 0x20)) return 0;
    a++; b++;
  }
  return (*a == '\0' && *b == '\0'); // end of string(both)
}

static uint8_t _epg_genre_find_by_name ( const char *name, const char *lang )
{
  uint8_t a, b;
  const char *s;
  for (a = 1; a < 11; a++) {
    for (b = 0; b < 16; b++) {
      s = _genre_get_name(a, b, lang);
      if (_genre_str_match(name, s))
        return (a << 4) | b;
    }
  }
  return 0; // undefined
}

uint8_t epg_genre_get_eit ( const epg_genre_t *genre )
{
  if (!genre) return 0;
  return genre->code;
}

size_t epg_genre_get_str ( const epg_genre_t *genre, int major_only,
                           int major_prefix, char *buf, size_t len,
                           const char *lang )
{
  const char *s;
  int maj, min;
  size_t ret = 0;
  if (!genre || !buf) return 0;
  maj = (genre->code >> 4) & 0xf;
  s = _genre_get_name(maj, 0, lang);
  if (s[0] == '\0') return 0;
  min = major_only ? 0 : (genre->code & 0xf);
  if (!min || major_prefix ) {
    tvh_strlcatf(buf, len, ret, "%s", s);
  }
  if (min) {
    s = _genre_get_name(maj, min, lang);
    if (s[0]) {
      tvh_strlcatf(buf, len, ret, " : ");
      tvh_strlcatf(buf, len, ret, "%s", s);
    }
  }
  return ret;
}

int epg_genre_list_add ( epg_genre_list_t *list, epg_genre_t *genre )
{
  epg_genre_t *g1, *g2;
  if (!list || !genre || !genre->code) return 0;
  g1 = LIST_FIRST(list);
  if (!g1) {
    g2 = calloc(1, sizeof(epg_genre_t));
    g2->code = genre->code;
    LIST_INSERT_HEAD(list, g2, link);
  } else {
    while (g1) {

      /* Already exists */
      if (g1->code == genre->code) return 0;

      /* Update a major only entry */
      if (g1->code == (genre->code & 0xF0)) {
        g1->code = genre->code;
        break;
      }

      /* Insert before */
      if (g1->code > genre->code) {
        g2 = calloc(1, sizeof(epg_genre_t));
        g2->code = genre->code;
        LIST_INSERT_BEFORE(g1, g2, link);
        break;
      }

      /* Insert after (end) */
      if (!(g2 = LIST_NEXT(g1, link))) {
        g2 = calloc(1, sizeof(epg_genre_t));
        g2->code = genre->code;
        LIST_INSERT_AFTER(g1, g2, link);
        break;
      }

      /* Next */
      g1 = g2;
    }
  }
  return 1;
}

int epg_genre_list_add_by_eit ( epg_genre_list_t *list, uint8_t eit )
{
  epg_genre_t g;
  g.code = eit;
  return epg_genre_list_add(list, &g);
}

int epg_genre_list_add_by_str ( epg_genre_list_t *list, const char *str, const char *lang )
{
  epg_genre_t g;
  g.code = _epg_genre_find_by_name(str, lang);
  return epg_genre_list_add(list, &g);
}

// Note: if partial=1 and genre is a major only category then all minor
// entries will also match
int epg_genre_list_contains
  ( epg_genre_list_t *list, epg_genre_t *genre, int partial )
{
  uint8_t mask = 0xFF;
  epg_genre_t *g;
  if (!list || !genre) return 0;
  if (partial && !(genre->code & 0x0F)) mask = 0xF0;
  LIST_FOREACH(g, list, link) {
    if ((g->code & mask) == genre->code) break;
  }
  return g ? 1 : 0;
}

void epg_genre_list_destroy ( epg_genre_list_t *list )
{
  epg_genre_t *g;
  while ((g = LIST_FIRST(list))) {
    LIST_REMOVE(g, link);
    free(g);
  }
  free(list);
}

htsmsg_t *epg_genres_list_all ( int major_only, int major_prefix, const char *lang )
{
  int i, j;
  htsmsg_t *e, *m;
  const char *s;
  m = htsmsg_create_list();
  for (i = 0; i < 16; i++ ) {
    for (j = 0; j < (major_only ? 1 : 16); j++) {
      if (_epg_genre_names[i][j]) {
        e = htsmsg_create_map();
        htsmsg_add_u32(e, "key", (i << 4) | (major_only ? 0 : j));
        s = _genre_get_name(i, j, lang);
        htsmsg_add_str(e, "val", s);
        // TODO: use major_prefix
        htsmsg_add_msg(m, NULL, e);
      }
    }
  }
  return m;
}

/* **************************************************************************
 * Querying
 * *************************************************************************/

static inline int
_eq_comp_num ( epg_filter_num_t *f, int64_t val )
{
  switch (f->comp) {
    case EC_EQ: return val != f->val1;
    case EC_LT: return val > f->val1;
    case EC_GT: return val < f->val1;
    case EC_RG: return val < f->val1 || val > f->val2;
    default: return 0;
  }
}

static inline int
_eq_comp_str ( epg_filter_str_t *f, const char *str )
{
  if (!str) return 0;
  switch (f->comp) {
    case EC_EQ: return strcmp(str, f->str);
    case EC_LT: return strcmp(str, f->str) > 0;
    case EC_GT: return strcmp(str, f->str) < 0;
    case EC_IN: return strstr(str, f->str) != NULL;
    case EC_RE: return regex_match(&f->re, str) != 0;
    default: return 0;
  }
}

static void
_eq_add ( epg_query_t *eq, epg_broadcast_t *e )
{
  const char *s, *lang = eq->lang;
  int fulltext = eq->stitle && eq->fulltext;
  int mergetext = eq->stitle && eq->mergetext;
  char    *mergedtext = NULL;
  int     mergedtextResult = 0;

  /* Filtering */
  if (e == NULL) return;
  if (e->stop < gclk()) return;
  if (_eq_comp_num(&eq->start, e->start)) return;
  if (_eq_comp_num(&eq->stop, e->stop)) return;
  if (eq->duration.comp != EC_NO) {
    int64_t duration = (int64_t)e->stop - (int64_t)e->start;
    if (_eq_comp_num(&eq->duration, duration)) return;
  }
  if (eq->stars.comp != EC_NO)
    if (_eq_comp_num(&eq->stars, e->star_rating)) return;
  if (eq->age.comp != EC_NO)
    if (_eq_comp_num(&eq->age, e->age_rating)) return;
  if (eq->channel_num.comp != EC_NO)
    if (_eq_comp_num(&eq->channel_num, channel_get_number(e->channel))) return;
  if (eq->channel_name.comp != EC_NO)
    if (_eq_comp_str(&eq->channel_name, channel_get_name(e->channel, NULL))) return;
  if (eq->cat1 && *eq->cat1) {
      /* No category? Can't match our requested category */
      if (!e->category)
          return;
      if (!string_list_contains_string(e->category, eq->cat1))
          return;
  }
  if (eq->cat2 && *eq->cat2) {
      /* No category? Can't match our requested category */
      if (!e->category)
          return;
      if (!string_list_contains_string(e->category, eq->cat2))
          return;
  }
  if (eq->cat3 && *eq->cat3) {
      /* No category? Can't match our requested category */
      if (!e->category)
          return;
      if (!string_list_contains_string(e->category, eq->cat3))
          return;
  }

  if (eq->genre_count) {
    epg_genre_t genre;
    uint32_t i, r = 0;
    for (i = 0; i < eq->genre_count; i++) {
      genre.code = eq->genre[i];
      if (genre.code == 0) continue;
      if (epg_genre_list_contains(&e->genre, &genre, 1)) r++;
    }
    if (!r) return;
  }
  if (eq->new_only) {
    if (!e->is_new)
      return;
  }
  
  //Search EPG text fields concatenated into one huge string.
  if(mergetext)
  {
    mergedtextResult = 0;
    mergedtext = epg_broadcast_get_merged_text(e);
    if(mergedtext)
    {
      mergedtextResult = regex_match(&eq->stitle_re, mergedtext);
      free(mergedtext);
      if(mergedtextResult)
      {
        return;
      }
    }
  }//END mergetext

  //A mergetext search takes priority over a fulltext search.
  if (fulltext && !mergetext) {
    if ((s = epg_broadcast_get_title(e, lang)) == NULL ||
        regex_match(&eq->stitle_re, s)) {
      if ((s = epg_broadcast_get_subtitle(e, lang)) == NULL ||
          regex_match(&eq->stitle_re, s)) {
        if ((s = epg_broadcast_get_summary(e, lang)) == NULL ||
            regex_match(&eq->stitle_re, s)) {
          if ((s = epg_broadcast_get_description(e, lang)) == NULL ||
              regex_match(&eq->stitle_re, s)) {
            if ((s = epg_broadcast_get_credits_cached(e, lang)) == NULL ||
                regex_match(&eq->stitle_re, s)) {
              if ((s = epg_broadcast_get_keyword_cached(e, lang)) == NULL ||
                  regex_match(&eq->stitle_re, s)) {
                return;
              }
            }
          }
        }
      }
    }
  }//END fulltext    

  if (eq->title.comp != EC_NO || (eq->stitle && !(fulltext || mergetext))) {
    if ((s = epg_broadcast_get_title(e, lang)) == NULL) return;
    if (eq->stitle && !(fulltext || mergetext) && regex_match(&eq->stitle_re, s)) return;
    if (eq->title.comp != EC_NO && _eq_comp_str(&eq->title, s)) return;
  }
  if (eq->subtitle.comp != EC_NO) {
    if ((s = epg_broadcast_get_subtitle(e, lang)) == NULL) return;
    if (_eq_comp_str(&eq->subtitle, s)) return;
  }
  if (eq->summary.comp != EC_NO) {
    if ((s = epg_broadcast_get_summary(e, lang)) == NULL) return;
    if (_eq_comp_str(&eq->summary, s)) return;
  }
  if (eq->description.comp != EC_NO) {
    if ((s = epg_broadcast_get_description(e, lang)) == NULL) return;
    if (_eq_comp_str(&eq->description, s)) return;
  }

  /* More space */
  if (eq->entries == eq->allocated) {
    eq->allocated = MAX(100, eq->allocated + 100);
    eq->result    = realloc(eq->result, eq->allocated * sizeof(epg_broadcast_t *));
  }

  /* Store */
  eq->result[eq->entries++] = e;
}

static void
_eq_add_channel ( epg_query_t *eq, channel_t *ch )
{
  epg_broadcast_t *ebc;
  RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link)
    _eq_add(eq, ebc);
}

static int
_eq_init_str( epg_filter_str_t *f )
{
  if (f->comp != EC_RE) return 0;
  return regex_compile(&f->re, f->str, TVHREGEX_CASELESS, LS_EPG);
}

static void
_eq_done_str( epg_filter_str_t *f )
{
  if (f->comp == EC_RE)
    regex_free(&f->re);
  free(f->str);
  f->str = NULL;
}

static int _epg_sort_start_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->start - (*(epg_broadcast_t**)b)->start;
}

static int _epg_sort_start_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->start - (*(epg_broadcast_t**)a)->start;
}

static int _epg_sort_stop_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->stop - (*(epg_broadcast_t**)b)->stop;
}

static int _epg_sort_stop_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->stop - (*(epg_broadcast_t**)a)->stop;
}

static inline int64_t _epg_sort_duration( const epg_broadcast_t *b )
{
  return b->stop - b->start;
}

static int _epg_sort_duration_ascending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_duration(*(epg_broadcast_t**)a) - _epg_sort_duration(*(epg_broadcast_t**)b);
}

static int _epg_sort_duration_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_duration(*(epg_broadcast_t**)b) - _epg_sort_duration(*(epg_broadcast_t**)a);
}

static int _epg_sort_title_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_title(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_title(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  int r = strcmp(s1, s2);
  if (r == 0) {
    s1 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
    s2 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
    if (s1 == NULL && s2 == NULL) return 0;
    if (s1 == NULL && s2) return 1;
    if (s1 && s2 == NULL) return -1;
    r = strcmp(s1, s2);
  }
  return r;
}

static int _epg_sort_title_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_title_ascending(a, b, eq) * -1;
}

static int _epg_sort_subtitle_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_subtitle(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_subtitle_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_subtitle_ascending(a, b, eq) * -1;
}

static int _epg_sort_summary_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_summary(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_summary(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_summary_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_summary_ascending(a, b, eq) * -1;
}

static int _epg_sort_description_ascending ( const void *a, const void *b, void *eq )
{
  const char *s1 = epg_broadcast_get_description(*(epg_broadcast_t**)a, ((epg_query_t *)eq)->lang);
  const char *s2 = epg_broadcast_get_description(*(epg_broadcast_t**)b, ((epg_query_t *)eq)->lang);
  if (s1 == NULL && s2 == NULL) return 0;
  if (s1 == NULL && s2) return 1;
  if (s1 && s2 == NULL) return -1;
  return strcmp(s1, s2);
}

static int _epg_sort_description_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_description_ascending(a, b, eq) * -1;
}

static int _epg_sort_extratext_ascending ( const void *a, const void *b, void *eq )
{
  int r = _epg_sort_subtitle_ascending(a, b, eq);
  if (r == 0) {
    r = _epg_sort_summary_ascending(a, b, eq);
    if (r == 0)
      return _epg_sort_description_ascending(a, b, eq);
  }
  return r;
}

static int _epg_sort_extratext_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_extratext_ascending(a, b, eq) * -1;
}

static int _epg_sort_channel_ascending ( const void *a, const void *b, void *eq )
{
  char *s1 = strdup(channel_get_name((*(epg_broadcast_t**)a)->channel, ""));
  const char *s2 =  channel_get_name((*(epg_broadcast_t**)b)->channel, "");
  int r = strcmp(s1, s2);
  free(s1);
  return r;
}

static int _epg_sort_channel_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_description_ascending(a, b, eq) * -1;
}

static int _epg_sort_channel_num_ascending ( const void *a, const void *b, void *eq )
{
  int64_t v1 = channel_get_number((*(epg_broadcast_t**)a)->channel);
  int64_t v2 = channel_get_number((*(epg_broadcast_t**)b)->channel);
  return (v1 > v2) - (v1 < v2);
}

static int _epg_sort_channel_num_descending ( const void *a, const void *b, void *eq )
{
  const int64_t v1 = channel_get_number((*(epg_broadcast_t**)a)->channel);
  const int64_t v2 = channel_get_number((*(epg_broadcast_t**)b)->channel);
  return (v2 > v1) - (v2 < v1);
}

static int _epg_sort_stars_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->star_rating - (*(epg_broadcast_t**)b)->star_rating;
}

static int _epg_sort_stars_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->star_rating - (*(epg_broadcast_t**)a)->star_rating;
}

static int _epg_sort_age_ascending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)a)->age_rating - (*(epg_broadcast_t**)b)->age_rating;
}

static int _epg_sort_age_descending ( const void *a, const void *b, void *eq )
{
  return (*(epg_broadcast_t**)b)->age_rating - (*(epg_broadcast_t**)a)->age_rating;
}

static uint64_t _epg_sort_genre_hash( epg_broadcast_t *b )
{
  uint64_t h = 0, t;
  epg_genre_t *g;

  LIST_FOREACH(g, &b->genre, link) {
    t = h >> 28;
    h <<= 8;
    h += (uint64_t)g->code + t;
  }
  return h;
}

static int _epg_sort_genre_ascending ( const void *a, const void *b, void *eq )
{
  const uint64_t v1 = _epg_sort_genre_hash(*(epg_broadcast_t**)a);
  const uint64_t v2 = _epg_sort_genre_hash(*(epg_broadcast_t**)b);
  return v1 - v2;
}

static int _epg_sort_genre_descending ( const void *a, const void *b, void *eq )
{
  return _epg_sort_genre_ascending(a, b, eq) * -1;
}

epg_broadcast_t **
epg_query ( epg_query_t *eq, access_t *perm )
{
  channel_t *channel;
  channel_tag_t *tag;
  int (*fcn)(const void *, const void *, void *) = NULL;

  /* Setup exp */
  if (_eq_init_str(&eq->title)) goto fin;
  if (_eq_init_str(&eq->subtitle)) goto fin;
  if (_eq_init_str(&eq->summary)) goto fin;
  if (_eq_init_str(&eq->description)) goto fin;
  if (_eq_init_str(&eq->extratext)) goto fin;
  if (_eq_init_str(&eq->channel_name)) goto fin;

  if (eq->stitle)
    if (regex_compile(&eq->stitle_re, eq->stitle, TVHREGEX_CASELESS, LS_EPG))
      goto fin;

  channel = channel_find_by_uuid(eq->channel) ?:
            channel_find_by_name(eq->channel);

  tag = channel_tag_find_by_uuid(eq->channel_tag) ?:
        channel_tag_find_by_name(eq->channel_tag, 0);

  /* Single channel */
  if (channel && tag == NULL) {
    if (channel_access(channel, perm, 0))
      _eq_add_channel(eq, channel);

  /* Tag based */
  } else if (tag) {
    idnode_list_mapping_t *ilm;
    channel_t *ch2;
    LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
      ch2 = (channel_t *)ilm->ilm_in2;
      if(ch2 == channel || channel == NULL)
        if (channel_access(ch2, perm, 0))
          _eq_add_channel(eq, ch2);
    }

  /* All channels */
  } else {
    CHANNEL_FOREACH(channel)
      if (channel_access(channel, perm, 0))
        _eq_add_channel(eq, channel);
  }

  switch (eq->sort_dir) {
  case ES_ASC:
    switch (eq->sort_key) {
    case ESK_START:       fcn = _epg_sort_start_ascending;        break;
    case ESK_STOP:        fcn = _epg_sort_stop_ascending;         break;
    case ESK_DURATION:    fcn = _epg_sort_duration_ascending;     break;
    case ESK_TITLE:       fcn = _epg_sort_title_ascending;        break;
    case ESK_SUBTITLE:    fcn = _epg_sort_subtitle_ascending;     break;
    case ESK_SUMMARY:     fcn = _epg_sort_summary_ascending;      break;
    case ESK_DESCRIPTION: fcn = _epg_sort_description_ascending;  break;
    case ESK_EXTRATEXT:   fcn = _epg_sort_extratext_ascending;    break;
    case ESK_CHANNEL:     fcn = _epg_sort_channel_ascending;      break;
    case ESK_CHANNEL_NUM: fcn = _epg_sort_channel_num_ascending;  break;
    case ESK_STARS:       fcn = _epg_sort_stars_ascending;        break;
    case ESK_AGE:         fcn = _epg_sort_age_ascending;          break;
    case ESK_GENRE:       fcn = _epg_sort_genre_ascending;        break;
    }
    break;
  case ES_DSC:
    switch (eq->sort_key) {
    case ESK_START:       fcn = _epg_sort_start_descending;       break;
    case ESK_STOP:        fcn = _epg_sort_stop_descending;        break;
    case ESK_DURATION:    fcn = _epg_sort_duration_descending;    break;
    case ESK_TITLE:       fcn = _epg_sort_title_descending;       break;
    case ESK_SUBTITLE:    fcn = _epg_sort_subtitle_descending;    break;
    case ESK_SUMMARY:     fcn = _epg_sort_summary_descending;     break;
    case ESK_DESCRIPTION: fcn = _epg_sort_description_descending; break;
    case ESK_EXTRATEXT:   fcn = _epg_sort_extratext_descending;   break;
    case ESK_CHANNEL:     fcn = _epg_sort_channel_descending;     break;
    case ESK_CHANNEL_NUM: fcn = _epg_sort_channel_num_descending; break;
    case ESK_STARS:       fcn = _epg_sort_stars_descending;       break;
    case ESK_AGE:         fcn = _epg_sort_age_descending;         break;
    case ESK_GENRE:       fcn = _epg_sort_genre_descending;       break;
    }
    break;
  }

  tvh_qsort_r(eq->result, eq->entries, sizeof(epg_broadcast_t *), fcn, eq);

fin:
  _eq_done_str(&eq->title);
  _eq_done_str(&eq->subtitle);
  _eq_done_str(&eq->summary);
  _eq_done_str(&eq->description);
  _eq_done_str(&eq->extratext);
  _eq_done_str(&eq->channel_name);

  if (eq->stitle)
    regex_free(&eq->stitle_re);

  free(eq->lang); eq->lang = NULL;
  free(eq->channel); eq->channel = NULL;
  free(eq->channel_tag); eq->channel_tag = NULL;
  free(eq->stitle); eq->stitle = NULL;
  free(eq->cat1); eq->cat1 = NULL;
  free(eq->cat2); eq->cat2 = NULL;
  free(eq->cat3); eq->cat3 = NULL;

  if (eq->genre != eq->genre_static)
    free(eq->genre);
  eq->genre = NULL;

  return eq->result;
}

void epg_query_free(epg_query_t *eq)
{
  free(eq->result); eq->result = NULL;
}


/* **************************************************************************
 * Miscellaneous
 * *************************************************************************/

htsmsg_t *epg_config_serialize( void )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "last_id", _epg_object_idx);
  return m;
}

int epg_config_deserialize( htsmsg_t *m )
{
  if (htsmsg_get_u32(m, "last_id", &_epg_object_idx))
    return 0;
  return 1; /* ok */
}

void epg_skel_done(void)
{
  epg_broadcast_t **broad;

  broad = _epg_broadcast_skel();
  free(*broad); *broad = NULL;
  assert(RB_FIRST(&epg_serieslinks) == NULL);
  assert(RB_FIRST(&epg_episodelinks) == NULL);
}
