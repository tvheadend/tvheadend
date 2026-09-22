/*
 *  API - DVR
 *
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
#include "dvr/dvr.h"
#include "dvr/dvr_autorec_expr.h"
#include "lang_codes.h"
#include "epg.h"
#include "api.h"

/*
 *
 */

static void
api_dvr_config_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_config_t *cfg;

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    if (!idnode_perm((idnode_t *)cfg, perm, NULL)) {
      idnode_set_add(ins, (idnode_t*)cfg, &conf->filter, perm->aa_lang_ui);
      idnode_perm_unset((idnode_t *)cfg);
    }
}

static int
api_dvr_config_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_config_t *cfg;
  htsmsg_t *conf;
  const char *s;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;
  if (!(s = htsmsg_get_str(conf, "name")))
    return EINVAL;
  if (s[0] == '\0')
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  if ((cfg = dvr_config_create(NULL, NULL, conf))) {
    api_idnode_create(resp, &cfg->dvr_id);
    dvr_config_changed(cfg);
  }
  tvh_mutex_unlock(&global_lock);

  return 0;
}

static void
api_dvr_entry_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
}

static void
api_dvr_entry_grid_upcoming
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;
  int duplicates = htsmsg_get_s32_or_default(args, "duplicates", 1);

  if (duplicates) {
    LIST_FOREACH(de, &dvrentries, de_global_link)
      if (dvr_entry_is_upcoming(de))
        idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
  } else {
    LIST_FOREACH(de, &dvrentries, de_global_link)
      if (dvr_entry_is_upcoming_nodup(de))
        idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
  }
}

static void
api_dvr_entry_grid_finished
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (dvr_entry_is_finished(de, DVR_FINISHED_SUCCESS))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
}

static void
api_dvr_entry_grid_failed
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (dvr_entry_is_finished(de, DVR_FINISHED_FAILED))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
}

static void
api_dvr_entry_grid_removed
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_entry_t *de;

  LIST_FOREACH(de, &dvrentries, de_global_link)
    if (dvr_entry_is_finished(de, DVR_FINISHED_REMOVED_SUCCESS | DVR_FINISHED_REMOVED_FAILED))
      idnode_set_add(ins, (idnode_t*)de, &conf->filter, perm->aa_lang_ui);
}

static int
api_dvr_entry_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_entry_t *de;
  dvr_config_t *cfg;
  htsmsg_t *conf, *m;
  char *s, *lang;
  const char *s1;
  int res = EPERM;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  s1 = htsmsg_get_str(conf, "config_name");
  cfg = dvr_config_find_by_list(perm->aa_dvrcfgs, s1);
  if (cfg) {
    htsmsg_set_uuid(conf, "config_name", &cfg->dvr_id.in_uuid);
    htsmsg_set_str2(conf, "owner", perm->aa_username);
    htsmsg_set_str2(conf, "creator", perm->aa_representative);

    lang = access_get_lang(perm, htsmsg_get_str(conf, "lang"));
    if (lang) {
      for (s = (char *)lang; *s && *s != ','; s++);
      *s = '\0';
    } else {
      lang = strdup(lang_code_preferred());
    }

    s1 = htsmsg_get_str(conf, "disp_title");
    if (s1 && !htsmsg_get_map(conf, "title")) {
      m = htsmsg_create_map();
      htsmsg_add_str(m, lang, s1);
      htsmsg_add_msg(conf, "title", m);
    }

    s1 = htsmsg_get_str(conf, "disp_subtitle");
    if (s1 == NULL || s1[0] == '\0')
      s1 = htsmsg_get_str(conf, "disp_extratext");
    if (s1 && !htsmsg_get_map(conf, "subtitle")) {
      m = htsmsg_create_map();
      htsmsg_add_str(m, lang, s1);
      htsmsg_add_msg(conf, "subtitle", m);
    }
    if ((de = dvr_entry_create(NULL, conf, 0)))
      api_idnode_create(resp, &de->de_id);

    res = 0;
    free(lang);
  }
  tvh_mutex_unlock(&global_lock);

  return res;
}

static htsmsg_t *
api_dvr_entry_create_from_single(htsmsg_t *args)
{
  htsmsg_t *entries, *m;
  const char *s1, *s2, *s3;

  if (!(s1 = htsmsg_get_str(args, "config_uuid")))
    return NULL;
  if (!(s2 = htsmsg_get_str(args, "event_id")))
    return NULL;
  s3 = htsmsg_get_str(args, "comment");
  entries = htsmsg_create_list();
  m = htsmsg_create_map();
  htsmsg_add_str(m, "config_uuid", s1);
  htsmsg_add_str(m, "event_id", s2);
  if (s3)
    htsmsg_add_str(m, "comment", s3);
  htsmsg_add_msg(entries, NULL, m);
  return entries;
}

static int
api_dvr_entry_create_by_event
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_entry_t *de;
  epg_broadcast_t *e;
  htsmsg_t *entries, *entries2 = NULL, *m, *l = NULL, *conf;
  htsmsg_field_t *f;
  const char *s, *config_uuid;
  int count = 0;

  if (!(entries = htsmsg_get_list(args, "entries"))) {
    entries = entries2 = api_dvr_entry_create_from_single(args);
    if (!entries)
      return EINVAL;
  }

  HTSMSG_FOREACH(f, entries) {
    if (!(m = htsmsg_get_map_by_field(f))) continue;

    if (!(s = htsmsg_get_str(m, "event_id")))
      continue;

    conf = htsmsg_create_map();
    htsmsg_copy_field(conf, "enabled", m, NULL);
    htsmsg_copy_field(conf, "comment", m, NULL);
    htsmsg_add_str2(conf, "owner", perm->aa_username);
    htsmsg_add_str2(conf, "creator", perm->aa_representative);
    config_uuid = htsmsg_get_str(m, "config_uuid");
    de = NULL;

    tvh_mutex_lock(&global_lock);
    if ((e = epg_broadcast_find_by_id(strtoll(s, NULL, 10)))) {
      dvr_config_t *cfg = dvr_config_find_by_list(perm->aa_dvrcfgs, config_uuid);
      if (cfg) {
        htsmsg_add_uuid(conf, "config_name", &cfg->dvr_id.in_uuid);
        de = dvr_entry_create_from_htsmsg(conf, e);
        if (de)
          idnode_changed(&de->de_id);
      }
    }
    tvh_mutex_unlock(&global_lock);

    htsmsg_destroy(conf);

    if (de) {
      if (l == NULL)
        l = htsmsg_create_list();
      htsmsg_add_uuid(l, NULL, &de->de_id.in_uuid);
    }

    count++;
  }

  htsmsg_destroy(entries2);

  api_idnode_create_list(resp, l);

  return !count ? EINVAL : 0;
}

static void
api_dvr_rerecord_toggle(access_t *perm, idnode_t *self)
{
  dvr_entry_set_rerecord((dvr_entry_t *)self, -1);
}

static int
api_dvr_entry_rerecord_toggle
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_rerecord_toggle, "rerecord", 0);
}

static void
api_dvr_rerecord_deny(access_t *perm, idnode_t *self)
{
  dvr_entry_set_rerecord((dvr_entry_t *)self, 0);
}

static int
api_dvr_entry_rerecord_deny
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_rerecord_deny, "rerecord", 0);
}

static void
api_dvr_rerecord_allow(access_t *perm, idnode_t *self)
{
  dvr_entry_set_rerecord((dvr_entry_t *)self, 1);
}

static int
api_dvr_entry_rerecord_allow
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_rerecord_allow, "rerecord", 0);
}

static void
api_dvr_stop(access_t *perm, idnode_t *self)
{
  dvr_entry_stop((dvr_entry_t *)self);
}

static int
api_dvr_entry_stop
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_stop, "stop", 0);
}

static void
api_dvr_cancel(access_t *perm, idnode_t *self)
{
  dvr_entry_cancel((dvr_entry_t *)self, 0);
}

static int
api_dvr_entry_cancel
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_cancel, "cancel", 0);
}

static void
api_dvr_remove(access_t *perm, idnode_t *self)
{
  dvr_entry_t *de = (dvr_entry_t *)self;
  if (de->de_sched_state != DVR_SCHEDULED && de->de_sched_state != DVR_NOSTATE)
    dvr_entry_cancel_remove(de, 0);
}

static int
api_dvr_entry_remove
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_remove, "remove", 0);
}

static void
api_dvr_prevrec_toggle(access_t *perm, idnode_t *self)
{
  dvr_entry_set_prevrec((dvr_entry_t *)self, -1);
}

static int
api_dvr_entry_prevrec_toggle
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_prevrec_toggle, "prevrec", 0);
}

static void
api_dvr_prevrec_unset(access_t *perm, idnode_t *self)
{
  dvr_entry_set_prevrec((dvr_entry_t *)self, 0);
}

static int
api_dvr_entry_prevrec_unset
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_prevrec_unset, "prevrec", 0);
}

static void
api_dvr_prevrec_set(access_t *perm, idnode_t *self)
{
  dvr_entry_set_prevrec((dvr_entry_t *)self, 1);
}

static int
api_dvr_entry_prevrec_set
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_prevrec_set, "prevrec", 0);
}

static void
api_dvr_move_finished(access_t *perm, idnode_t *self)
{
  dvr_entry_move((dvr_entry_t *)self, 0);
}

static int
api_dvr_entry_move_finished
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_move_finished, "move finished", 0);
}

static void
api_dvr_move_failed(access_t *perm, idnode_t *self)
{
  dvr_entry_move((dvr_entry_t *)self, 1);
}

static int
api_dvr_entry_move_failed
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  return api_idnode_handler(&dvr_entry_class, perm, args, resp, api_dvr_move_failed, "move failed", 0);
}

/* The autorec grid is split per view (same pattern as the
 * dvr/entry grid_* family): the default grid returns flat rules only,
 * grid_smart the expression-bearing rest. ExtJS calls the default
 * grid unchanged, so smart entries never reach it. */
static void
api_dvr_autorec_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_autorec_entry_t *dae;

  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    if (!dvr_autorec_entry_is_smart(dae))
      idnode_set_add(ins, (idnode_t*)dae, &conf->filter, perm->aa_lang_ui);
}

static void
api_dvr_autorec_grid_smart
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_autorec_entry_t *dae;

  TAILQ_FOREACH(dae, &autorec_entries, dae_link)
    if (dvr_autorec_entry_is_smart(dae))
      idnode_set_add(ins, (idnode_t*)dae, &conf->filter, perm->aa_lang_ui);
}

/* Match-all safeguard: a smart rule matching more than this
 * share of the scanned EPG is rejected at create time unless the
 * request carries a force flag. The gate is skipped when the scanned
 * population is too small to make the ratio meaningful. */
#define AUTOREC_GATE_MATCH_PERCENT  50
#define AUTOREC_GATE_MIN_SCANNED    100

static void
api_dvr_autorec_scan_count
  ( dvr_autorec_entry_t *dae, uint32_t *matchedp, uint32_t *scannedp )
{
  channel_t *ch;
  epg_broadcast_t *e;

  *matchedp = *scannedp = 0;
  CHANNEL_FOREACH(ch) {
    if (!ch->ch_enabled) continue;
    RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
      (*scannedp)++;
      if (dvr_autorec_cmp(dae, e))
        (*matchedp)++;
    }
  }
}

static int
api_dvr_autorec_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  dvr_config_t *cfg;
  dvr_autorec_entry_t *dae;
  const char *s1, *expr;
  uint32_t matched, scanned;
  int force;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;
  force = htsmsg_get_bool_or_default(args, "force", 0);

  htsmsg_set_str2(conf, "owner", perm->aa_username);
  htsmsg_set_str2(conf, "creator", perm->aa_representative);

  s1 = htsmsg_get_str(conf, "config_uuid");
  if (s1 == NULL)
    s1 = htsmsg_get_str(conf, "config_name");

  tvh_mutex_lock(&global_lock);
  cfg = dvr_config_find_by_list(perm->aa_dvrcfgs, s1);
  if (cfg) {
    htsmsg_set_uuid(conf, "config_name", &cfg->dvr_id.in_uuid);
    expr = htsmsg_get_str(conf, "expression");
    if (expr && expr[0] != '\0' && !force) {
      dae = dvr_autorec_create_transient(conf);
      if (dae == NULL) {
        tvh_mutex_unlock(&global_lock);
        return EINVAL;
      }
      dae->dae_enabled = 1;
      api_dvr_autorec_scan_count(dae, &matched, &scanned);
      dvr_autorec_destroy_transient(dae);
      if (scanned >= AUTOREC_GATE_MIN_SCANNED &&
          (uint64_t)matched * 100 > (uint64_t)scanned * AUTOREC_GATE_MATCH_PERCENT) {
        tvh_mutex_unlock(&global_lock);
        *resp = htsmsg_create_map();
        htsmsg_add_str(*resp, "error",
                       "expression matches most of the EPG; "
                       "resubmit with force to save anyway");
        htsmsg_add_u32(*resp, "force_required", 1);
        htsmsg_add_u32(*resp, "matched", matched);
        htsmsg_add_u32(*resp, "scanned", scanned);
        return 0;
      }
    }
    dae = dvr_autorec_create(NULL, conf);
    if (dae) {
      api_idnode_create(resp, &dae->dae_id);
      dvr_autorec_changed(dae, 0);
      dvr_autorec_completed(dae, 0);
    }
  }
  tvh_mutex_unlock(&global_lock);

  return 0;
}

/*
 * Preview which EPG events an autorec rule (saved or not) would match,
 * without saving anything. The payload is the same "conf" object as
 * dvr/autorec/create; the scan runs the real matcher over a transient
 * entry, so the preview cannot drift from actual matching behaviour.
 * The "Duplicate handling" layer is previewed too: entries the rule
 * would create but recording-start dedup would skip carry duplicate=1.
 */
static int
api_dvr_autorec_preview_start_cmp(const void *a, const void *b)
{
  const epg_broadcast_t *e1 = *(epg_broadcast_t *const *)a;
  const epg_broadcast_t *e2 = *(epg_broadcast_t *const *)b;
  if (e1->start != e2->start)
    return e1->start < e2->start ? -1 : 1;
  return 0;
}

static int
api_dvr_autorec_preview
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf, *entries, *m;
  dvr_config_t *cfg;
  dvr_autorec_entry_t *dae, *existing;
  dvr_autorec_expr_t *comp;
  dvr_autorec_dedup_scan_t *das;
  dvr_entry_t *de;
  channel_t *ch;
  epg_broadcast_t *e, **cands, **grown;
  const char *s1, *expr, *disposition;
  char errbuf[512], ubuf[UUID_HEX_SIZE];
  uint32_t scanned = 0, matched = 0, sched = 0, maxsched, listed = 0;
  int ncand = 0, acand = 0, ci, duplicate;
  int64_t limit;

  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;
  limit = htsmsg_get_s64_or_default(args, "limit", 0);

  /* Pre-validate the expression so the editor gets the parse error
   * text; a real save has no error channel and can only flag. */
  expr = htsmsg_get_str(conf, "expression");
  if (expr && expr[0] != '\0') {
    comp = dvr_autorec_expr_compile(expr, errbuf, sizeof(errbuf));
    if (comp == NULL) {
      *resp = htsmsg_create_map();
      htsmsg_add_str(*resp, "error", errbuf);
      return 0;
    }
    dvr_autorec_expr_free(comp);
  }

  htsmsg_set_str2(conf, "owner", perm->aa_username);
  htsmsg_set_str2(conf, "creator", perm->aa_representative);

  s1 = htsmsg_get_str(conf, "config_uuid");
  if (s1 == NULL)
    s1 = htsmsg_get_str(conf, "config_name");

  tvh_mutex_lock(&global_lock);
  cfg = dvr_config_find_by_list(perm->aa_dvrcfgs, s1);
  if (cfg == NULL) {
    tvh_mutex_unlock(&global_lock);
    return EPERM;
  }
  htsmsg_set_uuid(conf, "config_name", &cfg->dvr_id.in_uuid);
  dae = dvr_autorec_create_transient(conf);
  if (dae == NULL) {
    tvh_mutex_unlock(&global_lock);
    return EINVAL;
  }
  if (dae->dae_error) {
    /* With the expression pre-validated above, the only remaining
     * dae_error producer is the title regex failing to compile. */
    dvr_autorec_destroy_transient(dae);
    tvh_mutex_unlock(&global_lock);
    *resp = htsmsg_create_map();
    htsmsg_add_str(*resp, "error", "invalid title regular expression");
    return 0;
  }
  /* the preview answers "what would this rule match when enabled" */
  dae->dae_enabled = 1;

  /* When previewing an edit, the rule's own live schedules count
   * toward the max schedules limit, as they would on a real save. */
  maxsched = dvr_autorec_get_max_sched_count(dae);
  s1 = htsmsg_get_str(conf, "uuid");
  existing = s1 ? dvr_autorec_find_by_uuid(s1) : NULL;
  if (existing) {
    LIST_FOREACH(de, &existing->dae_spawns, de_autorec_link)
      if (de->de_sched_state == DVR_SCHEDULED ||
          de->de_sched_state == DVR_RECORDING)
        sched++;
  }

  /* Collect the matches first: the "Duplicate handling" scan needs
   * its candidates in recording-start order, and the channel walk
   * yields them channel by channel. */
  entries = htsmsg_create_list();
  cands = NULL;
  CHANNEL_FOREACH(ch) {
    if (!ch->ch_enabled) continue;
    RB_FOREACH(e, &ch->ch_epg_schedule, sched_link) {
      scanned++;
      if (!dvr_autorec_cmp(dae, e)) continue;
      matched++;
      if (ncand == acand) {
        acand = acand ? acand * 2 : 128;
        grown = realloc(cands, acand * sizeof(*cands));
        if (grown == NULL)
          goto collected;
        cands = grown;
      }
      cands[ncand++] = e;
    }
  }
collected:
  if (ncand > 1)
    qsort(cands, ncand, sizeof(*cands), api_dvr_autorec_preview_start_cmp);
  das = dvr_autorec_dedup_scan_create(dae, cfg, existing);
  for (ci = 0; ci < ncand; ci++) {
    e = cands[ci];
    ch = e->channel;
    /* The identical-duplicate scan from dvr_entry_create_by_autorec,
     * read-only: an existing entry for the same broadcast (or a
     * matching episode) by the same owner means a save would not
     * schedule this event again. */
    disposition = "record";
    duplicate = 0;
    LIST_FOREACH(de, &dvrentries, de_global_link) {
      if ((de->de_bcast == e || epg_episode_match(de->de_bcast, e)) &&
          strcmp(dae->dae_owner ?: "", de->de_owner ?: "") == 0) {
        disposition = "scheduled";
        break;
      }
    }
    if (disposition[0] == 'r') {
      if (maxsched > 0 && sched >= maxsched)
        disposition = "maxsched";
      else {
        sched++;
        /* The entry would really be created and hold its schedule
         * slot; the "Duplicate handling" layer only decides at
         * recording start whether it records. Flag, don't filter:
         * a duplicate still records if its master fails, and the
         * verdict is about this rule's entry only, another rule
         * may cover the same event either way. */
        duplicate = dvr_autorec_dedup_scan_check(das, e);
        dvr_autorec_dedup_scan_accept(das, e);
      }
    }
    if (limit > 0 && listed >= limit)
      continue;
    listed++;
    m = htsmsg_create_map();
    htsmsg_add_u32(m, "eventId", e->id);
    htsmsg_add_str(m, "channelUuid", idnode_uuid_as_str(&ch->ch_id, ubuf));
    htsmsg_add_str2(m, "channelName", channel_get_name(ch, ""));
    htsmsg_add_str2(m, "title", lang_str_get(e->title, perm->aa_lang_ui));
    htsmsg_add_str2(m, "subtitle", lang_str_get(e->subtitle, perm->aa_lang_ui));
    htsmsg_add_s64(m, "start", e->start);
    htsmsg_add_s64(m, "stop", e->stop);
    htsmsg_add_str(m, "disposition", disposition);
    if (duplicate)
      htsmsg_add_u32(m, "duplicate", 1);
    htsmsg_add_msg(entries, NULL, m);
  }
  dvr_autorec_dedup_scan_destroy(das);
  free(cands);
  dvr_autorec_destroy_transient(dae);
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", entries);
  htsmsg_add_u32(*resp, "matched", matched);
  htsmsg_add_u32(*resp, "scanned", scanned);
  if (matched > listed)
    htsmsg_add_u32(*resp, "truncated", 1);
  /* the create gate's verdict, precomputed for the editor: the hard
   * gate covers creates only, the Vue edit-save flow warns
   * voluntarily on the same threshold, and serving the flag here
   * keeps the constants in one place */
  if (scanned >= AUTOREC_GATE_MIN_SCANNED &&
      (uint64_t)matched * 100 > (uint64_t)scanned * AUTOREC_GATE_MATCH_PERCENT)
    htsmsg_add_u32(*resp, "matchall", 1);
  return 0;
}

/*
 * Convert a flat rule into its exact smart equivalent. dry_run
 * returns the generated expression and warnings without touching the
 * rule; apply first leaves a disabled backup copy of the rule behind
 * (suppressed by backup=0; the copy's uuid is returned as "backup"),
 * then clears the flat selector fields and stores the expression. The clearing must happen server-side and first: the
 * dynamic write masking keys on a non-empty expression (and writes the
 * expression property before the selectors in table order), and
 * serieslink is read-only at the API surface at all times. Clearing
 * the flat fields is what keeps the no-constraint HTSP encoding
 * true for converted rules.
 */
static int
api_dvr_autorec_convert
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_autorec_entry_t *dae, *bdae;
  htsmsg_t *warnings, *m, *days, *bconf;
  const char *uuid, *oname;
  char *expr, *bname;
  char ubuf[UUID_HEX_SIZE];
  tvh_uuid_t buuid;
  size_t blen;
  int dry_run, backup, have_backup = 0, i;

  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;
  dry_run = htsmsg_get_bool_or_default(args, "dry_run", 0);
  backup = htsmsg_get_bool_or_default(args, "backup", 1);

  tvh_mutex_lock(&global_lock);
  dae = dvr_autorec_find_by_uuid(uuid);
  if (dae == NULL) {
    tvh_mutex_unlock(&global_lock);
    return ENOENT;
  }
  if (dvr_autorec_entry_verify(dae, perm, 0)) {
    tvh_mutex_unlock(&global_lock);
    return EPERM;
  }
  if (dvr_autorec_entry_is_smart(dae)) {
    tvh_mutex_unlock(&global_lock);
    *resp = htsmsg_create_map();
    htsmsg_add_str(*resp, "error", "the rule is already a smart entry");
    return 0;
  }

  warnings = htsmsg_create_list();
  expr = dvr_autorec_expr_from_flat(dae, warnings);
  if (expr == NULL) {
    htsmsg_field_t *wf = TAILQ_FIRST(&warnings->hm_fields);
    const char *why = wf ? htsmsg_field_get_string(wf) : NULL;
    tvh_mutex_unlock(&global_lock);
    *resp = htsmsg_create_map();
    htsmsg_add_str(*resp, "error",
                   why ?: "the rule has no convertible matching conditions");
    htsmsg_add_msg(*resp, "warnings", warnings);
    return 0;
  }

  if (!dry_run) {
    /* Phase 0: leave a disabled backup copy behind, the revert path
     * for a conversion that has no undo. The config round-trips
     * through idnode_save exactly like a disk load, so the
     * always-PO_RDONLY serieslink survives the copy; born disabled,
     * the copy spawns nothing. backup=0 is the escape hatch for
     * bulk tooling. */
    if (backup) {
      bconf = htsmsg_create_map();
      idnode_save(&dae->dae_id, bconf);
      /* set_bool, not set_u32: the field exists as HMF_BOOL from
       * idnode_save, and htsmsg_set_s64 refuses (without writing)
       * on a type mismatch. */
      htsmsg_set_bool(bconf, "enabled", 0);
      oname = htsmsg_get_str(bconf, "name") ?: "";
      blen = strlen(oname) + sizeof(" (converted to smart)");
      bname = malloc(blen);
      snprintf(bname, blen, "%s%s(converted to smart)",
               oname, *oname ? " " : "");
      htsmsg_set_str(bconf, "name", bname);
      free(bname);
      bdae = dvr_autorec_create(NULL, bconf);
      htsmsg_destroy(bconf);
      if (bdae) {
        dvr_autorec_changed(bdae, 0);
        dvr_autorec_completed(bdae, 0);
        idnode_changed(&bdae->dae_id);
        buuid = bdae->dae_id.in_uuid;
        have_backup = 1;
      } else {
        tvhwarn(LS_DVR, "autorec convert: backup copy creation failed for %s",
                idnode_uuid_as_str(&dae->dae_id, ubuf));
      }
    }

    /* Phase 1: clear every flat selector while the expression is
     * still empty. Mask-free internal write (optmask 0) so the
     * always-PO_RDONLY serieslink resets along with the rest. */
    m = htsmsg_create_map();
    htsmsg_add_str(m, "title", "");
    htsmsg_add_bool(m, "fulltext", 0);
    htsmsg_add_bool(m, "mergetext", 0);
    htsmsg_add_str(m, "channel", "");
    htsmsg_add_str(m, "tag", "");
    htsmsg_add_u32(m, "btype", DVR_AUTOREC_BTYPE_ALL);
    htsmsg_add_u32(m, "content_type", 0);
    htsmsg_add_str(m, "cat1", "");
    htsmsg_add_str(m, "cat2", "");
    htsmsg_add_str(m, "cat3", "");
    htsmsg_add_u32(m, "star_rating", 0);
    htsmsg_add_str(m, "start", "");
    htsmsg_add_str(m, "start_window", "");
    htsmsg_add_s64(m, "minduration", 0);
    htsmsg_add_s64(m, "maxduration", 0);
    htsmsg_add_u32(m, "minyear", 0);
    htsmsg_add_u32(m, "maxyear", 0);
    htsmsg_add_u32(m, "minseason", 0);
    htsmsg_add_u32(m, "maxseason", 0);
    days = htsmsg_create_list();
    for (i = 1; i <= 7; i++)
      htsmsg_add_u32(days, NULL, i);
    htsmsg_add_msg(m, "weekdays", days);
    idnode_write0(&dae->dae_id, m, 0, 0);
    htsmsg_destroy(m);
    free((void *)dae->dae_serieslink_uri);
    dae->dae_serieslink_uri = NULL;

    /* Phase 2: store the expression; dosave fires the class changed
     * hook (spawn rescan, HTSP update) and persists. */
    m = htsmsg_create_map();
    htsmsg_add_str(m, "expression", expr);
    idnode_write0(&dae->dae_id, m, 0, 1);
    htsmsg_destroy(m);
  }
  tvh_mutex_unlock(&global_lock);

  *resp = htsmsg_create_map();
  htsmsg_add_str(*resp, "expression", expr);
  htsmsg_add_msg(*resp, "warnings", warnings);
  if (have_backup)
    htsmsg_add_uuid(*resp, "backup", &buuid);
  free(expr);
  return 0;
}

static int
api_dvr_autorec_create_by_series
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  dvr_autorec_entry_t *dae;
  epg_broadcast_t *e;
  htsmsg_t *entries, *entries2 = NULL, *m, *l = NULL;
  htsmsg_field_t *f;
  const char *config_uuid, *s, *title;
  int count = 0;
  char ubuf[UUID_HEX_SIZE];
  const char * msg = " - Created from EPG query";
  char *comment;

  if (!(entries = htsmsg_get_list(args, "entries"))) {
    entries = entries2 = api_dvr_entry_create_from_single(args);
    if (!entries)
      return EINVAL;
  }

  HTSMSG_FOREACH(f, entries) {
    if (!(m = htsmsg_get_map_by_field(f))) continue;

    if (!(s = htsmsg_get_str(m, "event_id")))
      continue;

    config_uuid = htsmsg_get_str(m, "config_uuid");

    tvh_mutex_lock(&global_lock);
    if ((e = epg_broadcast_find_by_id(strtoll(s, NULL, 10)))) {
      dvr_config_t *cfg = dvr_config_find_by_list(perm->aa_dvrcfgs, config_uuid);
      if (cfg) {
        title = epg_broadcast_get_title(e, NULL);
        comment = alloca(strlen(title) + strlen(msg) + 1);
        sprintf(comment, "%s%s", title, msg);
        dae = dvr_autorec_add_series_link(idnode_uuid_as_str(&cfg->dvr_id, ubuf),
                                          e,
                                          perm->aa_username,
                                          perm->aa_representative,
                                          comment);
        if (dae) {
          if (l == NULL)
            l = htsmsg_create_list();
          htsmsg_add_uuid(l, NULL, &dae->dae_id.in_uuid);
          idnode_changed(&dae->dae_id);
        }
      }
    }
    tvh_mutex_unlock(&global_lock);
    count++;
  }

  htsmsg_destroy(entries2);

  api_idnode_create_list(resp, l);

  return !count ? EINVAL : 0;
}

static void
api_dvr_timerec_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  dvr_timerec_entry_t *dte;

  TAILQ_FOREACH(dte, &timerec_entries, dte_link)
    idnode_set_add(ins, (idnode_t*)dte, &conf->filter, perm->aa_lang_ui);
}

static int
api_dvr_timerec_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *conf;
  dvr_timerec_entry_t *dte;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  htsmsg_set_str2(conf, "owner", perm->aa_username);
  htsmsg_set_str2(conf, "creator", perm->aa_representative);

  tvh_mutex_lock(&global_lock);
  dte = dvr_timerec_create(NULL, conf);
  if (dte) {
    api_idnode_create(resp, &dte->dte_id);
    dvr_timerec_check(dte);
  }
  tvh_mutex_unlock(&global_lock);

  return 0;
}

static int
api_dvr_entry_file_moved
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  const char *src, *dst;
  if (!(src = htsmsg_get_str(args, "src")))
    return EINVAL;
  if (!(dst = htsmsg_get_str(args, "dst")))
    return EINVAL;
  if (dvr_entry_file_moved(src, dst))
    return ENOENT;
  return 0;
}


void api_dvr_init ( void )
{
  static api_hook_t ah[] = {
    { "dvr/config/class",          ACCESS_OR|ACCESS_ADMIN|ACCESS_RECORDER,
                                     api_idnode_class, (void*)&dvr_config_class },
    { "dvr/config/grid",           ACCESS_OR|ACCESS_ADMIN|ACCESS_RECORDER,
                                     api_idnode_grid, api_dvr_config_grid },
    { "dvr/config/create",         ACCESS_ADMIN, api_dvr_config_create, NULL },

    { "dvr/entry/class",           ACCESS_RECORDER, api_idnode_class, (void*)&dvr_entry_class },
    { "dvr/entry/grid",            ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid },
    { "dvr/entry/grid_upcoming",   ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_upcoming },
    { "dvr/entry/grid_finished",   ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_finished },
    { "dvr/entry/grid_failed",     ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_failed },
    { "dvr/entry/grid_removed",    ACCESS_RECORDER, api_idnode_grid, api_dvr_entry_grid_removed },
    { "dvr/entry/create",          ACCESS_RECORDER, api_dvr_entry_create, NULL },
    { "dvr/entry/create_by_event", ACCESS_RECORDER, api_dvr_entry_create_by_event, NULL },
    { "dvr/entry/rerecord/toggle", ACCESS_RECORDER, api_dvr_entry_rerecord_toggle, NULL },
    { "dvr/entry/rerecord/deny",   ACCESS_RECORDER, api_dvr_entry_rerecord_deny, NULL },
    { "dvr/entry/rerecord/allow",  ACCESS_RECORDER, api_dvr_entry_rerecord_allow, NULL },
    { "dvr/entry/stop",            ACCESS_RECORDER, api_dvr_entry_stop, NULL },   /* Stop active recording gracefully */
    { "dvr/entry/cancel",          ACCESS_RECORDER, api_dvr_entry_cancel, NULL }, /* Cancel scheduled or active recording */
    { "dvr/entry/prevrec/toggle",  ACCESS_RECORDER, api_dvr_entry_prevrec_toggle, NULL },
    { "dvr/entry/prevrec/set",     ACCESS_RECORDER, api_dvr_entry_prevrec_set, NULL },
    { "dvr/entry/prevrec/unset",   ACCESS_RECORDER, api_dvr_entry_prevrec_unset, NULL },
    { "dvr/entry/remove",          ACCESS_RECORDER, api_dvr_entry_remove, NULL }, /* Remove recorded files from storage */
    { "dvr/entry/filemoved",       ACCESS_ADMIN,    api_dvr_entry_file_moved, NULL },
    { "dvr/entry/move/finished",   ACCESS_RECORDER, api_dvr_entry_move_finished, NULL },
    { "dvr/entry/move/failed",     ACCESS_RECORDER, api_dvr_entry_move_failed, NULL },

    { "dvr/autorec/class",         ACCESS_RECORDER, api_idnode_class, (void*)&dvr_autorec_entry_class },
    { "dvr/autorec/grid",          ACCESS_RECORDER, api_idnode_grid,  api_dvr_autorec_grid },
    { "dvr/autorec/grid_smart",    ACCESS_RECORDER, api_idnode_grid,  api_dvr_autorec_grid_smart },
    { "dvr/autorec/create",        ACCESS_RECORDER, api_dvr_autorec_create, NULL },
    { "dvr/autorec/preview",       ACCESS_RECORDER, api_dvr_autorec_preview, NULL },
    { "dvr/autorec/convert",       ACCESS_RECORDER, api_dvr_autorec_convert, NULL },
    { "dvr/autorec/create_by_series", ACCESS_RECORDER, api_dvr_autorec_create_by_series, NULL },

    { "dvr/timerec/class",         ACCESS_RECORDER, api_idnode_class, (void*)&dvr_timerec_entry_class },
    { "dvr/timerec/grid",          ACCESS_RECORDER, api_idnode_grid,  api_dvr_timerec_grid },
    { "dvr/timerec/create",        ACCESS_RECORDER, api_dvr_timerec_create, NULL },

    { NULL },
  };

  api_register_all(ah);
}
