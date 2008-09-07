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
#include "xmltv.h"
#include "dtable.h"

struct channel_list channels_not_xmltv_mapped;

struct channel_tree channel_name_tree;
static struct channel_tree channel_identifier_tree;
struct channel_tag_queue channel_tags;

static void channel_tag_map(channel_t *ch, channel_tag_t *ct, int check);
static channel_tag_t *channel_tag_find(const char *id, int create);
static void channel_tag_mapping_destroy(channel_tag_mapping_t *ctm);

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
      *cp++ = '_';
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
  LIST_INSERT_HEAD(&channels_not_xmltv_mapped, ch, ch_xc_link);
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
static void
channel_load_one(htsmsg_t *c, int id)
{
  channel_t *ch;
  const char *s;
  const char *name = htsmsg_get_str(c, "name");
  htsmsg_t *tags;
  htsmsg_field_t *f;
  channel_tag_t *ct;

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

  RB_INIT(&ch->ch_epg_events);

  channel_set_name(ch, name);

  if((s = htsmsg_get_str(c, "icon")) != NULL)
    ch->ch_icon = strdup(s);
 
  if((s = htsmsg_get_str(c, "xmltv-channel")) != NULL &&
     (ch->ch_xc = xmltv_channel_find(s, 0)) != NULL)
    LIST_INSERT_HEAD(&ch->ch_xc->xc_channels, ch, ch_xc_link);
  else
    LIST_INSERT_HEAD(&channels_not_xmltv_mapped, ch, ch_xc_link);

  if((tags = htsmsg_get_array(c, "tags")) != NULL) {
    HTSMSG_FOREACH(f, tags) {
      if(f->hmf_type == HMF_STR && 
	 (ct = channel_tag_find(f->hmf_str, 0)) != NULL) {
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
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
	continue;
      channel_load_one(c, atoi(f->hmf_name));
    }
  }
}


/**
 * Write out a config file for a channel
 */
static void
channel_save(channel_t *ch)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_t *tags;
  channel_tag_mapping_t *ctm;

  lock_assert(&global_lock);

  htsmsg_add_str(m, "name", ch->ch_name);

  if(ch->ch_xc != NULL)
    htsmsg_add_str(m, "xmltv-channel", ch->ch_xc->xc_identifier);

  if(ch->ch_icon != NULL)
    htsmsg_add_str(m, "icon", ch->ch_icon);

  htsmsg_add_str(m, "commercial-detect", 
		 val2str(ch->ch_commercial_detection,
			 commercial_detect_tab) ?: "?");
  
  tags = htsmsg_create_array();
  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    htsmsg_add_str(tags, NULL, ctm->ctm_tag->ct_identifier);

  htsmsg_add_msg(m, "tags", tags);

  hts_settings_save(m, "channels/%d", ch->ch_id);
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
  channel_tag_mapping_t *ctm;

  lock_assert(&global_lock);

  while((ctm = LIST_FIRST(&ch->ch_ctms)) != NULL)
    channel_tag_mapping_destroy(ctm);

  tvhlog(LOG_NOTICE, "channels", "Channel \"%s\" deleted",
	 ch->ch_name);

  fprintf(stderr, "!!!!!//pvr_destroy_by_channel(ch);\n");

  while((t = LIST_FIRST(&ch->ch_transports)) != NULL) {
    transport_unmap_channel(t);
    pthread_mutex_lock(&t->tht_stream_mutex);
    t->tht_config_change(t);
    pthread_mutex_unlock(&t->tht_stream_mutex);
  }

  while((s = LIST_FIRST(&ch->ch_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    s->ths_channel = NULL;
  }

  epg_unlink_from_channel(ch);

  fprintf(stderr, "!!!!!//autorec_destroy_by_channel(ch);\n");

  hts_settings_remove("channels/%d", ch->ch_id);

  RB_REMOVE(&channel_name_tree, ch, ch_name_link);
  RB_REMOVE(&channel_identifier_tree, ch, ch_identifier_link);

  LIST_REMOVE(ch, ch_xc_link);

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
    transport_unmap_channel(t);
    transport_map_channel(t, dst);
    pthread_mutex_lock(&t->tht_stream_mutex);
    t->tht_config_change(t);
    pthread_mutex_unlock(&t->tht_stream_mutex);
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

  if(ch->ch_icon != NULL && !strcmp(ch->ch_icon, icon))
    return;

  free(ch->ch_icon);
  ch->ch_icon = strdup(icon);
  channel_save(ch);
}


/**
 *
 */
void
channel_set_xmltv_source(channel_t *ch, xmltv_channel_t *xc)
{
  lock_assert(&global_lock);

  if(xc == ch->ch_xc)
    return;

  LIST_REMOVE(ch, ch_xc_link);

  if(xc == NULL) {
    LIST_INSERT_HEAD(&channels_not_xmltv_mapped, ch, ch_xc_link);
  } else {
    LIST_INSERT_HEAD(&xc->xc_channels, ch, ch_xc_link);
  }
  ch->ch_xc = xc;
  channel_save(ch);
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
      channel_tag_mapping_destroy(ctm);
    }
  }

  if(change)
    channel_save(ch);
}




/**
 *
 */
static void
channel_tag_map(channel_t *ch, channel_tag_t *ct, int check)
{
  channel_tag_mapping_t *ctm;

  if(check) {
    LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
      if(ctm->ctm_tag == ct)
	return;

    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      if(ctm->ctm_channel == ch)
	return;
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
}


/**
 *
 */
static void
channel_tag_mapping_destroy(channel_tag_mapping_t *ctm)
{
  LIST_REMOVE(ctm, ctm_channel_link);
  LIST_REMOVE(ctm, ctm_tag_link);
  free(ctm);
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

  if(id != NULL) {
    TAILQ_FOREACH(ct, &channel_tags, ct_link)
      if(!strcmp(ct->ct_identifier, id))
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

  ct->ct_identifier = strdup(id);
  ct->ct_name = strdup("New tag");
  ct->ct_comment = strdup("");
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
    channel_tag_mapping_destroy(ctm);
    channel_save(ch);
  }

  free(ct->ct_identifier);
  free(ct->ct_name);
  free(ct->ct_comment);
  TAILQ_REMOVE(&channel_tags, ct, ct_link);
  free(ct);
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_build(channel_tag_t *ct)
{
  htsmsg_t *e = htsmsg_create();
  htsmsg_add_u32(e, "enabled",  !!ct->ct_enabled);
  htsmsg_add_u32(e, "internal",  !!ct->ct_internal);

  htsmsg_add_str(e, "name", ct->ct_name);
  htsmsg_add_str(e, "comment", ct->ct_comment);
  htsmsg_add_str(e, "id", ct->ct_identifier);
  return e;
}


/**
 *
 */
static htsmsg_t *
channel_tag_record_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_array();
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

  if((ct = channel_tag_find(id, maycreate)) == NULL)
    return NULL;

  tvh_str_update(&ct->ct_name,    htsmsg_get_str(values, "name"));
  tvh_str_update(&ct->ct_comment, htsmsg_get_str(values, "comment"));

 if(!htsmsg_get_u32(values, "enabled", &u32))
    ct->ct_enabled = u32;

 if(!htsmsg_get_u32(values, "internal", &u32))
    ct->ct_internal = u32;

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
};


/**
 *
 */
void
channels_init(void)
{
  dtable_t *dt;

  TAILQ_INIT(&channel_tags);

  dt = dtable_create(&channel_tags_dtc, "channeltags", NULL);
  dtable_load(dt);

  channels_load();
}
