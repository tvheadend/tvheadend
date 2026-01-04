/*
 *  JSON helpers
 *  Copyright (C) 2011 Andreas Öman
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <cJSON.h>

#include "json.h"
#include "tvheadend.h"

/**
 * Convert a cJSON item to the deserializer callback format
 */
static void *
json_convert_item(const cJSON *item, const char *name, void *parent,
                  const json_deserializer_t *jd, void *opaque)
{
  void *obj;
  cJSON *child;

  if (cJSON_IsObject(item)) {
    obj = jd->jd_create_map(opaque);
    cJSON_ArrayForEach(child, item) {
      json_convert_item(child, child->string, obj, jd, opaque);
    }
    if (parent)
      jd->jd_add_obj(opaque, parent, name, obj);
    return obj;
  }

  if (cJSON_IsArray(item)) {
    obj = jd->jd_create_list(opaque);
    cJSON_ArrayForEach(child, item) {
      json_convert_item(child, NULL, obj, jd, opaque);
    }
    if (parent)
      jd->jd_add_obj(opaque, parent, name, obj);
    return obj;
  }

  if (cJSON_IsString(item)) {
    char *str = strdup(item->valuestring);
    jd->jd_add_string(opaque, parent, name, str);
    return NULL;
  }

  if (cJSON_IsNumber(item)) {
    double d = item->valuedouble;
    /* Check if the number is an integer (no fractional part and within int64 range) */
    if (d == floor(d) && d >= (double)INT64_MIN && d <= (double)INT64_MAX) {
      jd->jd_add_s64(opaque, parent, name, (int64_t)d);
    } else {
      jd->jd_add_double(opaque, parent, name, d);
    }
    return NULL;
  }

  if (cJSON_IsBool(item)) {
    jd->jd_add_bool(opaque, parent, name, cJSON_IsTrue(item) ? 1 : 0);
    return NULL;
  }

  if (cJSON_IsNull(item)) {
    jd->jd_add_null(opaque, parent, name);
    return NULL;
  }

  return NULL;
}


/**
 *
 */
void *
json_deserialize(const char *src, const json_deserializer_t *jd, void *opaque,
                 char *errbuf, size_t errlen)
{
  cJSON *root;
  void *result;
  const char *error_ptr;

  if (src == NULL) {
    if (errbuf && errlen > 0)
      snprintf(errbuf, errlen, "NULL input");
    return NULL;
  }

  root = cJSON_Parse(src);
  if (root == NULL) {
    error_ptr = cJSON_GetErrorPtr();
    if (errbuf && errlen > 0) {
      if (error_ptr != NULL) {
        snprintf(errbuf, errlen, "JSON parse error near: '%.20s'", error_ptr);
      } else {
        snprintf(errbuf, errlen, "JSON parse error");
      }
    }
    return NULL;
  }

  if (!cJSON_IsObject(root) && !cJSON_IsArray(root)) {
    if (errbuf && errlen > 0)
      snprintf(errbuf, errlen, "Invalid JSON, expected '{' or '['");
    cJSON_Delete(root);
    return NULL;
  }

  result = json_convert_item(root, NULL, NULL, jd, opaque);
  cJSON_Delete(root);
  return result;
}
