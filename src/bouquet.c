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

bouquet_tree_t bouquets;

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
bouquet_t *
bouquet_create(const char *uuid, htsmsg_t *conf,
               const char *name, const char *src)
{
  bouquet_t *bq, *bq2;
  int i;

  lock_assert(&global_lock);

  bq = calloc(1, sizeof(bouquet_t));
  bq->bq_services = idnode_set_create();

  if (idnode_insert(&bq->bq_id, uuid, &bouquet_class, 0)) {
    if (uuid)
      tvherror("bouquet", "invalid uuid '%s'", uuid);
    free(bq);
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

  bq2 = RB_INSERT_SORTED(&bouquets, bq, bq_link, _bq_cmp);
  assert(bq2 == NULL);

  bq->bq_saveflag = 1;

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
  idnode_unlink(&bq->bq_id);

  idnode_set_free(bq->bq_services);
  free(bq->bq_name);
  free(bq->bq_src);
  free(bq);
}

/**
 *
 */
void
bouquet_destroy_by_service(service_t *t)
{
  bouquet_t *bq;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)
    if (idnode_set_exists(bq->bq_services, &t->s_id))
      idnode_set_remove(bq->bq_services, &t->s_id);
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
  if (bq)
    return bq;
  if (create && name)
    return bouquet_create(NULL, NULL, name, src);
  return NULL;
}

/*
 *
 */
void
bouquet_add_service(bouquet_t *bq, service_t *s)
{
  lock_assert(&global_lock);

  if (!idnode_set_exists(bq->bq_services, &s->s_id)) {
    idnode_set_add(bq->bq_services, &s->s_id, NULL);
    bq->bq_saveflag = 1;
  }
}

/**
 *
 */
void
bouquet_save(bouquet_t *bq, int notify)
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&bq->bq_id, c);
  hts_settings_save(c, "bouquet/%s", idnode_uuid_as_str(&bq->bq_id));
  if (bq->bq_shield)
    htsmsg_add_bool(c, "shield", 1);
  htsmsg_destroy(c);
  bq->bq_saveflag = 0;
  if (notify)
    idnode_notify_simple(&bq->bq_id);
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static void
bouquet_class_save(idnode_t *self)
{
  bouquet_save((bouquet_t *)self, 0);
}

static void
bouquet_class_delete(idnode_t *self)
{
  bouquet_t *bq = (bouquet_t *)self;

  if (!bq->bq_shield) {
    hts_settings_remove("bouquet/%s", idnode_uuid_as_str(&bq->bq_id));
    bouquet_destroy(bq);
  } else {
    idnode_set_free(bq->bq_services);
    bq->bq_services = idnode_set_create();
    bouquet_save(bq, 1);
  }
}

static const char *
bouquet_class_get_title (idnode_t *self)
{
  bouquet_t *bq = (bouquet_t *)self;

  if (bq->bq_comment && bq->bq_comment[0] != '\0')
    return bq->bq_comment;
  return bq->bq_name ?: "";
}

static void
bouquet_class_enabled_notify ( void *obj )
{
  bouquet_t *bq = obj;
  service_t *s;
  size_t z;

  if (!bq->bq_enabled) {
    for (z = 0; z < bq->bq_services->is_count; z++) {
      s = (service_t *)bq->bq_services->is_array[z];
      if (s->s_master_bouquet == bq)
        s->s_master_bouquet = NULL;
    }
  }
}

static const void *
bouquet_class_services_get ( void *obj )
{
  htsmsg_t *l = htsmsg_create_list();
  bouquet_t *bq = obj;
  size_t z;

  /* Add all */
  for (z = 0; z < bq->bq_services->is_count; z++)
    htsmsg_add_str(l, NULL, idnode_uuid_as_str(bq->bq_services->is_array[z]));

  return l;
}

static char *
bouquet_class_services_rend ( void *obj )
{
  bouquet_t *bq = obj;
  char buf[32];
  snprintf(buf, sizeof(buf), "Services Count %zi", bq->bq_services->is_count);
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

const idclass_t bouquet_class = {
  .ic_class      = "bouquet",
  .ic_caption    = "Bouquet",
  .ic_event      = "bouquet",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = bouquet_class_save,
  .ic_get_title  = bouquet_class_get_title,
  .ic_delete     = bouquet_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(bouquet_t, bq_enabled),
      .notify   = bouquet_class_enabled_notify,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = "Name",
      .off      = offsetof(bouquet_t, bq_name),
    },
    {
      .type     = PT_STR,
      .id       = "source",
      .name     = "Source",
      .off      = offsetof(bouquet_t, bq_src),
      .opts     = PO_RDONLY,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "services",
      .name     = "Services",
      .get      = bouquet_class_services_get,
      .set      = bouquet_class_services_set,
      .rend     = bouquet_class_services_rend,
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {
      .type     = PT_U32,
      .id       = "services_count",
      .name     = "# Services",
      .get      = bouquet_class_services_count_get,
      .opts     = PO_RDONLY | PO_NOSAVE,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
      .off      = offsetof(bouquet_t, bq_comment),
    },
    {
      .type     = PT_U32,
      .id       = "lcn_off",
      .name     = "Channel Number Offset",
      .off      = offsetof(bouquet_t, bq_lcn_offset),
    },
    {}
  }
};

/**
 *
 */
void
bouquet_init(void)
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;

  RB_INIT(&bouquets);

  /* Load */
  if ((c = hts_settings_load("bouquet")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)bouquet_create(f->hmf_name, m, NULL, NULL);
    }
    htsmsg_destroy(c);
  }
}

void
bouquet_service_resolve(void)
{
  bouquet_t *bq;
  htsmsg_field_t *f;
  service_t *s;
  const char *str;
  int saveflag;

  lock_assert(&global_lock);

  RB_FOREACH(bq, &bouquets, bq_link)  {
    if (!bq->bq_services_waiting)
      continue;
    saveflag = bq->bq_saveflag;
    HTSMSG_FOREACH(f, bq->bq_services_waiting) {
      if ((str = htsmsg_field_get_str(f))) {
        s = service_find_by_identifier(str);
        if (s)
          bouquet_add_service(bq, s);
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
