/*
 *  TV Input - Linux DVB interface
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

#ifndef INPUT_DVB_H
#define INPUT_DVB_H

void dvb_add_adapters(void);

th_dvb_adapter_t *dvb_alloc_adapter(struct dvb_frontend_parameters *params,
				    int force);

int dvb_configure_transport(th_transport_t *t, const char *muxname);

int dvb_start_feed(th_transport_t *t, unsigned int weight);

void dvb_stop_feed(th_transport_t *t);

#endif /* INPUT_DVB_H */
