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

#ifndef DTABLE_H__
#define DTABLE_H__

#include "htsmsg.h"

#include "access.h"

typedef struct dtable_class {
  const char *dtc_name;

  htsmsg_t *(*dtc_record_get_all)(void *opaque);

  htsmsg_t *(*dtc_record_get)(void *opaque, const char *id);
  
  htsmsg_t *(*dtc_record_create)(void *opaque);
  
  htsmsg_t *(*dtc_record_update)(void *opaque, const char *id, 
				 htsmsg_t *values, int maycreate);

  int (*dtc_record_delete)(void *opaque, const char *id);

  int dtc_read_access;
  int dtc_write_access;

} dtable_class_t;


typedef struct dtable {
  LIST_ENTRY(dtable) dt_link;

  void *dt_opaque;
  char *dt_tablename;

  const dtable_class_t *dt_dtc;

} dtable_t;

dtable_t *dtable_create(const dtable_class_t *dtc, const char *name, 
			void *opaque);

int dtable_load(dtable_t *dt);

dtable_t *dtable_find(const char *name);

int dtable_record_update_by_array(dtable_t *dt, htsmsg_t *msg);

void dtable_record_delete(dtable_t *dt, const char *id);

int dtable_record_delete_by_array(dtable_t *dt, htsmsg_t *msg);

htsmsg_t *dtable_record_create(dtable_t *dt);

htsmsg_t *dtable_record_get_all(dtable_t *dt);

void dtable_record_store(dtable_t *dt, const char *id, htsmsg_t *r);

void dtable_record_erase(dtable_t *dt, const char *id);

#endif /* DTABLE_H__ */
