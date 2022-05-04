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
#include "access.h"

#define TVH_API_VERSION 19

/*
 * Command hook
 */

typedef int (*api_callback_t)
  ( access_t *perm, void *opaque, const char *op,
    htsmsg_t *args, htsmsg_t **resp );

typedef struct api_hook
{
  const char         *ah_subsystem;
  uint32_t            ah_access;
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
int  api_exec ( access_t *perm, const char *subsystem,
                htsmsg_t *args, htsmsg_t **resp );

/*
 * Initialise
 */
void api_init               ( void );
void api_done               ( void );
void api_config_init        ( void );
void api_idnode_init        ( void );
void api_idnode_raw_init    ( void );
void api_input_init         ( void );
void api_service_init       ( void );
void api_channel_init       ( void );
void api_bouquet_init       ( void );
void api_mpegts_init        ( void );
void api_epg_init           ( void );
void api_epggrab_init       ( void );
void api_status_init        ( void );
void api_imagecache_init    ( void );
void api_esfilter_init      ( void );
void api_intlconv_init      ( void );
void api_access_init        ( void );
void api_dvr_init           ( void );
void api_caclient_init      ( void );
void api_profile_init       ( void );
void api_language_init      ( void );
void api_satip_server_init  ( void );
void api_timeshift_init     ( void );
void api_wizard_init        ( void );

#if ENABLE_LIBAV
void api_codec_init         ( void );
#else
static inline void api_codec_init(void) {};
#endif

/*
 * IDnode
 */
typedef struct api_idnode_grid_conf
{
  uint32_t        start;
  uint32_t        limit;
  idnode_filter_t filter;
  idnode_sort_t   sort;
} api_idnode_grid_conf_t;

typedef void (*api_idnode_grid_callback_t)
  (access_t *perm, idnode_set_t*, api_idnode_grid_conf_t*, htsmsg_t *args);
typedef idnode_set_t *(*api_idnode_tree_callback_t)
  (access_t *perm);

htsmsg_t *api_idnode_flist_conf
  ( htsmsg_t *args, const char *name );

int api_idnode_grid
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_class
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_tree
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_load_by_class
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_handler
  ( const idclass_t *idc, access_t *perm, htsmsg_t *args, htsmsg_t **resp,
    void (*handler)(access_t *perm, idnode_t *in), const char *op, int destroyed );

int api_idnode_load_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

int api_idnode_save_simple
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp );

void api_idnode_create
  ( htsmsg_t **resp, idnode_t *in );

void api_idnode_create_list
  ( htsmsg_t **resp, htsmsg_t *list );

/*
 * Service mapper
 */
void api_service_mapper_notify ( void );

#endif /* __TVH_API_H__ */
