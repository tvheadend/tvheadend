/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * TVheadend - time processing
 */

#ifndef __TVH_TIME_H__
#define __TVH_TIME_H__

#include "idnode.h"

void tvhtime_init ( void );
void tvhtime_update ( time_t utc, const char *srcname );

#endif /* __TVH_TIME_H__ */
