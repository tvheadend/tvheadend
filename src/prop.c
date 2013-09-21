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
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask,
   htsmsg_t *updated)
{
  int save, save2 = 0;
  htsmsg_field_t *f;
  const property_t *p;
  void *cur;
  const void *new;
  double dbl;
  int i;
  uint32_t u32;
  uint16_t u16;
#define PROP_UPDATE(v, t)\
  new = &v;\
  if (!p->set && (*((t*)cur) != *((t*)new))) {\
    save = 1;\
    *((t*)cur) = *((t*)new);\
  } (void)0

  if (!pl) return 0;

  for (p = pl; p->id; p++) {
    f = htsmsg_field_find(m, p->id);
    if (!f) continue;

    /* Ignore */
    if(p->opts & optmask) continue;

    /* Write */
    save = 0;
    cur  = obj + p->off;
    new  = NULL;

    /* List */
    if (p->islist)
      new = htsmsg_field_get_list(f);

    /* Singular */
    else {
      switch (p->type) {
      case PT_BOOL: {
        if (htsmsg_field_get_bool(f, &i))
          continue;
        PROP_UPDATE(i, int);
        break;
      }
      case PT_INT: {
        if (htsmsg_field_get_u32(f, &u32))
          continue;
        i = u32;
        PROP_UPDATE(i, int);
        break;
      }
      case PT_U16: {
        if (htsmsg_field_get_u32(f, &u32))
          continue;
        u16 = (uint16_t)u32;
        PROP_UPDATE(u16, uint16_t);
        break;
      }
      case PT_U32: {
        if (htsmsg_field_get_u32(f, &u32))
          continue;
        PROP_UPDATE(u32, uint32_t);
        break;
      }
      case PT_DBL: {
        if (htsmsg_field_get_dbl(f, &dbl))
          continue;
        PROP_UPDATE(dbl, double);
        break;
      }
      case PT_STR: {
        char **str = cur;
        if (!(new = htsmsg_field_get_str(f)))
          continue;
        if (!p->set && strcmp((*str) ?: "", new)) {
          free(*str);
          *str = strdup(new);
          save = 1;
        }
        break;
      }
      }
    }
  
    /* Setter */
    if (p->set && new)
      save = p->set(obj, new);

    /* Updated */
    if (save) {
      save2 = 1;
      if (p->notify)
        p->notify(obj);
      if (updated)
        htsmsg_set_u32(updated, p->id, 1);
    }
  }
#undef PROP_UPDATE
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
  (void *obj, const property_t *p, htsmsg_t *m, const char *name,
   int optmask, htsmsg_t *inc)
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

  /* List */
  if (p->islist)
    htsmsg_add_msg(m, name, (htsmsg_t*)val);
  
  /* Single */
  else {
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
      if ((s = *(const char **)val))
        htsmsg_add_str(m, name, s);
      break;
    case PT_DBL:
      htsmsg_add_dbl(m, name, *(double*)val);
      break;
    }
  }
}

/**
 *
 */
void
prop_read_values
  (void *obj, const property_t *pl, htsmsg_t *m, int optmask, htsmsg_t *inc)
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
prop_serialize
  (void *obj, const property_t *pl, htsmsg_t *msg, int optmask, htsmsg_t *inc)
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
    if (pl->islist)
      htsmsg_add_u32(m, "list", 1);

    /* Default */
    // TODO: currently no support for list defaults
    switch (pl->type) {
      case PT_BOOL:
        htsmsg_add_bool(m, "default", pl->def.i);
        break;
      case PT_INT:
        htsmsg_add_u32(m, "default", pl->def.i);
        break;
      case PT_U16:
        htsmsg_add_u32(m, "default", pl->def.u16);
        break;
      case PT_U32:
        htsmsg_add_u32(m, "default", pl->def.u32);
        break;
      case PT_DBL:
        htsmsg_add_dbl(m, "default", pl->def.d);
        break;
      case PT_STR:
        htsmsg_add_str(m, "default", pl->def.s ?: "");
        break;
    }

    /* Options */
    if (pl->opts & PO_RDONLY)
      htsmsg_add_bool(m, "rdonly", 1);
    if (pl->opts & PO_NOSAVE)
      htsmsg_add_bool(m, "nosave", 1);
    if (pl->opts & PO_WRONCE)
      htsmsg_add_bool(m, "wronce", 1);
    if (pl->opts & PO_ADVANCED)
      htsmsg_add_bool(m, "advanced", 1);
    if (pl->opts & PO_HIDDEN)
      htsmsg_add_bool(m, "hidden", 1);

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
