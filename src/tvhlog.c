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

#include <stdio.h>
#include "tvhlog.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#if ENABLE_EXECINFO
#include <execinfo.h>
#endif

#include "bitops.h"
#include "settings.h"
#include "libav.h"
#include "tcp.h"
#include "webui/webui.h"

int                      tvhlog_run;
int                      tvhlog_level;
int                      tvhlog_options;
char                    *tvhlog_path;
//TVHLOG_BITARRAY is defined in bitops.h
bitops_ulong_t           tvhlog_debug[TVHLOG_BITARRAY];
bitops_ulong_t           tvhlog_trace[TVHLOG_BITARRAY];
pthread_t                tvhlog_tid;
tvh_mutex_t              tvhlog_mutex;
tvh_cond_t               tvhlog_cond;
TAILQ_HEAD(,tvhlog_msg)  tvhlog_queue;
int                      tvhlog_queue_size;
int                      tvhlog_queue_full;
#if ENABLE_TRACE
int                      tvhlog_rtfd = STDOUT_FILENO;
struct sockaddr_storage  tvhlog_rtss;
#endif

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

tvhlog_subsys_t tvhlog_subsystems[] = {
  [LS_NONE]          = { "<none>",        N_("None") },
  [LS_START]         = { "START",         N_("START") },
  [LS_STOP]          = { "STOP",          N_("STOP") },
  [LS_CRASH]         = { "CRASH",         N_("CRASH") },
  [LS_CPU]           = { "CPU",           N_("CPU") },
  [LS_MAIN]          = { "main",          N_("Main") },
  [LS_TPROF]         = { "tprof",         N_("Time profiling") },
  [LS_QPROF]         = { "qprof",         N_("Queue profiling") },
  [LS_THREAD]        = { "thread",        N_("Thread") },
  [LS_TVHPOLL]       = { "tvhpoll",       N_("Poll multiplexer") },
  [LS_TIME]          = { "time",          N_("Time") },
  [LS_SPAWN]         = { "spawn",         N_("Spawn") },
  [LS_FSMONITOR]     = { "fsmonitor",     N_("Filesystem monitor") },
  [LS_LOCK]          = { "lock",          N_("Locking") },
  [LS_UUID]          = { "uuid",          N_("UUID") },
  [LS_IDNODE]        = { "idnode",        N_("Node subsystem") },
  [LS_URL]           = { "url",           N_("URL") },
  [LS_TCP]           = { "tcp",           N_("TCP Protocol") },
  [LS_RTSP]          = { "rtsp",          N_("RTSP Protocol") },
  [LS_UPNP]          = { "upnp",          N_("UPnP Protocol") },
  [LS_SETTINGS]      = { "settings",      N_("Settings") },
  [LS_CONFIG]        = { "config",        N_("Configuration") },
  [LS_ACCESS]        = { "access",        N_("Access (ACL)") },
  [LS_CRON]          = { "cron",          N_("Cron") },
  [LS_DBUS]          = { "dbus",          N_("DBUS") },
  [LS_AVAHI]         = { "avahi",         N_("Avahi") },
  [LS_BONJOUR]       = { "bonjour",       N_("Bonjour") },
  [LS_API]           = { "api",           N_("API") },
  [LS_HTTP]          = { "http",          N_("HTTP Server") },
  [LS_HTTPC]         = { "httpc",         N_("HTTP Client") },
  [LS_HTSP]          = { "htsp",          N_("HTSP Server") },
  [LS_HTSP_SUB]      = { "htsp-sub",      N_("HTSP Subscription") },
  [LS_HTSP_REQ]      = { "htsp-req",      N_("HTSP Request") },
  [LS_HTSP_ANS]      = { "htsp-ans",      N_("HTSP Answer") },
  [LS_IMAGECACHE]    = { "imagecache",    N_("Image Cache") },
  [LS_TBL]           = { "tbl",           N_("DVB SI Tables") },
  [LS_TBL_BASE]      = { "tbl-base",      N_("Base DVB SI Tables (PAT,CAT,PMT,SDT etc.)") },
  [LS_TBL_CSA]       = { "tbl-csa",       N_("DVB CSA (descrambling) Tables") },
  [LS_TBL_EIT]       = { "tbl-eit",       N_("DVB EPG Tables") },
  [LS_TBL_TIME]      = { "tbl-time",      N_("DVB Time Tables") },
  [LS_TBL_ATSC]      = { "tbl-atsc",      N_("ATSC SI Tables") },
  [LS_TBL_PASS]      = { "tbl-pass",      N_("Passthrough Muxer SI Tables") },
  [LS_TBL_SATIP]     = { "tbl-satip",     N_("SAT>IP Server SI Tables") },
  [LS_FASTSCAN]      = { "fastscan",      N_("Fastscan DVB") },
  [LS_PCR]           = { "pcr",           N_("PCR Clocks") },
  [LS_PARSER]        = { "parser",        N_("MPEG-TS Parser") },
  [LS_TS]            = { "TS",            N_("Transport Stream") },
  [LS_GLOBALHEADERS] = { "globalheaders", N_("Global Headers") },
  [LS_TSFIX]         = { "tsfix",         N_("Time Stamp Fix") },
  [LS_HEVC]          = { "hevc",          N_("HEVC - H.265") },
  [LS_MUXER]         = { "muxer",         N_("Muxer") },
  [LS_PASS]          = { "pass",          N_("Pass-thru muxer") },
  [LS_AUDIOES]       = { "audioes",       N_("Audioes muxer") },
  [LS_MKV]           = { "mkv",           N_("Matroska muxer") },
  [LS_SERVICE]       = { "service",       N_("Service") },
  [LS_CHANNEL]       = { "channel",       N_("Channel") },
  [LS_SUBSCRIPTION]  = { "subscription",  N_("Subscription") },
  [LS_SERVICE_MAPPER]= { "service-mapper",N_("Service Mapper") },
  [LS_BOUQUET]       = { "bouquet",       N_("Bouquet") },
  [LS_ESFILTER]      = { "esfilter",      N_("Elementary Stream Filter") },
  [LS_PROFILE]       = { "profile",       N_("Streaming Profile") },
  [LS_DESCRAMBLER]   = { "descrambler",   N_("Descrambler") },
  [LS_DESCRAMBLER_EMM]={ "descrambler-emm",N_("Descrambler EMM") },
  [LS_CACLIENT]      = { "caclient",      N_("CA (descrambling) Client") },
  [LS_CSA]           = { "csa",           N_("CSA (descrambling)") },
  [LS_CAPMT]         = { "capmt",         N_("CAPMT CA Client") },
  [LS_CWC]           = { "cwc",           N_("CWC CA Client") },
  [LS_CCCAM]         = { "cccam",         N_("CWC CCCam Client") },
  [LS_DVBCAM]        = { "dvbcam",        N_("DVB CAM Client") },
  [LS_DVR]           = { "dvr",           N_("Digital Video Recorder") },
  [LS_DVR_INOTIFY]   = { "dvr-inotify",   N_("DVR Inotify") },
  [LS_EPG]           = { "epg",           N_("Electronic Program Guide") },
  [LS_EPGDB]         = { "epgdb",         N_("EPG Database") },
  [LS_EPGGRAB]       = { "epggrab",       N_("EPG Grabber") },
  [LS_CHARSET]       = { "charset",       N_("Charset") },
  [LS_DVB]           = { "dvb",           N_("DVB") },
  [LS_MPEGTS]        = { "mpegts",        N_("MPEG-TS") },
  [LS_MUXSCHED]      = { "muxsched",      N_("Mux Scheduler") },
  [LS_LIBAV]         = { "libav",         N_("libav / ffmpeg") },
  [LS_TRANSCODE]     = { "transcode",     N_("Transcode") },
  [LS_IPTV]          = { "iptv",          N_("IPTV") },
  [LS_IPTV_PCR]      = { "iptv-pcr",      N_("IPTV PCR") },
  [LS_IPTV_SUB]      = { "iptv-sub",      N_("IPTV Subcription") },
  [LS_LINUXDVB]      = { "linuxdvb",      N_("LinuxDVB Input") },
  [LS_DISEQC]        = { "diseqc",        N_("DiseqC") },
  [LS_EN50221]       = { "en50221",       N_("CI Module") },
  [LS_EN50494]       = { "en50494",       N_("Unicable (EN50494)") },
  [LS_SATIP]         = { "satip",         N_("SAT>IP Client") },
  [LS_SATIPS]        = { "satips",        N_("SAT>IP Server") },
  [LS_TVHDHOMERUN]   = { "tvhdhomerun",   N_("TVHDHomeRun Client") },
  [LS_PSIP]          = { "psip",          N_("ATSC PSIP EPG") },
  [LS_OPENTV]        = { "opentv",        N_("OpenTV EPG") },
  [LS_PYEPG]         = { "pyepg",         N_("PyEPG Import") },
  [LS_XMLTV]         = { "xmltv",         N_("XMLTV EPG Import") },
  [LS_WEBUI]         = { "webui",         N_("Web User Interface") },
  [LS_TIMESHIFT]     = { "timeshift",     N_("Timeshift") },
  [LS_SCANFILE]      = { "scanfile",      N_("Scanfile") },
  [LS_TSFILE]        = { "tsfile",        N_("MPEG-TS File") },
  [LS_TSDEBUG]       = { "tsdebug",       N_("MPEG-TS Input Debug") },
  [LS_CODEC]         = { "codec",         N_("Codec") },
#if ENABLE_DDCI
  [LS_DDCI]          = { "ddci",          N_("DD-CI") },
#endif
  [LS_UDP]           = { "udp",           N_("UDP Streamer") },
  [LS_RATINGLABELS]  = { "ratinglabels",  N_("Rating Labels") },

};

/*
 * logging transcoding
 */
#define SUB_SYSTEM_TRANSCODE_NAME_LENGHT_MAX 64
// name (2nd parameter) has max 64 chars
tvhlog_subsys_t tvhlog_transcode_subsystems[] = {
    [LST_NONE]          = { "",              N_("") },
    [LST_AUDIO]         = { "audio",         N_("Audio") },
    [LST_VIDEO]         = { "video",         N_("Video") },
    [LST_CODEC]         = { "codec",         N_("Codec") },
    [LST_MP2]           = { "mp2",           N_("MP2") },
    [LST_AAC]           = { "aac",           N_("AAC") },
    [LST_FLAC]          = { "flac",          N_("FLAC") },
    [LST_LIBFDKAAC]     = { "libfdk-aac",    N_("LIB FDK_AAC") },
    [LST_LIBOPUS]       = { "libopus",       N_("LIB OPUS") },
    [LST_LIBTHEORA]     = { "libtheora",     N_("LIB THEORA") },
    [LST_LIBVORBIS]     = { "libvorbis",     N_("LIB VORBIS") },
    [LST_VORBIS]        = { "vorbis",        N_("VORBIS") },
    [LST_MPEG2VIDEO]    = { "mpeg2video",    N_("MPEG2 VIDEO") },
    [LST_LIBVPX]        = { "libvpx",        N_("LIB VPX") },
    [LST_LIBX26X]       = { "libx26x",       N_("LIB x264_x265") },
    [LST_NVENC]         = { "nvenc",         N_("NVENC") },
    [LST_OMX]           = { "omx",           N_("OMX") },
    [LST_VAAPI]         = { "vaapi",         N_("VA-API") },
    [LST_VAINFO]        = { "vainfo",        N_("VAINFO") },
};

// delimiter ': ' has 2 chars
#define SUB_SYSTEM_TRANSCODE_DELIMITER_LENGTH 2

void
tvh_concatenate_subsystem_with_logsv(char* buf, int subsys, const char *fmt, va_list *args)
{
  // to store the subsystem codec name
  char subsystem[SUB_SYSTEM_TRANSCODE_NAME_LENGHT_MAX];
  // to store (temporary buffer)
  char log[SUB_SYSTEM_TRANSCODE_LOG_LENGTH_MAX - SUB_SYSTEM_TRANSCODE_NAME_LENGHT_MAX - SUB_SYSTEM_TRANSCODE_DELIMITER_LENGTH];
  if (args)
    vsnprintf(log, sizeof(log), fmt, *args);
  else
    snprintf(log, sizeof(log), "%s", fmt);
  snprintf(subsystem, sizeof(subsystem), "%s", tvhlog_transcode_subsystems[subsys].name);
  // concatenate strings with delimited ': '
  snprintf(buf, SUB_SYSTEM_TRANSCODE_LOG_LENGTH_MAX, "%s: %s", subsystem, log);
}

void 
tvh_concatenate_subsystem_with_logs(char* buf, int subsys, const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt);
  tvh_concatenate_subsystem_with_logsv(buf, subsys, fmt, &args);
  va_end(args);
}

static void
tvhlog_get_subsys ( bitops_ulong_t *ss, char *subsys, size_t len )
{
  size_t c = 0;
  uint_fast32_t first = 1, i;
  *subsys = '\0';
  for (i = 0; i < LS_LAST; i++)
    if (!test_bit(i, ss)) break;
  if (i >= LS_LAST) {
    tvh_strlcatf(subsys, len, c, "all");
    return;
  }
  for (i = 0; i < LS_LAST; i++) {
    if (!test_bit(i, ss)) continue;
    tvh_strlcatf(subsys, len, c, "%s%s",
                 first ? "" : ",",
                 tvhlog_subsystems[i].name);
    first = 0;
  }
}

/* Set subsys */
static void
tvhlog_set_subsys ( bitops_ulong_t *c, const char *subsys )
{
  uint_fast32_t a, i;
  char *s, *t, *r = NULL;

  memset(c, 0, TVHLOG_BITARRAY * sizeof(bitops_ulong_t));

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
      memset(c, a ? 0xff : 0, TVHLOG_BITARRAY * sizeof(bitops_ulong_t));
    } else {
      for (i = 0; i < LS_LAST; i++)
        if (!strcmp(tvhlog_subsystems[i].name, t)) {
          if (a) set_bit(i, c); else clear_bit(i, c);
          break;
        }
      if (i >= LS_LAST)
        tvherror(LS_CONFIG, "unknown subsystem '%s'", t);
    }
next:
    t = strtok_r(NULL, ",", &r);
  }
  free(s);
}

void
tvhlog_set_debug ( const char *subsys )
{
  tvhlog_set_subsys(tvhlog_debug, subsys);
}

void
tvhlog_set_trace ( const char *subsys )
{
  tvhlog_set_subsys(tvhlog_trace, subsys);
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
  if (msg->notify && msg->severity < LOG_TRACE) {
    snprintf(buf, sizeof(buf), "%s %s", t, msg->msg);
    comet_mailbox_add_logmsg(buf, msg->severity >= LOG_DEBUG, 0);
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

  tvh_mutex_lock(&tvhlog_mutex);
  while (tvhlog_run) {

    /* Wait */
    if (!(msg = TAILQ_FIRST(&tvhlog_queue))) {
      if (fp) {
        fclose(fp); // only issue here is we close with mutex!
                    // but overall performance will be higher
        fp = NULL;
      }
      tvh_cond_wait(&tvhlog_cond, &tvhlog_mutex);
      continue;
    }
    TAILQ_REMOVE(&tvhlog_queue, msg, link);
    tvhlog_queue_size--;
    if (tvhlog_queue_size < (TVHLOG_QUEUE_MAXSIZE / 2))
      tvhlog_queue_full = 0;

    /* Copy options and path */
    if (!fp) {
      if (tvhlog_path) {
        strlcpy(buf, tvhlog_path, sizeof(buf));
        path = buf;
      } else {
        path = NULL;
      }
    }
    options  = tvhlog_options;
    tvh_mutex_unlock(&tvhlog_mutex);
    tvhlog_process(msg, options, &fp, path);
    tvh_mutex_lock(&tvhlog_mutex);
  }
  if (fp)
    fclose(fp);
  tvh_mutex_unlock(&tvhlog_mutex);
  return NULL;
}

void tvhlogv ( const char *file, int line, int severity,
               int subsys, const char *fmt, va_list *args )
{
  int ok, options, notify;
  size_t l;
  char buf[1024];

  notify = (severity & LOG_TVH_NOTIFY) ? 1 : 0;
  severity &= ~LOG_TVH_NOTIFY;

  if (severity >= LOG_DEBUG) {
    ok = 0;
    if (severity <= atomic_get(&tvhlog_level)) {
      ok = test_bit(subsys, tvhlog_trace);
      if (!ok && severity == LOG_DEBUG)
        ok = test_bit(subsys, tvhlog_debug);
    }
  } else {
    ok = 1;
  }

  /* Ignore */
  if (!ok)
    return;

  tvh_mutex_lock(&tvhlog_mutex);

  /* Check for full */
  if (tvhlog_queue_full) {
    tvh_mutex_unlock(&tvhlog_mutex);
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
  options = tvhlog_options;
  l = 0;
  if (options & TVHLOG_OPT_THREAD) {
    tvh_strlcatf(buf, sizeof(buf), l, "tid %ld: ", (long)pthread_self());
  }
  tvh_strlcatf(buf, sizeof(buf), l, "%s: ", tvhlog_subsystems[subsys].name);
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
    tvh_cond_signal(&tvhlog_cond, 0);
  } else {
#endif
    FILE *fp = NULL;
    tvhlog_process(msg, tvhlog_options, &fp, tvhlog_path);
    if (fp) fclose(fp);
#if TVHLOG_THREAD
  }
#endif
  tvh_mutex_unlock(&tvhlog_mutex);
}


/*
 * Map args
 */
void _tvhlog ( const char *file, int line, int severity,
               int subsys, const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt);
  tvhlogv(file, line, severity, subsys, fmt, &args);
  va_end(args);
}

/*
 * Log a hexdump
 */
#define HEXDUMP_WIDTH 16
void
_tvhlog_hexdump(const char *file, int line, int severity,
                int subsys, const uint8_t *data, ssize_t len )
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
    tvhlogv(file, line, severity, subsys, str, NULL);
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
 *
 */
#if ENABLE_TRACE
static void tvhdbgv(int subsys, const char *fmt, va_list *args)
{
  char buf[2048];
  size_t l = 0;

  if (tvhlog_rtfd < 0) return;
  tvh_strlcatf(buf, sizeof(buf), l, "%s: ", tvhlog_subsystems[subsys].name);
  l += vsnprintf(buf + l, sizeof(buf) - l, fmt, *args);
  if (l + 1 < sizeof(buf))
    buf[l++] = '\n';
  sendto(tvhlog_rtfd, buf, l, 0, (struct sockaddr *)&tvhlog_rtss, sizeof(struct sockaddr_in));
}
#endif

/*
 *
 */
#if ENABLE_TRACE
void tvhdbg(int subsys, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  tvhdbgv(subsys, fmt, &args);
  va_end(args);
}
#endif

/*
 * Initialise
 */
void
tvhlog_init ( int level, int options, const char *path )
{
  atomic_set(&tvhlog_level, level);
  tvhlog_options = options;
  tvhlog_path    = path ? strdup(path) : NULL;
  memset(tvhlog_trace, 0, sizeof(tvhlog_trace));
  memset(tvhlog_debug, 0, sizeof(tvhlog_debug));
  tvhlog_run     = 1;
  openlog("tvheadend", LOG_PID, LOG_DAEMON);
  tvh_mutex_init(&tvhlog_mutex, NULL);
  tvh_cond_init(&tvhlog_cond, 1);
  TAILQ_INIT(&tvhlog_queue);
#if ENABLE_TRACE
  {
    const char *rtport0 = getenv("TVHEADEND_RTLOG_UDP_PORT");
    int rtport = rtport0 ? atoi(rtport0) : 0;
    if (rtport > 0) {
      tvhlog_rtfd = tvh_socket(AF_INET, SOCK_DGRAM, 0);
      tcp_get_ip_from_str("127.0.0.1", &tvhlog_rtss);
      IP_AS_V4(&tvhlog_rtss, port) = htons(rtport);
    }
  }
#endif
}

void
tvhlog_start ( void )
{
  idclass_register(&tvhlog_conf_class);
  tvh_thread_create(&tvhlog_tid, NULL, tvhlog_thread, NULL, "log");
}

void
tvhlog_end ( void )
{
  FILE *fp = NULL;
  tvhlog_msg_t *msg;
  tvh_mutex_lock(&tvhlog_mutex);
  tvhlog_run = 0;
  tvh_cond_signal(&tvhlog_cond, 0);
  tvh_mutex_unlock(&tvhlog_mutex);
  pthread_join(tvhlog_tid, NULL);
  tvh_mutex_lock(&tvhlog_mutex);
  while ((msg = TAILQ_FIRST(&tvhlog_queue)) != NULL) {
    TAILQ_REMOVE(&tvhlog_queue, msg, link);
    tvhlog_process(msg, tvhlog_options, &fp, tvhlog_path);
  }
  tvhlog_queue_full = 1;
  tvh_mutex_unlock(&tvhlog_mutex);
  if (fp)
    fclose(fp);
  free(tvhlog_path);
#if ENABLE_TRACE
  if (tvhlog_rtfd >= 0) {
    close(tvhlog_rtfd);
    tvhlog_rtfd = 1;
  }
#endif
  closelog();
}

/*
 * Configuration
 */

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
    tvh_mutex_lock(&tvhlog_mutex);
    free(tvhlog_path);
    tvhlog_path = strdup(s ?: "");
    if (tvhlog_path && tvhlog_path[0])
      tvhlog_options |= TVHLOG_OPT_DBG_FILE;
    else
      tvhlog_options &= ~TVHLOG_OPT_DBG_FILE;
    tvh_mutex_unlock(&tvhlog_mutex);
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
  tvh_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_SYSLOG;
  else
    tvhlog_options &= ~TVHLOG_OPT_SYSLOG;
  tvh_mutex_unlock(&tvhlog_mutex);
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
  tvh_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_DBG_SYSLOG;
  else
    tvhlog_options &= ~TVHLOG_OPT_DBG_SYSLOG;
  tvh_mutex_unlock(&tvhlog_mutex);
  return 1;
}

static const void *
tvhlog_class_trace_get ( void *o )
{
  static int si;
  si = atomic_get(&tvhlog_level) >= LOG_TRACE;
  return &si;
}

static int
tvhlog_class_trace_set ( void *o, const void *v )
{
  atomic_set(&tvhlog_level, *(int *)v ? LOG_TRACE : LOG_DEBUG);
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
  tvh_mutex_lock(&tvhlog_mutex);
  if (*(int *)v)
    tvhlog_options |= TVHLOG_OPT_LIBAV;
  else
    tvhlog_options &= ~TVHLOG_OPT_LIBAV;
  libav_set_loglevel();
  tvh_mutex_unlock(&tvhlog_mutex);
  return 1;
}

idnode_t tvhlog_conf = {
  .in_class      = &tvhlog_conf_class
};

CLASS_DOC(debugging)

const idclass_t tvhlog_conf_class = {
  .ic_snode      = &tvhlog_conf,
  .ic_class      = "tvhlog_conf",
  .ic_caption    = N_("Debugging"),
  .ic_doc        = tvh_doc_debugging_class,
  .ic_event      = "tvhlog_conf",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_groups     = (const property_group_t[]) {
    {
      .name   = N_("General Settings"),
      .number = 1,
    },
    {
      .name   = N_("Subsystem Output Settings"),
      .number = 2,
    },
    {
      .name   = N_("Miscellaneous Settings"),
      .number = 3,
    },
    {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "path",
      .name   = N_("Filename (including path)"),
      .desc   = N_("Enter the filename (including path) where "
                   "Tvheadend should write the log."),
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
      .type   = PT_BOOL,
      .id     = "trace",
      .name   = N_("Debug trace (low-level)"),
      .desc   = N_("Enable/disable inclusion of low-level debug traces."),
      .get    = tvhlog_class_trace_get,
      .set    = tvhlog_class_trace_set,
#if !ENABLE_TRACE
      .opts   = PO_RDONLY | PO_HIDDEN,
#endif
      .group  = 1,
    },
    {
      .type   = PT_STR,
      .id     = "debugsubs",
      .name   = N_("Debug subsystems"),
      .desc   = N_("Enter comma-separated list of subsystems you want "
                   "debugging output for (e.g. "
                   "linuxdvb,subscription,mpegts)."),
      .get    = tvhlog_class_debugsubs_get,
      .set    = tvhlog_class_debugsubs_set,
      .opts   = PO_MULTILINE,
      .group  = 2,
    },
    {
      .type   = PT_STR,
      .id     = "tracesubs",
      .name   = N_("Trace subsystems"),
      .desc   = N_("Enter comma-separated list of subsystems you want "
                   "to get traces for (e.g linuxdvb,subscription,mpegts)."),
      .get    = tvhlog_class_tracesubs_get,
      .set    = tvhlog_class_tracesubs_set,
#if !ENABLE_TRACE
      .opts   = PO_RDONLY | PO_HIDDEN |  PO_MULTILINE,
#else
      .opts   = PO_MULTILINE,
#endif
      .group  = 2,
    },
    {
      .type   = PT_BOOL,
      .id     = "libav",
      .name   = N_("Debug libav log"),
      .desc   = N_("Enable/disable libav log output."),
      .get    = tvhlog_class_libav_get,
      .set    = tvhlog_class_libav_set,
      .group  = 3,
    },
    {}
  }
};
