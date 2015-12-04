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
#include "wizard.h"

static int
wizard_idnode_load_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int r;
  wizard_build_fcn_t fcn = opaque;
  wizard_page_t *page = fcn();
  r = api_idnode_load_simple(perm, &page->idnode, op, args, resp);
  page->free(page);
  return r;
}

static int
wizard_idnode_save_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int r;
  wizard_build_fcn_t fcn = opaque;
  wizard_page_t *page = fcn();
  r = api_idnode_save_simple(perm, &page->idnode, op, args, resp);
  page->free(page);
  return r;
}

static int
wizard_page
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp, const char *page )
{
  pthread_mutex_lock(&global_lock);
  free(config.wizard);
  config.wizard = strdup(page);
  config_save();
  pthread_mutex_unlock(&global_lock);
  return 0;
}

static int
wizard_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return wizard_page(perm, opaque, op, args, resp, "");
}

static int
wizard_start
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return wizard_page(perm, opaque, op, args, resp, "hello");
}

void
api_wizard_init ( void )
{
  static api_hook_t ah[] = {
    { "wizard/hello/load",   ACCESS_ADMIN, wizard_idnode_load_simple, wizard_hello },
    { "wizard/hello/save",   ACCESS_ADMIN, wizard_idnode_save_simple, wizard_hello },
    { "wizard/network/load", ACCESS_ADMIN, wizard_idnode_load_simple, wizard_network },
    { "wizard/network/save", ACCESS_ADMIN, wizard_idnode_save_simple, wizard_network },
    { "wizard/input/load",   ACCESS_ADMIN, wizard_idnode_load_simple, wizard_input },
    { "wizard/input/save",   ACCESS_ADMIN, wizard_idnode_save_simple, wizard_input },
    { "wizard/status/load",  ACCESS_ADMIN, wizard_idnode_load_simple, wizard_status },
    { "wizard/status/save",  ACCESS_ADMIN, wizard_idnode_save_simple, wizard_status },
    { "wizard/mapping/load", ACCESS_ADMIN, wizard_idnode_load_simple, wizard_mapping },
    { "wizard/mapping/save", ACCESS_ADMIN, wizard_idnode_save_simple, wizard_mapping },
    { "wizard/start",        ACCESS_ADMIN, wizard_start, NULL },
    { "wizard/cancel",       ACCESS_ADMIN, wizard_cancel, NULL },
    { NULL },
  };

  api_register_all(ah);
}
