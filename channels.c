/*
 *  tvheadend, channel functions
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "input_dvb.h"
#include "input_v4l.h"
#include "input_iptv.h"

#include "channels.h"
#include "transports.h"

struct th_channel_queue channels;
static int channel_tally;

#define MAXCHANNELS 256

static th_channel_t *charray[MAXCHANNELS];
static int numchannels;

th_channel_t *
channel_find(const char *name, int create)
{
  th_channel_t *ch;

  TAILQ_FOREACH(ch, &channels, ch_global_link) {
    if(!strcmp(name, ch->ch_name))
      return ch;
  }
  if(create == 0)
    return NULL;

  ch = calloc(1, sizeof(th_channel_t));
  ch->ch_name = strdup(name);
  TAILQ_INSERT_TAIL(&channels, ch, ch_global_link);
  ch->ch_index = channel_tally++;
  ch->ch_ref = ++reftally;

  charray[ch->ch_index] = ch;
  numchannels++;
  return ch;
}


static int
transportcmp(th_transport_t *a, th_transport_t *b)
{
  return a->tht_prio - b->tht_prio;
}



static void
service_load(struct config_head *head)
{
  const char *name,  *v;
  pidinfo_t pids[10];
  int i, npids = 0;
  th_transport_t *t;
  pidinfo_t *pi;
  th_channel_t *ch;

  if((name = config_get_str_sub(head, "channel", NULL)) == NULL)
    return;

  ch = channel_find(name, 1);

  t = calloc(1, sizeof(th_transport_t));

  if(0) {
#ifdef ENABLE_INPUT_DVB
  } else if((v = config_get_str_sub(head, "dvbmux", NULL)) != NULL) {
    if(dvb_configure_transport(t, v)) {
      free(t);
      return;
    }
#endif
#ifdef ENABLE_INPUT_IPTV
  } else if((v = config_get_str_sub(head, "iptvmux", NULL)) != NULL) {
    if(iptv_configure_transport(t, v)) {
      free(t);
      return;
    }
#endif
#ifdef ENABLE_INPUT_V4L
  } else if((v = config_get_str_sub(head, "v4lmux", NULL)) != NULL) {
    if(v4l_configure_transport(t, v)) {
      free(t);
      return;
    }
#endif
  } else {
    free(t);
    return;
  }

  transport_monitor_init(t);

  if(t->tht_npids == 0) {

    if((v = config_get_str_sub(head, "video", NULL)) != NULL) {
      pids[npids].pid = strtol(v, NULL, 0);
      pids[npids++].type = HTSTV_MPEG2VIDEO;
    }

    if((v = config_get_str_sub(head, "h264", NULL)) != NULL) {
      pids[npids].pid = strtol(v, NULL, 0);
      pids[npids++].type = HTSTV_H264;
    }

    if((v = config_get_str_sub(head, "audio", NULL)) != NULL) {
      pids[npids].pid = strtol(v, NULL, 0);
      pids[npids++].type = HTSTV_MPEG2AUDIO;
    }
  
    if((v = config_get_str_sub(head, "ac3", NULL)) != NULL) {
      pids[npids].pid = strtol(v, NULL, 0);
      pids[npids++].type = HTSTV_AC3;
    }

    if((v = config_get_str_sub(head, "teletext", NULL)) != NULL) {
      pids[npids].pid = strtol(v, NULL, 0);
      pids[npids++].type = HTSTV_TELETEXT;
    }

    t->tht_pids = calloc(1, npids * sizeof(pidinfo_t));

    for(i = 0; i < npids; i++) {
      pi = t->tht_pids + i;

      pi->pid = pids[i].pid;
      pi->type = pids[i].type;
      pi->demuxer_fd = -1;
      pi->cc_valid = 0;
    }

    t->tht_npids = npids;
  }
  t->tht_prio = atoi(config_get_str_sub(head, "prio", ""));

  syslog(LOG_DEBUG, "Added service \"%s\" for channel \"%s\"",
	 t->tht_name, ch->ch_name);

  t->tht_channel = ch;
  LIST_INSERT_SORTED(&ch->ch_transports, t, tht_channel_link, transportcmp);
}



static void
channel_load(struct config_head *head)
{
  const char *name, *v;
  th_channel_t *ch;

  if((name = config_get_str_sub(head, "name", NULL)) == NULL)
    return;

  ch = channel_find(name, 1);

  syslog(LOG_DEBUG, "Added channel \"%s\"", name);

  if((v = config_get_str_sub(head, "teletext-rundown", NULL)) != NULL) {
    ch->ch_teletext_rundown = atoi(v);
  }
}


void
channels_load(void)
{
  config_entry_t *ce;
  TAILQ_INIT(&channels);

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp("channel", ce->ce_key)) {
      channel_load(&ce->ce_sub);
    }
  }

  TAILQ_FOREACH(ce, &config_list, ce_link) {
    if(ce->ce_type == CFG_SUB && !strcasecmp("service", ce->ce_key)) {
      service_load(&ce->ce_sub);
    }
  }
}


th_channel_t *
channel_by_id(unsigned int id)
{
  return id < MAXCHANNELS ? charray[id] : NULL;
}


int
id_by_channel(th_channel_t *ch)
{
  return ch->ch_index;
}


int
channel_get_channels(void)
{
  return numchannels;
}
