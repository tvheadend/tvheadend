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

#if ENABLE_EXECINFO
#include <execinfo.h>
#endif

#include "libav.h"
#include "webui/webui.h"

time_t                   dispatch_clock;

int                      tvhlog_run;
int                      tvhlog_level;
int                      tvhlog_options;
char                    *tvhlog_path;
htsmsg_t                *tvhlog_debug;
htsmsg_t                *tvhlog_trace;
pthread_t                tvhlog_tid;
pthread_mutex_t          tvhlog_mutex;
pthread_cond_t           tvhlog_cond;
TAILQ_HEAD(,tvhlog_msg)  tvhlog_queue;
int                      tvhlog_queue_size;
int                      tvhlog_queue_full;

#define TVHLOG_QUEUE_MAXSIZE 10000
#define TVHLOG_THREAD 1

typedef struct tvhlog_msg
{
  TAILQ_ENTRY(tvhlog_msg)  link;
  char                    *msg;
  int                      severity;
  int                      notify;
  struct timeval           time;
} tvhlog_msg_t;

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

static void
tvhlog_get_subsys ( htsmsg_t *ss, char *subsys, size_t len )
{
  size_t c = 0;
  int first = 1;
  htsmsg_field_t *f;
  *subsys = '\0';
  if (ss) {
    HTSMSG_FOREACH(f, ss) {
      if (f->hmf_type != HMF_S64) continue;
      tvh_strlcatf(subsys, len, c, "%s%c%s",
                    first ? "" : ",",
                    f->hmf_s64 ? '+' : '-',
                    f->hmf_name);
      first = 0;
    }
  }
}

/* Set subsys */
static void
tvhlog_set_subsys ( htsmsg_t **c, const char *subsys )
{
  uint32_t a;
  char *s, *t, *r = NULL;

  if (*c)
    htsmsg_destroy(*c);
  *c = NULL;

  if (!subsys)
    return;

  s = strdup(subsys);
  t = strtok_r(s, ",", &r);
  while ( t ) {
    subsys = NULL;
    a      = 1;
    while (*t && (*t == '+' || *t == '-' || *t <= ' ')) {
      if (*t > ' ')
        a = *t == '+';
      t++;
    }
    if (!*t) goto next;
    if (!strcmp(t, "all")) {
      if (*c)
        htsmsg_destroy(*c);
      *c = NULL;
    }
    if (!*c)
      *c = htsmsg_create_map();
    htsmsg_set_u32(*c, t, a);
next:
    t = strtok_r(NULL, ",", &r);
  }
  free(s);
}

void
tvhlog_set_debug ( const char *subsys )
{
  tvhlog_set_subsys(&tvhlog_debug, subsys);
}

void
tvhlog_set_trace ( const char *subsys )
{
  tvhlog_set_subsys(&tvhlog_trace, subsys);
}

void
tvhlog_get_debug ( char *subsys, size_t len )
{
  tvhlog_get_subsys(tvhlog_debug, subsys, len);
}

void
tvhlog_get_trace ( char *subsys, size_t len )
{
  tvhlog_get_subsys(tvhlog_trace, subsys, len);
}

static void
tvhlog_process
  ( tvhlog_msg_t *msg, int options, FILE **fp, const char *path )
{
  int s;
  size_t l;
  char buf[2048], t[128];
  struct tm tm;

  /* Syslog */
  if (options & TVHLOG_OPT_SYSLOG) {
    if (options & TVHLOG_OPT_DBG_SYSLOG || msg->severity < LOG_DEBUG) {
      s = msg->severity > LOG_DEBUG ? LOG_DEBUG : msg->severity;
      syslog(s, "%s", msg->msg);
    }
  }

  /* Get time */
  localtime_r(&msg->time.tv_sec, &tm);
  l = strftime(t, sizeof(t), "%F %T", &tm);// %d %H:%M:%S", &tm);
  if (options & TVHLOG_OPT_MILLIS) {
    int ms = msg->time.tv_usec / 1000;
    tvh_strlcatf(t, sizeof(t), l, ".%03d", ms);
  }

  /* Comet (debug must still be enabled??) */
  if(msg->notify && msg->severity < LOG_TRACE) {
    htsmsg_t *m = htsmsg_create_map();
    snprintf(buf, sizeof(buf), "%s %s", t, msg->msg);
    htsmsg_add_str(m, "notificationClass", "logmessage");
    htsmsg_add_str(m, "logtxt", buf);
    comet_mailbox_add_message(m, msg->severity >= LOG_DEBUG, 0);
    htsmsg_destroy(m);
  }

  /* Console */
  if (options & TVHLOG_OPT_STDERR) {
    if (options & TVHLOG_OPT_DBG_STDERR || msg->severity < LOG_DEBUG) {
      const char *ltxt = logtxtmeta[msg->severity][0];
      const char *sgr  = logtxtmeta[msg->severity][1];
      const char *sgroff;
    
      if (options & TVHLOG_OPT_DECORATE)
        sgroff = "\033[0m";
      else {
        sgr    = "";
        sgroff = "";
      }
      fprintf(stderr, "%s%s [%7s] %s%s\n", sgr, t, ltxt, msg->msg, sgroff);
    }
  }

  /* File */
  if (*fp || path) {
    if (options & TVHLOG_OPT_DBG_FILE || msg->severity < LOG_DEBUG) {
      const char *ltxt = logtxtmeta[msg->severity][0];
      if (!*fp)
        *fp = tvh_fopen(path, "a");
      if (*fp)
        fprintf(*fp, "%s [%7s]:%s\n", t, ltxt, msg->msg);
    }
  }
  
  free(msg->msg);
  free(msg);
}

/* Log */
static void *
tvhlog_thread ( void *p )
{
  int options;
  char *path = NULL, buf[512];
  FILE *fp = NULL;
  tvhlog_msg_t *msg;

  pthread_mutex_lock(&tvhlog_mutex);
  while (tvhlog_run) {

    /* Wait */
    if (!(msg = TAILQ_FIRST(&tvhlog_queue))) {
      if (fp) {
        fclose(fp); // only issue here is we close with mutex!
                    // but overall performance will be higher
        fp = NULL;
      }
      pthread_cond_wait(&tvhlog_cond, &tvhlog_mutex);
      continue;
    }
    TAILQ_REMOVE(&tvhlog_queue, msg, link);
    tvhlog_queue_size--;
    if (tvhlog_queue_size < (TVHLOG_QUEUE_MAXSIZE / 2))
      tvhlog_queue_full = 0;

    /* Copy options and path */
    if (!fp) {
      if (tvhlog_path) {
        strncpy(buf, tvhlog_path, sizeof(buf));
        path = buf;
      } else {
        path = NULL;
      }
    }
    options  = tvhlog_options; 
    pthread_mutex_unlock(&tvhlog_mutex);
    tvhlog_process(msg, options, &fp, path);
    pthread_mutex_lock(&tvhlog_mutex);
  }
  if (fp)
    fclose(fp);
  pthread_mutex_unlock(&tvhlog_mutex);
  return NULL;
}

void tvhlogv ( const char *file, int line,
               int notify, int severity,
               const char *subsys, const char *fmt, va_list *args )
{
  int ok, options;
  size_t l;
  char buf[1024];

  pthread_mutex_lock(&tvhlog_mutex);

  /* Check for full */
  if (tvhlog_queue_full) {
    pthread_mutex_unlock(&tvhlog_mutex);
    return;
  }

  /* Check debug enabled (and cache config) */
  options = tvhlog_options;
  if (severity >= LOG_DEBUG) {
    ok = 0;
    if (severity <= tvhlog_level) {
      if (tvhlog_trace) {
        ok = htsmsg_get_u32_or_default(tvhlog_trace, "all", 0);
        ok = htsmsg_get_u32_or_default(tvhlog_trace, subsys, ok);
      }
      if (!ok && severity == LOG_DEBUG && tvhlog_debug) {
        ok = htsmsg_get_u32_or_default(tvhlog_debug, "all", 0);
        ok = htsmsg_get_u32_or_default(tvhlog_debug, subsys, ok);
      }
    }
  } else {
    ok = 1;
  }

  /* Ignore */
  if (!ok) {
    pthread_mutex_unlock(&tvhlog_mutex);
    return;
  }

  /* FULL */
  if (tvhlog_queue_size == TVHLOG_QUEUE_MAXSIZE) {
    tvhlog_queue_full = 1;
    fmt      = "log buffer full";
    args     = NULL;
    severity = LOG_ERR;
  }

  /* Basic message */
  l = 0;
  if (options & TVHLOG_OPT_THREAD) {
    tvh_strlcatf(buf, sizeof(buf), l, "tid %ld: ", (long)pthread_self());
  }
  tvh_strlcatf(buf, sizeof(buf), l, "%s: ", subsys);
  if (options & TVHLOG_OPT_FILELINE && severity >= LOG_DEBUG)
    tvh_strlcatf(buf, sizeof(buf), l, "(%s:%d) ", file, line);
  if (args)
    vsnprintf(buf + l, sizeof(buf) - l, fmt, *args);
  else
    snprintf(buf + l, sizeof(buf) - l, "%s", fmt);

  /* Store */
  tvhlog_msg_t *msg = calloc(1, sizeof(tvhlog_msg_t));
  gettimeofday(&msg->time, NULL);
  msg->msg      = strdup(buf);
  msg->severity = severity;
  msg->notify   = notify;
#if TVHLOG_THREAD
  if (tvhlog_run) {
    TAILQ_INSERT_TAIL(&tvhlog_queue, msg, link);
    tvhlog_queue_size++;
    pthread_cond_signal(&tvhlog_cond);
  } else {
#endif
    FILE *fp = NULL;
    tvhlog_process(msg, tvhlog_options, &fp, tvhlog_path);
    if (fp) fclose(fp);
#if TVHLOG_THREAD
  }
#endif
  pthread_mutex_unlock(&tvhlog_mutex);
}


/*
 * Map args
 */
void _tvhlog ( const char *file, int line,
               int notify, int severity,
               const char *subsys, const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt);
  tvhlogv(file, line, notify, severity, subsys, fmt, &args);
  va_end(args);
}

/*
 * Log a hexdump
 */
#define HEXDUMP_WIDTH 16
void
_tvhlog_hexdump(const char *file, int line,
                int notify, int severity,
                const char *subsys,
                const uint8_t *data, ssize_t len )
{
  int i, c;
  char str[1024];

  /* Assume that severify was validated before call */

  /* Build and log output */
  while (len > 0) {
    c = 0;
    for (i = 0; i < HEXDUMP_WIDTH; i++) {
      if (i >= len)
        tvh_strlcatf(str, sizeof(str), c, "   ");
      else
        tvh_strlcatf(str, sizeof(str), c, "%02X ", data[i]);
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
    tvhlogv(file, line, notify, severity, subsys, str, NULL);
    len  -= HEXDUMP_WIDTH;
    data += HEXDUMP_WIDTH;
  }
}

/*
 *
 */
void
tvhlog_backtrace_printf(const char *fmt, ...)
{
#if ENABLE_EXECINFO
  static void *frames[5];
  int nframes = backtrace(frames, 5), i;
  char **strings = backtrace_symbols(frames, nframes);
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  for (i = 0; i < nframes; i++)
    printf("  %s\n", strings[i]);
  free(strings);
#endif
}

/*
 * Initialise
 */
void 
tvhlog_init ( int level, int options, const char *path )
{
  tvhlog_level   = level;
  tvhlog_options = options;
  tvhlog_path    = path ? strdup(path) : NULL;
  tvhlog_trace   = NULL;
  tvhlog_debug   = NULL;
  tvhlog_run     = 1;
  openlog("tvheadend", LOG_PID, LOG_DAEMON);
  pthread_mutex_init(&tvhlog_mutex, NULL);
  pthread_cond_init(&tvhlog_cond, NULL);
  TAILQ_INIT(&tvhlog_queue);
}

void
tvhlog_start ( void )
{
  tvhthread_create(&tvhlog_tid, NULL, tvhlog_thread, NULL, "log");
}

void
tvhlog_end ( void )
{
  FILE *fp = NULL;
  tvhlog_msg_t *msg;
  pthread_mutex_lock(&tvhlog_mutex);
  tvhlog_run = 0;
  pthread_cond_signal(&tvhlog_cond);
  pthread_mutex_unlock(&tvhlog_mutex);
  pthread_join(tvhlog_tid, NULL);
  pthread_mutex_lock(&tvhlog_mutex);
  while ((msg = TAILQ_FIRST(&tvhlog_queue)) != NULL) {
    TAILQ_REMOVE(&tvhlog_queue, msg, link);
    tvhlog_process(msg, tvhlog_options, &fp, tvhlog_path);
  }
  tvhlog_queue_full = 1;
  pthread_mutex_unlock(&tvhlog_mutex);
  if (fp)
    fclose(fp);
  free(tvhlog_path);
  htsmsg_destroy(tvhlog_debug);
  htsmsg_destroy(tvhlog_trace);
  closelog();
}

/*
 * Configuration
 */

static void tvhlog_class_save(idnode_t *self)
{
}

static const void *
tvhlog_class_path_get ( void *o )
{
  return &tvhlog_path;
}

static int
tvhlog_class_path_set ( void *o, const void *v )
{
  const char *s = v;
  if (strcmp(s ?: "", tvhlog_path ?: "")) {
    pthread_mutex_lock(&tvhlog_mutex);
    free(tvhlog_path);
    tvhlog_path = strdup(s ?: "");
    if (tvhlog_path && tvhlog_path[0])
      tvhlog_options |= TVHLOG_OPT_DBG_FILE;
    else
      tvhlog_options &= ~TVHLOG_OPT_DBG_FILE;
    pthread_mutex_unlock(&tvhlog_mutex);
    return 1;
  }
  return 0;
}

static const void *
tvhlog_class_debugsubs_get ( void *o )
{
  static char *p = prop_sbuf;
  tvhlog_get_debug(prop_sbuf, PROP_SBUF_LEN);
  return &p;
}

static int
tvhlog_class_debugsubs_set ( void *o, const void *v )
{
  tvhlog_set_debug((const char *)v);
  return 1;
}

static const void *
tvhlog_class_tracesubs_get ( void *o )
{
  static char *p = prop_sbuf;
  tvhlog_get_trace(prop_sbuf, PROP_SBUF_LEN);
  return &p;
}

static int
tvhlog_class_tracesubs_set ( void *o, const void *v )
{
  tvhlog_set_trace((const char *)v);
  return 1;
}

static const void *
tvhlog_class_enable_syslog_get ( void *o )
{
  static int si;
  si = (tvhlog_options & TVHLOG_OPT_SYSLOG) ? 1 : 0;
  return &si;
}

static int
tvhlog_class_enable_syslog_set ( void *o, const void *v )
{
  pthread_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_SYSLOG;
  else
    tvhlog_options &= ~TVHLOG_OPT_SYSLOG;
  pthread_mutex_unlock(&tvhlog_mutex);
  return 1;
}

static const void *
tvhlog_class_debug_syslog_get ( void *o )
{
  static int si;
  si = (tvhlog_options & TVHLOG_OPT_DBG_SYSLOG) ? 1 : 0;
  return &si;
}

static int
tvhlog_class_debug_syslog_set ( void *o, const void *v )
{
  pthread_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_DBG_SYSLOG;
  else
    tvhlog_options &= ~TVHLOG_OPT_DBG_SYSLOG;
  pthread_mutex_unlock(&tvhlog_mutex);
  return 1;
}

static const void *
tvhlog_class_trace_get ( void *o )
{
  static int si;
  si = tvhlog_level >= LOG_TRACE;
  return &si;
}

static int
tvhlog_class_trace_set ( void *o, const void *v )
{
  pthread_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_level = LOG_TRACE;
  else
    tvhlog_level = LOG_DEBUG;
  pthread_mutex_unlock(&tvhlog_mutex);
  return 1;
}

static const void *
tvhlog_class_libav_get ( void *o )
{
  static int si;
  si = (tvhlog_options & TVHLOG_OPT_LIBAV) ? 1 : 0;
  return &si;
}

static int
tvhlog_class_libav_set ( void *o, const void *v )
{
  pthread_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_LIBAV;
  else
    tvhlog_options &= ~TVHLOG_OPT_LIBAV;
  libav_set_loglevel();
  pthread_mutex_unlock(&tvhlog_mutex);
  return 1;
}

idnode_t tvhlog_conf = {
  .in_class      = &tvhlog_conf_class
};

const idclass_t tvhlog_conf_class = {
  .ic_snode      = &tvhlog_conf,
  .ic_class      = "tvhlog_conf",
  .ic_caption    = N_("Debugging"),
  .ic_event      = "tvhlog_conf",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = tvhlog_class_save,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("Settings"),
      .number = 1,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "path",
      .name   = N_("Debug log path"),
      /* Should this really be called Debug log path? Don't you need to
       * enter a filename here not just a path? */
      .desc   = N_("Enter a filename you want to save the debug log to."),
      .get    = tvhlog_class_path_get,
      .set    = tvhlog_class_path_set,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "enable_syslog",
      .name   = N_("Enable syslog"),
      .desc   = N_("Enable/disable logging to syslog."),
      .get    = tvhlog_class_enable_syslog_get,
      .set    = tvhlog_class_enable_syslog_set,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "syslog",
      .name   = N_("Debug to syslog"),
      .desc   = N_("Enable/disable debugging output to syslog."),
      .get    = tvhlog_class_debug_syslog_get,
      .set    = tvhlog_class_debug_syslog_set,
      .group  = 1,
    },
    {
      .type   = PT_STR,
      .id     = "debugsubs",
      .name   = N_("Debug subsystems"),
      .desc   = N_("Enter comma separated list of subsystems you want "
                   "debugging output for (e.g "
                   "+linuxdvb,+subscriptions,+mpegts)."),
      .get    = tvhlog_class_debugsubs_get,
      .set    = tvhlog_class_debugsubs_set,
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "trace",
      .name   = N_("Debug trace (low-level)"),
      .desc   = N_("Enable/disable including of low level debug traces."),
      .get    = tvhlog_class_trace_get,
      .set    = tvhlog_class_trace_set,
#if !ENABLE_TRACE
      .opts   = PO_RDONLY | PO_HIDDEN,
#endif
      .group  = 1,
    },
    {
      .type   = PT_STR,
      .id     = "tracesubs",
      .name   = N_("Trace subsystems"),
      .desc   = N_("Enter comma separated list of subsystems you want "
                   "to get traces for (e.g +linuxdvb,+subscriptions,+mpegts)."),
      .get    = tvhlog_class_tracesubs_get,
      .set    = tvhlog_class_tracesubs_set,
#if !ENABLE_TRACE
      .opts   = PO_RDONLY | PO_HIDDEN,
#endif
      .group  = 1,
    },
    {
      .type   = PT_BOOL,
      .id     = "libav",
      .name   = N_("Debug libav log"),
      .desc   = N_("Enable/disable libav log output."),
      .get    = tvhlog_class_libav_get,
      .set    = tvhlog_class_libav_set,
      .group  = 1,
    },
    {}
  }
};
