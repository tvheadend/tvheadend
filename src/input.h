/*
 *  TVheadend
 *  Copyright (C) 2007 - 2010 Andreas �man
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

#ifndef __TVH_INPUT_H__
#define __TVH_INPUT_H__

#if ENABLE_MPEGPS
#include "input/mpegps.h"
#endif

#if ENABLE_MPEGTS
#include "input/mpegts.h"
#endif

void input_init ( void );

#endif /* __TVH_INPUT_H__ */
