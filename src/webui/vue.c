/*
 *  tvheadend, Vue web user interface
 *  Copyright (C) 2026 Oliver Sluke
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
#include "http.h"
#include "webui.h"

#if ENABLE_VUE_BUILD

#define VUE_DIST "src/webui/static-vue/dist"

static int
page_vue_index(http_connection_t *hc, const char *remain, void *opaque)
{
  /* Serves index.html for /vue, /vue/, and any unknown subpath, so Vue
   * Router history mode survives hard reloads on deep links. Real
   * assets are intercepted first by the more-specific /vue/static route
   * (LIFO matching in http_resolve). The remain/opaque params are part
   * of the http_callback_t signature mandated by http_path_add() but
   * aren't needed here — page_static_file builds the path from VUE_DIST
   * + a fixed filename. */
  (void)remain;
  (void)opaque;
  return page_static_file(hc, "index.html", (void *)VUE_DIST);
}

#else /* !ENABLE_VUE_BUILD */

static int
page_vue_index(http_connection_t *hc, const char *remain, void *opaque)
{
  /* Vue UI build was disabled at configure time (--disable-vue_build,
   * or node/npm were not detected). Serve a minimal stub pointing
   * users to the existing ExtJS UI rather than 500ing on a missing
   * dist/index.html. remain/opaque are unused — see comment above. */
  (void)remain;
  (void)opaque;
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsbuf_append_str(hq,
    "<!DOCTYPE html><html lang=\"en\"><head>"
    "<meta charset=\"utf-8\"><title>Tvheadend Vue UI</title>"
    "</head><body>"
    "<h1>Vue UI not available in this build</h1>"
    "<p>This binary was built with <code>--disable-vue_build</code>, "
    "or <code>node</code>/<code>npm</code> were not detected at "
    "<code>./configure</code> time. "
    "<a href=\"/extjs.html\">Use the ExtJS UI instead.</a></p>"
    "</body></html>\n");
  http_output_html(hc);
  return 0;
}

#endif /* ENABLE_VUE_BUILD */

void
vue_init(void)
{
  /* Registration order is significant: http_path_add inserts at the
   * head of the path list (src/http.c LIST_INSERT_HEAD), so later
   * registrations are matched first (LIFO). /vue/static must register
   * AFTER /vue so the more-specific prefix wins for asset URLs.
   * Reordering these two lines would silently route asset requests
   * through the SPA fallback, breaking the UI.
   *
   * When ENABLE_VUE_BUILD is off, /vue/static is not registered at all
   * — there is no dist/ to serve from. Requests under /vue/static/...
   * fall through to the bare /vue handler and get the fallback stub. */
  http_path_add("/vue",        NULL,
                page_vue_index, ACCESS_WEB_INTERFACE);
#if ENABLE_VUE_BUILD
  http_path_add("/vue/static", (void *)VUE_DIST,
                page_static_file, ACCESS_WEB_INTERFACE);
#endif
}
