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

#include "settings.h"

#include "tvheadend.h"
#include "psi.h"
#include "epg.h"
#include "channels.h"
#include "dtable.h"
#include "notify.h"
#include "dvr/dvr.h"
#include "htsp.h"

//struct channel_list channels_not_xmltv_mapped;

struct channel_tree channel_name_tree;
static struct channel_tree channel_identifier_tree;
struct channel_tag_queue channel_tags;
static dtable_t *channeltags_dtable;

static channel_tag_t *channel_tag_find(const char *id, int create);
static void channel_tag_mapping_destroy(channel_tag_mapping_t *ctm, 
					int flags);

#define CTM_DESTROY_UPDATE_TAG     0x1
#define CTM_DESTROY_UPDATE_CHANNEL 0x2


/**
 *
 */
static void
channel_list_changed(void)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg("channels", m);
}


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

  n2 = strdup(name);

  for(i = 0; i < strlen(n2); i++) {
    c = tolower(n2[i]);
    if(isalnum(c))
      *cp++ = c;
    else
      *cp++ = '_';
  }
  *cp = 0;

  free((void *)n2);

  x = RB_INSERT_SORTED(&channel_name_tree, ch, ch_name_link, channelcmp);
  assert(x == NULL);

  /* Notify clients */
  channel_list_changed();

}


/**
 *
 */
static channel_t *
channel_create(const char *name, int number)
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
  channel_set_name(ch, name);
  ch->ch_number = number;

  ch->ch_id = id;
  x = RB_INSERT_SORTED(&channel_identifier_tree, ch, 
		       ch_identifier_link, chidcmp);

  assert(x == NULL);

  epg_add_channel(ch);

  htsp_channel_add(ch);
  return ch;
}

/**
 *
 */
channel_t *
channel_find_by_name(const char *name, int create, int channel_number)
{
  channel_t skel, *ch;

  lock_assert(&global_lock);

  skel.ch_name = (char *)name;
  ch = RB_FIND(&channel_name_tree, &skel, ch_name_link, channelcmp);
  if(ch != NULL || create == 0)
    return ch;
  return channel_create(name, channel_number);
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

/**
 *
 */
static void
channel_load_one(htsmsg_t *c, int id)
{
  channel_t *ch;
  const char *name = htsmsg_get_str(c, "name");
  htsmsg_t *tags;
  htsmsg_field_t *f;
  channel_tag_t *ct;
  char buf[32];

  if(name == NULL)
    return;

  ch = calloc(1, sizeof(channel_t));
  ch->ch_id = id;
  if(RB_INSERT_SORTED(&channel_identifier_tree, ch, 
		      ch_identifier_link, chidcmp)) {
    /* ID collision, should not happen unless there is something
       wrong in the setting storage */
    free(ch);
    return;
  }

  channel_set_name(ch, name);

  epg_add_channel(ch);

  tvh_str_update(&ch->ch_icon, htsmsg_get_str(c, "icon"));

  htsmsg_get_s32(c, "dvr_extra_time_pre",  &ch->ch_dvr_extra_time_pre);
  htsmsg_get_s32(c, "dvr_extra_time_post", &ch->ch_dvr_extra_time_post);
  htsmsg_get_s32(c, "channel_number", &ch->ch_number);

  if((tags = htsmsg_get_list(c, "tags")) != NULL) {
    HTSMSG_FOREACH(f, tags) {
      if(f->hmf_type == HMF_S64) {
	snprintf(buf, sizeof(buf), "%" PRId64 , f->hmf_s64);

	if((ct = channel_tag_find(buf, 0)) != NULL) 
	  channel_tag_map(ch, ct, 1);
      }
    }
  }
}


/**
 *
 */
static void
channels_load(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load("channels")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;
      channel_load_one(c, atoi(f->hmf_name));
    }
    htsmsg_destroy(l);
  }
}


/**
 * Write out a config file for a channel
 */
void
channel_save(channel_t *ch)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *tags;
  channel_tag_mapping_t *ctm;

  lock_assert(&global_lock);

  htsmsg_add_str(m, "name", ch->ch_name);

  if(ch->ch_icon != NULL)
    htsmsg_add_str(m, "icon", ch->ch_icon);

  tags = htsmsg_create_list();
  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    htsmsg_add_u32(tags, NULL, ctm->ctm_tag->ct_identifier);

  htsmsg_add_msg(m, "tags", tags);

  htsmsg_add_u32(m, "dvr_extra_time_pre",  ch->ch_dvr_extra_time_pre);
  htsmsg_add_u32(m, "dvr_extra_time_post", ch->ch_dvr_extra_time_post);
  htsmsg_add_s32(m, "channel_number", ch->ch_number);

  hts_settings_save(m, "channels/%d", ch->ch_id);
  htsmsg_destroy(m);
}

/**
 * Rename a channel and all tied services
 */
int
channel_rename(channel_t *ch, const char *newname)
{
  service_t *t;

  lock_assert(&global_lock);

  if(channel_find_by_name(newname, 0, 0))
    return -1;

  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" renamed to \"%s\"",
	 ch->ch_name, newname);

  RB_REMOVE(&channel_name_tree, ch, ch_name_link);
  channel_set_name(ch, newname);

  LIST_FOREACH(t, &ch->ch_services, s_ch_link)
    t->s_config_save(t);

  channel_save(ch);
  htsp_channel_update(ch);
  return 0;
}

/**
 * Delete channel
 */
void
channel_delete(channel_t *ch)
{
  service_t *t;
  th_subscription_t *s;
  channel_tag_mapping_t *ctm;

  lock_assert(&global_lock);

  while((ctm = LIST_FIRST(&ch->ch_ctms)) != NULL)
    channel_tag_mapping_destroy(ctm, CTM_DESTROY_UPDATE_TAG);

  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" deleted",
	 ch->ch_name);

  autorec_destroy_by_channel(ch);

  dvr_destroy_by_channel(ch);

  while((t = LIST_FIRST(&ch->ch_services)) != NULL)
    service_map_channel(t, NULL, 1);

  while((s = LIST_FIRST(&ch->ch_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    s->ths_channel = NULL;
  }

  epg_rem_channel(ch);

  hts_settings_remove("channels/%d", ch->ch_id);

  htsp_channel_delete(ch);

  RB_REMOVE(&channel_name_tree, ch, ch_name_link);
  RB_REMOVE(&channel_identifier_tree, ch, ch_identifier_link);

  free(ch->ch_name);
  free(ch->ch_sname);
  free(ch->ch_icon);

  channel_list_changed();
  
  free(ch);
}



/**
 * Merge services from channel 'src' to channel 'dst'
 *
 * Then, destroy the 'src' channel
 */
void
channel_merge(channel_t *dst, channel_t *src)
{
  service_t *t;

  lock_assert(&global_lock);
  
  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" merged into \"%s\"",
	 src->ch_name, dst->ch_name);

  while((t = LIST_FIRST(&src->ch_services)) != NULL)
    service_map_channel(t, dst, 1);

  channel_delete(src);
}

/**
 *
 */
void
channel_set_icon(channel_t *ch, const char *icon)
{
  lock_assert(&global_lock);

  if(ch->ch_icon != NULL && !strcmp(ch->ch_icon, icon))
    return;

  free(ch->ch_icon);
  ch->ch_icon = strdup(icon);
  channel_save(ch);
  htsp_channel_update(ch);
}

/**
 *  Set the amount of minutes to start before / end after recording on a channel
 */
void
channel_set_epg_postpre_time(channel_t *ch, int pre, int mins)
{
  if (mins < -10000 || mins > 10000)
    mins = 0;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "channels", 
	 "Channel \"%s\" epg %s-time set to %d minutes",
	 ch->ch_name, pre ? "pre":"post", mins);

  if (pre) {
    if (ch->ch_dvr_extra_time_pre == mins)
      return;
    else
      ch->ch_dvr_extra_time_pre = mins;
  } else {
    if (ch->ch_dvr_extra_time_post == mins)
      return;
    else
       ch->ch_dvr_extra_time_post = mins;
  }
  channel_save(ch);
  htsp_channel_update(ch);
}

/**
 * Set the channel number
 */
void
channel_set_number(channel_t *ch, int number)
{
  if(ch->ch_number == number)
    return;
  ch->ch_number = number;
  channel_save(ch);
  htsp_channel_update(ch);
}

/**
 *
 */
void
channel_set_epg_source(channel_t *ch, epg_channel_t *ec)
{
  lock_assert(&global_lock);

  if(ec == ch->ch_epg_channel)
    return;

  ch->ch_epg_channel = ec;
  htsp_channel_update(ch); // Not sure?
}

/**
 *
 */
void
channel_set_tags_from_list(channel_t *ch, const char *maplist)
{
  channel_tag_mapping_t *ctm, *n;
  channel_tag_t *ct;
  char buf[40];
  int i, change = 0;

  lock_assert(&global_lock);

  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    ctm->ctm_mark = 1; /* Mark for delete */

  while(*maplist) {
    for(i = 0; i < sizeof(buf) - 1; i++) {
      buf[i] = *maplist;
      if(buf[i] == 0)
	break;

      maplist++;
      if(buf[i] == ',') {
	break;
      }
    }

    buf[i] = 0;
    if((ct = channel_tag_find(buf, 0)) == NULL)
      continue;

    LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
      if(ctm->ctm_tag == ct) {
	ctm->ctm_mark = 0;
	break;
      }
    
    if(ctm == NULL) {
      /* Need to create mapping */
      change = 1;
      channel_tag_map(ch, ct, 0);
    }
  }

  for(ctm = LIST_FIRST(&ch->ch_ctms); ctm != NULL; ctm = n) {
    n = LIST_NEXT(ctm, ctm_channel_link);
    if(ctm->ctm_mark) {
      change = 1;
      channel_tag_mapping_destroy(ctm, CTM_DESTROY_UPDATE_TAG |
				  CTM_DESTROY_UPDATE_CHANNEL);
    }
  }

  if(change)
    channel_save(ch);
}




/**
 *
 */
int
channel_tag_map(channel_t *ch, channel_tag_t *ct, int check)
{
  channel_tag_mapping_t *ctm;

  if(check) {
    LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
      if(ctm->ctm_tag == ct)
	return 0;

    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      if(ctm->ctm_channel == ch)
	return 0;
  }

  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    assert(ctm->ctm_tag != ct);

  LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
    assert(ctm->ctm_channel != ch);

  ctm = malloc(sizeof(channel_tag_mapping_t));

  ctm->ctm_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_ctms, ctm, ctm_channel_link);

  ctm->ctm_tag = ct;
  LIST_INSERT_HEAD(&ct->ct_ctms, ctm, ctm_tag_link);

  ctm->ctm_mark = 0;

  if(ct->ct_enabled && !ct->ct_internal) {
    htsp_tag_update(ct);
    htsp_channel_update(ch);
  }
  return 0;
}


/**
 *
 */
static void
channel_tag_mapping_destroy(channel_tag_mapping_t *ctm, int flags)
{
  channel_tag_t *ct = ctm->ctm_tag;
  channel_t *ch = ctm->ctm_channel;

  LIST_REMOVE(ctm, ctm_channel_link);
  LIST_REMOVE(ctm, ctm_tag_link);
  free(ctm);

  if(ct->ct_enabled && !ct->ct_internal) {
    if(flags & CTM_DESTROY_UPDATE_TAG)
      htsp_tag_update(ct);
    if(flags & CTM_DESTROY_UPDATE_CHANNEL)
      htsp_channel_update(ch);
  }
}


/**
 *
 */
static channel_tag_t *
channel_tag_find(const char *id, int create)
{
  channel_tag_t *ct;
  char buf[20];
  static int tally;
  uint32_t u32;

  if(id != NULL) {
    u32 = atoi(id);
    TAILQ_FOREACH(ct, &channel_tags, ct_link)
      if(ct->ct_identifier == u32)
	return ct;
  }

  if(create == 0)
    return NULL;

  ct = calloc(1, sizeof(channel_tag_t));
  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }

  ct->ct_identifier = atoi(id);
  ct->ct_name = strdup("New tag");
  ct->ct_comment = strdup("");
  ct->ct_icon = strdup("");
  TAILQ_INSERT_TAIL(&channel_tags, ct, ct_link);
  return ct;
}

/**
 *
 */
static void
channel_tag_destroy(channel_tag_t *ct)
{
  channel_tag_mapping_t *ctm;
  channel_t *ch;

  while((ctm = LIST_FIRST(&ct->ct_ctms)) != NULL) {
    ch = ctm->ctm_channel;
    channel_tag_mapping_destroy(ctm, CTM_DESTROY_UPDATE_CHANNEL);
    channel_save(ch);
  }

  if(ct->ct_enabled && !ct->ct_internal)
    htsp_tag_delete(ct);

  free(ct->ct_name);
  free(ct->ct_comment);
  free(ct->ct_icon);
  TAILQ_REMOVE(&channel_tags, ct, ct_link);
  free(ct);
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_build(channel_tag_t *ct)
{
  htsmsg_t *e = htsmsg_create_map();
  htsmsg_add_u32(e, "enabled",  !!ct->ct_enabled);
  htsmsg_add_u32(e, "internal",  !!ct->ct_internal);
  htsmsg_add_u32(e, "titledIcon",  !!ct->ct_titled_icon);

  htsmsg_add_str(e, "name", ct->ct_name);
  htsmsg_add_str(e, "comment", ct->ct_comment);
  htsmsg_add_str(e, "icon", ct->ct_icon);
  htsmsg_add_u32(e, "id", ct->ct_identifier);
  return e;
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  channel_tag_t *ct;

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    htsmsg_add_msg(r, NULL, channel_tag_record_build(ct));

  return r;
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_get(void *opaque, const char *id)
{
  channel_tag_t *ct;

  if((ct = channel_tag_find(id, 0)) == NULL)
    return NULL;
  return channel_tag_record_build(ct);
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_create(void *opaque)
{
  return channel_tag_record_build(channel_tag_find(NULL, 1));
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_update(void *opaque, const char *id, htsmsg_t *values, 
			  int maycreate)
{
  channel_tag_t *ct;
  uint32_t u32;
  int was_exposed, is_exposed;
  channel_tag_mapping_t *ctm;

  if((ct = channel_tag_find(id, maycreate)) == NULL)
    return NULL;

  tvh_str_update(&ct->ct_name,    htsmsg_get_str(values, "name"));
  tvh_str_update(&ct->ct_comment, htsmsg_get_str(values, "comment"));
  tvh_str_update(&ct->ct_icon,    htsmsg_get_str(values, "icon"));

  if(!htsmsg_get_u32(values, "titledIcon", &u32))
    ct->ct_titled_icon = u32;

  was_exposed = ct->ct_enabled && !ct->ct_internal;

  if(!htsmsg_get_u32(values, "enabled", &u32))
    ct->ct_enabled = u32;

  if(!htsmsg_get_u32(values, "internal", &u32))
    ct->ct_internal = u32;

  is_exposed = ct->ct_enabled && !ct->ct_internal;

  /* We only export tags to HTSP if enabled == true and internal == false,
     thus, it's not as simple as just sending updates here.
     Depending on how the flags transition we add update or delete tags */

  if(was_exposed == 0 && is_exposed == 1) {
    htsp_tag_add(ct);

    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      htsp_channel_update(ctm->ctm_channel);

  } else if(was_exposed == 1 && is_exposed == 1)
    htsp_tag_update(ct);
  else if(was_exposed == 1 && is_exposed == 0) {
    
    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      htsp_channel_update(ctm->ctm_channel);

    htsp_tag_delete(ct);
  }
  return channel_tag_record_build(ct);
}


/**
 *
 */
static int
channel_tag_record_delete(void *opaque, const char *id)
{
  channel_tag_t *ct;

  if((ct = channel_tag_find(id, 0)) == NULL)
    return -1;
  channel_tag_destroy(ct);
  return 0;
}


/**
 *
 */
static const dtable_class_t channel_tags_dtc = {
  .dtc_record_get     = channel_tag_record_get,
  .dtc_record_get_all = channel_tag_record_get_all,
  .dtc_record_create  = channel_tag_record_create,
  .dtc_record_update  = channel_tag_record_update,
  .dtc_record_delete  = channel_tag_record_delete,
  .dtc_read_access = ACCESS_ADMIN,
  .dtc_write_access = ACCESS_ADMIN,
  .dtc_mutex = &global_lock,
};



/**
 *
 */
channel_tag_t *
channel_tag_find_by_name(const char *name, int create)
{
  channel_tag_t *ct;
  char str[50];

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(!strcasecmp(ct->ct_name, name))
      return ct;

  if(!create)
    return NULL;

  ct = channel_tag_find(NULL, 1);
  ct->ct_enabled = 1;
  tvh_str_update(&ct->ct_name, name);

  snprintf(str, sizeof(str), "%d", ct->ct_identifier);
  dtable_record_store(channeltags_dtable, str, channel_tag_record_build(ct));

  dtable_store_changed(channeltags_dtable);
  return ct;
}


/**
 *
 */
channel_tag_t *
channel_tag_find_by_identifier(uint32_t id) {
  channel_tag_t *ct;

  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    if(ct->ct_identifier == id)
      return ct;

  return NULL;
}


/**
 *
 */
void
channels_init(void)
{
  TAILQ_INIT(&channel_tags);

  channeltags_dtable = dtable_create(&channel_tags_dtc, "channeltags", NULL);
  dtable_load(channeltags_dtable);

  channels_load();
}
