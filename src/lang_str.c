/*
 *  Multi-language String support
 *  Copyright (C) 2012 Adam Sutton
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

#include <string.h>
#include <stdlib.h>

#include "redblack.h"
#include "lang_codes.h"
#include "lang_str.h"

/* ************************************************************************
 * Support
 * ***********************************************************************/

/* Compare language codes */
static int _lang_cmp ( void *a, void *b )
{
  return strcmp(((lang_str_ele_t*)a)->lang, ((lang_str_ele_t*)b)->lang);
}

/* ************************************************************************
 * Language String
 * ***********************************************************************/

/* Create new instance */
lang_str_t *lang_str_create ( void )
{
  return calloc(1, sizeof(lang_str_t));
}

/* Destroy (free memory) */
void lang_str_destroy ( lang_str_t *ls )
{
  lang_str_ele_t *e;
  while ((e = RB_FIRST(ls))) {
    if (e->str)  free(e->str);
    RB_REMOVE(ls, e, link);
    free(e);
  }
  free(ls);
}

/* Copy the lang_str instance */
lang_str_t *lang_str_copy ( const lang_str_t *ls )
{
  lang_str_t *ret = lang_str_create();
  lang_str_ele_t *e;
  RB_FOREACH(e, ls, link)
    lang_str_add(ret, e->str, e->lang, 0);
  return ret;
}

/* Get language element */
lang_str_ele_t *lang_str_get2
  ( lang_str_t *ls, const char *lang )
{
  int i;
  const char **langs;
  lang_str_ele_t skel, *e = NULL;

  if (!ls) return NULL;

  /* Check config/requested langs */
  if ((langs = lang_code_split(lang))) {
    i = 0;
    while (langs[i]) {
      skel.lang = langs[i];
      if ((e = RB_FIND(ls, &skel, link, _lang_cmp)))
        break;
      i++;
    }
    free(langs);
  }

  /* Use first available */
  if (!e) e = RB_FIRST(ls);

  /* Return */
  return e;
}

/* Get string */
const char *lang_str_get
  ( lang_str_t *ls, const char *lang )
{
  lang_str_ele_t *e = lang_str_get2(ls, lang);
  return e ? e->str : NULL;
}

/* Internal insertion routine */
static int _lang_str_add
  ( lang_str_t *ls, const char *str, const char *lang, int update, int append )
{
  int save = 0;
  static lang_str_ele_t *skel = NULL;
  lang_str_ele_t *e;

  if (!str) return 0;

  /* Get proper code */
  if (!(lang = lang_code_get(lang))) return 0;

  /* Create skel */
  if (!skel) skel = calloc(1, sizeof(lang_str_ele_t));
  skel->lang = lang;

  /* Create */
  e = RB_INSERT_SORTED(ls, skel, link, _lang_cmp);
  if (!e) {
    skel->str = strdup(str);
    skel = NULL;
    save = 1;

  /* Append */
  } else if (append) {
    e->str = realloc(e->str, strlen(e->str) + strlen(str) + 1);
    strcat(e->str, str);
    save = 1;

  /* Update */
  } else if (update && strcmp(str, e->str)) {
    free(e->str);
    e->str = strdup(str);
    save = 1;
  }

  return save;
}

/* Add new string (or replace existing one) */
int lang_str_add
  ( lang_str_t *ls, const char *str, const char *lang, int update )
{
  return _lang_str_add(ls, str, lang, update, 0);
}

/* Append to existing string (or add new one) */
int lang_str_append
  ( lang_str_t *ls, const char *str, const char *lang )
{
  return _lang_str_add(ls, str, lang, 0, 1);
}

/* Serialize */
void lang_str_serialize ( lang_str_t *ls, htsmsg_t *m, const char *f )
{
  lang_str_ele_t *e;
  if (!ls) return;
  htsmsg_t *a = htsmsg_create_map();
  RB_FOREACH(e, ls, link) {
    htsmsg_add_str(a, e->lang, e->str);
  }
  htsmsg_add_msg(m, f, a);
}

/* De-serialize */
lang_str_t *lang_str_deserialize ( htsmsg_t *m, const char *n )
{
  lang_str_t *ret = NULL;
  htsmsg_t *a;
  htsmsg_field_t *f;
  const char *str;

  if ((a = htsmsg_get_map(m, n))) {
    ret = lang_str_create();
    HTSMSG_FOREACH(f, a) {
      if ((str = htsmsg_field_get_string(f))) {
        lang_str_add(ret, str, f->hmf_name, 0);
      }
    }
  } else if ((str = htsmsg_get_str(m, n))) {
    ret = lang_str_create();
    lang_str_add(ret, str, NULL, 0);
  }
  return ret;
}

