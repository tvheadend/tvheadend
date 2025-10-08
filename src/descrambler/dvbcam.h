/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * tvheadend - CSA wrapper
 */

#ifndef __DVBCAM_H__
#define __DVBCAM_H__

struct service;
struct mpegts_service;
struct elementary_stream;

#if ENABLE_LINUXDVB_CA

struct linuxdvb_ca;

void dvbcam_init(void);
void dvbcam_register_cam(struct linuxdvb_ca *lca, uint16_t * caids, int num_caids);
void dvbcam_unregister_cam(struct linuxdvb_ca *lca);
void dvbcam_pmt_data(struct mpegts_service *s, const uint8_t *ptr, int len);

#endif

#if ENABLE_LINUXDVB_CA && ENABLE_DDCI
int dvbcam_is_ddci(struct service *t);
#else
static inline int dvbcam_is_ddci(struct service *t) { return 0; }
#endif

#endif /* __DVBCAM_H__ */
