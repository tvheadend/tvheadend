/*
 *  Averaging functions
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef AVG_H
#define AVG_H

#include <pthread.h>
#include "queue.h"

/*
 * avg stat queue
 */

TAILQ_HEAD(avgstat_entry_queue, avgstat_entry);

typedef struct avgstat {
  pthread_mutex_t as_mutex;
  struct avgstat_entry_queue as_queue;
  int as_depth;  /* in seconds */
} avgstat_t;

typedef struct avgstat_entry {
  TAILQ_ENTRY(avgstat_entry) ase_link;
  int ase_clock;
  int ase_count;
} avgstat_entry_t;

void avgstat_init(avgstat_t *as, int maxdepth);
void avgstat_add(avgstat_t *as, int count, time_t now);
void avgstat_flush(avgstat_t *as);
unsigned int avgstat_read_and_expire(avgstat_t *as, time_t now);
unsigned int avgstat_read(avgstat_t *as, int depth, time_t now);

#endif /* AVG_H */
