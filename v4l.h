/*
 *  TV Input - Linux analogue (v4lv2) interface
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

#ifndef V4L_H_
#define V4L_H_

extern struct th_v4l_adapter_list v4l_adapters;

void v4l_init(void);

int v4l_configure_transport(th_transport_t *t, const char *muxname,
			    const char *channel_name);

int v4l_start_feed(th_transport_t *t, unsigned int weight);

void v4l_stop_feed(th_transport_t *t);

#endif /* V4L_H */
