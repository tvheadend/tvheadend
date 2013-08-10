/*
 *  tvheadend, WebAPI access point
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

#include "tvheadend.h"
#include "webui.h"
#include "access.h"
#include "http.h"
#include "api.h"
#include "htsmsg.h"
#include "htsmsg_json.h"

static int
webui_api_handler
  ( http_connection_t *hc, const char *remain, void *opaque )
{
  int r;
  htsmsg_t *args, *resp;
  const char *a  = http_arg_get(&hc->hc_req_args, "args");
  const char *op = http_arg_get(&hc->hc_req_args, "method");

  // Compat
  if (!op)
    op = http_arg_get(&hc->hc_req_args, "op");


  /* Parse arguments */
  if (a)
    args = htsmsg_json_deserialize(a);
  else
    args = htsmsg_create_map();
  if (!args)
    return HTTP_STATUS_BAD_REQUEST;

  /* Add operation */
  if (!htsmsg_get_str(args, "method") && op)
    htsmsg_add_str(args, "method", op);

  /* Call */
  r = api_exec(remain, args, &resp);
  
  /* Convert error */
  if (r) {
    switch (r) {
      case EACCES:
        r = HTTP_STATUS_UNAUTHORIZED;
      case ENOENT:
      case ENOSYS:
        r = HTTP_STATUS_NOT_FOUND;
      default:
        r = HTTP_STATUS_BAD_REQUEST;
    }

  /* Output response */
  } else {
    htsmsg_json_serialize(resp, &hc->hc_reply, 0);
    http_output_content(hc, "text/x-json; charset=UTF-8");
    htsmsg_destroy(resp);
  }
  
  return r;
}

void
webui_api_init ( void )
{
  http_path_add("/api", NULL, webui_api_handler, ACCESS_WEB_INTERFACE);
}
