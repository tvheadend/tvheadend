/*
 *  tvheadend - CSA wrapper
 *  Copyright (C) 2013 Adam Sutton
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
