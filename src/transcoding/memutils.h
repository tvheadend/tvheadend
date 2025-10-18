/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
 */

#ifndef TVH_TRANSCODING_MEMUTILS_H__
#define TVH_TRANSCODING_MEMUTILS_H__


#include "tvheadend.h"
#include "streaming.h"


#define TVH_INPUT_BUFFER_MAX_SIZE (INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE)

#define TVHPKT_CLEAR(ptr) \
    do { \
        th_pkt_t *_tmp = (ptr); \
        if (_tmp != NULL) { \
            (ptr) = NULL; \
            pkt_ref_dec(_tmp); \
        } \
    } while (0)

#define TVHPKT_SET(ptr, pkt) \
    do { \
        pkt_ref_inc((pkt)); \
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
