/*
 *  Tvheadend - TS file input system
 *
 *  Copyright (C) 2013 Adam Sutton
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
#include "input.h"
#include "channels.h"
#include "tsfile_private.h"
#include "service_mapper.h"

/*
 * Globals
 */
pthread_mutex_t          tsfile_lock;
mpegts_network_t         tsfile_network;
mpegts_input_list_t      tsfile_inputs;

extern const idclass_t mpegts_service_class;
extern const idclass_t mpegts_network_class;

/*
 * Network definition
 */
static mpegts_service_t *
tsfile_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  static int t = 0;
  pthread_mutex_lock(&tsfile_lock);
  mpegts_service_t *s = mpegts_service_create1(NULL, mm, sid, pmt_pid, NULL);
  pthread_mutex_unlock(&tsfile_lock);

  // TODO: HACK: REMOVE ME
  if (s) {
    char buf[128];
    sprintf(buf, "channel-%d", t);
    channel_t *c = channel_find_by_name(buf, 1, t);
    service_mapper_link((service_t*)s, c);
    t++;
  }
  return s;
}

/*
 * Initialise
 */
void tsfile_init ( int tuners )
{
  int i;
  mpegts_input_t *mi;

  /* Mutex - used for minor efficiency in service processing */
  pthread_mutex_init(&tsfile_lock, NULL);

  /* Shared network */
  mpegts_network_create0(&tsfile_network, &mpegts_network_class, NULL,
                         "TSfile Network", NULL);
  tsfile_network.mn_create_service = tsfile_network_create_service;

  /* IPTV like setup */
  if (tuners <= 0) {
    mi = tsfile_input_create(0);
    mpegts_input_set_network(mi, &tsfile_network);
  } else {
    for (i = 0; i < tuners; i++) {
      mi = tsfile_input_create(i+1);
      mpegts_input_set_network(mi, &tsfile_network);
    }
  }
}

/*
 * Add multiplex
 */
void tsfile_add_file ( const char *path )
{
  mpegts_input_t        *mi;
  mpegts_mux_t          *mm;
  tvhtrace("tsfile", "add file %s", path);

  /* Create logical instance */
  mm = tsfile_mux_create(&tsfile_network);
  
  /* Create physical instance (for each tuner) */
  LIST_FOREACH(mi, &tsfile_inputs, mi_global_link)
    tsfile_mux_instance_create(path, mi, mm);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
