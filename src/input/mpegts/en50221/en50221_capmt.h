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

/* Optional PID/SID remap hooks, applied to the CA-PMT's own PID and SID
 * fields as they're written - not by post-processing the finished
 * buffer with a second parser, which risks the two layouts diverging as
 * either side changes. Pass NULL for either (or both) to get the
 * original, unmapped behaviour byte-for-byte - every existing caller
 * that has no need to remap anything keeps working unchanged. Added for
 * DDCI's MTD PID/SID remap (see linuxdvb_ddci.c / dvbcam.c) - a service
 * whose CI slot shares real PIDs/SIDs with another real transponder
 * needs its CA-PMT to speak the same remapped numbers the actual TS
 * packets going to the CAM use. */
enum capmt_pid_map_kind {
  CAPMT_PID_MAP_CA = 1,
  CAPMT_PID_MAP_ES = 2
};
typedef uint16_t (*capmt_pid_mapper_t)(void *opaque, uint16_t pid,
                                       enum capmt_pid_map_kind kind);
typedef uint16_t (*capmt_sid_mapper_t)(void *opaque, uint16_t sid);

int en50221_capmt_build
  (struct mpegts_service *s,
   int bcmd, uint16_t svcid, const uint16_t *caids, int caids_count,
   const uint8_t *pmt, size_t pmtlen, uint8_t **capmt, size_t *capmtlen,
   capmt_pid_mapper_t pmap, capmt_sid_mapper_t smap, void *opaque);

int en50221_capmt_build_query(const uint8_t *capmt, size_t capmtlen,
                              uint8_t **dst, size_t *dstlen);

void en50221_capmt_dump
  (int subsys, const char *prefix, const uint8_t *capmt, size_t capmtlen);

#endif /* EN50221_CAPMT_H */
