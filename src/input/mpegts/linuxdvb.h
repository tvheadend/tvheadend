/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * Tvheadend - Linux DVB input system
 */

#ifndef __TVH_LINUX_DVB_H__
#define __TVH_LINUX_DVB_H__

void linuxdvb_init ( int mask );

void linuxdvb_done ( void );

idnode_set_t *linuxdvb_root ( void );

#endif /* __TVH_LINUX_DVB_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
