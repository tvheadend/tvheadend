/*
 *  TVheadend - JavaScript support
 *
 *  Copyright (C) 2019 Tvheadend Foundation CIC
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TVHJS_H__
#define __TVHJS_H__

#include "duk_config.h"

void tvhjs_init   ( void );
void tvhjs_done   ( void );

duk_context *tvhjs_get_ctx(void);

#endif
