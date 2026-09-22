/*
 *  tvheadend, smart autorec match expressions
 *  Copyright (C) 2026 Pim Zandbergen
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
#include "channels.h"
#include "epg.h"
#include "htsbuf.h"
#include "htsmsg_json.h"
#include "lang_str.h"
#include "string_list.h"
#include "tvhregex.h"
#include "dvr.h"
#include "dvr_autorec_expr.h"

/*
 * The expression grammar is documented in docs/markdown/inc/dvr_expression.md.
 * Semantics of every leaf mirror the flat matcher (dvr_autorec_cmp) so that
 * each flat rule has an exact smart equivalent. Skipped nodes are pruned
 * here, at compile time; the stored JSONC string keeps them.
 */

typedef enum {
  DAE_EXPR_ALL,
  DAE_EXPR_ANY,
  DAE_EXPR_NOT,
  DAE_EXPR_TEXT,           /* title/subtitle/... regex leaves */
  DAE_EXPR_CHANNEL,
  DAE_EXPR_TAG,
  DAE_EXPR_CHANNEL_NAME,
  DAE_EXPR_CONTENT_TYPE,
  DAE_EXPR_CATEGORY,
  DAE_EXPR_BROADCAST_TYPE,
  DAE_EXPR_SERIESLINK,
  DAE_EXPR_DURATION,
  DAE_EXPR_YEAR,
  DAE_EXPR_SEASON,
  DAE_EXPR_STAR_RATING,
  DAE_EXPR_START,
  DAE_EXPR_WEEKDAYS,
  DAE_EXPR_PRESENT,
} dae_expr_type_t;

/* Text leaf targets */
enum {
  DAE_TXT_TITLE,
  DAE_TXT_SUBTITLE,
  DAE_TXT_SUMMARY,
  DAE_TXT_DESCRIPTION,
  DAE_TXT_CREDITS,
  DAE_TXT_KEYWORD,
  DAE_TXT_MERGEDTEXT,
};

/* Existence leaf targets: exactly the fields that can be absent */
enum {
  DAE_PRES_SUBTITLE,
  DAE_PRES_SUMMARY,
  DAE_PRES_DESCRIPTION,
  DAE_PRES_CREDITS,
  DAE_PRES_KEYWORD,
  DAE_PRES_CATEGORY,
  DAE_PRES_GENRE,
  DAE_PRES_SERIESLINK,
  DAE_PRES_YEAR,
  DAE_PRES_SEASON,
  DAE_PRES_STAR_RATING,
};

enum {
  DAE_BTYPE_NEW,
  DAE_BTYPE_NEW_OR_UNKNOWN,
  DAE_BTYPE_REPEAT,
};

typedef struct dae_expr_node {
  dae_expr_type_t type;
  union {
    struct {
      struct dae_expr_node **elems;
      int count;
    } op;                             /* all, any */
    struct dae_expr_node *child;      /* not */
    struct {
      int field;
      tvh_regex_t regex;
    } text;                           /* text leaves, channel_pattern */
    struct {
      char *uuid;
      char *name;
    } ref;                            /* channel, tag */
    uint32_t content_type;
    char *str;                        /* category, serieslink */
    int btype;
    struct {
      int64_t min, max;
      int has_min, has_max;
    } range;                          /* duration, year, season, star_rating */
    struct {
      int after, before;              /* minutes from midnight */
    } start;
    uint32_t weekdays;                /* bit 0 = Monday, as the flat field */
    int present;
  } u;
} dae_expr_node_t;

struct dvr_autorec_expr {
  dae_expr_node_t *root;              /* NULL: fully pruned, matches nothing */
};

static const struct {
  const char *name;
  int field;
} dae_expr_text_leaves[] = {
  { "title",       DAE_TXT_TITLE },
  { "subtitle",    DAE_TXT_SUBTITLE },
  { "summary",     DAE_TXT_SUMMARY },
  { "description", DAE_TXT_DESCRIPTION },
  { "credits",     DAE_TXT_CREDITS },
  { "keyword",     DAE_TXT_KEYWORD },
  { "mergedtext",  DAE_TXT_MERGEDTEXT },
};

static const struct {
  const char *name;
  int field;
} dae_expr_present_names[] = {
  { "subtitle",    DAE_PRES_SUBTITLE },
  { "summary",     DAE_PRES_SUMMARY },
  { "description", DAE_PRES_DESCRIPTION },
  { "credits",     DAE_PRES_CREDITS },
  { "keyword",     DAE_PRES_KEYWORD },
  { "category",    DAE_PRES_CATEGORY },
  { "genre",       DAE_PRES_GENRE },
  { "serieslink",  DAE_PRES_SERIESLINK },
  { "year",        DAE_PRES_YEAR },
  { "season",      DAE_PRES_SEASON },
  { "star_rating", DAE_PRES_STAR_RATING },
};

/* ************************************************************************
 * Compile
 * ***********************************************************************/

static int
dae_expr_node_count(const dae_expr_node_t *n)
{
  int i, c;
  if (n == NULL)
    return 0;
  c = 1;
  switch (n->type) {
  case DAE_EXPR_ALL:
  case DAE_EXPR_ANY:
    for (i = 0; i < n->u.op.count; i++)
      c += dae_expr_node_count(n->u.op.elems[i]);
    break;
  case DAE_EXPR_NOT:
    c += dae_expr_node_count(n->u.child);
    break;
  default:
    break;
  }
  return c;
}

static void
dae_expr_node_free(dae_expr_node_t *n)
{
  int i;
  if (n == NULL)
    return;
  switch (n->type) {
  case DAE_EXPR_ALL:
  case DAE_EXPR_ANY:
    for (i = 0; i < n->u.op.count; i++)
      dae_expr_node_free(n->u.op.elems[i]);
    free(n->u.op.elems);
    break;
  case DAE_EXPR_NOT:
    dae_expr_node_free(n->u.child);
    break;
  case DAE_EXPR_TEXT:
  case DAE_EXPR_CHANNEL_NAME:
    regex_free(&n->u.text.regex);
    break;
  case DAE_EXPR_CHANNEL:
  case DAE_EXPR_TAG:
    free(n->u.ref.uuid);
    free(n->u.ref.name);
    break;
  case DAE_EXPR_CATEGORY:
  case DAE_EXPR_SERIESLINK:
    free(n->u.str);
    break;
  default:
    break;
  }
  free(n);
}

/*
 * Strip JSONC comments (respecting string literals) and enforce the raw
 * size and nesting depth caps. Depth is checked here, before the JSON
 * deserializer recurses over the text. Returns a malloc'd plain-JSON
 * copy, or NULL with errbuf set.
 */
static char *
dae_expr_preprocess(const char *src, char *errbuf, size_t errlen)
{
  size_t len = strlen(src), i, o = 0, j;
  int depth = 0, in_str = 0, esc = 0;
  char *out, c;

  if (len > DVR_AUTOREC_EXPR_MAX_SIZE) {
    snprintf(errbuf, errlen, "expression exceeds %d bytes",
             DVR_AUTOREC_EXPR_MAX_SIZE);
    return NULL;
  }
  out = malloc(len + 1);
  if (out == NULL) {
    snprintf(errbuf, errlen, "out of memory");
    return NULL;
  }
  for (i = 0; i < len; i++) {
    c = src[i];
    if (in_str) {
      out[o++] = c;
      if (esc)
        esc = 0;
      else if (c == '\\')
        esc = 1;
      else if (c == '"')
        in_str = 0;
      continue;
    }
    if (c == '"') {
      in_str = 1;
      out[o++] = c;
      continue;
    }
    if (c == '/' && i + 1 < len && src[i+1] == '/') {
      while (i < len && src[i] != '\n')
        i++;
      if (i < len)
        out[o++] = '\n';
      continue;
    }
    if (c == '/' && i + 1 < len && src[i+1] == '*') {
      j = i + 2;
      while (j + 1 < len && !(src[j] == '*' && src[j+1] == '/'))
        j++;
      if (j + 1 >= len) {
        snprintf(errbuf, errlen, "unterminated /* comment");
        free(out);
        return NULL;
      }
      i = j + 1;
      out[o++] = ' ';
      continue;
    }
    if (c == '{' || c == '[') {
      if (++depth > DVR_AUTOREC_EXPR_MAX_DEPTH) {
        snprintf(errbuf, errlen, "expression nests deeper than %d levels",
                 DVR_AUTOREC_EXPR_MAX_DEPTH);
        free(out);
        return NULL;
      }
    } else if (c == '}' || c == ']') {
      /* floor at 0: stray closers are the deserializer's error to
       * report, but they must not offset the nesting count */
      if (depth > 0)
        depth--;
    }
    out[o++] = c;
  }
  if (in_str) {
    snprintf(errbuf, errlen, "unterminated string");
    free(out);
    return NULL;
  }
  out[o] = '\0';
  return out;
}

static int
dae_expr_get_int(htsmsg_field_t *f, int64_t *val,
                 const char *leaf, char *errbuf, size_t errlen)
{
  if (htsmsg_field_get_s64(f, val)) {
    snprintf(errbuf, errlen, "\"%s\": \"%s\" must be an integer",
             leaf, htsmsg_field_name(f));
    return -1;
  }
  return 0;
}

static dae_expr_node_t *
dae_expr_parse_node(htsmsg_t *m, int is_root, int depth,
                    int *prunedp, char *errbuf, size_t errlen);

static dae_expr_node_t *
dae_expr_parse_op(htsmsg_field_t *f, dae_expr_type_t type, int depth,
                  int *prunedp, char *errbuf, size_t errlen)
{
  const char *name = htsmsg_field_name(f);
  htsmsg_t *list, *sub;
  htsmsg_field_t *cf;
  dae_expr_node_t *n, *child;
  int count = 0, pruned;

  list = htsmsg_field_get_list(f);
  if (list == NULL) {
    snprintf(errbuf, errlen, "\"%s\" must be an array of nodes", name);
    return NULL;
  }
  HTSMSG_FOREACH(cf, list)
    count++;
  if (count == 0) {
    snprintf(errbuf, errlen, "\"%s\" array must not be empty", name);
    return NULL;
  }
  n = calloc(1, sizeof(*n));
  n->type = type;
  n->u.op.elems = calloc(count, sizeof(n->u.op.elems[0]));
  n->u.op.count = 0;
  HTSMSG_FOREACH(cf, list) {
    sub = htsmsg_field_get_map(cf);
    if (sub == NULL) {
      snprintf(errbuf, errlen, "\"%s\" array elements must be objects", name);
      dae_expr_node_free(n);
      return NULL;
    }
    pruned = 0;
    child = dae_expr_parse_node(sub, 0, depth + 1, &pruned, errbuf, errlen);
    if (child == NULL) {
      if (pruned)
        continue;
      dae_expr_node_free(n);
      return NULL;
    }
    /* count bounds the appends by construction: both loops iterate
     * the same list. The explicit check exists for static analyzers
     * that cannot connect the two iterations; the else branch is
     * unreachable. */
    if (n->u.op.count < count)
      n->u.op.elems[n->u.op.count++] = child;
    else
      dae_expr_node_free(child);
  }
  if (n->u.op.count == 0) {
    /* every child skipped: the pruning cascades to this operator */
    dae_expr_node_free(n);
    *prunedp = 1;
    return NULL;
  }
  return n;
}

static dae_expr_node_t *
dae_expr_parse_ref(htsmsg_field_t *f, dae_expr_type_t type,
                   char *errbuf, size_t errlen)
{
  const char *name = htsmsg_field_name(f);
  const char *uuid = NULL, *refname = NULL, *k;
  htsmsg_t *map;
  htsmsg_field_t *sf;
  dae_expr_node_t *n;

  map = htsmsg_field_get_map(f);
  if (map == NULL) {
    snprintf(errbuf, errlen,
             "\"%s\" must be an object with \"uuid\" and/or \"name\"", name);
    return NULL;
  }
  HTSMSG_FOREACH(sf, map) {
    k = htsmsg_field_name(sf);
    if (!strcmp(k, "uuid"))
      uuid = htsmsg_field_get_string(sf);
    else if (!strcmp(k, "name"))
      refname = htsmsg_field_get_string(sf);
    else {
      snprintf(errbuf, errlen, "\"%s\": unknown key \"%s\"", name, k);
      return NULL;
    }
  }
  if (tvh_str_default(uuid, NULL) == NULL &&
      tvh_str_default(refname, NULL) == NULL) {
    snprintf(errbuf, errlen,
             "\"%s\" needs at least one of \"uuid\" and \"name\"", name);
    return NULL;
  }
  n = calloc(1, sizeof(*n));
  n->type = type;
  n->u.ref.uuid = uuid && uuid[0] ? strdup(uuid) : NULL;
  n->u.ref.name = refname && refname[0] ? strdup(refname) : NULL;
  return n;
}

static dae_expr_node_t *
dae_expr_parse_range(htsmsg_field_t *f, dae_expr_type_t type,
                     char *errbuf, size_t errlen)
{
  const char *name = htsmsg_field_name(f);
  htsmsg_t *map;
  htsmsg_field_t *sf;
  dae_expr_node_t *n;
  int64_t v;
  int64_t min = 0, max = 0;
  int has_min = 0, has_max = 0;
  const char *k;

  map = htsmsg_field_get_map(f);
  if (map == NULL) {
    snprintf(errbuf, errlen,
             "\"%s\" must be an object with \"min\" and/or \"max\"", name);
    return NULL;
  }
  HTSMSG_FOREACH(sf, map) {
    k = htsmsg_field_name(sf);
    if (strcmp(k, "min") && strcmp(k, "max")) {
      snprintf(errbuf, errlen, "\"%s\": unknown key \"%s\"", name, k);
      return NULL;
    }
    if (dae_expr_get_int(sf, &v, name, errbuf, errlen))
      return NULL;
    if (v < 0) {
      snprintf(errbuf, errlen, "\"%s\": \"%s\" must not be negative", name, k);
      return NULL;
    }
    if (type == DAE_EXPR_STAR_RATING && v > 100) {
      snprintf(errbuf, errlen, "\"%s\": \"%s\" must be 0..100", name, k);
      return NULL;
    }
    if (!strcmp(k, "min")) {
      min = v;
      has_min = 1;
    } else {
      max = v;
      has_max = 1;
    }
  }
  if (!has_min && !has_max) {
    snprintf(errbuf, errlen,
             "\"%s\" needs at least one of \"min\" and \"max\"", name);
    return NULL;
  }
  /* an inverted range is a typo, and a nasty one: it would match
   * only events WITHOUT the metadata (the missing-value policies),
   * which no one means. */
  if (has_min && has_max && min > max) {
    snprintf(errbuf, errlen, "\"%s\": \"min\" must not exceed \"max\"", name);
    return NULL;
  }
  n = calloc(1, sizeof(*n));
  n->type = type;
  n->u.range.min = min;
  n->u.range.max = max;
  n->u.range.has_min = has_min;
  n->u.range.has_max = has_max;
  return n;
}

static int
dae_expr_parse_hhmm(const char *s, int *minutes)
{
  int h, mi;
  char dummy;
  if (s == NULL || sscanf(s, "%d:%d%c", &h, &mi, &dummy) != 2)
    return -1;
  if (h < 0 || h > 23 || mi < 0 || mi > 59)
    return -1;
  *minutes = h * 60 + mi;
  return 0;
}

static dae_expr_node_t *
dae_expr_parse_start(htsmsg_field_t *f, char *errbuf, size_t errlen)
{
  htsmsg_t *map;
  htsmsg_field_t *sf;
  dae_expr_node_t *n;
  const char *k;
  int after = -1, before = -1;

  map = htsmsg_field_get_map(f);
  if (map == NULL) {
    snprintf(errbuf, errlen,
             "\"start\" must be an object with \"after\" and \"before\"");
    return NULL;
  }
  HTSMSG_FOREACH(sf, map) {
    k = htsmsg_field_name(sf);
    if (strcmp(k, "after") && strcmp(k, "before")) {
      snprintf(errbuf, errlen, "\"start\": unknown key \"%s\"", k);
      return NULL;
    }
    if (dae_expr_parse_hhmm(htsmsg_field_get_string(sf),
                            !strcmp(k, "after") ? &after : &before)) {
      snprintf(errbuf, errlen, "\"start\": \"%s\" must be \"HH:MM\"", k);
      return NULL;
    }
  }
  if (after < 0 || before < 0) {
    snprintf(errbuf, errlen,
             "\"start\" needs both \"after\" and \"before\"");
    return NULL;
  }
  n = calloc(1, sizeof(*n));
  n->type = DAE_EXPR_START;
  n->u.start.after = after;
  n->u.start.before = before;
  return n;
}

static dae_expr_node_t *
dae_expr_parse_weekdays(htsmsg_field_t *f, char *errbuf, size_t errlen)
{
  htsmsg_t *list;
  htsmsg_field_t *sf;
  dae_expr_node_t *n;
  uint32_t mask = 0;
  int64_t v;

  list = htsmsg_field_get_list(f);
  if (list == NULL) {
    snprintf(errbuf, errlen, "\"weekdays\" must be an array of days 1..7");
    return NULL;
  }
  HTSMSG_FOREACH(sf, list) {
    if (dae_expr_get_int(sf, &v, "weekdays", errbuf, errlen)) {
      snprintf(errbuf, errlen, "\"weekdays\" entries must be integers 1..7");
      return NULL;
    }
    if (v < 1 || v > 7) {
      snprintf(errbuf, errlen, "\"weekdays\" entries must be 1..7");
      return NULL;
    }
    mask |= 1 << (v - 1);
  }
  if (mask == 0) {
    snprintf(errbuf, errlen, "\"weekdays\" array must not be empty");
    return NULL;
  }
  n = calloc(1, sizeof(*n));
  n->type = DAE_EXPR_WEEKDAYS;
  n->u.weekdays = mask;
  return n;
}

static dae_expr_node_t *
dae_expr_parse_leaf(htsmsg_field_t *f, int depth,
                    int *prunedp, char *errbuf, size_t errlen)
{
  const char *name = htsmsg_field_name(f);
  const char *s;
  dae_expr_node_t *n;
  htsmsg_t *sub;
  int64_t v;
  int i, pruned;

  /* operators first */
  if (!strcmp(name, "all"))
    return dae_expr_parse_op(f, DAE_EXPR_ALL, depth, prunedp, errbuf, errlen);
  if (!strcmp(name, "any"))
    return dae_expr_parse_op(f, DAE_EXPR_ANY, depth, prunedp, errbuf, errlen);
  if (!strcmp(name, "not")) {
    sub = htsmsg_field_get_map(f);
    if (sub == NULL) {
      snprintf(errbuf, errlen, "\"not\" must hold a single node object");
      return NULL;
    }
    pruned = 0;
    dae_expr_node_t *child =
      dae_expr_parse_node(sub, 0, depth + 1, &pruned, errbuf, errlen);
    if (child == NULL) {
      /* a not whose child is pruned is pruned */
      if (pruned)
        *prunedp = 1;
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = DAE_EXPR_NOT;
    n->u.child = child;
    return n;
  }

  /* text leaves and channel_pattern: caseless regex, compiled once */
  for (i = 0; i < ARRAY_SIZE(dae_expr_text_leaves); i++) {
    if (strcmp(name, dae_expr_text_leaves[i].name))
      continue;
    s = htsmsg_field_get_string(f);
    if (s == NULL || s[0] == '\0') {
      snprintf(errbuf, errlen, "\"%s\" must be a non-empty regex string", name);
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = DAE_EXPR_TEXT;
    n->u.text.field = dae_expr_text_leaves[i].field;
    if (regex_compile(&n->u.text.regex, s, TVHREGEX_CASELESS, LS_DVR)) {
      snprintf(errbuf, errlen, "\"%s\": invalid regex", name);
      /* regex_compile allocates the PCRE2 match context before
       * compiling; free it even on failure */
      regex_free(&n->u.text.regex);
      free(n);
      return NULL;
    }
    return n;
  }
  if (!strcmp(name, "channel_pattern")) {
    s = htsmsg_field_get_string(f);
    if (s == NULL || s[0] == '\0') {
      snprintf(errbuf, errlen, "\"%s\" must be a non-empty regex string", name);
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = DAE_EXPR_CHANNEL_NAME;
    if (regex_compile(&n->u.text.regex, s, TVHREGEX_CASELESS, LS_DVR)) {
      snprintf(errbuf, errlen, "\"%s\": invalid regex", name);
      regex_free(&n->u.text.regex);
      free(n);
      return NULL;
    }
    return n;
  }

  if (!strcmp(name, "channel"))
    return dae_expr_parse_ref(f, DAE_EXPR_CHANNEL, errbuf, errlen);
  if (!strcmp(name, "tag"))
    return dae_expr_parse_ref(f, DAE_EXPR_TAG, errbuf, errlen);

  if (!strcmp(name, "content_type")) {
    if (dae_expr_get_int(f, &v, name, errbuf, errlen))
      return NULL;
    if (v < 1 || v > 255) {
      snprintf(errbuf, errlen, "\"content_type\" must be a DVB genre code 1..255");
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = DAE_EXPR_CONTENT_TYPE;
    n->u.content_type = v;
    return n;
  }

  if (!strcmp(name, "category") || !strcmp(name, "serieslink")) {
    s = htsmsg_field_get_string(f);
    if (s == NULL || s[0] == '\0') {
      snprintf(errbuf, errlen, "\"%s\" must be a non-empty string", name);
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = !strcmp(name, "category") ? DAE_EXPR_CATEGORY
                                        : DAE_EXPR_SERIESLINK;
    n->u.str = strdup(s);
    return n;
  }

  if (!strcmp(name, "broadcast_type")) {
    s = htsmsg_field_get_string(f);
    if (s == NULL) {
      snprintf(errbuf, errlen, "\"broadcast_type\" must be a string");
      return NULL;
    }
    n = calloc(1, sizeof(*n));
    n->type = DAE_EXPR_BROADCAST_TYPE;
    if (!strcmp(s, "new"))
      n->u.btype = DAE_BTYPE_NEW;
    else if (!strcmp(s, "new_or_unknown"))
      n->u.btype = DAE_BTYPE_NEW_OR_UNKNOWN;
    else if (!strcmp(s, "repeat"))
      n->u.btype = DAE_BTYPE_REPEAT;
    else {
      snprintf(errbuf, errlen, "\"broadcast_type\" must be \"new\", "
               "\"new_or_unknown\" or \"repeat\"");
      free(n);
      return NULL;
    }
    return n;
  }

  if (!strcmp(name, "duration"))
    return dae_expr_parse_range(f, DAE_EXPR_DURATION, errbuf, errlen);
  if (!strcmp(name, "year"))
    return dae_expr_parse_range(f, DAE_EXPR_YEAR, errbuf, errlen);
  if (!strcmp(name, "season"))
    return dae_expr_parse_range(f, DAE_EXPR_SEASON, errbuf, errlen);
  if (!strcmp(name, "star_rating"))
    return dae_expr_parse_range(f, DAE_EXPR_STAR_RATING, errbuf, errlen);

  if (!strcmp(name, "start"))
    return dae_expr_parse_start(f, errbuf, errlen);
  if (!strcmp(name, "weekdays"))
    return dae_expr_parse_weekdays(f, errbuf, errlen);

  if (!strcmp(name, "present")) {
    s = htsmsg_field_get_string(f);
    for (i = 0; s && i < ARRAY_SIZE(dae_expr_present_names); i++) {
      if (!strcmp(s, dae_expr_present_names[i].name)) {
        n = calloc(1, sizeof(*n));
        n->type = DAE_EXPR_PRESENT;
        n->u.present = dae_expr_present_names[i].field;
        return n;
      }
    }
    snprintf(errbuf, errlen, "\"present\": \"%s\" is not a field that can "
             "be absent", s ?: "");
    return NULL;
  }

  snprintf(errbuf, errlen, "unknown operator or leaf \"%s\"", name);
  return NULL;
}

/*
 * Parse one node: an object with exactly one operator/leaf key, plus an
 * optional "skip" boolean on non-root nodes. Returns NULL with *prunedp
 * set when the node (or its whole subtree) is skipped; NULL with errbuf
 * set on validation errors.
 */
static dae_expr_node_t *
dae_expr_parse_node(htsmsg_t *m, int is_root, int depth,
                    int *prunedp, char *errbuf, size_t errlen)
{
  htsmsg_field_t *f, *op = NULL;
  const char *k;
  int skip = 0, b;

  if (depth > DVR_AUTOREC_EXPR_MAX_DEPTH) {
    snprintf(errbuf, errlen, "expression nests deeper than %d levels",
             DVR_AUTOREC_EXPR_MAX_DEPTH);
    return NULL;
  }
  HTSMSG_FOREACH(f, m) {
    k = htsmsg_field_name(f);
    if (!strcmp(k, "skip")) {
      if (htsmsg_field_get_bool(f, &b)) {
        snprintf(errbuf, errlen, "\"skip\" must be true or false");
        return NULL;
      }
      if (b && is_root) {
        snprintf(errbuf, errlen, "\"skip\" is not allowed on the root node; "
                 "use the rule's Enabled setting to disable the whole rule");
        return NULL;
      }
      skip = b;
      continue;
    }
    if (op != NULL) {
      snprintf(errbuf, errlen, "a node must hold exactly one operator or "
               "leaf, found both \"%s\" and \"%s\"",
               htsmsg_field_name(op), k);
      return NULL;
    }
    op = f;
  }
  if (op == NULL) {
    snprintf(errbuf, errlen, "a node must hold an operator or leaf");
    return NULL;
  }
  if (skip) {
    *prunedp = 1;
    return NULL;
  }
  return dae_expr_parse_leaf(op, depth, prunedp, errbuf, errlen);
}

dvr_autorec_expr_t *
dvr_autorec_expr_compile(const char *jsonc, char *errbuf, size_t errlen)
{
  dvr_autorec_expr_t *expr;
  dae_expr_node_t *root;
  htsmsg_t *m;
  char *clean;
  int pruned = 0;

  errbuf[0] = '\0';
  clean = dae_expr_preprocess(jsonc, errbuf, errlen);
  if (clean == NULL)
    return NULL;
  m = htsmsg_json_deserialize(clean);
  free(clean);
  if (m == NULL) {
    snprintf(errbuf, errlen, "expression is not valid JSON");
    return NULL;
  }
  if (m->hm_islist) {
    snprintf(errbuf, errlen, "the expression root must be a single node "
             "object, not an array");
    htsmsg_destroy(m);
    return NULL;
  }
  root = dae_expr_parse_node(m, 1, 1, &pruned, errbuf, errlen);
  htsmsg_destroy(m);
  if (root == NULL && !pruned)
    return NULL;
  if (dae_expr_node_count(root) > DVR_AUTOREC_EXPR_MAX_NODES) {
    snprintf(errbuf, errlen, "expression has more than %d conditions",
             DVR_AUTOREC_EXPR_MAX_NODES);
    dae_expr_node_free(root);
    return NULL;
  }
  /* root == NULL with pruned set: every branch skipped. Valid, and
   * matches nothing — never vacuous truth, never an error. */
  expr = calloc(1, sizeof(*expr));
  expr->root = root;
  return expr;
}

void
dvr_autorec_expr_free(dvr_autorec_expr_t *expr)
{
  if (expr == NULL)
    return;
  dae_expr_node_free(expr->root);
  free(expr);
}

/* ************************************************************************
 * Evaluate
 * ***********************************************************************/

/* A text leaf matches when any language variant matches, exactly as the
 * flat matcher iterates lang_str trees. regex_match returns 0 on match. */
static int
dae_expr_match_lang_str(tvh_regex_t *regex, lang_str_t *tree)
{
  lang_str_ele_t *ls;
  if (tree == NULL)
    return 0;
  RB_FOREACH(ls, tree, link)
    if (!regex_match(regex, ls->str))
      return 1;
  return 0;
}

static int
dae_expr_eval_text(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  tvh_regex_t *regex = (tvh_regex_t *)&n->u.text.regex;
  char *merged;
  int r;

  switch (n->u.text.field) {
  case DAE_TXT_TITLE:       return dae_expr_match_lang_str(regex, e->title);
  case DAE_TXT_SUBTITLE:    return dae_expr_match_lang_str(regex, e->subtitle);
  case DAE_TXT_SUMMARY:     return dae_expr_match_lang_str(regex, e->summary);
  case DAE_TXT_DESCRIPTION: return dae_expr_match_lang_str(regex, e->description);
  case DAE_TXT_CREDITS:     return dae_expr_match_lang_str(regex, e->credits_cached);
  case DAE_TXT_KEYWORD:     return dae_expr_match_lang_str(regex, e->keyword_cached);
  case DAE_TXT_MERGEDTEXT:
    merged = epg_broadcast_get_merged_text(e);
    if (merged == NULL)
      return 0;
    r = !regex_match(regex, merged);
    free(merged);
    return r;
  }
  return 0;
}

/* uuid is authoritative when it resolves; exact current-name match is
 * the fallback. ch_enabled plays no role in leaf semantics. */
static int
dae_expr_eval_channel(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  channel_t *ch;
  const char *name;

  if (n->u.ref.uuid) {
    ch = channel_find_by_uuid(n->u.ref.uuid);
    if (ch != NULL)
      return ch == e->channel;
  }
  if (n->u.ref.name) {
    name = channel_get_name(e->channel, NULL);
    return name != NULL && !strcmp(name, n->u.ref.name);
  }
  return 0;
}

static int
dae_expr_eval_tag(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  channel_tag_t *tag = NULL;
  idnode_list_mapping_t *ilm;

  if (n->u.ref.uuid)
    tag = channel_tag_find_by_uuid(n->u.ref.uuid);
  if (tag == NULL && n->u.ref.name)
    tag = channel_tag_find_by_name(n->u.ref.name, 0);
  if (tag == NULL)
    return 0;
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link)
    if ((channel_t *)ilm->ilm_in2 == e->channel)
      return 1;
  return 0;
}

/* Port of the flat start-window wrap logic (dvr_autorec_cmp), with
 * after/before as the flat start/start_window times of day. */
static int
dae_expr_eval_start(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  struct tm a_time, ev_time;
  time_t ta, te, tad;
  int after = n->u.start.after, before = n->u.start.before;

  localtime_r(&e->start, &a_time);
  ev_time = a_time;
  a_time.tm_min = after % 60;
  a_time.tm_hour = after / 60;
  ta = mktime(&a_time);
  te = mktime(&ev_time);
  if (after > before) {
    ta -= 24 * 3600;
    tad = ((24 * 60) - after + before) * 60;
    if (ta > te || te > ta + tad) {
      ta += 24 * 3600;
      if (ta > te || te > ta + tad)
        return 0;
    }
  } else {
    tad = (before - after) * 60;
    if (ta > te || te > ta + tad)
      return 0;
  }
  return 1;
}

static int
dae_expr_eval_present(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  switch (n->u.present) {
  case DAE_PRES_SUBTITLE:    return e->subtitle != NULL;
  case DAE_PRES_SUMMARY:     return e->summary != NULL;
  case DAE_PRES_DESCRIPTION: return e->description != NULL;
  case DAE_PRES_CREDITS:     return e->credits_cached != NULL;
  case DAE_PRES_KEYWORD:     return e->keyword_cached != NULL;
  case DAE_PRES_CATEGORY:    return e->category != NULL;
  case DAE_PRES_GENRE:       return LIST_FIRST(&e->genre) != NULL;
  case DAE_PRES_SERIESLINK:  return e->serieslink != NULL;
  case DAE_PRES_YEAR:        return e->copyright_year != 0;
  case DAE_PRES_SEASON:      return e->epnum.s_num != 0;
  case DAE_PRES_STAR_RATING: return e->star_rating != 0;
  }
  return 0;
}

static int
dae_expr_eval_node(const dae_expr_node_t *n, epg_broadcast_t *e)
{
  epg_genre_t ct;
  struct tm tm;
  double duration;
  int i;

  switch (n->type) {
  case DAE_EXPR_ALL:
    for (i = 0; i < n->u.op.count; i++)
      if (!dae_expr_eval_node(n->u.op.elems[i], e))
        return 0;
    return 1;
  case DAE_EXPR_ANY:
    for (i = 0; i < n->u.op.count; i++)
      if (dae_expr_eval_node(n->u.op.elems[i], e))
        return 1;
    return 0;
  case DAE_EXPR_NOT:
    return !dae_expr_eval_node(n->u.child, e);

  case DAE_EXPR_TEXT:
    return dae_expr_eval_text(n, e);
  case DAE_EXPR_CHANNEL:
    return dae_expr_eval_channel(n, e);
  case DAE_EXPR_TAG:
    return dae_expr_eval_tag(n, e);
  case DAE_EXPR_CHANNEL_NAME: {
    const char *name = channel_get_name(e->channel, NULL);
    return name != NULL &&
           !regex_match((tvh_regex_t *)&n->u.text.regex, name);
  }

  case DAE_EXPR_CONTENT_TYPE:
    memset(&ct, 0, sizeof(ct));
    ct.code = n->u.content_type;
    return epg_genre_list_contains(&e->genre, &ct, 1);
  case DAE_EXPR_CATEGORY:
    /* an event without categories fails the leaf (flat behaviour) */
    return e->category != NULL &&
           string_list_contains_string(e->category, n->u.str);
  case DAE_EXPR_BROADCAST_TYPE:
    switch (n->u.btype) {
    case DAE_BTYPE_NEW:            return e->is_new != 0;
    case DAE_BTYPE_NEW_OR_UNKNOWN: return e->is_repeat == 0;
    case DAE_BTYPE_REPEAT:         return e->is_repeat != 0;
    }
    return 0;
  case DAE_EXPR_SERIESLINK:
    /* exact string equality on the stored URI, whatever its scheme */
    return e->serieslink != NULL &&
           !strcmp(n->u.str, e->serieslink->uri);

  case DAE_EXPR_DURATION:
    duration = difftime(e->stop, e->start);
    if (n->u.range.has_min && duration < n->u.range.min)
      return 0;
    if (n->u.range.has_max && duration > n->u.range.max)
      return 0;
    return 1;
  case DAE_EXPR_YEAR:
    /* an event without the value passes (flat: undated specials) */
    if (e->copyright_year == 0)
      return 1;
    if (n->u.range.has_min && e->copyright_year < n->u.range.min)
      return 0;
    if (n->u.range.has_max && e->copyright_year > n->u.range.max)
      return 0;
    return 1;
  case DAE_EXPR_SEASON:
    if (e->epnum.s_num == 0)
      return 1;
    if (n->u.range.has_min && e->epnum.s_num < n->u.range.min)
      return 0;
    if (n->u.range.has_max && e->epnum.s_num > n->u.range.max)
      return 0;
    return 1;
  case DAE_EXPR_STAR_RATING:
    /* an unrated event fails the leaf (flat matcher comment) */
    if (e->star_rating == 0)
      return 0;
    if (n->u.range.has_min && e->star_rating < n->u.range.min)
      return 0;
    if (n->u.range.has_max && e->star_rating > n->u.range.max)
      return 0;
    return 1;

  case DAE_EXPR_START:
    return dae_expr_eval_start(n, e);
  case DAE_EXPR_WEEKDAYS:
    localtime_r(&e->start, &tm);
    return ((1 << ((tm.tm_wday ?: 7) - 1)) & n->u.weekdays) != 0;
  case DAE_EXPR_PRESENT:
    return dae_expr_eval_present(n, e);
  }
  return 0;
}

int
dvr_autorec_expr_eval(dvr_autorec_expr_t *expr, epg_broadcast_t *e)
{
  if (expr == NULL || expr->root == NULL)
    return 0;
  return dae_expr_eval_node(expr->root, e);
}

/* ************************************************************************
 * Flat-to-smart conversion
 *
 * Builds the exact smart equivalent of a flat rule, mirroring what the
 * flat matcher actually evaluates: a field it ignores (the text block
 * under serieslink, fulltext under mergetext, a half-set start window)
 * is dropped here too, so preview(flat) == preview(convert(flat)).
 * ***********************************************************************/

#define FROM_FLAT_MAX_LEAVES 24

typedef struct {
  char *text;      /* leaf node JSON, single line */
  char *comment;   /* optional trailing line comment, no slashes */
} from_flat_leaf_t;

static void
from_flat_add(from_flat_leaf_t *leaves, int *count, htsbuf_queue_t *hq,
              char *comment)
{
  if (*count >= FROM_FLAT_MAX_LEAVES) {
    /* Unreachable by construction: a flat rule yields at most 15
     * leaves (channel, tag, one text block, broadcast_type,
     * content_type, cat1..3, four ranges, start window, weekdays)
     * against a cap of 24. Should a future leaf source ever
     * overflow, poison the count so the builder fails the whole
     * conversion loudly — truncating would be a silent parity
     * break, the one failure mode conversion must never have. */
    *count = FROM_FLAT_MAX_LEAVES + 1;
    htsbuf_queue_flush(hq);
    free(comment);
    return;
  }
  leaves[*count].text = htsbuf_to_string(hq);
  leaves[*count].comment = comment;
  (*count)++;
  htsbuf_queue_flush(hq);
}

static void
from_flat_ref(from_flat_leaf_t *leaves, int *count, const char *leaf,
              idnode_t *in, const char *name)
{
  htsbuf_queue_t hq;
  char ubuf[UUID_HEX_SIZE];

  htsbuf_queue_init(&hq, 0);
  htsbuf_qprintf(&hq, "{ \"%s\": { \"uuid\": \"%s\"", leaf,
                 idnode_uuid_as_str(in, ubuf));
  if (name && name[0] != '\0') {
    htsbuf_append_str(&hq, ", \"name\": ");
    htsbuf_append_and_escape_jsonstr(&hq, name);
  }
  htsbuf_append_str(&hq, " } }");
  from_flat_add(leaves, count, &hq, NULL);
}

static void
from_flat_range(from_flat_leaf_t *leaves, int *count, const char *leaf,
                int64_t min, int64_t max)
{
  htsbuf_queue_t hq;

  if (min <= 0 && max <= 0)
    return;
  htsbuf_queue_init(&hq, 0);
  htsbuf_qprintf(&hq, "{ \"%s\": { ", leaf);
  if (min > 0)
    htsbuf_qprintf(&hq, "\"min\": %"PRId64, min);
  if (max > 0)
    htsbuf_qprintf(&hq, "%s\"max\": %"PRId64, min > 0 ? ", " : "", max);
  htsbuf_append_str(&hq, " } }");
  from_flat_add(leaves, count, &hq, NULL);
}

/* The dropped title (or fulltext/mergetext pattern) is the only
 * human-readable hint of what a serieslink rule records, so it
 * survives as a comment on the leaf. Newlines cannot appear in
 * a line comment; patterns never really contain them, but replace
 * defensively. */
static char *
from_flat_dropped_comment(dvr_autorec_entry_t *dae)
{
  const char *label;
  char *c, *p;
  size_t need;

  if (dae->dae_title == NULL || dae->dae_title[0] == '\0')
    return NULL;
  label = dae->dae_mergetext ? "mergetext" :
          dae->dae_fulltext ? "fulltext" : "title";
  need = strlen(label) + strlen(dae->dae_title) + 16;
  c = malloc(need);
  if (c == NULL)
    return NULL; /* the caller treats NULL as "no comment" */
  snprintf(c, need, "%s was \"%s\"", label, dae->dae_title);
  for (p = c; *p; p++)
    if (*p == '\n' || *p == '\r')
      *p = ' ';
  return c;
}

/* A structurally dead flat rule must refuse conversion outright
 * (decided 2026-07-15): its leaves would come back to life as a smart
 * expression, silently recording what the flat rule deliberately did
 * not. Two flat idioms are dead at match time: the weekdays == 0
 * soft-disable, and the super-wildcard guard (dvr_autorec_cmp), which
 * rejects any rule whose only constraints are the weak selectors
 * (btype, star rating, weekdays, start window). */
static int
from_flat_dead(dvr_autorec_entry_t *dae, htsmsg_t *warnings)
{
  if (dae->dae_weekdays == 0) {
    htsmsg_add_str(warnings, NULL,
                   "weekdays is empty, so the flat rule matches nothing; "
                   "converting would bring it back to life - use the "
                   "rule's Enabled setting to disable a rule, then "
                   "convert");
    return 1;
  }
  if (dae->dae_channel == NULL &&
      dae->dae_channel_tag == NULL &&
      dae->dae_content_type == 0 &&
      (dae->dae_title == NULL || dae->dae_title[0] == '\0') &&
      (dae->dae_cat1 == NULL || dae->dae_cat1[0] == '\0') &&
      (dae->dae_cat2 == NULL || dae->dae_cat2[0] == '\0') &&
      (dae->dae_cat3 == NULL || dae->dae_cat3[0] == '\0') &&
      dae->dae_minduration <= 0 &&
      (dae->dae_maxduration <= 0 || dae->dae_maxduration > 24 * 3600) &&
      dae->dae_minyear <= 0 &&
      dae->dae_maxyear <= 0 &&
      dae->dae_minseason <= 0 &&
      dae->dae_maxseason <= 0 &&
      dae->dae_serieslink_uri == NULL) {
    htsmsg_add_str(warnings, NULL,
                   "the flat matcher treats this rule as matching nothing "
                   "(no strong selector, the super-wildcard guard); "
                   "converting its weak selectors would bring it back "
                   "to life");
    return 1;
  }
  return 0;
}

char *
dvr_autorec_expr_from_flat(dvr_autorec_entry_t *dae, htsmsg_t *warnings)
{
  from_flat_leaf_t leaves[FROM_FLAT_MAX_LEAVES];
  int count = 0, i, d;
  htsbuf_queue_t hq;
  char *result;
  const char *cats[3];
  static const char *ft_fields[] =
    { "title", "subtitle", "summary", "description", "credits", "keyword" };

  if (from_flat_dead(dae, warnings))
    return NULL;

  htsbuf_queue_init(&hq, 0);

  if (dae->dae_channel)
    from_flat_ref(leaves, &count, "channel", &dae->dae_channel->ch_id,
                  channel_get_name(dae->dae_channel, NULL));
  if (dae->dae_channel_tag)
    from_flat_ref(leaves, &count, "tag", &dae->dae_channel_tag->ct_id,
                  dae->dae_channel_tag->ct_name);

  /* the text block, with the flat matcher's exact precedence:
   * serieslink disables it entirely, mergetext beats fulltext */
  if (dae->dae_serieslink_uri && dae->dae_serieslink_uri[0] != '\0') {
    htsbuf_append_str(&hq, "{ \"serieslink\": ");
    htsbuf_append_and_escape_jsonstr(&hq, dae->dae_serieslink_uri);
    htsbuf_append_str(&hq, " }");
    from_flat_add(leaves, &count, &hq, from_flat_dropped_comment(dae));
  } else if (dae->dae_title && dae->dae_title[0] != '\0') {
    if (dae->dae_mergetext) {
      htsbuf_append_str(&hq, "{ \"mergedtext\": ");
      htsbuf_append_and_escape_jsonstr(&hq, dae->dae_title);
      htsbuf_append_str(&hq, " }");
    } else if (dae->dae_fulltext) {
      /* fulltext desugar: independent, individually editable copies */
      htsbuf_append_str(&hq, "{ \"any\": [ ");
      for (i = 0; i < ARRAY_SIZE(ft_fields); i++) {
        htsbuf_qprintf(&hq, "%s{ \"%s\": ", i ? ", " : "", ft_fields[i]);
        htsbuf_append_and_escape_jsonstr(&hq, dae->dae_title);
        htsbuf_append_str(&hq, " }");
      }
      htsbuf_append_str(&hq, " ] }");
    } else {
      htsbuf_append_str(&hq, "{ \"title\": ");
      htsbuf_append_and_escape_jsonstr(&hq, dae->dae_title);
      htsbuf_append_str(&hq, " }");
    }
    from_flat_add(leaves, &count, &hq, NULL);
  }

  if (dae->dae_btype != DVR_AUTOREC_BTYPE_ALL) {
    htsbuf_qprintf(&hq, "{ \"broadcast_type\": \"%s\" }",
                   dae->dae_btype == DVR_AUTOREC_BTYPE_NEW ? "new" :
                   dae->dae_btype == DVR_AUTOREC_BTYPE_REPEAT ? "repeat" :
                   "new_or_unknown");
    from_flat_add(leaves, &count, &hq, NULL);
  }

  if (dae->dae_content_type != 0) {
    htsbuf_qprintf(&hq, "{ \"content_type\": %u ", dae->dae_content_type);
    htsbuf_append_str(&hq, "}");
    from_flat_add(leaves, &count, &hq, NULL);
  }

  cats[0] = dae->dae_cat1;
  cats[1] = dae->dae_cat2;
  cats[2] = dae->dae_cat3;
  for (i = 0; i < 3; i++) {
    if (cats[i] == NULL || cats[i][0] == '\0')
      continue;
    htsbuf_append_str(&hq, "{ \"category\": ");
    htsbuf_append_and_escape_jsonstr(&hq, cats[i]);
    htsbuf_append_str(&hq, " }");
    from_flat_add(leaves, &count, &hq, NULL);
  }

  from_flat_range(leaves, &count, "duration",
                  dae->dae_minduration, dae->dae_maxduration);
  from_flat_range(leaves, &count, "year", dae->dae_minyear, dae->dae_maxyear);
  from_flat_range(leaves, &count, "season",
                  dae->dae_minseason, dae->dae_maxseason);
  from_flat_range(leaves, &count, "star_rating", dae->dae_star_rating, 0);

  /* the flat matcher applies the window only when both bounds are
   * valid times of day; conversion emits the leaf only in that case */
  if (dae->dae_start >= 0 && dae->dae_start < 24 * 60 &&
      dae->dae_start_window >= 0 && dae->dae_start_window < 24 * 60) {
    htsbuf_qprintf(&hq,
                   "{ \"start\": { \"after\": \"%02d:%02d\", "
                   "\"before\": \"%02d:%02d\" } }",
                   dae->dae_start / 60, dae->dae_start % 60,
                   dae->dae_start_window / 60, dae->dae_start_window % 60);
    from_flat_add(leaves, &count, &hq, NULL);
  }

  if (dae->dae_weekdays != 0x7f) {
    htsbuf_append_str(&hq, "{ \"weekdays\": [ ");
    for (i = 0, d = 0; i < 7; i++)
      if (dae->dae_weekdays & (1 << i))
        htsbuf_qprintf(&hq, "%s%d", d++ ? ", " : "", i + 1);
    htsbuf_append_str(&hq, " ] }");
    from_flat_add(leaves, &count, &hq, NULL);
  }

  if (count > FROM_FLAT_MAX_LEAVES) {
    for (i = 0; i < FROM_FLAT_MAX_LEAVES; i++) {
      free(leaves[i].text);
      free(leaves[i].comment);
    }
    htsmsg_add_str(warnings, NULL,
                   "internal error: conversion exceeded the leaf capacity");
    return NULL;
  }

  if (count == 0) {
    htsmsg_add_str(warnings, NULL,
                   "the rule has no convertible matching conditions");
    return NULL;
  }

  if (count == 1 && leaves[0].comment == NULL) {
    result = leaves[0].text;
    return result;
  }

  if (count == 1) {
    htsbuf_qprintf(&hq, "%s // %s", leaves[0].text, leaves[0].comment);
  } else {
    htsbuf_append_str(&hq, "{ \"all\": [\n");
    for (i = 0; i < count; i++) {
      htsbuf_qprintf(&hq, "  %s%s", leaves[i].text,
                     i + 1 < count ? "," : "");
      if (leaves[i].comment)
        htsbuf_qprintf(&hq, " // %s", leaves[i].comment);
      htsbuf_append_str(&hq, "\n");
    }
    htsbuf_append_str(&hq, "] }");
  }
  for (i = 0; i < count; i++) {
    free(leaves[i].text);
    free(leaves[i].comment);
  }
  return htsbuf_to_string(&hq);
}
