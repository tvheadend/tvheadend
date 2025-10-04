/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * Tvheadend - Linux DVB input system
 */

#include "tvheadend.h"
#include "input.h"
#include "settings.h"
#include "linuxdvb_private.h"

int linuxdvb_adapter_mask;

void linuxdvb_init ( int adapter_mask )
{
  linuxdvb_adapter_mask = adapter_mask;

  /* Initialise en50494 locks */
  linuxdvb_en50494_init();

  /* Initialsie devices */
  linuxdvb_adapter_init();
}

void linuxdvb_done ( void )
{
  linuxdvb_adapter_done();
}
