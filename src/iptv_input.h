/*
 *  Multicasted IPTV Input
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef IPTV_INPUT_H_
#define IPTV_INPUT_H_

#include "service.h"

typedef enum {
    SERVICE_TYPE_IPTV_MCAST,
    SERVICE_TYPE_IPTV_RTSP
} iptv_subtype_t;

void iptv_input_init(void);

struct service *iptv_service_find(const char *id, int create);
int iptv_is_service_subtype(service_t *service, iptv_subtype_t subtype);

extern struct service_list iptv_all_services;


#endif /* IPTV_INPUT_H_ */
