/*
 *  Tvheadend - conditional access key client superclass
 *  Copyright (C) 2014 Jaroslav Kysela <perex@perex.cz>
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
#include "caclient.h"

const idclass_t *caclient_classes[] = {
#if ENABLE_CWC
  &caclient_cwc_class,
#endif
#if ENABLE_CCCAM
  &caclient_cccam_class,
#endif
#if ENABLE_CAPMT
  &caclient_capmt_class,
#endif
#if ENABLE_CONSTCW
  &caclient_ccw_csa_cbc_class,
  &caclient_ccw_des_ncb_class,
  &caclient_ccw_aes_ecb_class,
  &caclient_ccw_aes128_ecb_class,
#endif
  NULL
};

struct caclient_entry_queue caclients;
static pthread_mutex_t caclients_mutex;

static const idclass_t *
caclient_class_find(const char *name)
{
  const idclass_t **r;
  for (r = caclient_classes; *r; r++) {
    if (strcmp((*r)->ic_class, name) == 0)
      return *r;
  }
  return NULL;
}

static void
caclient_reindex(void)
{
  caclient_t *cac;
  int i = 1;

  TAILQ_FOREACH(cac, &caclients, cac_link)
    cac->cac_save = 0;
  TAILQ_FOREACH(cac, &caclients, cac_link) {
    if (cac->cac_index != i) {
      cac->cac_index = i;
      cac->cac_save = 1;
    }
    i++;
  }
  TAILQ_FOREACH(cac, &caclients, cac_link)
    if (cac->cac_save) {
      cac->cac_save = 0;
      idnode_changed((idnode_t *)cac);
    }
}

static int
cac_cmp(caclient_t *a, caclient_t *b)
{
  return a->cac_index - b->cac_index;
}

caclient_t *
caclient_create
  (const char *uuid, htsmsg_t *conf, int save)
{
  caclient_t *cac = NULL;
  const idclass_t *c = NULL;
  const char *s;

  lock_assert(&global_lock);

  if ((s = htsmsg_get_str(conf, "class")) != NULL)
    c = caclient_class_find(s);
  if (c == NULL) {
    tvherror(LS_CACLIENT, "unknown class %s!", s);
    return NULL;
  }
#if ENABLE_CWC
  if (c == &caclient_cwc_class)
    cac = cwc_create();
#endif
#if ENABLE_CCCAM
  if (c == &caclient_cccam_class)
    cac = cccam_create();
#endif
#if ENABLE_CAPMT
  if (c == &caclient_capmt_class)
    cac = capmt_create();
#endif
#if ENABLE_CONSTCW
  if (c == &caclient_ccw_csa_cbc_class ||
      c == &caclient_ccw_des_ncb_class ||
      c == &caclient_ccw_aes_ecb_class)
    cac = constcw_create();
#endif
  if (cac == NULL) {
    tvherror(LS_CACLIENT, "CA Client class %s is not available!", s);
    return NULL;
  }
  if (idnode_insert(&cac->cac_id, uuid, c, 0)) {
    if (uuid)
      tvherror(LS_CACLIENT, "invalid uuid '%s'", uuid);
    free(cac);
    return NULL;
  }
  if (conf)
    idnode_load(&cac->cac_id, conf);
  pthread_mutex_lock(&caclients_mutex);
  if (cac->cac_index) {
    TAILQ_INSERT_SORTED(&caclients, cac, cac_link, cac_cmp);
  } else {
    TAILQ_INSERT_TAIL(&caclients, cac, cac_link);
    caclient_reindex();
  }
  pthread_mutex_unlock(&caclients_mutex);
  if (save)
    idnode_changed((idnode_t *)cac);
  cac->cac_conf_changed(cac);
  return cac;
}

static void
caclient_delete(caclient_t *cac, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&cac->cac_id, delconf);
  cac->cac_enabled = 0;
  cac->cac_conf_changed(cac);
  if (delconf)
    hts_settings_remove("caclient/%s", idnode_uuid_as_str(&cac->cac_id, ubuf));
  pthread_mutex_lock(&caclients_mutex);
  TAILQ_REMOVE(&caclients, cac, cac_link);
  pthread_mutex_unlock(&caclients_mutex);
  idnode_unlink(&cac->cac_id);
  if (cac->cac_free)
    cac->cac_free(cac);
  free(cac->cac_name);
  free(cac->cac_comment);
  free(cac);
}

static void
caclient_class_changed ( idnode_t *in )
{
  caclient_t *cac = (caclient_t *)in;
  cac->cac_conf_changed(cac);
}

static htsmsg_t *
caclient_class_save ( idnode_t *in, char *filename, size_t fsize )
{
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(in, c);
  if (filename)
    snprintf(filename, fsize, "caclient/%s", idnode_uuid_as_str(in, ubuf));
  return c;
}

static const char *
caclient_class_get_title ( idnode_t *in, const char *lang )
{
  caclient_t *cac = (caclient_t *)in;
  if (cac->cac_name && cac->cac_name[0])
    return cac->cac_name;
  snprintf(prop_sbuf, PROP_SBUF_LEN,
           tvh_gettext_lang(lang, N_("CA client %i")), cac->cac_index);
  return prop_sbuf;
}

static void
caclient_class_delete(idnode_t *self)
{
  caclient_t *cac = (caclient_t *)self;
  caclient_delete(cac, 1);
}

static void
caclient_class_moveup(idnode_t *self)
{
  caclient_t *cac = (caclient_t *)self;
  caclient_t *prev = TAILQ_PREV(cac, caclient_entry_queue, cac_link);
  if (prev) {
    TAILQ_REMOVE(&caclients, cac, cac_link);
    TAILQ_INSERT_BEFORE(prev, cac, cac_link);
    caclient_reindex();
  }
}

static void
caclient_class_movedown(idnode_t *self)
{
  caclient_t *cac = (caclient_t *)self;
  caclient_t *next = TAILQ_NEXT(cac, cac_link);
  if (next) {
    TAILQ_REMOVE(&caclients, cac, cac_link);
    TAILQ_INSERT_AFTER(&caclients, next, cac, cac_link);
    caclient_reindex();
  }
}

static const void *
caclient_class_class_get(void *o)
{
  caclient_t *cac = o;
  static const char *ret;
  ret = cac->cac_id.in_class->ic_class;
  return &ret;
}

static int
caclient_class_class_set(void *o, const void *v)
{
  /* just ignore, create fcn does the right job */
  return 0;
}

static const void *
caclient_class_status_get(void *o)
{
  caclient_t *cac = o;
  prop_ptr = caclient_get_status(cac);
  return &prop_ptr;
}

CLASS_DOC(caclient)

const idclass_t caclient_class =
{
  .ic_class      = "caclient",
  .ic_caption    = N_("Conditional Access Client"),
  .ic_changed    = caclient_class_changed,
  .ic_save       = caclient_class_save,
  .ic_event      = "caclient",
  .ic_doc        = tvh_doc_caclient_class,
  .ic_get_title  = caclient_class_get_title,
  .ic_delete     = caclient_class_delete,
  .ic_moveup     = caclient_class_moveup,
  .ic_movedown   = caclient_class_movedown,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "class",
      .name     = N_("Class"),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_NOUI,
      .get      = caclient_class_class_get,
      .set      = caclient_class_class_set,
    },
    {
      .type     = PT_INT,
      .id       = "index",
      .name     = N_("Index"),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_NOUI,
      .off      = offsetof(caclient_t, cac_index),
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/Disable CA client."),
      .off      = offsetof(caclient_t, cac_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Client name"),
      .desc     = N_("Name of the client."),
      .off      = offsetof(caclient_t, cac_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like."),
      .off      = offsetof(caclient_t, cac_comment),
    },
    {
      .type     = PT_STR,
      .id       = "status",
      .name     = N_("Status"),
      .get      = caclient_class_status_get,
      .opts     = PO_RDONLY | PO_HIDDEN | PO_NOSAVE | PO_NOUI,
    },
    { }
  }
};

void
caclient_start ( struct service *t )
{
  caclient_t *cac;

  pthread_mutex_lock(&caclients_mutex);
  TAILQ_FOREACH(cac, &caclients, cac_link)
    if (cac->cac_enabled)
      cac->cac_start(cac, t);
  pthread_mutex_unlock(&caclients_mutex);
#if ENABLE_TSDEBUG
  tsdebugcw_service_start(t);
#endif
}

void
caclient_caid_update(struct mpegts_mux *mux, uint16_t caid, uint16_t pid, int valid)
{
  caclient_t *cac;

  lock_assert(&global_lock);

  pthread_mutex_lock(&caclients_mutex);
  TAILQ_FOREACH(cac, &caclients, cac_link)
    if (cac->cac_caid_update && cac->cac_enabled)
      cac->cac_caid_update(cac, mux, caid, pid, valid);
  pthread_mutex_unlock(&caclients_mutex);
}

void
caclient_set_status(caclient_t *cac, caclient_status_t status)
{
  if (cac->cac_status != status) {
    cac->cac_status = status;
    idnode_notify_changed(&cac->cac_id);
  }
}

const char *
caclient_get_status(caclient_t *cac)
{
  switch (cac->cac_status) {
  case CACLIENT_STATUS_NONE:         return "caclientNone";
  case CACLIENT_STATUS_READY:        return "caclientReady";
  case CACLIENT_STATUS_CONNECTED:    return "caclientConnected";
  case CACLIENT_STATUS_DISCONNECTED: return "caclientDisconnected";
  default:                           return "caclientUnknown";
  }
}

/*
 *  Initialize
 */
void
caclient_init(void)
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  const idclass_t **r;

  pthread_mutex_init(&caclients_mutex, NULL);
  TAILQ_INIT(&caclients);
  idclass_register(&caclient_class);
#if ENABLE_TSDEBUG
  tsdebugcw_init();
#endif

  for (r = caclient_classes; *r; r++)
    idclass_register(*r);

  if (!(c = hts_settings_load("caclient")))
    return;
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f)))
      continue;
    caclient_create(f->hmf_name, e, 0);
  }
  htsmsg_destroy(c);
}

void
caclient_done(void)
{
  caclient_t *cac;

  pthread_mutex_lock(&global_lock);
  while ((cac = TAILQ_FIRST(&caclients)) != NULL)
    caclient_delete(cac, 0);
  pthread_mutex_unlock(&global_lock);
}
