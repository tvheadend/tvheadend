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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include "tvhead.h"
#include "input_dvb.h"
#include "input_v4l.h"
#include "channels.h"
#include "output_client.h"
#include "epg_xmltv.h"
#include "pvr.h"
#include "dispatch.h"
#include "output_multicast.h"

int reftally;
int running;

int xmltvreload;

static void
xmltvdoreload(int x)
{
  xmltvreload = 1;
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
  char *cfgfile;
  int logfacility = LOG_DAEMON;

  

  signal(SIGPIPE, handle_sigpipe);

  cfgfile = "/etc/tvhead.cfg";

  while((c = getopt(argc, argv, "c:fu:g:")) != -1) {
    switch(c) {
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

  config_read_file(cfgfile);

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
  
  syslog(LOG_NOTICE, "Started HTS TV Headend");

  dispatch_init();

  signal(SIGUSR1, xmltvdoreload);
  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);

  av_register_all();
  av_log_set_level(AV_LOG_QUIET);

  client_start();
  dvb_add_adapters();

  //  v4l_add_adapters();

  channels_load();

  xmltv_init();

  pvr_init();
  output_multicast_setup();

  
  running = 1;


  while(running)
    dispatcher();

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


