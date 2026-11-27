/*
 *  Functions converting HTSMSGs to/from JSON
 *  Copyright (C) 2008 Andreas Öman
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
#include <stdint.h>

#include <cJSON.h>

#include "htsmsg_json.h"
#include "htsbuf.h"
#include "misc/json.h"


/**
 * Convert htsmsg to cJSON object
 */
static cJSON *
htsmsg_to_cjson(htsmsg_t *msg)
{
  htsmsg_field_t *f;
  cJSON *obj;
  cJSON *item;
  char buf[100];

  if (msg->hm_islist)
    obj = cJSON_CreateArray();
  else
    obj = cJSON_CreateObject();

  if (obj == NULL)
    return NULL;

  HTSMSG_FOREACH(f, msg) {
    const char *name = msg->hm_islist ? NULL : htsmsg_field_name(f);

    switch (f->hmf_type) {
    case HMF_MAP:
    case HMF_LIST:
      item = htsmsg_to_cjson(f->hmf_msg);
      break;

    case HMF_STR:
      item = cJSON_CreateString(f->hmf_str);
      break;

    case HMF_UUID:
      uuid_get_hex((tvh_uuid_t *)f->hmf_uuid, buf);
      item = cJSON_CreateString(buf);
      break;

    case HMF_BIN:
      item = cJSON_CreateString("binary");
      break;

    case HMF_BOOL:
      item = cJSON_CreateBool(f->hmf_bool);
      break;

    case HMF_S64:
      item = cJSON_CreateNumber((double)f->hmf_s64);
      break;

    case HMF_DBL:
      item = cJSON_CreateNumber(f->hmf_dbl);
      break;

    default:
      item = NULL;
      break;
    }

    if (item == NULL) {
      cJSON_Delete(obj);
      return NULL;
    }

    if (msg->hm_islist)
      cJSON_AddItemToArray(obj, item);
    else
      cJSON_AddItemToObject(obj, name, item);
  }

  return obj;
}


/**
 *
 */
void
htsmsg_json_serialize(htsmsg_t *msg, htsbuf_queue_t *hq, int pretty)
{
  cJSON *obj;
  char *str;

  obj = htsmsg_to_cjson(msg);
  if (obj == NULL)
    return;

  if (pretty)
    str = cJSON_Print(obj);
  else
    str = cJSON_PrintUnformatted(obj);

  cJSON_Delete(obj);

  if (str != NULL) {
    htsbuf_append_str(hq, str);
    if (pretty)
      htsbuf_append(hq, "\n", 1);
    cJSON_free(str);
  }
}


/**
 *
 */
char *
htsmsg_json_serialize_to_str(htsmsg_t *msg, int pretty)
{
  cJSON *obj;
  char *str;

  obj = htsmsg_to_cjson(msg);
  if (obj == NULL)
    return NULL;

  if (pretty)
    str = cJSON_Print(obj);
  else
    str = cJSON_PrintUnformatted(obj);

  cJSON_Delete(obj);
  return str;
}


/**
 *
 */
static void *
create_map(void *opaque)
{
  return htsmsg_create_map();
}

static void *
create_list(void *opaque)
{
  return htsmsg_create_list();
}

static void
destroy_obj(void *opaque, void *obj)
{
  return htsmsg_destroy(obj);
}

static void
add_obj(void *opaque, void *parent, const char *name, void *child)
{
  htsmsg_add_msg(parent, name, child);
}

static void 
add_string(void *opaque, void *parent, const char *name,  char *str)
{
  htsmsg_add_str(parent, name, str);
  free(str);
}

static void 
add_s64(void *opaque, void *parent, const char *name, int64_t v)
{
  htsmsg_add_s64(parent, name, v);
}

static void 
add_double(void *opaque, void *parent, const char *name, double v)
{
  htsmsg_add_dbl(parent, name, v);
}

static void 
add_bool(void *opaque, void *parent, const char *name, int v)
{
  htsmsg_add_bool(parent, name, v);
}

static void 
add_null(void *opaque, void *parent, const char *name)
{
}

/**
 *
 */
static const json_deserializer_t json_to_htsmsg = {
  .jd_create_map      = create_map,
  .jd_create_list     = create_list,
  .jd_destroy_obj     = destroy_obj,
  .jd_add_obj         = add_obj,
  .jd_add_string      = add_string,
  .jd_add_s64         = add_s64,
  .jd_add_double      = add_double,
  .jd_add_bool        = add_bool,
  .jd_add_null        = add_null,
};


/**
 *
 */
htsmsg_t *
htsmsg_json_deserialize(const char *src)
{
  return json_deserialize(src, &json_to_htsmsg, NULL, NULL, 0);
}
