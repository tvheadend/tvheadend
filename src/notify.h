/*
 *  tvheadend, Notification framework
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef NOTIFY_H_
#define NOTIFY_H_

#include "htsmsg.h"

#define NOTIFY_REWRITE_SUBSCRIPTIONS 1

void notify_by_msg(const char *class, htsmsg_t *m, int rewrite);

void notify_reload(const char *class);

void notify_delayed(const char *id, const char *event, const char *action);

void notify_init(void);
void notify_done(void);

#endif /* NOTIFY_H_ */
