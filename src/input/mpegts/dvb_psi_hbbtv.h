/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2007 - 2010 Andreas Öman
 *
 * MPEG TS Program Specific Information code
 */

#ifndef __DVB_PSI_HBBTV_H
#define __DVB_PSI_HBBTV_H 1

#include "dvb.h"
#include "htsmsg.h"

struct mpegts_table;

/*
 * HBBTV processing
 */

htsmsg_t *dvb_psi_parse_hbbtv
  (struct mpegts_psi_table *mt, const uint8_t *buf, int len, int *_sect);

void dvb_psi_hbbtv_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len);

int dvb_hbbtv_callback
  (struct mpegts_table *mt, const uint8_t *buf, int len, int tableid);

#endif
