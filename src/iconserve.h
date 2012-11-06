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

int page_logo(http_connection_t *hc, const char *remain, void *opaque);
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

const char *logo_query(const char *ch_icon);

void logo_loader(void);

#endif /* ICONSERVE_H */
