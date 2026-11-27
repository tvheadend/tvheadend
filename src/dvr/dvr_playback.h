// SPDX-License-Identifier: GPL-3.0-or-later
/*
 *  Tvheadend - recorded media input
 *  Copyright (C) 2026 Tvheadend contributors
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

#ifndef TVH_DVR_PLAYBACK_H
#define TVH_DVR_PLAYBACK_H

#include "streaming.h"

typedef struct dvr_playback dvr_playback_t;

/* Times use 90 kHz. The caller serializes operations and owns returned data.
 * read returns 1 for a packet, 0 at EOF, or a negative libav error. */
int dvr_playback_open(dvr_playback_t **playback,
                      const char *filename,
                      int (*interrupt)(void *), void *opaque);
/* Follow callback waits for more bytes: positive retries, zero makes EOF final,
 * negative aborts this read (for seek/close). It runs on the reader's thread. */
void dvr_playback_follow(dvr_playback_t *playback, int (*wait)(void *), void *opaque);
/* Update the available duration after append, preserving the read cursor. */
int dvr_playback_refresh(dvr_playback_t *playback, const char *filename);
void dvr_playback_close(dvr_playback_t *playback);
streaming_start_t *dvr_playback_start(dvr_playback_t *playback);
int64_t dvr_playback_duration(dvr_playback_t *playback);
int64_t dvr_playback_origin(dvr_playback_t *playback);
int dvr_playback_primary(dvr_playback_t *playback, const th_pkt_t *packet);
int dvr_playback_read(dvr_playback_t *playback, th_pkt_t **packet);
int dvr_playback_seek(dvr_playback_t *playback, int64_t time, int64_t *landed);

#endif
