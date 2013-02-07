#include "prop.h"

#if 0
/**
 *
 */
void
prop_write_values(void *ptr, const property_t p[], htsmsg_t *m)
{
  int i = 0;
  for(;p[i].id; i++) {
    switch(p[i].type) {
    case PT_BOOL:
      htsmsg_add_bool(m, p[i].id, *(int *)(ptr + p[i].off));
      break;
    case PT_INT:
      htsmsg_add_s32(m, p[i].id, *(int *)(ptr + p[i].off));
      break;
    case PT_STR:
      htsmsg_add_str(m, p[i].id, (const char *)(ptr + p[i].off));
      break;
    }
  }
}
#endif


/**
 *
 */
void
prop_read_values(void *ptr, const property_t p[], htsmsg_t *m)
{
  int i = 0;
  for(;p[i].id; i++) {
    switch(p[i].type) {
    case PT_BOOL:
      htsmsg_add_bool(m, p[i].id, *(int *)(ptr + p[i].off));
      break;
    case PT_INT:
      htsmsg_add_s32(m, p[i].id, *(int *)(ptr + p[i].off));
      break;
    case PT_STR:
      htsmsg_add_str(m, p[i].id, (const char *)(ptr + p[i].off));
      break;
    }
  }
}


/**
 *
 */
htsmsg_t *
prop_get_values(void *ptr, const property_t p[])
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_values(ptr, p, m);
  return m;
}


/**
 *
 */
void
prop_read_names(const property_t p[], htsmsg_t *m)
{
  int i = 0;
  for(;p[i].name; i++)
    htsmsg_add_str(m, p[i].id, p[i].name);
}


/**
 *
 */
htsmsg_t *
prop_get_names(const property_t p[])
{
  htsmsg_t *m = htsmsg_create_map();
  prop_read_names(p, m);
  return m;
}
