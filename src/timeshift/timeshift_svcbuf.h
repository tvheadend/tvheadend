/*
 *  TV headend - Timeshift - channel cache, raw MPEG-TS of a running service
 *  Copyright (C) 2026 Vincent Fortier
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

struct service;

typedef struct svcbuf svcbuf_t;
typedef struct svcbuf_gate svcbuf_gate_t;

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
  ( struct service *t, time_t from, streaming_target_t *output,
    int *replaying, time_t *replay_start, streaming_queue_t *sq );
/* Stop replaying, returns the gate's output.  s_stream_mutex held. */
streaming_target_t *svcbuf_gate_stop ( streaming_target_t *pad );
/* Free a stopped gate, s_stream_mutex not held */
void svcbuf_gate_destroy ( streaming_target_t *pad );

#endif /* __TVH_TIMESHIFT_SVCBUF_H__ */
