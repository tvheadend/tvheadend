/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Adam Sutton
 *
 * Tvheadend - Network Scanner
 */

#ifndef __TVH_MPEGTS_NETWORK_SCAN_H__
#define __TVH_MPEGTS_NETWORK_SCAN_H__

#include "tvheadend.h"
#include "cron.h"
#include "idnode.h"
#include "subscriptions.h"

/*
 * Timer callback (only to be used in network init)
 */
void mpegts_network_scan_timer_cb ( void *p );

/*
 * Registration functions
 */
void mpegts_network_scan_queue_add ( mpegts_mux_t *mm, int weight,
                                     int flags, int delay );
void mpegts_network_scan_queue_del ( mpegts_mux_t *mm );

/*
 * Events
 */
void mpegts_network_scan_mux_fail    ( mpegts_mux_t *mm );
void mpegts_network_scan_mux_done    ( mpegts_mux_t *mm );
void mpegts_network_scan_mux_partial ( mpegts_mux_t *mm );
void mpegts_network_scan_mux_cancel  ( mpegts_mux_t *mm, int reinsert );
void mpegts_network_scan_mux_active  ( mpegts_mux_t *mm );
void mpegts_network_scan_mux_reactivate ( mpegts_mux_t *mm );

/*
 * Init / Teardown
 */
void mpegts_network_scan_init ( void );
void mpegts_network_scan_done ( void );

#endif /* __TVH_MPEGTS_NETWORK_SCAN_H__*/

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
