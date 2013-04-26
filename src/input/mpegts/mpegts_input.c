/*
 *  Tvheadend - MPEGTS input source
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

#include "input/mpegts.h"

#include <pthread.h>

const idclass_t mpegts_input_class =
{
  .ic_class      = "mpegts_input",
  .ic_caption    = "MPEGTS Input",
  .ic_properties = (const property_t[]){
  }
};

void
mpegts_input_recv_packets
  ( mpegts_input_t *mi, const uint8_t *tsb, size_t len,
    int64_t *pcr, uint16_t *pcr_pid )
{
}

mpegts_input_t*
mpegts_input_create0 ( const char *uuid )
{
  mpegts_input_t *mi = idnode_create(mpegts_input, uuid);

  /* Init mutex */
  pthread_mutex_init(&mi->mi_delivery_mutex, NULL);

  /* Init input thread control */
  mi->mi_thread_pipe.rd = mi->mi_thread_pipe.wr = -1;

  return mi;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
