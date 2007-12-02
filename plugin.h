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

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include "tvhead.h"

LIST_HEAD(th_plugin_list, th_plugin);

extern struct th_plugin_list th_plugins;
struct th_plugin;

typedef struct th_plugin {
  
  LIST_ENTRY(th_plugin) thp_link;

  void *thp_opaque;

  const char *thp_name;

  void (*thp_transport_start)(struct th_plugin *p, struct pluginaux_list *h, 
			      th_transport_t *t);

  void (*thp_transport_stop)(struct th_plugin *p, struct pluginaux_list *h, 
			     th_transport_t *t);

  void (*thp_psi_table)(struct th_plugin *p, struct pluginaux_list *h, 
			th_transport_t *t, struct th_stream *st,
			const uint8_t *data, size_t len);

  int (*thp_tsb_process)(pluginaux_t *pa, th_transport_t *t,
			 struct th_stream *st, uint8_t *tsb);
  
} th_plugin_t;

void *plugin_aux_create(struct pluginaux_list *h, struct th_plugin *p,
			size_t siz);

void *plugin_aux_find(struct pluginaux_list *h, struct th_plugin *p);

void plugin_aux_destroy(void *pa);

th_plugin_t *plugin_alloc(const char *name, void *opaque, size_t minsiz);

#endif /* PLUGIN_H_ */
