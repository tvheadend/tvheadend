/*
 *  TV headend - Timeshift
 *  Copyright (C) 2012 Adam Sutton
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

#ifndef __TVH_TIMESHIFT_H__
#define __TVH_TIMESHIFT_H__

extern uint32_t  timeshift_enabled;
extern int       timeshift_ondemand;
extern char     *timeshift_path;
extern int       timeshift_unlimited_period;
extern uint32_t  timeshift_max_period;
extern int       timeshift_unlimited_size;
extern size_t    timeshift_max_size;

void timeshift_init ( void );
void timeshift_term ( void );
void timeshift_save ( void );

streaming_target_t *timeshift_create
  (streaming_target_t *out, time_t max_period);

void timeshift_destroy(streaming_target_t *pad);

#endif /* __TVH_TIMESHIFT_H__ */
