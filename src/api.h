/*
 *  API - Common functions for control/query API
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_API_H__
#define __TVH_API_H__

#include "htsmsg.h"
#include "idnode.h"
#include "redblack.h"

#define TVH_API_VERSION 12

/*
 * Command hook
 */

typedef int (*api_callback_t)
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

typedef struct api_hook
{
  const char         *ah_subsystem;
  int                 ah_access;
  api_callback_t      ah_callback;
  void               *ah_opaque;
} api_hook_t;

/*
 * Regsiter handler
 */
void api_register     ( const api_hook_t *hook );
void api_register_all ( const api_hook_t *hooks );

/*
 * Execute
 */
int  api_exec ( const char *subsystem, htsmsg_t *args, htsmsg_t **resp );

/*
 * Initialise
 */
void api_init         ( void );
void api_idnode_init  ( void );
void api_input_init   ( void );
void api_service_init ( void );
void api_channel_init ( void );
void api_mpegts_init  ( void );
void api_epg_init     ( void );
void api_epggrab_init ( void );

/*
 * IDnode
 */
typedef struct api_idnode_grid_conf
{
  int             start;
  int             limit;
  idnode_filter_t filter;
  idnode_sort_t   sort;
} api_idnode_grid_conf_t;

typedef void (*api_idnode_grid_callback_t)
  (idnode_set_t*, api_idnode_grid_conf_t*, htsmsg_t *args);
typedef idnode_set_t *(*api_idnode_tree_callback_t)
  (void);

int api_idnode_grid
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_class
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_tree
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_load_by_class
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

#endif /* __TVH_API_H__ */
