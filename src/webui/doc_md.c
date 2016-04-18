/*
 *  tvheadend, documenation markdown generator
 *  Copyright (C) 2016 Jaroslav Kysela
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
#include "config.h"
#include "webui.h"
#include "http.h"
#include "docs.h"

static int md_class(htsbuf_queue_t *hq, const char *clazz, const char *lang,
                    int hdr, int docs, int props);

/* */
static int
md_nl(htsbuf_queue_t *hq, int nl)
{
  if (nl)
    htsbuf_append(hq, "\n", 1);
  return 1;
}

/* */
static void
md_header(htsbuf_queue_t *hq, const char *prefix, const char *s)
{
  htsbuf_append_str(hq, prefix);
  htsbuf_append_str(hq, s);
  htsbuf_append(hq, "\n", 1);
}

/* */
static void
md_style(htsbuf_queue_t *hq, const char *style, const char *s)
{
  size_t l = strlen(style);
  htsbuf_append(hq, style, l);
  htsbuf_append_str(hq, s);
  htsbuf_append(hq, style, l);
}

/* */
static void
md_text(htsbuf_queue_t *hq, const char *first, const char *next, const char *text)
{
  char *s, *t, *p;
  int col, nl;

  t = s = p = tvh_strdupa(text);
  col = nl = 0;
  while (*s) {
    if (++col > 74) {
      nl = md_nl(hq, nl);
      if (first) {
        htsbuf_append_str(hq, first);
        first = NULL;
      } else if (next) {
        htsbuf_append_str(hq, next);
      }
      if (p <= t)
        p = t + 74;
      htsbuf_append(hq, t, p - t);
      col = 0;
      t = s = p;
    } else if (*s <= ' ') {
      *s = ' ';
      p = ++s;
    } else {
      s++;
    }
  }
  if (t < s) {
    md_nl(hq, nl);
    if (first)
      htsbuf_append_str(hq, first);
    else if (next)
      htsbuf_append_str(hq, next);
    htsbuf_append(hq, t, s - t);
  }
}

/* */
static int
md_props(htsbuf_queue_t *hq, htsmsg_t *m, const char *lang, int nl)
{
  htsmsg_t *l, *n, *e, *x;
  htsmsg_field_t *f, *f2;
  const char *s;
  int first = 1, b;

  l = htsmsg_get_list(m, "props");
  HTSMSG_FOREACH(f, l) {
    n = htsmsg_field_get_map(f);
    if (!n) continue;
    if (!htsmsg_get_bool(n, "noui", &b) && b) continue;
    s = htsmsg_get_str(n, "caption");
    if (!s) continue;
    if (first) {
      nl = md_nl(hq, nl);
      htsbuf_append_str(hq, "#### ");
      htsbuf_append_str(hq, tvh_gettext_lang(lang, N_("Items")));
      md_nl(hq, 1);
      md_nl(hq, 1);
      htsbuf_append_str(hq, tvh_gettext_lang(lang, N_("The items have the following functions:")));
      md_nl(hq, 1);
      first = 0;
    }
    nl = md_nl(hq, nl);
    md_style(hq, "**", s);
    if (!htsmsg_get_bool(n, "rdonly", &b) && b) {
      htsbuf_append(hq, " _", 2);
      htsbuf_append_str(hq, tvh_gettext_lang(lang, N_("(Read-only)")));
      htsbuf_append(hq, "_", 1);
    }
    md_nl(hq, 1);
    s = htsmsg_get_str(n, "description");
    if (s) {
      md_text(hq, ": ", "  ", s);
      md_nl(hq, 1);
    }
    s = htsmsg_get_str(n, "doc");
    if (s) {
      htsbuf_append_str(hq, s);
      md_nl(hq, 1);
    }
    if (!htsmsg_get_bool_or_default(n, "doc_nlist", 0)) {
      e = htsmsg_get_list(n, "enum");
      if (e) {
        HTSMSG_FOREACH(f2, e) {
          x = htsmsg_field_get_map(f2);
          if (x) {
            s = htsmsg_get_str(x, "val");
          } else {
            s = htsmsg_field_get_string(f2);
          }
          if (s) {
            md_nl(hq, 1);
            htsbuf_append(hq, "  * ", 4);
            md_style(hq, "**", s);
          }
        }
        md_nl(hq, 1);
      }
    }
  }
  return nl;
}

/* */
static void
md_render(htsbuf_queue_t *hq, const char *doc, const char *lang)
{
  if (doc[0] == '\xff') {
    switch (doc[1]) {
    case 1:
      htsbuf_append_str(hq, tvh_gettext_lang(lang, doc + 2));
      break;
    case 2:
      md_class(hq, doc + 2, lang, 0, 1, 0);
      break;
    case 3:
      md_class(hq, doc + 2, lang, 0, 0, 1);
      break;
    }
  } else {
    htsbuf_append_str(hq, doc);
  }
}

/* */
static int
md_doc(htsbuf_queue_t *hq, const char **doc, const char *lang, int nl)
{
  if (doc == NULL)
    return nl;
  for (; *doc; doc++) {
    md_render(hq, *doc, lang);
    nl = 1;
  }
  return nl;
}

/* */
static int
md_class(htsbuf_queue_t *hq, const char *clazz, const char *lang,
         int hdr, int docs, int props)
{
  const idclass_t *ic;
  htsmsg_t *m;
  const char *s, **doc;
  int nl = 0;

  pthread_mutex_lock(&global_lock);
  ic = idclass_find(clazz);
  if (ic == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_NOT_FOUND;
  }
  doc = idclass_get_doc(ic);
  m = idclass_serializedoc(ic, lang);
  pthread_mutex_unlock(&global_lock);
  if (hdr) {
    s = htsmsg_get_str(m, "caption");
    if (s) {
      md_header(hq, "## ", s);
      nl = md_nl(hq, 1);
    }
  }
  if (docs)
    nl = md_doc(hq, doc, lang, nl);
  if (props)
    nl = md_props(hq, m, lang, nl);
  htsmsg_destroy(m);
  return 0;
}

/**
 * List of all classes with documentation
 */
static int
http_markdown_classes(http_connection_t *hc)
{
  idclass_t const **all, **all2;
  const idclass_t *ic;
  htsbuf_queue_t *hq = &hc->hc_reply;

  pthread_mutex_lock(&global_lock);
  all = idclass_find_all();
  if (all == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_NOT_FOUND;
  }
  for (all2 = all; *all2; all2++) {
    ic = *all2;
    if (ic->ic_caption) {
      htsbuf_append_str(hq, ic->ic_class);
      htsbuf_append(hq, "\n", 1);
    }
  }
  pthread_mutex_unlock(&global_lock);

  free(all);
  return 0;
}

/**
 *
 */
static int
http_markdown_class(http_connection_t *hc, const char *clazz)
{
  const char *lang = hc->hc_access->aa_lang_ui;
  htsbuf_queue_t *hq = &hc->hc_reply;

  return md_class(hq, clazz, lang, 1, 1, 1);
}

/**
 *
 */
static int
http_markdown_page(http_connection_t *hc, const struct tvh_doc_page *page)
{
  const char **doc = page->strings;
  const char *lang = hc->hc_access->aa_lang_ui;
  htsbuf_queue_t *hq = &hc->hc_reply;

  if (doc == NULL)
    return HTTP_STATUS_NOT_FOUND;
  md_doc(hq, doc, lang, 0);
  return 0;
}

/**
 * Handle requests for markdown export.
 */
int
page_markdown(http_connection_t *hc, const char *remain, void *opaque)
{
  const struct tvh_doc_page *page;
  char *components[2];
  int nc, r;

  nc = http_tokenize((char *)remain, components, 2, '/');
  if (!nc)
    return HTTP_STATUS_BAD_REQUEST;

  if (nc == 2)
    http_deescape(components[1]);

  if (nc == 1 && !strcmp(components[0], "classes"))
    r = http_markdown_classes(hc);
  else if (nc == 2 && !strcmp(components[0], "class"))
    r = http_markdown_class(hc, components[1]);
  else if (nc == 1) {
    for (page = tvh_doc_markdown_pages; page->name; page++)
      if (!strcmp(page->name, components[0])) {
        r = http_markdown_page(hc, page);
        goto done;
      }
    r = HTTP_STATUS_BAD_REQUEST;
  } else {
    r = HTTP_STATUS_BAD_REQUEST;
  }

done:
  if (r == 0)
    http_output_content(hc, "text/markdown");

  return r;
}
