/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * tvheadend, UPnP interface
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

void
dbus_register_rpc_s64(const char *call_name, void *opaque,
                      int64_t (*fcn)(void *, const char *, int64_t));

void
dbus_register_rpc_str(const char *call_name, void *opaque,
                      char *(*fcn)(void *, const char *, char *));

void dbus_server_init(int enabled, int session);
void dbus_server_start(void);
void dbus_server_done(void);

#else

static inline void
dbus_emit_signal(const char *obj_name, const char *sig_name, htsmsg_t *msg) { htsmsg_destroy(msg); }
static inline void
dbus_emit_signal_str(const char *obj_name, const char *sig_name, const char *value) { }
static inline void
dbus_emit_signal_s64(const char *obj_name, const char *sig_name, int64_t value) { }

static inline void
dbus_register_rpc_s64(const char *call_name, void *opaque,
                      int64_t (*fcn)(void *, const char *, int64_t)) { }

static inline void
dbus_register_rpc_str(const char *call_name, void *opaque,
                      char *(*fcn)(void *, const char *, char *)) { }

static inline void dbus_server_init(int enabled, int session) { }
static inline void dbus_server_start(void) { }
static inline void dbus_server_done(void) { }

#endif

#endif /* DBUS_H_ */
