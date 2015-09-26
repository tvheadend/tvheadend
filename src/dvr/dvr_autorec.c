/*
 *  tvheadend, Automatic recordings
 *  Copyright (C) 2010 Andreas Öman
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

#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "tvheadend.h"
#include "settings.h"
#include "dvr.h"
#include "epg.h"
#include "htsp_server.h"

struct dvr_autorec_entry_queue autorec_entries;

/**
 * Unlink - and remove any unstarted
 */
static epg_broadcast_t **
dvr_autorec_purge_spawns(dvr_autorec_entry_t *dae, int del, int disabled)
{
  dvr_entry_t *de;
  epg_broadcast_t **bcast = NULL, **nbcast;
  int i = 0, size = 0;

  while((de = LIST_FIRST(&dae->dae_spawns)) != NULL) {
    LIST_REMOVE(de, de_autorec_link);
    de->de_autorec = NULL;
    if (!del) continue;
    if (de->de_sched_state == DVR_SCHEDULED) {
      if (disabled && !de->de_enabled && de->de_bcast) {
        if (i >= size - 1) {
          nbcast = realloc(bcast, (size + 16) * sizeof(epg_broadcast_t *));
          if (nbcast != NULL) {
            bcast = nbcast;
            size += 16;
            bcast[i++] = de->de_bcast;
          }
        } else {
          bcast[i++] = de->de_bcast;
        }
      }
      dvr_entry_cancel(de);
    } else
      dvr_entry_save(de);
  }
  if (bcast)
    bcast[i] = NULL;
  return bcast;
}

/**
 * return 1 if the event 'e' is matched by the autorec rule 'dae'
 */
static int
autorec_cmp(dvr_autorec_entry_t *dae, epg_broadcast_t *e)
{
  idnode_list_mapping_t *ilm;
  dvr_config_t *cfg;
  double duration;

  if (!e->channel) return 0;
  if (!e->episode) return 0;
  if(dae->dae_enabled == 0 || dae->dae_weekdays == 0)
    return 0;

  if(dae->dae_channel == NULL &&
     dae->dae_channel_tag == NULL &&
     dae->dae_content_type == 0 &&
     (dae->dae_title == NULL ||
     dae->dae_title[0] == '\0') &&
     dae->dae_brand == NULL &&
     dae->dae_season == NULL &&
     dae->dae_minduration <= 0 &&
     (dae->dae_maxduration <= 0 || dae->dae_maxduration > 24 * 3600) &&
     dae->dae_serieslink == NULL)
    return 0; // Avoid super wildcard match

  // Note: we always test season first, though it will only be set
  //       if configured
  if(dae->dae_serieslink) {
    if (!e->serieslink || dae->dae_serieslink != e->serieslink) return 0;
  } else {
    if(dae->dae_season)
      if (!e->episode->season || dae->dae_season != e->episode->season) return 0;
    if(dae->dae_brand)
      if (!e->episode->brand || dae->dae_brand != e->episode->brand) return 0;
  }
  if(dae->dae_title != NULL && dae->dae_title[0] != '\0') {
    lang_str_ele_t *ls;
    if (!dae->dae_fulltext) {
      if(!e->episode->title) return 0;
      RB_FOREACH(ls, e->episode->title, link)
        if (!regexec(&dae->dae_title_preg, ls->str, 0, NULL, 0)) break;
    } else {
      ls = NULL;
      if (e->episode->title)
        RB_FOREACH(ls, e->episode->title, link)
          if (!regexec(&dae->dae_title_preg, ls->str, 0, NULL, 0)) break;
      if (!ls && e->episode->subtitle)
        RB_FOREACH(ls, e->episode->subtitle, link)
          if (!regexec(&dae->dae_title_preg, ls->str, 0, NULL, 0)) break;
      if (!ls && e->summary)
        RB_FOREACH(ls, e->summary, link)
          if (!regexec(&dae->dae_title_preg, ls->str, 0, NULL, 0)) break;
      if (!ls && e->description)
        RB_FOREACH(ls, e->description, link)
          if (!regexec(&dae->dae_title_preg, ls->str, 0, NULL, 0)) break;
    }
    if (!ls) return 0;
  }

  // Note: ignore channel test if we allow quality unlocking 
  if ((cfg = dae->dae_config) == NULL)
    return 0;
  if(dae->dae_channel != NULL) {
    if (dae->dae_channel != e->channel &&
        dae->dae_channel->ch_enabled)
      return 0;
    if (!dae->dae_channel->ch_enabled)
      return 0;
  }

  if(dae->dae_channel_tag != NULL) {
    LIST_FOREACH(ilm, &dae->dae_channel_tag->ct_ctms, ilm_in1_link)
      if((channel_t *)ilm->ilm_in2 == e->channel)
	break;
    if(ilm == NULL)
      return 0;
  }

  if(dae->dae_content_type != 0) {
    epg_genre_t ct;
    memset(&ct, 0, sizeof(ct));
    ct.code = dae->dae_content_type;
    if (!epg_genre_list_contains(&e->episode->genre, &ct, 1))
      return 0;
  }

  if(dae->dae_start >= 0 && dae->dae_start_window >= 0 &&
     dae->dae_start < 24*60 && dae->dae_start_window < 24*60) {
    struct tm a_time, ev_time;
    time_t ta, te, tad;
    localtime_r(&e->start, &a_time);
    ev_time = a_time;
    a_time.tm_min = dae->dae_start % 60;
    a_time.tm_hour = dae->dae_start / 60;
    ta = mktime(&a_time);
    te = mktime(&ev_time);
    if(dae->dae_start > dae->dae_start_window) {
      ta -= 24 * 3600; /* 24 hours */
      tad = ((24 * 60) - dae->dae_start + dae->dae_start_window) * 60;
      if(ta > te || te > ta + tad) {
        ta += 24 * 3600;
        if(ta > te || te > ta + tad)
          return 0;
      }
    } else {
      tad = (dae->dae_start_window - dae->dae_start) * 60;
      if(ta > te || te > ta + tad)
        return 0;
    }
  }

  duration = difftime(e->stop,e->start);

  if(dae->dae_minduration > 0) {
    if(duration < dae->dae_minduration) return 0;
  }

  if(dae->dae_maxduration > 0) {
    if(duration > dae->dae_maxduration) return 0;
  }

  if(dae->dae_weekdays != 0x7f) {
    struct tm tm;
    localtime_r(&e->start, &tm);
    if(!((1 << ((tm.tm_wday ?: 7) - 1)) & dae->dae_weekdays))
      return 0;
  }
  return 1;
}

/**
 *
 */
dvr_autorec_entry_t *
dvr_autorec_create(const char *uuid, htsmsg_t *conf)
{
  dvr_autorec_entry_t *dae;

  dae = calloc(1, sizeof(*dae));

  if (idnode_insert(&dae->dae_id, uuid, &dvr_autorec_entry_class, 0)) {
    if (uuid)
      tvhwarn("dvr", "invalid autorec entry uuid '%s'", uuid);
    free(dae);
    return NULL;
  }

  dae->dae_weekdays = 0x7f;
  dae->dae_pri = DVR_PRIO_NORMAL;
  dae->dae_start = -1;
  dae->dae_start_window = -1;
  dae->dae_config = dvr_config_find_by_name_default(NULL);
  LIST_INSERT_HEAD(&dae->dae_config->dvr_autorec_entries, dae, dae_config_link);

  TAILQ_INSERT_TAIL(&autorec_entries, dae, dae_link);

  idnode_load(&dae->dae_id, conf);

  htsp_autorec_entry_add(dae);

  return dae;
}


dvr_autorec_entry_t*
dvr_autorec_create_htsp(const char *dvr_config_name, const char *title, int fulltext,
                            channel_t *ch, uint32_t enabled, int32_t start, int32_t start_window,
                            uint32_t weekdays, time_t start_extra, time_t stop_extra,
                            dvr_prio_t pri, int retention,
                            int min_duration, int max_duration, dvr_autorec_dedup_t dup_detect,
                            const char *owner, const char *creator, const char *comment, 
                            const char *name, const char *directory)
{
  dvr_autorec_entry_t *dae;
  htsmsg_t *conf, *days;

  conf = htsmsg_create_map();
  days = htsmsg_create_list();

  htsmsg_add_u32(conf, "enabled",     enabled > 0 ? 1 : 0);
  htsmsg_add_u32(conf, "retention",   retention);
  htsmsg_add_u32(conf, "pri",         pri);
  htsmsg_add_u32(conf, "minduration", min_duration);
  htsmsg_add_u32(conf, "maxduration", max_duration);
  htsmsg_add_s64(conf, "start_extra", start_extra);
  htsmsg_add_s64(conf, "stop_extra",  stop_extra);
  htsmsg_add_str(conf, "title",       title);
  htsmsg_add_u32(conf, "fulltext",    fulltext);
  htsmsg_add_u32(conf, "record",      dup_detect);
  htsmsg_add_str(conf, "config_name", dvr_config_name ?: "");
  htsmsg_add_str(conf, "owner",       owner ?: "");
  htsmsg_add_str(conf, "creator",     creator ?: "");
  htsmsg_add_str(conf, "comment",     comment ?: "");
  htsmsg_add_str(conf, "name",        name ?: "");
  htsmsg_add_str(conf, "directory",   directory ?: "");

  if (start >= 0)
    htsmsg_add_s32(conf, "start", start);
  if (start_window >= 0)
    htsmsg_add_s32(conf, "start_window", start_window);
  if (ch)
    htsmsg_add_str(conf, "channel", idnode_uuid_as_sstr(&ch->ch_id));

  int i;
  for (i = 0; i < 7; i++)
    if (weekdays & (1 << i))
      htsmsg_add_u32(days, NULL, i + 1);

  htsmsg_add_msg(conf, "weekdays", days);

  dae = dvr_autorec_create(NULL, conf);
  htsmsg_destroy(conf);

  if (dae) {
    dvr_autorec_save(dae);
    dvr_autorec_changed(dae, 1);
  }

  return dae;
}

/**
 *
 */
dvr_autorec_entry_t *
dvr_autorec_add_series_link(const char *dvr_config_name,
                            epg_broadcast_t *event,
                            const char *owner, const char *creator,
                            const char *comment)
{
  dvr_autorec_entry_t *dae;
  htsmsg_t *conf;
  char *title;
  if (!event || !event->episode)
    return NULL;
  conf = htsmsg_create_map();
  title = regexp_escape(epg_broadcast_get_title(event, NULL));
  htsmsg_add_u32(conf, "enabled", 1);
  htsmsg_add_str(conf, "title", title);
  free(title);
  htsmsg_add_str(conf, "config_name", dvr_config_name ?: "");
  htsmsg_add_str(conf, "channel", channel_get_name(event->channel));
  if (event->serieslink)
    htsmsg_add_str(conf, "serieslink", event->serieslink->uri);
  htsmsg_add_str(conf, "owner", owner ?: "");
  htsmsg_add_str(conf, "creator", creator ?: "");
  htsmsg_add_str(conf, "comment", comment ?: "");
  dae = dvr_autorec_create(NULL, conf);
  htsmsg_destroy(conf);
  return dae;
}

/**
 *
 */
static void
autorec_entry_destroy(dvr_autorec_entry_t *dae, int delconf)
{
  dvr_autorec_purge_spawns(dae, delconf, 0);

  if (delconf)
    hts_settings_remove("dvr/autorec/%s", idnode_uuid_as_sstr(&dae->dae_id));

  htsp_autorec_entry_delete(dae);

  TAILQ_REMOVE(&autorec_entries, dae, dae_link);
  idnode_unlink(&dae->dae_id);

  if(dae->dae_config)
    LIST_REMOVE(dae, dae_config_link);

  free(dae->dae_name);
  free(dae->dae_directory);
  free(dae->dae_owner);
  free(dae->dae_creator);
  free(dae->dae_comment);

  if(dae->dae_title != NULL) {
    free(dae->dae_title);
    regfree(&dae->dae_title_preg);
  }

  if(dae->dae_channel != NULL)
    LIST_REMOVE(dae, dae_channel_link);

  if(dae->dae_channel_tag != NULL)
    LIST_REMOVE(dae, dae_channel_tag_link);

  if(dae->dae_brand)
    dae->dae_brand->putref(dae->dae_brand);
  if(dae->dae_season)
    dae->dae_season->putref(dae->dae_season);
  if(dae->dae_serieslink)
    dae->dae_serieslink->putref(dae->dae_serieslink);

  free(dae);
}

/**
 *
 */
void
dvr_autorec_save(dvr_autorec_entry_t *dae)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  idnode_save(&dae->dae_id, m);
  hts_settings_save(m, "dvr/autorec/%s", idnode_uuid_as_sstr(&dae->dae_id));
  htsmsg_destroy(m);
}

/* **************************************************************************
 * DVR Autorec Entry Class definition
 * **************************************************************************/

static void
dvr_autorec_entry_class_save(idnode_t *self)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;
  dvr_autorec_save(dae);
  dvr_autorec_changed(dae, 1);
}

static void
dvr_autorec_entry_class_delete(idnode_t *self)
{
  autorec_entry_destroy((dvr_autorec_entry_t *)self, 1);
}

static int
dvr_autorec_entry_class_perm(idnode_t *self, access_t *a, htsmsg_t *msg_to_write)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;

  if (access_verify2(a, ACCESS_OR|ACCESS_ADMIN|ACCESS_RECORDER))
    return -1;
  if (!access_verify2(a, ACCESS_ADMIN))
    return 0;
  if (dvr_autorec_entry_verify(dae, a, msg_to_write == NULL ? 1 : 0))
    return -1;
  return 0;
}

static const char *
dvr_autorec_entry_class_get_title (idnode_t *self, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;
  const char *s = "";
  if (dae->dae_name && dae->dae_name[0] != '\0')
    s = dae->dae_name;
  else if (dae->dae_comment && dae->dae_comment[0] != '\0')
    s = dae->dae_comment;
  return s;
}

static int
dvr_autorec_entry_class_channel_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  channel_t *ch = v ? channel_find_by_uuid(v) : NULL;
  if (ch == NULL) ch = v ? channel_find_by_name(v) : NULL;
  if (ch == NULL) {
    if (dae->dae_channel) {
      LIST_REMOVE(dae, dae_channel_link);
      dae->dae_channel = NULL;
      return 1;
    }
  } else if (dae->dae_channel != ch) {
    if (dae->dae_channel)
      LIST_REMOVE(dae, dae_channel_link);
    dae->dae_channel = ch;
    LIST_INSERT_HEAD(&ch->ch_autorecs, dae, dae_channel_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_channel_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel)
    ret = idnode_uuid_as_sstr(&dae->dae_channel->ch_id);
  else
    ret = "";
  return &ret;
}

static char *
dvr_autorec_entry_class_channel_rend(void *o, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel)
    return strdup(channel_get_name(dae->dae_channel));
  return NULL;
}

static int
dvr_autorec_entry_class_title_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  const char *title = v ?: "";
  if (strcmp(title, dae->dae_title ?: "")) {
    if (dae->dae_title) {
       regfree(&dae->dae_title_preg);
       free(dae->dae_title);
       dae->dae_title = NULL;
    }
    if (title[0] != '\0' &&
        !regcomp(&dae->dae_title_preg, title,
                 REG_ICASE | REG_EXTENDED | REG_NOSUB))
      dae->dae_title = strdup(title);
    return 1;
  }
  return 0;
}

static int
dvr_autorec_entry_class_tag_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  channel_tag_t *tag = v ? channel_tag_find_by_uuid(v) : NULL;
  if (tag == NULL) tag = v ? channel_tag_find_by_name(v, 0) : NULL;
  if (tag == NULL && dae->dae_channel_tag) {
    LIST_REMOVE(dae, dae_channel_tag_link);
    dae->dae_channel_tag = NULL;
    return 1;
  } else if (dae->dae_channel_tag != tag) {
    if (dae->dae_channel_tag)
      LIST_REMOVE(dae, dae_channel_tag_link);
    dae->dae_channel_tag = tag;
    LIST_INSERT_HEAD(&tag->ct_autorecs, dae, dae_channel_tag_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_tag_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel_tag)
    ret = idnode_uuid_as_sstr(&dae->dae_channel_tag->ct_id);
  else
    ret = "";
  return &ret;
}

static char *
dvr_autorec_entry_class_tag_rend(void *o, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel_tag)
    return strdup(dae->dae_channel_tag->ct_name);
  return NULL;
}

static int
dvr_autorec_entry_class_time_set(void *o, const void *v, int *tm)
{
  const char *s = v;
  int t;

  if(s == NULL || s[0] == '\0' || !isdigit(s[0]))
    t = -1;
  else if(strchr(s, ':') != NULL)
    // formatted time string - convert
    t = (atoi(s) * 60) + atoi(s + 3);
  else {
    t = atoi(s);
  }
  if (t >= 24 * 60)
    t = -1;
  if (t != *tm) {
    *tm = t;
    return 1;
  }
  return 0;
}

static int
dvr_autorec_entry_class_start_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_time_set(o, v, &dae->dae_start);
}

static int
dvr_autorec_entry_class_start_window_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_time_set(o, v, &dae->dae_start_window);
}

static const void *
dvr_autorec_entry_class_time_get(void *o, int tm)
{
  static const char *ret;
  static char buf[16];
  if (tm >= 0)
    snprintf(buf, sizeof(buf), "%02d:%02d", tm / 60, tm % 60);
  else
    strncpy(buf, N_("Any"), 16);
  ret = buf;
  return &ret;
}

static const void *
dvr_autorec_entry_class_start_get(void *o)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_time_get(o, dae->dae_start);
}

static const void *
dvr_autorec_entry_class_start_window_get(void *o)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_time_get(o, dae->dae_start_window);
}

htsmsg_t *
dvr_autorec_entry_class_time_list(void *o, const char *null)
{
  int i;
  htsmsg_t *l = htsmsg_create_list();
  char buf[16];
  htsmsg_add_str(l, NULL, null);
  for (i = 0; i < 24*60;  i += 10) {
    snprintf(buf, sizeof(buf), "%02d:%02d", i / 60, (i % 60));
    htsmsg_add_str(l, NULL, buf);
  }
  return l;
}

static htsmsg_t *
dvr_autorec_entry_class_time_list_(void *o, const char *lang)
{
  const char *msg = N_("Any");
  return dvr_autorec_entry_class_time_list(o, tvh_gettext_lang(lang, msg));
}

static htsmsg_t *
dvr_autorec_entry_class_extra_list(void *o, const char *lang)
{
  const char *msg = N_("Not set (use channel or DVR configuration)");
  return dvr_entry_class_duration_list(o, tvh_gettext_lang(lang, msg), 4*60, 1, lang);
}

static htsmsg_t *
dvr_autorec_entry_class_minduration_list(void *o, const char *lang)
{
  const char *msg = N_("Any");
  return dvr_entry_class_duration_list(o, tvh_gettext_lang(lang, msg), 24*60, 60, lang);
}

static htsmsg_t *
dvr_autorec_entry_class_maxduration_list(void *o, const char *lang)
{
  const char *msg = N_("Any");
  return dvr_entry_class_duration_list(o, tvh_gettext_lang(lang, msg), 24*60, 60, lang);
}

static int
dvr_autorec_entry_class_config_name_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  dvr_config_t *cfg = v ? dvr_config_find_by_uuid(v) : NULL;
  if (cfg == NULL) cfg = v ? dvr_config_find_by_name_default(v): NULL;
  if (cfg == NULL && dae->dae_config) {
    dae->dae_config = NULL;
    LIST_REMOVE(dae, dae_config_link);
    return 1;
  } else if (cfg != dae->dae_config) {
    if (dae->dae_config)
      LIST_REMOVE(dae, dae_config_link);
    LIST_INSERT_HEAD(&cfg->dvr_autorec_entries, dae, dae_config_link);
    dae->dae_config = cfg;
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_config_name_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_config)
    ret = idnode_uuid_as_sstr(&dae->dae_config->dvr_id);
  else
    ret = "";
  return &ret;
}

static char *
dvr_autorec_entry_class_config_name_rend(void *o, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_config)
    return strdup(dae->dae_config->dvr_config_name);
  return NULL;
}

static int
dvr_autorec_entry_class_weekdays_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  htsmsg_field_t *f;
  uint32_t u32, bits = 0;

  HTSMSG_FOREACH(f, (htsmsg_t *)v)
    if (!htsmsg_field_get_u32(f, &u32) && u32 > 0 && u32 < 8)
      bits |= (1 << (u32 - 1));

  if (bits != dae->dae_weekdays) {
    dae->dae_weekdays = bits;
    return 1;
  }
  return 0;
}

htsmsg_t *
dvr_autorec_entry_class_weekdays_get(uint32_t weekdays)
{
  htsmsg_t *m = htsmsg_create_list();
  int i;
  for (i = 0; i < 7; i++)
    if (weekdays & (1 << i))
      htsmsg_add_u32(m, NULL, i + 1);
  return m;
}

static htsmsg_t *
dvr_autorec_entry_class_weekdays_default(void)
{
  return dvr_autorec_entry_class_weekdays_get(0x7f);
}

static const void *
dvr_autorec_entry_class_weekdays_get_(void *o)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_weekdays_get(dae->dae_weekdays);
}

static const struct strtab dvr_autorec_entry_class_weekdays_tab[] = {
  { N_("Mon"), 1 },
  { N_("Tue"), 2 },
  { N_("Wed"), 3 },
  { N_("Thu"), 4 },
  { N_("Fri"), 5 },
  { N_("Sat"), 6 },
  { N_("Sun"), 7 },
};

htsmsg_t *
dvr_autorec_entry_class_weekdays_list ( void *o, const char *lang )
{
  return strtab2htsmsg(dvr_autorec_entry_class_weekdays_tab, 1, lang);
}

char *
dvr_autorec_entry_class_weekdays_rend(uint32_t weekdays, const char *lang)
{
  char buf[32];
  size_t l;
  int i;
  if (weekdays == 0x7f)
    strcpy(buf + 1, tvh_gettext_lang(lang, N_("Every day")));
  else if (weekdays == 0)
    strcpy(buf + 1, tvh_gettext_lang(lang, N_("No days")));
  else {
    buf[0] = '\0';
    for (i = 0; i < 7; i++)
      if (weekdays & (1 << i)) {
        l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, ",%s",
                 tvh_gettext_lang(lang, val2str(i + 1, dvr_autorec_entry_class_weekdays_tab)));
      }
  }
  return strdup(buf + 1);
}

static char *
dvr_autorec_entry_class_weekdays_rend_(void *o, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  return dvr_autorec_entry_class_weekdays_rend(dae->dae_weekdays, lang);
}

static int
dvr_autorec_entry_class_brand_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  int save;
  epg_brand_t *brand;

  if (v && *(char *)v == '\0')
    v = NULL;
  brand = v ? epg_brand_find_by_uri(v, 1, &save) : NULL;
  if (brand && dae->dae_brand != brand) {
    if (dae->dae_brand)
      dae->dae_brand->putref((epg_object_t*)dae->dae_brand);
    brand->getref((epg_object_t*)brand);
    dae->dae_brand = brand;
    return 1;
  } else if (brand == NULL && dae->dae_brand) {
    dae->dae_brand->putref((epg_object_t*)dae->dae_brand);
    dae->dae_brand = NULL;
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_brand_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_brand)
    ret = dae->dae_brand->uri;
  else
    ret = "";
  return &ret;
}

static int
dvr_autorec_entry_class_season_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  int save;
  epg_season_t *season;

  if (v && *(char *)v == '\0')
    v = NULL;
  season = v ? epg_season_find_by_uri(v, 1, &save) : NULL;
  if (season && dae->dae_season != season) {
    if (dae->dae_season)
      dae->dae_season->putref((epg_object_t*)dae->dae_season);
    season->getref((epg_object_t*)season);
    dae->dae_season = season;
    return 1;
  } else if (season == NULL && dae->dae_season) {
    dae->dae_season->putref((epg_object_t*)dae->dae_season);
    dae->dae_season = NULL;
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_season_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_season)
    ret = dae->dae_season->uri;
  else
    ret = "";
  return &ret;
}

static int
dvr_autorec_entry_class_series_link_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  int save;
  epg_serieslink_t *sl;

  if (v && *(char *)v == '\0')
    v = NULL;
  sl = v ? epg_serieslink_find_by_uri(v, 1, &save) : NULL;
  if (sl && dae->dae_serieslink != sl) {
    if (dae->dae_serieslink)
      dae->dae_serieslink->putref((epg_object_t*)dae->dae_season);
    sl->getref((epg_object_t*)sl);
    dae->dae_serieslink = sl;
    return 1;
  } else if (sl == NULL && dae->dae_serieslink) {
    dae->dae_season->putref((epg_object_t*)dae->dae_season);
    dae->dae_season = NULL;
    return 1;
  }
  return 0;
}

static const void *
dvr_autorec_entry_class_series_link_get(void *o)
{
  static const char *ret;
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_serieslink)
    ret = dae->dae_serieslink->uri;
  else
    ret = "";
  return &ret;
}

static htsmsg_t *
dvr_autorec_entry_class_content_type_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "epg/content_type/list");
  return m;
}

static htsmsg_t *
dvr_autorec_entry_class_dedup_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Record all"),
        DVR_AUTOREC_RECORD_ALL },
    { N_("Record if different episode number"),
        DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER },
    { N_("Record if different subtitle"),
        DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE },
    { N_("Record if different description"),
        DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION },
    { N_("Record once per week"),
        DVR_AUTOREC_RECORD_ONCE_PER_WEEK },
    { N_("Record once per day"),
        DVR_AUTOREC_RECORD_ONCE_PER_DAY },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static uint32_t
dvr_autorec_entry_class_owner_opts(void *o)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae && dae->dae_id.in_access &&
      !access_verify2(dae->dae_id.in_access, ACCESS_ADMIN))
    return 0;
  return PO_RDONLY;
}

const idclass_t dvr_autorec_entry_class = {
  .ic_class      = "dvrautorec",
  .ic_caption    = N_("DVR Auto-Record Entry"),
  .ic_event      = "dvrautorec",
  .ic_save       = dvr_autorec_entry_class_save,
  .ic_get_title  = dvr_autorec_entry_class_get_title,
  .ic_delete     = dvr_autorec_entry_class_delete,
  .ic_perm       = dvr_autorec_entry_class_perm,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(dvr_autorec_entry_t, dae_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .off      = offsetof(dvr_autorec_entry_t, dae_name),
    },
	{
      .type     = PT_STR,
      .id       = "directory",
      .name     = N_("Directory"),
      .off      = offsetof(dvr_autorec_entry_t, dae_directory),
    },
    {
      .type     = PT_STR,
      .id       = "title",
      .name     = N_("Title (Regexp)"),
      .set      = dvr_autorec_entry_class_title_set,
      .off      = offsetof(dvr_autorec_entry_t, dae_title),
    },
    {
      .type     = PT_BOOL,
      .id       = "fulltext",
      .name     = N_("Full-text"),
      .off      = offsetof(dvr_autorec_entry_t, dae_fulltext),
    },
    {
      .type     = PT_STR,
      .id       = "channel",
      .name     = N_("Channel"),
      .set      = dvr_autorec_entry_class_channel_set,
      .get      = dvr_autorec_entry_class_channel_get,
      .rend     = dvr_autorec_entry_class_channel_rend,
      .list     = channel_class_get_list,
    },
    {
      .type     = PT_STR,
      .id       = "tag",
      .name     = N_("Channel Tag"),
      .set      = dvr_autorec_entry_class_tag_set,
      .get      = dvr_autorec_entry_class_tag_get,
      .rend     = dvr_autorec_entry_class_tag_rend,
      .list     = channel_tag_class_get_list,
    },
    {
      .type     = PT_STR,
      .id       = "start",
      .name     = N_("Start After"),
      .set      = dvr_autorec_entry_class_start_set,
      .get      = dvr_autorec_entry_class_start_get,
      .list     = dvr_autorec_entry_class_time_list_,
      .opts     = PO_SORTKEY
    },
    {
      .type     = PT_STR,
      .id       = "start_window",
      .name     = N_("Start Before"),
      .set      = dvr_autorec_entry_class_start_window_set,
      .get      = dvr_autorec_entry_class_start_window_get,
      .list     = dvr_autorec_entry_class_time_list_,
      .opts     = PO_SORTKEY,
    },
    {
      .type     = PT_TIME,
      .id       = "start_extra",
      .name     = N_("Extra Start Time"),
      .off      = offsetof(dvr_autorec_entry_t, dae_start_extra),
      .list     = dvr_autorec_entry_class_extra_list,
      .opts     = PO_DURATION | PO_SORTKEY
    },
    {
      .type     = PT_TIME,
      .id       = "stop_extra",
      .name     = N_("Extra Stop Time"),
      .off      = offsetof(dvr_autorec_entry_t, dae_stop_extra),
      .list     = dvr_autorec_entry_class_extra_list,
      .opts     = PO_DURATION | PO_SORTKEY
    },
    {
      .type     = PT_U32,
      .islist   = 1,
      .id       = "weekdays",
      .name     = N_("Days of Week"),
      .set      = dvr_autorec_entry_class_weekdays_set,
      .get      = dvr_autorec_entry_class_weekdays_get_,
      .list     = dvr_autorec_entry_class_weekdays_list,
      .rend     = dvr_autorec_entry_class_weekdays_rend_,
      .def.list = dvr_autorec_entry_class_weekdays_default
    },
    {
      .type     = PT_INT,
      .id       = "minduration",
      .name     = N_("Minimum Duration"),
      .list     = dvr_autorec_entry_class_minduration_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_minduration),
    },
    {
      .type     = PT_INT,
      .id       = "maxduration",
      .name     = N_("Maximum Duration"),
      .list     = dvr_autorec_entry_class_maxduration_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_maxduration),
    },
    {
      .type     = PT_U32,
      .id       = "content_type",
      .name     = N_("Content Type"),
      .list     = dvr_autorec_entry_class_content_type_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_content_type),
    },
    {
      .type     = PT_U32,
      .id       = "pri",
      .name     = N_("Priority"),
      .list     = dvr_entry_class_pri_list,
      .def.i    = DVR_PRIO_NORMAL,
      .off      = offsetof(dvr_autorec_entry_t, dae_pri),
    },
    {
      .type     = PT_U32,
      .id       = "record",
      .name     = N_("Duplicate Handling"),
      .def.i    = DVR_AUTOREC_RECORD_ALL,
      .off      = offsetof(dvr_autorec_entry_t, dae_record),
      .list     = dvr_autorec_entry_class_dedup_list,
    },
    {
      .type     = PT_INT,
      .id       = "retention",
      .name     = N_("Retention"),
      .off      = offsetof(dvr_autorec_entry_t, dae_retention),
    },
    {
      .type     = PT_STR,
      .id       = "config_name",
      .name     = N_("DVR Configuration"),
      .set      = dvr_autorec_entry_class_config_name_set,
      .get      = dvr_autorec_entry_class_config_name_get,
      .rend     = dvr_autorec_entry_class_config_name_rend,
      .list     = dvr_entry_class_config_name_list,
    },
    {
      .type     = PT_STR,
      .id       = "brand",
      .name     = N_("Brand"),
      .set      = dvr_autorec_entry_class_brand_set,
      .get      = dvr_autorec_entry_class_brand_get,
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "season",
      .name     = N_("Season"),
      .set      = dvr_autorec_entry_class_season_set,
      .get      = dvr_autorec_entry_class_season_get,
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "serieslink",
      .name     = N_("Series Link"),
      .set      = dvr_autorec_entry_class_series_link_set,
      .get      = dvr_autorec_entry_class_series_link_get,
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .id       = "owner",
      .name     = N_("Owner"),
      .off      = offsetof(dvr_autorec_entry_t, dae_owner),
      .get_opts = dvr_autorec_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "creator",
      .name     = N_("Creator"),
      .off      = offsetof(dvr_autorec_entry_t, dae_creator),
      .get_opts = dvr_autorec_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .off      = offsetof(dvr_autorec_entry_t, dae_comment),
    },
    {}
  }
};

/**
 *
 */
void
dvr_autorec_init(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  TAILQ_INIT(&autorec_entries);
  if((l = hts_settings_load("dvr/autorec")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
        continue;
      (void)dvr_autorec_create(f->hmf_name, c);
    }
    htsmsg_destroy(l);
  }
}

void
dvr_autorec_done(void)
{
  dvr_autorec_entry_t *dae;

  pthread_mutex_lock(&global_lock);
  while ((dae = TAILQ_FIRST(&autorec_entries)) != NULL)
    autorec_entry_destroy(dae, 0);
  pthread_mutex_unlock(&global_lock);
}

void
dvr_autorec_update(void)
{
  dvr_autorec_entry_t *dae;
  TAILQ_FOREACH(dae, &autorec_entries, dae_link) {
    dvr_autorec_changed(dae, 0);
  }
}

/**
 *
 */
void
dvr_autorec_check_event(epg_broadcast_t *e)
{
  dvr_autorec_entry_t *dae;

  if (e->channel && !e->channel->ch_enabled)
    return;
  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    if(autorec_cmp(dae, e))
      dvr_entry_create_by_autorec(1, e, dae);
  // Note: no longer updating event here as it will be done from EPG
  //       anyway
}

void dvr_autorec_check_brand(epg_brand_t *b)
{
// Note: for the most part this will only be relevant should an episode
//       to which a broadcast is linked suddenly get added to a new brand
//       this is pretty damn unlikely!
}

void dvr_autorec_check_season(epg_season_t *s)
{
// Note: I guess new episodes might have been added, but again its likely
//       this will already have been picked up by the check_event call
}

void dvr_autorec_check_serieslink(epg_serieslink_t *s)
{
// TODO: need to implement this
}

/**
 *
 */
void
dvr_autorec_changed(dvr_autorec_entry_t *dae, int purge)
{
  channel_t *ch;
  epg_broadcast_t *e, **disabled = NULL, **p;
  int enabled;

  if (purge)
    disabled = dvr_autorec_purge_spawns(dae, 1, 1);

  CHANNEL_FOREACH(ch) {
    if (!ch->ch_enabled) continue;
    RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
      if(autorec_cmp(dae, e)) {
        enabled = 1;
        if (disabled) {
          for (p = disabled; *p && *p != e; p++);
          enabled = *p != NULL;
        }
        dvr_entry_create_by_autorec(enabled, e, dae);
      }
    }
  }

  free(disabled);

  htsp_autorec_entry_update(dae);
}


/**
 *
 */
void
autorec_destroy_by_channel(channel_t *ch, int delconf)
{
  dvr_autorec_entry_t *dae;

  while((dae = LIST_FIRST(&ch->ch_autorecs)) != NULL)
    autorec_entry_destroy(dae, delconf);
}

/*
 *
 */
void
autorec_destroy_by_channel_tag(channel_tag_t *ct, int delconf)
{
  dvr_autorec_entry_t *dae;

  while((dae = LIST_FIRST(&ct->ct_autorecs)) != NULL) {
    LIST_REMOVE(dae, dae_channel_tag_link);
    dae->dae_channel_tag = NULL;
    idnode_notify_changed(&dae->dae_id);
    if (delconf)
      dvr_autorec_save(dae);
  }
}

/*
 *
 */
void
autorec_destroy_by_id(const char *id, int delconf)
{
  dvr_autorec_entry_t *dae;
  dae = dvr_autorec_find_by_uuid(id);

  if (dae)
    autorec_entry_destroy(dae, delconf);
}

/**
 *
 */
void
autorec_destroy_by_config(dvr_config_t *kcfg, int delconf)
{
  dvr_autorec_entry_t *dae;
  dvr_config_t *cfg = NULL;

  while((dae = LIST_FIRST(&kcfg->dvr_autorec_entries)) != NULL) {
    LIST_REMOVE(dae, dae_config_link);
    if (cfg == NULL && delconf)
      cfg = dvr_config_find_by_name_default(NULL);
    if (cfg)
      LIST_INSERT_HEAD(&cfg->dvr_autorec_entries, dae, dae_config_link);
    dae->dae_config = cfg;
    if (delconf)
      dvr_autorec_save(dae);
  }
}

static inline int extra_valid(time_t extra)
{
  return extra != 0 && extra != (time_t)-1;
}

/**
 *
 */
int
dvr_autorec_get_extra_time_pre( dvr_autorec_entry_t *dae )
{
  time_t extra = dae->dae_start_extra;

  if (!extra_valid(extra)) {
    if (dae->dae_channel)
      extra = dae->dae_channel->ch_dvr_extra_time_pre;
    if (!extra_valid(extra))
      extra = dae->dae_config->dvr_extra_time_pre;
  }
  return extra;
}

/**
 *
 */
int
dvr_autorec_get_extra_time_post( dvr_autorec_entry_t *dae )
{
  time_t extra = dae->dae_stop_extra;

  if (!extra_valid(extra)) {
    if (dae->dae_channel)
      extra = dae->dae_channel->ch_dvr_extra_time_post;
    if (!extra_valid(extra))
      extra = dae->dae_config->dvr_extra_time_post;
  }
  return extra;
}

/**
 *
 */
int
dvr_autorec_get_retention( dvr_autorec_entry_t *dae )
{
  if (dae->dae_retention > 0)
    return dae->dae_retention;
  return dae->dae_config->dvr_retention_days;
}
