#include <stdio.h>
#include <string.h>

#include "tvheadend.h"
#include "prop.h"





/**
 *
 */
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


const property_t *
prop_find(const property_t *p, const char *id)
{
  int i = 0;
  for(;p[i].id; i++)
    if(!strcmp(id, p[i].id))
      return p + i;
  return NULL;
}



#define TO_FROM(x, y) ((x) << 16 | (y))

/**
 *
 */
void
prop_write_values(void *obj, const property_t *pl, htsmsg_t *m)
{
  htsmsg_field_t *f;
  HTSMSG_FOREACH(f, m) {
    if(f->hmf_name == NULL)
      continue;
    const property_t *p = prop_find(pl, f->hmf_name);
    if(p == NULL) {
      //fprintf(stderr, "Property %s unmappable\n", f->hmf_name);
      continue;
    }

    void *val = obj + p->off;
    switch(TO_FROM(p->type, f->hmf_type)) {
    case TO_FROM(PT_BOOL, HMF_BOOL):
      *(int *)val = f->hmf_bool;
      break;
    case TO_FROM(PT_BOOL, HMF_S64):
      *(int *)val = !!f->hmf_s64;
      break;
    case TO_FROM(PT_INT, HMF_S64):
      *(int *)val = f->hmf_s64;
      break;
    case TO_FROM(PT_U16, HMF_S64):
      *(uint16_t *)val = f->hmf_s64;
      break;
    case TO_FROM(PT_U32, HMF_S64):
      *(uint32_t *)val = f->hmf_s64;
      break;
    case TO_FROM(PT_STR, HMF_STR):
      if(p->str_set != NULL)
        p->str_set(obj, f->hmf_str);
      else
        mystrset(val, f->hmf_str);
      break;
    }
  }
}



/**
 *
 */
static void
prop_read_value(void *obj, const property_t *p, htsmsg_t *m, const char *name)
{
  const char *s;
  const void *val = obj + p->off;

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
    if(p->str_get != NULL)
      s = p->str_get(obj);
    else
      s = *(const char **)val;
    if(s != NULL) {
      htsmsg_add_str(m, name, s);
    }
    break;
  }
}

/**
 *
 */
void
prop_read_values(void *obj, const property_t *p, htsmsg_t *m)
{
  if(p == NULL)
    return;
  int i = 0;
  for(;p[i].id; i++)
    prop_read_value(obj, p+i, m, p[i].id);
}


/**
 *
 */
const static struct strtab typetab[] = {
  { "bool",  PT_BOOL },
  { "int",   PT_INT },
  { "str",   PT_STR },
  { "u16",   PT_U16 },
  { "u32",   PT_U32 },
};


/**
 *
 */
void
prop_add_params_to_msg(void *obj, const property_t *p, htsmsg_t *msg)
{
  if(p == NULL)
    return;
  int i = 0;
  for(;p[i].id; i++) {
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "id", p[i].id);
    htsmsg_add_str(m, "caption", p[i].name);
    htsmsg_add_str(m, "type", val2str(p[i].type, typetab) ?: "unknown");
    if (p->options & PO_RDONLY)
      htsmsg_add_u32(m, "rdonly", 1);
    if (p->options & PO_NOSAVE)
      htsmsg_add_u32(m, "nosave", 1);
    if (obj)
      prop_read_value(obj, p+i, m, "value");
    htsmsg_add_msg(msg, NULL, m);
  }
}


/**
 * value can be NULL
 *
 * Return 1 if we actually changed something
 */
static int
prop_seti(void *obj, const property_t *p, const char *value)
{
  int i;
  uint32_t u32;
  uint16_t u16;
  const char *s;

  if (p->options & PO_NOSAVE) return 0;

  void *val = obj + p->off;
  switch(p->type) {

  case PT_BOOL:
    i = str_to_bool(value);
    if(0)
  case PT_INT:
    i = value ? atoi(value) : 0;
    if(*(int *)val == i)
      return 0; // Already set
    *(int *)val = i;
    break;
  case PT_U16:
    u16 = value ? atoi(value) : 0;
    if(*(uint16_t *)val == u16)
      return 0;
    *(uint16_t *)val = u16;
    break;
  case PT_U32:
    u32 = value ? atoi(value) : 0;
    if(*(uint32_t *)val == u32)
      return 0;
    *(uint32_t *)val = u32;
    break;
  case PT_STR:
    if(p->str_get != NULL)
      s = p->str_get(obj);
    else
      s = *(const char **)val;

    if(!strcmp(s ?: "", value ?: ""))
      return 0;

    if(p->str_set != NULL)
      p->str_set(obj, value);
    else
      mystrset(val, value);
    break;
  }
  if(p->notify)
    p->notify(obj);
  return 1;
}


/**
 * Return 1 if something changed
 */
int
prop_set(void *obj, const property_t *p, const char *key, const char *value)
{
  if((p = prop_find(p, key)) == NULL)
    return -1;
  return prop_seti(obj, p, value);
}


/**
 *
 */
int
prop_update_all(void *obj, const property_t *p,
                const char *(*getvalue)(void *opaque, const char *key),
                void *opaque)
{
  int i = 0;
  int r = 0;
  for(; p[i].id; i++)
    r |= prop_seti(obj, p+i, getvalue(opaque, p[i].id));
  return r;
}
