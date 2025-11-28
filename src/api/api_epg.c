/*
 *  API - EPG related calls
 *
 *  Copyright (C) 2013 Adam Sutton
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; withm even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "epg.h"
#include "imagecache.h"
#include "dvr/dvr.h"
#include "lang_codes.h"
#include "string_list.h"
#include "epggrab.h"  //Needed to be able to test for epggrab_conf.epgdb_processparentallabels

static htsmsg_t *
api_epg_get_list ( const char *s )
{
  htsmsg_t *m = NULL;
  char *r, *saveptr = NULL;
  if (s && s[0] != '\0') {
    s = r = strdup(s);
    r = strtok_r(r, ";", &saveptr);
    while (r) {
      while (*r != '\0' && *r <= ' ')
        r++;
      if (*r != '\0') {
        if (m == NULL)
          m = htsmsg_create_list();
        htsmsg_add_str(m, NULL, r);
      }
      r = strtok_r(NULL, ";", &saveptr);
    }
    free((char *)s);
  }
  return m;
}

static void
api_epg_add_channel ( htsmsg_t *m, channel_t *ch, const char *blank )
{
  int64_t chnum;
  char buf[128];
  const char *s;
  htsmsg_add_str(m, "channelName", channel_get_name(ch, blank));
  htsmsg_add_uuid(m, "channelUuid", &ch->ch_id.in_uuid);
  if ((chnum = channel_get_number(ch)) >= 0) {
    uint32_t maj = chnum / CHANNEL_SPLIT;
    uint32_t min = chnum % CHANNEL_SPLIT;
    if (min)
      snprintf(buf, sizeof(buf), "%u.%u", maj, min);
    else
      snprintf(buf, sizeof(buf), "%u", maj);
    htsmsg_add_str(m, "channelNumber", buf);
  }
  s = channel_get_icon(ch);
  if (!strempty(s)) {
    s = imagecache_get_propstr(s, buf, sizeof(buf));
    if (s)
      htsmsg_add_str(m, "channelIcon", s);
  }
}

static htsmsg_t *
api_epg_entry ( epg_broadcast_t *eb, const char *lang, const access_t *perm, const char **blank )
{
  const char *s, *blank2 = NULL;
  char buf[128];
  channel_t *ch = eb->channel;
  htsmsg_t *m, *m2;
  epg_episode_num_t epnum;
  epg_genre_t *eg;
  dvr_entry_t *de;
  char ubuf[UUID_HEX_SIZE];

  if (!ch) return NULL;

  if (blank == NULL)
    blank = &blank2;
  if (*blank == NULL)
    *blank = tvh_gettext_lang(lang, channel_blank_name);

  m = htsmsg_create_map();

  /* EPG IDs */
  htsmsg_add_u32(m, "eventId", eb->id);

  if(eb->xmltv_eid)  //This is the optional external reference provided by XMLTV.
  {
    htsmsg_add_str(m, "eventId_xmltv", eb->xmltv_eid);
  }

  if (eb->episodelink && strncasecmp(eb->episodelink->uri, "tvh://", 6))
    htsmsg_add_str(m, "episodeUri", eb->episodelink->uri);
  if (eb->serieslink)
    htsmsg_add_str(m, "serieslinkUri", eb->serieslink->uri);

  /* Channel Info */
  api_epg_add_channel(m, ch, *blank);

  /* Time */
  htsmsg_add_s64(m, "start", eb->start);
  htsmsg_add_s64(m, "stop", eb->stop);

  /* Title/description */
  if ((s = epg_broadcast_get_title(eb, lang)))
    htsmsg_add_str(m, "title", s);
  if ((s = epg_broadcast_get_subtitle(eb, lang)))
    htsmsg_add_str(m, "subtitle", s);
  if ((s = epg_broadcast_get_summary(eb, lang)))
    htsmsg_add_str(m, "summary", s);
  if ((s = epg_broadcast_get_description(eb, lang)))
    htsmsg_add_str(m, "description", s);
  if (eb->credits) {
    htsmsg_add_msg(m, "credits", htsmsg_copy(eb->credits));
  }
  if (eb->category) {
    htsmsg_add_msg(m, "category", string_list_to_htsmsg(eb->category));
  }
  if (eb->keyword) {
    htsmsg_add_msg(m, "keyword", string_list_to_htsmsg(eb->keyword));
  }

  if (eb->is_new)
    htsmsg_add_u32(m, "new", eb->is_new);
  if (eb->is_repeat)
    htsmsg_add_u32(m, "repeat", eb->is_repeat);
  if (eb->is_widescreen)
    htsmsg_add_u32(m, "widescreen", eb->is_widescreen);
  if (eb->is_deafsigned)
    htsmsg_add_u32(m, "deafsigned", eb->is_deafsigned);
  if (eb->is_subtitled)
    htsmsg_add_u32(m, "subtitled", eb->is_subtitled);
  if (eb->is_audio_desc)
    htsmsg_add_u32(m, "audiodesc", eb->is_audio_desc);
  if (eb->is_hd)
    htsmsg_add_u32(m, "hd", eb->is_hd);
  if (eb->is_bw)
    htsmsg_add_u32(m, "bw", eb->is_bw);
  if (eb->lines)
    htsmsg_add_u32(m, "lines", eb->lines);
  if (eb->aspect)
    htsmsg_add_u32(m, "aspect", eb->aspect);

  /* Episode info */
  epg_broadcast_get_epnum(eb, &epnum);
  if (epnum.s_num) {
    htsmsg_add_u32(m, "seasonNumber", epnum.s_num);
    if (epnum.s_cnt)
      htsmsg_add_u32(m, "seasonCount", epnum.s_cnt);
  }
  if (epnum.e_num) {
    htsmsg_add_u32(m, "episodeNumber", epnum.e_num);
    if (epnum.e_cnt)
      htsmsg_add_u32(m, "episodeCount", epnum.e_cnt);
  }
  if (epnum.p_num) {
    htsmsg_add_u32(m, "partNumber", epnum.p_num);
    if (epnum.p_cnt)
      htsmsg_add_u32(m, "partCount", epnum.p_cnt);
  }
  if (epnum.text)
    htsmsg_add_str(m, "episodeOnscreen", epnum.text);
  else if (epg_broadcast_epnumber_format(eb, buf, sizeof(buf), NULL,
                                         "s%02d", ".", "e%02d", ""))
    htsmsg_add_str(m, "episodeOnscreen", buf);

  /* Image */
  s = eb->image;
  if (!strempty(s)) {
    s = imagecache_get_propstr(s, buf, sizeof(buf));
    if (s)
      htsmsg_add_str(m, "image", s);
  }

  /* Rating */
  if (eb->star_rating)
    htsmsg_add_u32(m, "starRating", eb->star_rating);
  if (eb->age_rating)
    htsmsg_add_u32(m, "ageRating", eb->age_rating);

  if(epggrab_conf.epgdb_processparentallabels)
  {
    if (eb->rating_label)
      {
        if(eb->rating_label->rl_display_label){
          htsmsg_add_str(m, "ratingLabel", eb->rating_label->rl_display_label);
        }
        if(eb->rating_label->rl_icon){
          s = eb->rating_label->rl_icon;
          if (!strempty(s)) {
            s = imagecache_get_propstr(s, buf, sizeof(buf));
            if (s)
              htsmsg_add_str(m, "ratingLabelIcon", s);
          }//END we got an imagecache
        }//END rating label icon is not null
      }//END rating label is not null
  }//END parental labels enabled.

  if (eb->first_aired)
    htsmsg_add_s64(m, "first_aired", eb->first_aired);
  if (eb->copyright_year)
    htsmsg_add_u32(m, "copyright_year", eb->copyright_year);

  /* Content Type */
  m2 = NULL;
  LIST_FOREACH(eg, &eb->genre, link) {
    if (m2 == NULL)
      m2 = htsmsg_create_list();
    htsmsg_add_u32(m2, NULL, eg->code);
  }
  if (m2)
    htsmsg_add_msg(m, "genre", m2);

  /* Recording */
  if (eb->channel && !access_verify2(perm, ACCESS_RECORDER)) {
    /* Note: only first hit is matched */
    LIST_FOREACH(de, &eb->channel->ch_dvrs, de_channel_link) {
      if (de->de_bcast != eb)
        continue;
      if (access_verify_list(perm->aa_dvrcfgs,
                             idnode_uuid_as_str(&de->de_config->dvr_id, ubuf)))
        continue;
      htsmsg_add_uuid(m, "dvrUuid", &de->de_id.in_uuid);
      htsmsg_add_str(m, "dvrState", dvr_entry_schedstatus(de));
      break;
    }
  }

  /* Next event */
  if ((eb = epg_broadcast_get_next(eb)))
    htsmsg_add_u32(m, "nextEventId", eb->id);

  return m;
}

static void
api_epg_filter_set_str
  ( epg_filter_str_t *f, const char *str, int comp )
{
  f->str = strdup(str);
  f->comp = comp;
}

static void
api_epg_filter_add_str
  ( epg_query_t *eq, const char *k, const char *v, int comp )
{
  if (!strcmp(k, "channelName"))
    api_epg_filter_set_str(&eq->channel_name, v, comp);
  else if (!strcmp(k, "title"))
    api_epg_filter_set_str(&eq->title, v, comp);
  else if (!strcmp(k, "subtitle"))
    api_epg_filter_set_str(&eq->subtitle, v, comp);
  else if (!strcmp(k, "summary"))
    api_epg_filter_set_str(&eq->summary, v, comp);
  else if (!strcmp(k, "description"))
    api_epg_filter_set_str(&eq->description, v, comp);
  else if (!strcmp(k, "extratext"))
    api_epg_filter_set_str(&eq->extratext, v, comp);
}

static void
api_epg_filter_set_num
  ( epg_filter_num_t *f, int64_t v1, int64_t v2, int comp )
{
  /* Range? */
  if (f->comp == EC_LT && comp == EC_GT) {
    f->val2 = f->val1;
    f->val1 = v1;
    f->comp = EC_RG;
    return;
  }
  if (f->comp == EC_GT && comp == EC_LT) {
    f->val2 = v1;
    f->comp = EC_RG;
    return;
  }
  f->val1 = v1;
  f->val2 = v2;
  f->comp = comp;
}

static void
api_epg_filter_add_num
  ( epg_query_t *eq, const char *k, int64_t v1, int64_t v2, int comp )
{
  if (!strcmp(k, "start"))
    api_epg_filter_set_num(&eq->start, v1, v2, comp);
  else if (!strcmp(k, "stop"))
    api_epg_filter_set_num(&eq->stop, v1, v2, comp);
  else if (!strcmp(k, "duration"))
    api_epg_filter_set_num(&eq->duration, v1, v2, comp);
  else if (!strcmp(k, "episode"))
    api_epg_filter_set_num(&eq->episode, v1, v2, comp);
  else if (!strcmp(k, "stars"))
    api_epg_filter_set_num(&eq->stars, v1, v2, comp);
  else if (!strcmp(k, "age"))
    api_epg_filter_set_num(&eq->age, v1, v2, comp);
}

static struct strtab sortcmptab[] = {
  { "start",         ESK_START },
  { "stop",          ESK_STOP },
  { "duration",      ESK_DURATION },
  { "title",         ESK_TITLE },
  { "subtitle",      ESK_SUBTITLE },
  { "summary",       ESK_SUMMARY },
  { "description",   ESK_DESCRIPTION },
  { "extratext",     ESK_EXTRATEXT },
  { "channelName",   ESK_CHANNEL },
  { "channelNumber", ESK_CHANNEL_NUM },
  { "starRating",    ESK_STARS },
  { "ageRating",     ESK_AGE },
  { "genre",         ESK_GENRE }
};

static struct strtab filtcmptab[] = {
  { "gt",    EC_GT },
  { "lt",    EC_LT },
  { "eq",    EC_EQ },
  { "regex", EC_RE },
  { "range", EC_RG }
};

static int64_t
api_epg_decode_channel_num ( const char *s )
{
   int64_t v = atol(s);
   const char *s1 = strchr(s, '.');
   if (s1)
     v += atol(s1 + 1) % CHANNEL_SPLIT;
   return v;
}

static int
api_epg_grid
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i;
  epg_query_t eq;
  const char *str, *blank = NULL;
  char *lang;
  uint32_t start, limit, end, genre;
  int64_t duration_min, duration_max;
  htsmsg_field_t *f, *f2;
  htsmsg_t *l = NULL, *e, *filter;
  const char* mode;

  memset(&eq, 0, sizeof(eq));

  lang = access_get_lang(perm, htsmsg_get_str(args, "lang"));
  if (lang)
    eq.lang = strdup(lang);
  mode = htsmsg_get_str(args, "mode");
  str = htsmsg_get_str(args, "title");
  if (str)
    eq.stitle = strdup(str);
  eq.fulltext = htsmsg_get_bool_or_default(args, "fulltext", 0);
  eq.mergetext = htsmsg_get_bool_or_default(args, "mergetext", 0);
  eq.new_only = htsmsg_get_bool_or_default(args, "new", 0);
  str = htsmsg_get_str(args, "channel");
  if (str)
    eq.channel = strdup(str);
  str = htsmsg_get_str(args, "channelTag");
  if (str)
    eq.channel_tag = strdup(str);
  str = htsmsg_get_str(args, "cat1");
  if (str)
    eq.cat1 = strdup(str);
  str = htsmsg_get_str(args, "cat2");
  if (str)
    eq.cat2 = strdup(str);
  str = htsmsg_get_str(args, "cat3");
  if (str)
    eq.cat3 = strdup(str);

  if (mode != NULL) {
      if (!strcmp(mode, "now")) {
        eq.start.comp = EC_LT;
        eq.stop.comp = EC_GT;
        eq.start.val1 = eq.stop.val1 = gclk();
      }
  }

  duration_min = -1;
  duration_max = -1;
  htsmsg_get_s64(args, "durationMin", &duration_min);
  htsmsg_get_s64(args, "durationMax", &duration_max);
  if (duration_min > 0 || duration_max > 0) {
    eq.duration.comp = EC_RG;
    eq.duration.val1 = duration_min < 0 ? 0 : duration_min;
    eq.duration.val2 = duration_max < 0 ? 0 : duration_max;
  }

  if (!htsmsg_get_u32(args, "contentType", &genre)) {
    eq.genre = eq.genre_static;
    eq.genre[0] = genre;
    eq.genre_count = 1;
  }

  /* Filter params */
  if ((filter = htsmsg_get_list(args, "filter"))) {
    HTSMSG_FOREACH(f, filter) {
      const char *k, *t, *v;
      int comp;
      if (!(e = htsmsg_get_map_by_field(f))) continue;
      if (!(k = htsmsg_get_str(e, "field"))) continue;
      if (!(t = htsmsg_get_str(e, "type")))  continue;
      comp = str2val(htsmsg_get_str(e, "comparison") ?: "", filtcmptab);
      if (comp == -1) comp = EC_EQ;
      if (!strcmp(k, "channelNumber")) {
        if (!strcmp(t, "numeric")) {
          f2 = htsmsg_field_find(e, "value");
          if (f2) {
            int64_t v1, v2 = 0;
            if (f2->hmf_type == HMF_STR) {
              const char *s = htsmsg_field_get_str(f2);
              const char *z = s ? strchr(s, ';') : NULL;
              if (s) {
                v1 = api_epg_decode_channel_num(s);
                if (z)
                  v2 = api_epg_decode_channel_num(z);
                api_epg_filter_set_num(&eq.channel_num, v1, v2, comp);
              }
            } else {
              if (!htsmsg_field_get_s64(f2, &v1)) {
                if (v1 < CHANNEL_SPLIT)
                  v1 *= CHANNEL_SPLIT;
                api_epg_filter_set_num(&eq.channel_num, v1, 0, comp);
              }
            }
          }
        }
      } else if (!strcmp(k, "genre")) {
        if (!strcmp(t, "numeric")) {
          f2 = htsmsg_field_find(e, "value");
          if (f2) {
            int64_t v;
            if (f2->hmf_type == HMF_STR) {
              htsmsg_t *z = api_epg_get_list(htsmsg_field_get_str(f2));
              if (z) {
                htsmsg_field_t *f3;
                uint32_t count = 0;
                HTSMSG_FOREACH(f3, z)
                  count++;
                if (ARRAY_SIZE(eq.genre_static) > count)
                  eq.genre = malloc(sizeof(eq.genre[0]) * count);
                else
                  eq.genre = eq.genre_static;
                HTSMSG_FOREACH(f3, z)
                  if (!htsmsg_field_get_s64(f3, &v))
                    eq.genre[eq.genre_count++] = v;
                htsmsg_destroy(z);
              }
            } else {
              if (!htsmsg_field_get_s64(f2, &v)) {
                eq.genre_count = 1;
                eq.genre = eq.genre_static;
                eq.genre[0] = v;
              }
            }
          }
        }
      } else if (!strcmp(t, "string")) {
        if ((v = htsmsg_get_str(e, "value")))
          api_epg_filter_add_str(&eq, k, v, EC_RE);
      } else if (!strcmp(t, "numeric")) {
        f2 = htsmsg_field_find(e, "value");
        if (f2) {
          int64_t v1 = 0, v2 = 0;
          if (f2->hmf_type == HMF_STR) {
            const char *z = htsmsg_field_get_str(f2);
            if (z) {
              const char *z2 = strchr(z, ';');
              if (z2)
                v2 = strtoll(z2 + 1, NULL, 0);
              v1 = strtoll(z, NULL, 0);
            }
            api_epg_filter_add_num(&eq, k, v1, v2, comp);
          } else {
            if (!htsmsg_field_get_s64(f2, &v1))
              api_epg_filter_add_num(&eq, k, v1, v2, comp);
          }
        }
      }
    }
  }

  /* Sort */
  if ((str = htsmsg_get_str(args, "sort"))) {
    int skey = str2val(str, sortcmptab);
    if (skey >= 0) {
      eq.sort_key = skey;
      if ((str = htsmsg_get_str(args, "dir")) && !strcasecmp(str, "DESC"))
        eq.sort_dir = IS_DSC;
      else
        eq.sort_dir = IS_ASC;
    }
  } /* else.. keep default start time ascending sorting */

  /* Pagination settings */
  start = htsmsg_get_u32_or_default(args, "start", 0);
  limit = htsmsg_get_u32_or_default(args, "limit", 50);

  /* Query the EPG */
  tvh_mutex_lock(&global_lock);
  epg_query(&eq, perm);

  /* Build response */
  start = MIN(eq.entries, start);
  end   = MIN(eq.entries, start + limit);
  l     = htsmsg_create_list();
  for (i = start; i < end; i++) {
    if (!(e = api_epg_entry(eq.result[i], lang, perm, &blank))) continue;
    htsmsg_add_msg(l, NULL, e);
  }
  tvh_mutex_unlock(&global_lock);

  epg_query_free(&eq);
  free(lang);

  /* Build response */
  *resp = htsmsg_create_map();
  htsmsg_add_u32(*resp, "totalCount", eq.entries);
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_epg_sort_by_time_t(const void *a, const void *b, void *arg)
{
  const time_t *at= (const time_t*)a;
  const time_t *bt= (const time_t*)b;
  return *at - *bt;
}

/// Generate a sorted list of episodes that
/// do NOT match ebc_skip in to message l.
/// @return number of entries allocated.
static uint32_t
api_epg_episode_sorted(const struct epg_set *set,
                       const access_t *perm,
                       htsmsg_t *l,
                       const char *lang,
                       const epg_broadcast_t *ebc_skip)
{
  typedef struct {
    time_t start;
    htsmsg_t *m;
  } bcast_entry_t;

  epg_broadcast_t *ebc;
  htsmsg_t *m;
  bcast_entry_t *bcast_entries = NULL;
  const epg_set_item_t *item;
  bcast_entry_t new_bcast_entry;
  size_t num_allocated = 0;
  size_t num_entries = 0;
  size_t i;

  LIST_FOREACH(item, &set->broadcasts, item_link) {
    ebc = item->broadcast;
    if (ebc != ebc_skip) {
      m = api_epg_entry(ebc, lang, perm, NULL);
      if (num_entries == num_allocated) {
        num_allocated = MAX(100, num_allocated + 100);
        /* We don't expect any/many reallocs so we store physical struct instead of pointers */
        bcast_entries = realloc(bcast_entries, num_allocated * sizeof(bcast_entry_t));
      }

      new_bcast_entry.start = htsmsg_get_u32_or_default(m, "start", 0);
      new_bcast_entry.m = m;
      bcast_entries[num_entries++] = new_bcast_entry;
    }
  }

  if(bcast_entries != NULL)
    tvh_qsort_r(bcast_entries, num_entries, sizeof(bcast_entry_t), api_epg_sort_by_time_t, 0);

  for (i=0; i<num_entries; ++i) {
    htsmsg_t *m = bcast_entries[i].m;
    htsmsg_add_msg(l, NULL, m);
  }
  free(bcast_entries);

  return num_entries;
}


static void
api_epg_episode_broadcasts
  ( access_t *perm, htsmsg_t *l, const char *lang, epg_broadcast_t *ep,
    uint32_t *entries, epg_broadcast_t *ebc_skip )
{
  epg_set_t *episodelink = ep->episodelink;

  if (episodelink == NULL)
    return;

  /* Need to sort these ourselves since they are used as a livegrid
   * which requires remote sort.
   */
  *entries = api_epg_episode_sorted(episodelink, perm, l, lang, ebc_skip);
}

static int
api_epg_alternative
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  uint32_t id, entries = 0;
  htsmsg_t *l;
  epg_broadcast_t *e;
  char *lang;

  if (htsmsg_get_u32(args, "eventId", &id))
    return EINVAL;

  l = htsmsg_create_list();

  /* Main Job */
  lang = access_get_lang(perm, htsmsg_get_str(args, "lang"));
  tvh_mutex_lock(&global_lock);
  e = epg_broadcast_find_by_id(id);
  if (e)
    api_epg_episode_broadcasts(perm, l, lang, e, &entries, e);
  tvh_mutex_unlock(&global_lock);
  free(lang);

  /* Build response */
  *resp = htsmsg_create_map();
  htsmsg_add_u32(*resp, "totalCount", entries);
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_epg_related
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  uint32_t id, entries = 0;
  htsmsg_t *l;
  epg_broadcast_t *e;
  char *lang, *title_esc, *title_anchor;
  epg_set_t *serieslink = NULL;
  const char *title = NULL;

  if (htsmsg_get_u32(args, "eventId", &id))
    return EINVAL;

  l = htsmsg_create_list();

  /* Main Job */
  lang = access_get_lang(perm, htsmsg_get_str(args, "lang"));
  tvh_mutex_lock(&global_lock);
  e = epg_broadcast_find_by_id(id);
  /* e might not exist if it is a dvr entry that does not exist in the epg */
  if (e)
    serieslink = e->serieslink;
  if (serieslink) {
    entries = api_epg_episode_sorted(serieslink, perm, l, lang, e);
  } else {
    /* Ensure we have a title in the query we are generating.  If no
     * title, then we return a dummy message.
     */
    title = epg_broadcast_get_title(e, lang);
    if (title) {
      /* Need to escape/anchor the search, otherwise the film "elf"
       *  matches titles containing "self".
       */
      title_esc = regexp_escape(title);
      title_anchor = alloca(strlen(title_esc) + 3);
      sprintf(title_anchor, "^%s$", title_esc);
      htsmsg_add_str(args, "title", title_anchor);
      free(title_esc);

      /* Have to unlock here since grid will re-lock */
      tvh_mutex_unlock(&global_lock);
      free(lang);
      htsmsg_destroy(l);
      /* And let the grid do the query for us */
      return api_epg_grid(perm, opaque, op, args, resp);
    }
    /*FALLTHRU*/
  }

  tvh_mutex_unlock(&global_lock);
  free(lang);

  /* Build response */
  *resp = htsmsg_create_map();
  htsmsg_add_u32(*resp, "totalCount", entries);
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_epg_load
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  uint32_t id = 0, entries = 0;
  htsmsg_t *l, *ids = NULL, *m;
  htsmsg_field_t *f;
  epg_broadcast_t *e;
  const char *blank = NULL;
  char *lang;

  if (!(f = htsmsg_field_find(args, "eventId")))
    return EINVAL;
  if (!(ids = htsmsg_field_get_list(f)))
    if (htsmsg_field_get_u32(f, &id))
      return EINVAL;

  l = htsmsg_create_list();

  /* Main Job */
  tvh_mutex_lock(&global_lock);
  lang = access_get_lang(perm, htsmsg_get_str(args, "lang"));
  if (ids) {
    HTSMSG_FOREACH(f, ids) {
      if (htsmsg_field_get_u32(f, &id)) continue;
      e = epg_broadcast_find_by_id(id);
      if (e == NULL) continue;
      if ((m = api_epg_entry(e, lang, perm, &blank)) == NULL) continue;
      htsmsg_add_msg(l, NULL, m);
      entries++;
    }
  } else {
    e = epg_broadcast_find_by_id(id);
    if (e != NULL && (m = api_epg_entry(e, lang, perm, &blank)) != NULL) {
      htsmsg_add_msg(l, NULL, m);
      entries++;
    }
  }
  tvh_mutex_unlock(&global_lock);
  free(lang);

  /* Build response */
  *resp = htsmsg_create_map();
  htsmsg_add_u32(*resp, "totalCount", entries);
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_epg_content_type_list(access_t *perm, void *opaque, const char *op,
                          htsmsg_t *args, htsmsg_t **resp)
{
  htsmsg_t *array;
  int full = 0;

  htsmsg_get_bool(args, "full", &full);

  *resp = htsmsg_create_map();
  array = epg_genres_list_all(full ? 0 : 1, 0, perm->aa_lang_ui);
  htsmsg_add_msg(*resp, "entries", array);
  return 0;
}

void api_epg_init ( void )
{
  static api_hook_t ah[] = {
    { "epg/events/grid",        ACCESS_ANONYMOUS, api_epg_grid, NULL },
    { "epg/events/alternative", ACCESS_ANONYMOUS, api_epg_alternative, NULL },
    { "epg/events/related",     ACCESS_ANONYMOUS, api_epg_related, NULL },
    { "epg/events/load",        ACCESS_ANONYMOUS, api_epg_load, NULL },
    { "epg/content_type/list",  ACCESS_ANONYMOUS, api_epg_content_type_list, NULL },

    { NULL },
  };

  api_register_all(ah);
}
