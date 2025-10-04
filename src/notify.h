/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * tvheadend, Notification framework
 */

#ifndef NOTIFY_H_
#define NOTIFY_H_

#include "htsmsg.h"

#define NOTIFY_REWRITE_TITLE         1
#define NOTIFY_REWRITE_SUBSCRIPTIONS 2

void notify_by_msg(const char *class, htsmsg_t *m, int isrestricted, int rewrite);

void notify_reload(const char *class);

void notify_delayed(const char *id, const char *event, const char *action);

void notify_init(void);
void notify_done(void);

#endif /* NOTIFY_H_ */
