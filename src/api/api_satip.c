/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * API - SAT>IP Server related calls
 */

#include "tvheadend.h"
#include "channels.h"
#include "access.h"
#include "api.h"
#include "satip/server.h"

#if ENABLE_SATIP_SERVER

void
api_satip_server_init ( void )
{
  static api_hook_t ah[] = {
    { "satips/config/load", ACCESS_ADMIN, api_idnode_load_simple, &satip_server_conf },
    { "satips/config/save", ACCESS_ADMIN, api_idnode_save_simple, &satip_server_conf },
    { NULL },
  };

  api_register_all(ah);
}

#else /* ENABLE_SATIP_SERVER */

void 
api_satip_server_init ( void )
{
}

#endif
