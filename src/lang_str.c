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
#include "tvheadend.h"

SKEL_DECLARE(lang_str_ele_skel, lang_str_ele_t);

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

lang_str_t *lang_str_create2 ( const char *s, const char *lang )
{
  lang_str_t *ls = lang_str_create();
  if (ls)
    lang_str_add(ls, s, lang, 0);
  return ls;
}

/* Destroy (free memory) */
void lang_str_destroy ( lang_str_t *ls )
{ 
  lang_str_ele_t *e;
  if (ls == NULL)
    return;
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
  ( const lang_str_t *ls, const char *lang )
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
  ( const lang_str_t *ls, const char *lang )
{
  lang_str_ele_t *e = lang_str_get2(ls, lang);
  return e ? e->str : NULL;
}

/* Internal insertion routine */
static int _lang_str_add
  ( lang_str_t *ls, const char *str, const char *lang, int update, int append )
{
  int save = 0;
  lang_str_ele_t *e;

  if (!str) return 0;

  /* Get proper code */
  if (!lang) lang = lang_code_preferred();
  if (!(lang = lang_code_get(lang))) return 0;

  /* Create skel */
  SKEL_ALLOC(lang_str_ele_skel);
  lang_str_ele_skel->lang = lang;

  /* Create */
  e = RB_INSERT_SORTED(ls, lang_str_ele_skel, link, _lang_cmp);
  if (!e) {
    lang_str_ele_skel->str = strdup(str);
    SKEL_USED(lang_str_ele_skel);
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

/* Set new string with update check */
int lang_str_set
  ( lang_str_t **dst, const char *str, const char *lang )
{
  lang_str_ele_t *e;
  int found;
  if (*dst == NULL) goto change1;
  if (!lang) lang = lang_code_preferred();
  if (!(lang = lang_code_get(lang))) return 0;
  if (*dst) {
    found = 0;
    RB_FOREACH(e, *dst, link) {
      if (found)
        goto change;
      found = strcmp(e->lang, lang) == 0 &&
              strcmp(e->str, str) == 0;
      if (!found)
        goto change;
    }
    if (found)
      return 0;
  }
change:
  lang_str_destroy(*dst);
change1:
  *dst = lang_str_create();
  lang_str_add(*dst, str, lang, 1);
  return 1;
}

/* Set new strings with update check */
int lang_str_set2
  ( lang_str_t **dst, lang_str_t *src )
{
  if (*dst) {
    if (!lang_str_compare(*dst, src))
      return 0;
    lang_str_destroy(*dst);
  }
  *dst = lang_str_copy(src);
  return 1;
}

/* Serialize  map */
htsmsg_t *lang_str_serialize_map ( lang_str_t *ls )
{
  lang_str_ele_t *e;
  if (!ls) return NULL;
  htsmsg_t *a = htsmsg_create_map();
  RB_FOREACH(e, ls, link) {
    htsmsg_add_str(a, e->lang, e->str);
  }
  return a;
}

/* Serialize */
void lang_str_serialize ( lang_str_t *ls, htsmsg_t *m, const char *f )
{
  if (!ls) return;
  htsmsg_add_msg(m, f, lang_str_serialize_map(ls));
}

/* De-serialize map */
lang_str_t *lang_str_deserialize_map ( htsmsg_t *map )
{
  lang_str_t *ret = lang_str_create();
  htsmsg_field_t *f;
  const char *str;

  HTSMSG_FOREACH(f, map) {
    if ((str = htsmsg_field_get_string(f))) {
      lang_str_add(ret, str, f->hmf_name, 0);
    }
  }
  return ret;
}

/* De-serialize */
lang_str_t *lang_str_deserialize ( htsmsg_t *m, const char *n )
{
  htsmsg_t *a;
  const char *str;
  
  if ((a = htsmsg_get_map(m, n))) {
    return lang_str_deserialize_map(a);
  } else if ((str = htsmsg_get_str(m, n))) {
    lang_str_t *ret = lang_str_create();
    lang_str_add(ret, str, NULL, 0);
    return ret;
  }
  return NULL;
}

/* Compare */
int lang_str_compare( const lang_str_t *ls1, const lang_str_t *ls2 )
{
  lang_str_ele_t *e;
  const char *s1, *s2;
  int r;

  if (ls1 == NULL && ls2)
    return -1;
  if (ls2 == NULL && ls1)
    return 1;
  if (ls1 == ls2)
    return 0;
  /* Note: may be optimized to not check languages twice */
  RB_FOREACH(e, ls1, link) {
    s1 = lang_str_get(ls1, e->lang);
    s2 = lang_str_get(ls2, e->lang);
    if (s1 == NULL && s2 != NULL)
      return -1;
    if (s2 == NULL && s1 != NULL)
      return 1;
    if (s1 == NULL || s2 == NULL)
      continue;
    r = strcmp(s1, s2);
    if (r) return r;
  }
  RB_FOREACH(e, ls2, link) {
    s1 = lang_str_get(ls1, e->lang);
    s2 = lang_str_get(ls2, e->lang);
    if (s1 == NULL && s2 != NULL)
      return -1;
    if (s2 == NULL && s1 != NULL)
      return 1;
    if (s1 == NULL || s2 == NULL)
      continue;
    r = strcmp(s1, s2);
    if (r) return r;
  }
  return 0;
}

int strempty(const char* c) {
  return !c || c[0] == 0;
}

int lang_str_empty(lang_str_t* str) {
  return strempty(lang_str_get(str, NULL));
}

size_t lang_str_size(const lang_str_t *ls)
{
  lang_str_ele_t *e;
  size_t size;
  if (!ls) return 0;
  size = sizeof(*ls);
  RB_FOREACH(e, ls, link) {
    size += sizeof(*e);
    size += tvh_strlen(e->str);
    size += tvh_strlen(e->lang);
  }
  return size;
}

void lang_str_done( void )
{
  SKEL_FREE(lang_str_ele_skel);
}
