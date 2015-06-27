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
mpegts_network_t         *tsfile_network;
tsfile_input_list_t      tsfile_inputs;

extern const idclass_t mpegts_service_class;
extern const idclass_t mpegts_network_class;

static void
tsfile_service_config_save ( service_t *s )
{
}

/*
 * Network definition
 */
static mpegts_service_t *
tsfile_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  pthread_mutex_lock(&tsfile_lock);
  mpegts_service_t *s = mpegts_service_create1(NULL, mm, sid, pmt_pid, NULL);

  // TODO: HACK: REMOVE ME
  if (s) {
    s->s_config_save = tsfile_service_config_save;
    pthread_mutex_unlock(&tsfile_lock);
    channel_t *c = channel_create(NULL, NULL, NULL);
    service_mapper_link((service_t*)s, c, NULL);
  }
  else
    pthread_mutex_unlock(&tsfile_lock);

  return s;
}

/*
 * Initialise
 */
void tsfile_init ( int tuners )
{
  int i;
  tsfile_input_t *mi;

  /* Mutex - used for minor efficiency in service processing */
  pthread_mutex_init(&tsfile_lock, NULL);

  /* Shared network */
  tsfile_network = calloc(1, sizeof(*tsfile_network));
  mpegts_network_create0(tsfile_network, &mpegts_network_class, NULL,
                         "TSfile Network", NULL);
  tsfile_network->mn_create_service = tsfile_network_create_service;

  /* IPTV like setup */
  if (tuners <= 0) {
    mi = tsfile_input_create(0);
    mpegts_input_add_network((mpegts_input_t*)mi, tsfile_network);
  } else {
    for (i = 0; i < tuners; i++) {
      mi = tsfile_input_create(i+1);
      mpegts_input_add_network((mpegts_input_t*)mi, tsfile_network);
    }
  }
}

/*
 * Shutdown
 */
void
tsfile_done ( void )
{
  tsfile_input_t *mi;
  pthread_mutex_lock(&global_lock);
  while ((mi = LIST_FIRST(&tsfile_inputs))) {
    LIST_REMOVE(mi, tsi_link);
    mpegts_input_stop_all((mpegts_input_t*)mi);
    mpegts_input_delete((mpegts_input_t*)mi, 0);
    // doesn't close the pipe!
  }
  mpegts_network_class_delete(&mpegts_network_class, 1);
  pthread_mutex_unlock(&global_lock);
}

/*
 * Add multiplex
 */
void tsfile_add_file ( const char *path )
{
  tsfile_input_t        *mi;
  mpegts_mux_t          *mm;
  char *uuid = NULL, *tok;

  char tmp[strlen(path) + 1];
  strcpy(tmp, path);

  /* Pull UUID from info */
  if ((tok = strstr(tmp, "::"))) {
    *tok = '\0';
    path = tok + 2;
    uuid = tmp;
  }

  tvhtrace("tsfile", "add file %s (uuid:%s)", path, uuid);
  
  /* Create logical instance */
  mm = tsfile_mux_create(uuid, tsfile_network);
  
  /* Create physical instance (for each tuner) */
  LIST_FOREACH(mi, &tsfile_inputs, tsi_link)
    tsfile_mux_instance_create(path, (mpegts_input_t*)mi, mm);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
