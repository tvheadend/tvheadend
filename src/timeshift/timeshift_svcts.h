/*
 *  TV headend - Timeshift - clients timeshifting through the channel cache
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

#ifndef __TVH_TIMESHIFT_SVCTS_H__
#define __TVH_TIMESHIFT_SVCTS_H__

#include "streaming.h"

struct service;

/* Clients timeshift through the channel's cache rather than their own */
int svcts_enabled ( void );

/* In a client chain, where timeshift_create() would go */
streaming_target_t *svcts_create ( streaming_target_t *out, time_t max_period );
void svcts_destroy ( streaming_target_t *pad );

/* The client's subscription linked to / unlinked from a service
 * (s_stream_mutex held) */
void svcts_attach ( streaming_target_t *pad, struct service *t );
void svcts_detach ( streaming_target_t *pad );

#endif /* __TVH_TIMESHIFT_SVCTS_H__ */
