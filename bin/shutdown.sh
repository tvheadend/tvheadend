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
# Script to shutdown or suspend the computer

logger -p daemon.debug -t tvheadendscript "Shutdown executed!"
sudo /sbin/shutdown -h now
exit 0
