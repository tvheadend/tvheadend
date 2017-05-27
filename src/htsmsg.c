/*
 *  Functions for manipulating HTS messages
 *  Copyright (C) 2007 Andreas Öman
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

#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "build.h"
#include "htsmsg.h"
#include "misc/dbl.h"
#include "htsmsg_json.h"
#include "memoryinfo.h"

#if ENABLE_SLOW_MEMORYINFO
memoryinfo_t htsmsg_memoryinfo = { .my_name = "htsmsg" };
memoryinfo_t htsmsg_field_memoryinfo = { .my_name = "htsmsg field" };
#endif

static void htsmsg_clear(htsmsg_t *msg);
static htsmsg_t *htsmsg_field_get_msg ( htsmsg_field_t *f, int islist );

/**
 *
 */
static void
htsmsg_field_data_destroy(htsmsg_field_t *f)
{
  switch(f->hmf_type) {
  case HMF_MAP:
  case HMF_LIST:
    htsmsg_clear(&f->hmf_msg);
    break;

  case HMF_STR:
    if(f->hmf_flags & HMF_ALLOCED) {
#if ENABLE_SLOW_MEMORYINFO
      memoryinfo_remove(&htsmsg_field_memoryinfo, strlen(f->hmf_str) + 1);
#endif
      free((void *)f->hmf_str);
    }
    break;

  case HMF_BIN:
    if(f->hmf_flags & HMF_ALLOCED) {
#if ENABLE_SLOW_MEMORYINFO
      memoryinfo_remove(&htsmsg_field_memoryinfo, f->hmf_binsize);
#endif
      free((void *)f->hmf_bin);
    }
    break;
  default:
    break;
  }
  f->hmf_flags &= ~HMF_ALLOCED;
}

/**
 *
 */
void
htsmsg_field_destroy(htsmsg_t *msg, htsmsg_field_t *f)
{
#if ENABLE_SLOW_MEMORYINFO
  size_t asize = 0;
#endif

  TAILQ_REMOVE(&msg->hm_fields, f, hmf_link);

  htsmsg_field_data_destroy(f);

  if (f->hmf_flags & HMF_NAME_ALLOCED) {
#if ENABLE_SLOW_MEMORYINFO
    asize += strlen(f->hmf_name);
#endif
    free((void *)f->hmf_name);
  }
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_free(&htsmsg_field_memoryinfo,
                  sizeof(*f) + f->hmf_edata_size + asize);
#endif
  free(f);
}

/*
 *
 */
static void
htsmsg_clear(htsmsg_t *msg)
{
  htsmsg_field_t *f;

  while((f = TAILQ_FIRST(&msg->hm_fields)) != NULL)
    htsmsg_field_destroy(msg, f);
}


/*
 *
 */
htsmsg_field_t *
htsmsg_field_add(htsmsg_t *msg, const char *name, int type, int flags, size_t esize)
{
  size_t nsize = 0;
  htsmsg_field_t *f;
#if ENABLE_SLOW_MEMORYINFO
  size_t asize = 0;
#endif
  
  if((flags & HMF_NAME_INALLOCED) && name)
    nsize = strlen(name) + 1;
  f = malloc(sizeof(htsmsg_field_t) + nsize + esize);
  if(f == NULL)
    return NULL;
  TAILQ_INSERT_TAIL(&msg->hm_fields, f, hmf_link);

  if(msg->hm_islist) {
    assert(name == NULL);
  } else {
    assert(name != NULL);
  }

  if(flags & HMF_NAME_INALLOCED) {
    if (name) {
      f->hmf_name = f->hmf_edata;
      strcpy(f->hmf_edata, name);
    } else {
      f->hmf_name = NULL;
    }
  } else if(flags & HMF_NAME_ALLOCED) {
    f->hmf_name = name ? strdup(name) : NULL;
#if ENABLE_SLOW_MEMORYINFO
    asize = name ? strlen(name) + 1 : 0;
#endif
  } else {
    f->hmf_name = name;
  }

  if(esize) {
    if(type == HMF_STR)
      f->hmf_str = f->hmf_edata + nsize;
    else if(type == HMF_BIN)
      f->hmf_bin = f->hmf_edata + nsize;
  }

  f->hmf_type = type;
  f->hmf_flags = flags;
#if ENABLE_SLOW_MEMORYINFO
  f->hmf_edata_size = nsize + esize;
  memoryinfo_alloc(&htsmsg_field_memoryinfo,
                   sizeof(htsmsg_field_t) + f->hmf_edata_size + asize);
#endif
  return f;
}


/*
 *
 */
htsmsg_field_t *
htsmsg_field_find(htsmsg_t *msg, const char *name)
{
  htsmsg_field_t *f;

  if (msg == NULL || name == NULL)
    return NULL;
  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    if(f->hmf_name != NULL && !strcmp(f->hmf_name, name))
      return f;
  }
  return NULL;
}


/*
 *
 */
htsmsg_field_t *
htsmsg_field_last(htsmsg_t *msg)
{
  if (msg == NULL)
    return NULL;
  return TAILQ_LAST(&msg->hm_fields, htsmsg_field_queue);
}


/**
 *
 */
int
htsmsg_delete_field(htsmsg_t *msg, const char *name)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  htsmsg_field_destroy(msg, f);
  return 0;
}


/**
 *
 */
int
htsmsg_is_empty(htsmsg_t *msg)
{
  if (msg == NULL)
    return 1;

  assert(msg->hm_data == NULL);
  return TAILQ_EMPTY(&msg->hm_fields);
}


/*
 *
 */
htsmsg_t *
htsmsg_create_map(void)
{
  htsmsg_t *msg;

  msg = malloc(sizeof(htsmsg_t));
  if (msg) {
    TAILQ_INIT(&msg->hm_fields);
    msg->hm_data = NULL;
    msg->hm_data_size = 0;
    msg->hm_islist = 0;
#if ENABLE_SLOW_MEMORYINFO
    memoryinfo_alloc(&htsmsg_memoryinfo, sizeof(htsmsg_t));
#endif
  }
  return msg;
}

/*
 *
 */
htsmsg_t *
htsmsg_create_list(void)
{
  htsmsg_t *msg;

  msg = malloc(sizeof(htsmsg_t));
  if (msg) {
    TAILQ_INIT(&msg->hm_fields);
    msg->hm_data = NULL;
    msg->hm_data_size = 0;
    msg->hm_islist = 1;
#if ENABLE_SLOW_MEMORYINFO
    memoryinfo_alloc(&htsmsg_memoryinfo, sizeof(htsmsg_t));
#endif
  }
  return msg;
}


/*
 *
 */
void
htsmsg_destroy(htsmsg_t *msg)
{
#if ENABLE_SLOW_MEMORYINFO
  size_t size = 0;
#endif

  if(msg == NULL)
    return;

  htsmsg_clear(msg);
  if (msg->hm_data) {
    free((void *)msg->hm_data);
#if ENABLE_SLOW_MEMORYINFO
    size += msg->hm_data_size;
#endif
  }
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_free(&htsmsg_memoryinfo, sizeof(htsmsg_t) + size);
#endif
  free(msg);
}

/*
 *
 */
void
htsmsg_add_bool(htsmsg_t *msg, const char *name, int b)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_BOOL, HMF_NAME_INALLOCED, 0);
  f->hmf_bool = !!b;
}

/*
 *
 */
void
htsmsg_set_bool(htsmsg_t *msg, const char *name, int b)
{
  htsmsg_field_t *f = htsmsg_field_find(msg, name);
  if (!f)
    f = htsmsg_field_add(msg, name, HMF_BOOL, HMF_NAME_INALLOCED, 0);
  f->hmf_bool = !!b;
}

/*
 *
 */
void
htsmsg_add_s64(htsmsg_t *msg, const char *name, int64_t s64)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_S64, HMF_NAME_INALLOCED, 0);
  f->hmf_s64 = s64;
}

/*
 *
 */
int
htsmsg_set_s64(htsmsg_t *msg, const char *name, int64_t s64)
{
  htsmsg_field_t *f = htsmsg_field_find(msg, name);
  if (!f)
    f = htsmsg_field_add(msg, name, HMF_S64, HMF_NAME_INALLOCED, 0);
  if (f->hmf_type != HMF_S64)
    return 1;
  f->hmf_s64 = s64;
  return 0;
}


/*
 *
 */
void
htsmsg_add_dbl(htsmsg_t *msg, const char *name, double dbl)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_DBL, HMF_NAME_INALLOCED, 0);
  f->hmf_dbl = dbl;
}



/*
 *
 */
void
htsmsg_add_str(htsmsg_t *msg, const char *name, const char *str)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_STR, HMF_NAME_INALLOCED,
                                       strlen(str) + 1);
  strcpy((char *)f->hmf_str, str);
  f->hmf_flags |= HMF_INALLOCED;
}

/*
 *
 */
void
htsmsg_add_str2(htsmsg_t *msg, const char *name, const char *str)
{
  if (msg && name && str)
    htsmsg_add_str(msg, name, str);
}

/*
 *
 */
void
htsmsg_add_str_exclusive(htsmsg_t *msg, const char *str)
{
  htsmsg_field_t *f;

  assert(msg->hm_islist);

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    assert(f->hmf_type == HMF_STR);
    if (strcmp(f->hmf_str, str) == 0)
      return;
  }

  htsmsg_add_str(msg, NULL, str);
}

/*
 *
 */
int
htsmsg_field_set_str(htsmsg_field_t *f, const char *str)
{
  if (f->hmf_type != HMF_STR)
    return 1;
  if (f->hmf_flags & HMF_ALLOCED) {
#if ENABLE_SLOW_MEMORYINFO
    memoryinfo_remove(&htsmsg_field_memoryinfo, strlen(f->hmf_str) + 1);
#endif
    free((void *)f->hmf_str);
  }
  else if (f->hmf_flags & HMF_INALLOCED) {
    if (strlen(f->hmf_str) >= strlen(str)) {
      strcpy((char *)f->hmf_str, str);
      return 0;
    }
    f->hmf_flags &= ~HMF_INALLOCED;
  }
  f->hmf_flags |= HMF_ALLOCED;
  f->hmf_str = strdup(str);
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_alloc(&htsmsg_field_memoryinfo, strlen(str) + 1);
#endif
  return 0;
}

/*
 *
 */
int
htsmsg_field_set_str_force(htsmsg_field_t *f, const char *str)
{
  if (f->hmf_type != HMF_STR) {
    htsmsg_field_data_destroy(f);
    f->hmf_type = HMF_STR;
    f->hmf_str = "";
  }
  return htsmsg_field_set_str(f, str);
}

/*
 *
 */
int
htsmsg_set_str(htsmsg_t *msg, const char *name, const char *str)
{
  htsmsg_field_t *f = htsmsg_field_find(msg, name);
  if (!f) {
    htsmsg_add_str(msg, name, str);
    return 0;
  }
  return htsmsg_field_set_str(f, str);
}

/*
 *
 */
void
htsmsg_add_bin(htsmsg_t *msg, const char *name, const void *bin, size_t len)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_BIN, HMF_NAME_INALLOCED, len);
  f->hmf_bin = f->hmf_str;
  f->hmf_binsize = len;
  f->hmf_flags |= HMF_INALLOCED;
  memcpy((void *)f->hmf_bin, bin, len);
}

/*
 *
 */
void
htsmsg_add_binptr(htsmsg_t *msg, const char *name, const void *bin, size_t len)
{
  htsmsg_field_t *f = htsmsg_field_add(msg, name, HMF_BIN, HMF_NAME_INALLOCED, 0);
  f->hmf_bin = bin;
  f->hmf_binsize = len;
}


/*
 *
 */
static htsmsg_t *
htsmsg_field_set_msg(htsmsg_field_t *f, htsmsg_t *sub)
{
  assert(sub->hm_data == NULL);
  f->hmf_msg.hm_data = NULL;
  f->hmf_msg.hm_data_size = 0;
  f->hmf_msg.hm_islist = sub->hm_islist;
  TAILQ_MOVE(&f->hmf_msg.hm_fields, &sub->hm_fields, hmf_link);
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_free(&htsmsg_memoryinfo, sizeof(htsmsg_t));
#endif
  free(sub);

  if (f->hmf_type == (f->hmf_msg.hm_islist ? HMF_LIST : HMF_MAP))
    return &f->hmf_msg;

  return NULL;
}

/*
 *
 */
htsmsg_t *
htsmsg_add_msg(htsmsg_t *msg, const char *name, htsmsg_t *sub)
{
  htsmsg_field_t *f;

  f = htsmsg_field_add(msg, name, sub->hm_islist ? HMF_LIST : HMF_MAP,
		       HMF_NAME_INALLOCED, 0);
  return htsmsg_field_set_msg(f, sub);
}

/*
 *
 */
htsmsg_t *
htsmsg_set_msg(htsmsg_t *msg, const char *name, htsmsg_t *sub)
{
  htsmsg_field_t *f = htsmsg_field_find(msg, name);
  if (!f)
    return htsmsg_add_msg(msg, name, sub);
  htsmsg_field_data_destroy(f);
  return htsmsg_field_set_msg(f, sub);
}



/*
 *
 */
void
htsmsg_add_msg_extname(htsmsg_t *msg, const char *name, htsmsg_t *sub)
{
  htsmsg_field_t *f;

  f = htsmsg_field_add(msg, name, sub->hm_islist ? HMF_LIST : HMF_MAP, 0, 0);

  assert(sub->hm_data == NULL);
  f->hmf_msg.hm_data = NULL;
  f->hmf_msg.hm_data_size = 0;
  TAILQ_MOVE(&f->hmf_msg.hm_fields, &sub->hm_fields, hmf_link);
  f->hmf_msg.hm_islist = sub->hm_islist;
#if ENABLE_SLOW_MEMORYINFO
  memoryinfo_free(&htsmsg_memoryinfo, sizeof(htsmsg_t));
#endif
  free(sub);
}



/**
 *
 */
int
htsmsg_get_s64(htsmsg_t *msg, const char *name, int64_t *s64p)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  
  return htsmsg_field_get_s64(f, s64p);
}

int
htsmsg_field_get_s64
  (htsmsg_field_t *f, int64_t *s64p)
{
  switch(f->hmf_type) {
  default:
    return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  case HMF_STR:
    *s64p = strtoll(f->hmf_str, NULL, 0);
    break;
  case HMF_S64:
    *s64p = f->hmf_s64;
    break;
  case HMF_BOOL:
    *s64p = f->hmf_bool;
    break;
  case HMF_DBL:
    *s64p = f->hmf_dbl;
    break;
  }
  return 0;
}


/**
 *
 */

int
bool_check(const char *str)
{
  if (str &&
      (!strcmp(str, "yes")  ||
       !strcmp(str, "true") ||
       !strcmp(str, "on") ||
       !strcmp(str, "1")))
    return 1;
  return 0;
}

int
htsmsg_field_get_bool
  ( htsmsg_field_t *f, int *boolp )
{
  switch(f->hmf_type) {
  default:
    return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  case HMF_STR:
    *boolp = bool_check(f->hmf_str);
    break;
  case HMF_S64:
    *boolp = f->hmf_s64 ? 1 : 0;
    break;
  case HMF_BOOL:
    *boolp = f->hmf_bool;
    break;
  }
  return 0;
}

int
htsmsg_get_bool
  (htsmsg_t *msg, const char *name, int *boolp)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  
  return htsmsg_field_get_bool(f, boolp);
}

int
htsmsg_get_bool_or_default(htsmsg_t *msg, const char *name, int def)
{
  int ret;
  return htsmsg_get_bool(msg, name, &ret) ? def : ret;
}


/**
 *
 */
int64_t
htsmsg_get_s64_or_default(htsmsg_t *msg, const char *name, int64_t def)
{
  int64_t s64;
  return htsmsg_get_s64(msg, name, &s64) ? def : s64;
}


/*
 *
 */
int
htsmsg_get_u32(htsmsg_t *msg, const char *name, uint32_t *u32p)
{
  int r;
  int64_t s64;

  if((r = htsmsg_get_s64(msg, name, &s64)) != 0)
    return r;

  if(s64 < 0 || s64 > 0xffffffffLL)
    return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  
  *u32p = s64;
  return 0;
}

int
htsmsg_field_get_u32(htsmsg_field_t *f, uint32_t *u32p)
{
  int r;
  int64_t s64;
    
  if ((r = htsmsg_field_get_s64(f, &s64)))
    return r;

  if (s64 < 0 || s64 > 0xffffffffL)
      return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  
  *u32p = s64;
  return 0;
}

/**
 *
 */
int
htsmsg_get_u32_or_default(htsmsg_t *msg, const char *name, uint32_t def)
{
  uint32_t u32;
  return htsmsg_get_u32(msg, name, &u32) ? def : u32;
}


/**
 *
 */
int32_t
htsmsg_get_s32_or_default(htsmsg_t *msg, const char *name, int32_t def)
{
  int32_t s32;
  return htsmsg_get_s32(msg, name, &s32) ? def : s32;
}



/*
 *
 */
int
htsmsg_get_s32(htsmsg_t *msg, const char *name, int32_t *s32p)
{
  int r;
  int64_t s64;

  if((r = htsmsg_get_s64(msg, name, &s64)) != 0)
    return r;

  if(s64 < -0x80000000LL || s64 > 0x7fffffffLL)
    return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  
  *s32p = s64;
  return 0;
}


/*
 *
 */
int
htsmsg_get_dbl(htsmsg_t *msg, const char *name, double *dblp)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  
  return htsmsg_field_get_dbl(f, dblp);
}

int
htsmsg_field_get_dbl
  ( htsmsg_field_t *f, double *dblp )
{
  switch (f->hmf_type) {
    case HMF_S64:
      *dblp = (double)f->hmf_s64;
      break;
    case HMF_DBL:
      *dblp = f->hmf_dbl;
      break;
    case HMF_STR:
      *dblp = my_str2double(f->hmf_str, NULL);
      // TODO: better safety checks?
      break;
    default:
      return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;
  }
  return 0;
}

/*
 *
 */
int
htsmsg_get_bin(htsmsg_t *msg, const char *name, const void **binp,
	       size_t *lenp)
{
  htsmsg_field_t *f;
  
  if((f = htsmsg_field_find(msg, name)) == NULL)
    return HTSMSG_ERR_FIELD_NOT_FOUND;
  
  if(f->hmf_type != HMF_BIN)
    return HTSMSG_ERR_CONVERSION_IMPOSSIBLE;

  *binp = f->hmf_bin;
  *lenp = f->hmf_binsize;
  return 0;
}

/**
 *
 */
const char *
htsmsg_field_get_string(htsmsg_field_t *f)
{
  char buf[128];
  
  switch(f->hmf_type) {
  default:
    return NULL;
  case HMF_STR:
    break;
  case HMF_BOOL:
    htsmsg_field_set_str_force(f, f->hmf_bool ? "true" : "false");
    break;
  case HMF_S64:
    snprintf(buf, sizeof(buf), "%"PRId64, f->hmf_s64);
    htsmsg_field_set_str_force(f, buf);
    break;
  case HMF_DBL:
    snprintf(buf, sizeof(buf), "%lf", f->hmf_dbl);
    htsmsg_field_set_str_force(f, buf);
    break;
  }
  return f->hmf_str;
}

/*
 *
 */
const char *
htsmsg_get_str(htsmsg_t *msg, const char *name)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return NULL;
  return htsmsg_field_get_string(f);

}

/*
 *
 */
htsmsg_t *
htsmsg_get_map(htsmsg_t *msg, const char *name)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return NULL;

  return htsmsg_field_get_map(f);
}

htsmsg_t *
htsmsg_field_get_map(htsmsg_field_t *f)
{
  return htsmsg_field_get_msg(f, 0);
}

/**
 *
 */
htsmsg_t *
htsmsg_get_map_multi(htsmsg_t *msg, ...)
{
  va_list ap;
  const char *n;

  va_start(ap, msg);
  while(msg != NULL && (n = va_arg(ap, char *)) != NULL)
    msg = htsmsg_get_map(msg, n);
  va_end(ap);

  return msg;
}

/**
 *
 */
const char *
htsmsg_get_str_multi(htsmsg_t *msg, ...)
{
  va_list ap;
  const char *n, *r = NULL;
  htsmsg_field_t *f;

  va_start(ap, msg);
  while((n = va_arg(ap, char *)) != NULL) {
    if((f = htsmsg_field_find(msg, n)) == NULL)
      break;
    else if(f->hmf_type == HMF_STR) {
      r = f->hmf_str;
      break;
    } else if(f->hmf_type == HMF_MAP)
      msg = &f->hmf_msg;
    else
      break;
  }
  va_end(ap);

  return r;
}



/*
 *
 */
htsmsg_t *
htsmsg_get_list(htsmsg_t *msg, const char *name)
{
  htsmsg_field_t *f;

  if((f = htsmsg_field_find(msg, name)) == NULL)
    return NULL;

  return htsmsg_field_get_list(f);
}

htsmsg_t *
htsmsg_field_get_list ( htsmsg_field_t *f )
{
  return htsmsg_field_get_msg(f, 1);
}

static htsmsg_t *
htsmsg_field_get_msg ( htsmsg_field_t *f, int islist )
{
  htsmsg_t *m;

  /* Deserialize JSON (will keep either list or map) */
  if (f->hmf_type == HMF_STR) {
    if ((m = htsmsg_json_deserialize(f->hmf_str))) {
      if (f->hmf_flags & HMF_ALLOCED) {
#if ENABLE_SLOW_MEMORYINFO
        memoryinfo_free(&htsmsg_field_memoryinfo, strlen(f->hmf_str) + 1);
#endif
        free((void*)f->hmf_str);
      }
      f->hmf_type          = m->hm_islist ? HMF_LIST : HMF_MAP;
      f->hmf_msg.hm_islist = m->hm_islist;
      f->hmf_msg.hm_data   = NULL;
      f->hmf_msg.hm_data_size = 0;
      TAILQ_MOVE(&f->hmf_msg.hm_fields, &m->hm_fields, hmf_link);
#if ENABLE_SLOW_MEMORYINFO
      memoryinfo_free(&htsmsg_memoryinfo, sizeof(htsmsg_t));
#endif
      free(m);
    }
  }

  if (f->hmf_type == (islist ? HMF_LIST : HMF_MAP))
    return &f->hmf_msg;

  return NULL;
}

/**
 *
 */
htsmsg_t *
htsmsg_detach_submsg(htsmsg_field_t *f)
{
  htsmsg_t *r = htsmsg_create_map();

  TAILQ_MOVE(&r->hm_fields, &f->hmf_msg.hm_fields, hmf_link);
  TAILQ_INIT(&f->hmf_msg.hm_fields);
  r->hm_islist = f->hmf_type == HMF_LIST;
  return r;
}


/*
 *
 */
static void
htsmsg_print0(htsmsg_t *msg, int indent)
{
  htsmsg_field_t *f;
  int i;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {

    for(i = 0; i < indent; i++) printf("\t");

    printf("%s (", f->hmf_name ?: "");

    switch(f->hmf_type) {

    case HMF_MAP:
      printf("MAP) = {\n");
      htsmsg_print0(&f->hmf_msg, indent + 1);
      for(i = 0; i < indent; i++) printf("\t");
      printf("}\n");
      break;

    case HMF_LIST:
      printf("LIST) = {\n");
      htsmsg_print0(&f->hmf_msg, indent + 1);
      for(i = 0; i < indent; i++) printf("\t");
      printf("}\n");
      break;

    case HMF_STR:
      printf("STR) = \"%s\"\n", f->hmf_str);
      break;

    case HMF_BIN:
      printf("BIN) = [");
      for(i = 0; i < f->hmf_binsize - 1; i++)
	printf("%02x.", ((uint8_t *)f->hmf_bin)[i]);
      printf("%02x]\n", ((uint8_t *)f->hmf_bin)[i]);
      break;

    case HMF_S64:
      printf("S64) = %" PRId64 "\n", f->hmf_s64);
      break;

    case HMF_BOOL:
      printf("BOOL) = %s\n", f->hmf_bool ? "true" : "false");
      break;

    case HMF_DBL:
      printf("DBL) = %f\n", f->hmf_dbl);
      break;
    }
  }
} 

/*
 *
 */
void
htsmsg_print(htsmsg_t *msg)
{
  htsmsg_print0(msg, 0);
} 


/**
 *
 */
static void
htsmsg_copy_i(htsmsg_t *src, htsmsg_t *dst)
{
  htsmsg_field_t *f;
  htsmsg_t *sub;

  TAILQ_FOREACH(f, &src->hm_fields, hmf_link) {

    switch(f->hmf_type) {

    case HMF_MAP:
    case HMF_LIST:
      sub = f->hmf_type == HMF_LIST ? 
	htsmsg_create_list() : htsmsg_create_map();
      htsmsg_copy_i(&f->hmf_msg, sub);
      htsmsg_add_msg(dst, f->hmf_name, sub);
      break;
      
    case HMF_STR:
      htsmsg_add_str(dst, f->hmf_name, f->hmf_str);
      break;

    case HMF_S64:
      htsmsg_add_s64(dst, f->hmf_name, f->hmf_s64);
      break;

    case HMF_BOOL:
      htsmsg_add_bool(dst, f->hmf_name, f->hmf_bool);
      break;

    case HMF_BIN:
      htsmsg_add_bin(dst, f->hmf_name, f->hmf_bin, f->hmf_binsize);
      break;

    case HMF_DBL:
      htsmsg_add_dbl(dst, f->hmf_name, f->hmf_dbl);
      break;
    }
  }
}

htsmsg_t *
htsmsg_copy(htsmsg_t *src)
{
  htsmsg_t *dst;
  if (src == NULL) return NULL;
  dst = src->hm_islist ? htsmsg_create_list() : htsmsg_create_map();
  htsmsg_copy_i(src, dst);
  return dst;
}

/**
 *
 */
int
htsmsg_cmp(htsmsg_t *m1, htsmsg_t *m2)
{
  htsmsg_field_t *f1, *f2;

  if (m1 == NULL && m2 == NULL)
    return 0;
  if (m1 == NULL || m2 == NULL)
    return 1;

  f2 = TAILQ_FIRST(&m2->hm_fields);
  TAILQ_FOREACH(f1, &m1->hm_fields, hmf_link) {

    if (f2 == NULL)
      return 1;

    if (f1->hmf_type != f2->hmf_type)
      return 1;
    if (strcmp(f1->hmf_name ?: "", f2->hmf_name ?: ""))
      return 1;

    switch(f1->hmf_type) {

    case HMF_MAP:
    case HMF_LIST:
      if (htsmsg_cmp(&f1->hmf_msg, &f2->hmf_msg))
        return 1;
      break;
      
    case HMF_STR:
      if (strcmp(f1->hmf_str, f2->hmf_str))
        return 1;
      break;

    case HMF_S64:
      if (f1->hmf_s64 != f2->hmf_s64)
        return 1;
      break;

    case HMF_BOOL:
      if (f1->hmf_bool != f2->hmf_bool)
        return 1;
      break;

    case HMF_BIN:
      if (f1->hmf_binsize != f2->hmf_binsize)
        return 1;
      if (memcmp(f1->hmf_bin, f2->hmf_bin, f1->hmf_binsize))
        return 1;
      break;

    case HMF_DBL:
      if (f1->hmf_dbl != f2->hmf_dbl)
        return 1;
      break;
    }

    f2 = TAILQ_NEXT(f2, hmf_link);
  }

  if (f2)
    return 1;
  return 0;
}

/**
 *
 */
htsmsg_t *
htsmsg_get_map_in_list(htsmsg_t *m, int num)
{
  htsmsg_field_t *f;

  HTSMSG_FOREACH(f, m) {
    if(!--num)
      return htsmsg_get_map_by_field(f);
  }
  return NULL;
}


/**
 *
 */
htsmsg_t *
htsmsg_get_map_by_field_if_name(htsmsg_field_t *f, const char *name)
{
  if(f->hmf_type != HMF_MAP)
    return NULL;
  if(strcmp(f->hmf_name, name))
    return NULL;
  return &f->hmf_msg;
}


/**
 *
 */
const char *
htsmsg_get_cdata(htsmsg_t *m, const char *field)
{
  return htsmsg_get_str_multi(m, field, "cdata", NULL);
}

/**
 * Convert list to CSV string
 *
 * Note: this will NOT work for lists of complex types
 */
char *
htsmsg_list_2_csv(htsmsg_t *m, char delim, int human)
{
  int alloc, used, first = 1;
  char *ret;
  htsmsg_field_t *f;
  char sep[3];
  const char *ssep;
  if (!m->hm_islist) return NULL;

#define REALLOC(l)\
  if ((alloc - used) < l) {\
    alloc = MAX((l)*2, alloc*2);\
    ret   = realloc(ret, alloc);\
  }\

  ret  = malloc(alloc = 512);
  *ret = 0;
  used = 0;
  if (human) {
    sep[0] = delim;
    if (human & 2) {
      sep[1] = '\0';
    } else {
      sep[1] = ' ';
      sep[2] = '\0';
    }
    ssep = "";
  } else {
    sep[0] = delim;
    sep[1] = '\0';
    ssep = "\"";
  }
  HTSMSG_FOREACH(f, m) {
    if (f->hmf_type == HMF_STR) {
      REALLOC(4 + strlen(f->hmf_str));
      used += sprintf(ret+used, "%s%s%s%s", !first ? sep : "", ssep, f->hmf_str, ssep);
    } else if (f->hmf_type == HMF_S64) {
      REALLOC(34); // max length is actually 20 chars + 2
      used += sprintf(ret+used, "%s%"PRId64, !first ? sep : "", f->hmf_s64);
    } else if (f->hmf_type == HMF_BOOL) {
      REALLOC(12); // max length is actually 10 chars + 2
      used += sprintf(ret+used, "%s%d", !first ? sep : "", f->hmf_bool);
    } else {
      // TODO: handle doubles
      free(ret);
      return NULL;
    }
    first = 0;
  }

  return ret;
}

htsmsg_t *
htsmsg_csv_2_list(const char *str, char delim)
{
  char *tokbuf, *tok, *saveptr = NULL, *p;
  const char d[2] = { delim, '\0' };
  htsmsg_t *m = htsmsg_create_list();

  if (str) {
    tokbuf = strdup(str);
    tok = strtok_r(tokbuf, d, &saveptr);
    while (tok) {
      if (tok[0] == '"') {
        p = ++tok;
        while (*p && *p != '"') {
          if (*p == '\\') {
            p++;
            if (*p)
              p++;
            continue;
          }
          p++;
        }
        *p = '\0';
      }
      htsmsg_add_str(m, NULL, tok);
      tok = strtok_r(NULL, ",", &saveptr);
    }
    free(tokbuf);
  }
  return m;
}

/*
 *
 */
htsmsg_t *
htsmsg_create_key_val(const char *key, const char *val)
{
  htsmsg_t *r = htsmsg_create_map();
  if (r) {
    htsmsg_add_str(r, "key", key);
    htsmsg_add_str(r, "val", val);
  }
  return r;
}
