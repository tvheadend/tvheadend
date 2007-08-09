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

#ifndef INPUT_IPTV_H
#define UNPUT_IPTV_H

int iptv_configure_transport(th_transport_t *t, const char *muxname);

int iptv_start_feed(th_transport_t *t, unsigned int weight);

int iptv_stop_feed(th_transport_t *t);

#endif /* INPUT_IPTV_H */
