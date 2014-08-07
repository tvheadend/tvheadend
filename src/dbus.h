/*
 *  tvheadend, UPnP interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#ifndef DBUS_H_
#define DBUS_H_

#include "build.h"
#include "htsmsg.h"

#if ENABLE_DBUS_1

void
dbus_emit_signal(const char *obj_name, const char *sig_name, htsmsg_t *msg);
void
dbus_emit_signal_str(const char *obj_name, const char *sig_name, const char *value);
void
dbus_emit_signal_s64(const char *obj_name, const char *sig_name, int64_t value);

void dbus_server_init(void);
void dbus_server_start(void);
void dbus_server_done(void);

#else

static inline void
dbus_emit_signal(const char *sig_name, htsmsg_t *msg) { htsmsg_destroy(msg); }
static inline void
dbus_emit_signal_str(const char *sig_name, const char *value) { }
static inline void
dbus_emit_signal_s64(const char *sig_name, int64_t value) { }

static inline void dbus_server_init(void) { }
static inline void dbus_server_done(void) { }

#endif

#endif /* DBUS_H_ */
