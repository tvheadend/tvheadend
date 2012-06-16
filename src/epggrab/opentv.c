/*
 *  Electronic Program Guide - eit grabber
 *  Copyright (C) 2012 Adam Sutton
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

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "tvheadend.h"
#include "dvb/dvb.h"
#include "channels.h"
#include "huffman.h"
#include "epg.h"
#include "epggrab/opentv.h"
#include "subscriptions.h"
#include "service.h"

/* ************************************************************************
 * Module Setup
 * ***********************************************************************/

static epggrab_module_t _opentv_mod;
static huffman_node_t  *_opentv_dict;

static void* _opentv_thread ( void *p )
{
  int err;
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  service_t *svc = NULL;

  pthread_mutex_lock(&global_lock);

  printf("find service\n");
  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
      if ((svc = dvb_transport_find(tdmi, 4152, 0, NULL))) break;
    }
  }

  /* start */
  printf("found svc = %p\n", svc);
  if(svc) err = service_start(svc, 1, 0);
  printf("service_start = %d\n", err);
  
  pthread_mutex_unlock(&global_lock);

  while (1) {
    sleep(10);
  }
  return NULL;
}

static int _opentv_enable ( epggrab_module_t *m, uint8_t e )
{
  pthread_t tid;
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid, &tattr, _opentv_thread, NULL);
  return 0;
}

void opentv_init ( epggrab_module_list_t *list )
{
  _opentv_mod.id     = strdup("opentv");
  _opentv_mod.name   = strdup("OpenTV EPG");
  _opentv_mod.enable = _opentv_enable;
  *((uint8_t*)&_opentv_mod.flags) = EPGGRAB_MODULE_EXTERNAL; // TODO: hack
  LIST_INSERT_HEAD(list, &_opentv_mod, link);
  // Note: this is mostly ignored anyway as EIT is treated as a special case!
}

void opentv_load ( void )
{
  _opentv_dict = huffman_tree_load("epggrab/opentv/dict/skyuk");
  assert(_opentv_dict);
  tvhlog(LOG_INFO, "opentv", "huffman tree loaded");
}
