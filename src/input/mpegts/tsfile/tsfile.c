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

#include "tsfile.h"
#include "tsfile_private.h"

static mpegts_network_t *tsfile_network;
LIST_HEAD(,mpegts_input) tsfile_inputs;

/*
 * Initialise
 */
void tsfile_init ( int tuners )
{
  int i;
  mpegts_input_t *mi;

  /* Shared network */
  tsfile_network = mpegts_network_create0(NULL, "tsfile network");

  /* Create inputs */
  for (i = 0; i < tuners; i++) {
    mi = tsfile_input_create(); 
    mi->mi_network = tsfile_network;
    LIST_INSERT_HEAD(&tsfile_inputs, mi, mi_global_link);
  }
}

/*
 * Add multiplex
 */
void tsfile_add_file ( const char *path )
{
  mpegts_input_t        *mi;
  mpegts_mux_t          *mm;
printf("tsfile_add_file(%s)\n", path);

  /* Create logical instance */
  mm = mpegts_mux_create0(NULL, tsfile_network, MM_ONID_NONE, MM_TSID_NONE);
  
  /* Create physical instance (for each tuner) */
  LIST_FOREACH(mi, &tsfile_inputs, mi_global_link)
    tsfile_mux_instance_create(path, mi, mm);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
