/*
 *  Download a file from storage or network
 *
 *  Copyright (C) 2015 Jaroslav Kysela
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

#ifndef __DOWNLOAD__
#define __DOWNLOAD__

#include "http.h"

typedef struct download {
  char *log;
  char *url;
  void *aux;
  int ssl_peer_verify;
  int (*process)(void *aux, const char *last_url, char *data, size_t len);
  void (*stop)(void *aux);
  /* internal members */
  http_client_t *http_client;
  gtimer_t fetch_timer;
} download_t;

void download_init ( download_t *dn, const char *log );
void download_start( download_t *dn, const char *url, void *aux );
void download_done ( download_t *dn );

#endif /* __DOWNLOAD__ */
