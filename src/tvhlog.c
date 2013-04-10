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

#include "tvhlog.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "webui/webui.h"

int              tvhlog_level;
int              tvhlog_options;
char            *tvhlog_path;
int              tvhlog_path_fail;
htsmsg_t        *tvhlog_subsys;
pthread_mutex_t  tvhlog_mutex;

static const char *logtxtmeta[9][2] = {
  {"EMERGENCY", "\033[31m"},
  {"ALERT",     "\033[31m"},
  {"CRITICAL",  "\033[31m"},
  {"ERROR",     "\033[31m"},
  {"WARNING",   "\033[33m"},
  {"NOTICE",    "\033[36m"},
  {"INFO",      "\033[32m"},
  {"DEBUG",     "\033[32m"},
  {"TRACE",     "\033[32m"},
};

/* Initialise */
void 
tvhlog_init ( int level, int options, const char *path )
{
  tvhlog_level   = level;
  tvhlog_options = options;
  tvhlog_path    = path ? strdup(path) : NULL;
  tvhlog_subsys  = NULL;
  openlog("tvheadend", LOG_PID, LOG_DAEMON);
  pthread_mutex_init(&tvhlog_mutex, NULL);
}

/* Set subsys */
void tvhlog_set_subsys ( const char *subsys )
{
  uint32_t a;
  char *t, *r, *s;

  if (tvhlog_subsys)
    htsmsg_destroy(tvhlog_subsys);
  tvhlog_subsys = NULL;

  if (!subsys)
    return;

  s = strdup(subsys);
  t = strtok_r(s, ",", &r);
  while ( t ) {
    subsys = NULL;
    a      = 1;
    if (!*t) goto next;
    if (t[0] == '+' || t[0] == '-') {
      a = t[0] == '+';
      t++;
    }
    if (!strcmp(t, "all")) {
      if (tvhlog_subsys)
        htsmsg_destroy(tvhlog_subsys);
      tvhlog_subsys = NULL;
    }
    if (!tvhlog_subsys)
      tvhlog_subsys = htsmsg_create_map();
    htsmsg_set_u32(tvhlog_subsys, t, a);
next:
    t = strtok_r(NULL, ",", &r);
  }
  free(s);
}

/* Log */
void tvhlogv ( const char *file, int line,
               int notify, int severity,
               const char *subsys, const char *fmt, va_list args )
{
  struct timeval now;
  struct tm tm;
  char t[128], buf[2048], buf2[2048];
  uint32_t a;
  size_t l;
  int s;

  /* Map down */
  if (severity > LOG_DEBUG)
    s = LOG_DEBUG;
  else
    s = severity;

  /* Check debug enabled */
  if (severity >= LOG_DEBUG) {
    if (!tvhlog_subsys)
      return;
    if (severity > tvhlog_level)
      return;
    a = htsmsg_get_u32_or_default(tvhlog_subsys, "all", 0);
    if (!htsmsg_get_u32_or_default(tvhlog_subsys, subsys, a))
      return;
  }

  /* Get time */
  gettimeofday(&now, NULL);
  localtime_r(&now.tv_sec, &tm);
  l = strftime(t, sizeof(t), "%b %d %H:%M:%S", &tm);
  if (tvhlog_options & TVHLOG_OPT_MILLIS) {
    int ms = now.tv_usec / 1000;
    snprintf(t+l, sizeof(t)-l, ".%03d", ms);
  }

  /* Basic message */
  l = snprintf(buf, sizeof(buf), "%s: ", subsys);
  if (tvhlog_options & TVHLOG_OPT_FILELINE && severity >= LOG_DEBUG)
    l += snprintf(buf + l, sizeof(buf) - l, "(%s:%d) ", file, line);
  l += vsnprintf(buf + l, sizeof(buf) - l, fmt, args);

  /* Syslog */
  if (tvhlog_options & TVHLOG_OPT_SYSLOG) {
    if (tvhlog_options & TVHLOG_OPT_DBG_SYSLOG || severity < LOG_DEBUG) {
      syslog(s, "%s", buf);
    }
  } 

  /* Comet (debug must still be enabled??) */
  if(notify && severity < LOG_TRACE) {
    htsmsg_t *m = htsmsg_create_map();
    snprintf(buf2, sizeof(buf2), "%s %s", t, buf);
    htsmsg_add_str(m, "notificationClass", "logmessage");
    htsmsg_add_str(m, "logtxt", buf2);
    comet_mailbox_add_message(m, severity >= LOG_DEBUG);
    htsmsg_destroy(m);
  }

  /* Console */
  if (tvhlog_options & TVHLOG_OPT_STDERR) {
    if (tvhlog_options & TVHLOG_OPT_DBG_STDERR || severity < LOG_DEBUG) {
      const char *leveltxt = logtxtmeta[severity][0];
      const char *sgr      = logtxtmeta[severity][1];
      const char *sgroff;
    
      if (tvhlog_options & TVHLOG_OPT_DECORATE)
        sgroff = "\033[0m";
      else {
        sgr    = "";
        sgroff = "";
      }
      fprintf(stderr, "%s%s [%7s] %s%s\n", sgr, t, leveltxt, buf, sgroff);
    }
  }

  /* File */
  if (tvhlog_path) {
    if (tvhlog_options & TVHLOG_OPT_DBG_FILE || severity < LOG_DEBUG) {
      const char *leveltxt = logtxtmeta[severity][0];
      FILE *fp = fopen(tvhlog_path, "a");
      if (fp) {
        tvhlog_path_fail = 0;
        fprintf(fp, "%s [%7s]:%s\n", t, leveltxt, buf);
        fclose(fp);
      } else {
        if (!tvhlog_path_fail)
          syslog(LOG_WARNING, "failed to write log file %s", tvhlog_path);
        tvhlog_path_fail = 1;
      }
    }
  }
}

/*
 * Map args
 */
void _tvhlog ( const char *file, int line,
               int notify, int severity,
               const char *subsys, const char *fmt, ... )
{
  va_list args;
  pthread_mutex_lock(&tvhlog_mutex);
  va_start(args, fmt);
  tvhlogv(file, line, notify, severity, subsys, fmt, args);
  va_end(args);
  pthread_mutex_unlock(&tvhlog_mutex);
}

/*
 * Log a hexdump
 */
#define HEXDUMP_WIDTH 16
void
_tvhlog_hexdump(const char *file, int line,
                int notify, int severity,
                const char *subsys,
                const uint8_t *data, ssize_t len)
{
  int i, c;
  char str[1024];
  va_list args;

  pthread_mutex_lock(&tvhlog_mutex);
  while (len > 0) {
    c = 0;
    for (i = 0; i < HEXDUMP_WIDTH; i++) {
      if (i >= len)
        c += snprintf(str+c, sizeof(str)-c, "   ");
      else
        c += snprintf(str+c, sizeof(str)-c, "%02X ", data[i]);
    }
    for (i = 0; i < HEXDUMP_WIDTH; i++) {
      if (i < len) {
        if (data[i] < ' ' || data[i] > '~')
          str[c] = '.';
        else
          str[c] = data[i];
      } else
        str[c] = ' ';
      c++;
    }
    str[c] = '\0';
    tvhlogv(file, line, notify, severity, subsys, str, args);
    len  -= HEXDUMP_WIDTH;
    data += HEXDUMP_WIDTH;
  }
  pthread_mutex_unlock(&tvhlog_mutex);
}

