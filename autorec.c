/*
 *  Automatic recording
 *  Copyright (C) 2008 Andreas Öman
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
#define _GNU_SOURCE
#include <stdlib.h>

#include <pthread.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "dispatch.h"
#include "autorec.h"
#include "pvr.h"
#include "epg.h"
#include "channels.h"

struct autorec_queue autorecs;
static int ar_id_ceil;

static void autorec_save_entry(autorec_t *ar);
static void autorec_load(void);

void
autorec_init(void)
{
  TAILQ_INIT(&autorecs);
  autorec_load();
}


/**
 * return 1 if the event 'e' is matched by the autorec rule 'ar'
 */
static int
autorec_cmp(autorec_t *ar, event_t *e)
{
  if(ar->ar_channel != NULL && 
     ar->ar_channel != e->e_channel)
    return 0;

  if(ar->ar_channel_group != NULL && 
     ar->ar_channel_group != e->e_channel->ch_group)
    return 0;

  if(ar->ar_ecg != NULL) {
    if(e->e_content_type == NULL || 
       ar->ar_ecg != e->e_content_type->ect_group)
      return 0;
  }
  
  if(ar->ar_title != NULL && 
     regexec(&ar->ar_title_preg, e->e_title, 0, NULL, 0))
    return 0;

  return 1;
}

/**
 *
 */
static void
autorec_tag(autorec_t *ar, event_t *e)
{
  char creator[200];

  snprintf(creator, sizeof(creator), "autorec: %s", ar->ar_name);
  pvr_schedule_by_event(e, creator);
}


/**
 * Check the current event and the next after that on all channels
 */
static void
autorec_check_new_ar(autorec_t *ar)
{
  event_t *e;
  channel_t *ch;

  LIST_FOREACH(ch, &channels, ch_global_link) {
    e = ch->ch_epg_cur_event;
    if(e == NULL)
      continue;
    
    if(autorec_cmp(ar, e))
      autorec_tag(ar, e);

    e = TAILQ_NEXT(e, e_channel_link);
    if(e == NULL)
      continue;

    if(autorec_cmp(ar, e))
      autorec_tag(ar, e);
  }
}


/**
 * For the given event, check if any of the autorec matches
 *
 * Assumes epg_lock() is held
 */
void
autorec_check_new_event(event_t *e)
{
  autorec_t *ar;

  TAILQ_FOREACH(ar, &autorecs, ar_link) {
    if(autorec_cmp(ar, e))
      autorec_tag(ar, e);
  }
}





/**
 * Create a new autorec rule, return -1 on failure, internal func
 */
static autorec_t *
autorec_create0(const char *name, int prio, const char *title,
		epg_content_group_t *ecg, channel_group_t *tcg,
		channel_t *ch, int id, const char *creator)
{
  autorec_t *ar = calloc(1, sizeof(autorec_t));
  
  if(title != NULL) {
    ar->ar_title = strdup(title);
    if(regcomp(&ar->ar_title_preg, title, 
	       REG_ICASE | REG_EXTENDED | REG_NOSUB))
      return NULL;
  }

  ar->ar_name     = strdup(name);
  ar->ar_rec_prio = prio;
  ar->ar_ecg      = ecg;

  ar->ar_channel_group = tcg;
  LIST_INSERT_HEAD(&tcg->tcg_autorecs, ar, ar_channel_group_link);

  ar->ar_channel  = ch;
  LIST_INSERT_HEAD(&ch->ch_autorecs, ar, ar_channel_link);

  ar->ar_creator  = strdup(creator);

  ar->ar_id = id;
  TAILQ_INSERT_TAIL(&autorecs, ar, ar_link);
  autorec_check_new_ar(ar);
  return ar;
}

/**
 * Destroy and remove the given autorec
 */
static void
autorec_destroy(autorec_t *ar)
{
  char buf[400];

  snprintf(buf, sizeof(buf), "%s/autorec/%d", settings_dir, ar->ar_id);
  unlink(buf);

  if(ar->ar_title != NULL) {
    regfree(&ar->ar_title_preg);
    free((void *)ar->ar_title);
  }
  free((void *)ar->ar_creator);
  free((void *)ar->ar_name);

  LIST_REMOVE(ar, ar_channel_group_link);
  LIST_REMOVE(ar, ar_channel_link);

  TAILQ_REMOVE(&autorecs, ar, ar_link);
  free(ar);
}


/**
 * Create a new autorec rule, return -1 on failure
 */
int
autorec_create(const char *name, int prio, const char *title,
	       epg_content_group_t *ecg, channel_group_t *tcg,
	       channel_t *ch, const char *creator)
{
  autorec_t *ar;

  ar_id_ceil++;
  ar = autorec_create0(name, prio, title, ecg, tcg, ch, ar_id_ceil, creator);
  if(ar == NULL)
    return -1;

  autorec_save_entry(ar);
  return 0;
}

/**
 * Based on the given id, delete the autorec entry
 */
void
autorec_delete_by_id(int id)
{
  autorec_t *ar;

  TAILQ_FOREACH(ar, &autorecs, ar_link)
    if(ar->ar_id == id)
      break;
  
  if(ar == NULL)
    return;

  autorec_destroy(ar);
}


/**
 * Store an autorec entry on disk
 */
static void 
autorec_save_entry(autorec_t *ar)
{
  char buf[400];
  FILE *fp;

  snprintf(buf, sizeof(buf), "%s/autorec/%d", settings_dir, ar->ar_id);
  
  if((fp = settings_open_for_write(buf)) == NULL)
    return;
  
  fprintf(fp, "name = %s\n", ar->ar_name);
  fprintf(fp, "rec_prio = %d\n", ar->ar_rec_prio);
  fprintf(fp, "creator = %s\n", ar->ar_creator);
  if(ar->ar_title != NULL)
    fprintf(fp, "event_title = %s\n", ar->ar_title);
    
  if(ar->ar_ecg != NULL)
    fprintf(fp, "event_content_group = %s\n", ar->ar_ecg->ecg_name);

  if(ar->ar_channel_group != NULL)
    fprintf(fp, "channel_group = %s\n", ar->ar_channel_group->tcg_name);

  if(ar->ar_channel != NULL)
    fprintf(fp, "channel = %s\n", ar->ar_channel->ch_name);
  fclose(fp);
}

/**
 * Load all entries from disk
 */
static void
autorec_load(void)
{
  struct config_head cl;
  char buf[400];
  struct dirent *d;
  const char *name, *title, *contentgroup, *chgroup, *channel, *creator;
  DIR *dir;
  int prio, id;

  snprintf(buf, sizeof(buf), "%s/autorec", settings_dir);

  if((dir = opendir(buf)) == NULL)
    return;

  while((d = readdir(dir)) != NULL) {

    if(d->d_name[0] == '.')
      continue;

    snprintf(buf, sizeof(buf), "%s/autorec/%s", settings_dir, d->d_name);

    TAILQ_INIT(&cl);
    config_read_file0(buf, &cl);

    /* Required */
    name         = config_get_str_sub(&cl, "name",                NULL);
    creator      = config_get_str_sub(&cl, "creator",             NULL);
    prio    = atoi(config_get_str_sub(&cl, "rec_prio",          "250"));
    /* Optional */
    title        = config_get_str_sub(&cl, "event_title",         NULL);
    contentgroup = config_get_str_sub(&cl, "event_content_group", NULL);
    chgroup      = config_get_str_sub(&cl, "channel_group",       NULL);
    channel      = config_get_str_sub(&cl, "channel",             NULL);

    if(name != NULL && prio > 0 && creator != NULL) {
      id = atoi(d->d_name);
      
      autorec_create0(name, prio, title, 

		      contentgroup ? 
		      epg_content_group_find_by_name(contentgroup) : NULL,

		      chgroup ? channel_group_find(chgroup, 1) : NULL,

		      channel ? channel_find(channel, 1, NULL) : NULL,
		      id, creator);

      if(id > ar_id_ceil)
	ar_id_ceil = id;

    }
    config_free0(&cl);
  }
  closedir(dir);
}

/**
 *
 */
void
autorec_destroy_by_channel(channel_t *ch)
{
  autorec_t *ar;

  while((ar = LIST_FIRST(&ch->ch_autorecs)) != NULL)
    autorec_destroy(ar);
}

