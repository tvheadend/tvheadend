/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * API - epggrab related calls
 */

#include "tvheadend.h"
#include "access.h"
#include "api.h"
#include "epggrab.h"

static void
api_epggrab_channel_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  epggrab_channel_t *ec;

  TAILQ_FOREACH(ec, &epggrab_channel_entries, all_link)
    idnode_set_add(ins, (idnode_t*)ec, &conf->filter, perm->aa_lang_ui);
}

static int
api_epggrab_module_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *l = htsmsg_create_list(), *m;
  epggrab_module_t *mod;
  char buf[384];
  tvh_mutex_lock(&global_lock);
  LIST_FOREACH(mod, &epggrab_modules, link) {
    m = htsmsg_create_map();
    htsmsg_add_uuid(m, "uuid", &mod->idnode.in_uuid);
    htsmsg_add_str(m, "status", epggrab_module_get_status(mod));
    htsmsg_add_str(m, "title", idnode_get_title(&mod->idnode, perm->aa_lang_ui, buf, sizeof(buf)));
    htsmsg_add_msg(l, NULL, m);
  }
  tvh_mutex_unlock(&global_lock);
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  return 0;
}

static int
api_epggrab_ota_trigger
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int32_t s32;
  if (htsmsg_get_s32(args, "trigger", &s32))
    return EINVAL;
  if (s32 > 0) {
    tvh_mutex_lock(&global_lock);
    epggrab_ota_trigger(s32);
    tvh_mutex_unlock(&global_lock);
  }
  return 0;
}

static int
api_epggrab_rerun_internal
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int32_t s32;
  if (htsmsg_get_s32(args, "rerun", &s32))
    return EINVAL;
  if (s32 > 0) {
    tvh_mutex_lock(&global_lock);
    epggrab_rerun_internal();
    tvh_mutex_unlock(&global_lock);
  }
  return 0;
}

void api_epggrab_init ( void )
{
  static api_hook_t ah[] = {
    { "epggrab/channel/list", ACCESS_ANONYMOUS, api_idnode_load_by_class, (void*)&epggrab_channel_class },
    { "epggrab/channel/class", ACCESS_ADMIN, api_idnode_class, (void*)&epggrab_channel_class },
    { "epggrab/channel/grid", ACCESS_ADMIN, api_idnode_grid, api_epggrab_channel_grid },

    { "epggrab/module/list",  ACCESS_ADMIN, api_epggrab_module_list, NULL },
    { "epggrab/config/load",  ACCESS_ADMIN, api_idnode_load_simple, &epggrab_conf.idnode },
    { "epggrab/config/save",  ACCESS_ADMIN, api_idnode_save_simple, &epggrab_conf.idnode },
    { "epggrab/ota/trigger",  ACCESS_ADMIN, api_epggrab_ota_trigger, NULL },
    { "epggrab/internal/rerun",  ACCESS_ADMIN, api_epggrab_rerun_internal, NULL },
    { NULL },
  };

  api_register_all(ah);
}
