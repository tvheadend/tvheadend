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

#include <ctype.h>

#include "tvheadend.h"
#include "settings.h"
#include "string_list.h"
#include "dvr.h"
#include "epg.h"
#include "htsp_server.h"

struct dvr_autorec_entry_queue autorec_entries;

static void autorec_regfree(dvr_autorec_entry_t *dae)
{
  if (dae->dae_title) {
    regex_free(&dae->dae_title_regex);
    free(dae->dae_title);
    dae->dae_title = NULL;
  }
}

/*
 *
 */
static uint32_t dvr_autorec_get_max_count(dvr_autorec_entry_t *dae)
{
  uint32_t max_count = dae->dae_max_count;
  if (max_count == 0)
    max_count = dae->dae_config ? dae->dae_config->dvr_autorec_max_count : 0;
  return max_count;
}

/*
 *
 */
uint32_t dvr_autorec_get_max_sched_count(dvr_autorec_entry_t *dae)
{
  uint32_t max_count = dae->dae_max_sched_count;
  if (max_count == 0)
    max_count = dae->dae_config ? dae->dae_config->dvr_autorec_max_sched_count : 0;
  return max_count;
}

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
      dvr_entry_cancel(de, 0);
    } else
      idnode_changed(&de->de_id);
  }
  if (bcast)
    bcast[i] = NULL;
  return bcast;
}


/**
 * If autorec entry no longer matches autorec rule then it can be purged.
 * @return 1 if it can be purged
 */
int
dvr_autorec_entry_can_be_purged(const dvr_entry_t *de)
{
  /* Not an autorec so ignore */
  if (!de->de_autorec)
    return 0;

  /* Entry is not scheduled (for example finished) so can not be purged */
  if (de->de_sched_state != DVR_SCHEDULED)
    return 0;

  /* No broadcast matched when entry was reloaded from dvr/log */
  if (!de->de_bcast)
    return 1;

  /* Confirm autorec still matches the broadcast */
  return !dvr_autorec_cmp(de->de_autorec, de->de_bcast);
}

/**
 * Purge any dvr_entry for autorec entries that
 * no longer match the current schedule.
 */
void
dvr_autorec_purge_obsolete_timers(void)
{
  dvr_entry_t *de;
  int num_purged = 0;

  LIST_FOREACH(de, &dvrentries, de_global_link) {
    if (dvr_autorec_entry_can_be_purged(de)) {
      char ubuf[UUID_HEX_SIZE];
      char t1buf[32], t2buf[32];
      tvhinfo(LS_DVR, "Entry %s can be purged for \"%s\" start %s stop %s",
              idnode_uuid_as_str(&de->de_id, ubuf),
              lang_str_get(de->de_title, NULL),
              gmtime2local(de->de_start, t1buf, sizeof(t1buf)),
              gmtime2local(de->de_stop, t2buf, sizeof(t2buf)));

      dvr_entry_assign_broadcast(de, NULL);
      dvr_entry_destroy(de, 1);
      ++num_purged;
    }
  }
  if (num_purged)
    tvhinfo(LS_DVR, "Purged %d autorec entries that no longer match schedule", num_purged);
}


/**
 * Handle maxcount
 */
void
dvr_autorec_completed(dvr_autorec_entry_t *dae, int error_code)
{
  uint32_t count, total = 0;
  dvr_entry_t *de, *de_prev;
  uint32_t max_count;
  char ubuf[UUID_HEX_SIZE];

  if (dae == NULL) return;
  max_count = dvr_autorec_get_max_count(dae);
  if (max_count <= 0) return;
  while (1) {
    count = 0;
    de_prev = NULL;
    LIST_FOREACH(de, &dae->dae_spawns, de_autorec_link) {
      if (de->de_sched_state != DVR_COMPLETED) continue;
      if (dvr_get_filesize(de, 0) < 0) continue;
      if (de_prev == NULL || de_prev->de_start > de->de_start)
        de_prev = de;
      count++;
    }
    if (total == 0)
      total = count;
    if (count <= max_count)
      break;
    if (de_prev) {
      tvhinfo(LS_DVR, "autorec %s removing recordings %s (allowed count %u total %u)",
              dae->dae_name, idnode_uuid_as_str(&de_prev->de_id, ubuf), max_count, total);
      dvr_entry_cancel_remove(de_prev, 0);
    }
  }
}

/**
 * return 1 if the event 'e' is matched by the autorec rule 'dae'
 */
int
dvr_autorec_cmp(dvr_autorec_entry_t *dae, epg_broadcast_t *e)
{
  idnode_list_mapping_t *ilm;
  dvr_config_t *cfg;
  double duration;

  if (!e) return 0;
  if (!e->channel) return 0;
  if(dae->dae_enabled == 0 || dae->dae_weekdays == 0)
    return 0;

  if(dae->dae_channel == NULL &&
     dae->dae_channel_tag == NULL &&
     dae->dae_content_type == 0 &&
     (dae->dae_title == NULL ||
     dae->dae_title[0] == '\0') &&
     (dae->dae_cat1 == NULL || *dae->dae_cat1 == 0) &&
     (dae->dae_cat2 == NULL || *dae->dae_cat2 == 0) &&
     (dae->dae_cat3 == NULL || *dae->dae_cat3 == 0) &&
     dae->dae_minduration <= 0 &&
     (dae->dae_maxduration <= 0 || dae->dae_maxduration > 24 * 3600) &&
     dae->dae_minyear <= 0 &&
     dae->dae_maxyear <= 0 &&
     dae->dae_minseason <= 0 &&
     dae->dae_maxseason <= 0 &&
     dae->dae_serieslink_uri == NULL)
    return 0; // Avoid super wildcard match

  if(dae->dae_serieslink_uri) {
    if (!e->serieslink ||
        strcmp(dae->dae_serieslink_uri ?: "", e->serieslink->uri)) return 0;
  }

  if(dae->dae_btype != DVR_AUTOREC_BTYPE_ALL) {
    if (dae->dae_btype == DVR_AUTOREC_BTYPE_NEW_OR_UNKNOWN && e->is_repeat)
      return 0;
    else if (dae->dae_btype == DVR_AUTOREC_BTYPE_NEW && !e->is_new)
      return 0;
    else if (dae->dae_btype == DVR_AUTOREC_BTYPE_REPEAT && e->is_repeat == 0)
      return 0;
  }

  /* Note: ignore channel test if we allow quality unlocking */
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
    if (!epg_genre_list_contains(&e->genre, &ct, 1))
      return 0;
  }

  if (e->category) {
    if (dae->dae_cat1 && *dae->dae_cat1 &&
        !string_list_contains_string(e->category, dae->dae_cat1))
      return 0;
    if (dae->dae_cat2 && *dae->dae_cat2 &&
        !string_list_contains_string(e->category, dae->dae_cat2))
      return 0;
    if (dae->dae_cat3 && *dae->dae_cat3 &&
        !string_list_contains_string(e->category, dae->dae_cat3))
      return 0;
  } else if ((dae->dae_cat1 && *dae->dae_cat1) ||
             (dae->dae_cat2 && *dae->dae_cat2) ||
             (dae->dae_cat3 && *dae->dae_cat3)) {
    /* No category in event but autorec has category, so no match. */
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

  /* Only do year/season checks when programme guide has these values available.
   * This is because you might have "minseason=5" but Christmas specials have no
   * no season (or season=0) and many people still want them to be picked up by
   * default.
   */
  if(e->copyright_year && dae->dae_minyear > 0) {
    if(e->copyright_year < dae->dae_minyear) return 0;
  }

  if(e->copyright_year && dae->dae_maxyear > 0) {
    if(e->copyright_year > dae->dae_maxyear) return 0;
  }

  if(e->epnum.s_num && dae->dae_minseason > 0) {
    if(e->epnum.s_num < dae->dae_minseason) return 0;
  }

  if(e->epnum.s_num && dae->dae_maxseason > 0) {
    if(e->epnum.s_num > dae->dae_maxseason) return 0;
  }

  if(dae->dae_weekdays != 0x7f) {
    struct tm tm;
    localtime_r(&e->start, &tm);
    if(!((1 << ((tm.tm_wday ?: 7) - 1)) & dae->dae_weekdays))
      return 0;
  }

  /* If we have a dae_star_rating but the episode has no star
   * rating (zero) then it will not be recorded.  So we do not
   * have "&& e->episode->star_rating" here.  Conversely, if
   * dae_star_rating is zero then that means "do not check
   * star rating of episode".
   */
  if (dae->dae_star_rating)
    if (e->star_rating < dae->dae_star_rating)
      return 0;

  /* Do not check title if the event is from the serieslink group */
  if((dae->dae_serieslink_uri == NULL || dae->dae_serieslink_uri[0] == '\0') &&
     dae->dae_title != NULL && dae->dae_title[0] != '\0') {
    lang_str_ele_t *ls;
    if (!dae->dae_fulltext) {
      if(!e->title) return 0;
      RB_FOREACH(ls, e->title, link)
        if (!regex_match(&dae->dae_title_regex, ls->str)) break;
    } else {
      ls = NULL;
      if (e->title)
        RB_FOREACH(ls, e->title, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
      if (!ls && e->subtitle)
        RB_FOREACH(ls, e->subtitle, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
      if (!ls && e->summary)
        RB_FOREACH(ls, e->summary, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
      if (!ls && e->description)
        RB_FOREACH(ls, e->description, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
      if (!ls && e->credits_cached)
        RB_FOREACH(ls, e->credits_cached, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
      if (!ls && e->keyword_cached)
        RB_FOREACH(ls, e->keyword_cached, link)
          if (!regex_match(&dae->dae_title_regex, ls->str)) break;
    }
    if (!ls) return 0;
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
      tvhwarn(LS_DVR, "invalid autorec entry uuid '%s'", uuid);
    free(dae);
    return NULL;
  }

  dvr_config_t *c = dvr_config_find_by_uuid(htsmsg_get_str(conf, "config_name"));
  if (c && c->dvr_autorec_dedup) dae->dae_record = c->dvr_autorec_dedup;
  dae->dae_weekdays = 0x7f;
  dae->dae_pri = DVR_PRIO_DEFAULT;
  dae->dae_start = -1;
  dae->dae_start_window = -1;
  dae->dae_enabled = 1;
  dae->dae_record = DVR_AUTOREC_RECORD_DVR_PROFILE;
  dae->dae_config = dvr_config_find_by_name_default(NULL);
  LIST_INSERT_HEAD(&dae->dae_config->dvr_autorec_entries, dae, dae_config_link);

  TAILQ_INSERT_TAIL(&autorec_entries, dae, dae_link);

  idnode_load(&dae->dae_id, conf);

  htsp_autorec_entry_add(dae);

  return dae;
}


dvr_autorec_entry_t*
dvr_autorec_create_htsp(htsmsg_t *conf)
{
  dvr_autorec_entry_t *dae;

  dae = dvr_autorec_create(NULL, conf);
  htsmsg_destroy(conf);

  if (dae) {
    dvr_autorec_changed(dae, 0);
    dvr_autorec_completed(dae, 0);
    idnode_changed(&dae->dae_id);
  }

  return dae;
}

void
dvr_autorec_update_htsp(dvr_autorec_entry_t *dae, htsmsg_t *conf)
{
  idnode_update(&dae->dae_id, conf);
  idnode_changed(&dae->dae_id);
  tvhinfo(LS_DVR, "autorec \"%s\" on \"%s\": Updated", dae->dae_title ? dae->dae_title : "",
      (dae->dae_channel && dae->dae_channel->ch_name) ? dae->dae_channel->ch_name : "any channel");
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
  const char *chname;
  char *title;
  const char *name;
  if (!event)
    return NULL;
  chname = channel_get_name(event->channel, NULL);
  if (!chname)
    return NULL;
  conf = htsmsg_create_map();
  title = regexp_escape(epg_broadcast_get_title(event, NULL));
  name = epg_broadcast_get_title(event, NULL);
  htsmsg_add_u32(conf, "enabled", 1);
  htsmsg_add_str(conf, "title", title);
  htsmsg_add_str(conf, "name", name);
  free(title);
  htsmsg_add_str(conf, "config_name", dvr_config_name ?: "");
  htsmsg_add_str(conf, "channel", chname);
  if (event->serieslink)
    htsmsg_add_str(conf, "serieslink", event->serieslink->uri);
  htsmsg_add_str(conf, "owner", owner ?: "");
  htsmsg_add_str(conf, "creator", creator ?: "");
  htsmsg_add_str(conf, "comment", comment ?: "");
  dae = dvr_autorec_create(NULL, conf);
  htsmsg_destroy(conf);
  if (dae) {
    dvr_autorec_changed(dae, 0);
    dvr_autorec_completed(dae, 0);
  }
  return dae;
}

/**
 *
 */
static void
autorec_entry_destroy(dvr_autorec_entry_t *dae, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&dae->dae_id, delconf);

  dvr_autorec_purge_spawns(dae, delconf, 0);

  if (delconf)
    hts_settings_remove("dvr/autorec/%s", idnode_uuid_as_str(&dae->dae_id, ubuf));

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
  free(dae->dae_cat1);
  free(dae->dae_cat2);
  free(dae->dae_cat3);

  autorec_regfree(dae);

  if(dae->dae_channel != NULL)
    LIST_REMOVE(dae, dae_channel_link);

  if(dae->dae_channel_tag != NULL)
    LIST_REMOVE(dae, dae_channel_tag_link);

  free(dae);
}

/* **************************************************************************
 * DVR Autorec Entry Class definition
 * **************************************************************************/

static void
dvr_autorec_entry_class_changed(idnode_t *self)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;

  if (dae->dae_error)
    dae->dae_enabled = 0;
  dvr_autorec_changed(dae, 1);
  dvr_autorec_completed(dae, 0);
  htsp_autorec_entry_update(dae);
}

static htsmsg_t *
dvr_autorec_entry_class_save(idnode_t *self, char *filename, size_t fsize)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;
  htsmsg_t *m = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&dae->dae_id, m);
  if (filename)
    snprintf(filename, fsize, "dvr/autorec/%s", idnode_uuid_as_str(&dae->dae_id, ubuf));
  return m;
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

static void
dvr_autorec_entry_class_get_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)self;
  const char *s = "";
  if (dae->dae_name && dae->dae_name[0] != '\0')
    s = dae->dae_name;
  else if (dae->dae_comment && dae->dae_comment[0] != '\0')
    s = dae->dae_comment;
  snprintf(dst, dstsize, "%s", s);
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
    if (dae->dae_id.in_access &&
        !channel_access(ch, dae->dae_id.in_access, 1))
      return 0;
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
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel)
    idnode_uuid_as_str(&dae->dae_channel->ch_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static char *
dvr_autorec_entry_class_channel_rend(void *o, const char *lang)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel)
    return strdup(channel_get_name(dae->dae_channel, tvh_gettext_lang(lang, channel_blank_name)));
  return NULL;
}

static int
dvr_autorec_entry_class_title_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  const char *title = v ?: "";
  if (strcmp(title, dae->dae_title ?: "")) {
    if (dae->dae_title)
      autorec_regfree(dae);
    dae->dae_error = 0;
    if (!regex_compile(&dae->dae_title_regex, title, TVHREGEX_CASELESS, LS_DVR))
      dae->dae_title = strdup(title);
    else
      dae->dae_error = 1;
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
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_channel_tag)
    idnode_uuid_as_str(&dae->dae_channel_tag->ct_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
  if (tm >= 0)
    snprintf(prop_sbuf, PROP_SBUF_LEN, "%02d:%02d", tm / 60, tm % 60);
  else
    strlcpy(prop_sbuf, N_("Any"), PROP_SBUF_LEN);
  return &prop_sbuf_ptr;
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
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae->dae_config)
    idnode_uuid_as_str(&dae->dae_config->dvr_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
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
    snprintf(buf, sizeof(buf), "%s", tvh_gettext_lang(lang, N_("Every day")));
  else if (weekdays == 0)
    snprintf(buf, sizeof(buf), "%s", tvh_gettext_lang(lang, N_("No days")));
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

/** Validate star rating is in range */
static int
dvr_autorec_entry_class_star_rating_set(void *o, const void *v)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  const uint16_t *val = (uint16_t*)v;
  if (*val > 100)
    return 0;
  dae->dae_star_rating = *val;
  return 1;
}

static htsmsg_t *
dvr_autorec_entry_class_star_rating_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e = htsmsg_create_map();
  /* No htsmsg_add_u16 so use htsmsg_add_u32 instead */
  htsmsg_add_u32(e, "key", 0);
  /* Instead of "Any" we use "No rating needed" since "Any" could
   * suggest that the programme needs a rating that could be anything,
   * whereas many programmes have no rating at all.
   */
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("No rating needed")));
  htsmsg_add_msg(m, NULL, e);

  uint32_t i;
  /* We create the list from highest to lowest since you're more
   * likely to want to record something with a high rating than scroll
   * through the list to find programmes with a poor rating.
   */
  for (i = 100; i > 0 ; i-=5) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_u32(e, "val", i);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
}

/** Generate a year list to make it easier to select min/max year */
static htsmsg_t *
dvr_autorec_entry_class_year_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_u32(e, "key", 0);
  htsmsg_add_str(e, "val", tvh_gettext_lang(lang, N_("Any")));
  htsmsg_add_msg(m, NULL, e);

  uint32_t i;
  /* We create the list from highest to lowest since you're more
   * likely to want to record something recent.
   */
  for (i = 2025; i > 1900 ; i-=5) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_u32(e, "val", i);
    htsmsg_add_msg(m, NULL, e);
  }
  return m;
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
    { N_("Use DVR configuration"),
        DVR_AUTOREC_RECORD_DVR_PROFILE },
    { N_("Record all"),
        DVR_AUTOREC_RECORD_ALL },
    { N_("All: Record if EPG/XMLTV indicates it is a unique programme"),
        DVR_AUTOREC_RECORD_UNIQUE },
    { N_("All: Record if different episode number"),
        DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER },
    { N_("All: Record if different subtitle"),
        DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE },
    { N_("All: Record if different description"),
        DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION },
    { N_("All: Record once per month"),
        DVR_AUTOREC_RECORD_ONCE_PER_MONTH },
    { N_("All: Record once per week"),
        DVR_AUTOREC_RECORD_ONCE_PER_WEEK },
    { N_("All: Record once per day"),
        DVR_AUTOREC_RECORD_ONCE_PER_DAY },
    { N_("Local: Record if different episode number"),
        DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER },
    { N_("Local: Record if different title"),
        DVR_AUTOREC_LRECORD_DIFFERENT_TITLE },
    { N_("Local: Record if different subtitle"),
        DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE },
    { N_("Local: Record if different description"),
        DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION },
    { N_("Local: Record once per month"),
        DVR_AUTOREC_LRECORD_ONCE_PER_MONTH },
    { N_("Local: Record once per week"),
        DVR_AUTOREC_LRECORD_ONCE_PER_WEEK },
    { N_("Local: Record once per day"),
        DVR_AUTOREC_LRECORD_ONCE_PER_DAY },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
dvr_autorec_entry_class_btype_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Any"),
        DVR_AUTOREC_BTYPE_ALL },
    { N_("New / premiere / unknown"),
        DVR_AUTOREC_BTYPE_NEW_OR_UNKNOWN },
    { N_("New / premiere"),
        DVR_AUTOREC_BTYPE_NEW },
    { N_("Repeated"),
        DVR_AUTOREC_BTYPE_REPEAT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
dvr_autorec_entry_category_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "channelcategory/list");
  return m;
}


static uint32_t
dvr_autorec_entry_class_owner_opts(void *o, uint32_t opts)
{
  dvr_autorec_entry_t *dae = (dvr_autorec_entry_t *)o;
  if (dae && dae->dae_id.in_access &&
      !access_verify2(dae->dae_id.in_access, ACCESS_ADMIN))
    return PO_ADVANCED;
  return PO_RDONLY | PO_ADVANCED;
}

CLASS_DOC(dvrautorec)
PROP_DOC(duplicate_handling)

/* We provide several category drop-downs to make it easy for user
 * to select several. So abstract the properties away since they
 * are nearly identical for each entry.
 */
#define CATEGORY_SELECTION_PROP(NUM)                                  \
  .type     = PT_STR,                                                 \
  .id       = "cat" #NUM,                                             \
  .name     = N_("Category " #NUM  " (selection)"),                   \
  .desc     = N_("The category of the program to look for. The xmltv "\
                 "providers often supply detailed categories such as "\
                 "Sitcom, Movie, Track/field, etc. "                  \
                 "This let you select from categories for current programmes. " \
                 "It is then combined (AND) with other fields to limit to " \
                 "programmes that match all categories. "             \
                 "If this selection list is empty then it means your provider does not " \
                 "supply programme categories."                       \
                 ),                                                   \
  .off      = offsetof(dvr_autorec_entry_t, dae_cat ## NUM),          \
  .opts     = PO_EXPERT,                                              \
  .list     = dvr_autorec_entry_category_list


const idclass_t dvr_autorec_entry_class = {
  .ic_class      = "dvrautorec",
  .ic_caption    = N_("DVR - Auto-recording (Autorecs)"),
  .ic_event      = "dvrautorec",
  .ic_doc        = tvh_doc_dvrautorec_class,
  .ic_changed    = dvr_autorec_entry_class_changed,
  .ic_save       = dvr_autorec_entry_class_save,
  .ic_get_title  = dvr_autorec_entry_class_get_title,
  .ic_delete     = dvr_autorec_entry_class_delete,
  .ic_perm       = dvr_autorec_entry_class_perm,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable auto-rec rule."),
      .def.i    = 1,
      .off      = offsetof(dvr_autorec_entry_t, dae_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("The name of the the rule."),
      .off      = offsetof(dvr_autorec_entry_t, dae_name),
    },
	  {
      .type     = PT_STR,
      .id       = "directory",
      .name     = N_("Directory"),
      .desc     = N_("When specified, this setting overrides the "
                     "subdirectory rules (except the base directory) "
                     "defined in the DVR configuration and puts all "
                     "recordings done by this entry into the "
                     "subdirectory named here. Note that Format Strings "
                     "cannot be used here. See Help for more info."),
      .off      = offsetof(dvr_autorec_entry_t, dae_directory),
      .opts     = PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "title",
      .name     = N_("Title (regexp)"),
      .desc     = N_("The title of the program to look for. Note that "
                     "this accepts case-insensitive regular expressions."),
      .set      = dvr_autorec_entry_class_title_set,
      .off      = offsetof(dvr_autorec_entry_t, dae_title),
    },
    /* We provide a small number of selection drop-downs. This is to
     * make it easier for users to see what categories are available and
     * make it easy to record for example '"Movie" "Martial arts" "Western"'
     * without user needing a regex, and allowing them to easily see what
     * categories they have available.
     */
    {
      CATEGORY_SELECTION_PROP(1)
    },
    {
      CATEGORY_SELECTION_PROP(2)
    },
    {
      CATEGORY_SELECTION_PROP(3)
    },
    {
      .type     = PT_BOOL,
      .id       = "fulltext",
      .name     = N_("Full-text"),
      .desc     = N_("When the fulltext is checked, the title pattern is "
                     "matched against title, subtitle, summary and description."),
      .off      = offsetof(dvr_autorec_entry_t, dae_fulltext),
    },
    {
      .type     = PT_STR,
      .id       = "channel",
      .name     = N_("Channel"),
      .desc     = N_("The channel on which this rule applies, i.e. the "
                     "channel you're aiming to record. You can leave "
                     "this field blank to apply the rule to all "
                     "channels."),
      .set      = dvr_autorec_entry_class_channel_set,
      .get      = dvr_autorec_entry_class_channel_get,
      .rend     = dvr_autorec_entry_class_channel_rend,
      .list     = channel_class_get_list,
    },
    {
      .type     = PT_STR,
      .id       = "tag",
      .name     = N_("Channel tag"),
      .desc     = N_("A channel tag (e.g. a group of channels) to which "
                     "this rule applies."),
      .set      = dvr_autorec_entry_class_tag_set,
      .get      = dvr_autorec_entry_class_tag_get,
      .rend     = dvr_autorec_entry_class_tag_rend,
      .list     = channel_tag_class_get_list,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "btype",
      .name     = N_("Broadcast type"),
      .desc     = N_("Select type of broadcast."),
      .def.i    = DVR_AUTOREC_BTYPE_ALL,
      .off      = offsetof(dvr_autorec_entry_t, dae_btype),
      .list     = dvr_autorec_entry_class_btype_list,
      .opts     = PO_HIDDEN | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "content_type",
      .name     = N_("Content type"),
      .desc     = N_("The content type (Movie/Drama, Sports, etc.) to "
                     "be used to filter matching events/programs."),
      .list     = dvr_autorec_entry_class_content_type_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_content_type),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_U16,
      .id       = "star_rating",
      .name     = N_("Star rating"),
      .desc     = N_("The minimum number of stars the broadcast should have - in "
                     "other words, only match programs that have at "
                     "least this rating."),
      .set      = dvr_autorec_entry_class_star_rating_set,
      .list     = dvr_autorec_entry_class_star_rating_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_star_rating),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "start",
      .name     = N_("Start after"),
      .desc     = N_("An event which starts between this \"start after\" "
                     "and \"start before\" will be matched (including "
                     "boundary values)."),
      .def.s    = N_("Any"),
      .set      = dvr_autorec_entry_class_start_set,
      .get      = dvr_autorec_entry_class_start_get,
      .list     = dvr_autorec_entry_class_time_list_,
      .opts     = PO_SORTKEY | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "start_window",
      .name     = N_("Start before"),
      .desc     = N_("An event which starts between this \"start after\" "
                     "and \"start before\" will be matched (including "
                     "boundary values)."),
      .def.s    = N_("Any"),
      .set      = dvr_autorec_entry_class_start_window_set,
      .get      = dvr_autorec_entry_class_start_window_get,
      .list     = dvr_autorec_entry_class_time_list_,
      .opts     = PO_SORTKEY | PO_DOC_NLIST,
    },
    {
      .type     = PT_TIME,
      .id       = "start_extra",
      .name     = N_("Extra start time"),
      .desc     = N_("Start recording earlier than the defined start "
                     "time by x minutes."),
      .off      = offsetof(dvr_autorec_entry_t, dae_start_extra),
      .list     = dvr_autorec_entry_class_extra_list,
      .opts     = PO_DURATION | PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_TIME,
      .id       = "stop_extra",
      .name     = N_("Extra stop time"),
      .desc     = N_("Continue recording for x minutes after scheduled "
                     "stop time"),
      .off      = offsetof(dvr_autorec_entry_t, dae_stop_extra),
      .list     = dvr_autorec_entry_class_extra_list,
      .opts     = PO_DURATION | PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .islist   = 1,
      .id       = "weekdays",
      .name     = N_("Days of week"),
      .desc     = N_("Days of the week to which the rule should apply."),
      .set      = dvr_autorec_entry_class_weekdays_set,
      .get      = dvr_autorec_entry_class_weekdays_get_,
      .list     = dvr_autorec_entry_class_weekdays_list,
      .rend     = dvr_autorec_entry_class_weekdays_rend_,
      .def.list = dvr_autorec_entry_class_weekdays_default,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "minduration",
      .name     = N_("Minimum duration"),
      .desc     = N_("The minimal duration of a matching event - in "
                     "other words, only match programs that are no "
                     "shorter than this duration."),
      .list     = dvr_autorec_entry_class_minduration_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_minduration),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "maxduration",
      .name     = N_("Maximum duration"),
      .desc     = N_("The maximal duration of a matching event - in "
                     "other words, only match programmes that are no "
                     "longer than this duration."),
      .list     = dvr_autorec_entry_class_maxduration_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_maxduration),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "minyear",
      .name     = N_("Minimum year"),
      .desc     = N_("The earliest year for the programme. Programmes must be equal to or later than this year."),
      .list     = dvr_autorec_entry_class_year_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_minyear),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "maxyear",
      .name     = N_("Maximum year"),
      .desc     = N_("The latest year for the programme. Programmes must be equal to or earlier than this year."),
      .list     = dvr_autorec_entry_class_year_list,
      .off      = offsetof(dvr_autorec_entry_t, dae_maxyear),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U16,
      .id       = "minseason",
      .name     = N_("Minimum season"),
      .desc     = N_("The earliest season for the programme. Programmes must be equal to or later than this season."),
      .off      = offsetof(dvr_autorec_entry_t, dae_minseason),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U16,
      .id       = "maxseason",
      .name     = N_("Maximum season"),
      .desc     = N_("The latest season for the programme. Programmes must be equal to or earlier than this season."),
      .off      = offsetof(dvr_autorec_entry_t, dae_maxseason),
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "pri",
      .name     = N_("Priority"),
      .desc     = N_("Priority of the recording. Higher priority entries "
                     "will take precedence and cancel lower-priority events. "
                     "The 'Not Set' value inherits the settings from "
                     "the assigned DVR configuration."),
      .list     = dvr_entry_class_pri_list,
      .def.i    = DVR_PRIO_DEFAULT,
      .off      = offsetof(dvr_autorec_entry_t, dae_pri),
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "record",
      .name     = N_("Duplicate handling"),
      .desc     = N_("How to handle duplicate recordings. The 'Use DVR "
                     "Configuration' value (the default) inherits the "
                     "settings from the assigned DVR configuration"),
      .def.i    = DVR_AUTOREC_RECORD_DVR_PROFILE,
      .doc      = prop_doc_duplicate_handling,
      .off      = offsetof(dvr_autorec_entry_t, dae_record),
      .list     = dvr_autorec_entry_class_dedup_list,
      .opts     = PO_HIDDEN | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "retention",
      .name     = N_("DVR log retention"),
      .desc     = N_("Number of days to retain information about recording."),
      .def.i    = DVR_RET_REM_DVRCONFIG,
      .off      = offsetof(dvr_autorec_entry_t, dae_retention),
      .list     = dvr_entry_class_retention_list,
      .opts     = PO_HIDDEN | PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "removal",
      .name     = N_("DVR file retention period"),
      .desc     = N_("Number of days to keep the recorded file."),
      .def.i    = DVR_RET_REM_DVRCONFIG,
      .off      = offsetof(dvr_autorec_entry_t, dae_removal),
      .list     = dvr_entry_class_removal_list,
      .opts     = PO_HIDDEN | PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "maxcount",
      .name     = N_("Maximum count (0=default)"),
      .desc     = N_("The maximum number of times this rule can be "
                     "triggered."),
      .off      = offsetof(dvr_autorec_entry_t, dae_max_count),
      .opts     = PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_U32,
      .id       = "maxsched",
      .name     = N_("Maximum schedules limit (0=default)"),
      .desc     = N_("The maximum number of recording entries this rule "
                     "can create."),
      .off      = offsetof(dvr_autorec_entry_t, dae_max_sched_count),
      .opts     = PO_HIDDEN | PO_EXPERT,
    },
    {
      .type     = PT_STR,
      .id       = "config_name",
      .name     = N_("DVR configuration"),
      .desc     = N_("The DVR profile to be used/used by this rule."),
      .set      = dvr_autorec_entry_class_config_name_set,
      .get      = dvr_autorec_entry_class_config_name_get,
      .rend     = dvr_autorec_entry_class_config_name_rend,
      .list     = dvr_entry_class_config_name_list,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "serieslink",
      .name     = N_("Series link"),
      .desc     = N_("Series link ID."),
      .off      = offsetof(dvr_autorec_entry_t, dae_serieslink_uri),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "owner",
      .name     = N_("Owner"),
      .desc     = N_("Owner of the rule."),
      .off      = offsetof(dvr_autorec_entry_t, dae_owner),
      .get_opts = dvr_autorec_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "creator",
      .name     = N_("Creator"),
      .desc     = N_("The user who created the recording, or the "
                     "auto-recording source and IP address if scheduled "
                     "by a matching rule."),
      .off      = offsetof(dvr_autorec_entry_t, dae_creator),
      .get_opts = dvr_autorec_entry_class_owner_opts,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
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
  idclass_register(&dvr_autorec_entry_class);
  if((l = hts_settings_load("dvr/autorec")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
        continue;
      (void)dvr_autorec_create(htsmsg_field_name(f), c);
    }
    htsmsg_destroy(l);
  }
}

void
dvr_autorec_done(void)
{
  dvr_autorec_entry_t *dae;

  tvh_mutex_lock(&global_lock);
  while ((dae = TAILQ_FIRST(&autorec_entries)) != NULL)
    autorec_entry_destroy(dae, 0);
  tvh_mutex_unlock(&global_lock);
}

void
dvr_autorec_update(void)
{
  dvr_autorec_entry_t *dae;
  TAILQ_FOREACH(dae, &autorec_entries, dae_link) {
    dvr_autorec_changed(dae, 0);
    dvr_autorec_completed(dae, 0);
  }
}

static void
dvr_autorec_async_reschedule_cb(void *ignored)
{
  tvhdebug(LS_DVR, "dvr_autorec_async_reschedule_cb - begin");
  dvr_autorec_update();
  tvhdebug(LS_DVR, "dvr_autorec_async_reschedule_cb - end");
}

void
dvr_autorec_async_reschedule(void)
{
  tvhtrace(LS_DVR, "dvr_autorec_async_reschedule");
  static mtimer_t reschedule_timer;
  mtimer_disarm(&reschedule_timer);
  /* We schedule the update after a brief period. This allows the
   * system to quiesce in case the user is doing a large operation
   * such as deleting numerous records due to disabling an autorec
   * rule.
   */
  mtimer_arm_rel(&reschedule_timer, dvr_autorec_async_reschedule_cb, NULL,
                 sec2mono(60));
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
    if(dvr_autorec_cmp(dae, e))
      dvr_entry_create_by_autorec(1, e, dae);
  // Note: no longer updating event here as it will be done from EPG
  //       anyway
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
      if(dvr_autorec_cmp(dae, e)) {
        enabled = 1;
        if (disabled) {
          for (p = disabled; *p && *p != e; p++);
          enabled = *p == NULL;
        }
        dvr_entry_create_by_autorec(enabled, e, dae);
      }
    }
  }

  free(disabled);
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
      idnode_changed(&dae->dae_id);
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
      idnode_changed(&dae->dae_id);
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
uint32_t
dvr_autorec_get_retention_days( dvr_autorec_entry_t *dae )
{
  if (dae->dae_retention > 0) {
    if (dae->dae_retention > DVR_RET_REM_FOREVER)
      return DVR_RET_REM_FOREVER;

    return dae->dae_retention;
  }
  return dvr_retention_cleanup(dae->dae_config->dvr_retention_days);
}

/**
 *
 */
uint32_t
dvr_autorec_get_removal_days( dvr_autorec_entry_t *dae )
{
  if (dae->dae_removal > 0) {
    if (dae->dae_removal > DVR_RET_REM_FOREVER)
      return DVR_RET_REM_FOREVER;

    return dae->dae_removal;
  }
  return dvr_retention_cleanup(dae->dae_config->dvr_removal_days);
}
