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

struct access;
typedef struct idnode idnode_t;

/*
 * Node set
 */
typedef struct idnode_set
{
  idnode_t **is_array;  ///< Array of nodes
  size_t     is_alloc;  ///< Size of is_array
  size_t     is_count;  ///< Current usage of is_array
  uint8_t    is_sorted; ///< Sorted array of nodes
} idnode_set_t;

/*
 * Property groups
 */
typedef struct property_group
{
  const char *name;
  uint32_t    number;
  uint32_t    parent;
  uint32_t    column;
} property_group_t;

/*
 * Class definition
 */
#define IDCLASS_ALWAYS_SAVE    (1<<0)      ///< Always call the save callback

typedef struct idclass idclass_t;
struct idclass {
  const struct idclass   *ic_super;        ///< Parent class
  const char             *ic_class;        ///< Class name
  const char             *ic_caption;      ///< Class description
  const char             *ic_order;        ///< Property order (comma separated)
  const property_group_t *ic_groups;       ///< Groups for visual representation
  const property_t       *ic_properties;   ///< Property list
  const char             *ic_event;        ///< Events to fire on add/delete/title
  uint32_t                ic_perm_def;     ///< Default permissions
  uint32_t                ic_flags;        ///< Extra flags
  idnode_t               *ic_snode;        ///< Simple node

  /* Callbacks */
  idnode_set_t   *(*ic_get_childs) (idnode_t *self);
  const char     *(*ic_get_title)  (idnode_t *self, const char *lang);
  void            (*ic_save)       (idnode_t *self);
  void            (*ic_delete)     (idnode_t *self);
  void            (*ic_moveup)     (idnode_t *self);
  void            (*ic_movedown)   (idnode_t *self);
  int             (*ic_perm)       (idnode_t *self, struct access *a, htsmsg_t *msg_to_write);
};


typedef RB_HEAD(, idnode) idnodes_rb_t;

/*
 * Node definition
 */
struct idnode {
  tvh_uuid_t        in_uuid;                ///< Unique ID
  RB_ENTRY(idnode)  in_link;                ///< Global hash
  RB_ENTRY(idnode)  in_domain_link;         ///< Root class link (domain)
  idnodes_rb_t     *in_domain;              ///< Domain nodes
  const idclass_t  *in_class;               ///< Class definition
  struct access    *in_access;              ///< Actual permissions

};

/*
 * Node list mapping definition
 */
struct idnode_list_mapping;

typedef struct idnode_list_head {
  struct idnode_list_mapping *lh_first;
} idnode_list_head_t;

typedef struct idnode_list_mapping {
  LIST_ENTRY(idnode_list_mapping) ilm_in1_link;
  LIST_ENTRY(idnode_list_mapping) ilm_in2_link;

  idnode_t *ilm_in1;
  idnode_t *ilm_in2;

  uint8_t ilm_in1_save;
  uint8_t ilm_in2_save;
  uint8_t ilm_mark;
} idnode_list_mapping_t;

/*
 * Sorting definition
 */
typedef struct idnode_sort {
  const char *lang; ///< Language (UI)
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

  int checked;
  char *key;                          ///< Filter key
  enum {
    IF_STR,
    IF_NUM,
    IF_DBL,
    IF_BOOL
  } type;                             ///< Filter type
  union {
    int      b;
    char    *s;
    struct {
      int64_t n;
      int64_t intsplit;
    } n;
    double   dbl;
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

extern idnode_t tvhlog_conf;
extern const idclass_t tvhlog_conf_class;

void idnode_init(void);
void idnode_done(void);

#define IDNODE_SHORT_UUID (1<<0)

int  idnode_insert(idnode_t *in, const char *uuid, const idclass_t *idc, int flags);
void idnode_unlink(idnode_t *in);

uint32_t      idnode_get_short_uuid (const idnode_t *in);
const char   *idnode_uuid_as_str  (const idnode_t *in, char *buf);
idnode_set_t *idnode_get_childs   (idnode_t *in);
const char   *idnode_get_title    (idnode_t *in, const char *lang);
int           idnode_is_leaf      (idnode_t *in);
int           idnode_is_instance  (idnode_t *in, const idclass_t *idc);
void          idnode_delete       (idnode_t *in);
void          idnode_moveup       (idnode_t *in);
void          idnode_movedown     (idnode_t *in);

void          idnode_changed      (idnode_t *in);

void         *idnode_find    (const char *uuid, const idclass_t *idc, const idnodes_rb_t *nodes);
idnode_set_t *idnode_find_all(const idclass_t *idc, const idnodes_rb_t *nodes);


void idnode_notify (idnode_t *in, const char *action);
void idnode_notify_changed (void *in);
void idnode_notify_title_changed (void *in, const char *lang);

void idclass_register ( const idclass_t *idc );
const idclass_t *idclass_find ( const char *name );
const char *idclass_get_caption ( const idclass_t *idc, const char *lang );
htsmsg_t *idclass_serialize0 (const idclass_t *idc, htsmsg_t *list, int optmask, const char *lang);
htsmsg_t *idnode_serialize0  (idnode_t *self, htsmsg_t *list, int optmask, const char *lang);
void      idnode_read0  (idnode_t *self, htsmsg_t *m, htsmsg_t *list, int optmask, const char *lang);
int       idnode_write0 (idnode_t *self, htsmsg_t *m, int optmask, int dosave);

#define idclass_serialize(idc, lang) idclass_serialize0(idc, NULL, 0, lang)
#define idnode_serialize(in, lang)   idnode_serialize0(in, NULL, 0, lang)
#define idnode_load(in, m)     idnode_write0(in, m, PO_NOSAVE, 0)
#define idnode_save(in, m)     idnode_read0(in, m, NULL, PO_NOSAVE | PO_USERAW, NULL)
#define idnode_update(in, m)   idnode_write0(in, m, PO_RDONLY | PO_WRONCE, 1)

int idnode_perm(idnode_t *self, struct access *a, htsmsg_t *msg_to_write);
static inline void idnode_perm_unset(idnode_t *self) { self->in_access = NULL; }

#define idnode_lang(self) \
  (((idnode_t *)self)->in_access ? \
   ((idnode_t *)self)->in_access->aa_lang : NULL)
#define idnode_lang_ui(self) \
  (((idnode_t *)self)->in_access ? \
   ((idnode_t *)self)->in_access->aa_lang_ui : NULL)

idnode_list_mapping_t * idnode_list_link
                       ( idnode_t *in1, idnode_list_head_t *in1_list,
                         idnode_t *in2, idnode_list_head_t *in2_list,
                         void *origin, uint32_t savemask );
void idnode_list_unlink ( idnode_list_mapping_t *ilm, void *origin );
void idnode_list_destroy ( idnode_list_head_t *ilh, void *origin );
htsmsg_t * idnode_list_get1 ( idnode_list_head_t *in1_list );
htsmsg_t * idnode_list_get2 ( idnode_list_head_t *in2_list );
char * idnode_list_get_csv1 ( idnode_list_head_t *in1_list, const char *lang );
char * idnode_list_get_csv2 ( idnode_list_head_t *in2_list, const char *lang );
int idnode_list_set1 ( idnode_t *in1, idnode_list_head_t *in1_list,
                       const idclass_t *in2_class, htsmsg_t *in2_list,
                       int (*in2_create)(idnode_t *in1, idnode_t *in2, void *origin) );
int idnode_list_set2 ( idnode_t *in2, idnode_list_head_t *in2_list,
                       const idclass_t *in1_class, htsmsg_t *in1_list,
                       int (*in2_create)(idnode_t *in1, idnode_t *in2, void *origin) );

const char *idnode_get_str (idnode_t *self, const char *key );
int         idnode_get_u32 (idnode_t *self, const char *key, uint32_t *u32);
int         idnode_get_s64 (idnode_t *self, const char *key,  int64_t *s64);
int         idnode_get_dbl (idnode_t *self, const char *key,   double *dbl);
int         idnode_get_bool(idnode_t *self, const char *key, int *b);
int         idnode_get_time(idnode_t *self, const char *key, time_t *tm);

void idnode_filter_add_str
  (idnode_filter_t *f, const char *k, const char *v, int t);
void idnode_filter_add_num
  (idnode_filter_t *f, const char *k, int64_t s64, int t, int64_t intsplit);
void idnode_filter_add_dbl
  (idnode_filter_t *f, const char *k, double dbl, int t);
void idnode_filter_add_bool
  (idnode_filter_t *f, const char *k, int b, int t);
void idnode_filter_clear
  (idnode_filter_t *f);
int  idnode_filter
  ( idnode_t *in, idnode_filter_t *filt, const char *lang );
static inline idnode_set_t * idnode_set_create(int sorted)
  { idnode_set_t *is = calloc(1, sizeof(idnode_set_t));
    is->is_sorted = sorted; return is; }
void idnode_set_alloc ( idnode_set_t *is, size_t alloc );
void idnode_set_add
  ( idnode_set_t *is, idnode_t *in, idnode_filter_t *filt, const char *lang );
int idnode_set_remove ( idnode_set_t *is, idnode_t *in );
ssize_t idnode_set_find_index( idnode_set_t *is, idnode_t *in );
static inline int idnode_set_exists ( idnode_set_t *is, idnode_t *in )
  { return idnode_set_find_index(is, in) >= 0; }
void idnode_set_sort ( idnode_set_t *is, idnode_sort_t *s );
void idnode_set_sort_by_title ( idnode_set_t *is, const char *lang );
htsmsg_t *idnode_set_as_htsmsg ( idnode_set_t *is );
void idnode_set_clear ( idnode_set_t *is );
void idnode_set_free ( idnode_set_t *is );

#endif /* __TVH_IDNODE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
