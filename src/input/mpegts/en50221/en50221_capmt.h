/*
 *  Tvheadend - CI CAM (EN50221) CAPMT interface
 *  Copyright (C) 2017 Jaroslav Kysela
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

#ifndef __EN50221_CAPMT_H__
#define __EN50221_CAPMT_H__

#include "service.h"

struct mpegts_service;

#define EN50221_CAPMT_BUILD_DELETE       0
#define EN50221_CAPMT_BUILD_ONLY         1
#define EN50221_CAPMT_BUILD_ADD          2
#define EN50221_CAPMT_BUILD_UPDATE       4

int en50221_capmt_build
  (struct mpegts_service *s,
   int bcmd, uint16_t svcid, const uint16_t *caids, int caids_count,
   const uint8_t *pmt, size_t pmtlen, uint8_t **capmt, size_t *capmtlen);

int en50221_capmt_build_query(const uint8_t *capmt, size_t capmtlen,
                              uint8_t **dst, size_t *dstlen);

void en50221_capmt_dump
  (int subsys, const char *prefix, const uint8_t *capmt, size_t capmtlen);

#endif /* EN50221_CAPMT_H */
