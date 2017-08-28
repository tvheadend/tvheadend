/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2016 Tvheadend
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


#ifndef TVH_TRANSCODING_MEMUTILS_H__
#define TVH_TRANSCODING_MEMUTILS_H__


#include "tvheadend.h"
#include "streaming.h"


#define TVH_INPUT_BUFFER_MAX_SIZE (INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE)


#define TVHPKT_INCREF pkt_ref_inc

#define TVHPKT_DECREF pkt_ref_dec

#define TVHPKT_CLEAR(ptr) \
    do { \
        th_pkt_t *_tmp = (ptr); \
        if (_tmp != NULL) { \
            (ptr) = NULL; \
            TVHPKT_DECREF(_tmp); \
        } \
    } while (0)

#define TVHPKT_SET(ptr, pkt) \
    do { \
        TVHPKT_INCREF((pkt)); \
        TVHPKT_CLEAR((ptr)); \
        (ptr) = (pkt); \
    } while (0)


#define str_clear(str) \
    do { \
        if ((str) != NULL) { \
            free((str)); \
            (str) = NULL; \
        } \
    } while (0)

char *
str_join(const char *separator, ...);

int
str_snprintf(char *str, size_t size, const char *format, ...);

/* _IMPORTANT!_: need to check for pb->pb_size and pb->pb_data
   _BEFORE_ calling pktbuf_copy_data */
uint8_t *
pktbuf_copy_data(pktbuf_t *pb);

#endif // TVH_TRANSCODING_MEMUTILS_H__
