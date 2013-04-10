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

#include <sys/syslog.h>
#include <pthread.h>
#include <stdarg.h>

#include "htsmsg.h"

/* Config */
extern int              tvhlog_level;
extern htsmsg_t        *tvhlog_subsys;
extern char            *tvhlog_path;
extern int              tvhlog_options;
extern pthread_mutex_t  tvhlog_mutex;

/* Initialise */
void tvhlog_init       ( int level, int options, const char *path ); 
void tvhlog_set_subsys ( const char *subsys );
void tvhlogv           ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys, const char *fmt, va_list args );
void _tvhlog           ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys, const char *fmt, ... )
  __attribute__((format(printf,6,7)));
void _tvhlog_hexdump   ( const char *file, int line,
                         int notify, int severity,
                         const char *subsys,
                         const uint8_t *data, ssize_t len );


/* Options */
#define TVHLOG_OPT_DBG_SYSLOG   0x0001
#define TVHLOG_OPT_DBG_STDERR   0x0002
#define TVHLOG_OPT_DBG_FILE     0x0004
#define TVHLOG_OPT_SYSLOG       0x0010
#define TVHLOG_OPT_STDERR       0x0020
#define TVHLOG_OPT_MILLIS       0x0100
#define TVHLOG_OPT_DECORATE     0x0200
#define TVHLOG_OPT_FILELINE     0x0400
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
#define tvhtrace(subsys, fmt, ...)\
  _tvhlog(__FILE__, __LINE__, 0, LOG_TRACE, subsys, fmt, ##__VA_ARGS__)
#define tvhlog_hexdump(subsys, data, len)\
  _tvhlog_hexdump(__FILE__, __LINE__, 0, LOG_TRACE, subsys, (uint8_t*)data, len)
#else
#define tvhtrace(...) (void)0
#define tvhlog_hexdump(...) (void)0
#endif

#endif /* __TVH_LOGGING_H__ */
