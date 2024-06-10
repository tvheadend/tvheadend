/*
 *  Functions converting HTSMSGs to/from a simple binary format
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

#ifndef HTSMSG_BINARY2_H_
#define HTSMSG_BINARY2_H_

#include "htsmsg.h"

htsmsg_t* htsmsg_binary2_deserialize0(const void* data, size_t len, const void* buf);

int htsmsg_binary2_deserialize(htsmsg_t** msg, const void* data, size_t* len, const void* buf);

int htsmsg_binary2_serialize0(htsmsg_t* msg, void** datap, size_t* lenp, size_t maxlen);

int htsmsg_binary2_serialize(htsmsg_t* msg, void** datap, size_t* lenp, size_t maxlen);

#endif /* HTSMSG_BINARY2_H_ */
