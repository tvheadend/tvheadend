/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * Tvheadend - TS file input system
 */

#ifndef __TVH_TSFILE_H__
#define __TVH_TSFILE_H__

#include <stdint.h>

struct mpegts_mux;
struct mpegts_network;

/* Initialise system (with N tuners) */
void tsfile_init ( int tuners );

/* Shutdown */
void tsfile_done ( void );

/* Add a new file (multiplex) */
void tsfile_add_file ( const char *path );

#endif /* __TVH_TSFILE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
