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

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "dvb.h"
#include "v4l.h"
#include "iptv_input.h"

#include "channels.h"
#include "transports.h"

struct th_channel_queue channels;
struct th_transport_list all_transports;
int nchannels;

void scanner_init(void);

th_channel_t *
channel_find(const char *name, int create)
{
  th_channel_t *ch;

  TAILQ_FOREACH(ch, &channels, ch_global_link)
    if(!strcasecmp(name, ch->ch_name))
      return ch;

  if(create == 0)
    return NULL;

  ch = calloc(1, sizeof(th_channel_t));
  ch->ch_name = strdup(name);
  ch->ch_index = nchannels;
  TAILQ_INIT(&ch->ch_epg_events);

  TAILQ_INSERT_TAIL(&channels, ch, ch_global_link);
  ch->ch_tag = tag_get();
  nchannels++;
  return ch;
}


static int
transportcmp(th_transport_t *a, th_transport_t *b)
{
  return a->tht_prio - b->tht_prio;
}


int 
transport_set_channel(th_transport_t *t, const char *name)
{
  th_channel_t *ch;
  th_pid_t *tp;
  char *chname;

  if(LIST_FIRST(&t->tht_pids) == NULL)
    return -1;

  if(t->tht_channel != NULL)
    return 0;

  ch = channel_find(name, 1);
  t->tht_channel = ch;
  LIST_INSERT_SORTED(&ch->ch_transports, t, tht_channel_link, transportcmp);

  chname = utf8toprintable(ch->ch_name);

  syslog(LOG_DEBUG, "Added service \"%s\" for channel \"%s\"",
	 t->tht_name, chname);
  free(chname);

  LIST_FOREACH(tp, &t->tht_pids, tp_link)
    syslog(LOG_DEBUG, "   Pid %5d [%s]",
	   tp->tp_pid, htstvstreamtype2txt(tp->tp_type));

  return 0;
}




static void
service_load(struct config_head *head)
{
  const char *name,  *v;
  th_transport_t *t;
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

  if((v = config_get_str_sub(head, "service_id", NULL)) != NULL)
    t->tht_dvb_service_id = strtol(v, NULL, 0);

  if((v = config_get_str_sub(head, "network_id", NULL)) != NULL)
    t->tht_dvb_network_id = strtol(v, NULL, 0);

  if((v = config_get_str_sub(head, "transport_id", NULL)) != NULL)
    t->tht_dvb_transport_id = strtol(v, NULL, 0);

  if((v = config_get_str_sub(head, "video", NULL)) != NULL)
    transport_add_pid(t, strtol(v, NULL, 0), HTSTV_MPEG2VIDEO);

  if((v = config_get_str_sub(head, "h264", NULL)) != NULL)
    transport_add_pid(t, strtol(v, NULL, 0), HTSTV_H264);
  
  if((v = config_get_str_sub(head, "audio", NULL)) != NULL) 
    transport_add_pid(t, strtol(v, NULL, 0), HTSTV_MPEG2AUDIO);
  
  if((v = config_get_str_sub(head, "ac3", NULL)) != NULL)
    transport_add_pid(t, strtol(v, NULL, 0), HTSTV_AC3);

  if((v = config_get_str_sub(head, "teletext", NULL)) != NULL)
    transport_add_pid(t, strtol(v, NULL, 0), HTSTV_TELETEXT);

  t->tht_prio = atoi(config_get_str_sub(head, "prio", ""));

  transport_set_channel(t, name);

  transport_monitor_init(t);
  LIST_INSERT_HEAD(&all_transports, t, tht_global_link);
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
channel_by_index(uint32_t index)
{
  th_channel_t *ch;

  TAILQ_FOREACH(ch, &channels, ch_global_link)
    if(ch->ch_index == index)
      return ch;

  return NULL;
}
