#include <stdio.h>
#include <string.h>

#include "tvheadend.h"
#include "prop.h"




static const property_t *
prop_find(const property_t *p, const char *id)
{
  int i;
  for(;p[i].id; i++)
    if(!strcmp(id, p[i].id))
      return p;
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
      fprintf(stderr, "Property %s unmappable\n", f->hmf_name);
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
void
prop_read_values(void *obj, const property_t *p, htsmsg_t *m)
{
  const char *s;
  if(p == NULL)
    return;
  int i = 0;
  for(;p[i].id; i++) {
    void *val = obj + p[i].off;
    switch(p[i].type) {
    case PT_BOOL:
      htsmsg_add_bool(m, p[i].id, *(int *)val);
      break;
    case PT_INT:
      htsmsg_add_s32(m, p[i].id, *(int *)val);
      break;
    case PT_STR:
      if(p->str_get != NULL)
        s = p->str_get(obj);
      else
        s = *(const char **)val;
      if(s != NULL)
        htsmsg_add_str(m, p[i].id, s);
      break;
    }
  }
}


/**
 *
 */
htsmsg_t *
prop_get_values(void *ptr, const property_t *p)
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_values(ptr, p, m);
  return m;
}


/**
 *
 */
void
prop_read_names(const property_t *p, htsmsg_t *m)
{
  if(p == NULL)
    return;
  int i = 0;
  for(;p[i].id; i++)
    htsmsg_add_str(m, p[i].id, p[i].name);
}


/**
 *
 */
htsmsg_t *
prop_get_names(const property_t *p)
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_names(p, m);
  return m;
}
