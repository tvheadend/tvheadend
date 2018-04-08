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

#ifndef __TVH_TPROFILE_H__
#define __TVH_TPROFILE_H__

#define STRINGIFY(s) # s
#define SRCLINEID() SRCLINEID2(__FILE__, __LINE__)
#define SRCLINEID2(f,l) f ":" STRINGIFY(l)

typedef struct tprofile_avg tprofile_avg_t;
typedef struct tprofile_time tprofile_time_t;
typedef struct tprofile tprofile_t;
typedef struct tprofile_avg qprofile_avg_t;
typedef struct qprofile_time qprofile_time_t;
typedef struct qprofile qprofile_t;

struct tprofile_avg {
  uint64_t count;
  uint64_t avg;
};

struct tprofile_time {
  uint64_t t;
  char *id;
};

struct tprofile {
  LIST_ENTRY(tprofile) link;
  char *name;
  tprofile_time_t tmax;
  tprofile_avg_t tavg;
  uint64_t start;
  char *start_id;
  uint8_t changed;
  uint8_t finish;
};

struct qprofile_time {
  uint64_t t;
  uint64_t pos;
  char *id;
};

struct qprofile {
  LIST_ENTRY(qprofile) link;
  char *name;
  uint64_t qpos;
  qprofile_time_t qmax;
  tprofile_avg_t qavg;
  uint64_t qsum;
  uint64_t qdrop;
  uint64_t qdropcnt;
  uint64_t tstamp;
  uint8_t changed;
  uint8_t finish;
};

extern int tprofile_running;

void tprofile_init1(tprofile_t *tprof, const char *name);
void tprofile_done1(tprofile_t *tprof);
void tprofile_start1(tprofile_t *tprof, const char *id);
void tprofile_finish1(tprofile_t *tprof);

static inline void tprofile_init(tprofile_t *tprof, const char *name)
  { if (tprofile_running) tprofile_init1(tprof, name); }
static inline void tprofile_done(tprofile_t *tprof)
  { if (tprofile_running) tprofile_done1(tprof); }
static inline void tprofile_start(tprofile_t *tprof, const char *id)
  { if (tprofile_running) tprofile_start1(tprof, id); }
static inline void tprofile_finish(tprofile_t *tprof)
  { if (tprofile_running) tprofile_finish1(tprof); }

void tprofile_queue_init1(qprofile_t *qprof, const char *name);
void tprofile_queue_done1(qprofile_t *qprof);
void tprofile_queue_set1(qprofile_t *qprof, const char *id, uint64_t pos);
void tprofile_queue_add1(qprofile_t *qprof, const char *id, uint64_t pos);
void tprofile_queue_drop1(qprofile_t *qprof, const char *id, uint64_t pos);

static inline void tprofile_queue_init(qprofile_t *qprof, const char *name)
  { if (tprofile_running) tprofile_queue_init1(qprof, name); }
static inline void tprofile_queue_done(qprofile_t *qprof)
  { if (tprofile_running) tprofile_queue_done1(qprof); }
static inline void tprofile_queue_set(qprofile_t *qprof, const char *id, uint64_t pos)
  { if (tprofile_running) tprofile_queue_set1(qprof, id, pos); }
static inline void tprofile_queue_add(qprofile_t *qprof, const char *id, uint64_t pos)
  { if (tprofile_running) tprofile_queue_add1(qprof, id, pos); }
static inline void tprofile_queue_drop(qprofile_t *qprof, const char *id, uint64_t pos)
  { if (tprofile_running) tprofile_queue_drop1(qprof, id, pos); }

void tprofile_log_stats1(void);

static inline void tprofile_log_stats(void)
  { if (tprofile_running) tprofile_log_stats1(); }

char *tprofile_get_json_stats(void);

void tprofile_module_init(int enable);
void tprofile_module_done(void);

#endif /* __TVH_TPROFILE_H__ */
