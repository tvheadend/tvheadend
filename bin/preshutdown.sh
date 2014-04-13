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
# Script to check if it is okay to shutdown the computer
# If it outputs "true" then tvheadend will execute the shutdown.sh script

USERS=`who -q | tail -n 1 | sed 's/[A-Za-z #]*=//'`

if [ "$USERS" == "0" ]; then
    logger -p daemon.debug -t tvheadendscript "No more users logged in, okay to shut down."
    echo "true"
    exit 0
  else
    logger -p daemon.debug -t tvheadendscript "Someone is still logged in, don't shut down."
    echo "false"
    exit 1
fi

exit 0
