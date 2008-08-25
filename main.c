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
#include "dvb/dvb.h"
#include "v4l.h"
#include "channels.h"
#include "htsclient.h"
#include "epg.h"
#include "epg_xmltv.h"
#include "pvr.h"
#include "dispatch.h"
#include "transports.h"
#include "subscriptions.h"
#include "iptv_output.h"
#include "rtsp.h"
#include "http.h"
#include "htsp.h"
#include "buffer.h"
#include "htmlui.h"
#include "avgen.h"
#include "file_input.h"
#include "cwc.h"
#include "autorec.h"
#include "spawn.h"
#include "ffmuxer.h"
#include "xbmsp.h"
//#include "ajaxui/ajaxui.h"
#include "webui/webui.h"
#include "access.h"
#include "serviceprobe.h"

#include <libhts/htsparachute.h>
#include <libhts/htssettings.h>


int running;
int startupcounter;
const char *settings_dir;
const char *sys_warning;
extern const char *htsversion;
static pthread_mutex_t tag_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t tag_tally;

uint32_t
tag_get(void)
{
  uint32_t r;

  pthread_mutex_lock(&tag_mutex);
  r = ++tag_tally;
  if(r == 0)
    r = ++tag_tally;

  pthread_mutex_unlock(&tag_mutex);
  return r;
}


static void
doexit(int x)
{
  running = 0;
}

static void
handle_sigpipe(int x)
{
  return;
}

static void
pull_chute (int sig)
{
  char pwd[PATH_MAX];

  getcwd(pwd, sizeof(pwd));
  syslog(LOG_ERR, "HTS Tvheadend crashed on signal %i (pwd \"%s\")",
	 sig, pwd);
}

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
  int p;
  char buf[128];
  char buf2[128];
  char buf3[128];
  char *settingspath = NULL;
  const char *homedir;

  signal(SIGPIPE, handle_sigpipe);

  while((c = getopt(argc, argv, "c:fu:g:ds:")) != -1) {
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
    case 's':
      settingspath = optarg;
      break;
    }
  }

  config_open_by_prgname("tvheadend", cfgfile);

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

  openlog("tvheadend", LOG_PID, logfacility);

  hts_settings_init("tvheadend2");


  if(settingspath == NULL && (homedir = getenv("HOME")) != NULL) {
    snprintf(buf2, sizeof(buf2), "%s/.hts", homedir);
      
    if(mkdir(buf2, 0777) == 0 || errno == EEXIST)
      settingspath = buf2;
    else
      syslog(LOG_ERR, 
	     "Unable to create directory for storing settings \"%s\" -- %s",
	     buf, strerror(errno));
  }

  if(settingspath == NULL) {
    settingspath = "/tmp";
    sys_warning = 
      "All settings are stored in "
      "'/tmp/' and may not survive a system restart. "
      "Please see the configuration manual for how to setup this correctly.";
    syslog(LOG_ERR, "%s", sys_warning);
  }

  snprintf(buf3, sizeof(buf3), "%s/tvheadend", settingspath);
  settings_dir = buf3;

  if(!(mkdir(settings_dir, 0777) == 0 || errno == EEXIST)) {
    syslog(LOG_ERR, 
	   "Unable to create directory for storing settings \"%s\" -- %s",
	   settings_dir, strerror(errno));
  }

  snprintf(buf, sizeof(buf), "%s/channels", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/transports", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/recordings", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/autorec", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/dvbadapters", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/dvbadapters", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/dvbmuxes", settings_dir);
  mkdir(buf, 0777);

  syslog(LOG_NOTICE, 
	 "Starting HTS Tvheadend (%s), settings stored in \"%s\"",
	 htsversion, settings_dir);

  if(!forkaway)
    fprintf(stderr, 
	    "\nStarting HTS Tvheadend (%s)\nSettings stored in \"%s\"\n",
	    htsversion, settings_dir);

  dispatch_init();

  access_init();

  htsparachute_init(pull_chute);

  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);

  channels_load();

  spawn_init();

  av_register_all();
  av_log_set_level(AV_LOG_INFO);
  tffm_init();
  pkt_init();
  serviceprobe_setup();

  if(!disable_dvb)
    dvb_init();
  v4l_init();

  autorec_init();
  epg_init();
  //  xmltv_init();

  subscriptions_init();

  //  htmlui_start();

  webui_start();
  //  ajaxui_start();

  avgen_init();

  file_input_init();

  cwc_init();

  running = 1;
  while(running) {

    if(startupcounter == 0) {
      syslog(LOG_NOTICE, 
	     "Initial input setup completed, starting output modules");

      if(!forkaway)
	fprintf(stderr,
		"\nInitial input setup completed, starting output modules\n");
      
      startupcounter = -1;

      pvr_init();
      output_multicast_setup();
      client_start();

      p = atoi(config_get_str("http-server-port", "9981"));
      if(p)
	http_start(p);

      p = atoi(config_get_str("htsp-server-port", "9910"));
      if(p)
	htsp_start(p);

      p = atoi(config_get_str("xbmsp-server-port", "0"));
      if(p)
	xbmsp_start(p);

    }
    dispatcher();
  }

  syslog(LOG_NOTICE, "Exiting HTS Tvheadend");

  if(forkaway)
    unlink("/var/run/tvhead.pid");

  return 0;

}


config_entry_t *
find_mux_config(const char *muxtype, const char *muxname)
{
  config_entry_t *ce;
  const char *s;

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp(muxtype, ce->ce_key)) {

      if((s = config_get_str_sub(&ce->ce_sub, "name", NULL)) == NULL)
	continue;
      
      if(!strcmp(s, muxname))
	break;
    }
  }
  return ce;
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


FILE *
settings_open_for_write(const char *name)
{
  FILE *fp;
  int fd;

  if((fd = open(name, O_CREAT | O_TRUNC | O_RDWR, 0600)) < 0) {
    syslog(LOG_ALERT, "Unable to open settings file \"%s\" -- %s",
	   name, strerror(errno));
    return NULL;
  }

  if((fp = fdopen(fd, "w+")) == NULL)
    syslog(LOG_ALERT, "Unable to open settings file \"%s\" -- %s",
	   name, strerror(errno));

  return fp;
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
  int l;
  
  l = snprintf(buf, sizeof(buf), "%s: ", subsys);

  va_start(ap, fmt);
  vsnprintf(buf + l, sizeof(buf) - l, fmt, ap);
  va_end(ap);

  syslog(severity, "%s", buf);

  /**
   *
   */

  m = htsmsg_create();
  htsmsg_add_str(m, "notificationClass", "logmessage");
  htsmsg_add_str(m, "logtxt", buf);
  comet_mailbox_add_message(m);
  htsmsg_destroy(m);
}
