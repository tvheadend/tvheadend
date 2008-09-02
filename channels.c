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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <libhts/htssettings.h>

#include "tvhead.h"
#include "psi.h"
#include "channels.h"
#include "transports.h"
#include "epg.h"
#include "pvr.h"
#include "autorec.h"

struct channel_tree channel_name_tree;
static struct channel_tree channel_identifier_tree;

static int
dictcmp(const char *a, const char *b)
{
  long int da, db;

  while(1) {
    switch((*a >= '0' && *a <= '9' ? 1 : 0)|(*b >= '0' && *b <= '9' ? 2 : 0)) {
    case 0:  /* 0: a is not a digit, nor is b */
      if(*a != *b)
	return *(const unsigned char *)a - *(const unsigned char *)b;
      if(*a == 0)
	return 0;
      a++;
      b++;
      break;
    case 1:  /* 1: a is a digit,  b is not */
    case 2:  /* 2: a is not a digit,  b is */
	return *(const unsigned char *)a - *(const unsigned char *)b;
    case 3:  /* both are digits, switch to integer compare */
      da = strtol(a, (char **)&a, 10);
      db = strtol(b, (char **)&b, 10);
      if(da != db)
	return da - db;
      break;
    }
  }
}


/**
 *
 */
static int
channelcmp(const channel_t *a, const channel_t *b)
{
  return dictcmp(a->ch_name, b->ch_name);
}


/**
 *
 */
static int
chidcmp(const channel_t *a, const channel_t *b)
{
  return a->ch_id - b->ch_id;
}


/**
 *
 */
static void
channel_set_name(channel_t *ch, const char *name)
{
  channel_t *x;
  const char *n2;
  int l, i;
  char *cp, c;

  free((void *)ch->ch_name);
  free((void *)ch->ch_sname);

  ch->ch_name = strdup(name);

  l = strlen(name);
  ch->ch_sname = cp = malloc(l + 1);

  n2 = utf8toprintable(name);

  for(i = 0; i < strlen(n2); i++) {
    c = tolower(n2[i]);
    if(isalnum(c))
      *cp++ = c;
    else
      *cp++ = '-';
  }
  *cp = 0;

  free((void *)n2);

  x = RB_INSERT_SORTED(&channel_name_tree, ch, ch_name_link, channelcmp);
  assert(x == NULL);
}



/**
 *
 */
static channel_t *
channel_create(const char *name)
{
  channel_t *ch, *x;
  int id;

  ch = RB_LAST(&channel_identifier_tree);
  if(ch == NULL) {
    id = 1;
  } else {
    id = ch->ch_id + 1;
  }

  ch = calloc(1, sizeof(channel_t));
  RB_INIT(&ch->ch_epg_events);

  channel_set_name(ch, name);

  ch->ch_id = id;
  x = RB_INSERT_SORTED(&channel_identifier_tree, ch, 
		       ch_identifier_link, chidcmp);
  assert(x == NULL);
  return ch;
}

/**
 *
 */
channel_t *
channel_find_by_name(const char *name, int create)
{
  channel_t skel, *ch;

  lock_assert(&global_lock);

  skel.ch_name = (char *)name;
  ch = RB_FIND(&channel_name_tree, &skel, ch_name_link, channelcmp);
  if(ch != NULL || create == 0)
    return ch;
  return channel_create(name);
}


/**
 *
 */
channel_t *
channel_find_by_identifier(int id)
{
  channel_t skel, *ch;

  lock_assert(&global_lock);

  skel.ch_id = id;
  ch = RB_FIND(&channel_identifier_tree, &skel, ch_identifier_link, chidcmp);
  return ch;
}




static struct strtab commercial_detect_tab[] = {
  { "none",       COMMERCIAL_DETECT_NONE   },
  { "ttp192",     COMMERCIAL_DETECT_TTP192 },
};


/**
 *
 */
void
channels_load(void)
{

}



/**
 * Write out a config file for a channel
 */
static void
channel_save(channel_t *ch)
{
  htsmsg_t *m = htsmsg_create();

  lock_assert(&global_lock);

  if(ch->ch_icon != NULL)
    htsmsg_add_str(m, "icon", ch->ch_icon);

  htsmsg_add_str(m, "commercial_detect", 
		 val2str(ch->ch_commercial_detection,
			 commercial_detect_tab) ?: "?");
  hts_settings_save(m, "channels/%s", ch->ch_name);
  htsmsg_destroy(m);
}

/**
 * Rename a channel and all tied transports
 */
int
channel_rename(channel_t *ch, const char *newname)
{
  th_transport_t *t;

  lock_assert(&global_lock);

  if(channel_find_by_name(newname, 0))
    return -1;

  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" renamed to \"%s\"",
	 ch->ch_name, newname);

  hts_settings_remove("channels/%s", ch->ch_name);

  RB_REMOVE(&channel_name_tree, ch, ch_name_link);
  channel_set_name(ch, newname);

  LIST_FOREACH(t, &ch->ch_transports, tht_ch_link) {
    free(t->tht_chname);
    t->tht_chname = strdup(newname);
    t->tht_config_change(t);
  }

  channel_save(ch);
  return 0;
}

/**
 * Delete channel
 */
void
channel_delete(channel_t *ch)
{
  th_transport_t *t;
  th_subscription_t *s;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" deleted",
	 ch->ch_name);

  abort();//pvr_destroy_by_channel(ch);

  while((t = LIST_FIRST(&ch->ch_transports)) != NULL) {
    abort();//transport_unmap_channel(t);
    t->tht_config_change(t);
  }

  while((s = LIST_FIRST(&ch->ch_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    s->ths_channel = NULL;
  }

  abort();//epg_destroy_by_channel(ch);

  abort();//autorec_destroy_by_channel(ch);

  hts_settings_remove("channels/%s", ch->ch_name);

  RB_REMOVE(&channel_name_tree, ch, ch_name_link);
  RB_REMOVE(&channel_identifier_tree, ch, ch_identifier_link);

  free(ch->ch_name);
  free(ch->ch_sname);
  free(ch->ch_icon);
  
  free(ch);
}



/**
 * Merge transports from channel 'src' to channel 'dst'
 *
 * Then, destroy the 'src' channel
 */
void
channel_merge(channel_t *dst, channel_t *src)
{
  th_transport_t *t;

  lock_assert(&global_lock);
  
  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" merged into \"%s\"",
	 src->ch_name, dst->ch_name);

  while((t = LIST_FIRST(&src->ch_transports)) != NULL) {
    abort();//transport_unmap_channel(t);

    abort();//transport_map_channel(t, dst);
    t->tht_config_change(t);
  }

  channel_delete(src);
}

/**
 *
 */
void
channel_set_icon(channel_t *ch, const char *icon)
{
  lock_assert(&global_lock);

  free(ch->ch_icon);
  ch->ch_icon = icon ? strdup(icon) : NULL;
  channel_save(ch);
}
