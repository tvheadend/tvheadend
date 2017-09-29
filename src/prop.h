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
  PT_S64,
  PT_S64_ATOMIC,
  PT_DBL,
  PT_TIME,
  PT_LANGSTR,
  PT_PERM,                // like PT_U32 but with the special save
} prop_type_t;

/*
 * Property options
 */
#define PO_NONE      (0<<0)
#define PO_RDONLY    (1<<1)  // Property is read-only
#define PO_NOSAVE    (1<<2)  // Property is transient (not saved)
#define PO_WRONCE    (1<<3)  // Property is write-once (i.e. on creation)
#define PO_ADVANCED  (1<<4)  // Property is advanced
#define PO_EXPERT    (1<<5)  // Property is for experts
#define PO_NOUI      (1<<6)  // Property should not be presented in the user interface
#define PO_HIDDEN    (1<<7)  // Property column is hidden (by default)
#define PO_PHIDDEN   (1<<8)  // Property is permanently hidden
#define PO_USERAW    (1<<9)  // Only save the RAW (off) value if it exists
#define PO_SORTKEY   (1<<10) // Sort using key (not display value)
#define PO_PASSWORD  (1<<11) // String is a password
#define PO_DURATION  (1<<12) // For PT_TIME - differentiate between duration and datetime
#define PO_HEXA      (1<<13) // Hexadecimal value
#define PO_DATE      (1<<14) // Show date only
#define PO_LOCALE    (1<<15) // Call tvh_locale_lang on string
#define PO_LORDER    (1<<16) // Manage order in lists
#define PO_MULTILINE (1<<17) // Multiline string
#define PO_PERSIST   (1<<18) // Persistent value (return back on save)
#define PO_DOC       (1<<19) // Use doc callback instead description if exists
#define PO_DOC_NLIST (1<<20) // Do not show list in doc
#define PO_TRIM      (1<<21) // Trim whitespaces (left & right) on load

/*
 * min/max/step helpers
 */
#define INTEXTRA_RANGE(min, max, step) \
  ((1<<31)|(((step)&0x7f)<<24)|(((max)&0xfff)<<12)|((min)&0xfff))

#define INTEXTRA_IS_RANGE(e) (((e) & (1<<31)) != 0)
#define INTEXTRA_IS_SPLIT(e) !INTEXTRA_IS_RANGE(e)
#define INTEXTRA_GET_STEP(e) (((e)>>24)&0x7f)
#define INTEXTRA_GET_MAX(e)  ((e)&(1<<23)?-(((e)>>12)&0x7ff):(((e)>>12)&0x7ff))
#define INTEXTRA_GET_MIN(e)  ((e)&(1<<11)?-((e)&0x7ff):((e)&0x7ff))

/*
 * Property definition
 */
typedef struct property {
  const char *id;         ///< Property Key
  const char *name;       ///< Textual description
  const char *desc;       ///< Verbose description (tooltip)
  prop_type_t type;       ///< Type
  uint8_t     islist;     ///< Is a list
  uint8_t     group;      ///< Visual group ID (like ExtJS FieldSet)
  size_t      off;        ///< Offset into object
  uint32_t    opts;       ///< Options
  uint32_t    intextra;   ///< intsplit: integer/remainder boundary or range: min/max/step

  /* String based processing */
  const void *(*get)  (void *ptr);
  int         (*set)  (void *ptr, const void *v);
  htsmsg_t   *(*list) (void *ptr, const char *lang);
  char       *(*rend) (void *ptr, const char *lang);
                          ///< Provide the rendered value for enum/list
                          ///< Lists should be CSV. This is used for
                          ///< sorting and searching in UI API

  /* Default (for UI) */
  union {
    int         i;   // PT_BOOL/PT_INT
    const char *s;   // PT_STR
    uint16_t    u16; // PT_U16
    uint32_t    u32; // PT_U32
    int64_t     s64; // PT_S64
    double      d;   // PT_DBL
    time_t      tm;  // PT_TIME
    htsmsg_t *(*list)(void); // islist != 0
  } def;

  /* Extended options */
  uint32_t    (*get_opts) (void *ptr, uint32_t opts);

  /* Documentation callback */
  char       *(*doc) ( const struct property *prop, const char *lang );

  /* Notification callback */
  void        (*notify)   (void *ptr, const char *lang);

} property_t;

#define PROP_SBUF_LEN 4096
extern char prop_sbuf[PROP_SBUF_LEN];
extern const char *prop_sbuf_ptr;
extern const char *prop_ptr;

const property_t *prop_find(const property_t *p, const char *name);

int prop_write_values
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *updated);

void prop_read_values
  (void *obj, const property_t *pl, htsmsg_t *m, htsmsg_t *list,
   int optmask, const char *lang);

void prop_serialize
  (void *obj, const property_t *pl, htsmsg_t *m, htsmsg_t *list,
   int optmask, const char *lang);

static inline int64_t prop_intsplit_from_str(const char *s, int64_t intsplit)
{
  int64_t s64 = (int64_t)atol(s) * intsplit;
  if ((s = strchr(s, '.')) != NULL)
    s64 += (atol(s + 1) % intsplit);
  return s64;
}

char *
prop_md_doc(const char **md, const char *lang);

#define PROP_DOC(name) \
extern const char *tvh_doc_##name##_property[]; \
static char * \
prop_doc_##name(const struct property *p, const char *lang) \
{ return prop_md_doc(tvh_doc_##name##_property, lang); }


/* helpers */
htsmsg_t *proplib_kill_list ( void *o, const char *lang );

#endif /* __TVH_PROP_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
