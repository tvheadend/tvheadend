/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * tvheadend, web user interface
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
