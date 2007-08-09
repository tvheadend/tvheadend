/*
 *  Electronic Program Guide - Common functions
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

#ifndef EPG_H
#define EPG_H

programme_t *epg_find_programme(th_channel_t *ch, 
				time_t start, time_t stop);

programme_t *epg_find_programme_by_time(th_channel_t *ch, time_t t);

/* XXX: this is an ugly one */

programme_t *epg_get_prog_by_id(th_channel_t *ch, unsigned int progid);

/* epg_get_cur_prog must be called while holding ch_prg_mutex */

programme_t *epg_get_cur_prog(th_channel_t *ch);

#endif /* EPG_H */
