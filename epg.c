/*
 *  Electronic Program Guide - Common functions
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"
#include "dispatch.h"
#include "htsclient.h"
#include "htsp.h"

#define EPG_MAX_AGE 86400

#define EPG_HASH_ID_WIDTH 256

static pthread_mutex_t epg_mutex = PTHREAD_MUTEX_INITIALIZER;
struct event_list epg_hash[EPG_HASH_ID_WIDTH];
static dtimer_t epg_channel_maintain_timer;

void
epg_lock(void)
{
  pthread_mutex_lock(&epg_mutex);
}

void
epg_unlock(void)
{
  pthread_mutex_unlock(&epg_mutex);
}


void
epg_event_set_title(event_t *e, const char *title)
{
  free((void *)e->e_title);
  e->e_title = strdup(title);
}

void
epg_event_set_desc(event_t *e, const char *desc)
{
  free((void *)e->e_desc);
  e->e_desc = strdup(desc);
}


event_t *
epg_event_find_by_time0(struct event_queue *q, time_t start)
{
  event_t *e;

  TAILQ_FOREACH(e, q, e_link)
    if(start >= e->e_start && start < e->e_start + e->e_duration)
      break;
  return e;
}

event_t *
epg_event_find_by_time(th_channel_t *ch, time_t start)
{
  return epg_event_find_by_time0(&ch->ch_epg_events, start);
}

event_t *
epg_event_find_by_tag(uint32_t tag)
{
  event_t *e;
  unsigned int l = tag % EPG_HASH_ID_WIDTH;

  LIST_FOREACH(e, &epg_hash[l], e_hash_link)
    if(e->e_tag == tag)
      break;

  return e;
}


event_t *
epg_event_get_current(th_channel_t *ch)
{
  event_t *e;

  time_t now;
  time(&now);

  e = ch->ch_epg_cur_event;
  if(e == NULL || now < e->e_start || now > e->e_start + e->e_duration)
    return NULL;

  return e;
}

event_t *
epg_event_find_current_or_upcoming(th_channel_t *ch)
{
  event_t *e;

  TAILQ_FOREACH(e, &ch->ch_epg_events, e_link)
    if(e->e_start + e->e_duration > dispatch_clock)
      break;
  return e;
}


static int
startcmp(event_t *a, event_t *b)
{
  return a->e_start - b->e_start;
}

event_t *
epg_event_build(struct event_queue *head, time_t start, int duration)
{
  time_t now;
  event_t *e;

  time(&now);

  if(duration < 1 || start + duration < now - EPG_MAX_AGE)
    return NULL;

  TAILQ_FOREACH(e, head, e_link)
    if(start == e->e_start && duration == e->e_duration)
      return e;
 
  e = calloc(1, sizeof(event_t));

  e->e_duration = duration;
  e->e_start = start;
  TAILQ_INSERT_SORTED(head, e, e_link, startcmp);

  return e;
}



void
epg_event_free(event_t *e)
{
  free((void *)e->e_title);
  free((void *)e->e_desc);
  free(e);
}



static void
epg_event_destroy(th_channel_t *ch, event_t *e)
{
  //  printf("epg: flushed event %s\n", e->e_title);

  if(ch->ch_epg_cur_event == e)
    ch->ch_epg_cur_event = NULL;

  TAILQ_REMOVE(&ch->ch_epg_events, e, e_link);
  LIST_REMOVE(e, e_hash_link);

  epg_event_free(e);
}


void
event_time_txt(time_t start, int duration, char *out, int outlen)
{
  char tmp1[40];
  char tmp2[40];
  char *c;
  time_t stop = start + duration;

  ctime_r(&start, tmp1);
  c = strchr(tmp1, '\n');
  if(c)
    *c = 0;

  ctime_r(&stop, tmp2);
  c = strchr(tmp2, '\n');
  if(c)
    *c = 0;

  snprintf(out, outlen, "[%s - %s]", tmp1, tmp2);
}


static int
check_overlap0(th_channel_t *ch, event_t *a)
{
  char atime[100];
  char btime[100];
  event_t *b;
  int overshot;

  b = TAILQ_NEXT(a, e_link);
  if(b == NULL)
    return 0;

  overshot = a->e_start + a->e_duration - b->e_start;

  if(overshot < 1)
    return 0;

  event_time_txt(a->e_start, a->e_duration, atime, sizeof(atime));
  event_time_txt(b->e_start, b->e_duration, btime, sizeof(btime));

  if(a->e_source > b->e_source) {

    b->e_start += overshot;
    b->e_duration -= overshot;
    
    if(b->e_duration < 1) {
      epg_event_destroy(ch, b);
      return 1;
    }
  } else {
    a->e_duration -= overshot;

    if(a->e_duration < 1) {
      epg_event_destroy(ch, a);
      return 1;
    }
  }
  return 0;
}

static int
check_overlap(th_channel_t *ch, event_t *e)
{
  event_t *p;

  p = TAILQ_PREV(e, event_queue, e_link);
  if(p != NULL) {
    if(check_overlap0(ch, p))
      return 1;
  }

  return check_overlap0(ch, e);
}




static void
epg_event_create(th_channel_t *ch, time_t start, int duration, 
		 const char *title, const char *desc, int source, 
		 uint16_t id, refstr_t *icon)
{
  unsigned int l;
  time_t now;
  event_t *e;

  time(&now);

  if(duration < 1 || start + duration < now - EPG_MAX_AGE)
    return;

  TAILQ_FOREACH(e, &ch->ch_epg_events, e_link) {
    if(start == e->e_start && duration == e->e_duration)
      break;

    if(start == e->e_start && !strcmp(e->e_title ?: "", title))
      break;
  }

  if(e == NULL) {
 
    e = calloc(1, sizeof(event_t));

    e->e_start = start;
    TAILQ_INSERT_SORTED(&ch->ch_epg_events, e, e_link, startcmp);

    e->e_ch = ch;
    e->e_event_id = id;
    e->e_duration = duration;

    e->e_tag = tag_get();
    l = e->e_tag % EPG_HASH_ID_WIDTH;
    LIST_INSERT_HEAD(&epg_hash[l], e, e_hash_link);
  }

  if(e->e_icon == NULL)
    e->e_icon = refstr_dup(icon);
  else
    refstr_free(icon);

  if(source > e->e_source) {

    e->e_source = source;

    if(e->e_duration != duration) {
      char before[100];
      char after[100];

      event_time_txt(e->e_start, e->e_duration, before, sizeof(before));
      event_time_txt(e->e_start, duration, after, sizeof(after));

      e->e_duration = duration;
    }

    if(title != NULL) epg_event_set_title(e, title);
    if(desc  != NULL) epg_event_set_desc(e, desc);
  }
  
  check_overlap(ch, e);
}




void
epg_update_event_by_id(th_channel_t *ch, uint16_t event_id, 
		       time_t start, int duration, const char *title,
		       const char *desc)
{
  event_t *e;

  TAILQ_FOREACH(e, &ch->ch_epg_events, e_link)
    if(e->e_event_id == event_id)
      break;

  if(e != NULL) {
    /* We already have information about this event */

     if(e->e_duration != duration || e->e_start != start) {

      char before[100];
      char after[100];

      event_time_txt(e->e_start, e->e_duration, before, sizeof(before));
      event_time_txt(start, duration, after, sizeof(after));
       
      TAILQ_REMOVE(&ch->ch_epg_events, e, e_link);

      e->e_duration = duration;
      e->e_start = start;
      TAILQ_INSERT_SORTED(&ch->ch_epg_events, e, e_link, startcmp);

      if(check_overlap(ch, e))
	return; /* event was destroyed, return at once */
    }

    epg_event_set_title(e, title);
    epg_event_set_desc(e, desc);

  } else {
  
    epg_event_create(ch, start, duration, title, desc,
		     EVENT_SRC_DVB, event_id, NULL);
  }
}


static void
epg_set_current_event(th_channel_t *ch, event_t *e)
{
  if(e == ch->ch_epg_cur_event)
    return;

  ch->ch_epg_cur_event = e;

  /* Notify clients that a new programme is on */

  clients_send_ref(ch->ch_tag);

  htsp_async_channel_update(ch);
}

static void
epg_locate_current_event(th_channel_t *ch, time_t now)
{
  event_t *e;
  e = epg_event_find_by_time(ch, now);

  if(e != NULL && e->e_icon != NULL) {
    refstr_free(ch->ch_icon);
    ch->ch_icon = refstr_dup(e->e_icon);
  }
  epg_set_current_event(ch, e);
}



static void
epg_channel_maintain(void *aux, int64_t clk)
{
  th_channel_t *ch;
  event_t *e, *cur;
  time_t now;

  dtimer_arm(&epg_channel_maintain_timer, epg_channel_maintain, NULL, 5);

  now = dispatch_clock;

  epg_lock();

  LIST_FOREACH(ch, &channels, ch_global_link) {

    /* Age out any old events */

    e = TAILQ_FIRST(&ch->ch_epg_events);
    if(e != NULL && e->e_start + e->e_duration < now - EPG_MAX_AGE)
      epg_event_destroy(ch, e);

    cur = ch->ch_epg_cur_event;

    e = ch->ch_epg_cur_event;
    if(e == NULL) {
      epg_locate_current_event(ch, now);
      continue;
    }

    if(now >= e->e_start && now < e->e_start + e->e_duration)
      continue;

    e = TAILQ_NEXT(e, e_link);
    if(e != NULL && now >= e->e_start && now < e->e_start + e->e_duration) {
      epg_set_current_event(ch, e);
      continue;
    }

    epg_locate_current_event(ch, now);
  }

  epg_unlock();

}


/*
 *
 */

void
epg_transfer_events(th_channel_t *ch, struct event_queue *src, 
		    const char *srcname, refstr_t *icon)
{
  event_t *e;
  int cnt = 0;

  epg_lock();

  if(ch->ch_icon == NULL)
    ch->ch_icon = refstr_dup(icon);

  TAILQ_FOREACH(e, src, e_link) {

    epg_event_create(ch, e->e_start, e->e_duration, e->e_title,
		     e->e_desc, EVENT_SRC_XMLTV, 0, refstr_dup(icon));
    cnt++;
  }
  epg_unlock();
}



/*
 *
 */
void
epg_init(void)
{
  dtimer_arm(&epg_channel_maintain_timer, epg_channel_maintain, NULL, 5);
}

