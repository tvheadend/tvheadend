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
#include <regex.h>
#include <assert.h>

#include "tvhead.h"
#include "channels.h"
#include "epg.h"

#define EPG_MAX_AGE 86400

epg_content_group_t *epg_content_groups[16];

static int
e_ch_cmp(const event_t *a, const event_t *b)
{
  return a->e_start - b->e_start;
}


/**
 *
 */
static void
epg_event_changed(event_t *e)
{
  /* nothing atm  */
}


/**
 *
 */
void
epg_event_set_title(event_t *e, const char *title)
{
  if(e->e_title != NULL && !strcmp(e->e_title, title))
    return;
  free((void *)e->e_title);
  e->e_title = strdup(title);
  epg_event_changed(e);
}


/**
 *
 */
void
epg_event_set_desc(event_t *e, const char *desc)
{
  if(e->e_desc != NULL && !strcmp(e->e_desc, desc))
    return;
  free((void *)e->e_desc);
  e->e_desc = strdup(desc);
  epg_event_changed(e);
}


/**
 *
 */
void
epg_event_set_duration(event_t *e, int duration)
{
  if(e->e_duration == duration)
    return; 

  e->e_duration = duration;
  epg_event_changed(e);
}


/**
 *
 */
void
epg_event_set_content_type(event_t *e, epg_content_type_t *ect)
{
  if(e->e_content_type == ect)
    return;

  if(e->e_content_type != NULL)
    LIST_REMOVE(e, e_content_type_link);
  
  e->e_content_type = ect;
  if(ect != NULL)
    LIST_INSERT_HEAD(&ect->ect_events, e, e_content_type_link);

  epg_event_changed(e);
}


/**
 *
 */
event_t *
epg_event_find_by_start(channel_t *ch, time_t start, int create)
{
  static event_t *skel, *e;
  static int tally;

  lock_assert(&global_lock);

  if(skel == NULL)
    skel = calloc(1, sizeof(event_t));

  skel->e_start = start;

  if(!create)
    return RB_FIND(&ch->ch_epg_events, skel, e_channel_link, e_ch_cmp);

  e = RB_INSERT_SORTED(&ch->ch_epg_events, skel, e_channel_link, e_ch_cmp);
  if(e == NULL) {
    /* New entry was inserted */
    e = skel;
    skel = NULL;

    e->e_id = ++tally;
    e->e_refcount = 1;
    e->e_channel = ch;
    epg_event_changed(e);
  }

  return e;
}


/**
 *
 */
event_t *
epg_event_find_by_time(channel_t *ch, time_t t)
{
  event_t skel, *e;

  skel.e_start = t;
  e = RB_FIND_LE(&ch->ch_epg_events, &skel, e_channel_link, e_ch_cmp);
  if(e == NULL || e->e_start + e->e_duration < t)
    return NULL;
  return e;
}


/**
 *
 */
static void
epg_event_destroy(event_t *e)
{
  if(e->e_content_type != NULL)
    LIST_REMOVE(e, e_content_type_link);

  free((void *)e->e_title);
  free((void *)e->e_desc);
  free(e);
}

/**
 *
 */
static void
epg_event_unref(event_t *e)
{
  if(e->e_refcount > 1) {
    e->e_refcount--;
    return;
  }
  assert(e->e_refcount == 1);
  epg_event_destroy(e);
}


#if 0
/**
 *
 */
void
epg_channel_maintain()
{
  channel_t *ch;
  event_t *e, *cur;
  time_t now;

  dtimer_arm(&epg_channel_maintain_timer, epg_channel_maintain, NULL, 5);

  now = dispatch_clock;

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {

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

    e = TAILQ_NEXT(e, e_channel_link);
    if(e != NULL && now >= e->e_start && now < e->e_start + e->e_duration) {
      epg_set_current_event(ch, e);
      continue;
    }

    epg_locate_current_event(ch, now);
  }
}
#endif


/**
 *
 */
void
epg_unlink_from_channel(channel_t *ch)
{
  event_t *e;

  while((e = ch->ch_epg_events.root) != NULL) {
    RB_REMOVE(&ch->ch_epg_events, e, e_channel_link);
    e->e_channel = NULL;
    epg_event_unref(e);
  }
}



/**
 *
 */
static const char *groupnames[16] = {
  [0] = "Unclassified",
  [1] = "Movie / Drama",
  [2] = "News / Current affairs",
  [3] = "Show / Games",
  [4] = "Sports",
  [5] = "Children's/Youth",
  [6] = "Music",
  [7] = "Art/Culture",
  [8] = "Social/Political issues/Economics",
  [9] = "Education/Science/Factual topics",
  [10] = "Leisure hobbies",
  [11] = "Special characteristics",
};

/**
 *
 */
const char *
epg_content_group_get_name(unsigned int id)
{
  return id < 16 ? groupnames[id] : NULL;
}

/**
 * Find a content type
 */
epg_content_type_t *
epg_content_type_find_by_dvbcode(uint8_t dvbcode)
{
  epg_content_group_t *ecg;
  epg_content_type_t *ect;
  int group = dvbcode >> 4;
  int type  = dvbcode & 0xf;
  char buf[20];

  ecg = epg_content_groups[group];
  if(ecg == NULL) {
    ecg = epg_content_groups[group] = calloc(1, sizeof(epg_content_group_t));
    ecg->ecg_name = groupnames[group];
  }

  ect = ecg->ecg_types[type];
  if(ect == NULL) {
    ect = ecg->ecg_types[type] = calloc(1, sizeof(epg_content_type_t));
    ect->ect_group = ecg;
    snprintf(buf, sizeof(buf), "type%d", type);
    ect->ect_name = strdup(buf);
  }

  return ect;
}

/**
 *
 */
epg_content_group_t *
epg_content_group_find_by_name(const char *name)
{
  epg_content_group_t *ecg;
  int i;
  
  for(i = 0; i < 16; i++) {
    ecg = epg_content_groups[i];
    if(ecg != NULL && ecg->ecg_name && !strcmp(name, ecg->ecg_name))
      return ecg;
  }
  return NULL;
}


/**
 * Given the arguments, search all EPG events and enlist them
 *
 * XXX: Optimize if we know channel, group, etc
 */
#if 0
int
epg_search(struct event_list *h, const char *title, 
	   epg_content_group_t *s_ecg,
	   channel_t *s_ch)
{
  channel_t *ch;
  event_t *e;
  int num = 0;
  regex_t preg;

  if(title != NULL && 
     regcomp(&preg, title, REG_ICASE | REG_EXTENDED | REG_NOSUB))
    return -1;

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    if(LIST_FIRST(&ch->ch_transports) == NULL)
      continue;

    if(s_ch != NULL && s_ch != ch)
      continue;

    TAILQ_FOREACH(e, &ch->ch_epg_events, e_channel_link) {

      if(e->e_start + e->e_duration < dispatch_clock)
	continue; /* old */

      if(s_ecg != NULL) {
	if(e->e_content_type == NULL)
	  continue;
	
	if(e->e_content_type->ect_group != s_ecg)
	  continue;
      }

      if(title != NULL) {
	if(regexec(&preg, e->e_title, 0, NULL, 0))
	  continue;
      }

      LIST_INSERT_HEAD(h, e, e_tmp_link);
      num++;
    }
  }
  if(title != NULL)
    regfree(&preg);

  return num;
}
#endif

/*
 *
 */
void
epg_init(void)
{
  int i;

  for(i = 0x0; i < 0x100; i+=16)
    epg_content_type_find_by_dvbcode(i);
}


/**
 *
 */
static void
eqr_add(epg_query_result_t *eqr, event_t *e, regex_t *preg, time_t now)
{
  if(preg != NULL && regexec(preg, e->e_title, 0, NULL, 0))
    return;

  if(e->e_start + e->e_duration < now)
    return; /* Already passed */

  if(eqr->eqr_entries == eqr->eqr_alloced) {
    /* Need to alloc more space */

    eqr->eqr_alloced = MAX(100, eqr->eqr_alloced * 2);
    eqr->eqr_array = realloc(eqr->eqr_array, 
			     eqr->eqr_alloced * sizeof(event_t *));
  }
  eqr->eqr_array[eqr->eqr_entries++] = e;
  e->e_refcount++;
}

/**
 *
 */
static void
epg_query_add_channel(epg_query_result_t *eqr, channel_t *ch,
		      epg_content_group_t *ecg, regex_t *preg, time_t now)
{
  event_t *e;

  if(ecg == NULL) {
    RB_FOREACH(e, &ch->ch_epg_events, e_channel_link)
      eqr_add(eqr, e, preg, now);
    return;
  }

  RB_FOREACH(e, &ch->ch_epg_events, e_channel_link)
    if(e->e_content_type != NULL && ecg == e->e_content_type->ect_group)
      eqr_add(eqr, e, preg, now);
}

/**
 *
 */
void
epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	  const char *contentgroup, const char *title)
{
  channel_t *ch = channel ? channel_find_by_name(channel, 0) : NULL;
  channel_tag_t *ct = tag ? channel_tag_find_by_name(tag)    : NULL;
  epg_content_group_t *ecg = contentgroup ? 
    epg_content_group_find_by_name(contentgroup) : NULL;
  channel_tag_mapping_t *ctm;
  time_t now;

  time(&now);

  regex_t preg0, *preg;

  if(title != NULL) {
    if(regcomp(&preg0, title, REG_ICASE | REG_EXTENDED | REG_NOSUB))
      return;
    preg = &preg0;
  } else {
    preg = NULL;
  }

  lock_assert(&global_lock);
  memset(eqr, 0, sizeof(epg_query_result_t));

  if(ch != NULL && ct == NULL) {
    epg_query_add_channel(eqr, ch, ecg, preg, now);
    return;
  }
  
  if(ct != NULL) {
    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      if(ch == NULL || ctm->ctm_channel == ch)
	epg_query_add_channel(eqr, ctm->ctm_channel, ecg, preg, now);
    return;
  }

  RB_FOREACH(ch, &channel_name_tree, ch_name_link)
    epg_query_add_channel(eqr, ch, ecg, preg, now);
}


/**
 *
 */
void
epg_query_free(epg_query_result_t *eqr)
{
  int i;

  for(i = 0; i < eqr->eqr_entries; i++)
    epg_event_unref(eqr->eqr_array[i]);
  free(eqr->eqr_array);
}

/**
 * Sorting functions
 */
static int
epg_sort_start_ascending(const void *A, const void *B)
{
  event_t *a = *(event_t **)A;
  event_t *b = *(event_t **)B;
  return a->e_start - b->e_start;
}

/**
 *
 */
void
epg_query_sort(epg_query_result_t *eqr)
{
  int (*sf)(const void *a, const void *b);

  sf = epg_sort_start_ascending;

  qsort(eqr->eqr_array, eqr->eqr_entries, sizeof(event_t *), sf);
}
