/*
 *  tvheadend, continuous timestamps for pass recordings
 *  Copyright (C) 2026 Bernhard Berger
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

#ifndef PASS_TIMELINE_H_
#define PASS_TIMELINE_H_

#include <stddef.h>
#include <stdint.h>

#define PASS_TIMELINE_NO_PCR 0x1fff

typedef struct pass_timeline pass_timeline_t;

/* Return nonzero on output failure. Name and opaque remain owned by caller. */
typedef int (*pass_timeline_write_cb_t)(void *opaque, const void *data, size_t size);
pass_timeline_t *pass_timeline_create(const char *name,
                                      pass_timeline_write_cb_t write_cb, void *opaque);
void pass_timeline_write(pass_timeline_t *t, const uint8_t *data, size_t size, unsigned pcr_pid);
void pass_timeline_drain(pass_timeline_t *t);
void pass_timeline_destroy(pass_timeline_t *t);

#endif /* PASS_TIMELINE_H_ */
