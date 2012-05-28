/*
 *  Electronic Program Guide - eit grabber
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

#ifndef EPGGRAB_EIT_H
#define EPGGRAB_EIT_H

#include "epggrab.h"

epggrab_module_t *eit_init ( void );

void eit_callback ( channel_t *ch, int id, time_t start, time_t stop,
                    const char *title, const char *desc,
                    const char *extitem, const char *extdesc,
                    const char *exttext );

#endif
