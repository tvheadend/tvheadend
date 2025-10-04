/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * API - Timeshift related calls
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "timeshift.h"

#if ENABLE_TIMESHIFT

void
api_timeshift_init ( void )
{
  static api_hook_t ah[] = {
    { "timeshift/config/load", ACCESS_ADMIN, api_idnode_load_simple, &timeshift_conf },
    { "timeshift/config/save", ACCESS_ADMIN, api_idnode_save_simple, &timeshift_conf },
    { NULL },
  };

  api_register_all(ah);
}

#else

void
api_timeshift_init ( void )
{
}

#endif
