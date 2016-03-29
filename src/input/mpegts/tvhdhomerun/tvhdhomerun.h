/*
 *  Tvheadend - HDHomeRun DVB private data
 *
 *  Copyright (C) 2014 Patric Karlström
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

#ifndef __TVH_tvhdhomerun_H__
#define __TVH_tvhdhomerun_H__

extern const idclass_t tvhdhomerun_device_class;
extern const idclass_t tvhdhomerun_frontend_dvbt_class;
extern const idclass_t tvhdhomerun_frontend_dvbc_class;
extern const idclass_t tvhdhomerun_frontend_atsc_t_class;
extern const idclass_t tvhdhomerun_frontend_atsc_c_class;

void tvhdhomerun_init( void );
void tvhdhomerun_done( void );

#endif /* __TVH_SATIP_H__ */
