/**
 *  Dtable (dyanmic, data, etc) table 
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


#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <libhts/htssettings.h>

#include "tvhead.h"
#include "dtable.h"
#include "notify.h"

static LIST_HEAD(, dtable) dtables;

/**
 *
 */
static void
dtable_store_changed(const dtable_t *dt)
{
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg(dt->dt_tablename, m);
}

/**
 *
 */
dtable_t *
dtable_create(const dtable_class_t *dtc, const char *name, void *opaque)
{
  dtable_t *dt = calloc(1, sizeof(dtable_t));

  dt->dt_opaque = opaque;
  dt->dt_tablename = strdup(name);
  dt->dt_dtc = dtc;

  LIST_INSERT_HEAD(&dtables, dt, dt_link);
  return dt;
}

/**
 *
 */
int
dtable_load(dtable_t *dt)
{
  htsmsg_t *l, *c, *m;
  htsmsg_field_t *f;
  const char *id;

  int records = 0;

  if((l = hts_settings_load("%s", dt->dt_tablename)) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_msg_by_field(f)) == NULL)
	continue;
      
      if((id = htsmsg_get_str(c, "id")) == NULL)
	continue;

      m = dt->dt_dtc->dtc_record_update(dt->dt_opaque, id, c, 1);
      if(m != NULL) {
	records++;
	htsmsg_destroy(m);
      }
    }
  }
  return records;
}


/**
 *
 */
dtable_t *
dtable_find(const char *name)
{
  dtable_t *dt;
  LIST_FOREACH(dt, &dtables, dt_link)
    if(!strcmp(dt->dt_tablename, name))
      break;
  return dt;
}


/**
 *
 */
int
dtable_record_update_by_array(dtable_t *dt, htsmsg_t *msg)
{
  htsmsg_t *c, *update;
  htsmsg_field_t *f;
  const char *id;
  int changed = 0;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    if((c = htsmsg_get_msg_by_field(f)) == NULL)
      continue;
    if((id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((update = dt->dt_dtc->dtc_record_update(dt->dt_opaque, id, c, 0)) 
       != NULL) {
      /* Data changed */
      changed = 1;
      hts_settings_save(update, "%s/%s", dt->dt_tablename, id);
      htsmsg_destroy(update);
    }
  }
  if(changed)
    dtable_store_changed(dt);
  return 0;
}


/**
 *
 */
void
dtable_record_delete(dtable_t *dt, const char *id)
{
  dt->dt_dtc->dtc_record_delete(dt->dt_opaque, id);
  hts_settings_remove("%s/%s", dt->dt_tablename, id);
  dtable_store_changed(dt);
}


/**
 *
 */
int
dtable_record_delete_by_array(dtable_t *dt, htsmsg_t *msg)
{
  htsmsg_field_t *f;
  const char *id;
  int changed = 0;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL) {
      changed = 1;
      dtable_record_delete(dt, id);
    }
  }
  if(changed)
    dtable_store_changed(dt);
  return 0;
}


/**
 *
 */
htsmsg_t *
dtable_record_create(dtable_t *dt)
{
  htsmsg_t *r;
  const char *id;

  if((r = dt->dt_dtc->dtc_record_create(dt->dt_opaque)) == NULL)
    return NULL;

  if((id = htsmsg_get_str(r, "id")) == NULL) {
    htsmsg_destroy(r);
    return NULL;
  }

  hts_settings_save(r, "%s/%s", dt->dt_tablename, id);
  return r;
}


/**
 *
 */
htsmsg_t *
dtable_record_get_all(dtable_t *dt)
{
  return dt->dt_dtc->dtc_record_get_all(dt->dt_opaque);
}


/**
 *
 */
void
dtable_record_store(dtable_t *dt, const char *id, htsmsg_t *r)
{
  hts_settings_save(r, "%s/%s", dt->dt_tablename, id);
}
