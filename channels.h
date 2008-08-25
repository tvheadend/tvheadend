/*
 *  tvheadend, channel functions
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

#ifndef CHANNELS_H
#define CHANNELS_H

void channels_load(void);

channel_t *channel_by_index(uint32_t id);

channel_t *channel_by_tag(uint32_t tag);

int id_by_channel(channel_t *ch);

int channel_get_channels(void);

void channel_unsubscribe(th_subscription_t *s);

channel_t *channel_find(const char *name, int create);

void channel_set_teletext_rundown(channel_t *ch, int v);

void channel_settings_write(channel_t *ch);

int channel_rename(channel_t *ch, const char *newname);

void channel_delete(channel_t *ch);

void channel_merge(channel_t *dst, channel_t *src);

void channel_set_icon(channel_t *ch, const char *icon);

#endif /* CHANNELS_H */
