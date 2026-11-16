/*
 *  API - EIT ignore rules
 *
 *  Copyright (C) 2026 Michael Utech
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

#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "channels.h"
#include "epg.h"
#include "epggrab/eitignore.h"

#define EITIGNORE_PREVIEW_MAX 200

static void
api_eitignore_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  eitignore_t *ei;

  (void)args;
  TAILQ_FOREACH(ei, &eitignores, ei_link)
    idnode_set_add(ins, (idnode_t*)ei, &conf->filter, perm->aa_lang_ui);
}

static int
api_eitignore_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  eitignore_t *ei;

  (void)perm;
  (void)opaque;
  (void)op;
  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  ei = eitignore_create(NULL, conf);
  if (ei)
    api_idnode_create(resp, &ei->ei_id);
  tvh_mutex_unlock(&global_lock);

  return ei ? 0 : EINVAL;
}

static htsmsg_t *
api_eitignore_event ( epg_broadcast_t *ebc, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  const char *title = epg_broadcast_get_title(ebc, lang);

  htsmsg_add_u32(m, "eventId", ebc->id);
  htsmsg_add_uuid(m, "channelUuid", &ebc->channel->ch_id.in_uuid);
  htsmsg_add_str(m, "channelName", channel_get_name(ebc->channel, ""));
  htsmsg_add_s64(m, "start", ebc->start);
  htsmsg_add_s64(m, "stop", ebc->stop);
  htsmsg_add_str(m, "title", title ?: "");
  return m;
}

/*
 * The EPG events the rule given by "conf" (saved or not) matches.
 */
static int
api_eitignore_preview
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf, *l;
  eitignore_t *ei;
  channel_t *ch;
  epg_broadcast_t *ebc;
  uint32_t total = 0;

  (void)opaque;
  (void)op;
  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  ei = eitignore_create_temp(conf);
  if (ei) {
    CHANNEL_FOREACH(ch)
      RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link)
        if (eitignore_match_broadcast(ei, ebc) &&
            total++ < EITIGNORE_PREVIEW_MAX)
          htsmsg_add_msg(l, NULL, api_eitignore_event(ebc, perm->aa_lang_ui));
    eitignore_destroy_temp(ei);
  }
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "total", total);
  return 0;
}

/*
 * The rules (enabled or not) that match the EPG event "eventId".
 */
static int
api_eitignore_match
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *l;
  eitignore_t *ei;
  epg_broadcast_t *ebc;
  uint32_t id;
  char ubuf[UUID_HEX_SIZE];

  (void)perm;
  (void)opaque;
  (void)op;
  if (htsmsg_get_u32(args, "eventId", &id))
    return EINVAL;

  l = htsmsg_create_list();
  tvh_mutex_lock(&global_lock);
  ebc = epg_broadcast_find_by_id(id);
  if (ebc)
    TAILQ_FOREACH(ei, &eitignores, ei_link)
      if (eitignore_match_broadcast(ei, ebc))
        htsmsg_add_str(l, NULL, idnode_uuid_as_str(&ei->ei_id, ubuf));
  tvh_mutex_unlock(&global_lock);

  if (ebc == NULL) {
    htsmsg_destroy(l);
    return ENOENT;
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

void api_eitignore_init ( void )
{
  static api_hook_t ah[] = {
    { "eitignore/class",   ACCESS_ADMIN, api_idnode_class, (void*)&eitignore_class },
    { "eitignore/grid",    ACCESS_ADMIN, api_idnode_grid,  &api_eitignore_grid },
    { "eitignore/create",  ACCESS_ADMIN, api_eitignore_create, NULL },
    { "eitignore/preview", ACCESS_ADMIN, api_eitignore_preview, NULL },
    { "eitignore/match",   ACCESS_ADMIN, api_eitignore_match, NULL },
    { NULL },
  };

  api_register_all(ah);
}
