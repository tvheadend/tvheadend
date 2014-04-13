#!/bin/bash
#
#  tvheadend, power management functions
#  Copyright (C) 2014 Joerg Werner
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Script to set the rtc wake-up time
# $1 is the wake-up time in seconds since epoch, or -1 for disabling the wake-up
(
    flock --timeout 10 9 || exit 1
    if (( $1 > 0 )); then
	logger -p daemon.debug -t tvheadendscript "Setting wake-up time to $1"
	sudo /sbin/rtcwake -m no -t $1 
    else
	logger -p daemon.debug -t tvheadendscript "Disabling wake-up"
	sudo /sbin/rtcwake -m disable
    fi
    logger -p daemon.debug -t tvheadendscript "Finished setting wake-up"
) 9>/tmp/setwakeup.sh.lock


exit 0
