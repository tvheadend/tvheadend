/*
 *  TVheadend
 *  Copyright (C) 2007 - 2010 Andreas �man
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

#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
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
#include "tcp.h"
#include "access.h"
#include "http.h"
#include "webui/webui.h"
#include "dvb/dvb.h"
#include "epggrab.h"
#include "spawn.h"
#include "subscriptions.h"
#include "serviceprobe.h"
#include "cwc.h"
#include "capmt.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "rawtsinput.h"
#include "avahi.h"
#include "iptv_input.h"
#include "service.h"
#include "v4l.h"
#include "trap.h"
#include "settings.h"
#include "ffdecsa/FFdecsa.h"
#include "muxes.h"
#include "config2.h"
#include "imagecache.h"
#include "timeshift.h"
#if ENABLE_LIBAV
#include "libav.h"
#endif

/* Command line option struct */
typedef struct {
  const char  sopt;
  const char *lopt;
  const char *desc;
  enum {
    OPT_STR,
    OPT_INT,
    OPT_BOOL
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
int              tvheadend_webui_port;
int              tvheadend_webui_debug;
int              tvheadend_htsp_port;
int              tvheadend_htsp_port_extra;
const char      *tvheadend_cwd;
const char      *tvheadend_webroot;
const tvh_caps_t tvheadend_capabilities[] = {
#if ENABLE_CWC
  { "cwc", NULL },
#endif
#if ENABLE_V4L
  { "v4l", NULL },
#endif
#if ENABLE_LINUXDVB
  { "linuxdvb", NULL },
#endif
#if ENABLE_IMAGECACHE
  { "imagecache", &imagecache_enabled },
#endif
#if ENABLE_TIMESHIFT
  { "timeshift", &timeshift_enabled },
#endif
  { NULL, NULL }
};

time_t dispatch_clock;
pthread_mutex_t global_lock;
pthread_mutex_t ffmpeg_lock;
pthread_mutex_t fork_lock;

/*
 * Locals
 */
static int running;
static int log_stderr;
static int log_decorate;
static LIST_HEAD(, gtimer) gtimers;
static int log_debug_to_syslog;
static int log_debug_to_console;

static void
handle_sigpipe(int x)
{
  return;
}

static void
doexit(int x)
{
  running = 0;
}

static int
get_user_groups (const struct passwd *pw, gid_t* glist, size_t gmax)
{
  int num = 0;
  struct group *gr;
  char **mem;
  glist[num++] = pw->pw_gid;
  for ( gr = getgrent(); (gr != NULL) && (num < gmax); gr = getgrent() ) {
    if (gr->gr_gid == pw->pw_gid) continue;
    for (mem = gr->gr_mem; *mem; mem++) {
      if(!strcmp(*mem, pw->pw_name)) glist[num++] = gr->gr_gid;
    }
  }
  return num;
}

/**
 *
 */
static int
gtimercmp(gtimer_t *a, gtimer_t *b)
{
  if(a->gti_expire < b->gti_expire)
    return -1;
  else if(a->gti_expire > b->gti_expire)
    return 1;
 return 0;
}


/**
 *
 */
void
gtimer_arm_abs(gtimer_t *gti, gti_callback_t *callback, void *opaque,
	       time_t when)
{
  lock_assert(&global_lock);

  if(gti->gti_callback != NULL)
    LIST_REMOVE(gti, gti_link);
    
  gti->gti_callback = callback;
  gti->gti_opaque = opaque;
  gti->gti_expire = when;

  LIST_INSERT_SORTED(&gtimers, gti, gti_link, gtimercmp);
}

/**
 *
 */
void
gtimer_arm(gtimer_t *gti, gti_callback_t *callback, void *opaque, int delta)
{
  time_t now;
  time(&now);
  
  gtimer_arm_abs(gti, callback, opaque, now + delta);
}

/**
 *
 */
void
gtimer_disarm(gtimer_t *gti)
{
  if(gti->gti_callback) {
    LIST_REMOVE(gti, gti_link);
    gti->gti_callback = NULL;
  }
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
  printf("Usage :- %s [options]\n\n", argv0);
  printf("Options\n");
  for (i = 0; i < num; i++) {

    /* Section */
    if (!opts[i].lopt) {
      printf("\n%s\n\n",
            opts[i].desc);

    /* Option */
    } else {
      char sopt[4];
      char *desc, *tok;
      if (opts[i].sopt)
        snprintf(sopt, sizeof(sopt), "-%c/", opts[i].sopt);
      else
        sopt[0] = 0;
      snprintf(buf, sizeof(buf), "  %s--%s", sopt, opts[i].lopt);
      desc = strdup(opts[i].desc);
      tok  = strtok(desc, "\n");
      while (tok) {
        printf("%s\t\t%s\n", buf, tok);
        tok = buf;
        while (*tok) {
          *tok = ' ';
          tok++;
        }
        tok = strtok(NULL, "\n");
      }
    }
  }
  printf("\n");
  printf("For more information read the man page or visit\n");
  printf(" http://www.lonelycoder.com/hts/\n");
  printf("\n");
  exit(0);
}



/**
 *
 */
static void
mainloop(void)
{
  gtimer_t *gti;
  gti_callback_t *cb;

  while(running) {
    sleep(1);
    spawn_reaper();

    time(&dispatch_clock);

    comet_flush(); /* Flush idle comet mailboxes */

    pthread_mutex_lock(&global_lock);
    
    while((gti = LIST_FIRST(&gtimers)) != NULL) {
      if(gti->gti_expire > dispatch_clock)
	break;
      
      cb = gti->gti_callback;
      LIST_REMOVE(gti, gti_link);
      gti->gti_callback = NULL;

      cb(gti->gti_opaque);
      
    }
    pthread_mutex_unlock(&global_lock);
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
#if ENABLE_LINUXDVB
  uint32_t adapter_mask;
#endif

  /* Defaults */
  log_stderr                = 1;
  log_decorate              = isatty(2);
  log_debug_to_syslog       = 0;
  log_debug_to_console      = 0;
  tvheadend_webui_port      = 9981;
  tvheadend_webroot         = NULL;
  tvheadend_htsp_port       = 9982;
  tvheadend_htsp_port_extra = 0;

  /* Command line options */
  int         opt_help         = 0,
              opt_version      = 0,
              opt_fork         = 0,
              opt_firstrun     = 0,
              opt_debug        = 0,
              opt_syslog       = 0,
              opt_uidebug      = 0,
              opt_abort        = 0;
  const char *opt_config       = NULL,
             *opt_user         = NULL,
             *opt_group        = NULL,
             *opt_pidpath      = "/var/run/tvheadend.pid",
#if ENABLE_LINUXDVB
             *opt_dvb_adapters = NULL,
             *opt_dvb_raw      = NULL,
#endif
             *opt_rawts        = NULL,
             *opt_subscribe    = NULL;
  cmdline_opt_t cmdline_opts[] = {
    {   0, NULL,        "Generic Options",         OPT_BOOL, NULL         },
    { 'h', "help",      "Show this page",          OPT_BOOL, &opt_help    },
    { 'v', "version",   "Show version infomation", OPT_BOOL, &opt_version },

    {   0, NULL,        "Service Configuration",   OPT_BOOL, NULL         },
    { 'c', "config",    "Alternate config path",   OPT_STR,  &opt_config  },
    { 'f', "fork",      "Fork and run as daemon",  OPT_BOOL, &opt_fork    },
    { 'u', "user",      "Run as user",             OPT_STR,  &opt_user    },
    { 'g', "group",     "Run as group",            OPT_STR,  &opt_group   },
    { 'p', "pid",       "Alternate pid path",      OPT_STR,  &opt_pidpath },
    { 'C', "firstrun",  "If no useraccount exist then create one with\n"
	                      "no username and no password. Use with care as\n"
	                      "it will allow world-wide administrative access\n"
	                      "to your Tvheadend installation until you edit\n"
	                      "the access-control from within the Tvheadend UI",
      OPT_BOOL, &opt_firstrun },
#if ENABLE_LINUXDVB
    { 'a', "adapters",  "Use only specified DVB adapters",
      OPT_STR, &opt_dvb_adapters },
#endif

    {   0, NULL,         "Server Connectivity",    OPT_BOOL, NULL         },
    {   0, "http_port",  "Specify alternative http port",
      OPT_INT, &tvheadend_webui_port },
    {   0, "http_root",  "Specify alternative http webroot",
      OPT_STR, &tvheadend_webroot },
    {   0, "htsp_port",  "Specify alternative htsp port",
      OPT_INT, &tvheadend_htsp_port },
    {   0, "htsp_port2", "Specify extra htsp port",
      OPT_INT, &tvheadend_htsp_port_extra },

    {   0, NULL,        "Debug Options",           OPT_BOOL, NULL         },
    { 'd', "debug",     "Enable all debug",        OPT_BOOL, &opt_debug   },
    { 's', "syslog",    "Enable debug to syslog",  OPT_BOOL, &opt_syslog  },
    {   0, "uidebug",   "Enable webUI debug",      OPT_BOOL, &opt_uidebug },
    { 'A', "abort",     "Immediately abort",       OPT_BOOL, &opt_abort   },
#if ENABLE_LINUXDVB
    { 'R', "dvbraw",    "Use rawts file to create virtual adapter",
      OPT_STR, &opt_dvb_raw },
#endif
    { 'r', "rawts",     "Use rawts file to generate virtual services",
      OPT_STR, &opt_rawts },
    { 'j', "join",      "Subscribe to a service permanently",
      OPT_STR, &opt_subscribe }
  };

  /* Get current directory */
  tvheadend_cwd = dirname(dirname(tvh_strdupa(argv[0])));

  /* Set locale */
  setlocale(LC_ALL, "");

  /* make sure the timezone is set */
  tzset();

  /* Process command line */
  for (i = 1; i < argc; i++) {

    /* Find option */
    cmdline_opt_t *opt
      = cmdline_opt_find(cmdline_opts, ARRAY_SIZE(cmdline_opts), argv[i]);
    if (!opt)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts),
                 "invalid option specified [%s]", argv[i]);

    /* Process */
    if (opt->type == OPT_BOOL)
      *((int*)opt->param) = 1;
    else if (++i == argc)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts),
                 "option %s requires a value", opt->lopt);
    else if (opt->type == OPT_INT)
      *((int*)opt->param) = atoi(argv[i]);
    else
      *((char**)opt->param) = argv[i];

    /* Stop processing */
    if (opt_help)
      show_usage(argv[0], cmdline_opts, ARRAY_SIZE(cmdline_opts), NULL);
    if (opt_version)
      show_version(argv[0]);
  }

  /* Additional cmdline processing */
  log_debug_to_console  = opt_debug;
  log_debug_to_syslog   = opt_syslog;
  tvheadend_webui_debug = opt_debug || opt_uidebug;
#if ENABLE_LINUXDVB
  if (!opt_dvb_adapters) {
    adapter_mask = ~0;
  } else {
    char *p, *r, *e;
    adapter_mask = 0x0;
    p = strtok_r((char*)opt_dvb_adapters, ",", &r);
    while (p) {
      int a = strtol(p, &e, 10);
      if (*e != 0 || a < 0 || a > 31) {
        tvhlog(LOG_ERR, "START", "Invalid adapter number '%s'", p);
        return 1;
      }
      adapter_mask |= (1 << a);
      p = strtok_r(NULL, ",", &r);
    }
    if (!adapter_mask) {
      tvhlog(LOG_ERR, "START", "No adapters specified!");
      return 1;
    }
  }
#endif
  if (tvheadend_webroot) {
    char *tmp;
    if (*tvheadend_webroot == '/')
      tmp = strdup(tvheadend_webroot);
    else {
      tmp = malloc(strlen(tvheadend_webroot)+1);
      *tmp = '/';
      strcpy(tmp+1, tvheadend_webroot);
    }
    if (tmp[strlen(tmp)-1] == '/')
      tmp[strlen(tmp)-1] = '\0';
    tvheadend_webroot = tmp;
  }

  signal(SIGPIPE, handle_sigpipe); // will be redundant later

  /* Daemonise */
  if(opt_fork) {
    const char *homedir;
    gid_t gid;
    uid_t uid;
    struct group  *grp = getgrnam(opt_group ?: "video");
    struct passwd *pw  = getpwnam(opt_user) ?: NULL;
    FILE   *pidfile    = fopen(opt_pidpath, "w+");

    if(grp != NULL) {
      gid = grp->gr_gid;
    } else {
      gid = 1;
    }

    if (pw != NULL) {
      if (getuid() != pw->pw_uid) {
        gid_t glist[10];
        int gnum;
        gnum = get_user_groups(pw, glist, 10);
        if (setgroups(gnum, glist)) {
          tvhlog(LOG_ALERT, "START",
                 "setgroups() failed, do you have permission?");
          return 1;
        }
      }
      uid     = pw->pw_uid;
      homedir = pw->pw_dir;
      setenv("HOME", homedir, 1);
    } else {
      uid = 1;
    }
    if ((getgid() != gid) && setgid(gid)) {
      tvhlog(LOG_ALERT, "START",
             "setgid() failed, do you have permission?");
      return 1;
    }
    if ((getuid() != uid) && setuid(uid)) {
      tvhlog(LOG_ALERT, "START",
             "setuid() failed, do you have permission?");
      return 1;
    }

    if(daemon(0, 0)) {
      exit(2);
    }
    if(pidfile != NULL) {
      fprintf(pidfile, "%d\n", getpid());
      fclose(pidfile);
    }

    umask(0);
  }

  /* Setup logging */
  log_stderr   = !opt_fork;
  log_decorate = isatty(2);
  openlog("tvheadend", LOG_PID, LOG_DAEMON);

  /* Initialise configuration */
  hts_settings_init(opt_config);

  /* Setup global mutexes */
  pthread_mutex_init(&ffmpeg_lock, NULL);
  pthread_mutex_init(&fork_lock, NULL);
  pthread_mutex_init(&global_lock, NULL);
  pthread_mutex_lock(&global_lock);

  time(&dispatch_clock);

  /* Signal handling */
  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);
  trap_init(argv[0]);
  
  /**
   * Initialize subsystems
   */

#if ENABLE_LIBAV
  libav_init();
#endif

  config_init();

  imagecache_init();

  service_init();

  channels_init();

  subscription_init();

  access_init(opt_firstrun);

#if ENABLE_LINUXDVB
  muxes_init();
  dvb_init(adapter_mask, opt_dvb_raw);
#endif

  iptv_input_init();

#if ENABLE_V4L
  v4l_init();
#endif

#if ENABLE_TIMESHIFT
  timeshift_init();
#endif

  tcp_server_init();
  http_server_init();
  webui_init();

  serviceprobe_init();

#if ENABLE_CWC
  cwc_init();
  capmt_init();
#if (!ENABLE_DVBCSA)
  ffdecsa_init();
#endif
#endif

  epggrab_init();
  epg_init();

  dvr_init();

  htsp_init();

  if(opt_rawts != NULL)
    rawts_init(opt_rawts);

  if(opt_subscribe != NULL)
    subscription_dummy_join(opt_subscribe, 1);

#ifdef CONFIG_AVAHI
  avahi_init();
#endif

  epg_updated(); // cleanup now all prev ref's should have been created

  pthread_mutex_unlock(&global_lock);

  /**
   * Wait for SIGTERM / SIGINT, but only in this thread
   */

  running = 1;
  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);

  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);

  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  tvhlog(LOG_NOTICE, "START", "HTS Tvheadend version %s started, "
	 "running as PID:%d UID:%d GID:%d, settings located in '%s'",
	 tvheadend_version,
	 getpid(), getuid(), getgid(), hts_settings_get_root());

  if(opt_abort)
    abort();

  mainloop();

  epg_save();

#if ENABLE_TIMESHIFT
  timeshift_term();
#endif

  tvhlog(LOG_NOTICE, "STOP", "Exiting HTS Tvheadend");

  if(opt_fork)
    unlink(opt_pidpath);

  return 0;

}


static const char *logtxtmeta[8][2] = {
  {"EMERGENCY", "\033[31m"},
  {"ALERT",     "\033[31m"},
  {"CRITICAL",  "\033[31m"},
  {"ERROR",     "\033[31m"},
  {"WARNING",   "\033[33m"},
  {"NOTICE",    "\033[36m"},
  {"INFO",      "\033[32m"},
  {"DEBUG",     "\033[32m"},
};

/**
 * Internal log function
 */
static void
tvhlogv(int notify, int severity, const char *subsys, const char *fmt, 
	va_list ap)
{
  char buf[2048];
  char buf2[2048];
  char t[50];
  int l;
  struct tm tm;
  time_t now;

  l = snprintf(buf, sizeof(buf), "%s: ", subsys);

  vsnprintf(buf + l, sizeof(buf) - l, fmt, ap);

  if(log_debug_to_syslog || severity < LOG_DEBUG)
    syslog(severity, "%s", buf);

  /**
   * Get time (string)
   */
  time(&now);
  localtime_r(&now, &tm);
  strftime(t, sizeof(t), "%b %d %H:%M:%S", &tm);

  /**
   * Send notification to Comet (Push interface to web-clients)
   */
  if(notify) {
    htsmsg_t *m;

    snprintf(buf2, sizeof(buf2), "%s %s", t, buf);
    m = htsmsg_create_map();
    htsmsg_add_str(m, "notificationClass", "logmessage");
    htsmsg_add_str(m, "logtxt", buf2);
    comet_mailbox_add_message(m, severity == LOG_DEBUG);
    htsmsg_destroy(m);
  }

  /**
   * Write to stderr
   */
  
  if(log_stderr && (log_debug_to_console || severity < LOG_DEBUG)) {
    const char *leveltxt = logtxtmeta[severity][0];
    const char *sgr      = logtxtmeta[severity][1];
    const char *sgroff;

    if(!log_decorate) {
      sgr = "";
      sgroff = "";
    } else {
      sgroff = "\033[0m";
    }
    fprintf(stderr, "%s%s [%s]:%s%s\n", sgr, t, leveltxt, buf, sgroff);
  }
}


/**
 *
 */
void
tvhlog(int severity, const char *subsys, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  tvhlogv(1, severity, subsys, fmt, ap);
  va_end(ap);
}


/**
 * May be invoked from a forked process so we can't do any notification
 * to comet directly.
 *
 * @todo Perhaps do it via a pipe?
 */
void
tvhlog_spawn(int severity, const char *subsys, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  tvhlogv(0, severity, subsys, fmt, ap);
  va_end(ap);
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
void
scopedunlock(pthread_mutex_t **mtxp)
{
  pthread_mutex_unlock(*mtxp);
}


void
limitedlog(loglimiter_t *ll, const char *sys, const char *o, const char *event)
{
  time_t now;
  char buf[64];
  time(&now);

  ll->events++;
  if(ll->last == now)
    return; // Duplicate event

  if(ll->last <= now - 10) {
    // Too old, reset duplicate counter
    ll->events = 0;
    buf[0] = 0;
  } else {
    snprintf(buf, sizeof(buf), ", %d duplicate log lines suppressed", 
	     ll->events);
  }

  tvhlog(LOG_WARNING, sys, "%s: %s%s", o, event, buf);
  ll->last = now;
}


/**
 *
 */  
const char *
hostconnection2str(int type)
{
  switch(type) {
  case HOSTCONNECTION_USB12:
    return "USB (12 Mbit/s)";
    
  case HOSTCONNECTION_USB480:
    return "USB (480 Mbit/s)";

  case HOSTCONNECTION_PCI:
    return "PCI";
  }
  return "Unknown";

}


/**
 *
 */
static int
readlinefromfile(const char *path, char *buf, size_t buflen)
{
  int fd = open(path, O_RDONLY);
  ssize_t r;

  if(fd == -1)
    return -1;

  r = read(fd, buf, buflen - 1);
  close(fd);
  if(r < 0)
    return -1;

  buf[buflen - 1] = 0;
  return 0;
}


/**
 *
 */  
int
get_device_connection(const char *dev)
{
  char path[200];
  char l[64];
  int speed;

  snprintf(path, sizeof(path),  "/sys/class/%s/device/speed", dev);

  if(readlinefromfile(path, l, sizeof(l))) {
    // Unable to read speed, assume it's PCI
    return HOSTCONNECTION_PCI;
  } else {
    speed = atoi(l);
   
    return speed >= 480 ? HOSTCONNECTION_USB480 : HOSTCONNECTION_USB12;
  }
}


