/*
 *  TV headend
 *  Copyright (C) 2007 Andreas Öman
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
#include <iconv.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <libavformat/avformat.h>

#include "tvhead.h"
#include "tcp.h"
#include "access.h"
#include "http.h"
#include "webui/webui.h"
#include "dvb/dvb.h"
#include "xmltv.h"
#include "spawn.h"
#include "subscriptions.h"
#include "serviceprobe.h"
#include "cwc.h"

#include <libhts/htsparachute.h>
#include <libhts/htssettings.h>


int running;
extern const char *htsversion;
time_t dispatch_clock;
static LIST_HEAD(, gtimer) gtimers;
pthread_mutex_t global_lock;



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

static void
pull_chute (int sig)
{
  char pwd[PATH_MAX];

  getcwd(pwd, sizeof(pwd));
  syslog(LOG_ERR, "HTS Tvheadend crashed on signal %i (pwd \"%s\")",
	 sig, pwd);
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
gtimer_arm(gtimer_t *gti, gti_callback_t *callback, void *opaque, int delta)
{
  time_t now;
  time(&now);
  
  lock_assert(&global_lock);

  if(gti->gti_callback != NULL)
    LIST_REMOVE(gti, gti_link);
    
  gti->gti_callback = callback;
  gti->gti_opaque = opaque;
  gti->gti_expire = now + delta;

  LIST_INSERT_SORTED(&gtimers, gti, gti_link, gtimercmp);
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
  struct group *grp;
  struct passwd *pw;
  const char *usernam = NULL;
  const char *groupnam = NULL;
  char *cfgfile = NULL;
  int logfacility = LOG_DAEMON;
  int disable_dvb = 0;
  sigset_t set;

  signal(SIGPIPE, handle_sigpipe);

  while((c = getopt(argc, argv, "c:fu:g:d")) != -1) {
    switch(c) {
    case 'd':
      disable_dvb = 1;
      break;
    case 'c':
      cfgfile = optarg;
      break;
    case 'f':
      forkaway = 1;
      break;
    case 'u':
      usernam = optarg;
      break;
    case 'g':
      groupnam = optarg;
      break;
    }
  }

  if(forkaway) {
    if(daemon(0, 0)) {
      exit(2);
    }

    pidfile = fopen("/var/run/tvhead.pid", "w+");
    if(pidfile != NULL) {
      fprintf(pidfile, "%d\n", getpid());
      fclose(pidfile);
    }

    grp = getgrnam(groupnam ?: "video");
    if(grp != NULL) {
      setgid(grp->gr_gid);
    } else {
      setgid(1);
    }

    pw = usernam ? getpwnam(usernam) : NULL;
    
    if(pw != NULL) {
      setuid(pw->pw_uid);
    } else {
      setuid(1);
    }

    umask(0);
  }

  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);

  openlog("tvheadend", LOG_PID, logfacility);

  hts_settings_init("tvheadend2");

  pthread_mutex_init(&global_lock, NULL);

  pthread_mutex_lock(&global_lock);

  htsparachute_init(pull_chute);
  
  /**
   * Initialize subsystems
   */
  av_register_all();

  xmltv_init();   /* Must be initialized before channels */

  channels_init();

  access_init();

  tcp_server_init();

  dvb_init();

  http_server_init();

  webui_init();

  subscriptions_init();

  serviceprobe_init();

  cwc_init();

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


  mainloop();

  syslog(LOG_NOTICE, "Exiting HTS Tvheadend");

  if(forkaway)
    unlink("/var/run/tvhead.pid");

  return 0;

}



static char *
convert_to(const char *in, const char *target_encoding)
{
  iconv_t ic;
  size_t inlen, outlen, alloced, pos;
  char *out, *start;
  int r;

  inlen = strlen(in);
  alloced = 100;
  outlen = alloced;

  ic = iconv_open(target_encoding, "UTF8");
  if(ic == NULL)
    return NULL;

  out = start = malloc(alloced + 1);

  while(inlen > 0) {
    r = iconv(ic, (char **)&in, &inlen, &out, &outlen);

    if(r == (size_t) -1) {
      if(errno == EILSEQ) {
	in++;
	inlen--;
	continue;
      }

      if(errno == E2BIG) {
	pos = alloced - outlen;
	alloced *= 2;
	start = realloc(start, alloced + 1);
	out = start + pos;
	outlen = alloced - pos;
	continue;
      }
      break;
    }
  }

  iconv_close(ic);
  pos = alloced - outlen;
  start[pos] = 0;
  return start;
}





char *
utf8toprintable(const char *in) 
{
  return convert_to(in, "ISO_8859-1");
}

char *
utf8tofilename(const char *in) 
{
  return convert_to(in, "LATIN1");
}




/**
 *
 */
void
tvhlog(int severity, const char *subsys, const char *fmt, ...)
{
  va_list ap;
  htsmsg_t *m;
  char buf[512];
  char buf2[512];
  char t[50];
  int l;
  struct tm tm;
  time_t now;

  l = snprintf(buf, sizeof(buf), "%s: ", subsys);

  va_start(ap, fmt);
  vsnprintf(buf + l, sizeof(buf) - l, fmt, ap);
  va_end(ap);

  syslog(severity, "%s", buf);

  /**
   *
   */

  time(&now);
  localtime_r(&now, &tm);
  strftime(t, sizeof(t), "%b %d %H:%M:%S", &tm);

  snprintf(buf2, sizeof(buf2), "%s %s", t, buf);

  m = htsmsg_create();
  htsmsg_add_str(m, "notificationClass", "logmessage");
  htsmsg_add_str(m, "logtxt", buf2);
  comet_mailbox_add_message(m);
  htsmsg_destroy(m);
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

