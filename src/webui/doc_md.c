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
  const idclass_t *ic;
  const char *lang = hc->hc_access->aa_lang_ui;
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *m, *l, *n, *e, *x;
  htsmsg_field_t *f, *f2;
  const char *s;
  int nl = 0;

  pthread_mutex_lock(&global_lock);
  ic = idclass_find(clazz);
  if (ic == NULL) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_NOT_FOUND;
  }
  m = idclass_serialize(ic, lang);
  pthread_mutex_unlock(&global_lock);
  s = htsmsg_get_str(m, "caption");
  if (s) {
    md_header(hq, "####", s);
    nl = 1;
  }
  l = htsmsg_get_list(m, "props");
  HTSMSG_FOREACH(f, l) {
    n = htsmsg_field_get_map(f);
    if (!n) continue;
    s = htsmsg_get_str(n, "caption");
    if (!s) continue;
    nl = md_nl(hq, nl);
    md_style(hq, "**", s);
    md_nl(hq, 1);
    s = htsmsg_get_str(n, "description");
    if (s) {
      md_text(hq, ": ", "  ", s);
      md_nl(hq, 1);
    }
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
    htsmsg_print(n);
  }
  htsmsg_destroy(m);
  return 0;
}

/**
 * Handle requests for markdown export.
 */
int
page_markdown(http_connection_t *hc, const char *remain, void *opaque)
{
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
  else
    r = HTTP_STATUS_BAD_REQUEST;

  if (r == 0)
    http_output_content(hc, "text/xml");

  return r;
}
