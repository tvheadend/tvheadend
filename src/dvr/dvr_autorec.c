/*
 *  tvheadend, Automatic recordings
 *  Copyright (C) 2010 Andreas Öman
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
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>

#include "tvhead.h"
#include "settings.h"
#include "dvr.h"
#include "notify.h"
#include "dtable.h"
#include "epg.h"

dtable_t *autorec_dt;

TAILQ_HEAD(dvr_autorec_entry_queue, dvr_autorec_entry);

struct dvr_autorec_entry_queue autorec_entries;

static void dvr_autorec_changed(dvr_autorec_entry_t *dae);

/**
 *
 */
static void
dvr_autorec_purge_spawns(dvr_autorec_entry_t *dae)
{
  dvr_entry_t *de;

  while((de = LIST_FIRST(&dae->dae_spawns)) != NULL) {
    LIST_REMOVE(de, de_autorec_link);
    de->de_autorec = NULL;
    dvr_entry_cancel(de);
  }
}


/**
 * return 1 if the event 'e' is matched by the autorec rule 'dae'
 */
static int
autorec_cmp(dvr_autorec_entry_t *dae, event_t *e)
{
  channel_tag_mapping_t *ctm;

  if(dae->dae_enabled == 0 || dae->dae_weekdays == 0)
    return 0;

  if(dae->dae_channel == NULL &&
     dae->dae_channel_tag == NULL &&
     dae->dae_content_type == 0 &&
     dae->dae_title == NULL)
    return 0; // Avoid super wildcard match

  if(dae->dae_channel != NULL &&
     dae->dae_channel != e->e_channel)
    return 0;
  
  if(dae->dae_channel_tag != NULL) {
    LIST_FOREACH(ctm, &dae->dae_channel_tag->ct_ctms, ctm_tag_link)
      if(ctm->ctm_channel == e->e_channel)
	break;
    if(ctm == NULL)
      return 0;
  }


  if(dae->dae_content_type != 0 &&
     dae->dae_content_type != e->e_content_type)
    return 0;
  
  if(dae->dae_title != NULL) {
    if(e->e_title == NULL ||
       regexec(&dae->dae_title_preg, e->e_title, 0, NULL, 0))
    return 0;
  }

  if(dae->dae_approx_time != 0) {
    struct tm a_time;
    struct tm ev_time;
    localtime_r(&e->e_start, &a_time);
    localtime_r(&e->e_start, &ev_time);
    a_time.tm_min = dae->dae_approx_time % 60;
    a_time.tm_hour = dae->dae_approx_time / 60;
    if(abs(mktime(&a_time) - mktime(&ev_time)) > 900)
      return 0;
  }

  if(dae->dae_weekdays != 0x7f) {
    struct tm tm;
    localtime_r(&e->e_start, &tm);
    if(!((1 << ((tm.tm_wday ?: 7) - 1)) & dae->dae_weekdays))
      return 0;
  }
  return 1;
}


/**
 *
 */
dvr_autorec_entry_t *
autorec_entry_find(const char *id, int create)
{
  dvr_autorec_entry_t *dae;
  char buf[20];
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(dae, &autorec_entries, dae_link)
      if(!strcmp(dae->dae_id, id))
	return dae;
  }

  if(create == 0)
    return NULL;

  dae = calloc(1, sizeof(dvr_autorec_entry_t));
  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }
  dae->dae_weekdays = 0x7f;
  dae->dae_pri = DVR_PRIO_NORMAL;

  dae->dae_id = strdup(id);
  TAILQ_INSERT_TAIL(&autorec_entries, dae, dae_link);
  return dae;
}



/**
 *
 */
static void
autorec_entry_destroy(dvr_autorec_entry_t *dae)
{
  dvr_autorec_purge_spawns(dae);

  free(dae->dae_id);

  free(dae->dae_creator);
  free(dae->dae_comment);

  if(dae->dae_title != NULL) {
    free(dae->dae_title);
    regfree(&dae->dae_title_preg);
  }

  if(dae->dae_channel != NULL)
    LIST_REMOVE(dae, dae_channel_link);

  if(dae->dae_channel_tag != NULL)
    LIST_REMOVE(dae, dae_channel_tag_link);

  TAILQ_REMOVE(&autorec_entries, dae, dae_link);
  free(dae);
}

/**
 *
 */
static void
build_weekday_tags(char *buf, size_t buflen, int mask)
{
  int i, p = 0;
  for(i = 0; i < 7; i++) {
    if(mask & (1 << i) && p < buflen - 3) {
      if(p != 0)
	buf[p++] = ',';
      buf[p++] = '1' + i;
    }
  }
  buf[p] = 0;
}

/**
 *
 */
static int
build_weekday_mask(const char *str)
{
  int r = 0;
  for(; *str; str++) 
    if(*str >= '1' && *str <= '7')
      r |= 1 << (*str - '1');
  return r;
}


/**
 *
 */
static htsmsg_t *
autorec_record_build(dvr_autorec_entry_t *dae)
{
  char str[30];
  htsmsg_t *e = htsmsg_create_map();

  htsmsg_add_str(e, "id", dae->dae_id);
  htsmsg_add_u32(e, "enabled",  !!dae->dae_enabled);

  if(dae->dae_creator != NULL)
    htsmsg_add_str(e, "creator", dae->dae_creator);
  if(dae->dae_comment != NULL)
    htsmsg_add_str(e, "comment", dae->dae_comment);

  if(dae->dae_channel != NULL)
    htsmsg_add_str(e, "channel", dae->dae_channel->ch_name);

  if(dae->dae_channel_tag != NULL)
    htsmsg_add_str(e, "tag", dae->dae_channel_tag->ct_name);

  htsmsg_add_u32(e, "contenttype",dae->dae_content_type);

  htsmsg_add_str(e, "title", dae->dae_title ?: "");

  htsmsg_add_u32(e, "approx_time", dae->dae_approx_time);

  build_weekday_tags(str, sizeof(str), dae->dae_weekdays);
  htsmsg_add_str(e, "weekdays", str);

  htsmsg_add_str(e, "pri", dvr_val2pri(dae->dae_pri));

  return e;
}

/**
 *
 */
static htsmsg_t *
autorec_record_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  dvr_autorec_entry_t *dae;

  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    htsmsg_add_msg(r, NULL, autorec_record_build(dae));

  return r;
}

/**
 *
 */
static htsmsg_t *
autorec_record_get(void *opaque, const char *id)
{
  dvr_autorec_entry_t *ae;

  if((ae = autorec_entry_find(id, 0)) == NULL)
    return NULL;
  return autorec_record_build(ae);
}


/**
 *
 */
static htsmsg_t *
autorec_record_create(void *opaque)
{
  return autorec_record_build(autorec_entry_find(NULL, 1));
}


/**
 *
 */
static htsmsg_t *
autorec_record_update(void *opaque, const char *id, htsmsg_t *values, 
		      int maycreate)
{
  dvr_autorec_entry_t *dae;
  const char *s;
  channel_t *ch;
  channel_tag_t *ct;
  uint32_t u32;

  if((dae = autorec_entry_find(id, maycreate)) == NULL)
    return NULL;

  tvh_str_update(&dae->dae_creator, htsmsg_get_str(values, "creator"));
  tvh_str_update(&dae->dae_comment, htsmsg_get_str(values, "comment"));

  if((s = htsmsg_get_str(values, "channel")) != NULL) {
    if(dae->dae_channel != NULL) {
      LIST_REMOVE(dae, dae_channel_link);
      dae->dae_channel = NULL;
    }
    if((ch = channel_find_by_name(s, 0, 0)) != NULL) {
      LIST_INSERT_HEAD(&ch->ch_autorecs, dae, dae_channel_link);
      dae->dae_channel = ch;
    }
  }

  if((s = htsmsg_get_str(values, "title")) != NULL) {
    if(dae->dae_title != NULL) {
      free(dae->dae_title);
      dae->dae_title = NULL;
      regfree(&dae->dae_title_preg);
    }

    if(!regcomp(&dae->dae_title_preg, s,
		REG_ICASE | REG_EXTENDED | REG_NOSUB)) {
      dae->dae_title = strdup(s);
    }
  }

  if((s = htsmsg_get_str(values, "tag")) != NULL) {
    if(dae->dae_channel_tag != NULL) {
      LIST_REMOVE(dae, dae_channel_tag_link);
      dae->dae_channel_tag = NULL;
    }
    if((ct = channel_tag_find_by_name(s, 0)) != NULL) {
      LIST_INSERT_HEAD(&ct->ct_autorecs, dae, dae_channel_tag_link);
      dae->dae_channel_tag = ct;
    }
  }

  dae->dae_content_type = htsmsg_get_u32_or_default(values, "contenttype", 0);

  if((s = htsmsg_get_str(values, "approx_time")) != NULL) {
    if(strchr(s, ':') != NULL) {
      // formatted time string - convert
      dae->dae_approx_time = (atoi(s) * 60) + atoi(s + 3);
    } else if(strlen(s) == 0) {
      dae->dae_approx_time = 0;
    } else {
      dae->dae_approx_time = atoi(s);
    }
  }

  if((s = htsmsg_get_str(values, "weekdays")) != NULL)
    dae->dae_weekdays = build_weekday_mask(s);

  if(!htsmsg_get_u32(values, "enabled", &u32))
    dae->dae_enabled = u32;

  if((s = htsmsg_get_str(values, "pri")) != NULL)
    dae->dae_pri = dvr_pri2val(s);

  dvr_autorec_changed(dae);

  return autorec_record_build(dae);
}


/**
 *
 */
static int
autorec_record_delete(void *opaque, const char *id)
{
  dvr_autorec_entry_t *dae;

  if((dae = autorec_entry_find(id, 0)) == NULL)
    return -1;
  autorec_entry_destroy(dae);
  return 0;
}


/**
 *
 */
static const dtable_class_t autorec_dtc = {
  .dtc_record_get     = autorec_record_get,
  .dtc_record_get_all = autorec_record_get_all,
  .dtc_record_create  = autorec_record_create,
  .dtc_record_update  = autorec_record_update,
  .dtc_record_delete  = autorec_record_delete,
  .dtc_read_access = ACCESS_RECORDER,
  .dtc_write_access = ACCESS_RECORDER,
};

/**
 *
 */
void
dvr_autorec_init(void)
{
  TAILQ_INIT(&autorec_entries);
  autorec_dt = dtable_create(&autorec_dtc, "autorec", NULL);
  dtable_load(autorec_dt);
}


/**
 *
 */
void
dvr_autorec_add(const char *title, const char *channel,
		const char *tag, uint8_t content_type,
		const char *creator, const char *comment)
{
  dvr_autorec_entry_t *dae;
  htsmsg_t *m;
  channel_t *ch;
  channel_tag_t *ct;

  if((dae = autorec_entry_find(NULL, 1)) == NULL)
    return;

  tvh_str_set(&dae->dae_creator, creator);
  tvh_str_set(&dae->dae_comment, comment);

  if(channel != NULL &&  (ch = channel_find_by_name(channel, 0, 0)) != NULL) {
    LIST_INSERT_HEAD(&ch->ch_autorecs, dae, dae_channel_link);
    dae->dae_channel = ch;
  }

  if(title != NULL &&
     !regcomp(&dae->dae_title_preg, title,
	      REG_ICASE | REG_EXTENDED | REG_NOSUB)) {
    dae->dae_title = strdup(title);
  }

  if(tag != NULL && (ct = channel_tag_find_by_name(tag, 0)) != NULL) {
    LIST_INSERT_HEAD(&ct->ct_autorecs, dae, dae_channel_tag_link);
    dae->dae_channel_tag = ct;
  }

  dae->dae_enabled = 1;
  dae->dae_content_type = content_type;

  m = autorec_record_build(dae);
  hts_settings_save(m, "%s/%s", "autorec", dae->dae_id);
  htsmsg_destroy(m);

  /* Notify web clients that we have messed with the tables */
  
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("autorec", m);

  dvr_autorec_changed(dae);
}


/**
 *
 */
void
dvr_autorec_check_event(event_t *e)
{
  dvr_autorec_entry_t *dae;

  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    if(autorec_cmp(dae, e))
      dvr_entry_create_by_autorec(e, dae);
}

/**
 *
 */
static void
dvr_autorec_changed(dvr_autorec_entry_t *dae)
{
  channel_t *ch;
  event_t *e;

  dvr_autorec_purge_spawns(dae);

  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    RB_FOREACH(e, &ch->ch_epg_events, e_channel_link) {
      if(autorec_cmp(dae, e))
	dvr_entry_create_by_autorec(e, dae);
    }
  }
}


/**
 *
 */
void
autorec_destroy_by_channel(channel_t *ch)
{
  dvr_autorec_entry_t *dae;
  htsmsg_t *m;

  while((dae = LIST_FIRST(&ch->ch_autorecs)) != NULL) {
    dtable_record_erase(autorec_dt, dae->dae_id);
    autorec_entry_destroy(dae);
  }

  /* Notify web clients that we have messed with the tables */
  m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("autorec", m);
}
