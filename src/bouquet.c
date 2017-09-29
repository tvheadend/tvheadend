/*
 *  tvheadend, bouquets
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include "tvheadend.h"
#include "settings.h"
#include "access.h"
#include "bouquet.h"
#include "service.h"
#include "channels.h"
#include "service_mapper.h"
#include "download.h"
#include "input.h"

typedef struct bouquet_download {
  bouquet_t  *bq;
  download_t  download;
  mtimer_t    timer;
} bouquet_download_t;

bouquet_tree_t bouquets;

static void bouquet_remove_service(bouquet_t *bq, service_t *s, int delconf);
static void bouquet_download_trigger(bouquet_t *bq);
static void bouquet_download_stop(void *aux);
static int bouquet_download_process(void *aux, const char *last_url, const char *host_url, char *data, size_t len);

/**
 *
 */
static int
_bq_cmp(const void *a, const void *b)
{
  return strcmp(((bouquet_t *)a)->bq_src ?: "", ((bouquet_t *)b)->bq_src ?: "");
}

/**
 *
 */
static void
bouquet_free(bouquet_t *bq)
{
  bouquet_download_t *bqd;

  idnode_save_check(&bq->bq_id, 1);
  idnode_unlink(&bq->bq_id);

  if ((bqd = bq->bq_download) != NULL) {
    bouquet_download_stop(bqd);
    download_done(&bqd->download);
    free(bqd);
  }

  idnode_set_free(bq->bq_active_services);
  idnode_set_free(bq->bq_services);
  htsmsg_destroy(bq->bq_services_waiting);
  free((char *)bq->bq_chtag_waiting);
  free(bq->bq_name);
  free(bq->bq_ext_url);
  free(bq->bq_src);
  free(bq->bq_comment);
  free(bq);
}

/**
 *
 */
bouquet_t *
bouquet_create(const char *uuid, htsmsg_t *conf,
               const char *name, const char *src)
{
  bouquet_t *bq, *bq2;
  bouquet_download_t *bqd;
  char buf[128], ubuf[UUID_HEX_SIZE];
  int i;

  lock_assert(&global_lock);

  bq = calloc(1, sizeof(bouquet_t));
  bq->bq_services = idnode_set_create(1);
  bq->bq_active_services = idnode_set_create(1);
  bq->bq_ext_url_period = 60;
  bq->bq_mapencrypted = 1;
  bq->bq_mapradio = 1;
  bq->bq_maptoch = 1;
  bq->bq_chtag = 1;

  if (idnode_insert(&bq->bq_id, uuid, &bouquet_class, 0)) {
    if (uuid)
      tvherror(LS_BOUQUET, "invalid uuid '%s'", uuid);
    bouquet_free(bq);
    return NULL;
  }

  if (conf) {
    bq->bq_in_load = 1;
    idnode_load(&bq->bq_id, conf);
    bq->bq_in_load = 0;
    if (!htsmsg_get_bool(conf, "shield", &i) && i)
      bq->bq_shield = 1;
  }

  if (name) {
    free(bq->bq_name);
    bq->bq_name = strdup(name);
  }

  if (src) {
    free(bq->bq_src);
    bq->bq_src = strdup(src);
  }

  if (bq->bq_ext_url) {
    if (bq->bq_src && strncmp(bq->bq_src, "exturl://", 9) != 0) {
      free(bq->bq_ext_url);
      bq->bq_ext_url = NULL;
    } else {
      free(bq->bq_src);
      snprintf(buf, sizeof(buf), "exturl://%s", idnode_uuid_as_str(&bq->bq_id, ubuf));
      bq->bq_src = strdup(buf);
      bq->bq_download = bqd = calloc(1, sizeof(*bqd));
      bqd->bq = bq;
      download_init(&bqd->download, LS_BOUQUET);
      bqd->download.process = bouquet_download_process;
      bqd->download.stop = bouquet_download_stop;
      bouquet_change_comment(bq, bq->bq_ext_url, 0);
    }
  }

  bq2 = RB_INSERT_SORTED(&bouquets, bq, bq_link, _bq_cmp);
  if (bq2) {
    tvherror(LS_BOUQUET, "found duplicate source id: '%s', remove duplicate config", bq->bq_src);
    bouquet_free(bq);
    return NULL;
  }

  bq->bq_saveflag = 1;

  if (bq->bq_ext_url)
    bouquet_download_trigger(bq);

  return bq;
}

/**
 *
 */
static void
bouquet_destroy(bouquet_t *bq)
{
  if (!bq)
    return;

  RB_REMOVE(&bouquets, bq, bq_link);

  bouquet_free(bq);
}

/**
 *
 */
void
bouquet_destroy_by_service(service_t *t, int delconf)
{
  bouquet_t *bq;
  service_lcn_t *sl;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)
    if (idnode_set_exists(bq->bq_services, &t->s_id))
      bouquet_remove_service(bq, t, delconf);
  while ((sl = LIST_FIRST(&t->s_lcns)) != NULL) {
    LIST_REMOVE(sl, sl_link);
    free(sl);
  }
}

/**
 *
 */
void
bouquet_destroy_by_channel_tag(channel_tag_t *ct)
{
  bouquet_t *bq;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)
    if (bq->bq_chtag_ptr == ct)
      bq->bq_chtag_ptr = NULL;
}

/*
 *
 */
bouquet_t *
bouquet_find_by_source(const char *name, const char *src, int create)
{
  bouquet_t *bq;
  bouquet_t bqs;

  assert(src);

  lock_assert(&global_lock);

  bqs.bq_src = (char *)src;
  bq = RB_FIND(&bouquets, &bqs, bq_link, _bq_cmp);
  if (bq) {
    if (name && *name && bq->bq_name && strcmp(name, bq->bq_name)) {
      tvhwarn(LS_BOUQUET, "bouquet name '%s' changed to '%s'", bq->bq_name ?: "", name);
      free(bq->bq_name);
      bq->bq_name = strdup(name);
      idnode_changed(&bq->bq_id);
    }
    return bq;
  }
  if (create && name) {
    bq = bouquet_create(NULL, NULL, name, src);
    tvhinfo(LS_BOUQUET, "new bouquet '%s'", name);
    return bq;
  }
  return NULL;
}

/*
 *
 */
static channel_tag_t *
bouquet_tag(bouquet_t *bq, int create)
{
  channel_tag_t *ct;
  char buf[128];

  assert(!bq->bq_in_load);
  if (bq->bq_chtag_waiting) {
    bq->bq_chtag_ptr = channel_tag_find_by_uuid(bq->bq_chtag_waiting);
    free((char *)bq->bq_chtag_waiting);
    bq->bq_chtag_waiting = NULL;
  }
  if (bq->bq_chtag_ptr)
    return bq->bq_chtag_ptr;
  snprintf(buf, sizeof(buf), "*** %s", bq->bq_name ?: "???");
  ct = channel_tag_find_by_name(buf, create);
  if (ct) {
    bq->bq_chtag_ptr = ct;
    idnode_changed(&bq->bq_id);
  }
  return ct;
}

/*
 *
 */
static int
noname(const char *s)
{
  if (!s)
    return 1;
  while (*s) {
    if (*s > ' ')
      return 0;
    s++;
  }
  return 1;
}

/*
 *
 */
static void
bouquet_map_channel(bouquet_t *bq, service_t *t)
{
  channel_t *ch = NULL;
  idnode_list_mapping_t *ilm;
  static service_mapper_conf_t sm_conf = {
    .check_availability = 0,
    .encrypted          = 1,
    .merge_same_name    = 0,
    .type_tags          = 0,
    .provider_tags      = 0,
    .network_tags       = 0
  };

  if (!t->s_enabled)
    return;
  if (!bq->bq_mapradio && service_is_radio(t))
    return;
  if (!bq->bq_mapnolcn &&
      bouquet_get_channel_number(bq, t) <= 0)
    return;
  if (!bq->bq_mapnoname && noname(service_get_channel_name(t)))
    return;
  if (!bq->bq_mapencrypted && service_is_encrypted(t))
    return;
  LIST_FOREACH(ilm, &t->s_channels, ilm_in1_link)
    if (((channel_t *)ilm->ilm_in2)->ch_bouquet == bq)
      break;
  if (!ilm) {
    sm_conf.encrypted = bq->bq_mapencrypted;
    sm_conf.merge_same_name = bq->bq_mapmergename;
    sm_conf.type_tags = bq->bq_chtag_type_tags;
    sm_conf.provider_tags = bq->bq_chtag_provider_tags;
    sm_conf.network_tags = bq->bq_chtag_network_tags;
    ch = service_mapper_process(&sm_conf, t, bq);
  } else
    ch = (channel_t *)ilm->ilm_in2;
  if (ch && bq->bq_chtag)
    if (channel_tag_map(bouquet_tag(bq, 1), ch, ch))
      idnode_changed(&ch->ch_id);
}

/*
 *
 */
void
bouquet_add_service(bouquet_t *bq, service_t *s, uint64_t lcn, const char *tag)
{
  service_lcn_t *tl;
  idnode_list_mapping_t *ilm;

  lock_assert(&global_lock);

  if (!bq->bq_enabled)
    return;

  if (!idnode_set_exists(bq->bq_services, &s->s_id)) {
    tvhtrace(LS_BOUQUET, "add service %s [%p] to %s lcn %"PRIu64"(.%"PRIu64")",
             s->s_nicename, s, bq->bq_name ?: "<unknown>",
             lcn / CHANNEL_SPLIT, lcn % CHANNEL_SPLIT);
    idnode_set_add(bq->bq_services, &s->s_id, NULL, NULL);
    bq->bq_saveflag = 1;
  }

  LIST_FOREACH(tl, &s->s_lcns, sl_link)
    if (tl->sl_bouquet == bq)
      break;

  if (!tl) {
    tl = calloc(1, sizeof(*tl));
    tl->sl_bouquet = bq;
    LIST_INSERT_HEAD(&s->s_lcns, tl, sl_link);
    bq->bq_saveflag = 1;
  } else {
    if (tl->sl_lcn != lcn)
      bq->bq_saveflag = 1;
  }
  if (lcn != tl->sl_lcn) {
    tl->sl_lcn = lcn;
    LIST_FOREACH(ilm, &s->s_channels, ilm_in1_link)
      idnode_notify_changed(ilm->ilm_in2);
  }
  tl->sl_seen = 1;

  if (bq->bq_enabled && bq->bq_maptoch)
    bouquet_map_channel(bq, s);

  if (!bq->bq_in_load &&
      !idnode_set_exists(bq->bq_active_services, &s->s_id))
    idnode_set_add(bq->bq_active_services, &s->s_id, NULL, NULL);
}

/*
 *
 */
static void
bouquet_unmap_channel(bouquet_t *bq, service_t *t)
{
  idnode_list_mapping_t *ilm, *ilm_next;

  ilm = LIST_FIRST(&t->s_channels);
  while (ilm) {
    ilm_next = LIST_NEXT(ilm, ilm_in1_link);
    if (((channel_t *)ilm->ilm_in2)->ch_bouquet == bq) {
      tvhinfo(LS_BOUQUET, "%s / %s: unmapped from %s",
              channel_get_name((channel_t *)ilm->ilm_in2, channel_blank_name),
              t->s_nicename, bq->bq_name ?: "<unknown>");
      channel_delete((channel_t *)ilm->ilm_in2, 1);
    }
    ilm = ilm_next;
  }
}

/**
 *
 */
void
bouquet_notify_service_enabled(service_t *t)
{
  bouquet_t *bq;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)
    if (idnode_set_exists(bq->bq_services, &t->s_id)) {
      if (!t->s_enabled)
        bouquet_unmap_channel(bq, t);
      else if (bq->bq_enabled && bq->bq_maptoch)
        bouquet_map_channel(bq, t);
    }
}

/*
 *
 */
static void
bouquet_remove_service(bouquet_t *bq, service_t *s, int delconf)
{
  tvhtrace(LS_BOUQUET, "remove service %s from %s",
           s->s_nicename, bq->bq_name ?: "<unknown>");
  idnode_set_remove(bq->bq_services, &s->s_id);
  if (delconf)
    bouquet_unmap_channel(bq, s);
}

/*
 *
 */
void
bouquet_completed(bouquet_t *bq, uint32_t seen)
{
  idnode_set_t *remove;
  service_t *s;
  service_lcn_t *lcn, *lcn_next;
  size_t z;

  if (!bq)
    return;

  if (seen != bq->bq_services_seen) {
    bq->bq_services_seen = seen;
    bq->bq_saveflag = 1;
  }

  tvhtrace(LS_BOUQUET, "%s: completed: enabled=%d active=%zi old=%zi seen=%u",
            bq->bq_name ?: "", bq->bq_enabled, bq->bq_active_services->is_count,
            bq->bq_services->is_count, seen);

  if (!bq->bq_enabled)
    goto save;

  /* Add/Remove services */
  remove = idnode_set_create(0);
  for (z = 0; z < bq->bq_services->is_count; z++)
    if (!idnode_set_exists(bq->bq_active_services, bq->bq_services->is_array[z]))
      idnode_set_add(remove, bq->bq_services->is_array[z], NULL, NULL);
  for (z = 0; z < remove->is_count; z++)
    bouquet_remove_service(bq, (service_t *)remove->is_array[z], 1);
  idnode_set_free(remove);

  /* Remove no longer used LCNs */
  for (z = 0; z < bq->bq_services->is_count; z++) {
    s = (service_t *)bq->bq_services->is_array[z];
    for (lcn = LIST_FIRST(&s->s_lcns); lcn; lcn = lcn_next) {
      lcn_next = LIST_NEXT(lcn, sl_link);
      if (lcn->sl_bouquet != bq) continue;
      if (!lcn->sl_seen) {
        LIST_REMOVE(lcn, sl_link);
        free(lcn);
      } else {
        lcn->sl_seen = 0;
      }
    }
  }


  idnode_set_free(bq->bq_active_services);
  bq->bq_active_services = idnode_set_create(1);

save:
  if (bq->bq_saveflag) {
    bq->bq_saveflag = 0;
    idnode_changed(&bq->bq_id);
  }
}

/*
 *
 */
void
bouquet_map_to_channels(bouquet_t *bq)
{
  service_t *t;
  size_t z;

  for (z = 0; z < bq->bq_services->is_count; z++) {
    t = (service_t *)bq->bq_services->is_array[z];
    if (bq->bq_enabled && bq->bq_maptoch) {
      bouquet_map_channel(bq, t);
    } else {
      bouquet_unmap_channel(bq, t);
    }
  }

  if (!bq->bq_enabled) {
    if (bq->bq_services->is_count) {
      idnode_set_free(bq->bq_services);
      bq->bq_services = idnode_set_create(1);
      bq->bq_saveflag = 1;
    }
    if (bq->bq_active_services->is_count) {
      idnode_set_free(bq->bq_active_services);
      bq->bq_active_services = idnode_set_create(1);
    }
  }
}

/*
 *
 */
void
bouquet_notify_channels(bouquet_t *bq)
{
  idnode_list_mapping_t *ilm;
  service_t *t;
  size_t z;

  for (z = 0; z < bq->bq_services->is_count; z++) {
    t = (service_t *)bq->bq_services->is_array[z];
    LIST_FOREACH(ilm, &t->s_channels, ilm_in1_link)
      if (((channel_t *)ilm->ilm_in2)->ch_bouquet == bq)
        idnode_notify_changed(ilm->ilm_in2);
  }
}

/*
 *
 */
uint64_t
bouquet_get_channel_number(bouquet_t *bq, service_t *t)
{
  service_lcn_t *tl;

  LIST_FOREACH(tl, &t->s_lcns, sl_link)
    if (tl->sl_bouquet == bq)
      return (int64_t)tl->sl_lcn;
  return 0;
}

/*
 *
 */
static const char *
bouquet_get_tag_name(bouquet_t *bq, service_t *t)
{
  return 0;
}

/**
 *
 */
void
bouquet_delete(bouquet_t *bq)
{
  char ubuf[UUID_HEX_SIZE];
  if (bq == NULL) return;
  bq->bq_enabled = 0;
  bouquet_map_to_channels(bq);
  if (!bq->bq_shield) {
    hts_settings_remove("bouquet/%s", idnode_uuid_as_str(&bq->bq_id, ubuf));
    bouquet_destroy(bq);
  } else {
    idnode_set_free(bq->bq_services);
    bq->bq_services = idnode_set_create(1);
    idnode_changed(&bq->bq_id);
  }
}

/**
 *
 */
void
bouquet_change_comment ( bouquet_t *bq, const char *comment, int replace )
{
  if (!replace && bq->bq_comment && bq->bq_comment[0])
    return;
  free(bq->bq_comment);
  bq->bq_comment = comment ? strdup(comment) : NULL;
  bq->bq_saveflag = 1;
}

/*
 *
 */
void
bouquet_scan ( bouquet_t *bq )
{
  void mpegts_mux_bouquet_rescan ( const char *src, const char *extra );
  char *mpegts_network_uuid = NULL;
  if (bq->bq_src) {
#if ENABLE_IPTV
    if (strncmp(bq->bq_src, "iptv-network://", 15) == 0)
      mpegts_network_uuid = bq->bq_src + 15;
#endif
    if (strncmp(bq->bq_src, "mpegts-network://", 17) == 0)
      mpegts_network_uuid = bq->bq_src + 17;
    else if (strncmp(bq->bq_src, "exturl://", 9) == 0)
      return bouquet_download_trigger(bq);

    if (mpegts_network_uuid) {
      mpegts_network_t *mn = mpegts_network_find(mpegts_network_uuid);
      if (mn)
        return mpegts_network_bouquet_trigger(mn, 0);
    }
  }
  mpegts_mux_bouquet_rescan(bq->bq_src, bq->bq_comment);
  bq->bq_rescan = 0;
}

/*
 *
 */
void
bouquet_detach ( channel_t *ch )
{
  bouquet_t *bq = ch->ch_bouquet;
  idnode_list_mapping_t *ilm;
  service_lcn_t *tl;
  service_t *t;
  int64_t n = 0;

  if (!bq)
    return;
  LIST_FOREACH(ilm, &ch->ch_services, ilm_in2_link) {
    t = (service_t *)ilm->ilm_in1;
    LIST_FOREACH(tl, &t->s_lcns, sl_link)
      if (tl->sl_bouquet == bq) {
        n = (int64_t)tl->sl_lcn;
        goto found;
      }
  }
found:
  if (n) {
    n += (int64_t)ch->ch_bouquet->bq_lcn_offset * CHANNEL_SPLIT;
    ch->ch_number = n;
  }
  ch->ch_bouquet = NULL;
  idnode_changed(&ch->ch_id);
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static htsmsg_t *
bouquet_class_save(idnode_t *self, char *filename, size_t fsize)
{
  bouquet_t *bq = (bouquet_t *)self;
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&bq->bq_id, c);
  if (filename)
    snprintf(filename, fsize, "bouquet/%s", idnode_uuid_as_str(&bq->bq_id, ubuf));
  if (bq->bq_shield)
    htsmsg_add_bool(c, "shield", 1);
  bq->bq_saveflag = 0;
  return c;
}

static void
bouquet_class_delete(idnode_t *self)
{
  bouquet_delete((bouquet_t *)self);
}

static const char *
bouquet_class_get_title (idnode_t *self, const char *lang)
{
  bouquet_t *bq = (bouquet_t *)self;

  if (bq->bq_comment && bq->bq_comment[0] != '\0')
    return bq->bq_comment;
  return bq->bq_name ?: "";
}

/* exported for others */
htsmsg_t *
bouquet_class_get_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "bouquet/list");
  htsmsg_add_str(m, "event", "bouquet");
  return m;
}

static void
bouquet_class_enabled_notify ( void *obj, const char *lang )
{
  bouquet_t *bq = obj;

  if (bq->bq_enabled)
    bouquet_scan(bq);
  bouquet_map_to_channels(bq);
}

static void
bouquet_class_maptoch_notify ( void *obj, const char *lang )
{
  bouquet_map_to_channels((bouquet_t *)obj);
}

static void
bouquet_class_lcn_offset_notify ( void *obj, const char *lang )
{
  if (((bouquet_t *)obj)->bq_in_load)
    return;
  bouquet_notify_channels((bouquet_t *)obj);
}

static idnode_slist_t bouquest_class_mapopt_slist[] = {
  {
    .id   = "mapnolcn",
    .name = N_("Map zero-numbered channels"),
    .off  = offsetof(bouquet_t, bq_mapnolcn),
  },
  {
    .id   = "mapnoname",
    .name = N_("Map unnamed channels"),
    .off  = offsetof(bouquet_t, bq_mapnoname),
  },
  {
    .id   = "mapradio",
    .name = N_("Map radio channels"),
    .off  = offsetof(bouquet_t, bq_mapradio),
  },
  {
    .id   = "encrypted",
    .name = N_("Map encrypted services"),
    .off  = offsetof(bouquet_t, bq_mapencrypted),
  },
  {
    .id   = "merge_name",
    .name = N_("Merge same name"),
    .off  = offsetof(bouquet_t, bq_mapmergename),
  },
  {}
};

static htsmsg_t *
bouquet_class_mapopt_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, bouquest_class_mapopt_slist, lang);
}

static const void *
bouquet_class_mapopt_get ( void *obj )
{
  return idnode_slist_get(obj, bouquest_class_mapopt_slist);
}

static char *
bouquet_class_mapopt_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, bouquest_class_mapopt_slist, lang);
}

static int
bouquet_class_mapopt_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, bouquest_class_mapopt_slist, p);
}

static void
bouquet_class_mapopt_notify ( void *obj, const char *lang )
{
  bouquet_t *bq = obj;
  service_t *t;
  size_t z;

  if (bq->bq_in_load)
    return;
  if (bq->bq_enabled && bq->bq_maptoch) {
    for (z = 0; z < bq->bq_services->is_count; z++) {
      t = (service_t *)bq->bq_services->is_array[z];
      if (!bq->bq_mapradio && service_is_radio(t))
        bouquet_unmap_channel(bq, t);
      else if (!bq->bq_mapnoname && noname(service_get_channel_name(t)))
        bouquet_unmap_channel(bq, t);
      else if (!bq->bq_mapradio && service_is_radio(t))
        bouquet_unmap_channel(bq, t);
      else if (!bq->bq_mapencrypted && service_is_encrypted(t))
        bouquet_unmap_channel(bq, t);
    }
  }
  bouquet_map_to_channels(bq);
}

static idnode_slist_t bouquest_class_chtag_slist[] = {
  {
    .id   = "bouquet_tag",
    .name = N_("Create bouquet tag"),
    .off  = offsetof(bouquet_t, bq_chtag),
  },
  {
    .id   = "type_tags",
    .name = N_("Create type-based tags"),
    .off  = offsetof(bouquet_t, bq_chtag_type_tags),
  },
  {
    .id   = "providers_tags",
    .name = N_("Create provider name tags"),
    .off  = offsetof(bouquet_t, bq_chtag_provider_tags),
  },
  {
    .id   = "network_tags",
    .name = N_("Create network name tags"),
    .off  = offsetof(bouquet_t, bq_chtag_network_tags),
  },
  {}
};

static htsmsg_t *
bouquet_class_chtag_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, bouquest_class_chtag_slist, lang);
}

static const void *
bouquet_class_chtag_get ( void *obj )
{
  return idnode_slist_get(obj, bouquest_class_chtag_slist);
}

static char *
bouquet_class_chtag_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, bouquest_class_chtag_slist, lang);
}

static int
bouquet_class_chtag_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, bouquest_class_chtag_slist, p);
}

static void
bouquet_class_chtag_notify ( void *obj, const char *lang )
{
  bouquet_t *bq = obj;
  service_t *t;
  idnode_list_mapping_t *ilm;
  channel_tag_t *ct;
  size_t z;

  if (bq->bq_in_load)
    return;
  if (!bq->bq_chtag && bq->bq_enabled && bq->bq_maptoch) {
    ct = bouquet_tag(bq, 0);
    if (!ct)
      return;
    for (z = 0; z < bq->bq_services->is_count; z++) {
      t = (service_t *)bq->bq_services->is_array[z];
      LIST_FOREACH(ilm, &t->s_channels, ilm_in1_link)
        if (((channel_t *)ilm->ilm_in2)->ch_bouquet == bq)
          break;
      if (ilm)
        channel_tag_unmap((channel_t *)ilm->ilm_in2, ct);
    }
  }
  bouquet_map_to_channels(bq);
}

static const void *
bouquet_class_chtag_ref_get ( void *obj )
{
  bouquet_t *bq = obj;

  if (bq->bq_chtag_ptr)
    idnode_uuid_as_str(&bq->bq_chtag_ptr->ct_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static char *
bouquet_class_chtag_ref_rend ( void *obj, const char *lang )
{
  bouquet_t *bq = obj;
  if (bq->bq_chtag_ptr)
    return strdup(bq->bq_chtag_ptr->ct_name ?: "");
  else
    return strdup("");
}

static int
bouquet_class_chtag_ref_set ( void *obj, const void *p )
{
  bouquet_t *bq = obj;

  free((char *)bq->bq_chtag_waiting);
  bq->bq_chtag_waiting = NULL;
  if (bq->bq_in_load)
    bq->bq_chtag_waiting = strdup((const char *)p);
  return 0;
}

static const void *
bouquet_class_services_get ( void *obj )
{
  htsmsg_t *m = htsmsg_create_map(), *e;
  bouquet_t *bq = obj;
  service_t *t;
  int64_t lcn;
  const char *tag;
  size_t z;
  char ubuf[UUID_HEX_SIZE];

  /* Add all */
  for (z = 0; z < bq->bq_services->is_count; z++) {
    t = (service_t *)bq->bq_services->is_array[z];
    e = htsmsg_create_map();
    if ((lcn = bouquet_get_channel_number(bq, t)) != 0)
      htsmsg_add_s64(e, "lcn", lcn);
    if ((tag = bouquet_get_tag_name(bq, t)) != NULL)
      htsmsg_add_str(e, "tag", tag);
    htsmsg_add_msg(m, idnode_uuid_as_str(&t->s_id, ubuf), e);
  }

  return m;
}

static char *
bouquet_class_services_rend ( void *obj, const char *lang )
{
  bouquet_t *bq = obj;
  const char *sc = N_("Service count %zi");
  char buf[32];
  snprintf(buf, sizeof(buf), tvh_gettext_lang(lang, sc), bq->bq_services->is_count);
  return strdup(buf);
}

static int
bouquet_class_services_set ( void *obj, const void *p )
{
  bouquet_t *bq = obj;

  if (bq->bq_services_waiting)
    htsmsg_destroy(bq->bq_services_waiting);
  bq->bq_services_waiting = NULL;
  if (bq->bq_in_load)
    bq->bq_services_waiting = htsmsg_copy((htsmsg_t *)p);
  return 0;
}

static const void *
bouquet_class_services_count_get ( void *obj )
{
  static uint32_t u32;
  bouquet_t *bq = obj;

  u32 = bq->bq_services->is_count;
  return &u32;
}

static void
bouquet_class_ext_url_notify ( void *obj, const char *lang )
{
  bouquet_download_trigger((bouquet_t *)obj);
}

CLASS_DOC(bouquet)
PROP_DOC(bouquet_mapping_options)
PROP_DOC(bouquet_tagging)

const idclass_t bouquet_class = {
  .ic_class      = "bouquet",
  .ic_caption    = N_("Bouquets"),
  .ic_doc        = tvh_doc_bouquet_class,
  .ic_event      = "bouquet",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = bouquet_class_save,
  .ic_get_title  = bouquet_class_get_title,
  .ic_delete     = bouquet_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the bouquet."),
      .def.i    = 1,
      .off      = offsetof(bouquet_t, bq_enabled),
      .notify   = bouquet_class_enabled_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "maptoch",
      .name     = N_("Auto-Map to channels"),
      .desc     = N_("Automatically map channels defined within the "
                     "bouquet."),
      .off      = offsetof(bouquet_t, bq_maptoch),
      .notify   = bouquet_class_maptoch_notify,
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "mapopt",
      .name     = N_("Channel mapping options"),
      .desc     = N_("Options to use/used when mapping - see Help for details."),
      .doc      = prop_doc_bouquet_mapping_options,
      .notify   = bouquet_class_mapopt_notify,
      .list     = bouquet_class_mapopt_enum,
      .get      = bouquet_class_mapopt_get,
      .set      = bouquet_class_mapopt_set,
      .rend     = bouquet_class_mapopt_rend,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "chtag",
      .name     = N_("Create tags"),
      .desc     = N_("Create and link these tags to channels when "
                     "mapping."),
      .doc      = prop_doc_bouquet_tagging,
      .notify   = bouquet_class_chtag_notify,
      .list     = bouquet_class_chtag_enum,
      .get      = bouquet_class_chtag_get,
      .set      = bouquet_class_chtag_set,
      .rend     = bouquet_class_chtag_rend,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "chtag_ref",
      .name     = N_("Channel tag reference"),
      .get      = bouquet_class_chtag_ref_get,
      .set      = bouquet_class_chtag_ref_set,
      .rend     = bouquet_class_chtag_ref_rend,
      .opts     = PO_RDONLY | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("Name of the bouquet."),
      .off      = offsetof(bouquet_t, bq_name),
    },
    {
      .type     = PT_STR,
      .id       = "ext_url",
      .name     = N_("External URL"),
      .desc     = N_("External URL of the bouquet."),
      .off      = offsetof(bouquet_t, bq_ext_url),
      .opts     = PO_HIDDEN | PO_MULTILINE,
      .notify   = bouquet_class_ext_url_notify,
    },
    {
      .type     = PT_BOOL,
      .id       = "ssl_peer_verify",
      .name     = N_("SSL verify peer"),
      .desc     = N_("Verify the SSL certificate."),
      .off      = offsetof(bouquet_t, bq_ssl_peer_verify),
      .opts     = PO_ADVANCED | PO_HIDDEN | PO_EXPERT,
      .notify   = bouquet_class_ext_url_notify,
    },
    {
      .type     = PT_U32,
      .id       = "ext_url_period",
      .name     = N_("Re-fetch period (mins)"),
      .desc     = N_("Re-fetch the bouquet every x minutes."),
      .off      = offsetof(bouquet_t, bq_ext_url_period),
      .opts     = PO_ADVANCED | PO_HIDDEN,
      .notify   = bouquet_class_ext_url_notify,
      .def.i    = 60
    },
    {
      .type     = PT_STR,
      .id       = "source",
      .name     = N_("Source"),
      .desc     = N_("Bouquet source."),
      .off      = offsetof(bouquet_t, bq_src),
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "services",
      .name     = N_("Services"),
      .desc     = N_("Number of services."),
      .get      = bouquet_class_services_get,
      .set      = bouquet_class_services_set,
      .rend     = bouquet_class_services_rend,
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {
      .type     = PT_U32,
      .id       = "services_seen",
      .name     = N_("Services seen"),
      .desc     = N_("Total number of services seen."),
      .off      = offsetof(bouquet_t, bq_services_seen),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_U32,
      .id       = "services_count",
      .name     = N_("Services"),
      .desc     = N_("Total number of services."),
      .get      = bouquet_class_services_count_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like."),
      .off      = offsetof(bouquet_t, bq_comment),
    },
    {
      .type     = PT_U32,
      .id       = "lcn_off",
      .name     = N_("Channel number offset"),
      .desc     = N_("Offset the mapped channel numbers by x "
                     "(value here + channel number)."),
      .off      = offsetof(bouquet_t, bq_lcn_offset),
      .notify   = bouquet_class_lcn_offset_notify,
      .opts     = PO_ADVANCED
    },
    {}
  }
};

/**
 *
 */
static void
bouquet_download_trigger0(void *aux)
{
  bouquet_download_t *bqd = aux;
  bouquet_t *bq = bqd->bq;

  download_start(&bqd->download, bq->bq_ext_url, bqd);
  mtimer_arm_rel(&bqd->timer, bouquet_download_trigger0, bqd,
                 sec2mono(MAX(1, bq->bq_ext_url_period) * 60));
}

static void
bouquet_download_trigger(bouquet_t *bq)
{
  bouquet_download_t *bqd = bq->bq_download;
  if (bqd) {
    bqd->download.ssl_peer_verify = bq->bq_ssl_peer_verify;
    bouquet_download_trigger0(bqd);
  }
}

static void
bouquet_download_stop(void *aux)
{
  bouquet_download_t *bqd = aux;
  mtimer_disarm(&bqd->timer);
}

static int
parse_enigma2(bouquet_t *bq, char *data)
{
  extern service_t *mpegts_service_find_e2(uint32_t stype, uint32_t sid,
                                           uint32_t tsid, uint32_t onid,
                                           uint32_t hash);
  char *argv[11], *p, *tagname = NULL, *name;
  long lv, stype, sid, tsid, onid, hash;
  uint32_t seen = 0;
  int n, ver = 2;

  while (*data) {
    if (strncmp(data, "#NAME ", 6) == 0) {
      for (data += 6, p = data; *data && *data != '\r' && *data != '\n'; data++);
      if (*data) { *data = '\0'; data++; }
      if (bq->bq_name == NULL || bq->bq_name[0] == '\0')
        bq->bq_name = strdup(p);
    } if (strncmp(data, "#SERVICE ", 9) == 0) {
      data += 9, p = data;
service:
      while (*data && *data != '\r' && *data != '\n') data++;
      if (*data) { *data = '\0'; data++; }
      n = http_tokenize(p, argv, ARRAY_SIZE(argv), ':');
      if (n >= 10) {
        if (strtol(argv[0], NULL, 0) != 1) goto next;  /* item type */
        lv = strtol(argv[1], NULL, 16);                /* service flags? */
        if (lv != 0 && lv != 0x64) goto next;
        stype = strtol(argv[2], NULL, 16);             /* DVB service type */
        sid   = strtol(argv[3], NULL, 16);             /* DVB service ID */
        tsid  = strtol(argv[4], NULL, 16);
        onid  = strtol(argv[5], NULL, 16);
        hash  = strtol(argv[6], NULL, 16);
        name  = n > 10 ? argv[10] : NULL;
        if (lv == 0) {
          service_t *s = mpegts_service_find_e2(stype, sid, tsid, onid, hash);
          if (s)
            bouquet_add_service(bq, s, ((int64_t)++seen) * CHANNEL_SPLIT, tagname);
        } else if (lv == 0x64) {
          tagname = name;
        }
      }
    } else if (strncmp(data, "#SERVICE: ", 10) == 0) {
      ver = 1, data += 10, p = data;
      goto service;
    } else {
      while (*data && *data != '\r' && *data != '\n') data++;
    }
next:
    while (*data && (*data == '\r' || *data == '\n')) data++;
  }
  bouquet_completed(bq, seen);
  tvhinfo(LS_BOUQUET, "parsed Enigma%d bouquet %s (%d services)", ver, bq->bq_name, seen);
  return 0;
}

static int
bouquet_download_process(void *aux, const char *last_url, const char *host_url,
                         char *data, size_t len)
{
  bouquet_download_t *bqd = aux;
  while (*data) {
    while (*data && *data < ' ') data++;
    if (*data && !strncmp(data, "#NAME ", 6))
      return parse_enigma2(bqd->bq, data);
  }
  return 0;
}

/**
 *
 */
void
bouquet_init(void)
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;
  bouquet_t *bq;

  RB_INIT(&bouquets);
  idclass_register(&bouquet_class);

  /* Load */
  if ((c = hts_settings_load("bouquet")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      bq = bouquet_create(f->hmf_name, m, NULL, NULL);
      if (bq)
        bq->bq_saveflag = 0;
    }
    htsmsg_destroy(c);
  }
}

void
bouquet_service_resolve(void)
{
  bouquet_t *bq;
  htsmsg_t *e;
  htsmsg_field_t *f;
  service_t *s;
  int64_t lcn;
  const char *tag;
  int saveflag;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)  {
    if (!bq->bq_services_waiting)
      continue;
    saveflag = bq->bq_saveflag;
    if (bq->bq_enabled) {
      HTSMSG_FOREACH(f, bq->bq_services_waiting) {
        if ((e = htsmsg_field_get_map(f)) == NULL) continue;
        lcn = htsmsg_get_s64_or_default(e, "lcn", 0);
        tag = htsmsg_get_str(e, "tag");
        s = service_find_by_identifier(f->hmf_name);
        if (s)
          bouquet_add_service(bq, s, lcn, tag);
      }
    }
    htsmsg_destroy(bq->bq_services_waiting);
    bq->bq_services_waiting = NULL;
    bq->bq_saveflag = saveflag;
  }
}

void
bouquet_done(void)
{
  bouquet_t *bq;

  pthread_mutex_lock(&global_lock);
  while ((bq = RB_FIRST(&bouquets)) != NULL)
    bouquet_destroy(bq);
  pthread_mutex_unlock(&global_lock);
}
