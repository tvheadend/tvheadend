/*
 *  Functions converting HTSMSGs to/from a simple binary format
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
#include <string.h>

#include "htsmsg_binary2.h"
#include "memoryinfo.h"

/*
 *
 */
static inline uint32_t htsmsg_binary2_get_length(uint8_t const **p, const uint8_t *end)
{
  uint32_t r = 0;
  uint8_t c;
  while (*p != end) {
    c = **p; (*p)++;
    r |= c & 0x7f;
    if (c & 0x80) r <<= 7;
  }
  return r;
}

static inline void htsmsg_binary2_set_length(uint8_t **p, uint32_t len)
{
  if (len == 0) {
    **p = 0; (*p)++;
    return;
  } else if (len < 0x80) {
    /* nothing */
  } else if (len < 0x4000) {
    **p = 0x80 | (len >> 7); (*p)++;
  } else if (len < 0x200000) {
    **p = 0x80 | (len >> 14); (*p)++;
    **p = 0x80 | (len >> 7); (*p)++;
  } else if (len < 0x10000000) {
    **p = 0x80 | (len >> 21); (*p)++;
    **p = 0x80 | (len >> 14); (*p)++;
    **p = 0x80 | (len >> 7); (*p)++;
  } else {
    **p = 0x80 | (len >> 28); (*p)++;
    **p = 0x80 | (len >> 21); (*p)++;
    **p = 0x80 | (len >> 14); (*p)++;
    **p = 0x80 | (len >> 7); (*p)++;
  }
  **p = len; (*p)++;
}

static inline uint32_t htsmsg_binary2_length_count(uint32_t len)
{
  uint32_t r = 0;
  while (len > 0) {
    r++; len >>= 7;
  }
  return r;
}

/*
 *
 */
static int
htsmsg_binary2_des0(htsmsg_t *msg, const uint8_t *buf, uint32_t len)
{
  uint_fast32_t type, namelen, datalen;
  const uint8_t *p;
  uint32_t tlen, nlen;
  htsmsg_field_t *f;
  htsmsg_t *sub;
  uint64_t u64;
  int i, bin = 0;

  while(len > 2) {

    type     = buf[0];
    namelen  = buf[1];
    p        = buf + 2;
    datalen  = htsmsg_binary2_get_length(&p, buf + len);
    len     -= p - buf;
    buf      = p;

    if(len < namelen + datalen)
      return -1;

    nlen = namelen ? namelen + 1 : 0;
    tlen = sizeof(htsmsg_field_t) + nlen +
           (type == HMF_STR ? datalen + 1 : 0);
    f = malloc(tlen);
    if (f == NULL)
      return -1;
#if ENABLE_SLOW_MEMORYINFO
    f->hmf_edata_size = tlen - sizeof(htsmsg_field_t);
    memoryinfo_alloc(&htsmsg_field_memoryinfo, tlen);
#endif
    f->hmf_type  = type;
    f->hmf_flags = 0;

    if(namelen > 0) {
      f->hmf_name = f->hmf_edata;
      memcpy(f->hmf_edata, buf, namelen);
      f->hmf_edata[namelen] = 0;

      buf += namelen;
      len -= namelen;
    } else {
      f->hmf_name = NULL;
    }

    switch(type) {
    case HMF_STR:
      f->hmf_str = f->hmf_edata + nlen;
      memcpy((char *)f->hmf_str, buf, datalen);
      ((char *)f->hmf_str)[datalen] = 0;
      f->hmf_flags |= HMF_INALLOCED;
      break;

    case HMF_BIN:
      f->hmf_bin = (const void *)buf;
      f->hmf_binsize = datalen;
      bin = 1;
      break;

    case HMF_S64:
      u64 = 0;
      for(i = datalen - 1; i >= 0; i--)
	  u64 = (u64 << 8) | buf[i];
      f->hmf_s64 = u64;
      break;

    case HMF_MAP:
    case HMF_LIST:
      sub = &f->hmf_msg;
      TAILQ_INIT(&sub->hm_fields);
      sub->hm_data = NULL;
      sub->hm_data_size = 0;
      sub->hm_islist = type == HMF_LIST;
      i = htsmsg_binary2_des0(sub, buf, datalen);
      if (i < 0) {
#if ENABLE_SLOW_MEMORYINFO
        memoryinfo_free(&htsmsg_field_memoryinfo, tlen);
#endif
        free(f);
        return -1;
      }
      if (i > 0)
        bin = 1;
      break;

    case HMF_BOOL:
      f->hmf_bool = datalen == 1 ? buf[0] : 0;
      break;

    default:
#if ENABLE_SLOW_MEMORYINFO
      memoryinfo_free(&htsmsg_field_memoryinfo, tlen);
#endif
      free(f);
      return -1;
    }

    TAILQ_INSERT_TAIL(&msg->hm_fields, f, hmf_link);
    buf += datalen;
    len -= datalen;
  }
  return len ? -1 : bin;
}



/*
 *
 */
htsmsg_t *
htsmsg_binary2_deserialize(void *data, size_t len, const void *buf)
{
  htsmsg_t *msg;
  int r;

  if (len != (len & 0xffffffff)) {
    free((void *)buf);
    return NULL;
  }
  msg = htsmsg_create_map();
  r = htsmsg_binary2_des0(msg, data, len);
  if (r < 0) {
    free((void *)buf);
    htsmsg_destroy(msg);
    return NULL;
  }
  if (r > 0) {
    msg->hm_data = buf;
    msg->hm_data_size = len;
#if ENABLE_SLOW_MEMORYINFO
    memoryinfo_append(&htsmsg_memoryinfo, len);
#endif
  } else {
    free((void *)buf);
  }
  return msg;
}



/*
 *
 */
static uint32_t
htsmsg_binary2_count(htsmsg_t *msg)
{
  htsmsg_field_t *f;
  uint32_t len = 0, l;
  uint64_t u64;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {

    l = 0;
    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      l = htsmsg_binary2_count(&f->hmf_msg);
      break;

    case HMF_STR:
      l = strlen(f->hmf_str);
      break;

    case HMF_BIN:
      l = f->hmf_binsize;
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      while(u64 != 0) {
	l++;
	u64 = u64 >> 8;
      }
      break;

    case HMF_BOOL:
      if (f->hmf_bool) l = 1;
      break;
    }

    len += htsmsg_binary2_length_count(l) + 2 + l;
    len += f->hmf_name ? strlen(f->hmf_name) : 0;
  }
  return len;
}


/*
 *
 */
static void
htsmsg_binary2_write(htsmsg_t *msg, uint8_t *ptr)
{
  htsmsg_field_t *f;
  uint64_t u64;
  int l, i, namelen;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    namelen = f->hmf_name ? strlen(f->hmf_name) : 0;
    *ptr++ = f->hmf_type;
    *ptr++ = namelen;

    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      l = htsmsg_binary2_count(&f->hmf_msg);
      break;

    case HMF_STR:
      l = strlen(f->hmf_str);
      break;

    case HMF_BIN:
      l = f->hmf_binsize;
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      l = 0;
      while(u64 != 0) {
	l++;
	u64 = u64 >> 8;
      }
      break;

    case HMF_BOOL:
      l = f->hmf_bool ? 1 : 0;
      break;

    default:
      abort();
    }

    htsmsg_binary2_set_length(&ptr, l);

    if(namelen > 0) {
      memcpy(ptr, f->hmf_name, namelen);
      ptr += namelen;
    }

    switch(f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      htsmsg_binary2_write(&f->hmf_msg, ptr);
      break;

    case HMF_STR:
      memcpy(ptr, f->hmf_str, l);
      break;

    case HMF_BIN:
      memcpy(ptr, f->hmf_bin, l);
      break;

    case HMF_S64:
      u64 = f->hmf_s64;
      for(i = 0; i < l; i++) {
	ptr[i] = u64;
	u64 = u64 >> 8;
      }
      break;

    case HMF_BOOL:
      if (f->hmf_bool)
        ptr[0] = 1;
      break;

    default:
      abort();
    }
    ptr += l;
  }
}


/*
 *
 */
int
htsmsg_binary2_serialize(htsmsg_t *msg, void **datap, size_t *lenp, size_t maxlen)
{
  uint32_t len, llen;
  uint8_t *data, *p;

  len = htsmsg_binary2_count(msg);
  llen = htsmsg_binary2_length_count(len);
  if(len + llen > maxlen)
    return -1;

  p = data = malloc(len + llen);
 
  htsmsg_binary2_set_length(&p, len);
  htsmsg_binary2_write(msg, p);

  *datap = data;
  *lenp  = p - data;
  return 0;
}
