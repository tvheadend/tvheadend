/*
 *  Tvheadend - logging
 *  Copyright (C) 2012 Adam Sutton
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
#ifndef __TVH_LOGGING_H__
#define __TVH_LOGGING_H__

#include <sys/types.h>
#include "build.h"
#if ENABLE_ANDROID
#include <syslog.h>
#else
#include <sys/syslog.h>
#endif
#include <pthread.h>
#include <stdarg.h>
#include <time.h>

#include "atomic.h"
#include "clock.h"
#include "htsmsg.h"

typedef struct {
  int64_t last;
  size_t count;
} tvhlog_limit_t;

/* Config */
extern int              tvhlog_level;
extern htsmsg_t        *tvhlog_debug;
extern htsmsg_t        *tvhlog_trace;
extern char            *tvhlog_path;
extern int              tvhlog_options;
extern pthread_mutex_t  tvhlog_mutex;

/* Initialise */
void tvhlog_init       ( int level, int options, const char *path ); 
void tvhlog_start      ( void );
void tvhlog_end        ( void );
void tvhlog_set_debug  ( const char *subsys );
void tvhlog_get_debug  ( char *subsys, size_t len );
void tvhlog_set_trace  ( const char *subsys );
void tvhlog_get_trace  ( char *subsys, size_t len );
void tvhlogv           ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys, const char *fmt, va_list *args );
void _tvhlog           ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys, const char *fmt, ... )
  __attribute__((format(printf,6,7)));
void _tvhlog_hexdump   ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys,
                         const uint8_t *data, ssize_t len );
static inline void tvhlog_limit_reset ( tvhlog_limit_t *limit )
  { limit->last = 0; limit->count = 0; }
static inline int tvhlog_limit ( tvhlog_limit_t *limit, uint32_t delay )
  { int64_t t = mclk(); limit->count++;
    if (limit->last + sec2mono(delay) < t) { limit->last = t; return 1; }
    return 0; }


/* Options */
#define TVHLOG_OPT_DBG_SYSLOG   0x0001
#define TVHLOG_OPT_DBG_STDERR   0x0002
#define TVHLOG_OPT_DBG_FILE     0x0004
#define TVHLOG_OPT_SYSLOG       0x0010
#define TVHLOG_OPT_STDERR       0x0020
#define TVHLOG_OPT_MILLIS       0x0100
#define TVHLOG_OPT_DECORATE     0x0200
#define TVHLOG_OPT_FILELINE     0x0400
#define TVHLOG_OPT_THREAD       0x0800
#define TVHLOG_OPT_LIBAV        0x1000
#define TVHLOG_OPT_ALL          0xFFFF

/* Levels */
#ifndef LOG_TRACE
#define LOG_TRACE (LOG_DEBUG+1)
#endif

/* Macros */
#define tvhlog(severity, subsys, fmt, ...)\
  _tvhlog(__FILE__, __LINE__, 1, severity, subsys, fmt, ##__VA_ARGS__)
#define tvhlog_spawn(severity, subsys, fmt, ...)\
  _tvhlog(__FILE__, __LINE__, 0, severity, subsys, fmt, ##__VA_ARGS__)
#if ENABLE_TRACE
#define tvhtrace_enabled() (LOG_TRACE <= atomic_get(&tvhlog_level))
#define tvhtrace(subsys, fmt, ...) \
  do { \
    if (tvhtrace_enabled()) \
      _tvhlog(__FILE__, __LINE__, 0, LOG_TRACE, subsys, fmt, ##__VA_ARGS__); \
  } while (0)
#define tvhlog_hexdump(subsys, data, len) \
  do { \
    if (tvhtrace_enabled()) \
      _tvhlog_hexdump(__FILE__, __LINE__, 0, LOG_TRACE, subsys, (uint8_t*)data, len); \
  } while (0)
#else
static inline void tvhtrace_no_warnings(const char *fmt, ...) { (void)fmt; }
#define tvhtrace_enabled() 0
#define tvhtrace(subsys, fmt, ...) do { tvhtrace_no_warnings(NULL, subsys, fmt, ##__VA_ARGS__); } while (0)
#define tvhlog_hexdump(subsys, data, len) do { tvhtrace_no_warnings(NULL, subsys, data, len); } while (0)
#endif

#define tvhftrace(subsys, fcn) do { \
  tvhtrace(subsys, "%s() enter", #fcn); \
  fcn(); \
  tvhtrace(subsys, "%s() leave", #fcn); \
} while (0)

#define tvhdebug(...)  tvhlog(LOG_DEBUG,   ##__VA_ARGS__)
#define tvhinfo(...)   tvhlog(LOG_INFO,    ##__VA_ARGS__)
#define tvhwarn(...)   tvhlog(LOG_WARNING, ##__VA_ARGS__)
#define tvhnotice(...) tvhlog(LOG_NOTICE,  ##__VA_ARGS__)
#define tvherror(...)  tvhlog(LOG_ERR,     ##__VA_ARGS__)
#define tvhalert(...)  tvhlog(LOG_ALERT,   ##__VA_ARGS__)

void tvhlog_backtrace_printf(const char *fmt, ...);

#endif /* __TVH_LOGGING_H__ */
