/*
 *  tvheadend, access control
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

#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <regex.h>

#include "tvhead.h"
#include "dvr.h"
#include "dtable.h"
#include "epg.h"

TAILQ_HEAD(dvr_autorec_entry_queue, dvr_autorec_entry);

struct dvr_autorec_entry_queue autorec_entries;

typedef struct dvr_autorec_entry {
  TAILQ_ENTRY(dvr_autorec_entry) dae_link;
  char *dae_id;

  int dae_enabled;
  char *dae_creator;

  char *dae_title;
  regex_t dae_title_preg;
  
  epg_content_group_t *dae_ecg;

  channel_t *dae_channel;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_link;

  channel_tag_t *dae_channel_tag;
  LIST_ENTRY(dvr_autorec_entry) dae_channel_tag_link;

} dvr_autorec_entry_t;



#if 0
/**
 * return 1 if the event 'e' is matched by the autorec rule 'ar'
 */
static int
autorec_cmp(dvr_autorec_entry_t *dae, event_t *e)
{
  channel_tag_mapping_t *ctm;

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


  if(dae->dae_ecg != NULL) {
    if(e->e_content_type == NULL || 
       dae->dae_ecg != e->e_content_type->ect_group)
      return 0;
  }
  
  if(dae->dae_title != NULL && 
     regexec(&dae->dae_title_preg, e->e_title, 0, NULL, 0))
    return 0;

  return 1;
}
#endif


/**
 *
 */
static dvr_autorec_entry_t *
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
  free(dae->dae_id);

  free(dae->dae_creator);

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
static htsmsg_t *
autorec_record_build(dvr_autorec_entry_t *dae)
{
  htsmsg_t *e = htsmsg_create();

  htsmsg_add_str(e, "id", dae->dae_id);
  htsmsg_add_u32(e, "enabled",  !!dae->dae_enabled);
  htsmsg_add_str(e, "creator", dae->dae_creator ?: "");
  if(dae->dae_channel)
    htsmsg_add_str(e, "channel", 
		   dae->dae_channel ? dae->dae_channel->ch_name : "");
  if(dae->dae_channel_tag)
    htsmsg_add_str(e, "channeltag",
		   dae->dae_channel_tag ? dae->dae_channel_tag->ct_name : "");
  htsmsg_add_str(e, "title", dae->dae_title ?: "");

  return e;
}

/**
 *
 */
static htsmsg_t *
autorec_record_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_array();
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

  if((dae = autorec_entry_find(id, maycreate)) == NULL)
    return NULL;
  
  tvh_str_set(&dae->dae_creator, htsmsg_get_str(values, "creator"));

  if((s = htsmsg_get_str(values, "channel")) != NULL) {
    if(dae->dae_channel != NULL) {
      LIST_REMOVE(dae, dae_channel_link);
      dae->dae_channel = NULL;
    }
    if((ch = channel_find_by_name(s, 0)) != NULL) {
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

  if((s = htsmsg_get_str(values, "channeltag")) != NULL) {
    if(dae->dae_channel_tag != NULL) {
      LIST_REMOVE(dae, dae_channel_tag_link);
      dae->dae_channel_tag = NULL;
    }
    if((ct = channel_tag_find_by_name(s)) != NULL) {
      LIST_INSERT_HEAD(&ct->ct_autorecs, dae, dae_channel_tag_link);
      dae->dae_channel_tag = ct;
    }
  }

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
};

/**
 *
 */
void
dvr_autorec_init(void)
{
  dtable_t *dt;

  TAILQ_INIT(&autorec_entries);
  dt = dtable_create(&autorec_dtc, "autorec", NULL);
  dtable_load(dt);
}
