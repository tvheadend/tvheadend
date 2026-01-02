/*
 *  Tvheadend
 *  Copyright (C) 2007 - 2010 Andreas Öman
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
#include <tvh_thread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include <time.h>
#include <locale.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "tvheadend.h"
#include "api.h"
#include "tcp.h"
#include "access.h"
#include "http.h"
#include "upnp.h"
#include "webui/webui.h"
#include "epggrab.h"
#include "spawn.h"
#include "subscriptions.h"
#include "service_mapper.h"
#include "descrambler/descrambler.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "satip/server.h"
#include "avahi.h"
#include "bonjour.h"
#include "input.h"
#include "service.h"
#include "trap.h"
#include "settings.h"
#include "config.h"
#include "notify.h"
#include "idnode.h"
#include "imagecache.h"
#include "timeshift.h"
#include "fsmonitor.h"
#include "lang_codes.h"
#include "esfilter.h"
#include "intlconv.h"
#include "dbus.h"
#include "libav.h"
#include "transcoding/codec.h"
#include "profile.h"
#include "bouquet.h"
#include "ratinglabels.h"
#include "tvhtime.h"
#include "packet.h"
#include "streaming.h"
#include "memoryinfo.h"
#include "watchdog.h"
#include "tprofile.h"
#if CONFIG_LINUXDVB_CA
#include "input/mpegts/en50221/en50221.h"
#endif

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#endif
#include <sys/resource.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif

pthread_t main_tid;

int64_t   __mdispatch_clock;
time_t    __gdispatch_clock;

/* Command line option struct */
typedef struct {
  const char  sopt;
  const char *lopt;
  const char *desc;
  enum {
    OPT_STR,
    OPT_INT,
    OPT_BOOL,
    OPT_STR_LIST,
  }          type;
  void       *param;
} cmdline_opt_t;

static cmdline_opt_t* cmdline_opt_find
  ( cmdline_opt_t *opts, int num, const char *arg )
{
  int i;
  int isshort = 0;

  if (strlen(arg) < 2 || *arg != '-')
    return NULL;
  arg++;

  if (strlen(arg) == 1)
    isshort = 1;
  else if (*arg == '-')
    arg++;
  else
    return NULL;

  for (i = 0; i < num; i++) {
    if (!opts[i].lopt) continue;
    if (isshort && opts[i].sopt == *arg)
      return &opts[i];
    if (!isshort && !strcmp(opts[i].lopt, arg))
      return &opts[i];
  }

  return NULL;
}

/*
 * Globals
 */
int              tvheadend_running; /* do not use directly: tvheadend_is_running() */
int              tvheadend_mainloop;
int              tvheadend_webui_port;
int              tvheadend_webui_debug;
int              tvheadend_htsp_port;
int              tvheadend_htsp_port_extra;
const char      *tvheadend_cwd0;
const char      *tvheadend_cwd;
const char      *tvheadend_webroot;
const tvh_caps_t tvheadend_capabilities[] = {
#if ENABLE_CWC || ENABLE_CAPMT || ENABLE_CONSTCW || ENABLE_CCCAM
  { "caclient", NULL },
#endif
#if ENABLE_LINUXDVB || ENABLE_SATIP_CLIENT || ENABLE_HDHOMERUN_CLIENT
  { "tvadapters", NULL },
#endif
#if ENABLE_SATIP_CLIENT
  { "satip_client", NULL },
#endif
#if ENABLE_SATIP_SERVER
  { "satip_server", NULL },
#endif
#if ENABLE_TIMESHIFT
  { "timeshift", (uint32_t *)&timeshift_conf.enabled },
#endif
#if ENABLE_TRACE
  { "trace",     NULL },
#endif
#if ENABLE_LIBAV
  { "libav",     NULL },
#endif
  { NULL, NULL }
};

tvh_mutex_t global_lock;
tvh_mutex_t mtimer_lock;
tvh_mutex_t gtimer_lock;
tvh_mutex_t tasklet_lock;
tvh_mutex_t fork_lock;
tvh_mutex_t atomic_lock;

/*
 * Locals
 */
static LIST_HEAD(, mtimer) mtimers;
static tvh_cond_t mtimer_cond;
static mtimer_t *mtimer_running;
static int64_t mtimer_periodic;
static pthread_t mtimer_tid;
static pthread_t mtimer_tick_tid;
static tprofile_t mtimer_profile;
static LIST_HEAD(, gtimer) gtimers;
static gtimer_t *gtimer_running;
static tvh_cond_t gtimer_cond;
static tprofile_t gtimer_profile;
static TAILQ_HEAD(, tasklet) tasklets;
static tvh_cond_t tasklet_cond;
static pthread_t tasklet_tid;
static memoryinfo_t tasklet_memoryinfo = { .my_name = "Tasklet" };

static void
handle_sigpipe(int x)
{
  return;
}

static void
handle_sigill(int x)
{
  /* Note that on some platforms, the SSL library tries */
  /* to determine the CPU capabilities with possible */
  /* unknown instructions */
  tvhwarn(LS_CPU, "Illegal instruction handler (might be OK)");
  tvh_signal(SIGILL, handle_sigill);
}

void
doexit(int x)
{
  if (pthread_self() != main_tid)
    tvh_thread_kill(main_tid, SIGTERM);
  tvh_cond_signal(&gtimer_cond, 0);
  tvh_cond_signal(&mtimer_cond, 0);
  atomic_set(&tvheadend_running, 0);
  tvh_signal(x, doexit);
}

static int
get_user_groups (const struct passwd *pw, gid_t* glist, size_t gmax)
{
  int num = 0;
#if !ENABLE_ANDROID
  struct group *gr;
  char **mem;
  glist[num++] = pw->pw_gid;
  for ( gr = getgrent(); (gr != NULL) && (num < gmax); gr = getgrent() ) {
    if (gr->gr_gid == pw->pw_gid) continue;
    for (mem = gr->gr_mem; *mem; mem++) {
      if(!strcmp(*mem, pw->pw_name)) glist[num++] = gr->gr_gid;
    }
  }
#endif
  return num;
}

/**
 *
 */

#define safecmp(a, b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

static int
mtimercmp(mtimer_t *a, mtimer_t *b)
{
  return safecmp(a->mti_expire, b->mti_expire);
}

#if ENABLE_TRACE
static void mtimer_magic_check(mtimer_t *mti)
{
  if (mti->mti_magic1 != MTIMER_MAGIC1) {
    tvhdbg(LS_MAIN, "mtimer magic check failed for %p", mti);
    tvherror(LS_MAIN, "mtimer magic check failed for %p", mti);
    abort();
  }
}
#else
static inline void mtimer_magic_check(mtimer_t *mti)
{
}
#endif

/**
 * this routine can be called inside any locks
 */
void
GTIMER_FCN(mtimer_arm_abs)
  (GTIMER_TRACEID_ mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t when)
{
  tvh_mutex_lock(&mtimer_lock);

  if (mti->mti_callback != NULL) {
    mtimer_magic_check(mti);
    LIST_REMOVE(mti, mti_link);
  }

#if ENABLE_TRACE
  mti->mti_magic1   = MTIMER_MAGIC1;
#endif
  mti->mti_callback = callback;
  mti->mti_opaque   = opaque;
  mti->mti_expire   = when;
#if ENABLE_GTIMER_CHECK
  mti->mti_id       = id;
#endif

  LIST_INSERT_SORTED(&mtimers, mti, mti_link, mtimercmp);

  if (LIST_FIRST(&mtimers) == mti)
    tvh_cond_signal(&mtimer_cond, 0); // force timer re-check

  tvh_mutex_unlock(&mtimer_lock);
}

/**
 *
 */
void
GTIMER_FCN(mtimer_arm_rel)
  (GTIMER_TRACEID_ mtimer_t *gti, mti_callback_t *callback, void *opaque, int64_t delta)
{
#if ENABLE_GTIMER_CHECK
  GTIMER_FCN(mtimer_arm_abs)(id, gti, callback, opaque, mclk() + delta);
#else
  mtimer_arm_abs(gti, callback, opaque, mclk() + delta);
#endif
}

/**
 * the global_lock must be held
 */
void
mtimer_disarm(mtimer_t *mti)
{
  lock_assert(&global_lock);
  tvh_mutex_lock(&mtimer_lock);
  if (mtimer_running == mti)
    mtimer_running = NULL;
  if (mti->mti_callback) {
    mtimer_magic_check(mti);
    LIST_REMOVE(mti, mti_link);
    mti->mti_callback = NULL;
  }
  tvh_mutex_unlock(&mtimer_lock);
}

/**
 *
 */
static int
gtimercmp(gtimer_t *a, gtimer_t *b)
{
  return safecmp(a->gti_expire, b->gti_expire);
}

#if ENABLE_TRACE
static void gtimer_magic_check(gtimer_t *gti)
{
  if (gti->gti_magic1 != GTIMER_MAGIC1) {
    tvhdbg(LS_MAIN, "gtimer magic check failed for %p", gti);
    tvherror(LS_MAIN, "gtimer magic check failed for %p", gti);
    abort();
  }
}
#else
static inline void gtimer_magic_check(gtimer_t *gti)
{
}
#endif

/**
 * this routine can be called inside any locks
 */
void
GTIMER_FCN(gtimer_arm_absn)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t when)
{
  tvh_mutex_lock(&gtimer_lock);

  if (gti->gti_callback != NULL) {
    gtimer_magic_check(gti);
    LIST_REMOVE(gti, gti_link);
  }

#if ENABLE_TRACE
  gti->gti_magic1   = GTIMER_MAGIC1;
#endif
  gti->gti_callback = callback;
  gti->gti_opaque   = opaque;
  gti->gti_expire   = when;
#if ENABLE_GTIMER_CHECK
  gti->gti_id       = id;
#endif

  LIST_INSERT_SORTED(&gtimers, gti, gti_link, gtimercmp);

  if (LIST_FIRST(&gtimers) == gti)
    tvh_cond_signal(&gtimer_cond, 0); // force timer re-check

  tvh_mutex_unlock(&gtimer_lock);
}

/**
 *
 */
void
GTIMER_FCN(gtimer_arm_rel)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t delta)
{
#if ENABLE_GTIMER_CHECK
  GTIMER_FCN(gtimer_arm_absn)(id, gti, callback, opaque, gclk() + delta);
#else
  gtimer_arm_absn(gti, callback, opaque, gclk() + delta);
#endif
}

/**
 * the global_lock must be held
 */
void
gtimer_disarm(gtimer_t *gti)
{
  lock_assert(&global_lock);
  tvh_mutex_lock(&gtimer_lock);
  if (gti == gtimer_running)
    gtimer_running = NULL;
  if (gti->gti_callback) {
    gtimer_magic_check(gti);
    LIST_REMOVE(gti, gti_link);
    gti->gti_callback = NULL;
  }
  tvh_mutex_unlock(&gtimer_lock);
}

/**
 *
 */
tasklet_t *
tasklet_arm_alloc(tsk_callback_t *callback, void *opaque)
{
  tasklet_t *tsk = calloc(1, sizeof(*tsk));
  if (tsk) {
    memoryinfo_alloc(&tasklet_memoryinfo, sizeof(*tsk));
    tsk->tsk_free = free;
    tasklet_arm(tsk, callback, opaque);
  }
  return tsk;
}

/**
 *
 */
void
tasklet_arm(tasklet_t *tsk, tsk_callback_t *callback, void *opaque)
{
  tvh_mutex_lock(&tasklet_lock);

  if (tsk->tsk_callback != NULL)
    TAILQ_REMOVE(&tasklets, tsk, tsk_link);

  tsk->tsk_callback = callback;
  tsk->tsk_opaque   = opaque;

  TAILQ_INSERT_TAIL(&tasklets, tsk, tsk_link);

  if (TAILQ_FIRST(&tasklets) == tsk)
    tvh_cond_signal(&tasklet_cond, 0);

  tvh_mutex_unlock(&tasklet_lock);
}

/**
 *
 */
void
tasklet_disarm(tasklet_t *tsk)
{
  tvh_mutex_lock(&tasklet_lock);

  if(tsk->tsk_callback) {
    TAILQ_REMOVE(&tasklets, tsk, tsk_link);
    tsk->tsk_callback(tsk->tsk_opaque, 1);
    tsk->tsk_callback = NULL;
    if (tsk->tsk_free) tsk->tsk_free(tsk);
  }

  tvh_mutex_unlock(&tasklet_lock);
}

static void
tasklet_flush()
{
  tasklet_t *tsk;

  tvh_mutex_lock(&tasklet_lock);

  while ((tsk = TAILQ_FIRST(&tasklets)) != NULL) {
    TAILQ_REMOVE(&tasklets, tsk, tsk_link);
    tsk->tsk_callback(tsk->tsk_opaque, 1);
    tsk->tsk_callback = NULL;
    if (tsk->tsk_free) {
      memoryinfo_free(&tasklet_memoryinfo, sizeof(*tsk));
      tsk->tsk_free(tsk);
    }
  }

  tvh_mutex_unlock(&tasklet_lock);
}

/**
 *
 */
static void *
tasklet_thread ( void *aux )
{
  tasklet_t *tsk;
  tsk_callback_t *tsk_cb;
  void *opaque;

  tvh_thread_renice(20);

  tvh_mutex_lock(&tasklet_lock);
  while (tvheadend_is_running()) {
    tsk = TAILQ_FIRST(&tasklets);
    if (tsk == NULL) {
      tvh_cond_wait(&tasklet_cond, &tasklet_lock);
      continue;
    }
    /* the callback might re-initialize tasklet, save everything */
    TAILQ_REMOVE(&tasklets, tsk, tsk_link);
    tsk_cb = tsk->tsk_callback;
    opaque = tsk->tsk_opaque;
    tsk->tsk_callback = NULL;
    if (tsk->tsk_free) {
      memoryinfo_free(&tasklet_memoryinfo, sizeof(*tsk));
      tsk->tsk_free(tsk);
    }
    /* now, the callback can be safely called */
    if (tsk_cb) {
      tvh_mutex_unlock(&tasklet_lock);
      tsk_cb(opaque, 0);
      tvh_mutex_lock(&tasklet_lock);
    }
  }
  tvh_mutex_unlock(&tasklet_lock);

  return NULL;
}

/**
 * Show version info
 */
static void
show_version(const char *argv0)
{
  printf("%s: version %s\n", argv0, tvheadend_version);
  exit(0);
}

/**
 *
 */
static void
show_usage
  (const char *argv0, cmdline_opt_t *opts, int num, const char *err, ...)
{
  int i;
  char buf[256];
  printf(_("Usage: %s [OPTIONS]\n"), argv0);
  for (i = 0; i < num; i++) {

    /* Section */
    if (!opts[i].lopt) {
      printf("\n%s\n\n", tvh_gettext(opts[i].desc));

    /* Option */
    } else {
      char sopt[4];
      char *desc, *tok;
      if (opts[i].sopt)
        snprintf(sopt, sizeof(sopt), "-%c,", opts[i].sopt);
      else
        strcpy(sopt, "   ");
      snprintf(buf, sizeof(buf), "  %s --%s", sopt, opts[i].lopt);
      desc = strdup(tvh_gettext(opts[i].desc));
      tok  = strtok(desc, "\n");
      while (tok) {
        printf("%-30s%s\n", buf, tok);
        tok = buf;
        while (*tok) {
          *tok = ' ';
          tok++;
        }
        tok = strtok(NULL, "\n");
      }
      free(desc);
    }
  }
  printf("%s",
         _("\n"
           "For more information please visit the Tvheadend website:\n"
           "https://tvheadend.org\n"));
  exit(0);
}

/**
 * Show subsystems info
 */
static void
show_subsystems(const char *argv0)
{
  int i;
  tvhlog_subsys_t *ts = tvhlog_subsystems;
  printf("Subsystems:\n\n");
  for (i = 1, ts++; i < LS_LAST; i++, ts++) {
    printf("  %-15s %s\n", ts->name, _(ts->desc));
  }
  exit(0);
}

/**
 *
 */
static inline time_t
gdispatch_clock_update(void)
{
  time_t now = time(NULL);
  atomic_set_time_t(&__gdispatch_clock, now);
  return now;
}

/**
 *
 */
static int64_t
mdispatch_clock_update(void)
{
  int64_t mono = getmonoclock();

  if (mono > atomic_get_s64(&mtimer_periodic)) {
    atomic_set_s64(&mtimer_periodic, mono + MONOCLOCK_RESOLUTION);
    gdispatch_clock_update(); /* gclk() update */
    tprofile_log_stats(); /* Log timings */
    comet_flush(); /* Flush idle comet mailboxes */
  }

  atomic_set_s64(&__mdispatch_clock, mono);
  return mono;
}

/**
 *
 */
static void *
mtimer_tick_thread(void *aux)
{
  while (tvheadend_is_running()) {
    /* update clocks each 10x in one second */
    atomic_set_s64(&__mdispatch_clock, getmonoclock());
    tvh_safe_usleep(100000);
  }
  return NULL;
}

/**
 *
 */
static void *
mtimer_thread(void *aux)
{
  mtimer_t *mti;
  mti_callback_t *cb;
  int64_t now, next;
  const char *id;

  tvh_mutex_lock(&mtimer_lock);
  while (tvheadend_is_running() && atomic_get(&tvheadend_mainloop) == 0)
    tvh_cond_wait(&mtimer_cond, &mtimer_lock);
  tvh_mutex_unlock(&mtimer_lock);

  while (tvheadend_is_running()) {
    /* Global monoclock timers */
    now = mdispatch_clock_update();
    next = now + sec2mono(3600);

    while (1) {
      tvh_mutex_lock(&mtimer_lock);
      mti = LIST_FIRST(&mtimers);
      if (mti == NULL || mti->mti_expire > now) {
        if (mti)
          next = mti->mti_expire;
        tvh_mutex_unlock(&mtimer_lock);
        break;
      }

#if ENABLE_GTIMER_CHECK
      id = mti->mti_id;
#else
      id = NULL;
#endif
      cb = mti->mti_callback;
      LIST_REMOVE(mti, mti_link);
      mti->mti_callback = NULL;

      mtimer_running = mti;
      tvh_mutex_unlock(&mtimer_lock);

      tvh_mutex_lock(&global_lock);
      if (mtimer_running == mti) {
        tprofile_start(&mtimer_profile, id);
        cb(mti->mti_opaque);
        tprofile_finish(&mtimer_profile);
      }
      tvh_mutex_unlock(&global_lock);
    }

    /* Periodic updates */
    now = atomic_get_s64(&mtimer_periodic);
    if (next > now)
      next = now;

    /* Wait */
    tvh_mutex_lock(&mtimer_lock);
    tvh_cond_timedwait(&mtimer_cond, &mtimer_lock, next);
    tvh_mutex_unlock(&mtimer_lock);
  }

  return NULL;
}

/**
 *
 */
static void
mainloop(void)
{
  gtimer_t *gti;
  gti_callback_t *cb;
  time_t now;
  struct timespec ts;
  const char *id;

  while (tvheadend_is_running()) {
    now = gdispatch_clock_update();
    ts.tv_sec  = now + 3600;
    ts.tv_nsec = 0;

    while (1) {
      tvh_mutex_lock(&gtimer_lock);
      gti = LIST_FIRST(&gtimers);
      if (gti == NULL || gti->gti_expire > now) {
        if (gti)
          ts.tv_sec = gti->gti_expire;
        tvh_mutex_unlock(&gtimer_lock);
        break;
      }

#if ENABLE_GTIMER_CHECK
      id = gti->gti_id;
#else
      id = NULL;
#endif
      cb = gti->gti_callback;
      LIST_REMOVE(gti, gti_link);
      gti->gti_callback = NULL;
      gtimer_running = gti;
      tvh_mutex_unlock(&gtimer_lock);

      tvh_mutex_lock(&global_lock);
      if (gtimer_running == gti) {
        tprofile_start(&gtimer_profile, id);
        cb(gti->gti_opaque);
        tprofile_finish(&gtimer_profile);
      }
      tvh_mutex_unlock(&global_lock);
    }

    /* Wait */
    tvh_mutex_lock(&gtimer_lock);
    tvh_cond_timedwait_ts(&gtimer_cond, &gtimer_lock, &ts);
    tvh_mutex_unlock(&gtimer_lock);
  }
}


/**
 *
 */
int
main(int argc, char **argv)
{
  int i;
  sigset_t set;
#if ENABLE_MPEGTS
  uint32_t adapter_mask = 0;
#endif
  int  log_level   = LOG_INFO;
  int  log_options = TVHLOG_OPT_MILLIS | TVHLOG_OPT_STDERR | TVHLOG_OPT_SYSLOG;
  const char *log_debug = NULL, *log_trace = NULL;
  gid_t gid = -1;
  uid_t uid = -1;
  char buf[512];
  FILE *pidfile = NULL;
  static struct {
    void *thread_id;
    struct timeval tv;
    uint8_t ru[32];
  } randseed;
  struct rlimit rl;
  extern int dvb_bouquets_parse;
#if ENABLE_VAAPI
  extern int vainfo_probe_enabled;
#endif
  main_tid = pthread_self();

  /* Setup global mutexes */
  tvh_mutex_init(&fork_lock, NULL);
  tvh_mutex_init(&global_lock, NULL);
  tvh_mutex_init(&mtimer_lock, NULL);
  tvh_mutex_init(&gtimer_lock, NULL);
  tvh_mutex_init(&tasklet_lock, NULL);
  tvh_mutex_init(&atomic_lock, NULL);
  tvh_cond_init(&mtimer_cond, 1);
  tvh_cond_init(&gtimer_cond, 0);
  tvh_cond_init(&tasklet_cond, 1);
  TAILQ_INIT(&tasklets);

  /* Defaults */
  tvheadend_webui_port      = 9981;
  tvheadend_webroot         = NULL;
  tvheadend_htsp_port       = 9982;
  tvheadend_htsp_port_extra = 0;
  __mdispatch_clock = getmonoclock();
  __gdispatch_clock = time(NULL);

  /* Command line options */
  int         opt_help         = 0,
              opt_version      = 0,
              opt_fork         = 0,
              opt_firstrun     = 0,
              opt_stderr       = 0,
              opt_nostderr     = 0,
              opt_syslog       = 0,
              opt_nosyslog     = 0,
              opt_uidebug      = 0,
              opt_abort        = 0,
              opt_noacl        = 0,
              opt_fileline     = 0,
              opt_threadid     = 0,
              opt_libav        = 0,
              opt_ipv6         = 0,
              opt_nosatipcli   = 0,
              opt_satip_rtsp   = 0,
#if ENABLE_TSFILE || ENABLE_TSDEBUG
              opt_tsfile_tuner = 0,
#endif
              opt_dump         = 0,
              opt_xspf         = 0,
              opt_dbus         = 0,
              opt_dbus_session = 0,
              opt_nobackup     = 0,
              opt_nobat        = 0,
              opt_subsystems   = 0,
              opt_tprofile     = 0,
              opt_thread_debug = 0;
  const char *opt_config       = NULL,
             *opt_user         = NULL,
             *opt_group        = NULL,
             *opt_logpath      = NULL,
             *opt_log_debug    = NULL,
             *opt_log_trace    = NULL,
             *opt_pidpath      = "/run/tvheadend.pid",
#if ENABLE_LINUXDVB
             *opt_dvb_adapters = NULL,
#endif
             *opt_bindaddr     = NULL,
             *opt_subscribe    = NULL,
             *opt_user_agent   = NULL,
             *opt_satip_bindaddr = NULL;
  static char *__opt_satip_xml[10];
  str_list_t  opt_satip_xml    = { .max = 10, .num = 0, .str = __opt_satip_xml };
#if ENABLE_TSFILE || ENABLE_TSDEBUG
  static char *__opt_tsfile[10];
  str_list_t  opt_tsfile       = { .max = 10, .num = 0, .str = __opt_tsfile };
#endif
  cmdline_opt_t cmdline_opts[] = {
    {   0, NULL,        N_("Generic options"),         OPT_BOOL, NULL         },
    { 'h', "help",      N_("Show this page"),          OPT_BOOL, &opt_help    },
    { 'v', "version",   N_("Show version information"),OPT_BOOL, &opt_version },

    {   0, NULL,        N_("Service configuration"),   OPT_BOOL, NULL         },
    { 'c', "config",    N_("Alternate configuration path"), OPT_STR,  &opt_config  },
    { 'B', "nobackup",  N_("Don't backup configuration tree at upgrade"), OPT_BOOL, &opt_nobackup },
    { 'f', "fork",      N_("Fork and run as daemon"),  OPT_BOOL, &opt_fork    },
    { 'u', "user",      N_("Run as user"),             OPT_STR,  &opt_user    },
    { 'g', "group",     N_("Run as group"),            OPT_STR,  &opt_group   },
    { 'p', "pid",       N_("Alternate PID path"),      OPT_STR,  &opt_pidpath },
    { 'C', "firstrun",  N_("If no user account exists then create one with\n"
	                   "no username and no password. Use with care as\n"
	                   "it will allow world-wide administrative access\n"
	                   "to your Tvheadend installation until you create or edit\n"
	                   "the access control from within the Tvheadend web interface."),
      OPT_BOOL, &opt_firstrun },
#if ENABLE_DBUS_1
    { 'U', "dbus",      N_("Enable DBus"),
      OPT_BOOL, &opt_dbus },
    { 'e', "dbus_session", N_("DBus - use the session message bus instead of the system one"),
      OPT_BOOL, &opt_dbus_session },
#endif
#if ENABLE_LINUXDVB
    { 'a', "adapters",  N_("Only use specified DVB adapters (comma-separated, -1 = none)"),
      OPT_STR, &opt_dvb_adapters },
#endif
#if ENABLE_SATIP_SERVER
    {   0, "satip_bindaddr", N_("Specify bind address for SAT>IP server"),
      OPT_STR, &opt_satip_bindaddr },
    {   0, "satip_rtsp", N_("SAT>IP RTSP port number for server\n"
                            "(default: -1 = disable, 0 = webconfig, standard port is 554)"),
      OPT_INT, &opt_satip_rtsp },
#endif
#if ENABLE_SATIP_CLIENT
    {   0, "nosatip",    N_("Disable SAT>IP client (deprecated flag, use nosatipcli)"),
      OPT_BOOL, &opt_nosatipcli },
    {   0, "nosatipcli",    N_("Disable SAT>IP client"),
      OPT_BOOL, &opt_nosatipcli },
    {   0, "satip_xml",  N_("URL with the SAT>IP server XML location"),
      OPT_STR_LIST, &opt_satip_xml },
#endif
    {   0, NULL,         N_("Server connectivity"),    OPT_BOOL, NULL         },
    { '6', "ipv6",       N_("Listen on IPv6"),         OPT_BOOL, &opt_ipv6    },
    { 'b', "bindaddr",   N_("Specify bind address"),   OPT_STR,  &opt_bindaddr},
    {   0, "http_port",  N_("Specify alternative http port"),
      OPT_INT, &tvheadend_webui_port },
    {   0, "http_root",  N_("Specify alternative http webroot"),
      OPT_STR, &tvheadend_webroot },
    {   0, "htsp_port",  N_("Specify alternative htsp port"),
      OPT_INT, &tvheadend_htsp_port },
    {   0, "htsp_port2", N_("Specify extra htsp port"),
      OPT_INT, &tvheadend_htsp_port_extra },
    {   0, "useragent",  N_("Specify User-Agent header for the http client"),
      OPT_STR, &opt_user_agent },
    {   0, "xspf",       N_("Use XSPF playlist instead of M3U"),
      OPT_BOOL, &opt_xspf },

    {   0, NULL,        N_("Debug options"),           OPT_BOOL, NULL         },
    { 'd', "stderr",    N_("Enable debug on stderr"),  OPT_BOOL, &opt_stderr  },
    { 'n', "nostderr",  N_("Disable debug on stderr"), OPT_BOOL, &opt_nostderr },
    { 's', "syslog",    N_("Enable debug to syslog"),  OPT_BOOL, &opt_syslog  },
    { 'S', "nosyslog",  N_("Disable syslog (all messages)"), OPT_BOOL, &opt_nosyslog },
    { 'l', "logfile",   N_("Enable debug to file"),    OPT_STR,  &opt_logpath },
    {   0, "debug",     N_("Enable debug subsystems"),  OPT_STR,  &opt_log_debug },
#if ENABLE_TRACE
    {   0, "trace",     N_("Enable trace subsystems"), OPT_STR,  &opt_log_trace },
#endif
    {   0, "subsystems",N_("List subsystems"),         OPT_BOOL, &opt_subsystems },
    {   0, "fileline",  N_("Add file and line numbers to debug"), OPT_BOOL, &opt_fileline },
    {   0, "threadid",  N_("Add the thread ID to debug"), OPT_BOOL, &opt_threadid },
#if ENABLE_LIBAV
    {   0, "libav",     N_("More verbose libav log"),  OPT_BOOL, &opt_libav },
#endif
    {   0, "uidebug",   N_("Enable web UI debug (non-minified JS)"), OPT_BOOL, &opt_uidebug },
    { 'A', "abort",     N_("Immediately abort"),       OPT_BOOL, &opt_abort   },
    { 'D', "dump",      N_("Enable coredumps for daemon"), OPT_BOOL, &opt_dump },
    {   0, "noacl",     N_("Disable all access control checks"),
      OPT_BOOL, &opt_noacl },
    {   0, "nobat",     N_("Disable DVB bouquets"),
      OPT_BOOL, &opt_nobat },
    { 'j', "join",      N_("Subscribe to a service permanently"),
      OPT_STR, &opt_subscribe },


#if ENABLE_TSFILE || ENABLE_TSDEBUG
    { 0, NULL, N_("Testing options"), OPT_BOOL, NULL },
    { 0, "tsfile_tuners", N_("Number of tsfile tuners"), OPT_INT, &opt_tsfile_tuner },
    { 0, "tsfile", N_("tsfile input (mux file)"), OPT_STR_LIST, &opt_tsfile },
#endif

    { 0, "tprofile", N_("Gather timing statistics for the code"), OPT_BOOL, &opt_tprofile },
#if ENABLE_TRACE
    { 0, "thrdebug", N_("Thread debugging"), OPT_INT, &opt_thread_debug },
#endif

  };

  /* Get current directory */
  tvheadend_cwd0 = dirname(tvh_strdupa(argv[0]));
  tvheadend_cwd = dirname(tvh_strdupa(tvheadend_cwd0));

  /* Set locale */
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");
  tvh_gettext_init();

  /* make sure the timezone is set */
  tzset();

  /* Process command line */
  for (i = 1; i < argc; i++) {

    /* Find option */
    cmdline_opt_t *opt
      = cmdline_opt_find(cmdline_opts, ARRAY_SIZE(cmdline_opts), argv[i]);
    if (!opt) {
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts),
                 _("invalid option specified [%s]"), argv[i]);
      continue;
    }

    /* Process */
    if (opt->type == OPT_BOOL)
      *((int*)opt->param) = 1;
    else if (++i == argc)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts),
                 _("option %s requires a value"), opt->lopt);
    else if (opt->type == OPT_INT)
      *((int*)opt->param) = atoi(argv[i]);
    else if (opt->type == OPT_STR_LIST) {
      str_list_t *strl = opt->param;
      if (strl->num < strl->max)
        strl->str[strl->num++] = argv[i];
    }
    else
      *((char**)opt->param) = argv[i];

    /* Stop processing */
    if (opt_help)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts), NULL);
    if (opt_version)
      show_version(argv[0]);
    if (opt_subsystems)
      show_subsystems(argv[0]);
  }

  /* Additional cmdline processing */
  if (opt_nobat)
    dvb_bouquets_parse = 0;
#if ENABLE_LINUXDVB
  if (!opt_dvb_adapters) {
    adapter_mask = ~0;
  } else {
    char *p, *e;
    char *r = NULL;
    char *dvb_adapters = strdup(opt_dvb_adapters);
    adapter_mask = 0x0;
    i = 0;
    p = strtok_r(dvb_adapters, ",", &r);
    while (p) {
      int a = strtol(p, &e, 10);
      if (*e != 0 || a > 31) {
        fprintf(stderr, _("Invalid adapter number '%s'\n"), p);
        free(dvb_adapters);
        return 1;
      }
      i = 1;
      if (a < 0)
        adapter_mask = 0;
      else
        adapter_mask |= (1 << a);
      p = strtok_r(NULL, ",", &r);
    }
    free(dvb_adapters);
    if (!i) {
      fprintf(stderr, "%s", _("No adapters specified!\n"));
      return 1;
    }
  }
#endif
  if (tvheadend_webroot) {
    char *tmp;
    if (*tvheadend_webroot == '/')
      tmp = strdup(tvheadend_webroot);
    else {
      tmp = malloc(strlen(tvheadend_webroot)+2);
      *tmp = '/';
      strcpy(tmp+1, tvheadend_webroot);
    }
    if (tmp[strlen(tmp)-1] == '/')
      tmp[strlen(tmp)-1] = '\0';
    if (tmp[0])
      tvheadend_webroot = tmp;
    else
      free(tmp);
  }
  tvheadend_webui_debug = opt_uidebug;

  /* Setup logging */
  if (isatty(2))
    log_options |= TVHLOG_OPT_DECORATE;
  if (opt_stderr || opt_syslog || opt_logpath) {
    if (!opt_log_trace && !opt_log_debug)
      log_debug      = "all";
    log_level      = LOG_DEBUG;
    if (opt_stderr)
      log_options   |= TVHLOG_OPT_DBG_STDERR;
    if (opt_syslog)
      log_options   |= TVHLOG_OPT_DBG_SYSLOG;
    if (opt_logpath)
      log_options   |= TVHLOG_OPT_DBG_FILE;
  }
  if (opt_nostderr)
    log_options &= ~(TVHLOG_OPT_DECORATE|TVHLOG_OPT_STDERR|TVHLOG_OPT_DBG_STDERR);
  if (opt_nosyslog)
    log_options &= ~(TVHLOG_OPT_SYSLOG|TVHLOG_OPT_DBG_SYSLOG);
  if (opt_fileline)
    log_options |= TVHLOG_OPT_FILELINE;
  if (opt_threadid)
    log_options |= TVHLOG_OPT_THREAD;
  if (opt_libav)
    log_options |= TVHLOG_OPT_LIBAV;
  if (opt_log_trace) {
    log_level  = LOG_TRACE;
    log_trace  = opt_log_trace;
  }
  if (opt_log_debug)
    log_debug  = opt_log_debug;

  tvh_thread_init(opt_thread_debug);

  tvhlog_init(log_level, log_options, opt_logpath);
  tvhlog_set_debug(log_debug);
  tvhlog_set_trace(log_trace);
  tvhinfo(LS_MAIN, "Log started");

  tvh_signal(SIGPIPE, handle_sigpipe); // will be redundant later
  tvh_signal(SIGILL, handle_sigill);   // see handler..

  if (opt_fork && !opt_user && !opt_config)
    tvhwarn(LS_START, "Forking without --user or --config may use unexpected configuration location");

  /* Set privileges */
  if((opt_fork && getuid() == 0) || opt_group || opt_user) {
    const char *homedir;
    struct group  *grp = getgrnam(opt_group ?: "video");
    struct passwd *pw  = getpwnam(opt_user ?: "daemon");

    if(grp != NULL) {
      gid = grp->gr_gid;
    } else {
      gid = 1;
    }

    if (pw != NULL) {
      if (getuid() != pw->pw_uid) {
        gid_t glist[16];
        int gnum;
        gnum = get_user_groups(pw, glist, ARRAY_SIZE(glist));
        if (gnum > 0 && setgroups(gnum, glist)) {
          char buf[256] = "";
          int i;
          for (i = 0; i < gnum; i++)
            snprintf(buf + strlen(buf), sizeof(buf) - 1 - strlen(buf),
                     ",%d", glist[i]);
          tvhlog(LOG_ALERT, LS_START,
                 "setgroups(%s) failed, do you have permission?", buf+1);
          return 1;
        }
      }
      uid     = pw->pw_uid;
      homedir = pw->pw_dir;
      setenv("HOME", homedir, 1);
    } else {
      uid = 1;
    }
  }

  tprofile_module_init(opt_tprofile);
  tprofile_init(&gtimer_profile, "gtimer");
  tprofile_init(&mtimer_profile, "mtimer");
  uuid_init();
  idnode_boot();
  config_boot(opt_config, gid, uid, opt_user_agent);
  tcp_server_preinit(opt_ipv6);
  http_server_init(opt_bindaddr);    // bind to ports only
  htsp_init(opt_bindaddr);	     // bind to ports only
  satip_server_init(opt_satip_bindaddr, opt_satip_rtsp); // bind to ports only

  if (opt_fork)
    pidfile = tvh_fopen(opt_pidpath, "w+");

  if (gid != -1 && (getgid() != gid) && setgid(gid)) {
    tvhlog(LOG_ALERT, LS_START,
           "setgid(%d) failed, do you have permission?", gid);
    return 1;
  }
  if (uid != -1 && (getuid() != uid) && setuid(uid)) {
    tvhlog(LOG_ALERT, LS_START,
           "setuid(%d) failed, do you have permission?", uid);
    return 1;
  }

  /* Daemonise */
  if(opt_fork) {
    if(daemon(0, 0)) {
      exit(2);
    }
    if(pidfile != NULL) {
      fprintf(pidfile, "%d\n", getpid());
      fclose(pidfile);
    }

    /* Make dumpable */
    if (opt_dump) {
      if (chdir("/tmp"))
        tvhwarn(LS_START, "failed to change cwd to /tmp");
#ifdef PLATFORM_LINUX
      prctl(PR_SET_DUMPABLE, 1);
#else
      tvhwarn(LS_START, "Coredumps not implemented on your platform");
#endif
    }

    umask(0);
  }

  memset(&rl, 0, sizeof(rl));
  if (getrlimit(RLIMIT_STACK, &rl) || rl.rlim_cur < 2*1024*1024) {
    rlim_t rl2 = rl.rlim_cur;
    rl.rlim_cur = 2*1024*1024;
    if (setrlimit(RLIMIT_STACK, &rl)) {
      tvhlog(LOG_ALERT, LS_START, "too small stack size - %ld", (long)rl2);
      return 1;
    }
  }

  atomic_set(&tvheadend_running, 1);
  atomic_set(&tvheadend_mainloop, 0);

  /* Start log thread (must be done post fork) */
  tvhlog_start();

  /* Alter logging */
  if (opt_fork)
    tvhlog_options &= ~TVHLOG_OPT_STDERR;
  if (!isatty(2))
    tvhlog_options &= ~TVHLOG_OPT_DECORATE;

  /* Initialise clock */
  tvh_mutex_lock(&global_lock);
  __mdispatch_clock = getmonoclock();
  __gdispatch_clock = time(NULL);

  /* Signal handling */
  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);
  trap_init(argv[0]);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  /* SSL library init */
  OPENSSL_config(NULL);
  SSL_load_error_strings();
  SSL_library_init();
#endif

#ifndef OPENSSL_NO_ENGINE
  /* ENGINE init */
  ENGINE_load_builtin_engines();
#endif

  /* Rand seed */
  randseed.thread_id = (void *)main_tid;
  gettimeofday(&randseed.tv, NULL);
  uuid_random(randseed.ru, sizeof(randseed.ru));
  RAND_seed(&randseed, sizeof(randseed));

  /* Initialise configuration */
  tvhftrace(LS_MAIN, notify_init);
  tvhftrace(LS_MAIN, spawn_init);
  tvhftrace(LS_MAIN, idnode_init);
  tvhftrace(LS_MAIN, config_init, opt_nobackup == 0);

  /* Memoryinfo */
  idclass_register(&memoryinfo_class);
  memoryinfo_register(&tasklet_memoryinfo);
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_register(&htsmsg_memoryinfo);
  memoryinfo_register(&htsmsg_field_memoryinfo);
#endif
  memoryinfo_register(&pkt_memoryinfo);
  memoryinfo_register(&pktbuf_memoryinfo);
  memoryinfo_register(&pktref_memoryinfo);

  /**
   * Initialize subsystems
   */

  epg_in_load = 1;

  tvh_thread_create(&mtimer_tick_tid, NULL, mtimer_tick_thread, NULL, "mtick");
  tvh_thread_create(&mtimer_tid, NULL, mtimer_thread, NULL, "mtimer");
  tvh_thread_create(&tasklet_tid, NULL, tasklet_thread, NULL, "tasklet");

#if CONFIG_LINUXDVB_CA
  en50221_register_apps();
#endif

  tvhftrace(LS_MAIN, streaming_init);
  tvhftrace(LS_MAIN, tvh_hardware_init);
  tvhftrace(LS_MAIN, dbus_server_init, opt_dbus, opt_dbus_session);
  tvhftrace(LS_MAIN, intlconv_init);
  tvhftrace(LS_MAIN, api_init);
  tvhftrace(LS_MAIN, fsmonitor_init);
  tvhftrace(LS_MAIN, libav_init);
  tvhftrace(LS_MAIN, tvhtime_init);
#if ENABLE_VAAPI
  tvhftrace(LS_MAIN, codec_init, vainfo_probe_enabled);
#else
  tvhftrace(LS_MAIN, codec_init);
#endif
  tvhftrace(LS_MAIN, profile_init);
  tvhftrace(LS_MAIN, imagecache_init);
  tvhftrace(LS_MAIN, http_client_init);
  tvhftrace(LS_MAIN, esfilter_init);
  tvhftrace(LS_MAIN, bouquet_init);
  tvhftrace(LS_MAIN, ratinglabel_init);
  tvhftrace(LS_MAIN, service_init);
  tvhftrace(LS_MAIN, descrambler_init);
  tvhftrace(LS_MAIN, dvb_init);
#if ENABLE_MPEGTS
#if ENABLE_TSFILE || ENABLE_TSDEBUG
  tvhftrace(LS_MAIN, mpegts_init, adapter_mask, opt_nosatipcli, &opt_satip_xml,
            &opt_tsfile, opt_tsfile_tuner);
#else
  tvhftrace(LS_MAIN, mpegts_init, adapter_mask, opt_nosatipcli, &opt_satip_xml,
            NULL, 0);
#endif
#endif
  tvhftrace(LS_MAIN, channel_init);
  tvhftrace(LS_MAIN, bouquet_service_resolve);
  tvhftrace(LS_MAIN, subscription_init);
  tvhftrace(LS_MAIN, dvr_config_init);
  tvhftrace(LS_MAIN, access_init, opt_firstrun, opt_noacl);
#if ENABLE_TIMESHIFT
  tvhftrace(LS_MAIN, timeshift_init);
#endif
  tvhftrace(LS_MAIN, tcp_server_init);
  tvhftrace(LS_MAIN, webui_init, opt_xspf);
#if ENABLE_UPNP
  tvhftrace(LS_MAIN, upnp_server_init, opt_bindaddr);
#endif
  tvhftrace(LS_MAIN, service_mapper_init);
  tvhftrace(LS_MAIN, epggrab_init);
  tvhftrace(LS_MAIN, epg_init);
  tvhftrace(LS_MAIN, dvr_init);
  tvhftrace(LS_MAIN, dbus_server_start);
  tvhftrace(LS_MAIN, http_server_register);
  tvhftrace(LS_MAIN, satip_server_register);
  tvhftrace(LS_MAIN, htsp_register);

  if(opt_subscribe != NULL)
    subscription_dummy_join(opt_subscribe, 1);

  tvhftrace(LS_MAIN, avahi_init);
  tvhftrace(LS_MAIN, bonjour_init);

  tvhftrace(LS_MAIN, epg_updated); // cleanup now all prev ref's should have been created
  epg_in_load = 0;

  tvh_mutex_unlock(&global_lock);

  tvhftrace(LS_MAIN, watchdog_init);

  /**
   * Wait for SIGTERM / SIGINT, but only in this thread
   */

  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);

  tvh_signal(SIGTERM, doexit);
  tvh_signal(SIGINT, doexit);

  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  tvhlog(LOG_NOTICE, LS_START, "HTS Tvheadend version %s started, "
         "running as PID:%d UID:%d GID:%d, CWD:%s CNF:%s",
         tvheadend_version,
         getpid(), getuid(), getgid(), getcwd(buf, sizeof(buf)),
         hts_settings_get_root());

  if(opt_abort)
    abort();

  tvh_mutex_lock(&mtimer_lock);
  tvheadend_mainloop = 1;
  tvh_cond_signal(&mtimer_cond, 0);
  tvh_mutex_unlock(&mtimer_lock);
  mainloop();
  tvh_mutex_lock(&gtimer_lock);
  tvh_cond_signal(&gtimer_cond, 0);
  tvh_mutex_unlock(&gtimer_lock);
  pthread_join(mtimer_tid, NULL);

#if ENABLE_DBUS_1
  tvhftrace(LS_MAIN, dbus_server_done);
#endif
#if ENABLE_UPNP
  tvhftrace(LS_MAIN, upnp_server_done);
#endif
  tvhftrace(LS_MAIN, satip_server_done);
  tvhftrace(LS_MAIN, htsp_done);
  tvhftrace(LS_MAIN, http_server_done);
  tvhftrace(LS_MAIN, webui_done);
  tvhftrace(LS_MAIN, fsmonitor_done);
  tvhftrace(LS_MAIN, http_client_done);
  tvhftrace(LS_MAIN, tcp_server_done);

  // Note: the locking is obviously a bit redundant, but without
  //       we need to disable the gtimer_arm call in epg_save()
  tvh_mutex_lock(&global_lock);
  tvhftrace(LS_MAIN, epg_save);

#if ENABLE_TIMESHIFT
  tvhftrace(LS_MAIN, timeshift_term);
#endif
  tvh_mutex_unlock(&global_lock);

  tvhftrace(LS_MAIN, epggrab_done);
#if ENABLE_MPEGTS
  tvhftrace(LS_MAIN, mpegts_done);
#endif
  tvhftrace(LS_MAIN, dvr_done);
  tvhftrace(LS_MAIN, descrambler_done);
  tvhftrace(LS_MAIN, service_mapper_done);
  tvhftrace(LS_MAIN, service_done);
  tvhftrace(LS_MAIN, channel_done);
  tvhftrace(LS_MAIN, bouquet_done);
  tvhftrace(LS_MAIN, ratinglabel_done);
  tvhftrace(LS_MAIN, subscription_done);
  tvhftrace(LS_MAIN, access_done);
  tvhftrace(LS_MAIN, epg_done);
  tvhftrace(LS_MAIN, avahi_done);
  tvhftrace(LS_MAIN, bonjour_done);
  tvhftrace(LS_MAIN, imagecache_done);
  tvhftrace(LS_MAIN, lang_code_done);
  tvhftrace(LS_MAIN, api_done);

  tvhtrace(LS_MAIN, "tasklet enter");
  tvh_cond_signal(&tasklet_cond, 0);
  pthread_join(tasklet_tid, NULL);
  tvhtrace(LS_MAIN, "tasklet thread end");
  tasklet_flush();
  tvhtrace(LS_MAIN, "tasklet leave");
  tvhtrace(LS_MAIN, "mtimer tick thread join enter");
  pthread_join(mtimer_tick_tid, NULL);
  tvhtrace(LS_MAIN, "mtimer tick thread join leave");

  tvhftrace(LS_MAIN, dvb_done);
  tvhftrace(LS_MAIN, esfilter_done);
  tvhftrace(LS_MAIN, profile_done);
  tvhftrace(LS_MAIN, codec_done);
  tvhftrace(LS_MAIN, libav_done);
  tvhftrace(LS_MAIN, intlconv_done);
  tvhftrace(LS_MAIN, urlparse_done);
  tvhftrace(LS_MAIN, streaming_done);
  tvhftrace(LS_MAIN, idnode_done);
  tvhftrace(LS_MAIN, notify_done);
  tvhftrace(LS_MAIN, spawn_done);

  tprofile_done(&gtimer_profile);
  tprofile_done(&mtimer_profile);
  tprofile_module_done();
  tvhlog(LOG_NOTICE, LS_STOP, "Exiting HTS Tvheadend");
  tvhlog_end();

  tvhftrace(LS_MAIN, config_done);
  tvhftrace(LS_MAIN, hts_settings_done);

  tvh_thread_done();

  if(opt_fork)
    unlink(opt_pidpath);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  /* OpenSSL - welcome to the "cleanup" hell */
#ifndef OPENSSL_NO_ENGINE
  ENGINE_cleanup();
#endif
  RAND_cleanup();
  CRYPTO_cleanup_all_ex_data();
  EVP_cleanup();
  CONF_modules_free();
#if !defined(OPENSSL_NO_COMP)
  COMP_zlib_cleanup();
#endif
  ERR_remove_thread_state(NULL);
  ERR_free_strings();
#if !defined(OPENSSL_NO_COMP) && OPENSSL_VERSION_NUMBER < 0x1010006f
  sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
#endif
  /* end of OpenSSL cleanup code */
#endif

#if ENABLE_DBUS_1
  extern void dbus_shutdown(void);
  if (opt_dbus) dbus_shutdown();
#endif
  tvh_gettext_done();
  free((char *)tvheadend_webroot);

  tvhftrace(LS_MAIN, watchdog_done);
  return 0;
}

/**
 *
 */
void
tvh_str_set(char **strp, const char *src)
{
  free(*strp);
  *strp = src ? strdup(src) : NULL;
}


/**
 *
 */
int
tvh_str_update(char **strp, const char *src)
{
  if(src == NULL)
    return 0;
  free(*strp);
  *strp = strdup(src);
  return 1;
}


/**
 *
 */
htsmsg_t *tvheadend_capabilities_list(int check)
{
  const tvh_caps_t *tc = tvheadend_capabilities;
  htsmsg_t *r = htsmsg_create_list();
  while (tc->name) {
    if (!check || !tc->enabled || *tc->enabled)
      htsmsg_add_str(r, NULL, tc->name);
    tc++;
  }
  if (config.caclient_ui)
    htsmsg_add_str(r, NULL, "caclient_advanced");
  return r;
}

/**
 *
 */
void time_t_out_of_range_notify(int64_t val)
{
  tvherror(LS_MAIN, "time value out of range (%"PRId64") of time_t", val);
}
