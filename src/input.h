/*
 *  TVheadend
 *  Copyright (C) 2007 - 2010 Andreas �man
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

#ifndef __TVH_INPUT_H__
#define __TVH_INPUT_H__

#include "idnode.h"
#include "queue.h"

/*
 * Type-defs
 */
typedef struct tvh_hardware           tvh_hardware_t;
typedef struct tvh_input              tvh_input_t;
typedef struct tvh_input_stream       tvh_input_stream_t;
typedef struct tvh_input_stream_stats tvh_input_stream_stats_t;

typedef LIST_HEAD(,tvh_hardware)      tvh_hardware_list_t;
typedef LIST_HEAD(,tvh_input)         tvh_input_list_t;
typedef LIST_HEAD(,tvh_input_stream)  tvh_input_stream_list_t;

/*
 * Input stream structure - used for getting statistics about active streams
 */
struct tvh_input_stream_stats
{
  int signal; ///< Signal level (0-100)
  int ber;    ///< Bit error rate (0-100?)
  int unc;    ///< Uncorrectable errors
  int snr;    ///< Signal 2 Noise (dB)
  int bps;    ///< Bandwidth (bps)
};

struct tvh_input_stream {

  LIST_ENTRY(tvh_input_stream) link;

  char *uuid;         ///< Unique ID of the entry (used for updates)
  char *input_name;   ///< Name of the parent input
  char *stream_name;  ///< Name for this stream
  int   subs_count;   ///< Number of subcscriptions
  int   max_weight;   ///< Current max weight

  tvh_input_stream_stats_t stats;
};

/*
 * Generic input super-class
 */
typedef struct tvh_input {
  idnode_t ti_id;

  LIST_ENTRY(tvh_input) ti_link;

  void (*ti_get_streams) (struct tvh_input *, tvh_input_stream_list_t*);

} tvh_input_t;

/*
 * Generic hardware super-class
 */
typedef struct tvh_hardware {
  idnode_t                     th_id;
  LIST_ENTRY(tvh_hardware)     th_link;
} tvh_input_hw_t;


/*
 * Class and Global list defs
 */
extern const idclass_t tvh_input_class;

tvh_input_list_t    tvh_inputs;
tvh_hardware_list_t tvh_hardware;

#define TVH_INPUT_FOREACH(x) LIST_FOREACH(x, &tvh_inputs, ti_link)
#define TVH_HARDWARE_FOREACH(x) LIST_FOREACH(x, &tvh_hardware, th_link)

/*
 * Methods
 */

void input_init ( void );

htsmsg_t * tvh_input_stream_create_msg ( tvh_input_stream_t *st );

void tvh_input_stream_destroy ( tvh_input_stream_t *st );

/*
 * Input subsystem includes
 */

#if ENABLE_MPEGPS
#include "input/mpegps.h"
#endif

#if ENABLE_MPEGTS
#include "input/mpegts.h"
#if ENABLE_TSFILE
#include "input/mpegts/tsfile.h"
#endif
#if ENABLE_IPTV
#include "input/mpegts/iptv.h"
#endif
#if ENABLE_LINUXDVB
#include "input/mpegts/linuxdvb.h"
#endif
#endif

#endif /* __TVH_INPUT_H__ */
