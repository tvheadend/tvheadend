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

#include <stdio.h>
#include <string.h>

#include "tvheadend.h"
#include "prop.h"

/* **************************************************************************
 * Utilities
 * *************************************************************************/

#define TO_FROM(x, y) ((x) << 16 | (y))

/*
 * Bool conversion
 */
#if 0
static int
str_to_bool(const char *s)
{
  if(s == NULL)
    return 0;
  int v = atoi(s);
  if(v)
    return 1;
  if(!strcasecmp(s, "on") ||
     !strcasecmp(s, "true") ||
     !strcasecmp(s, "yes"))
    return 1;
  return 0;
}
#endif

/**
 *
 */
const static struct strtab typetab[] = {
  { "bool",  PT_BOOL },
  { "int",   PT_INT },
  { "str",   PT_STR },
  { "u16",   PT_U16 },
  { "u32",   PT_U32 },
  { "dbl",   PT_DBL },
};


const property_t *
prop_find(const property_t *p, const char *id)
{
  for(; p->id; p++)
    if(!strcmp(id, p->id))
      return p;
  return NULL;
}

/* **************************************************************************
 * Write
 * *************************************************************************/

/**
 *
 */
int
prop_write_values
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *updated)
{
  int save, save2 = 0;
  htsmsg_field_t *f;
  const property_t *p;

  if (!pl) return 0;

  for (p = pl; p->id; p++) {
    f = htsmsg_field_find(m, p->id);
    if (!f) continue;

    /* Ignore */
    if(p->opts & optmask) continue;

    /* Write */
    save = 0;
    void *v = obj + p->off;
    switch(TO_FROM(p->type, f->hmf_type)) {
    case TO_FROM(PT_BOOL, HMF_BOOL): {
      int *val = v;
      if (*val != f->hmf_bool) {
        *val = f->hmf_bool;
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_BOOL, HMF_S64): {
      int *val = v;
      if (*val != !!f->hmf_s64) {
        *val = !!f->hmf_s64;
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_INT, HMF_S64): {
      int *val = v;
      if (*val != f->hmf_s64) {
        *val = f->hmf_s64;
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_U16, HMF_S64): {
      uint16_t *val = v;
      if (*val != f->hmf_s64) {
        *val = f->hmf_s64;
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_U32, HMF_S64): {
      uint32_t *val = v;
      if (*val != f->hmf_s64) {
        *val = f->hmf_s64;
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_STR, HMF_STR): {
      char **val = v;
      if(p->set != NULL)
        save |= p->set(obj, f->hmf_str);
      else if (strcmp(*val ?: "", f->hmf_str)) {
        free(*val);
        *val = strdup(f->hmf_str);
        save = 1;
      }
      break;
    }
    case TO_FROM(PT_DBL, HMF_DBL): {
      double *val = v;
      if (*val != f->hmf_dbl) {
        *val = f->hmf_dbl;
        save = 1;
      }
      break;
    }
    }
  
    if (save) {
      save2 = 1;
      if (p->notify)
        p->notify(obj);
      if (updated)
        htsmsg_set_u32(updated, p->id, 1);
    }
  }
  return save2;
}

/* **************************************************************************
 * Read
 * *************************************************************************/

/**
 *
 */
static void
prop_read_value
  (void *obj, const property_t *p, htsmsg_t *m, const char *name, int optmask, htsmsg_t *inc)
{
  const char *s;
  const void *val = obj + p->off;

  /* Ignore */
  if (p->opts & optmask) return;

  /* Ignore */
  if (inc && !htsmsg_get_u32_or_default(inc, p->id, 0))
    return;

  /* Get method */
  if (p->get)
    val = p->get(obj);

  /* Read */
  switch(p->type) {
  case PT_BOOL:
    htsmsg_add_bool(m, name, *(int *)val);
    break;
  case PT_INT:
    htsmsg_add_s64(m, name, *(int *)val);
    break;
  case PT_U32:
    htsmsg_add_u32(m, name, *(uint32_t *)val);
    break;
  case PT_U16:
    htsmsg_add_u32(m, name, *(uint16_t *)val);
    break;
  case PT_STR:
    s = *(const char **)val;
    if(s != NULL) {
      htsmsg_add_str(m, name, s);
    }
    break;
  case PT_DBL:
    htsmsg_add_dbl(m, name, *(double*)val);
    break;
  }
}

/**
 *
 */
void
prop_read_values(void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *inc)
{
  if(pl == NULL)
    return;
  for (; pl->id; pl++)
    prop_read_value(obj, pl, m, pl->id, optmask, inc);
}

/**
 *
 */
void
prop_serialize(void *obj, const property_t *pl, htsmsg_t *msg, int optmask, htsmsg_t *inc)
{
  if(pl == NULL)
    return;

  for(; pl->id; pl++) {

    /* Ignore */
    if (inc && !htsmsg_get_u32_or_default(inc, pl->id, 0))
      continue;

    htsmsg_t *m = htsmsg_create_map();

    /* Metadata */
    htsmsg_add_str(m, "id",       pl->id);
    htsmsg_add_str(m, "caption",  pl->name);
    htsmsg_add_str(m, "type",     val2str(pl->type, typetab) ?: "unknown");

    /* Options */
    if (pl->opts & PO_RDONLY)
      htsmsg_add_u32(m, "rdonly", 1);
    if (pl->opts & PO_NOSAVE)
      htsmsg_add_u32(m, "nosave", 1);
    if (pl->opts & PO_WRONCE)
      htsmsg_add_u32(m, "wronce", 1);

    /* Enum list */
    if (pl->list)
      htsmsg_add_msg(m, "enum", pl->list(obj));

    /* Data */
    if (obj)
      prop_read_value(obj, pl, m, "value", optmask, NULL);

    htsmsg_add_msg(msg, NULL, m);
  }
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
