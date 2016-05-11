/*
 *  tvheadend, Elementary Stream Filter
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
#include "lang_codes.h"
#include "service.h"
#include "access.h"
#include "descrambler/caid.h"
#include "esfilter.h"

struct esfilter_entry_queue esfilters[ESF_CLASS_LAST + 1];

/*
 * Class masks
 */
uint32_t esfilterclsmask[ESF_CLASS_LAST+1] = {
  0,
  ESF_MASK_VIDEO,
  ESF_MASK_AUDIO,
  ESF_MASK_TELETEXT,
  ESF_MASK_SUBTIT,
  ESF_MASK_CA,
  ESF_MASK_OTHER
};

static const idclass_t *esfilter_classes[ESF_CLASS_LAST+1] = {
  NULL,
  &esfilter_class_video,
  &esfilter_class_audio,
  &esfilter_class_teletext,
  &esfilter_class_subtit,
  &esfilter_class_ca,
  &esfilter_class_other
};

/*
 * Class types
 */

static struct strtab esfilterclasstab[] = {
  { "NONE",       ESF_CLASS_NONE },
  { "VIDEO",      ESF_CLASS_VIDEO },
  { "AUDIO",      ESF_CLASS_AUDIO },
  { "TELETEXT",   ESF_CLASS_TELETEXT },
  { "SUBTIT",     ESF_CLASS_SUBTIT },
  { "CA",         ESF_CLASS_CA },
  { "OTHER",      ESF_CLASS_OTHER },
};

const char *
esfilter_class2txt(int cls)
{
  return val2str(cls, esfilterclasstab) ?: "INVALID";
}

#if 0
static int
esfilter_txt2class(const char *s)
{
  return s ? str2val(s, esfilterclasstab) : ESF_CLASS_NONE;
}
#endif

/*
 * Action types
 */

static struct strtab esfilteractiontab[] = {
  { "NONE",       ESFA_NONE },
  { "USE",	  ESFA_USE },
  { "ONE_TIME",	  ESFA_ONE_TIME },
  { "EXCLUSIVE",  ESFA_EXCLUSIVE },
  { "EMPTY",	  ESFA_EMPTY },
  { "IGNORE",     ESFA_IGNORE }
};

const char *
esfilter_action2txt(esfilter_action_t a)
{
  return val2str(a, esfilteractiontab) ?: "INVALID";
}

#if 0
static esfilter_action_t
esfilter_txt2action(const char *s)
{
  return s ? str2val(s, esfilteractiontab) : ESFA_NONE;
}
#endif

/*
 * Create / delete
 */

static void
esfilter_reindex(esfilter_class_t cls)
{
  esfilter_t *esf;
  int i = 1;

  TAILQ_FOREACH(esf, &esfilters[cls], esf_link)
    esf->esf_save = 0;
  TAILQ_FOREACH(esf, &esfilters[cls], esf_link) {
    if (esf->esf_index != i) {
      esf->esf_index = i;
      esf->esf_save = 1;
    }
    i++;
  }
  TAILQ_FOREACH(esf, &esfilters[cls], esf_link)
    if (esf->esf_save) {
      esf->esf_save = 0;
      idnode_changed(&esf->esf_id);
    }
}

static int
esfilter_cmp(esfilter_t *a, esfilter_t *b)
{
  return a->esf_index - b->esf_index;
}

esfilter_t *
esfilter_create
  (esfilter_class_t cls, const char *uuid, htsmsg_t *conf, int save)
{
  esfilter_t *esf = calloc(1, sizeof(*esf));
  const idclass_t *c = NULL;
  uint32_t ct;

  lock_assert(&global_lock);

  esf->esf_caid = -1;
  esf->esf_caprovider = -1;
  if (ESF_CLASS_IS_VALID(cls)) {
    c = esfilter_classes[cls];
  } else {
    if (!htsmsg_get_u32(conf, "class", &ct)) {
      cls = ct;
      if (ESF_CLASS_IS_VALID(cls))
        c = esfilter_classes[cls];
    }
  }
  if (!c) {
    tvherror("esfilter", "wrong class %d!", cls);
    abort();
  }
  if (idnode_insert(&esf->esf_id, uuid, c, 0)) {
    if (uuid)
      tvherror("esfilter", "invalid uuid '%s'", uuid);
    free(esf);
    return NULL;
  }
  if (conf)
    idnode_load(&esf->esf_id, conf);
  if (ESF_CLASS_IS_VALID(cls))
    esf->esf_class = cls;
  else if (!ESF_CLASS_IS_VALID(esf->esf_class)) {
    tvherror("esfilter", "wrong class %d!", esf->esf_class);
    abort();
  }
  if (esf->esf_index) {
    TAILQ_INSERT_SORTED(&esfilters[esf->esf_class], esf, esf_link, esfilter_cmp);
  } else {
    TAILQ_INSERT_TAIL(&esfilters[esf->esf_class], esf, esf_link);
  }
  if (!conf)
    esfilter_reindex(esf->esf_class);
  if (save)
    idnode_changed(&esf->esf_id);
  return esf;
}

static void
esfilter_delete(esfilter_t *esf, int delconf)
{
  char ubuf[UUID_HEX_SIZE];
  if (delconf)
    hts_settings_remove("esfilter/%s", idnode_uuid_as_str(&esf->esf_id, ubuf));
  TAILQ_REMOVE(&esfilters[esf->esf_class], esf, esf_link);
  idnode_save_check(&esf->esf_id, delconf);
  idnode_unlink(&esf->esf_id);
  free(esf->esf_comment);
  free(esf);
}

/*
 * Class functions
 */

static htsmsg_t *
esfilter_class_save(idnode_t *self, char *filename, size_t fsize)
{
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(self, c);
  snprintf(filename, fsize, "esfilter/%s", idnode_uuid_as_str(self, ubuf));
  return c;
}

static const char *
esfilter_class_get_title(idnode_t *self, const char *lang)
{
  esfilter_t *esf = (esfilter_t *)self;
  return idnode_uuid_as_str(&esf->esf_id, prop_sbuf);
}

static void
esfilter_class_delete(idnode_t *self)
{
  esfilter_t *esf = (esfilter_t *)self;
  esfilter_delete(esf, 1);
}

static void
esfilter_class_moveup(idnode_t *self)
{
  esfilter_t *esf = (esfilter_t *)self;
  esfilter_t *prev = TAILQ_PREV(esf, esfilter_entry_queue, esf_link);
  if (prev) {
    TAILQ_REMOVE(&esfilters[esf->esf_class], esf, esf_link);
    TAILQ_INSERT_BEFORE(prev, esf, esf_link);
    esfilter_reindex(esf->esf_class);
  }
}

static void
esfilter_class_movedown(idnode_t *self)
{
  esfilter_t *esf = (esfilter_t *)self;
  esfilter_t *next = TAILQ_NEXT(esf, esf_link);
  if (next) {
    TAILQ_REMOVE(&esfilters[esf->esf_class], esf, esf_link);
    TAILQ_INSERT_AFTER(&esfilters[esf->esf_class], next, esf, esf_link);
    esfilter_reindex(esf->esf_class);
  }
}

static const void *
esfilter_class_type_get(void *o)
{
  esfilter_t *esf = o;
  htsmsg_t *l = htsmsg_create_list();
  int i;

  for (i = SCT_UNKNOWN; i <= SCT_LAST; i++)
    if ((esf->esf_type & SCT_MASK(i)) != 0)
      htsmsg_add_u32(l, NULL, i);
  return l;
}

static char *
esfilter_class_type_rend (void *o, const char *lang)
{
  char *str;
  htsmsg_t *l = htsmsg_create_list();
  esfilter_t *esf = o;
  int i;

  for (i = SCT_UNKNOWN; i <= SCT_LAST; i++) {
    if (SCT_MASK(i) & esf->esf_type)
      htsmsg_add_str(l, NULL, streaming_component_type2txt(i));
  }

  str = htsmsg_list_2_csv(l, ',', 1);
  htsmsg_destroy(l);
  return str;
}

static int
esfilter_class_type_set_(void *o, const void *v, esfilter_class_t cls)
{
  esfilter_t *esf = o;
  htsmsg_t *types = (htsmsg_t*)v;
  htsmsg_field_t *f;
  uint32_t mask = 0, u32;
  uint32_t vmask = esfilterclsmask[cls];
  int save;

  HTSMSG_FOREACH(f, types) {
    if (!htsmsg_field_get_u32(f, &u32)) {
      if (SCT_MASK(u32) & vmask)
        mask |= SCT_MASK(u32);
    } else {
      return 0;
    }
  }
  save = esf->esf_type != mask;
  esf->esf_type = mask;
  return save;
}

static htsmsg_t *
esfilter_class_type_enum_(void *o, const char *lang, esfilter_class_t cls)
{
  uint32_t mask = esfilterclsmask[cls];
  htsmsg_t *l = htsmsg_create_list();
  const char *any = N_("ANY");
  int i;

  for (i = SCT_UNKNOWN; i <= SCT_LAST; i++) {
    if (mask & SCT_MASK(i)) {
      htsmsg_t *e = htsmsg_create_map();
      htsmsg_add_u32(e, "key", i);
      htsmsg_add_str(e, "val",
          i == SCT_UNKNOWN ? tvh_gettext_lang(lang, any) :
                             streaming_component_type2txt(i));
      htsmsg_add_msg(l, NULL, e);
    }
  }
  return l;
}

#define ESFILTER_CLS(func, type) \
static int esfilter_class_type_set_##func(void *o, const void *v) \
  { return esfilter_class_type_set_(o, v, type); } \
static htsmsg_t * esfilter_class_type_enum_##func(void *o, const char *lang) \
  { return esfilter_class_type_enum_(o, lang, type); }

ESFILTER_CLS(video, ESF_CLASS_VIDEO);
ESFILTER_CLS(audio, ESF_CLASS_AUDIO);
ESFILTER_CLS(teletext, ESF_CLASS_TELETEXT);
ESFILTER_CLS(subtit, ESF_CLASS_SUBTIT);
ESFILTER_CLS(ca, ESF_CLASS_CA);
ESFILTER_CLS(other, ESF_CLASS_OTHER);

static const void *
esfilter_class_language_get(void *o)
{
  static __thread char *ret;
  esfilter_t *esf = o;
  ret = esf->esf_language;
  return &ret;
}

static int
esfilter_class_language_set(void *o, const void *v)
{
  esfilter_t *esf = o;
  const char *s = v;
  char n[4];
  int save;
  strncpy(n, s && s[0] ? lang_code_get(s) : "", 4);
  n[3] = 0;
  save = strcmp(esf->esf_language, n);
  strcpy(esf->esf_language, n);
  return save;
}

static htsmsg_t *
esfilter_class_language_enum(void *o, const char *lang)
{
  htsmsg_t *l = htsmsg_create_list();
  const lang_code_t *lc = lang_codes;
  const char *any = N_("ANY");
  char buf[128];

  while (lc->code2b) {
    htsmsg_t *e = htsmsg_create_map();
    if (!strcmp(lc->code2b, "und")) {
      htsmsg_add_str(e, "key", "");
      htsmsg_add_str(e, "val", tvh_gettext_lang(lang, any));
    } else {
      htsmsg_add_str(e, "key", lc->code2b);
      snprintf(buf, sizeof(buf), "%s (%s)", lc->desc, lc->code2b);
      buf[sizeof(buf)-1] = '\0';
      htsmsg_add_str(e, "val", buf);
    }
    htsmsg_add_msg(l, NULL, e);
    lc++;
  }
  return l;
}

static const void *
esfilter_class_service_get(void *o)
{
  static __thread char *ret;
  esfilter_t *esf = o;
  ret = esf->esf_service;
  return &ret;
}

static int
esfilter_class_service_set(void *o, const void *v)
{
  esfilter_t *esf = o;
  const char *s = v;
  int save = 0;
  if (strncmp(esf->esf_service, s, UUID_HEX_SIZE)) {
    strncpy(esf->esf_service, s, UUID_HEX_SIZE);
    esf->esf_service[UUID_HEX_SIZE-1] = '\0';
    save = 1;
  }
  return save;
}

static htsmsg_t *
esfilter_class_service_enum(void *o, const char *lang)
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

#define MAX_ITEMS 256

static int
esfilter_build_ca_cmp(const void *_a, const void *_b)
{
  uint32_t a = *(uint32_t *)_a;
  uint32_t b = *(uint32_t *)_b;
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}

static htsmsg_t *
esfilter_build_ca_enum(int provider)
{
  htsmsg_t *e, *l;
  uint32_t *a = alloca(sizeof(uint32_t) * MAX_ITEMS);
  char buf[16], buf2[128];
  service_t *s;
  elementary_stream_t *es;
  caid_t *ca;
  uint32_t v;
  int i, count = 0;

  lock_assert(&global_lock);
  TAILQ_FOREACH(s, &service_all, s_all_link) {
    pthread_mutex_lock(&s->s_stream_mutex);
    TAILQ_FOREACH(es, &s->s_components, es_link) {
      LIST_FOREACH(ca, &es->es_caids, link) {
        v = provider ? ca->providerid : ca->caid;
        for (i = 0; i < count; i++)
          if (a[i] == v)
            break;
        if (i >= count)
          a[count++] = v;
      }
    }
    pthread_mutex_unlock(&s->s_stream_mutex);
  }
  qsort(a, count, sizeof(uint32_t), esfilter_build_ca_cmp);

  l = htsmsg_create_list();

  e = htsmsg_create_map();
  htsmsg_add_str(e, "key", provider ? "ffffff" : "ffff");
  htsmsg_add_str(e, "val", "ANY");
  htsmsg_add_msg(l, NULL, e);

  for (i = 0; i < count; i++) {
    e = htsmsg_create_map();
    snprintf(buf, sizeof(buf), provider ? "%06x" : "%04x", a[i]);
    if (!provider)
      snprintf(buf2, sizeof(buf2), "%04x - %s",
               a[i], caid2name(a[i]));
    htsmsg_add_str(e, "key", buf);
    htsmsg_add_str(e, "val", provider ? buf : buf2);
    htsmsg_add_msg(l, NULL, e);
  }
  return l;

}

static const void *
esfilter_class_caid_get(void *o)
{
  static __thread char *ret;
  static __thread char buf[16];
  esfilter_t *esf = o;
  snprintf(buf, sizeof(buf), "%04x", esf->esf_caid);
  ret = buf;
  return &ret;
}

static int
esfilter_class_caid_set(void *o, const void *v)
{
  esfilter_t *esf = o;
  uint16_t u;
  int save = 0;
  u = strtol(v, NULL, 16);
  if (u != esf->esf_caid) {
    esf->esf_caid = u;
    save = 1;
  }
  return save;
}

static htsmsg_t *
esfilter_class_caid_enum(void *o, const char *lang)
{
  return esfilter_build_ca_enum(0);
}

static const void *
esfilter_class_caprovider_get(void *o)
{
  static __thread char *ret;
  static __thread char buf[16];
  esfilter_t *esf = o;
  if (esf->esf_caprovider == -1)
    strcpy(buf, "ffffff");
  else
    snprintf(buf, sizeof(buf), "%06x", esf->esf_caprovider);
  ret = buf;
  return &ret;
}

static int
esfilter_class_caprovider_set(void *o, const void *v)
{
  esfilter_t *esf = o;
  uint32_t u;
  int save = 0;
  if (strcmp(v, "ffffff") == 0)
    u = -1;
  else
    u = strtol(v, NULL, 16);
  if (u != esf->esf_caprovider) {
    esf->esf_caprovider = u;
    save = 1;
  }
  return save;
}

static htsmsg_t *
esfilter_class_caprovider_enum(void *o, const char *lang)
{
  return esfilter_build_ca_enum(1);
}

static const void *
esfilter_class_action_get(void *o)
{
  esfilter_t *esf = o;
  return &esf->esf_action;
}

static int
esfilter_class_action_set(void *o, const void *v)
{
  esfilter_t *esf = o;
  int n = *(int *)v;
  int save = 0;
  if (n >= ESFA_USE && n <= ESFA_LAST) {
    save = esf->esf_action != n;
    esf->esf_action = n;
  }
  return save;
}

static htsmsg_t *
esfilter_class_action_enum(void *o, const char *lang)
{
  htsmsg_t *l = htsmsg_create_list();
  int i;

  for (i = ESFA_NONE; i <= ESFA_LAST; i++) {
    htsmsg_t *e = htsmsg_create_map();
    htsmsg_add_u32(e, "key", i);
    htsmsg_add_str(e, "val", esfilter_action2txt(i));
    htsmsg_add_msg(l, NULL, e);
  }
  return l;
}

CLASS_DOC(filters)
PROP_DOC(action)

const idclass_t esfilter_class = {
  .ic_class      = "esfilter",
  .ic_caption    = N_("Elementary stream filter"),
  .ic_doc        = tvh_doc_filters_class,
  .ic_event      = "esfilter",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = esfilter_class_save,
  .ic_get_title  = esfilter_class_get_title,
  .ic_delete     = esfilter_class_delete,
  .ic_moveup     = esfilter_class_moveup,
  .ic_movedown   = esfilter_class_movedown,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "class",
      .name     = N_("Class"),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_DOC_NLIST,
      .off      = offsetof(esfilter_t, esf_class),
    },
    {
      .type     = PT_INT,
      .id       = "index",
      .name     = N_("Index"),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_DOC_NLIST,
      .off      = offsetof(esfilter_t, esf_index),
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable this filter."),
      .off      = offsetof(esfilter_t, esf_enabled),
    },
    {}
  }
};

const idclass_t esfilter_class_video = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_video",
  .ic_caption    = N_("Video Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("The video stream types the filter should apply "
                     "to."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_video,
      .list     = esfilter_class_type_enum_video,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("The language the filter should apply to."),
      .get      = esfilter_class_language_get,
      .set      = esfilter_class_language_set,
      .list     = esfilter_class_language_enum,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "sindex",
      .name     = N_("Stream index"),
      .desc     = N_("The logical stream index to compare. Note that "
                     "this index is computed using all filters."
                     "Example: If filter is set to AC3 audio type and "
                     "the language to 'eng' and there are two AC3 "
                     "'eng' streams in the service, the first stream "
                     "could be identified using number 1 and the "
                     "second using number 2."),
      .off      = offsetof(esfilter_t, esf_sindex),
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

const idclass_t esfilter_class_audio = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_audio",
  .ic_caption    = N_("Audio Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("The audio stream types the filter should apply "
                     "to."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_audio,
      .list     = esfilter_class_type_enum_audio,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("The language the filter should apply to."),
      .get      = esfilter_class_language_get,
      .set      = esfilter_class_language_set,
      .list     = esfilter_class_language_enum,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "sindex",
      .name     = N_("Stream index"),
      .desc     = N_("The logical stream index to compare. Note that "
                     "this index is computed using all filters."
                     "Example: If filter is set to AC3 audio type and "
                     "the language to 'eng' and there are two AC3 "
                     "'eng' streams in the service, the first stream "
                     "could be identified using number 1 and the "
                     "second using number 2."),
      .off      = offsetof(esfilter_t, esf_sindex),
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

const idclass_t esfilter_class_teletext = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_teletext",
  .ic_caption    = N_("Teletext Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("Teletext stream type is only available for "
                     "this filter."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_teletext,
      .list     = esfilter_class_type_enum_teletext,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("The language the filter should apply to."),
      .get      = esfilter_class_language_get,
      .set      = esfilter_class_language_set,
      .list     = esfilter_class_language_enum,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "sindex",
      .name     = N_("Stream index"),
      .desc     = N_("The logical stream index to compare. Note that "
                     "this index is computed using all filters."
                     "Example: If filter is set to AC3 audio type and "
                     "the language to 'eng' and there are two AC3 "
                     "'eng' streams in the service, the first stream "
                     "could be identified using number 1 and the "
                     "second using number 2."),
      .off      = offsetof(esfilter_t, esf_sindex),
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

const idclass_t esfilter_class_subtit = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_subtit",
  .ic_caption    = N_("Subtitle Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("The subtitle stream types the filter should "
                     "apply to."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_subtit,
      .list     = esfilter_class_type_enum_subtit,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("The language the filter should apply to."),
      .get      = esfilter_class_language_get,
      .set      = esfilter_class_language_set,
      .list     = esfilter_class_language_enum,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "sindex",
      .name     = N_("Stream index"),
      .desc     = N_("The logical stream index to compare. Note that "
                     "this index is computed using all filters."
                     "Example: If filter is set to AC3 audio type and "
                     "the language to 'eng' and there are two AC3 "
                     "'eng' streams in the service, the first stream "
                     "could be identified using number 1 and the "
                     "second using number 2."),
      .off      = offsetof(esfilter_t, esf_sindex),
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

const idclass_t esfilter_class_ca = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_ca",
  .ic_caption    = N_("CA Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("The CA stream type is only available for this "
                     "filter."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_ca,
      .list     = esfilter_class_type_enum_ca,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "CAid",
      .name     = N_("CA identification"),
      .desc     = N_("The CAID to compare. Leave blank to apply to "
                     "all IDs."),
      .get      = esfilter_class_caid_get,
      .set      = esfilter_class_caid_set,
      .list     = esfilter_class_caid_enum,
    },
    {
      .type     = PT_STR,
      .id       = "CAprovider",
      .name     = N_("CA provider"),
      .desc     = N_("The CA provider to compare. Leave blank to apply "
                     "to all providers."),
      .get      = esfilter_class_caprovider_get,
      .set      = esfilter_class_caprovider_set,
      .list     = esfilter_class_caprovider_enum,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "sindex",
      .name     = N_("Stream index"),
      .desc     = N_("The logical stream index to compare. Note that "
                     "this index is computed using all filters."
                     "Example: If filter is set to AC3 audio type and "
                     "the language to 'eng' and there are two AC3 "
                     "'eng' streams in the service, the first stream "
                     "could be identified using number 1 and the "
                     "second using number 2."),
      .off      = offsetof(esfilter_t, esf_sindex),
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

const idclass_t esfilter_class_other = {
  .ic_super      = &esfilter_class,
  .ic_class      = "esfilter_other",
  .ic_caption    = N_("Other Stream Filter"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "type",
      .name     = N_("Stream type"),
      .desc     = N_("The MPEGTS stream type is only available for "
                     "this filter."),
      .get      = esfilter_class_type_get,
      .set      = esfilter_class_type_set_other,
      .list     = esfilter_class_type_enum_other,
      .rend     = esfilter_class_type_rend,
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("The language the filter should apply to."),
      .get      = esfilter_class_language_get,
      .set      = esfilter_class_language_set,
      .list     = esfilter_class_language_enum,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "service",
      .name     = N_("Service"),
      .desc     = N_("The service the filter should apply to. "
                     "Leave blank to apply the filter to all "
                     "services."),
      .get      = esfilter_class_service_get,
      .set      = esfilter_class_service_set,
      .list     = esfilter_class_service_enum,
    },
    {
      .type     = PT_INT,
      .id       = "pid",
      .name     = N_("PID"),
      .desc     = N_("Program identification (PID) number to compare. "
                     "Zero means any. This comparison is processed "
                     "only when service comparison is active and for "
                     "the Conditional Access filter."),
      .off      = offsetof(esfilter_t, esf_pid),
    },
    {
      .type     = PT_INT,
      .id       = "action",
      .name     = N_("Action"),
      .desc     = N_("The rule action defines the operation when all "
                     "comparisons succeed. See Help for more "
                     "information on what the various rules do."),
      .get      = esfilter_class_action_get,
      .set      = esfilter_class_action_set,
      .list     = esfilter_class_action_enum,
      .opts     = PO_DOC_NLIST,
      .doc      = prop_doc_action,
    },
    {
      .type     = PT_BOOL,
      .id       = "log",
      .name     = N_("Log"),
      .desc     = N_("Write a short message to log identifying the "
                     "matched parameters. It is useful for debugging "
                     "your setup or structure of incoming streams."),
      .off      = offsetof(esfilter_t, esf_log),
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-format text field. Enter whatever you "
                     "like here."),
      .off      = offsetof(esfilter_t, esf_comment),
    },
    {}
  }
};

/**
 *  Initialize
 */
void
esfilter_init(void)
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i;

  for (i = 0; i <= ESF_CLASS_LAST; i++) {
    TAILQ_INIT(&esfilters[i]);
    if (esfilter_classes[i])
      idclass_register(esfilter_classes[i]);
  }

  if (!(c = hts_settings_load("esfilter")))
    return;
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f)))
      continue;
    esfilter_create(-1, f->hmf_name, e, 0);
  }
  htsmsg_destroy(c);

  for (i = 0; i <= ESF_CLASS_LAST; i++)
    esfilter_reindex(i);
}

void
esfilter_done(void)
{
  esfilter_t *esf;
  int i;

  pthread_mutex_lock(&global_lock);
  for (i = 0; i <= ESF_CLASS_LAST; i++) {
    while ((esf = TAILQ_FIRST(&esfilters[i])) != NULL)
      esfilter_delete(esf, 0);
  }
  pthread_mutex_unlock(&global_lock);
}
