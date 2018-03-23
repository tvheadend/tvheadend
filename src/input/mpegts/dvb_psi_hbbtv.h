/*
 *  MPEG TS Program Specific Information code
 *  Copyright (C) 2007 - 2010 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
