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
#include "epg.h"
#include "epggrab.h"
#include "channels.h"
#include "dtable.h"
#include "notify.h"
#include "dvr/dvr.h"
#include "htsp_server.h"
#include "imagecache.h"
#include "service_mapper.h"
#include "htsbuf.h"

struct channel_tree channels;

struct channel_tag_queue channel_tags;
static dtable_t *channeltags_dtable;

static void channel_tag_init ( void );
static channel_tag_t *channel_tag_find(const char *id, int create);
static void channel_tag_mapping_destroy(channel_tag_mapping_t *ctm, 
					int flags);

#define CTM_DESTROY_UPDATE_TAG     0x1
#define CTM_DESTROY_UPDATE_CHANNEL 0x2

static int
ch_id_cmp ( channel_t *a, channel_t *b )
{
  return channel_get_id(a) - channel_get_id(b);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
channel_class_save ( idnode_t *self )
{
  channel_save((channel_t*)self);
}

static void
channel_class_delete ( idnode_t *self )
{
  channel_delete((channel_t*)self);
}

static const void *
channel_class_services_get ( void *obj )
{
  htsmsg_t *l = htsmsg_create_list();
  channel_t *ch = obj;
  channel_service_mapping_t *csm;

  /* Add all */
  LIST_FOREACH(csm, &ch->ch_services, csm_svc_link)
    htsmsg_add_str(l, NULL, idnode_uuid_as_str(&csm->csm_svc->s_id));

  return l;
}

static char *
channel_class_services_rend ( void *obj )
{
  char *str;
  htsmsg_t   *l = htsmsg_create_list();
  channel_t *ch = obj;
  channel_service_mapping_t  *csm;

  LIST_FOREACH(csm, &ch->ch_services, csm_svc_link)
    htsmsg_add_str(l, NULL, idnode_get_title(&csm->csm_svc->s_id) ?: "");

  str = htsmsg_list_2_csv(l);
  htsmsg_destroy(l);
  return str;
}

static int
channel_class_services_set ( void *obj, const void *p )
{
  return channel_set_services_by_list(obj, (htsmsg_t*)p);
}

static htsmsg_t *
channel_class_services_enum ( void *obj )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "service/list");
  htsmsg_add_str(m, "event", "service");
  e = htsmsg_create_map();
  htsmsg_add_bool(e, "enum", 1);
  htsmsg_add_msg(m, "params", e);
  return m;
}

static const void *
channel_class_tags_get ( void *obj )
{
  channel_tag_mapping_t *ctm;
  channel_t *ch = obj;
  htsmsg_t *m = htsmsg_create_list();

  /* Add all */
  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    htsmsg_add_u32(m, NULL, ctm->ctm_tag->ct_identifier);

  return m;
}

static char *
channel_class_tags_rend ( void *obj )
{
  char *str;
  htsmsg_t   *l = htsmsg_create_list();
  channel_t *ch = obj;
  channel_tag_mapping_t *ctm;

  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    htsmsg_add_str(l, NULL, ctm->ctm_tag->ct_name);

  str = htsmsg_list_2_csv(l);
  htsmsg_destroy(l);
  return str;
}

static int
channel_class_tags_set ( void *obj, const void *p )
{
  return channel_set_tags_by_list(obj, (htsmsg_t*)p);
}

static htsmsg_t *
channel_class_tags_enum ( void *obj )
{
  htsmsg_t *e, *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "channeltag/list");
  htsmsg_add_str(m, "event", "channeltag");
  e = htsmsg_create_map();
  htsmsg_add_bool(e, "enum", 1);
  htsmsg_add_msg(m, "params", e);
  return m;
}
  
static void
channel_class_icon_notify ( void *obj )
{
  channel_t *ch = obj;
  if (ch->ch_icon)
    imagecache_get_id(ch->ch_icon);
}

static const char *
channel_class_get_title ( idnode_t *self )
{
  channel_t *ch = (channel_t*)self;
  return ch->ch_name;
}

const idclass_t channel_class = {
  .ic_class      = "service",
  .ic_caption    = "Service",
  .ic_save       = channel_class_save,
  .ic_get_title  = channel_class_get_title,
  .ic_delete     = channel_class_delete,
  .ic_properties = (const property_t[]){
#if 0
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(channel_t, ch_enabled),
    },
#endif
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Name",
      .off      = offsetof(channel_t, ch_name),
    },
    {
      .type     = PT_INT,
      .id       = "number",
      .name     = "Number",
      .off      = offsetof(channel_t, ch_number),
    },
    {
      .type     = PT_STR,
      .id       = "icon",
      .name     = "Icon",
      .off      = offsetof(channel_t, ch_icon),
      .notify   = channel_class_icon_notify,
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pre_time",
      .name     = "DVR Pre", // TODO: better text?
      .off      = offsetof(channel_t, ch_dvr_extra_time_pre),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "dvr_pst_time",
      .name     = "DVR Post", // TODO: better text?
      .off      = offsetof(channel_t, ch_dvr_extra_time_post),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "services",
      .name     = "Services",
      .get      = channel_class_services_get,
      .set      = channel_class_services_set,
      .list     = channel_class_services_enum,
      .rend     = channel_class_services_rend,
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "tags",
      .name     = "Tags",
      .get      = channel_class_tags_get,
      .set      = channel_class_tags_set,
      .list     = channel_class_tags_enum,
      .rend     = channel_class_tags_rend
    },
    {}
  }
};

/* **************************************************************************
 * Find
 * *************************************************************************/

// Note: since channel names are no longer unique this method will simply
//       return the first entry encountered, so could be somewhat random
channel_t *
channel_find_by_name ( const char *name )
{
  channel_t *ch;
  CHANNEL_FOREACH(ch)
    if (!strcmp(ch->ch_name ?: "", name))
      break;
  return ch;
}

channel_t *
channel_find_by_id ( uint32_t i )
{
  channel_t skel;
  memcpy(skel.ch_id.in_uuid, &i, sizeof(i));

  return RB_FIND(&channels, &skel, ch_link, ch_id_cmp);
}

/* **************************************************************************
 * Property updating
 * *************************************************************************/

int
channel_set_services_by_list ( channel_t *ch, htsmsg_t *svcs )
{
  int save = 0;
  const char *str;
  service_t *svc;
  htsmsg_field_t *f;
  channel_service_mapping_t *csm, *n;

  /* Mark all for deletion */
  LIST_FOREACH(csm, &ch->ch_services, csm_chn_link)
    csm->csm_mark = 1;

  /* Link */
  HTSMSG_FOREACH(f, svcs) {
    if ((str = htsmsg_field_get_str(f)))
      if ((svc = service_find(str)))
        save |= service_mapper_link(svc, ch);
  }

  /* Remove */
  for (csm = LIST_FIRST(&ch->ch_services); csm != NULL; csm = n) {
    n = LIST_NEXT(csm, csm_chn_link);
    if (csm->csm_mark) {
      LIST_REMOVE(csm, csm_chn_link);
      LIST_REMOVE(csm, csm_svc_link);
      free(csm);
      save = 1;
    }
  }

  return save;
}

int
channel_set_tags_by_list ( channel_t *ch, htsmsg_t *tags )
{
  int save = 0;
  uint32_t u32;
  channel_tag_mapping_t *ctm, *n;
  channel_tag_t *ct;
  htsmsg_field_t *f;
  
  /* Mark for deletion */
  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    ctm->ctm_mark = 1;
  
  /* Link */
  HTSMSG_FOREACH(f, tags)
    if (!htsmsg_field_get_u32(f, &u32)) {
      if ((ct = channel_tag_find_by_identifier(u32)))
        save |= channel_tag_map(ch, ct);
    }
    
  /* Remove */
  for (ctm = LIST_FIRST(&ch->ch_ctms); ctm != NULL; ctm = n) {
    n = LIST_NEXT(ctm, ctm_channel_link);
    if (ctm->ctm_mark) {
      LIST_REMOVE(ctm, ctm_channel_link);
      LIST_REMOVE(ctm, ctm_tag_link);
      free(ctm);
      save = 1;
    }
  }

  return save;
}

/* **************************************************************************
 * Creation/Deletion
 * *************************************************************************/

channel_t *
channel_create0
  ( channel_t *ch, const idclass_t *idc, const char *uuid, htsmsg_t *conf,
    const char *name )
{
  lock_assert(&global_lock);

  idnode_insert(&ch->ch_id, uuid, idc);
  if (RB_INSERT_SORTED(&channels, ch, ch_link, ch_id_cmp)) {
    tvherror("channel", "id collision!");
    abort();
  }

  if (conf)
    idnode_load(&ch->ch_id, conf);

  /* Override the name */
  if (name) {
    free(ch->ch_name);
    ch->ch_name = strdup(name);
  }

  return ch;
}

void
channel_delete ( channel_t *ch )
{
  th_subscription_t *s;
  channel_tag_mapping_t *ctm;
  channel_service_mapping_t *csm;

  lock_assert(&global_lock);

  tvhinfo("channel", "%s - deleting", ch->ch_name);

  /* Tags */
  while((ctm = LIST_FIRST(&ch->ch_ctms)) != NULL)
    channel_tag_mapping_destroy(ctm, CTM_DESTROY_UPDATE_TAG);

  /* DVR */
  autorec_destroy_by_channel(ch);
  dvr_destroy_by_channel(ch);

  /* Services */
  while((csm = LIST_FIRST(&ch->ch_services)) != NULL)
    service_mapper_unlink(csm->csm_svc, ch);

  /* Subscriptions */
  while((s = LIST_FIRST(&ch->ch_subscriptions)) != NULL) {
    LIST_REMOVE(s, ths_channel_link);
    s->ths_channel = NULL;
  }

  /* EPG */
  epggrab_channel_rem(ch);
  epg_channel_unlink(ch);

  /* Settings */
  hts_settings_remove("channel/%s", idnode_uuid_as_str(&ch->ch_id));

  /* Free memory */
  RB_REMOVE(&channels, ch, ch_link);
  idnode_unlink(&ch->ch_id);
  free(ch->ch_name);
  free(ch->ch_icon);
  free(ch);
}

/*
 * Save
 */
void
channel_save ( channel_t *ch )
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&ch->ch_id, c);
  hts_settings_save(c, "channel/%s", idnode_uuid_as_str(&ch->ch_id));
  htsmsg_destroy(c);
}

/**
 *
 */
void
channel_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  RB_INIT(&channels);
  
  /* Tags */
  channel_tag_init();

  /* Channels */
  if (!(c = hts_settings_load_r(1, "channel")))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    (void)channel_create(f->hmf_name, e, NULL);
  }
  htsmsg_destroy(c);
}

/* ***
 * Channel tags TODO
 */

/**
 *
 */
int
channel_tag_map(channel_t *ch, channel_tag_t *ct)
{
  channel_tag_mapping_t *ctm;

  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link)
    if(ctm->ctm_tag == ct)
      break;
  if (!ctm)
    LIST_FOREACH(ctm, &ct->ct_ctms, ctm_tag_link)
      if(ctm->ctm_channel == ch)
        break;

  if (ctm) {
    ctm->ctm_mark = 0;
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
  return 1;
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

  TAILQ_FOREACH(ct, &channel_tags, ct_link) {
    if(ct->ct_identifier == id)
      return ct;
  }

  return NULL;
}

static void
channel_tag_init ( void )
{
  TAILQ_INIT(&channel_tags);
  channeltags_dtable = dtable_create(&channel_tags_dtc, "channeltags", NULL);
  dtable_load(channeltags_dtable);
}
