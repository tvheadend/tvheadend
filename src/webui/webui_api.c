/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * tvheadend, WebAPI access point
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
  http_arg_t *ha;
  htsmsg_t *args, *resp = NULL;

  /* Build arguments */
  args = htsmsg_create_map();
  TAILQ_FOREACH(ha, &hc->hc_req_args, link) {
    if (ha->val == NULL) {
      r = EINVAL;
      goto destroy_args;
    }
    htsmsg_add_str(args, ha->key, ha->val);
  }
      
  /* Call */
  r = api_exec(hc->hc_access, remain, args, &resp);

destroy_args:
  htsmsg_destroy(args);
  
  /* Convert error */
  if (r) {
    if (r < 0) {
      tvherror(LS_HTTP, "negative error code %d for url '%s'", r, hc->hc_url);
      r = ENOSYS;
    }
    switch (r) {
      case EPERM:
      case EACCES:
        r = http_noaccess_code(hc);
        break;
      case ENOENT:
      case ENOSYS:
        r = HTTP_STATUS_NOT_FOUND;
        break;
      default:
        r = HTTP_STATUS_BAD_REQUEST;
        break;
    }
  }

  /* Output response */
  if (!r && !resp)
    resp = htsmsg_create_map();
  if (resp) {
    htsmsg_json_serialize(resp, &hc->hc_reply, 0);
    http_output_content(hc, "application/json; charset=UTF-8");
    htsmsg_destroy(resp);
  }
  
  return r;
}

void
webui_api_init ( void )
{
  http_path_add("/api", NULL, webui_api_handler, ACCESS_WEB_INTERFACE);
}
