/*
 *  TVheadend - time processing
 *
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_TIME_H__
#define __TVH_TIME_H_

extern uint32_t tvhtime_update_enabled;
extern uint32_t tvhtime_ntp_enabled;
extern uint32_t tvhtime_tolerance;

void tvhtime_init ( void );
void tvhtime_update ( struct tm *now );

void tvhtime_set_update_enabled ( uint32_t on );
void tvhtime_set_ntp_enabled ( uint32_t on );
void tvhtime_set_tolerance ( uint32_t v );

#endif /* __TVH_TIME_H__ */
