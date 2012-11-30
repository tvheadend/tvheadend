/*
 *  TVheadend
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
#include "ccw.h"
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

int running;
time_t dispatch_clock;
static LIST_HEAD(, gtimer) gtimers;
pthread_mutex_t global_lock;
pthread_mutex_t ffmpeg_lock;
pthread_mutex_t fork_lock;
static int log_stderr;
static int log_decorate;

int log_debug_to_syslog;
int log_debug_to_console;

int webui_port;
int htsp_port;
int htsp_port_extra;
char *tvheadend_cwd;

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
 *
 */
static void
usage(const char *argv0)
{
  printf("HTS Tvheadend %s\n", tvheadend_version);
  printf("usage: %s [options]\n", argv0);
  printf("\n");
  printf(" -a <adapters>   Use only DVB adapters specified (csv)\n");
  printf(" -c <directory>  Alternate configuration path.\n"
	 "                 Defaults to [$HOME/.hts/tvheadend]\n");
  printf(" -m <directory>  Alternate mux configuration directory\n");
  printf(" -f              Fork and daemonize\n");
  printf(" -p <pidfile>    Write pid to <pidfile> instead of /var/run/tvheadend.pid,\n"
        "                 only works with -f\n");
  printf(" -u <username>   Run as user <username>, only works with -f\n");
  printf(" -g <groupname>  Run as group <groupname>, only works with -f\n");
  printf(" -C              If no useraccount exist then create one with\n"
	 "                 no username and no password. Use with care as\n"
	 "                 it will allow world-wide administrative access\n"
	 "                 to your Tvheadend installation until you edit\n"
	 "                 the access-control from within the Tvheadend UI\n");
  printf(" -s              Log debug to syslog\n");
  printf(" -w <portnumber> WebUI access port [default 9981]\n");
  printf(" -e <portnumber> HTSP access port [default 9982]\n");
  printf("\n");
  printf("Development options\n");
  printf("\n");
  printf(" -d              Log debug to console\n");
  printf(" -j <id>         Statically join the given transport id\n");
  printf(" -r <tsfile>     Read the given transport stream file and present\n"
	 "                 found services as channels\n");
  printf(" -A              Immediately call abort()\n");
	 
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
  int c;
  int forkaway = 0;
  FILE *pidfile;
  const char *pidpath = "/var/run/tvheadend.pid";
  struct group *grp;
  struct passwd *pw;
  const char *usernam = NULL;
  const char *groupnam = NULL;
  int logfacility = LOG_DAEMON;
  int createdefault = 0;
  sigset_t set;
  const char *homedir;
  const char *rawts_input = NULL;
  const char *dvb_rawts_input = NULL;
  const char *join_transport = NULL;
  const char *confpath = NULL;
  char *p, *endp;
  uint32_t adapter_mask = 0xffffffff;
  int crash = 0;
  webui_port = 9981;
  htsp_port = 9982;
  gid_t gid;
  uid_t uid;

  /* Get current directory */
  tvheadend_cwd = dirname(dirname(tvh_strdupa(argv[0])));

  /* Set locale */
  setlocale(LC_ALL, "");

  // make sure the timezone is set
  tzset();

  while((c = getopt(argc, argv, "Aa:fp:u:g:c:Chdr:j:sw:e:E:R:")) != -1) {
    switch(c) {
    case 'a':
      adapter_mask = 0x0;
      p = strtok(optarg, ",");
      if (p != NULL) {
        do {
          int adapter = strtol(p, &endp, 10);
          if (*endp != 0 || adapter < 0 || adapter > 31) {
              fprintf(stderr, "Invalid adapter number '%s'\n", p);
              return 1;
          }
          adapter_mask |= (1 << adapter);
        } while ((p = strtok(NULL, ",")) != NULL);
        if (adapter_mask == 0x0) {
          fprintf(stderr, "No adapters specified!\n");
          return 1;
        }
      } else {
        usage(argv[0]);
      }
      break;
    case 'A':
      crash = 1;
      break;
    case 'f':
      forkaway = 1;
      break;
    case 'p':
      pidpath = optarg;
      break;
    case 'w':
      webui_port = atoi(optarg);
      break;
    case 'e':
      htsp_port = atoi(optarg);
      break;
    case 'E':
      htsp_port_extra = atoi(optarg);
      break;
    case 'u':
      usernam = optarg;
      break;
    case 'g':
      groupnam = optarg;
      break;
    case 'c':
      confpath = optarg;
      break;
    case 'd':
      log_debug_to_console = 1;
      break;
    case 's':
      log_debug_to_syslog = 1;
      break;
    case 'C':
      createdefault = 1;
      break;
    case 'r':
      rawts_input = optarg;
      break;
    case 'R':
      dvb_rawts_input = optarg;
      break;
    case 'j':
      join_transport = optarg;
      break;
    default:
      usage(argv[0]);
    }
  }

  signal(SIGPIPE, handle_sigpipe);

  log_stderr   = 1;
  log_decorate = isatty(2);

  if(forkaway) {
    grp  = getgrnam(groupnam ?: "video");
    pw   = usernam ? getpwnam(usernam) : NULL;

    pidfile = fopen(pidpath, "w+");

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
          tvhlog(LOG_ALERT, "START", "setgroups() failed, do you have permission?");
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
      tvhlog(LOG_ALERT, "START", "setgid() failed, do you have permission?");
      return 1;
    }
    if ((getuid() != uid) && setuid(uid)) {
      tvhlog(LOG_ALERT, "START", "setuid() failed, do you have permission?");
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

  log_stderr = !forkaway;
  log_decorate = isatty(2);

  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);

  openlog("tvheadend", LOG_PID, logfacility);

  hts_settings_init(confpath);

  pthread_mutex_init(&ffmpeg_lock, NULL);
  pthread_mutex_init(&fork_lock, NULL);
  pthread_mutex_init(&global_lock, NULL);

  pthread_mutex_lock(&global_lock);

  time(&dispatch_clock);

  trap_init(argv[0]);
  
  /**
   * Initialize subsystems
   */

  config_init();

  muxes_init();

  service_init();

  channels_init();

  subscription_init();

  access_init(createdefault);

  tcp_server_init();
#if ENABLE_LINUXDVB
  dvb_init(adapter_mask, dvb_rawts_input);
#endif
  iptv_input_init();
#if ENABLE_V4L
  v4l_init();
#endif
  http_server_init();

  webui_init();

  serviceprobe_init();

  cwc_init();

  capmt_init();

  ccw_init();

  epggrab_init();
  epg_init();

  dvr_init();

  htsp_init();

#if (!ENABLE_DVBCSA)
  ffdecsa_init();
#endif

  if(rawts_input != NULL)
    rawts_init(rawts_input);

  if(join_transport != NULL)
    subscription_dummy_join(join_transport, 1);

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

  if(crash)
    abort();

  mainloop();

  epg_save();

  tvhlog(LOG_NOTICE, "STOP", "Exiting HTS Tvheadend");

  if(forkaway)
    unlink("/var/run/tvheadend.pid");

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


