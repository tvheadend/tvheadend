/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2017 Jaroslav Kysela
 *
 * Tvheadend - CI CAM (EN50221) CAPMT interface
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
