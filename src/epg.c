/*
 *  Electronic Program Guide - Common functions
 *  Copyright (C) 2007 Andreas �man
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

#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#include "tvheadend.h"
#include "channels.h"
#include "settings.h"
#include "epg.h"
#include "dvr/dvr.h"
#include "htsp.h"
#include "htsmsg_binary.h"

#define EPG_MAX_AGE 86400

#define EPG_GLOBAL_HASH_WIDTH 1024
#define EPG_GLOBAL_HASH_MASK (EPG_GLOBAL_HASH_WIDTH - 1)
static struct event_list epg_hash[EPG_GLOBAL_HASH_WIDTH];

static void epg_expire_event_from_channel(void *opauqe);
static void epg_ch_check_current_event(void *aux);
/* helper function to fuzzy compare two events */
static int epg_event_cmp_overlap(event_t *e1, event_t *e2);
static void epg_erase_duplicates(event_t *e, channel_t *ch);


static int
e_ch_cmp(const event_t *a, const event_t *b)
{
  return a->e_start - b->e_start;
}

/**
 *
 */
static void
epg_set_current(channel_t *ch, event_t *e, event_t *next)
{
  if(next == NULL && e != NULL)
    next = RB_NEXT(e, e_channel_link);

  if(ch->ch_epg_current == e && ch->ch_epg_next == next)
    return;

  ch->ch_epg_current = e;
  ch->ch_epg_next = next;
  if(e != NULL)
    gtimer_arm_abs(&ch->ch_epg_timer_current, epg_ch_check_current_event,
		   ch, MAX(e->e_stop, dispatch_clock + 1));
  htsp_channel_update_current(ch);
}

/**
 *
 */
static void
epg_ch_check_current_event(void *aux)
{
  channel_t *ch = aux;
  event_t skel, *e = ch->ch_epg_current;
  if(e != NULL) {
    if(e->e_start <= dispatch_clock && e->e_stop > dispatch_clock) {
      epg_set_current(ch, e, NULL);
      return;
    }

    if((e = RB_NEXT(e, e_channel_link)) != NULL) {

      if(e->e_start <= dispatch_clock && e->e_stop > dispatch_clock) {
	epg_set_current(ch, e, NULL);
	return;
      }
    }
  }

  e = epg_event_find_by_time(ch, dispatch_clock);
  if(e != NULL) {
    epg_set_current(ch, e, NULL);
    return;
  }

  skel.e_start = dispatch_clock;
  e = RB_FIND_GT(&ch->ch_epg_events, &skel, e_channel_link, e_ch_cmp);
  if(e != NULL) {
    gtimer_arm_abs(&ch->ch_epg_timer_current, epg_ch_check_current_event,
		   ch, MAX(e->e_start, dispatch_clock + 1));
    epg_set_current(ch, NULL, e);
  } else {
    epg_set_current(ch, NULL, NULL);
  }
}


/**
 *
 */
void
epg_event_updated(event_t *e)
{
  dvr_autorec_check_event(e);
}


/**
 *
 */
int
epg_event_set_title(event_t *e, const char *title)
{
  if(e->e_title != NULL && !strcmp(e->e_title, title))
    return 0;
  free(e->e_title);
  e->e_title = strdup(title);
  return 1;
}


/**
 *
 */
int
epg_event_set_desc(event_t *e, const char *desc)
{
  if(e->e_desc != NULL && strlen(e->e_desc) >= strlen(desc)) {
    /* The current description is longer than the one we try to set.
     * We assume that a longer description is better than a shorter
     * so we just bail out.
     * Typically happens when the XMLTV and DVB EPG feed differs.
     */
    return 0;
  }
  free(e->e_desc);
  e->e_desc = strdup(desc);
  return 1;
}

/**
 *
 */
int
epg_event_set_ext_desc(event_t *e, int ext_dn, const char *desc)
{
  if(e->e_ext_desc == NULL && ext_dn != 0)
    return 0;
  if(e->e_ext_desc != NULL && strstr(e->e_ext_desc, desc))
    return 0;

  int len = strlen(desc) + ( e->e_ext_desc ? strlen(e->e_ext_desc) : 0) + 1;
  char *tmp = (char*)malloc(len);

  if(e->e_ext_desc) {
    strcpy(tmp, e->e_ext_desc);
    strcat(tmp, desc);
    free(e->e_ext_desc);
  } else
    strcpy(tmp, desc);

  e->e_ext_desc = tmp;
  return 1;
}

/**
 *
 */
int
epg_event_set_ext_item(event_t *e, int ext_dn, const char *item)
{
  if(e->e_ext_item == NULL && ext_dn != 0)
    return 0;
  if(e->e_ext_item != NULL && strstr(e->e_ext_item, item))
    return 0;

  int len = strlen(item) + ( e->e_ext_item ? strlen(e->e_ext_item) : 0) + 1;
  char *tmp = (char*)malloc(len);

  if(e->e_ext_item) {
    strcpy(tmp, e->e_ext_item);
    strcat(tmp, item);
    free(e->e_ext_item);
  } else
    strcpy(tmp, item);

  e->e_ext_item = tmp;
  return 1;
}

/**
 *
 */
int
epg_event_set_ext_text(event_t *e, int ext_dn, const char *text)
{
  if(e->e_ext_text == NULL && ext_dn != 0)
    return 0;
  if(e->e_ext_text != NULL && strstr(e->e_ext_text, text))
    return 0;

  int len = strlen(text) + ( e->e_ext_text ? strlen(e->e_ext_text) : 0) + 1;
  char *tmp = (char*)malloc(len);

  if(e->e_ext_text) {
    strcpy(tmp, e->e_ext_text);
    strcat(tmp, text);
    free(e->e_ext_text);
  } else
    strcpy(tmp, text);

  e->e_ext_text = tmp;
  return 1;
}

/**
 *
 */
int
epg_event_set_content_type(event_t *e, uint8_t type)
{
  if(e->e_content_type == type)
    return 0;

  e->e_content_type = type;
  return 1;
}


/**
 *
 */
int
epg_event_set_episode(event_t *e, epg_episode_t *ee)
{
  if(e->e_episode.ee_season  == ee->ee_season &&
     e->e_episode.ee_episode == ee->ee_episode && 
     e->e_episode.ee_part    == ee->ee_part && 
     !strcmp(e->e_episode.ee_onscreen ?: "", ee->ee_onscreen ?: ""))
    return 0;

  e->e_episode.ee_season  = ee->ee_season;
  e->e_episode.ee_episode = ee->ee_episode;
  e->e_episode.ee_part    = ee->ee_part;

  tvh_str_set(&e->e_episode.ee_onscreen, ee->ee_onscreen);
  return 1;
}


/**
 *
 */
static void
epg_event_destroy(event_t *e)
{
  free(e->e_title);
  free(e->e_desc);
  free(e->e_ext_desc);
  free(e->e_ext_item);
  free(e->e_ext_text);
  free(e->e_episode.ee_onscreen);
  LIST_REMOVE(e, e_global_link);
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

/**
 *
 */
static void
epg_remove_event_from_channel(channel_t *ch, event_t *e)
{
  int wasfirst = e == RB_FIRST(&ch->ch_epg_events);
  event_t *n = RB_NEXT(e, e_channel_link);

  assert(e->e_channel == ch);

  RB_REMOVE(&ch->ch_epg_events, e, e_channel_link);
  e->e_channel = NULL;
  epg_event_unref(e);

  if(ch->ch_epg_current == e) {
    epg_set_current(ch, NULL, n);

    if(n != NULL)
      gtimer_arm_abs(&ch->ch_epg_timer_current, epg_ch_check_current_event,
		     ch, n->e_start);
  } else if(ch->ch_epg_next == e) {
    epg_set_current(ch, ch->ch_epg_current, NULL);
  }

  if(wasfirst && (e = RB_FIRST(&ch->ch_epg_events)) != NULL) {
    gtimer_arm_abs(&ch->ch_epg_timer_head, epg_expire_event_from_channel,
		   ch, e->e_stop);
  }
}


/**
 *
 */
event_t *
epg_event_create(channel_t *ch, time_t start, time_t stop, int dvb_id,
		 int *created)
{
  static event_t *skel;
  event_t *e;
  static int tally;

  if(created != NULL)
    *created = 0;

  if((stop - start) > 11 * 3600)
    return NULL;

  if(stop <= start)
    return NULL;

  lock_assert(&global_lock);

  if(skel == NULL)
    skel = calloc(1, sizeof(event_t));

  skel->e_start = start;
  
  e = RB_INSERT_SORTED(&ch->ch_epg_events, skel, e_channel_link, e_ch_cmp);
  if(e == NULL) {
    /* New entry was inserted */

    if(created != NULL)
      *created = 1;

    e = skel;
    skel = NULL;

    e->e_id = ++tally;
    e->e_stop = stop;
    e->e_dvb_id = dvb_id;

    LIST_INSERT_HEAD(&epg_hash[e->e_id & EPG_GLOBAL_HASH_MASK], e,
		     e_global_link);

    e->e_refcount = 1;
    e->e_channel = ch;

    if(e == RB_FIRST(&ch->ch_epg_events)) {
      /* First in temporal order, arm expiration timer */

      gtimer_arm_abs(&ch->ch_epg_timer_head, epg_expire_event_from_channel,
		     ch, e->e_stop);
    }


    
    if(ch->ch_epg_timer_current.gti_callback == NULL ||
       start < ch->ch_epg_timer_current.gti_expire) {

      gtimer_arm_abs(&ch->ch_epg_timer_current, epg_ch_check_current_event,
		     ch, e->e_start);
    }
  } else {
    /* Already exist */

    if(stop > e->e_stop) {
      /* We allow the event to extend in time */
#if 0
      printf("Event %s on %s extended stop time\n", e->e_title,
	     e->e_channel->ch_name);
      printf("Previous %s", ctime(&e->e_stop));
      printf("     New %s", ctime(&stop));
#endif
      e->e_stop = stop;

      if(e == ch->ch_epg_current) {
	gtimer_arm_abs(&ch->ch_epg_timer_current, epg_ch_check_current_event,
		       ch, e->e_start);
      }
    }
  }

  epg_erase_duplicates(e, ch);
  return e;
}

static void
epg_erase_duplicates(event_t *e, channel_t *ch) {

  event_t *p, *n;
  int dvb_id = e->e_dvb_id;

  if(dvb_id != -1) {
    /* Erase any close events with the same DVB event id or are very similar*/

    if((p = RB_PREV(e, e_channel_link)) != NULL) {
      if(p->e_dvb_id == dvb_id || epg_event_cmp_overlap(p, e)) {
        tvhlog(LOG_DEBUG, "epg",
               "Removing overlapping event instance %s from EPG", p->e_title);
        dvr_event_replaced(p, e);
	epg_remove_event_from_channel(ch, p);
      } else if((p = RB_PREV(p, e_channel_link)) != NULL) {
	if(p->e_dvb_id == dvb_id || epg_event_cmp_overlap(p, e)) {
          tvhlog(LOG_DEBUG, "epg",
                 "Removing overlapping event instance %s from EPG", p->e_title);
          dvr_event_replaced(p, e);
	  epg_remove_event_from_channel(ch, p);
        }
      }
    }

    if((n = RB_NEXT(e, e_channel_link)) != NULL) {
      if(n->e_dvb_id == dvb_id || epg_event_cmp_overlap(n, e)) {
        tvhlog(LOG_DEBUG, "epg",
               "Removing overlapping event instance %s from EPG", n->e_title);
        dvr_event_replaced(n, e);
	epg_remove_event_from_channel(ch, n);
      } else if((n = RB_NEXT(n, e_channel_link)) != NULL) {
	if(n->e_dvb_id == dvb_id || epg_event_cmp_overlap(n, e)) {
          tvhlog(LOG_DEBUG, "epg",
                 "Removing overlapping event instance %s from EPG", n->e_title);
          dvr_event_replaced(n, e);
	  epg_remove_event_from_channel(ch, n);
        }   
      }
    }
  }
  
}

static int
epg_event_cmp_overlap(event_t *e1, event_t *e2)
{

  int dur_a, dur_b, mindur;
    
  if ((e1->e_title == NULL) || (e2->e_title == NULL))
    return 0;
    
  dur_a = e1->e_stop - e1->e_start;
  dur_b = e2->e_stop - e2->e_start;
  mindur = dur_a < dur_b ? dur_a : dur_b;
    
  if ((abs(e1->e_start - e2->e_start) < mindur) && (abs(e1->e_stop - e2->e_stop) < mindur)) {
    return 1;
  }

  return 0;
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
  if(e == NULL || e->e_stop < t)
    return NULL;
  return e;
}


/**
 *
 */
event_t *
epg_event_find_by_id(int eventid)
{
  event_t *e;

  LIST_FOREACH(e, &epg_hash[eventid & EPG_GLOBAL_HASH_MASK], e_global_link)
    if(e->e_id == eventid)
      break;
  return e;
}



/**
 *
 */
static void
epg_expire_event_from_channel(void *opaque)
{
  channel_t *ch = opaque;
  event_t *e = RB_FIRST(&ch->ch_epg_events);
  epg_remove_event_from_channel(ch, e);
}


/**
 *
 */
void
epg_unlink_from_channel(channel_t *ch)
{
  event_t *e;

  while((e = ch->ch_epg_events.root) != NULL)
    epg_remove_event_from_channel(ch, e);

  gtimer_disarm(&ch->ch_epg_timer_head);
  gtimer_disarm(&ch->ch_epg_timer_current);

}



/**
 * EPG content group
 *
 * Based on the content types defined in EN 300 468
 */
static const char *groupnames[16] = {
  [1] = "Movie / Drama",
  [2] = "News / Current affairs",
  [3] = "Show / Games",
  [4] = "Sports",
  [5] = "Children's / Youth",
  [6] = "Music",
  [7] = "Art / Culture",
  [8] = "Social / Political issues / Economics",
  [9] = "Education / Science / Factual topics",
  [10] = "Leisure hobbies",
  [11] = "Special characteristics",
};

static const char *groupdefinition[0xFF] = {
    /* ox1Y are Movie/Drama */
    [0x10] = "Movie / Drama",
    [0x11] = "Detective / Thriller",
    [0x12] = "Adventure / Western / War",
    [0x13] = "science fiction/fantasy/horror",
    [0x14] = "comedy",
    [0x15] = "soap/melodrama/folkloric",
    [0x16] = "romance",
    [0x17] = "serious/classical/religious/historical movie/drama",
    [0x18] = "Drama",
    [0x19] = "film",
    [0x1A] = "movie",
    [0x1B] = "comedy drama",
    [0x1C] = "sitcom",
    [0x1D] = "entertainment",
    /* 0x2Y: News / Current Affairs */
    [0x20] = "News / Current Affairs",
    [0x21] = "news/weather report",
    [0x22] = "news magazine",
    [0x23] = "documentary",
    [0x24] = "discussion/interview/debate",
    [0x25] = "news",
    [0x26] = "current Affairs",
    [0x27] = "news and current affairs",
    /*0x3Y:  Show/Game show */
    [0x30] = "show/game show",
    [0x31] = "game show/quiz/contest",
    [0x32] = "variety show",
    [0x33] = "talk show",
    [0x34] = "game Show",
    /*0x4Y: Sports */
    [0x40] = "sports",
    [0x41] = "special events",
    [0x42] = "sports magazines",
    [0x43] = "football/soccer",
    [0x44] = "tennis/squash",
    [0x45] = "team sports",
    [0x46] = "athletics",
    [0x47] = "motor sport",
    [0x48] = "water sport",
    [0x49] = "winter sports",
    [0x4A] = "equestrian",
    [0x4B] = "martial sports",
    [0x4C] = "Sport",
    /*0x5Y: Children's/Youth programmes */
    [0x50] = "children's/youth programmes",
    [0x51] = "pre-school children's programmes",
    [0x52] = "entertainment programmes for 6 to 14",
    [0x53] = "entertainment programmes for 10 to 16",
    [0x54] = "informational/educational/school programmes",
    [0x55] = "cartoons/puppets",
    [0x56] = "children",
    [0x57] = "animation",
    /*0x6Y: Music/Ballet/Dance */
    [0x60] = "music/ballet/dance",
    [0x61] = "rock/pop ",
    [0x62] = "serious music/classical music",
    [0x63] = "folk/traditional music",
    [0x64] = "jazz",
    [0x65] = "musical/opera",
    [0x66] = "ballet",
    [0x67] = "music",
    [0x68] = "music and arts",
    /*0x7Y: Arts/Culture */
    [0x70] = "arts/culture",
    [0x71] = "performing arts",
    [0x72] = "fine arts",
    [0x73] = "religion",
    [0x74] = "popular culture/traditional arts",
    [0x75] = "literature",
    [0x76] = "film/cinema",
    [0x77] = "experimental film/video",
    [0x78] = "broadcasting/press",
    [0x79] = "new media",
    [0x7A] = "arts/culture magazines",
    [0x7B] = "fashion",
    [0x7C] = "arts and culture",
    /*0x8Y: Social/Political issues/Economics */
    [0x80] = "social/political issues/economics",
    [0x81] = "magazines/reports/documentary",
    [0x82] = "economics/social advisory",
    [0x83] = "remarkable people",
    [0x84] = "Discussion/Debate",
    [0x85] = "Reality",
    [0x86] = "Soap",
    /*0x8Y: Education/ Science/Factual topics */
    [0x90] = "education/science/factual topics",
    [0x91] = "nature/animals/environment",
    [0x92] = "technology/natural sciences",
    [0x93] = "medicine/physiology/psychology",
    [0x94] = "foreign countries/expeditions",
    [0x95] = "social/spiritual sciences",
    [0x96] = "further education",
    [0x97] = "languages",
    [0x98] = "Education",
    [0x99] = "sci-fi",
    [0x9A] = "drama documentary",
    [0x9B] = "documentary",
    [0x9C] = "factual",
    [0x9D] = "science",
    [0x9E] = "religion",
    /*0xA0: Leisure hobbies */
    [0xA0] = "leisure hobbies",
    [0xA1] = "tourism/travel",
    [0xA2] = "handicraft",
    [0xA3] = "motoring",
    [0xA4] = "fitness & health",
    [0xA5] = "cooking",
    [0xA6] = "advertisement/shopping",
    [0xA8] = "gardening",
    [0xA9] = "transport",
    [0xAA] = "cookery",
    [0xAB] = "nature",
    [0xAC] = "health",
    [0xAD] = "home and property",
    [0xAE] = "travel",
    [0xAF] = "FOOD",
    /*0xB0: Special Characteristics */
    [0xB0] = "original language",
    [0xB1] = "black & white",
    [0xB2] = "unpublished",
    [0xB3] = "live broadcast",
    [0xB4] = "special characteristics",
    [0xB5] = "interests",
};

/**
 *
 */
const char *
epg_content_group_get_name(uint8_t id)
{
  return id < 16 ? groupnames[id] : NULL;
}

/**
 *
 */
uint8_t
epg_content_group_find_by_name(const char *name)
{
      unsigned int i;
      for(i = 0; i < 0xFF; i++) {
         if(groupdefinition[i] != NULL && !strcasecmp(name, groupdefinition[i])){
             int b = (i >> 4) & 0xF;
             return b;
         }
      }
      return 0;
}


/**
 *
 */
static int
epg_event_create_by_msg(htsmsg_t *c, time_t now)
{
 channel_t *ch;
  event_t *e = NULL;
  uint32_t ch_id = 0;
  uint32_t e_start = 0;
  uint32_t e_stop = 0;
  int e_dvb_id = 0, v;
  const char *s;

  // Now create the event
  if(htsmsg_get_u32(c, "ch_id", &ch_id))
    return 0;

  if((ch = channel_find_by_identifier(ch_id)) == NULL)
    return 0;

  if(htsmsg_get_u32(c, "start", &e_start))
    return 0;

  if(htsmsg_get_u32(c, "stop", &e_stop))
    return 0;

  if(e_stop < now)
    return 0;

  if(htsmsg_get_s32(c, "dvb_id", &e_dvb_id))
    e_dvb_id = -1;

  e = epg_event_create(ch, e_start, e_stop, e_dvb_id, NULL);

  if((s = htsmsg_get_str(c, "title")) != NULL)
    epg_event_set_title(e, s);

  if((s = htsmsg_get_str(c, "desc")) != NULL)
    epg_event_set_desc(e, s);

  if(!htsmsg_get_s32(c, "season", &v))
    e->e_episode.ee_season = v;

  if(!htsmsg_get_s32(c, "episode", &v))
    e->e_episode.ee_episode = v;

  if(!htsmsg_get_s32(c, "part", &v))
    e->e_episode.ee_part = v;

  if((s = htsmsg_get_str(c, "epname")) != NULL)
    tvh_str_set(&e->e_episode.ee_onscreen, s);

  return 1;
}


/**
 *
 */
static void
epg_load(void)
{
  struct stat st;
  int fd = hts_settings_open_file(0, "epgdb");
  time_t now;
  int created = 0;

  time(&now);

  if(fd == -1)
    return;

  if(fstat(fd, &st)) {
    close(fd);
    return;
  }
  uint8_t *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if(mem == MAP_FAILED) {
    close(fd);
    return;
  }
  const uint8_t *rp = mem;
  size_t remain = st.st_size;

  while(remain > 4) {
    int msglen = (rp[0] << 24) | (rp[1] << 16) | (rp[2] << 8) | rp[3];
    remain -= 4;
    rp += 4;

    if(msglen > remain) {
      tvhlog(LOG_ERR, "EPG", "Malformed EPG database, skipping some data");
      break;
    }
    htsmsg_t *m = htsmsg_binary_deserialize(rp, msglen, NULL);

    created += epg_event_create_by_msg(m, now);

    htsmsg_destroy(m);
    rp += msglen;
    remain -= msglen;
  }

  munmap(mem, st.st_size);
  close(fd);
  tvhlog(LOG_NOTICE, "EPG", "Injected %d event from disk database", created);
}

/**
 *
 */
void
epg_init(void)
{
  channel_t *ch;

  epg_load();
  
  RB_FOREACH(ch, &channel_name_tree, ch_name_link)
    epg_ch_check_current_event(ch);
}


/**
 * Save the epg on disk
 */ 
void
epg_save(void)
{
  event_t *e;
  int num_saved = 0;
  size_t msglen;
  void *msgdata;
  channel_t *ch;

  int fd = hts_settings_open_file(1, "epgdb");

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    RB_FOREACH(e, &ch->ch_epg_events, e_channel_link) {
      if(!e->e_start || !e->e_stop)
	continue;

      htsmsg_t *m = htsmsg_create_map();
      htsmsg_add_u32(m, "ch_id", ch->ch_id);
      htsmsg_add_u32(m, "start", e->e_start);
      htsmsg_add_u32(m, "stop", e->e_stop);
      if(e->e_title != NULL)
	htsmsg_add_str(m, "title", e->e_title);
      if(e->e_desc != NULL)
	htsmsg_add_str(m, "desc", e->e_desc);
      if(e->e_dvb_id)
	htsmsg_add_u32(m, "dvb_id", e->e_dvb_id);

      if(e->e_episode.ee_season)
	htsmsg_add_u32(m, "season", e->e_episode.ee_season);

      if(e->e_episode.ee_episode)
	htsmsg_add_u32(m, "episode", e->e_episode.ee_episode);

      if(e->e_episode.ee_part)
	htsmsg_add_u32(m, "part", e->e_episode.ee_part);

      if(e->e_episode.ee_onscreen)
	htsmsg_add_str(m, "epname", e->e_episode.ee_onscreen);


      int r = htsmsg_binary_serialize(m, &msgdata, &msglen, 0x10000);
      htsmsg_destroy(m);

      if(!r) {
	ssize_t written = write(fd, msgdata, msglen);
	int err = errno;
	free(msgdata);
	if(written != msglen) {
	  tvhlog(LOG_DEBUG, "epg", "Failed to store EPG on disk -- %s",
		 strerror(err));
	  close(fd);
	  hts_settings_remove("epgdb");
	  return;
	}
      }
      num_saved++;
    }
  }
  close(fd);
  tvhlog(LOG_DEBUG, "EPG", "Stored EPG data for %d events on disk", num_saved);
}


/**
 *
 */
static void
eqr_add(epg_query_result_t *eqr, event_t *e, regex_t *preg, time_t now)
{
  if(e->e_title == NULL)
    return;

  if(preg != NULL && regexec(preg, e->e_title, 0, NULL, 0))
    return;

  if(e->e_stop < now)
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
		      uint8_t content_type, regex_t *preg, time_t now)
{
  event_t *e;

  if(content_type == 0) {
    RB_FOREACH(e, &ch->ch_epg_events, e_channel_link)
      eqr_add(eqr, e, preg, now);
  } else {
    RB_FOREACH(e, &ch->ch_epg_events, e_channel_link)
      if(content_type == e->e_content_type)
	eqr_add(eqr, e, preg, now);
  }
}

/**
 *
 */
void
epg_query0(epg_query_result_t *eqr, channel_t *ch, channel_tag_t *ct,
           uint8_t content_type, const char *title)
{
  channel_tag_mapping_t *ctm;
  time_t now;
  regex_t preg0, *preg;

  lock_assert(&global_lock);
  memset(eqr, 0, sizeof(epg_query_result_t));
  time(&now);

  if(title != NULL) {
    if(regcomp(&preg0, title, REG_ICASE | REG_EXTENDED | REG_NOSUB))
      return;
    preg = &preg0;
  } else {
    preg = NULL;
  }

  if(ch != NULL && ct == NULL) {
    epg_query_add_channel(eqr, ch, content_type, preg, now);
    return;
  }
  
  if(ct != NULL) {
    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      if(ch == NULL || ctm->ctm_channel == ch)
	epg_query_add_channel(eqr, ctm->ctm_channel, content_type, preg, now);
    return;
  }

  RB_FOREACH(ch, &channel_name_tree, ch_name_link)
    epg_query_add_channel(eqr, ch, content_type, preg, now);
}

/**
 *
 */
void
epg_query(epg_query_result_t *eqr, const char *channel, const char *tag,
	  const char *contentgroup, const char *title)
{
  channel_t *ch = channel ? channel_find_by_name(channel, 0, 0) : NULL;
  channel_tag_t *ct = tag ? channel_tag_find_by_name(tag, 0) : NULL;
  uint8_t content_type = contentgroup ? 
    epg_content_group_find_by_name(contentgroup) : 0;
  epg_query0(eqr, ch, ct, content_type, title);
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
