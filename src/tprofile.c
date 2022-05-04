/*
 *  tvheadend, Gather timing statistics - profiling
 *  Copyright (C) 2018 Jaroslav Kysela
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

#include <assert.h>
#include <stdio.h>
#include "tvhlog.h"
#include "clock.h"
#include "tprofile.h"

int tprofile_running;
static tvh_mutex_t tprofile_mutex;
static tvh_mutex_t qprofile_mutex;
static LIST_HEAD(, tprofile) tprofile_all;
static LIST_HEAD(, qprofile) qprofile_all;

void tprofile_init1(tprofile_t *tprof, const char *name)
{
  memset(tprof, 0, sizeof(*tprof));
  tprof->name = strdup(name);
  tvh_mutex_lock(&tprofile_mutex);
  LIST_INSERT_HEAD(&tprofile_all, tprof, link);
  tvh_mutex_unlock(&tprofile_mutex);
}

static void tprofile_time_done(tprofile_time_t *tpt)
{
  free(tpt->id);
}

static void tprofile_avg_done(tprofile_avg_t *tpa)
{
}

static void tprofile_destroy(tprofile_t *tprof)
{
  free(tprof->name);
  free(tprof->start_id);
  tprofile_time_done(&tprof->tmax);
  tprofile_avg_done(&tprof->tavg);
}

void tprofile_done1(tprofile_t *tprof)
{
  tprofile_t *fin = malloc(sizeof(*fin));
  tvh_mutex_lock(&tprofile_mutex);
  LIST_REMOVE(tprof, link);
  if (fin) {
    *fin = *tprof;
    fin->changed = fin->finish = 1;
    LIST_INSERT_HEAD(&tprofile_all, fin, link);
  }
  tvh_mutex_unlock(&tprofile_mutex);
  if (!fin) tprofile_destroy(tprof);
}

void tprofile_start1(tprofile_t *tprof, const char *id)
{
  tvh_mutex_lock(&tprofile_mutex);
  assert(tprof->start == 0);
  tprof->start = getfastmonoclock();
  if (id) {
    tprof->start_id = strdup(id);
  } else {
    char buf[32];
    snprintf(buf, sizeof(buf), "[%p]", tprof);
    tprof->start_id = strdup(buf);
  }
  tvh_mutex_unlock(&tprofile_mutex);
}

static void
tprofile_time_replace(tprofile_time_t *tpt, const char *id, uint64_t t)
{
  free(tpt->id);
  tpt->id = strdup(id);
  tpt->t = t;
}

static void
tprofile_avg_update(tprofile_avg_t *tpa, uint64_t val)
{
  tpa->avg = (tpa->avg * tpa->count + val) / (tpa->count + 1);
  tpa->count++;
}

void tprofile_finish1(tprofile_t *tprof)
{
  if (tprof->start) {
    uint64_t mono, diff;
    tvh_mutex_lock(&tprofile_mutex);
    mono = getfastmonoclock();
    diff = mono - tprof->start;
    if (diff > tprof->tmax.t)
      tprofile_time_replace(&tprof->tmax, tprof->start_id, diff);
    tprofile_avg_update(&tprof->tavg, diff);
    free(tprof->start_id);
    tprof->start_id = NULL;
    tprof->start = 0;
    tprof->changed = 1;
    tvh_mutex_unlock(&tprofile_mutex);
  }
}

void tprofile_queue_init1(qprofile_t *qprof, const char *name)
{
  memset(qprof, 0, sizeof(*qprof));
  qprof->name = strdup(name);
  tvh_mutex_lock(&qprofile_mutex);
  LIST_INSERT_HEAD(&qprofile_all, qprof, link);
  tvh_mutex_unlock(&qprofile_mutex);
}

static void qprofile_time_done(qprofile_time_t *qpt)
{
  free(qpt->id);
}

static void qprofile_avg_done(tprofile_avg_t *tpa)
{
}

static void qprofile_destroy(qprofile_t *qprof)
{
  free(qprof->name);
  qprofile_time_done(&qprof->qmax);
  qprofile_avg_done(&qprof->qavg);
}

void tprofile_queue_done1(qprofile_t *qprof)
{
  qprofile_t *fin = malloc(sizeof(*fin));
  tvh_mutex_lock(&qprofile_mutex);
  LIST_REMOVE(qprof, link);
  if (fin) {
    *fin = *qprof;
    fin->changed = fin->finish = 1;
    LIST_INSERT_HEAD(&qprofile_all, fin, link);
  }
  tvh_mutex_unlock(&qprofile_mutex);
  if (!fin) qprofile_destroy(qprof);
}

static void
qprofile_time_replace(qprofile_time_t *qpt, const char *id, uint64_t t, uint64_t pos)
{
  free(qpt->id);
  qpt->id = strdup(id);
  qpt->t = t;
  qpt->pos = pos;
}

static inline void
qprofile_avg_update(qprofile_avg_t *tpa, uint64_t val)
{
  tprofile_avg_update((tprofile_avg_t *)tpa, val);
}

#include <stdio.h>

void tprofile_queue_set1(qprofile_t *qprof, const char *id, uint64_t pos)
{
  uint64_t mono;
  tvh_mutex_lock(&qprofile_mutex);
  mono = getfastmonoclock();
  if (pos > qprof->qmax.pos)
    qprofile_time_replace(&qprof->qmax, id, mono, pos);
  qprofile_avg_update(&qprof->qavg, pos);
  qprof->changed = 1;
  tvh_mutex_unlock(&qprofile_mutex);
}

void tprofile_queue_add1(qprofile_t *qprof, const char *id, uint64_t count)
{
  tvh_mutex_lock(&qprofile_mutex);
  qprof->qsum += count;
  tvh_mutex_unlock(&qprofile_mutex);
}

void tprofile_queue_drop1(qprofile_t *qprof, const char *id, uint64_t count)
{
  tvh_mutex_lock(&qprofile_mutex);
  qprof->qdrop += count;
  qprof->qdropcnt++;
  tvh_mutex_unlock(&qprofile_mutex);
}

static void tprofile_log_tstats(void)
{
  tprofile_t *tprof, *tprof_next;

  tvh_mutex_lock(&tprofile_mutex);
  for (tprof = LIST_FIRST(&tprofile_all); tprof; tprof = tprof_next) {
    tprof_next = LIST_NEXT(tprof, link);
    if (tprof->changed == 0) continue;
    tvhtrace(LS_TPROF, "%s: max/avg/cnt=%"PRIu64"ms/%"PRIu64"ms/%"PRIu64" (max=%s)%s",
             tprof->name,
             mono2ms(tprof->tmax.t),
             mono2ms(tprof->tavg.avg), tprof->tavg.count,
             tprof->tmax.id ?: "?",
             tprof->finish ? " destroyed" : "");
    if (tprof->finish) {
      LIST_REMOVE(tprof, link);
      tprofile_destroy(tprof);
      free(tprof);
      continue;
    }
    tprof->changed = 0;
  }
  tvh_mutex_unlock(&tprofile_mutex);
}

static void qprofile_log_qstats(void)
{
  qprofile_t *qprof, *qprof_next;
  uint64_t mono, diff;

  tvh_mutex_lock(&qprofile_mutex);
  mono = getfastmonoclock();
  for (qprof = LIST_FIRST(&qprofile_all); qprof; qprof = qprof_next) {
    qprof_next = LIST_NEXT(qprof, link);
    if (qprof->changed == 0) continue;
    diff = qprof->tstamp ? mono - qprof->tstamp : 1;
    if (diff == 0) diff = 1;
    tvhtrace(LS_QPROF, "%s: max/avg/cnt/drop/dropcnt=%"
                       PRIu64"/%"PRIu64"/%"PRIu64"/%"PRIu64"/%"PRIu64
                       " (max=%s -%"PRId64"sec) BW=%"PRId64"%s",
             qprof->name,
             qprof->qmax.pos,
             qprof->qavg.avg, qprof->qavg.count,
             qprof->qdrop, qprof->qdropcnt,
             qprof->qmax.id, mono2sec(mono - qprof->qmax.t),
             sec2mono(qprof->qsum) / diff,
             qprof->finish ? " destroyed" : "");
    if (qprof->finish) {
      LIST_REMOVE(qprof, link);
      qprofile_destroy(qprof);
      free(qprof);
      continue;
    }
    qprof->changed = 0;
    qprof->qsum = 0;
    qprof->tstamp = mono;
  }
  tvh_mutex_unlock(&qprofile_mutex);
}

void tprofile_log_stats1(void)
{
  tprofile_log_tstats();
  qprofile_log_qstats();
}

char *tprofile_get_json_stats(void)
{
  return NULL;
}

void tprofile_module_init(int enable)
{
  tprofile_running = enable;
  tvh_mutex_init(&tprofile_mutex, NULL);
  tvh_mutex_init(&qprofile_mutex, NULL);
}

void tprofile_module_done(void)
{
  tprofile_log_stats1(); /* destroy all items in the finish state */
  assert(LIST_FIRST(&tprofile_all) == NULL);
  assert(LIST_FIRST(&qprofile_all) == NULL);
}
