/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * Tvheadend - FastScan support
 */

#ifndef __TVH_DVB_FASTSCAN_H__
#define __TVH_DVB_FASTSCAN_H__

#if ENABLE_MPEGTS_DVB

struct bouquet;

void
dvb_fastscan_each(void *aux, int position, uint32_t frequency, dvb_polarisation_t polarisation,
                  void (*job)(void *aux, struct bouquet *,
                              const char *name, int pid));

#endif

void dvb_fastscan_init ( void );
void dvb_fastscan_done ( void );

#endif /* TVH_DVB_FASTSCAN_H */
