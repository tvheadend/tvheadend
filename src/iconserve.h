/*
 *  Icon file serve operations
 *  Copyright (C) 2012 Andy Brown
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

#ifndef ICONSERVE_H
#define ICONSERVE_H

#include "http.h"

/* Struct of entries for icon grabbing
 * FIELD: chan_number
 * FIELD: icon url
 */
typedef struct iconserve_grab_queue
{
  TAILQ_ENTRY(iconserve_grab_queue) iconserve_link;
  int chan_number;
  char *icon_url;
} iconserve_grab_queue_t;


int page_logo(http_connection_t *hc, const char *remain, void *opaque);
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

void *iconserve_thread ( void *aux );

const char *logo_query(int ch_id, const char *ch_icon);

void iconserve_queue_add ( int chan_number, char *icon_url );

void logo_loader(void);

#endif /* ICONSERVE_H */
