/*
 *  Tvheadend - TS file private data
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

#ifndef __TVH_TSFILE_PRIVATE_H__
#define __TVH_TSFILE_PRIVATE_H__

#include "input.h"

/*
 * Typedefs
 */
typedef struct tsfile_input        tsfile_input_t;
typedef struct tsfile_mux_instance tsfile_mux_instance_t;
typedef LIST_HEAD(,tsfile_input)   tsfile_input_list_t;

/*
 * Globals
 */
extern mpegts_network_t    *tsfile_network;
extern tsfile_input_list_t tsfile_inputs;
extern tvh_mutex_t     tsfile_lock;


/*
 * Mux instance
 */
struct tsfile_mux_instance
{
  mpegts_mux_instance_t; ///< Parent obj

  /*
   * Timing
   */
  

  /*
   * File input
   */
  
  char     *mmi_tsfile_path;    ///< Source file path
  uint16_t  mmi_tsfile_pcr_pid; ///< Timing control
};

/*
 * TS file input
 */
struct tsfile_input
{
  mpegts_input_t;

  LIST_ENTRY(tsfile_input) tsi_link;
  th_pipe_t  ti_thread_pipe;
  pthread_t  ti_thread_id;
};

/*
 * Prototypes
 */
tsfile_input_t        *tsfile_input_create ( int idx );

tsfile_mux_instance_t *tsfile_mux_instance_create
  ( const char *path, mpegts_input_t *mi, mpegts_mux_t *mm );

mpegts_mux_t *
tsfile_mux_create ( const char *uuid, mpegts_network_t *mn );

#endif /* __TVH_TSFILE_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
