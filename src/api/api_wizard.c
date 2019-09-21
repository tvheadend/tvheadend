/*
 *  API - Wizard interface
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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
#include "config.h"
#include "input.h"
#include "wizard.h"

static int
wizard_page ( const char *page )
{
  tvh_mutex_lock(&global_lock);
  if (strcmp(page, config.wizard ?: "")) {
    free(config.wizard);
    config.wizard = page[0] ? strdup(page) : NULL;
    idnode_changed(&config.idnode);
  }
  tvh_mutex_unlock(&global_lock);
  return 0;
}

static int
wizard_idnode_load_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int r;
  wizard_build_fcn_t fcn = opaque;
  wizard_page_t *page = fcn(perm->aa_lang_ui);
  r = api_idnode_load_simple(perm, &page->idnode, op, args, resp);
  wizard_page(page->name);
  page->free(page);
  return r;
}

static int
wizard_idnode_save_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int r;
  wizard_build_fcn_t fcn = opaque;
  wizard_page_t *page = fcn(perm->aa_lang_ui);
  r = api_idnode_save_simple(perm, &page->idnode, op, args, resp);
  idnode_save_check(&page->idnode, 1);
  page->free(page);
  return r;
}

static int
wizard_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return wizard_page("");
}

static int
wizard_start
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return wizard_page("hello");
}

static int
wizard_status_progress
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int64_t active = 0, total = 0, services = 0;
  mpegts_service_t *s;
  mpegts_mux_t *mm;
  mpegts_network_t *mn;

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (!mn->mn_wizard) continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      total++;
      if (mm->mm_scan_state != MM_SCAN_STATE_IDLE)
        active++;
      LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
        services++;
    }
  }
  *resp = htsmsg_create_map();
  htsmsg_add_dbl(*resp, "progress", total ? ((double)1.0 - ((double)active / (double)total)) : 1);
  htsmsg_add_s64(*resp, "muxes", total);
  htsmsg_add_s64(*resp, "services", services);
  return 0;
}

void
api_wizard_init ( void )
{
  static api_hook_t ah[] = {
    { "wizard/hello/load",   ACCESS_ADMIN, wizard_idnode_load_simple, wizard_hello },
    { "wizard/hello/save",   ACCESS_ADMIN, wizard_idnode_save_simple, wizard_hello },
    { "wizard/login/load",   ACCESS_ADMIN, wizard_idnode_load_simple, wizard_login },
    { "wizard/login/save",   ACCESS_ADMIN, wizard_idnode_save_simple, wizard_login },
    { "wizard/network/load", ACCESS_ADMIN, wizard_idnode_load_simple, wizard_network },
    { "wizard/network/save", ACCESS_ADMIN, wizard_idnode_save_simple, wizard_network },
    { "wizard/muxes/load",   ACCESS_ADMIN, wizard_idnode_load_simple, wizard_muxes },
    { "wizard/muxes/save",   ACCESS_ADMIN, wizard_idnode_save_simple, wizard_muxes },
    { "wizard/status/load",  ACCESS_ADMIN, wizard_idnode_load_simple, wizard_status },
    { "wizard/status/save",  ACCESS_ADMIN, wizard_idnode_save_simple, wizard_status },
    { "wizard/status/progress", ACCESS_ADMIN, wizard_status_progress, NULL },
    { "wizard/mapping/load", ACCESS_ADMIN, wizard_idnode_load_simple, wizard_mapping },
    { "wizard/mapping/save", ACCESS_ADMIN, wizard_idnode_save_simple, wizard_mapping },
    { "wizard/channels/load", ACCESS_ADMIN, wizard_idnode_load_simple, wizard_channels },
    { "wizard/channels/save", ACCESS_ADMIN, wizard_idnode_save_simple, wizard_channels },
    { "wizard/start",        ACCESS_ADMIN, wizard_start, NULL },
    { "wizard/cancel",       ACCESS_ADMIN, wizard_cancel, NULL },
    { NULL },
  };

  api_register_all(ah);
}
