/*
 *  API - channel related calls
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

#ifndef __TVH_API_INPUT_H__
#define __TVH_API_INPUT_H__

#include "tvheadend.h"
#include "idnode.h"
#include "input.h"
#include "access.h"
#include "api.h"

static int
api_input_status
  ( void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int c = 0;
  htsmsg_t *l, *e;
  tvh_input_t *ti;
  tvh_input_stream_t *st;
  tvh_input_stream_list_t stl = { 0 };
  
  TVH_INPUT_FOREACH(ti)
    ti->ti_get_streams(ti, &stl);

  l = htsmsg_create_list();
  while ((st = LIST_FIRST(&stl))) {
    e = tvh_input_stream_create_msg(st);
    htsmsg_add_msg(l, NULL, e);
    tvh_input_stream_destroy(st);
    LIST_REMOVE(st, link);
    free(st);
    c++;
  }
    
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);
  htsmsg_add_u32(*resp, "totalCount", c);

  return 0;
}

void api_input_init ( void )
{
  static api_hook_t ah[] = {
    { "input/status",  ACCESS_ANONYMOUS, api_input_status, NULL },
//    { "input/tree",    ACCESS_ANONYMOUS, NULL, NULL },
    { NULL },
  };

  api_register_all(ah);
}


#endif /* __TVH_API_INPUT_H__ */
