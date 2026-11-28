/*
 *  TV headend - Timeshift - channel cache, raw MPEG-TS of a running service
 *  Copyright (C) 2026 Vincent Fortier
 *  Copyright (C) 2026 LiBe-net
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

#ifndef __TVH_TIMESHIFT_SVCBUF_H__
#define __TVH_TIMESHIFT_SVCBUF_H__

#include "streaming.h"
#include "htsmsg.h"

struct service;

typedef struct svcbuf svcbuf_t;
typedef struct svcbuf_gate svcbuf_gate_t;
typedef struct svcbuf_reader svcbuf_reader_t;

/* Service life cycle, s_stream_mutex held */
void svcbuf_service_start ( struct service *t );
svcbuf_t *svcbuf_service_stop ( struct service *t );
/* Release what svcbuf_service_stop() returned, s_stream_mutex not held */
void svcbuf_release ( svcbuf_t *sb );

/* Replay the channel cache from 'from', then pass live data on to
 * 'output'; *replaying is set while replaying and *replay_start to where
 * the replayed data begins, paced on the consumer's queue 'sq' when given.
 * NULL when the service is not cached.  s_stream_mutex held. */
streaming_target_t *svcbuf_gate_create
  ( struct service *t, time_t from, time_t to, int all,
    streaming_target_t *output, int *replaying,
    time_t *replay_start, streaming_queue_t *sq );
/* Stop replaying, returns the gate's output.  s_stream_mutex held. */
streaming_target_t *svcbuf_gate_stop ( streaming_target_t *pad );
/* Free a stopped gate, s_stream_mutex not held */
void svcbuf_gate_destroy ( streaming_target_t *pad );
void svcbuf_gate_set_stop ( streaming_target_t *pad, time_t to );

/* Readers, for clients timeshifting through the cache.  svcbuf_acquire()
 * takes a reference (s_stream_mutex held), svcbuf_release() drops it. */
svcbuf_t *svcbuf_acquire ( struct service *t );
struct service *svcbuf_service ( svcbuf_t *sb );
uint64_t svcbuf_start_seq ( svcbuf_t *sb );
int svcbuf_span ( svcbuf_t *sb, int64_t *oldest, int64_t *newest );

/* For the status pages.  svcbuf_status() describes one cache: where its
 * bytes live, the time it spans, how many readers and gates it has, and a
 * map of the span in `buckets` slots, each the place most of that slot's
 * data lives (SVCBUF_LOC_*).  svcbuf_gate_behind() is how far a recording's
 * replay is behind the newest cached data, 0 once it has caught up. */
#define SVCBUF_LOC_NONE    0    ///< Nothing cached for that slot: a gap
#define SVCBUF_LOC_RAM     1    ///< Kept in the timeshift RAM budget
#define SVCBUF_LOC_STAGING 2    ///< In RAM, waiting for the disk writer
#define SVCBUF_LOC_DISK    3    ///< Only in a segment file
htsmsg_t *svcbuf_status ( svcbuf_t *sb, int buckets );

/* The bytes of a cache whose blocks arrived within any of n wall-clock
 * ranges, given as start, stop pairs: by where they live (SVCBUF_LOC_*),
 * a block in several ranges counted once.  What recordings are made of. */
void svcbuf_bytes_within ( svcbuf_t *sb, const time_t *ranges, int n,
                           uint64_t bytes[4] );
int64_t svcbuf_gate_behind ( const streaming_target_t *pad );
uint64_t svcbuf_staging_used ( void );
uint64_t svcbuf_staging_limit ( void );
int svcbuf_count ( void );
svcbuf_reader_t *svcbuf_reader_create ( svcbuf_t *sb );
void svcbuf_reader_destroy ( svcbuf_reader_t *r );
int64_t svcbuf_reader_seek ( svcbuf_reader_t *r, int64_t mono );
int svcbuf_reader_read ( svcbuf_reader_t *r, pktbuf_t **pb );
int svcbuf_reader_read_map
  ( svcbuf_reader_t *r, pktbuf_t **pb, streaming_start_t **ss,
    uint64_t *start_seq, int64_t *block_mono );
int svcbuf_reader_lost ( svcbuf_reader_t *r );

#endif /* __TVH_TIMESHIFT_SVCBUF_H__ */
