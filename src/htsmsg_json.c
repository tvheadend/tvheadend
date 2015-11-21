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

#include "htsmsg_json.h"
#include "htsbuf.h"
#include "misc/json.h"
#include "misc/dbl.h"


/**
 *
 */
static void
htsmsg_json_write(htsmsg_t *msg, htsbuf_queue_t *hq, int isarray,
		  int indent, int pretty)
{
  htsmsg_field_t *f;
  char buf[100];
  static const char *indentor = "\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
  const char *s;

  htsbuf_append(hq, isarray ? "[" : "{", 1);

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {

    if(pretty) 
      htsbuf_append(hq, indentor, indent < 16 ? indent : 16);

    if(!isarray) {
      htsbuf_append_and_escape_jsonstr(hq, f->hmf_name ?: "noname");
      htsbuf_append(hq, ": ", pretty ? 2 : 1);
    }

    switch(f->hmf_type) {
    case HMF_MAP:
      htsmsg_json_write(&f->hmf_msg, hq, 0, indent + 1, pretty);
      break;

    case HMF_LIST:
      htsmsg_json_write(&f->hmf_msg, hq, 1, indent + 1, pretty);
      break;

    case HMF_STR:
      htsbuf_append_and_escape_jsonstr(hq, f->hmf_str);
      break;

    case HMF_BIN:
      htsbuf_append_and_escape_jsonstr(hq, "binary");
      break;

    case HMF_BOOL:
      s = f->hmf_bool ? "true" : "false";
      htsbuf_append_str(hq, s);
      break;

    case HMF_S64:
      snprintf(buf, sizeof(buf), "%" PRId64, f->hmf_s64);
      htsbuf_append_str(hq, buf);
      break;

    case HMF_DBL:
      my_double2str(buf, sizeof(buf), f->hmf_dbl);
      htsbuf_append_str(hq, buf);
      break;

    default:
      abort();
    }

    if(TAILQ_NEXT(f, hmf_link))
      htsbuf_append(hq, ",", 1);
  }
  
  if(pretty) 
    htsbuf_append(hq, indentor, indent-1 < 16 ? indent-1 : 16);
  htsbuf_append(hq, isarray ? "]" : "}", 1);
}

/**
 *
 */
void
htsmsg_json_serialize(htsmsg_t *msg, htsbuf_queue_t *hq, int pretty)
{
  htsmsg_json_write(msg, hq, msg->hm_islist, 2, pretty);
  if(pretty) 
    htsbuf_append(hq, "\n", 1);
}


/**
 *
 */
char *
htsmsg_json_serialize_to_str(htsmsg_t *msg, int pretty)
{
  htsbuf_queue_t hq;
  char *str;
  htsbuf_queue_init(&hq, 0);
  htsmsg_json_serialize(msg, &hq, pretty);
  str = htsbuf_to_string(&hq);
  htsbuf_queue_flush(&hq);
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
add_long(void *opaque, void *parent, const char *name, long v)
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
  .jd_add_long        = add_long,
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
