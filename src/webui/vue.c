// SPDX-License-Identifier: GPL-3.0-or-later
/*
 *  tvheadend, Vue web user interface
 *  Copyright (C) 2026 Tvheadend contributors
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
#include "filebundle.h"

#if ENABLE_VUE_UI

#define VUE_DIST "src/webui/static-vue/dist"

/* The <base href> token in dist/index.html; the webroot is injected
 * right after `href="` so the SPA discovers its mount point. */
#define VUE_BASE_TOKEN  "href=\"/gui/static/\""
#define VUE_BASE_SKIP   6 /* strlen("href=\"") */

/* Serve index.html with tvheadend_webroot injected into its
 * <base href> so the app derives all URLs from the real mount. */
static int
page_vue_index_webroot(http_connection_t *hc)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  fb_file *fp;
  char *buf, *pos;
  size_t size, off = 0;

  fp = fb_open(VUE_DIST "/index.html", 1, 0);
  if (!fp)
    return HTTP_STATUS_INTERNAL;
  size = fb_size(fp);
  buf = malloc(size + 1);
  if (!buf) {
    fb_close(fp);
    return HTTP_STATUS_INTERNAL;
  }
  while (off < size && !fb_eof(fp)) {
    ssize_t c = fb_read(fp, buf + off, size - off);
    if (c <= 0)
      break;
    off += c;
  }
  fb_close(fp);
  if (off != size) {
    free(buf);
    return HTTP_STATUS_INTERNAL;
  }
  buf[size] = '\0';

  pos = strstr(buf, VUE_BASE_TOKEN);
  if (pos) {
    htsbuf_append(hq, buf, (pos - buf) + VUE_BASE_SKIP);
    htsbuf_append_str(hq, tvheadend_webroot);
    htsbuf_append_str(hq, pos + VUE_BASE_SKIP);
  } else {
    htsbuf_append(hq, buf, size);
  }
  free(buf);
  http_output_html(hc);
  return 0;
}

static int
page_vue_index(http_connection_t *hc, const char *remain, void *opaque)
{
  /* Serves index.html for /gui, /gui/, and any unknown subpath, so Vue
   * Router history mode survives hard reloads on deep links. Real
   * assets are intercepted first by the more-specific /gui/static route
   * (LIFO matching in http_resolve). The remain/opaque params are part
   * of the http_callback_t signature mandated by http_path_add() but
   * aren't needed here — page_static_file builds the path from VUE_DIST
   * + a fixed filename. */
  (void)remain;
  (void)opaque;
  if (tvheadend_webroot == NULL)
    return page_static_file(hc, "index.html", (void *)VUE_DIST);
  return page_vue_index_webroot(hc);
}

#else /* !ENABLE_VUE_UI */

static int
page_vue_index(http_connection_t *hc, const char *remain, void *opaque)
{
  /* No Vue UI bundled — neither vue_build (local npm) nor vue_cache
   * (pre-built dist) was enabled at configure time. Serve a minimal
   * stub pointing users to the existing ExtJS UI rather than 500ing on
   * a missing dist/index.html. remain/opaque are unused — see above. */
  (void)remain;
  (void)opaque;
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsbuf_append_str(hq,
    "<!DOCTYPE html><html lang=\"en\"><head>"
    "<meta charset=\"utf-8\"><title>Tvheadend Vue UI</title>"
    "</head><body>"
    "<h1>Vue UI not available in this build</h1>"
    "<p>This binary was built without the Vue UI "
    "(neither <code>vue_build</code> nor <code>vue_cache</code> enabled). ");
  /* Honour the webroot in the escape link. */
  htsbuf_qprintf(hq,
    "<a href=\"%s/extjs.html\">Use the ExtJS UI instead.</a></p>"
    "</body></html>\n", tvheadend_webroot ?: "");
  http_output_html(hc);
  return 0;
}

#endif /* ENABLE_VUE_UI */

void
vue_init(void)
{
  /* Registration order is significant: http_path_add inserts at the
   * head of the path list (src/http.c LIST_INSERT_HEAD), so later
   * registrations are matched first (LIFO). /gui/static must register
   * AFTER /gui so the more-specific prefix wins for asset URLs.
   * Reordering these two lines would silently route asset requests
   * through the SPA fallback, breaking the UI.
   *
   * When ENABLE_VUE_UI is off, /gui/static is not registered at all
   * — there is no dist/ to serve from. Requests under /gui/static/...
   * fall through to the bare /gui handler and get the fallback stub. */
  http_path_add("/gui",        NULL,
                page_vue_index, ACCESS_WEB_INTERFACE);
#if ENABLE_VUE_UI
  http_path_add("/gui/static", (void *)VUE_DIST,
                page_static_file, ACCESS_WEB_INTERFACE);
#endif
}
