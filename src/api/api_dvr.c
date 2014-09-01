/*
 *  API - DVR
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include "dvr/dvr.h"
#include "epg.h"
#include "api.h"

static const char *
api_dvr_config_name( access_t *perm, const char *config_uuid )
{
  dvr_config_t *cfg;

  lock_assert(&global_lock);

  if (access_verify2(perm, ACCESS_RECORDER_ALL))
    return config_uuid; /* no change */

  cfg = dvr_config_find_by_name(perm->aa_username);
  if (cfg)
    return cfg->dvr_config_name;

  if (perm->aa_username)
    tvhlog(LOG_INFO, "dvr", "User '%s' has no dvr config with identical name, using default...", perm->aa_username);

  return NULL; /* default */
}

/*
 *
 */

static void
api_dvr_config_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_config_t *cfg;

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    if (!idnode_perm((idnode_t *)cfg, perm, NULL))
      idnode_set_add(ins, (idnode_t*)cfg, &conf->filter);
}

static int
api_dvr_config_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_config_t *cfg;
  htsmsg_t *conf;
  const char *s;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;
  if (!(s = htsmsg_get_str(conf, "name")))
    return EINVAL;
  if (s[0] == '\0')
    return EINVAL;
  if (access_verify2(perm, ACCESS_ADMIN) &&
      access_verify2(perm, ACCESS_RECORDER_ALL | ACCESS_RECORDER))
    return EACCES;

  pthread_mutex_lock(&global_lock);
  if ((cfg = dvr_config_create(NULL, NULL, conf)))
    dvr_config_save(cfg);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int is_dvr_entry_finished(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_COMPLETED && !entry->de_last_error && dvr_get_filesize(entry) != -1;
}

static int is_dvr_entry_upcoming(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_RECORDING || state == DVR_SCHEDULED;
}

static int is_dvr_entry_failed(dvr_entry_t *entry)
{
  if (is_dvr_entry_finished(entry))
    return 0;
  if (is_dvr_entry_upcoming(entry))
    return 0;
  return 1;
}

static void
api_dvr_entry_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    idnode_set_add(ins, (idnode_t*)de, &conf->filter);
}

static void
api_dvr_entry_grid_upcoming
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (is_dvr_entry_upcoming(de))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter);
}

static void
api_dvr_entry_grid_finished
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (is_dvr_entry_finished(de))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter);
}

static void
api_dvr_entry_grid_failed
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (is_dvr_entry_failed(de))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter);
}

static int
api_dvr_entry_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_entry_t *de;
  htsmsg_t *conf;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  if (access_verify2(perm, ACCESS_RECORDER_ALL)) {
    htsmsg_delete_field(conf, "config_name");
    htsmsg_add_str(conf, "config_name", perm->aa_username ?: "");
  }

  pthread_mutex_lock(&global_lock);
  if ((de = dvr_entry_create(NULL, conf)))
    dvr_entry_save(de);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static htsmsg_t *
api_dvr_entry_create_from_single(htsmsg_t *args)
{
  htsmsg_t *entries, *m;
  const char *s1, *s2;

  if (!(s1 = htsmsg_get_str(args, "config_uuid")))
    return NULL;
  if (!(s2 = htsmsg_get_str(args, "event_id")))
    return NULL;
  entries = htsmsg_create_list();
  m = htsmsg_create_map();
  htsmsg_add_str(m, "config_uuid", s1);
  htsmsg_add_str(m, "event_id", s2);
  htsmsg_add_msg(entries, NULL, m);
  return entries;
}

static int
api_dvr_entry_create_by_event
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_entry_t *de;
  const char *config_uuid;
  epg_broadcast_t *e;
  htsmsg_t *entries, *entries2 = NULL, *m;
  htsmsg_field_t *f;
  const char *s;
  int count = 0;

  if (!(entries = htsmsg_get_list(args, "entries"))) {
    entries = entries2 = api_dvr_entry_create_from_single(args);
    if (!entries)
      return EINVAL;
  }

  HTSMSG_FOREACH(f, entries) {
    if (!(m = htsmsg_get_map_by_field(f))) continue;

    if (!(config_uuid = htsmsg_get_str(m, "config_uuid")))
      continue;
    if (!(s = htsmsg_get_str(m, "event_id")))
      continue;

    pthread_mutex_lock(&global_lock);
    if ((e = epg_broadcast_find_by_id(atoi(s), NULL))) {
      de = dvr_entry_create_by_event(api_dvr_config_name(perm, config_uuid),
                                     e, 0, 0, perm->aa_representative,
                                     NULL, DVR_PRIO_NORMAL);
      if (de)
        dvr_entry_save(de);
    }
    pthread_mutex_unlock(&global_lock);
    count++;
  }

  htsmsg_destroy(entries2);

  return !count ? EINVAL : 0;
}

static void
api_dvr_cancel(access_t *perm, idnode_t *self)
{
  dvr_entry_cancel((dvr_entry_t *)self);
}

static int
api_dvr_entry_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(perm, args, resp, api_dvr_cancel);
}

static void
api_dvr_autorec_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_autorec_entry_t *dae;

  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    idnode_set_add(ins, (idnode_t*)dae, &conf->filter);
}

static int
api_dvr_autorec_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  dvr_autorec_entry_t *dae;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  htsmsg_delete_field(conf, "creator");
  if (perm->aa_representative)
    htsmsg_add_str(conf, "creator", perm->aa_representative);

  pthread_mutex_lock(&global_lock);
  dae = dvr_autorec_create(NULL, conf);
  if (dae)
    dvr_autorec_save(dae);
  pthread_mutex_unlock(&global_lock);

  return 0;
}

static int
api_dvr_autorec_create_by_series
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_autorec_entry_t *dae;
  epg_broadcast_t *e;
  htsmsg_t *entries, *entries2 = NULL, *m;
  htsmsg_field_t *f;
  const char *config_uuid, *s;
  int count = 0;

  if (!(entries = htsmsg_get_list(args, "entries"))) {
    entries = entries2 = api_dvr_entry_create_from_single(args);
    if (!entries)
      return EINVAL;
  }

  HTSMSG_FOREACH(f, entries) {
    if (!(m = htsmsg_get_map_by_field(f))) continue;

    if (!(config_uuid = htsmsg_get_str(m, "config_uuid")))
      continue;
    if (!(s = htsmsg_get_str(m, "event_id")))
      continue;

    pthread_mutex_lock(&global_lock);
    if ((e = epg_broadcast_find_by_id(atoi(s), NULL))) {
      dae = dvr_autorec_add_series_link(api_dvr_config_name(perm, config_uuid),
                                        e, perm->aa_representative,
                                        "Created from EPG query");
      if (dae)
        dvr_autorec_save(dae);
    }
    pthread_mutex_unlock(&global_lock);
    count++;
  }

  htsmsg_destroy(entries2);

  return !count ? EINVAL : 0;
}

void api_dvr_init ( void )
{
  static api_hook_t ah[] = {
    { "dvr/config/class",          ACCESS_RECORDER, api_idnode_class, (void*)&dvr_config_class },
    { "dvr/config/grid",           ACCESS_RECORDER, api_idnode_grid, api_dvr_config_grid },
    { "dvr/config/create",         ACCESS_ADMIN, api_dvr_config_create, NULL },

    { "dvr/entry/class",           ACCESS_RECORDER, api_idnode_class, (void*)&dvr_entry_class },
    { "dvr/entry/grid",            ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid },
    { "dvr/entry/grid_upcoming",   ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_upcoming },
    { "dvr/entry/grid_finished",   ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_finished },
    { "dvr/entry/grid_failed",     ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_failed },
    { "dvr/entry/create",          ACCESS_RECORDER, api_dvr_entry_create, NULL },
    { "dvr/entry/create_by_event", ACCESS_RECORDER, api_dvr_entry_create_by_event, NULL },
    { "dvr/entry/cancel",          ACCESS_RECORDER, api_dvr_entry_cancel, NULL },

    { "dvr/autorec/class",         ACCESS_RECORDER, api_idnode_class, (void*)&dvr_autorec_entry_class },
    { "dvr/autorec/grid",          ACCESS_RECORDER, api_idnode_grid,  api_dvr_autorec_grid },
    { "dvr/autorec/create",        ACCESS_RECORDER, api_dvr_autorec_create, NULL },
    { "dvr/autorec/create_by_series", ACCESS_RECORDER, api_dvr_autorec_create_by_series, NULL },

    { NULL },
  };

  api_register_all(ah);
}
