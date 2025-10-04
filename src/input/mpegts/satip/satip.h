/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * Tvheadend - SAT-IP DVB private data
 */

#ifndef __TVH_SATIP_H__
#define __TVH_SATIP_H__

void satip_device_discovery_start( void );

void satip_init( int nosatip, str_list_t *clients );
void satip_done( void );

#endif /* __TVH_SATIP_H__ */
