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

typedef struct {
  const char *name;
  const char *desc;
} tvhlog_subsys_t;

/* Config */
extern int              tvhlog_level;
extern char            *tvhlog_path;
extern int              tvhlog_options;
extern pthread_mutex_t  tvhlog_mutex;
extern tvhlog_subsys_t  tvhlog_subsystems[];

/* Initialise */
void tvhlog_init       ( int level, int options, const char *path ); 
void tvhlog_start      ( void );
void tvhlog_end        ( void );
void tvhlog_set_debug  ( const char *subsys );
void tvhlog_get_debug  ( char *subsys, size_t len );
void tvhlog_set_trace  ( const char *subsys );
void tvhlog_get_trace  ( char *subsys, size_t len );
void tvhlogv           ( const char *file, int line, int severity,
                         int subsys, const char *fmt, va_list *args );
void _tvhlog           ( const char *file, int line, int severity,
                         int subsys, const char *fmt, ... )
  __attribute__((format(printf,5,6)));
void _tvhlog_hexdump   ( const char *file, int line, int severity,
                         int subsys, const uint8_t *data, ssize_t len );
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

#define LOG_TVH_NOTIFY 0x40000000

/* Subsystems */
enum {
  LS_NONE,
  LS_START,
  LS_STOP,
  LS_CRASH,
  LS_MAIN,
  LS_GTIMER,
  LS_MTIMER,
  LS_CPU,
  LS_THREAD,
  LS_TVHPOLL,
  LS_TIME,
  LS_SPAWN,
  LS_FSMONITOR,
  LS_LOCK,
  LS_UUID,
  LS_IDNODE,
  LS_URL,
  LS_TCP,
  LS_RTSP,
  LS_UPNP,
  LS_SETTINGS,
  LS_CONFIG,
  LS_ACCESS,
  LS_CRON,
  LS_DBUS,
  LS_AVAHI,
  LS_BONJOUR,
  LS_API,
  LS_HTTP,
  LS_HTTPC,
  LS_HTSP,
  LS_HTSP_SUB,
  LS_HTSP_REQ,
  LS_HTSP_ANS,
  LS_IMAGECACHE,
  LS_TBL,
  LS_TBL_BASE,
  LS_TBL_CSA,
  LS_TBL_EIT,
  LS_TBL_TIME,
  LS_TBL_ATSC,
  LS_TBL_PASS,
  LS_TBL_SATIP,
  LS_FASTSCAN,
  LS_PCR,
  LS_PARSER,
  LS_TS,
  LS_GLOBALHEADERS,
  LS_TSFIX,
  LS_HEVC,
  LS_MUXER,
  LS_PASS,
  LS_AUDIOES,
  LS_MKV,
  LS_SERVICE,
  LS_CHANNEL,
  LS_SUBSCRIPTION,
  LS_SERVICE_MAPPER,
  LS_BOUQUET,
  LS_ESFILTER,
  LS_PROFILE,
  LS_DESCRAMBLER,
  LS_DESCRAMBLER_EMM,
  LS_CACLIENT,
  LS_CSA,
  LS_CAPMT,
  LS_CWC,
  LS_CCCAM,
  LS_DVBCAM,
  LS_DVR,
  LS_EPG,
  LS_EPGDB,
  LS_EPGGRAB,
  LS_CHARSET,
  LS_DVB,
  LS_MPEGTS,
  LS_MUXSCHED,
  LS_LIBAV,
  LS_TRANSCODE,
  LS_IPTV,
  LS_IPTV_PCR,
  LS_IPTV_SUB,
  LS_LINUXDVB,
  LS_DISEQC,
  LS_EN50221,
  LS_EN50494,
  LS_SATIP,
  LS_SATIPS,
  LS_TVHDHOMERUN,
  LS_PSIP,
  LS_OPENTV,
  LS_PYEPG,
  LS_XMLTV,
  LS_WEBUI,
  LS_TIMESHIFT,
  LS_SCANFILE,
  LS_TSFILE,
  LS_TSDEBUG,
  LS_LAST     /* keep this last */
};

/* Macros */
#define tvhlog(severity, subsys, fmt, ...)\
  _tvhlog(__FILE__, __LINE__, severity | LOG_TVH_NOTIFY, subsys, fmt, ##__VA_ARGS__)
#define tvhlog_spawn(severity, subsys, fmt, ...)\
  _tvhlog(__FILE__, __LINE__, severity, subsys, fmt, ##__VA_ARGS__)
#if ENABLE_TRACE
#define tvhtrace_enabled() (LOG_TRACE <= atomic_get(&tvhlog_level))
#define tvhtrace(subsys, fmt, ...) \
  do { \
    if (tvhtrace_enabled()) \
      _tvhlog(__FILE__, __LINE__, LOG_TRACE, subsys, fmt, ##__VA_ARGS__); \
  } while (0)
#define tvhlog_hexdump(subsys, data, len) \
  do { \
    if (tvhtrace_enabled()) \
      _tvhlog_hexdump(__FILE__, __LINE__, LOG_TRACE, subsys, (uint8_t*)data, len); \
  } while (0)
#else
static inline void tvhtrace_no_warnings(const char *fmt, ...) { (void)fmt; }
#define tvhtrace_enabled() 0
#define tvhtrace(subsys, fmt, ...) do { tvhtrace_no_warnings(NULL, subsys, fmt, ##__VA_ARGS__); } while (0)
#define tvhlog_hexdump(subsys, data, len) do { tvhtrace_no_warnings(NULL, subsys, data, len); } while (0)
#endif

#define tvhftrace(subsys, fcn, ...) do { \
  tvhtrace(subsys, "%s() enter", #fcn); \
  fcn(__VA_ARGS__); \
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
