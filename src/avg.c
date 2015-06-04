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

#include "avg.h"
#include <stdlib.h>

void
avgstat_init(avgstat_t *as, int depth)
{
  TAILQ_INIT(&as->as_queue);
  pthread_mutex_init(&as->as_mutex, NULL);
  as->as_depth = depth;
}


void
avgstat_flush(avgstat_t *as)
{
  avgstat_entry_t *ase;

  while((ase = TAILQ_FIRST(&as->as_queue)) != NULL) {
    TAILQ_REMOVE(&as->as_queue, ase, ase_link);
    free(ase);
  }
}


static void
avgstat_expire(avgstat_t *as, int now)
{
  avgstat_entry_t *ase;

  while(1) {
    ase = TAILQ_LAST(&as->as_queue, avgstat_entry_queue);
    if(ase == NULL || ase->ase_clock > now - as->as_depth) 
      break;
    TAILQ_REMOVE(&as->as_queue, ase, ase_link);
    free(ase);
  }
}


void
avgstat_add(avgstat_t *as, int count, time_t now)
{
  avgstat_entry_t *ase;

  pthread_mutex_lock(&as->as_mutex);
  ase = TAILQ_FIRST(&as->as_queue);
  if(ase != NULL && ase->ase_clock == now) {
    ase->ase_count += count;
  } else {
    ase = malloc(sizeof(avgstat_entry_t));
    TAILQ_INSERT_HEAD(&as->as_queue, ase, ase_link);
    ase->ase_clock = now;
    ase->ase_count = count;
  }

  avgstat_expire(as, now);
  pthread_mutex_unlock(&as->as_mutex);
}


unsigned int
avgstat_read_and_expire(avgstat_t *as, time_t now)
{
  avgstat_entry_t *ase;
  int r = 0;

  pthread_mutex_lock(&as->as_mutex);

  avgstat_expire(as, now);

  TAILQ_FOREACH(ase, &as->as_queue, ase_link)
    r += ase->ase_count;

  pthread_mutex_unlock(&as->as_mutex);
  return r;
}

unsigned int
avgstat_read(avgstat_t *as, int depth, time_t now)
{
  avgstat_entry_t *ase;
  int r = 0;

  pthread_mutex_lock(&as->as_mutex);

  TAILQ_FOREACH(ase, &as->as_queue, ase_link) {
    if(ase->ase_clock < now - depth)
      break;
    r += ase->ase_count;
  }
  pthread_mutex_unlock(&as->as_mutex);
  return r;
}

