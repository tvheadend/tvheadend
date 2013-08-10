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
void api_register ( const api_hook_t *hooks );

/*
 * Execute
 */
int  api_exec ( const char *subsystem, htsmsg_t *args, htsmsg_t **resp );

/*
 * Initialise
 */
void api_init ( void );

#endif /* __TVH_API_H__ */
