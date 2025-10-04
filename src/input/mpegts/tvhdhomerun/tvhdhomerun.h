/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Patric Karlström
 *
 * Tvheadend - HDHomeRun DVB private data
 */

#ifndef __TVH_tvhdhomerun_H__
#define __TVH_tvhdhomerun_H__

extern const idclass_t tvhdhomerun_device_class;
extern const idclass_t tvhdhomerun_frontend_dvbt_class;
extern const idclass_t tvhdhomerun_frontend_dvbc_class;
extern const idclass_t tvhdhomerun_frontend_atsc_t_class;
extern const idclass_t tvhdhomerun_frontend_atsc_c_class;
extern const idclass_t tvhdhomerun_frontend_cablecard_class;
extern const idclass_t tvhdhomerun_frontend_isdbt_class;

void tvhdhomerun_init( void );
void tvhdhomerun_done( void );

#endif /* __TVH_SATIP_H__ */
