/*
 *  tvheadend, encoding list
 *  Copyright (C) 2012 Mariusz Białończyk
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

#ifndef __TVH_ENCODING_H__
#define __TVH_ENCODING_H__

typedef struct encoding {
  LIST_ENTRY(encoding) link;
 unsigned int tsid;
 unsigned int onid;
} encoding_t;

LIST_HEAD(,encoding) encoding_list;

void encoding_init ( void );

#endif /* __TVH_ENCODING_H__ */
