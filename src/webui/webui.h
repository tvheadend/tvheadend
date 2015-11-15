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

void webui_init(int xspf);
void webui_done(void);

void simpleui_start(void);

void extjs_start(void);

size_t html_escaped_len(const char *src);
const char* html_escape(char *dst, const char *src, size_t len);

int page_static_file(http_connection_t *hc, const char *remain, void *opaque);
int page_xmltv(http_connection_t *hc, const char *remain, void *opaque);

#if ENABLE_LINUXDVB
void extjs_start_dvb(void);
#endif

void webui_api_init ( void );


/**
 *
 */
void comet_init(void);

void comet_done(void);

void comet_mailbox_add_message(htsmsg_t *m, int isdebug, int rewrite);

void comet_flush(void);

#endif /* WEBUI_H_ */
