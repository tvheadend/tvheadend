/*
 *  tvheadend, HTSP interface
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

#ifndef HTSP_H_
#define HTSP_H_

#include "epg.h"
#include "dvr/dvr.h"

void htsp_init(void);

void htsp_channgel_update_current(channel_t *ch);

void htsp_channel_add(channel_t *ch);
void htsp_channel_update(channel_t *ch);
void htsp_channel_delete(channel_t *ch);

void htsp_tag_add(channel_tag_t *ct);
void htsp_tag_update(channel_tag_t *ct);
void htsp_tag_delete(channel_tag_t *ct);

void htsp_dvr_entry_add(dvr_entry_t *de);
void htsp_dvr_entry_update(dvr_entry_t *de);
void htsp_dvr_entry_delete(dvr_entry_t *de);

#endif /* HTSP_H_ */
