/*
 *  tvheadend, web user interface
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef WEBUI_H_
#define WEBUI_H_

#include "htsmsg.h"
#include "idnode.h"
#include "http.h"

#define MIME_M3U      "audio/x-mpegurl"
#define MIME_E2       "application/x-e2-bouquet"
#define MIME_XSPF_XML "application/xspf+xml"

void webui_init(int xspf);
void webui_done(void);

void simpleui_start(void);

void extjs_start(void);

/* The Vue UI is embedded whenever it was built locally (vue_build) or
 * fetched prebuilt (vue_cache) — both bundle src/webui/static-vue/dist
 * (Makefile BUNDLES-$(CONFIG_VUE_BUILD)/$(CONFIG_VUE_CACHE)). Serving and
 * the root redirect gate on this, not ENABLE_VUE_BUILD alone, or a cache
 * build bundles the UI but serves the fallback stub. */
#define ENABLE_VUE_UI (ENABLE_VUE_BUILD || ENABLE_VUE_CACHE)

void vue_init(void);

size_t html_escaped_len(const char *src);
const char* html_escape(char *dst, const char *src, size_t len);

int
http_serve_file(http_connection_t *hc, const char *fname,
                int fconv, const char *content,
                int (*preop)(http_connection_t *hc, off_t file_start,
                             size_t content_len, void *opaque),
                int (*postop)(http_connection_t *hc, off_t file_start,
                              size_t content_len, off_t file_size, void *opaque),
                void (*stats)(http_connection_t *hc, size_t len, void *opaque),
                void *opaque);

int page_static_file(http_connection_t *hc, const char *remain, void *opaque);
int page_static_file_maxage(http_connection_t *hc, const char *remain,
                            const char *base, int maxage);
int page_xmltv(http_connection_t *hc, const char *remain, void *opaque);
int page_markdown(http_connection_t *hc, const char *remain, void *opaque);

#if ENABLE_LINUXDVB
void extjs_start_dvb(void);
#endif

void webui_api_init ( void );


/**
 *
 */
void comet_init(void);

void comet_done(void);

void comet_mailbox_add_message(htsmsg_t *m, int isdebug, int isrestricted, int rewrite);

void comet_mailbox_add_logmsg(const char *txt, int isdebug, int rewrite);

void comet_flush(void);

#endif /* WEBUI_H_ */
