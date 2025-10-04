/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Andreas Öman
 *
 * IPTV Input
 */

#ifndef __IPTV_H__
#define __IPTV_H__

void iptv_bouquet_trigger_by_uuid(const char *uuid);

void iptv_init ( void );
void iptv_done ( void );

#endif /* __IPTV_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
