/*
 *  Tvheadend - property system (part of idnode)
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

#ifndef __TVH_PROP_H__
#define __TVH_PROP_H__

#include <stddef.h>

#include "htsmsg.h"

/*
 * Property types
 */
typedef enum {
  PT_NONE,
  PT_BOOL,
  PT_STR,
  PT_INT,
  PT_U16,
  PT_U32,
  PT_DBL,
} prop_type_t;

/*
 * Property options
 */
#define PO_NONE     0x00
#define PO_RDONLY   0x01  // Property is read-only 
#define PO_NOSAVE   0x02  // Property is transient (not saved)
#define PO_WRONCE   0x04  // Property is write-once (i.e. on creation)
#define PO_ADVANCED 0x08  // Property is advanced
#define PO_HIDDEN   0x10  // Property is hidden (by default)
#define PO_USERAW   0x20  // Only save the RAW (off) value if it exists
#define PO_SORTKEY  0x40  // Sort using key (not display value)

/*
 * Property definition
 */
typedef struct property {
  const char  *id;        ///< Property Key
  const char *name;       ///< Textual description
  prop_type_t type;       ///< Type
  int         islist;     ///< Is a list
  size_t      off;        ///< Offset into object
  int         opts;       ///< Options

  /* String based processing */
  const void *(*get)  (void *ptr);
  int         (*set)  (void *ptr, const void *v);
  htsmsg_t   *(*list) (void *ptr);
  char       *(*rend) (void *ptr); ///< Provide the rendered value for enum/list
                                   ///< Lists should be CSV. This is used for
                                   ///< sorting and searching in UI API

  /* Default (for UI) */
  union {
    int         i;   // PT_BOOL/PT_INT
    const char *s;   // PR_STR
    uint16_t    u16; // PT_U16
    uint32_t    u32; // PR_U32
    double      d;   // PT_DBL
  } def;

  /* Notification callback */
  void        (*notify)   (void *ptr);

} property_t;

const property_t *prop_find(const property_t *p, const char *name);

int prop_write_values
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *updated);

void prop_read_values
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *inc);

void prop_serialize
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *inc);

#endif /* __TVH_PROP_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
