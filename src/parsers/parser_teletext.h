/*
 *  Teletext parsing functions
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

#ifndef TELETEXT_H
#define TELETEXT_H

#define PID_TELETEXT_BASE 0x2000

#include "input.h"

void teletext_input(struct mpegts_service *t, struct elementary_stream *st,
		    const uint8_t *data, int len);

#endif /* TELETEXT_H */
