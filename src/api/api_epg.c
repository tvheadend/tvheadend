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
#include "dvr/dvr.h"

static htsmsg_t *
api_epg_entry ( epg_broadcast_t *eb, const char *lang )
{
  const char *s;
  char buf[64];
  epg_episode_t *ee = eb->episode;
  channel_t     *ch = eb->channel;
  htsmsg_t *m;
  epg_episode_num_t epnum;
  dvr_entry_t *de;

  if (!ee || !ch) return NULL;

  m = htsmsg_create_map();

  /* EPG IDs */
  htsmsg_add_u32(m, "id", eb->id);
  // TODO: the above is for UI compat, remove it
  htsmsg_add_u32(m, "eventId", eb->id);
  if (ee) {
    htsmsg_add_u32(m, "episodeId", ee->id);
    if (ee->uri && strncasecmp(ee->uri, "tvh://", 6))
      htsmsg_add_str(m, "episodeUri", ee->uri);
  }
  if (eb->serieslink) {
    htsmsg_add_u32(m, "serieslinkId", eb->serieslink->id);
    if (eb->serieslink->uri)
      htsmsg_add_str(m, "serieslinkUri", eb->serieslink->uri);
  }
  
  /* Channel Info */
  htsmsg_add_str(m, "channel",     ch->ch_name ?: "");
  // TODO: the above is for UI compat, remove it
  htsmsg_add_str(m, "channelName", ch->ch_name ?: "");
  htsmsg_add_str(m, "channelUuid", channel_get_uuid(ch));
  htsmsg_add_u32(m, "channelId",   channel_get_id(ch));
  
  /* Time */
  htsmsg_add_s64(m, "start", eb->start);
  htsmsg_add_s64(m, "stop", eb->stop);
  htsmsg_add_s64(m, "duration", eb->stop - eb->start);
  // TODO: the above can be removed

  /* Title/description */
  if ((s = epg_broadcast_get_title(eb, lang)))
    htsmsg_add_str(m, "title", s);
#if TODO
  if ((s = epg_broadcast_get_subtitle(eb, lang)))
    htsmsg_add_str(m, "subtitle", s);
#endif
  if ((s = epg_broadcast_get_summary(eb, lang)))
    htsmsg_add_str(m, "summary", s);
  if ((s = epg_broadcast_get_description(eb, lang)))
    htsmsg_add_str(m, "description", s);

  /* Episode info */
  if (ee) {
    
    /* Number */
    epg_episode_get_epnum(ee, &epnum);
    if (epnum.s_num) {
      htsmsg_add_u32(m, "seasonNumber", epnum.s_num);
      if (epnum.s_cnt)
        htsmsg_add_u32(m, "seasonCount", epnum.s_cnt);
    }
    if (epnum.e_num) {
      htsmsg_add_u32(m, "episodeNumber", epnum.e_num);
      if (epnum.s_cnt)
        htsmsg_add_u32(m, "episodeCount", epnum.e_cnt);
    }
    if (epnum.p_num) {
      htsmsg_add_u32(m, "partNumber", epnum.p_num);
      if (epnum.p_cnt)
        htsmsg_add_u32(m, "partCount", epnum.p_cnt);
    }
    if (epnum.text)
      htsmsg_add_str(m, "episodeOnscreen", epnum.text);
    else if (epg_episode_number_format(ee, buf, sizeof(buf), NULL,
                                       "s%02d", ".", "e%02d", ""))
      htsmsg_add_str(m, "episodeOnscreen", buf);

    /* Image */
    if (ee->image)
      htsmsg_add_str(m, "image", ee->image);
  }
    
  /* Recording */
  if ((de = dvr_entry_find_by_event(eb)))
    htsmsg_add_u32(m, "dvrId", de->de_id);

  /* Next event */
  if ((eb = epg_broadcast_get_next(eb)))
    htsmsg_add_u32(m, "nextEventId", eb->id);
  
  return m;
}

static int
api_epg_grid
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i;
  epg_query_result_t eqr;
  const char *ch, *tag, *title, *lang/*, *genre*/;
  uint32_t start, limit, end;
  htsmsg_t *l = NULL, *e;

  *resp = htsmsg_create_map();

  /* Query params */
  ch    = htsmsg_get_str(args, "channel");
  tag   = htsmsg_get_str(args, "tag");
  //genre = htsmsg_get_str(args, "genre");
  title = htsmsg_get_str(args, "title");
  lang  = htsmsg_get_str(args, "lang");
  // TODO: support multiple tag/genre/channel?

  /* Pagination settings */
  start = htsmsg_get_u32_or_default(args, "start", 0);
  limit = htsmsg_get_u32_or_default(args, "limit", 50);

  /* Query the EPG */
  pthread_mutex_lock(&global_lock); 
  epg_query(&eqr, ch, tag, NULL, /*genre,*/ title, lang);
  epg_query_sort(&eqr);
  // TODO: optional sorting

  /* Build response */
  start = MIN(eqr.eqr_entries, start);
  end   = MIN(eqr.eqr_entries, start + limit);
  for (i = start; i < end; i++) {
    if (!(e = api_epg_entry(eqr.eqr_array[i], lang))) continue;
    if (!l) l = htsmsg_create_list();
    htsmsg_add_msg(l, NULL, e);
  }

  pthread_mutex_unlock(&global_lock);

  /* Build response */
  htsmsg_add_u32(*resp, "totalCount", eqr.eqr_entries);
  if (l)
    htsmsg_add_msg(*resp, "events", l);

  
  return 0;
}

void api_epg_init ( void )
{
  static api_hook_t ah[] = {
    { "epg/grid",  ACCESS_ANONYMOUS, api_epg_grid, NULL },
    { NULL },
  };

  api_register_all(ah);
}
