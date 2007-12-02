/*
 *  tvheadend, Plugin framework
 *  Copyright (C) 2007 Andreas Öman
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
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "tvhead.h"
#include "plugin.h"

struct th_plugin_list th_plugins;

/**
 *
 */
void *
plugin_aux_create(struct pluginaux_list *h, struct th_plugin *p, size_t siz)
{
  pluginaux_t *pa;

  pa = calloc(1, siz);
  pa->pa_plugin = p;
  LIST_INSERT_HEAD(h, pa, pa_link);
  return pa;
}


/**
 *
 */
void *
plugin_aux_find(struct pluginaux_list *h, struct th_plugin *p)
{
  pluginaux_t *pa;

  LIST_FOREACH(pa, h, pa_link)
    if(pa->pa_plugin == p)
      break;

  return pa;
}

/**
 *
 */
void
plugin_aux_destroy(void *pa)
{
  pluginaux_t *pa0 = pa;
  LIST_REMOVE(pa0, pa_link);
  free(pa);
}

/**
 *
 */
th_plugin_t *
plugin_alloc(const char *name, void *opaque, size_t minsiz)
{
  th_plugin_t *p;

  if(sizeof(th_plugin_t) < minsiz)
    return NULL;

  p = calloc(1, sizeof(th_plugin_t));

  p->thp_name = strdup(name);
  p->thp_opaque = opaque;

  LIST_INSERT_HEAD(&th_plugins, p, thp_link);

  return p;
}
