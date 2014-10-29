/*
 *  TV headend - Bouquets
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef BOUQUET_H_
#define BOUQUET_H_

#include "idnode.h"
#include "htsmsg.h"
#include "service.h"

typedef struct bouquet {
  idnode_t bq_id;
  RB_ENTRY(bouquet) bq_link;

  int           bq_saveflag;
  int           bq_in_load;

  int           bq_shield;
  int           bq_enabled;
  char         *bq_name;
  char         *bq_src;
  char         *bq_comment;
  idnode_set_t *bq_services;
  htsmsg_t     *bq_services_waiting;
  uint32_t      bq_lcn_offset;

} bouquet_t;

typedef RB_HEAD(,bouquet) bouquet_tree_t;

extern bouquet_tree_t bouquets;

extern const idclass_t bouquet_class;

/**
 *
 */
bouquet_t *
bouquet_create(const char *uuid, htsmsg_t *conf,
               const char *name, const char *src);

/**
 *
 */
void
bouquet_destroy_by_service(service_t *t);

/**
 *
 */
bouquet_t *
bouquet_find_by_source(const char *name, const char *src, int create);

/**
 *
 */
void
bouquet_add_service(bouquet_t *bq, service_t *s);

/**
 *
 */
void
bouquet_save(bouquet_t *bq, int notify);

/**
 *
 */
void bouquet_init(void);
void bouquet_service_resolve(void);
void bouquet_done(void);

#endif /* BOUQUET_H_ */
