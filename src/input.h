/*
 *  Tvheadend
 *  Copyright (C) 2007 - 2010 Andreas Öman
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
#include "streaming.h"

struct htsmsg;
struct mpegts_apids;

/*
 * Type-defs
 */
typedef struct tvh_hardware           tvh_hardware_t;
typedef struct tvh_input              tvh_input_t;
typedef struct tvh_input_instance     tvh_input_instance_t;
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
  int signal; ///< signal strength, value depending on signal_scale value:
              ///<  - SCALE_RELATIVE : 0...65535 (which means 0%...100%)
              ///<  - SCALE DECIBEL  : 0.0001 dBm units. This value is generally negative.
  int snr;    ///< signal to noise ratio, value depending on snr_scale value:
              ///<  - SCALE_RELATIVE : 0...65535 (which means 0%...100%)
              ///<  - SCALE DECIBEL  : 0.0001 dB units.
  int ber;    ///< bit error rate (driver/vendor specific value!)
  int unc;    ///< number of uncorrected blocks
  int bps;    ///< bandwidth (bps)
  int cc;     ///< number of continuity errors
  int te;     ///< number of transport errors

  signal_status_scale_t signal_scale;
  signal_status_scale_t snr_scale;

  /* Note: if tc_bit > 0, BER = ec_bit / tc_bit (0...1) else BER = ber (driver specific value) */
  int ec_bit;    ///< ERROR_BIT_COUNT (same as unc?)
  int tc_bit;    ///< TOTAL_BIT_COUNT

  /* Note: PER = ec_block / tc_block (0...1) */
  int ec_block;  ///< ERROR_BLOCK_COUNT
  int tc_block;  ///< TOTAL_BLOCK_COUNT
};

struct tvh_input_stream {

  LIST_ENTRY(tvh_input_stream) link;

  char *uuid;         ///< Unique ID of the entry (used for updates)
  char *input_name;   ///< Name of the parent input
  char *stream_name;  ///< Name for this stream
  int   subs_count;   ///< Number of subcscriptions
  int   max_weight;   ///< Current max weight

  struct mpegts_apids *pids; ///< active PID list

  tvh_input_stream_stats_t stats;
};

/*
 * Generic input super-class
 */
struct tvh_input {
  idnode_t ti_id;

  LIST_ENTRY(tvh_input) ti_link;

  void (*ti_get_streams) (tvh_input_t *, tvh_input_stream_list_t*);
  void (*ti_clear_stats) (tvh_input_t *);

  struct htsmsg *(*ti_wizard_get) (tvh_input_t *, const char *);
  void (*ti_wizard_set)  (tvh_input_t *, struct htsmsg *, const char *);
};

/*
 * Generic input instance super-class
 */
struct tvh_input_instance {
  idnode_t tii_id;

  LIST_ENTRY(tvh_input_instance) tii_input_link;

  tvh_mutex_t              tii_stats_mutex;
  tvh_input_stream_stats_t tii_stats;

  void (*tii_delete) (tvh_input_instance_t *tii);
  void (*tii_clear_stats) (tvh_input_instance_t *tii);
};

/*
 * Generic hardware super-class
 */
struct tvh_hardware {
  idnode_t                     th_id;
  LIST_ENTRY(tvh_hardware)     th_link;
};

void tvh_hardware_init(void);

void *tvh_hardware_create0
  ( void *o, const idclass_t *idc, const char *uuid, htsmsg_t *conf );
void tvh_hardware_delete ( tvh_hardware_t *th );

/*
 * Class and Global list defs
 */
extern const idclass_t tvh_input_class;
extern const idclass_t tvh_input_instance_class;

extern tvh_input_list_t    tvh_inputs;
extern tvh_hardware_list_t tvh_hardware;

#define TVH_INPUT_FOREACH(x) LIST_FOREACH(x, &tvh_inputs, ti_link)
#define TVH_HARDWARE_FOREACH(x) LIST_FOREACH(x, &tvh_hardware, th_link)

/*
 * Methods
 */

htsmsg_t * tvh_input_stream_create_msg ( tvh_input_stream_t *st );

void tvh_input_stream_destroy ( tvh_input_stream_t *st );

static inline tvh_input_t *
tvh_input_find_by_uuid(const char *uuid)
  { return (tvh_input_t*)idnode_find(uuid, &tvh_input_class, NULL); }

static inline tvh_input_instance_t *
tvh_input_instance_find_by_uuid(const char *uuid)
  { return (tvh_input_instance_t*)idnode_find(uuid, &tvh_input_instance_class, NULL); }

void tvh_input_instance_clear_stats ( tvh_input_instance_t *tii );

/*
 * Input subsystem includes
 */

#if ENABLE_MPEGTS
#include "input/mpegts.h"
#include "input/mpegts/mpegts_mux_sched.h"
#include "input/mpegts/mpegts_network_scan.h"
#if ENABLE_MPEGTS_DVB
#include "input/mpegts/mpegts_dvb.h"
#endif
#if ENABLE_TSFILE
#include "input/mpegts/tsfile.h"
#endif
#if ENABLE_IPTV
#include "input/mpegts/iptv.h"
#endif
#if ENABLE_LINUXDVB
#include "input/mpegts/linuxdvb.h"
#endif
#if ENABLE_SATIP_CLIENT
#include "input/mpegts/satip/satip.h"
#endif
#if ENABLE_HDHOMERUN_CLIENT
#include "input/mpegts/tvhdhomerun/tvhdhomerun.h"
#endif
#endif

#endif /* __TVH_INPUT_H__ */
