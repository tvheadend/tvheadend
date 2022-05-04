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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "redblack.h"
#include "lang_codes.h"
#include "lang_str.h"
#include "tvheadend.h"

#define LANG_STR_ADD    0
#define LANG_STR_UPDATE 1
#define LANG_STR_APPEND 2


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
    lang_str_add(ls, s, lang);
  return ls;
}

/* Destroy (free memory) */
void lang_str_destroy ( lang_str_t *ls )
{ 
  lang_str_ele_t *e;
  if (ls == NULL)
    return;
  while ((e = RB_FIRST(ls))) {
    RB_REMOVE(ls, e, link);
    free(e);
  }
  free(ls);
}

/* Copy the lang_str instance */
lang_str_t *lang_str_copy ( const lang_str_t *ls )
{
  lang_str_t *ret;
  lang_str_ele_t *e;
  if (ls == NULL)
    return NULL;
  ret = lang_str_create();
  RB_FOREACH(e, ls, link)
    lang_str_add(ret, e->str, e->lang);
  return ret;
}

/* Get language element, don't return first */
lang_str_ele_t *lang_str_get2_only
  ( const lang_str_t *ls, const char *lang )
{
  int i;
  const lang_code_list_t *langs;
  lang_str_ele_t skel, *e = NULL;

  if (!ls) return NULL;
  
  /* Check config/requested langs */
  if ((langs = lang_code_split(lang)) != NULL) {
    for (i = 0; i < langs->codeslen; i++) {
      strlcpy(skel.lang, langs->codes[i]->code2b, sizeof(skel.lang));
      if ((e = RB_FIND(ls, &skel, link, _lang_cmp)))
        break;
    }
  }

  /* Return */
  return e;
}

/* Get language element */
lang_str_ele_t *lang_str_get2
  ( const lang_str_t *ls, const char *lang )
{
  lang_str_ele_t *e = lang_str_get2_only(ls, lang);

  /* Use first available */
  if (!e && ls) e = RB_FIRST(ls);

  /* Return */
  return e;
}

/* Internal insertion routine */
static int _lang_str_add
  ( lang_str_t *ls, const char *str, const char *lang, int cmd )
{
  int save = 0;
  lang_str_ele_t *e, *ae;

  if (!ls || !str) return 0;

  /* Get proper code */
  if (!lang) lang = lang_code_preferred();
  if (!(lang = lang_code_get(lang))) return 0;

  /* Use 'dummy' ele pointer for _lang_cmp */
  ae = (lang_str_ele_t *)(lang - offsetof(lang_str_ele_t, lang));
  e = RB_FIND(ls, ae, link, _lang_cmp);

  /* Create */
  if (!e) {
    e = malloc(sizeof(*e) + strlen(str) + 1);
    strlcpy(e->lang, lang, sizeof(e->lang));
    strcpy(e->str, str);
    RB_INSERT_SORTED(ls, e, link, _lang_cmp);
    save = 1;

  /* Append */
  } else if (cmd == LANG_STR_APPEND) {
    RB_REMOVE(ls, e, link);
    ae = realloc(e, sizeof(*e) + strlen(e->str) + strlen(str) + 1);
    if (ae) {
      strcat(ae->str, str);
      save = 1;
    } else {
      ae = e;
    }
    RB_INSERT_SORTED(ls, ae, link, _lang_cmp);

  /* Update */
  } else if (cmd == LANG_STR_UPDATE && strcmp(str, e->str)) {
    if (strlen(e->str) >= strlen(str)) {
      strcpy(e->str, str);
      save = 1;
    } else {
      RB_REMOVE(ls, e, link);
      ae = realloc(e, sizeof(*e) + strlen(str) + 1);
      if (ae) {
        strcpy(ae->str, str);
        save = 1;
      } else {
        ae = e;
      }
      RB_INSERT_SORTED(ls, ae, link, _lang_cmp);
    }
  }
  
  return save;
}

/* Add new string (or replace existing one) */
int lang_str_add 
  ( lang_str_t *ls, const char *str, const char *lang )
{ 
  return _lang_str_add(ls, str, lang, LANG_STR_UPDATE);
}

/* Append to existing string (or add new one) */
int lang_str_append
  ( lang_str_t *ls, const char *str, const char *lang )
{
  return _lang_str_add(ls, str, lang, LANG_STR_APPEND);
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
  lang_str_add(*dst, str, lang);
  return 1;
}

/* Set new string with update check */
int lang_str_set_multi
  ( lang_str_t **dst, const lang_str_t *src )
{
  int changed = 0;
  lang_str_ele_t *e;
  RB_FOREACH(e, src, link) {
    changed |= lang_str_set(dst, e->str, e->lang);
  }
  return changed;
}

/* Set new strings with update check */
int lang_str_set2
  ( lang_str_t **dst, const lang_str_t *src )
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

/* Serialize one string element directly */
void lang_str_serialize_one
  ( htsmsg_t *m, const char *f, const char *str, const char *lang )
{
  lang_str_t *l = lang_str_create();
  if (l) {
    lang_str_add(l, str, lang);
    lang_str_serialize(l, m, f);
    lang_str_destroy(l);
  }
}

/* De-serialize map */
lang_str_t *lang_str_deserialize_map ( htsmsg_t *map )
{
  lang_str_t *ret = lang_str_create();
  htsmsg_field_t *f;
  const char *str;

  if (ret) {
    HTSMSG_FOREACH(f, map) {
      if ((str = htsmsg_field_get_string(f))) {
        lang_str_add(ret, str, htsmsg_field_name(f));
      }
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
    lang_str_add(ret, str, NULL);
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

size_t lang_str_size(const lang_str_t *ls)
{
  lang_str_ele_t *e;
  size_t size;
  if (!ls) return 0;
  size = sizeof(*ls);
  RB_FOREACH(e, ls, link) {
    size += sizeof(*e);
    size += tvh_strlen(e->str);
  }
  return size;
}
