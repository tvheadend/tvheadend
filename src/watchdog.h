/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2017-2018 Erkki Seppälä
 *
 * tvheadend, systemd watchdog support
 */

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include "config.h"

#if ENABLE_LIBSYSTEMD_DAEMON

void watchdog_init(void);
void watchdog_done(void);

#else /* #if ENABLE_LIBSYSTEMD_DAEMON */

static inline void watchdog_init(void) { }
static inline void watchdog_done(void) { }

#endif /* #if else ENABLE_LIBSYSTEMD_DAEMON */


#endif /* WATCHDOG_H_ */
