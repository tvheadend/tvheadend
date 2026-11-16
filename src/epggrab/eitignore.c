/*
 *  tvheadend, EIT ignore rules
 *  Copyright (C) 2026 Michael Utech
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
#include "channels.h"
#include "dvr/dvr.h"
#include "epggrab/eitignore.h"

struct eitignore_queue eitignores;

static int eitignore_has_criteria(const eitignore_t *ei);
static void eitignore_destroy(eitignore_t *ei, int delconf);

/* **************************************************************************
 * Rule management
 * **************************************************************************/

static void
eitignore_regfree(eitignore_t *ei)
{
  if (ei->ei_title) {
    if (!ei->ei_error)
      regex_free(&ei->ei_title_regex);
    free(ei->ei_title);
    ei->ei_title = NULL;
  }
  ei->ei_error = 0;
}

eitignore_t *
eitignore_create(const char *uuid, htsmsg_t *conf)
{
  eitignore_t *ei;

  lock_assert(&global_lock);

  ei = calloc(1, sizeof(*ei));
  if (ei == NULL)
    return NULL;
  ei->ei_enabled = 1;
  LIST_INIT(&ei->ei_channels);
  if (idnode_insert(&ei->ei_id, uuid, &eitignore_class, 0)) {
    if (uuid)
      tvherror(LS_EPGGRAB, "invalid EIT ignore rule uuid '%s'", uuid);
    free(ei);
    return NULL;
  }
  TAILQ_INSERT_TAIL(&eitignores, ei, ei_link);
  if (conf)
    idnode_load(&ei->ei_id, conf);
  /* A new rule without criteria would match nothing; refuse it. */
  if (uuid == NULL && !eitignore_has_criteria(ei)) {
    tvherror(LS_EPGGRAB, "EIT ignore rule without channel, title or duration refused");
    eitignore_destroy(ei, 0);
    return NULL;
  }
  if (uuid == NULL && ei->ei_nochannel) {
    eitignore_destroy(ei, 0);
    return NULL;
  }
  return ei;
}

static void
eitignore_destroy(eitignore_t *ei, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&ei->ei_id, delconf);
  if (delconf)
    hts_settings_remove("epggrab/eitignore/%s", idnode_uuid_as_str(&ei->ei_id, ubuf));
  TAILQ_REMOVE(&eitignores, ei, ei_link);
  idnode_list_destroy(&ei->ei_channels, ei);
  idnode_unlink(&ei->ei_id);
  eitignore_regfree(ei);
  free(ei->ei_name);
  free(ei->ei_lang);
  free(ei->ei_comment);
  htsmsg_destroy(ei->ei_nochannel_uuids);
  free(ei);
}

/* **************************************************************************
 * Matching
 * **************************************************************************/

/*
 * A rule without any criterion would ignore every event.
 */
static int
eitignore_has_criteria(const eitignore_t *ei)
{
  return !LIST_EMPTY(&ei->ei_channels) ||
         ei->ei_title != NULL ||
         ei->ei_minduration > 0 ||
         ei->ei_maxduration > 0;
}

static int
eitignore_match_text(eitignore_t *ei, const lang_str_t *ls)
{
  lang_str_ele_t *e;

  if (!ls)
    return 0;
  RB_FOREACH(e, (lang_str_t *)ls, link) {
    if (!strempty(ei->ei_lang) && strcmp(e->lang, ei->ei_lang))
      continue;
    if (!regex_match(&ei->ei_title_regex, e->str))
      return 1;
  }
  return 0;
}

/*
 * The title criterion: the regex matches the title, or with full-text
 * also the summary or the description. A rule without title passes.
 */
static int
eitignore_match_title(eitignore_t *ei,
                      const lang_str_t *title,
                      const lang_str_t *summary,
                      const lang_str_t *desc)
{
  if (ei->ei_title == NULL)
    return 1;
  if (eitignore_match_text(ei, title))
    return 1;
  return ei->ei_fulltext &&
         (eitignore_match_text(ei, summary) || eitignore_match_text(ei, desc));
}

/*
 * The channel criterion: no channels, or ch is one of them.
 */
static int
eitignore_match_channel(const eitignore_t *ei, const channel_t *ch)
{
  const idnode_list_mapping_t *ilm;

  if (LIST_EMPTY(&ei->ei_channels))
    return 1;
  if (ch == NULL)
    return 0;
  LIST_FOREACH(ilm, &ei->ei_channels, ilm_in1_link)
    if (ilm->ilm_in2 == &ch->ch_id)
      return 1;
  return 0;
}

eitignore_t *
eitignore_match(const channel_t *ch,
                const lang_str_t *title,
                const lang_str_t *summary,
                const lang_str_t *desc,
                time_t start, time_t stop)
{
  eitignore_t *ei;
  const int64_t duration = stop - start;

  lock_assert(&global_lock);

  TAILQ_FOREACH(ei, &eitignores, ei_link) {
    if (!ei->ei_enabled || ei->ei_error || ei->ei_nochannel ||
        !eitignore_has_criteria(ei))
      continue;
    if (!eitignore_match_channel(ei, ch))
      continue;
    if (ei->ei_minduration > 0 && duration < ei->ei_minduration)
      continue;
    if (ei->ei_maxduration > 0 && duration > ei->ei_maxduration)
      continue;
    if (!eitignore_match_title(ei, title, summary, desc))
      continue;
    return ei;
  }
  return NULL;
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static htsmsg_t *
eitignore_class_save(idnode_t *self, char *filename, size_t fsize)
{
  eitignore_t *ei = (eitignore_t *)self;
  htsmsg_t *m = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];

  idnode_save(&ei->ei_id, m);
  if (filename)
    snprintf(filename, fsize, "epggrab/eitignore/%s",
             idnode_uuid_as_str(&ei->ei_id, ubuf));
  return m;
}

static void
eitignore_class_delete(idnode_t *self)
{
  eitignore_destroy((eitignore_t *)self, 1);
}

static void
eitignore_class_get_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  const eitignore_t *ei = (const eitignore_t *)self;
  const char *s = "";

  (void)lang;
  if (!strempty(ei->ei_name))
    s = ei->ei_name;
  else if (!strempty(ei->ei_title))
    s = ei->ei_title;
  else if (!strempty(ei->ei_comment))
    s = ei->ei_comment;
  snprintf(dst, dstsize, "%s", s);
}

static int
eitignore_class_title_set(void *o, const void *v)
{
  eitignore_t *ei = (eitignore_t *)o;
  const char *title = v ?: "";

  if (!strcmp(title, ei->ei_title ?: ""))
    return 0;
  eitignore_regfree(ei);
  if (*title) {
    ei->ei_title = strdup(title);
    if (regex_compile(&ei->ei_title_regex, title, TVHREGEX_CASELESS, LS_EPGGRAB))
      ei->ei_error = 1;
  }
  return 1;
}

static int
eitignore_class_channel_set_cb(idnode_t *in1, idnode_t *in2, void *origin)
{
  eitignore_t *ei = (eitignore_t *)in1;
  channel_t *ch = (channel_t *)in2;
  return idnode_list_link(in1, &ei->ei_channels, in2, &ch->ch_eitignores,
                          origin, 1) ? 1 : 0;
}

static int
eitignore_class_channel_set(void *o, const void *v)
{
  eitignore_t *ei = (eitignore_t *)o;
  htsmsg_t *l = (htsmsg_t *)v, *kept;
  int nochannel, save = idnode_list_set1(&ei->ei_id, &ei->ei_channels,
                                         &channel_class, l,
                                         eitignore_class_channel_set_cb);
  /* An empty list means all channels: never widen a rule whose
   * channels do not exist. Keep their uuids, so the rule is saved with
   * them and stays unused after a restart. */
  nochannel = l && !htsmsg_is_empty(l) && LIST_EMPTY(&ei->ei_channels);
  kept = nochannel ? htsmsg_copy(l) : NULL;
  if (htsmsg_cmp(ei->ei_nochannel_uuids, kept))
    save = 1;
  htsmsg_destroy(ei->ei_nochannel_uuids);
  ei->ei_nochannel_uuids = kept;
  ei->ei_nochannel = nochannel;
  if (nochannel)
    tvherror(LS_EPGGRAB, "EIT ignore rule '%s': none of its channels exists, rule not used",
             ei->ei_name ?: "");
  return save;
}

static const void *
eitignore_class_channel_get(void *o)
{
  eitignore_t *ei = (eitignore_t *)o;

  if (ei->ei_nochannel_uuids)
    return htsmsg_copy(ei->ei_nochannel_uuids);
  return idnode_list_get1(&ei->ei_channels);
}

static char *
eitignore_class_channel_rend(void *o, const char *lang)
{
  return idnode_list_get_csv1(&((eitignore_t *)o)->ei_channels, lang);
}

static htsmsg_t *
eitignore_class_lang_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();

  (void)o;
  (void)lang;
  htsmsg_add_str(m, "type", "api");
  htsmsg_add_str(m, "uri",  "language/list");
  return m;
}

static htsmsg_t *
eitignore_class_duration_list(void *o, const char *lang)
{
  const char *msg = N_("Any");
  return dvr_entry_class_duration_list(o, tvh_gettext_lang(lang, msg), 24*60, 60, lang);
}

CLASS_DOC(eitignore)

const idclass_t eitignore_class = {
  .ic_class      = "eitignore",
  .ic_caption    = N_("EPG - EIT Ignore Rules"),
  .ic_doc        = tvh_doc_eitignore_class,
  .ic_event      = "eitignore",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = eitignore_class_save,
  .ic_get_title  = eitignore_class_get_title,
  .ic_delete     = eitignore_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the rule."),
      .def.i    = 1,
      .off      = offsetof(eitignore_t, ei_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Name"),
      .desc     = N_("The name of the rule."),
      .off      = offsetof(eitignore_t, ei_name),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channel",
      .name     = N_("Channels"),
      .desc     = N_("The channels to which this rule applies. "
                     "Leave empty to apply the rule to all channels."),
      .set      = eitignore_class_channel_set,
      .get      = eitignore_class_channel_get,
      .rend     = eitignore_class_channel_rend,
      .list     = channel_class_get_list,
    },
    {
      .type     = PT_STR,
      .id       = "title",
      .name     = N_("Title (regexp)"),
      .desc     = N_("The title of the events to ignore. Note that "
                     "this accepts case-insensitive regular expressions."),
      .set      = eitignore_class_title_set,
      .off      = offsetof(eitignore_t, ei_title),
    },
    {
      .type     = PT_BOOL,
      .id       = "fulltext",
      .name     = N_("Full-text"),
      .desc     = N_("Match the title pattern also against the summary "
                     "and the description."),
      .off      = offsetof(eitignore_t, ei_fulltext),
    },
    {
      .type     = PT_STR,
      .id       = "language",
      .name     = N_("Language"),
      .desc     = N_("Match the title pattern only against texts in "
                     "this language. Leave blank for all languages."),
      .off      = offsetof(eitignore_t, ei_lang),
      .list     = eitignore_class_lang_list,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "minduration",
      .name     = N_("Minimum duration"),
      .desc     = N_("Only match events that are no shorter than this "
                     "duration."),
      .list     = eitignore_class_duration_list,
      .off      = offsetof(eitignore_t, ei_minduration),
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "maxduration",
      .name     = N_("Maximum duration"),
      .desc     = N_("Only match events that are no longer than this "
                     "duration."),
      .list     = eitignore_class_duration_list,
      .off      = offsetof(eitignore_t, ei_maxduration),
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .off      = offsetof(eitignore_t, ei_comment),
    },
    {}
  }
};

void
eitignore_destroy_by_channel(channel_t *ch, int delconf)
{
  idnode_list_mapping_t *ilm;
  eitignore_t *ei;

  while ((ilm = LIST_FIRST(&ch->ch_eitignores)) != NULL) {
    ei = (eitignore_t *)ilm->ilm_in1;
    idnode_list_unlink(ilm, delconf ? ch : NULL);
    /* A rule limited to channels must not widen to all channels. */
    if (LIST_EMPTY(&ei->ei_channels))
      eitignore_destroy(ei, delconf);
  }
}

/* **************************************************************************
 * Init / done
 * **************************************************************************/

void
eitignore_init(void)
{
  htsmsg_t *c;
  htsmsg_t *m;
  htsmsg_field_t *f;

  TAILQ_INIT(&eitignores);
  idclass_register(&eitignore_class);

  if ((c = hts_settings_load("epggrab/eitignore")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)eitignore_create(htsmsg_field_name(f), m);
    }
    htsmsg_destroy(c);
  }
}

void
eitignore_done(void)
{
  eitignore_t *ei;

  tvh_mutex_lock(&global_lock);
  while ((ei = TAILQ_FIRST(&eitignores)) != NULL)
    eitignore_destroy(ei, 0);
  tvh_mutex_unlock(&global_lock);
}
