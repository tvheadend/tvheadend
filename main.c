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

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <libavformat/avformat.h>

#include "tvhead.h"
#include "dvb.h"
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

#include <libhts/htsparachute.h>

int running;
int startupcounter;
const char *settings_dir;
const char *sys_warning;

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
  syslog(LOG_ERR, "HTS TV Headend crashed on signal %i (pwd \"%s\")",
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
  char buf[400];

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

  config_open_by_prgname("tvheadend", cfgfile);

  if(forkaway) {
    if(daemon(0, 0)) {
      exit(2);
    }

    pidfile = fopen("/var/run/tvhead.pid", "w+");
    fprintf(pidfile, "%d\n", getpid());
    fclose(pidfile);

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
  
  settings_dir = config_get_str("settings-dir", NULL);

  if(settings_dir == NULL) {
    settings_dir = "/tmp/tvheadend";
    sys_warning = 
      "All channelsetup/recording info is stored in "
      "'/tmp/tvheadend' and may not survive a system restart. "
      "Please see the configuration manual for how to setup this correctly.";
  }
  mkdir(settings_dir, 0777);

  snprintf(buf, sizeof(buf), "%s/channels", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/transports", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/recordings", settings_dir);
  mkdir(buf, 0777);

  snprintf(buf, sizeof(buf), "%s/autorec", settings_dir);
  mkdir(buf, 0777);

  syslog(LOG_NOTICE, "Started HTS TV Headend, settings located in \"%s\"",
	 settings_dir);

  dispatch_init();

  htsparachute_init(pull_chute);

  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);

  spawn_init();

  av_register_all();
  av_log_set_level(AV_LOG_INFO);
  tffm_init();
  pkt_init();

  if(!disable_dvb)
    dvb_init();
  v4l_init();

  channels_load();

  autorec_init();
  epg_init();
  xmltv_init();

  subscriptions_init();

  htmlui_start();

  avgen_init();

  file_input_init();

  cwc_init();

  running = 1;
  while(running) {

    if(startupcounter == 0) {
      syslog(LOG_NOTICE, 
	     "Initial input setup completed, starting output modules");

      fprintf(stderr,
	      "Initial input setup completed, starting output modules\n");

      startupcounter = -1;

      pvr_init();
      output_multicast_setup();
      client_start();

      p = atoi(config_get_str("http-server-port", "9980"));
      if(p)
	http_start(p);

      p = atoi(config_get_str("htsp-server-port", "0"));
      if(p)
	htsp_start(p);

      p = atoi(config_get_str("xbmsp-server-port", "0"));
      if(p)
	xbmsp_start(p);

    }
    dispatcher();
  }

  syslog(LOG_NOTICE, "Exiting HTS TV Headend");

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






const char *
htstvstreamtype2txt(tv_streamtype_t s)
{
  switch(s) {
  case HTSTV_MPEG2VIDEO: return "mpeg2video";
  case HTSTV_MPEG2AUDIO: return "mpeg2audio";
  case HTSTV_H264:       return "h264";
  case HTSTV_AC3:        return "AC-3";
  case HTSTV_TELETEXT:   return "teletext";
  case HTSTV_SUBTITLES:  return "subtitles";
  case HTSTV_TABLE:      return "PSI table";
  default:               return "<unknown>";
  }
}


FILE *
settings_open_for_write(const char *name)
{
  FILE *fp;

  fp = fopen(name, "w+");
  if(fp == NULL)
    syslog(LOG_ALERT, "Unable to open settings file \"%s\" -- %s",
	   name, strerror(errno));
  return fp;
}

struct config_head *
user_resolve_to_config(const char *username, const char *password)
{
  config_entry_t *ce;
  const char *name, *pass;

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp("user", ce->ce_key)) {
      if((name = config_get_str_sub(&ce->ce_sub, "name", NULL)) == NULL)
	continue;
      if(!strcmp(name, username))
	break;
    }
  }

  if(ce == NULL)
    return NULL;

  if((pass = config_get_str_sub(&ce->ce_sub, "password", NULL)) == NULL)
    return NULL;

  if(strcmp(pass, password))
    return NULL;

  return &ce->ce_sub;
}
