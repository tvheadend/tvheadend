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

#ifndef __TVH_LANG_STR_H__
#define __TVH_LANG_STR_H__

#include "redblack.h"
#include "htsmsg.h"

typedef struct lang_str_ele
{
  RB_ENTRY(lang_str_ele) link;
  const char *lang;
  char *str;
} lang_str_ele_t;

typedef RB_HEAD(lang_str, lang_str_ele) lang_str_t;

/* Create/Destroy */
void            lang_str_destroy ( lang_str_t *ls );
lang_str_t     *lang_str_create  ( void );
lang_str_t     *lang_str_copy    ( const lang_str_t *ls );

/* Get elements */
const char     *lang_str_get     ( lang_str_t *ls, const char *lang );
lang_str_ele_t *lang_str_get2    ( lang_str_t *ls, const char *lang );

/* Add/Update elements */
int             lang_str_add      
  ( lang_str_t *ls, const char *str, const char *lang, int update );
int             lang_str_append  
  ( lang_str_t *ls, const char *str, const char *lang );
int             lang_str_set
  ( lang_str_t **dst, const char *str, const char *lang );
int             lang_str_set2
  ( lang_str_t **dst, lang_str_t *src );

/* Serialize/Deserialize */
htsmsg_t       *lang_str_serialize_map
  ( lang_str_t *ls );
void            lang_str_serialize   
  ( lang_str_t *ls, htsmsg_t *msg, const char *f );
lang_str_t     *lang_str_deserialize_map
  ( htsmsg_t *map );
lang_str_t     *lang_str_deserialize
  ( htsmsg_t *m, const char *f );

/* Compare */
int             lang_str_compare ( lang_str_t *ls1, lang_str_t *ls2 );

/* Empty */
int             strempty(const char* c);
int             lang_str_empty(lang_str_t* str);

/* Init/Done */
void            lang_str_done( void );

#endif /* __TVH_LANG_STR_H__ */
