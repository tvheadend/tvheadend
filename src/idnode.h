/*
 *  Tvheadend - idnode (class) system
 *
 *  Copyright (C) 2013 Andreas Öman
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

#ifndef __TVH_IDNODE_H__
#define __TVH_IDNODE_H__

#include "tvheadend.h"
#include "prop.h"
#include "uuid.h"

#include <regex.h>

struct htsmsg;
typedef struct idnode idnode_t;

/*
 * Node set
 */
typedef struct idnode_set
{
  idnode_t **is_array;  ///< Array of nodes
  size_t     is_alloc;  ///< Size of is_array
  size_t     is_count;  ///< Current usage of is_array
} idnode_set_t;

/*
 * Class definition
 */
typedef struct idclass idclass_t;
struct idclass {
  const struct idclass  *ic_super;      /// Parent class
  const char            *ic_class;      /// Class name
  const char            *ic_caption;    /// Class description
  const char            *ic_order;      /// Property order (comma separated)
  const property_t      *ic_properties; /// Property list
  const char            *ic_event;      /// Events to fire on add/delete/title

  /* Callbacks */
  idnode_set_t   *(*ic_get_childs) (idnode_t *self);
  const char     *(*ic_get_title)  (idnode_t *self);
  void            (*ic_save)       (idnode_t *self);
  void            (*ic_delete)     (idnode_t *self);
  void            (*ic_moveup)     (idnode_t *self);
  void            (*ic_movedown)   (idnode_t *self);
};

/*
 * Node definition
 */
struct idnode {
  uint8_t           in_uuid[UUID_BIN_SIZE]; ///< Unique ID
  RB_ENTRY(idnode)  in_link;                ///< Global hash
  const idclass_t  *in_class;               ///< Class definition
};

/*
 * Sorting definition
 */
typedef struct idnode_sort {
  const char *key;  ///< Sort key
  enum {
    IS_ASC,
    IS_DSC
  }           dir;  ///< Sort direction
} idnode_sort_t;

/*
 * Filter definition
 */
typedef struct idnode_filter_ele
{
  LIST_ENTRY(idnode_filter_ele) link; ///< List link

  char *key;                          ///< Filter key
  enum {
    IF_STR,
    IF_NUM,
    IF_BOOL
  } type;                             ///< Filter type
  union {
    int      b;
    char    *s;
    int64_t  n;
    regex_t  re;
  } u;                                ///< Filter data
  enum {
    IC_EQ, ///< Equals
    IC_LT, ///< LT
    IC_GT, ///< GT
    IC_IN, ///< contains (STR only)
    IC_RE, ///< regexp (STR only)
  } comp;                             ///< Filter comparison
} idnode_filter_ele_t;

typedef LIST_HEAD(,idnode_filter_ele) idnode_filter_t;

void idnode_init(void);
void idnode_done(void);

#define IDNODE_SHORT_UUID (1<<0)

int  idnode_insert(idnode_t *in, const char *uuid, const idclass_t *idc, int flags);
void idnode_unlink(idnode_t *in);

uint32_t      idnode_get_short_uuid (const idnode_t *in);
const char   *idnode_uuid_as_str  (const idnode_t *in);
idnode_set_t *idnode_get_childs   (idnode_t *in);
const char   *idnode_get_title    (idnode_t *in);
int           idnode_is_leaf      (idnode_t *in);
int           idnode_is_instance  (idnode_t *in, const idclass_t *idc);
void          idnode_delete       (idnode_t *in);
void          idnode_moveup       (idnode_t *in);
void          idnode_movedown     (idnode_t *in);

void         *idnode_find    (const char *uuid, const idclass_t *idc);
idnode_set_t *idnode_find_all(const idclass_t *idc);

#define idnode_updated(in) idnode_notify(in, NULL, 0, 0)
void idnode_notify
  (idnode_t *in, const char *chn, int force, int event);
void idnode_notify_simple (void *in);
void idnode_notify_title_changed (void *in);

void idclass_register ( const idclass_t *idc );
const idclass_t *idclass_find ( const char *name );
htsmsg_t *idclass_serialize0 (const idclass_t *idc, int optmask);
htsmsg_t *idnode_serialize0  (idnode_t *self, int optmask);
void      idnode_read0  (idnode_t *self, htsmsg_t *m, int optmask);
int       idnode_write0 (idnode_t *self, htsmsg_t *m, int optmask, int dosave);

#define idclass_serialize(idc) idclass_serialize0(idc, 0)
#define idnode_serialize(in)   idnode_serialize0(in, 0)
#define idnode_load(in, m)     idnode_write0(in, m, PO_NOSAVE, 0)
#define idnode_save(in, m)     idnode_read0(in, m, PO_NOSAVE | PO_USERAW)
#define idnode_update(in, m)   idnode_write0(in, m, PO_RDONLY | PO_WRONCE, 1)

const char *idnode_get_str (idnode_t *self, const char *key );
int         idnode_get_u32 (idnode_t *self, const char *key, uint32_t *u32);
int         idnode_get_bool(idnode_t *self, const char *key, int *b);

void idnode_filter_add_str
  (idnode_filter_t *f, const char *k, const char *v, int t);
void idnode_filter_add_num
  (idnode_filter_t *f, const char *k, int64_t s64, int t);
void idnode_filter_add_bool
  (idnode_filter_t *f, const char *k, int b, int t);
void idnode_filter_clear
  (idnode_filter_t *f);
int  idnode_filter
  ( idnode_t *in, idnode_filter_t *filt );
#define idnode_set_create() calloc(1, sizeof(idnode_set_t))
void idnode_set_add
  ( idnode_set_t *is, idnode_t *in, idnode_filter_t *filt );
int idnode_set_exists ( idnode_set_t *is, idnode_t *in );
void idnode_set_sort    ( idnode_set_t *is, idnode_sort_t *s );
void idnode_set_sort_by_title ( idnode_set_t *is );
htsmsg_t *idnode_set_as_htsmsg ( idnode_set_t *is );
void idnode_set_free    ( idnode_set_t *is );

#endif /* __TVH_IDNODE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
