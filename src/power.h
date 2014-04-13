/*
 *  tvheadend, power management functions
 *  Copyright (C) 2014 Joerg Werner
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

#ifndef POWER_H
#define POWER_H

#include "service.h"
#include <time.h>

extern uint32_t power_enabled;
extern uint32_t power_idletime;
extern uint32_t power_prerecordingwakeup;
extern char* power_epgwakeup;
extern char* power_rtcscript;
extern char* power_preshutdownscript;
extern char* power_shutdownscript;
extern time_t last_wakeup_time;

time_t
power_reschedule_poweron(void);

void
power_schedule_poweron(time_t next_recording_time);

void
power_init(void);

void
power_done(void);

void
tvhpower_set_power_enabled(uint32_t on);
void
tvhpower_set_power_idletime(uint32_t idletime);
void
tvhpower_set_power_prerecordingwakeup(uint32_t prerecordwakeup);
void
tvhpower_set_power_epgwakeup(const char* epgwakeup);
void
tvhpower_set_power_rtcscript(const char* rtcscript);
void
tvhpower_set_power_preshutdownscript(const char* preshutdownscript);
void
tvhpower_set_power_shutdownscript(const char* shutdownscript);

#endif /* POWER_H */
