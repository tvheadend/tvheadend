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


#ifndef TVH_TRANSCODING_TRANSCODE_HWACCELS_H__
#define TVH_TRANSCODING_TRANSCODE_HWACCELS_H__


#include "tvheadend.h"
#include "../internals.h"
#include <libavcodec/avcodec.h>


/* decoding ================================================================= */

enum AVPixelFormat
hwaccels_decode_get_format(AVCodecContext *avctx,
                           const enum AVPixelFormat *pix_fmts);

int
hwaccels_decode_get_filters(TVHContext *self, char *filter, size_t filter_len);

int
hwaccels_download(TVHContext *self, char *filter, size_t filter_len, int skip_format);

int
hwaccels_upload(TVHContext *self, char *filter, size_t filter_len);


/* encoding ================================================================= */

int
hwaccels_encode_setup_context(TVHContext *self);

/* module =================================================================== */

void
hwaccels_init(void);

void
hwaccels_done(void);

/**
 * Release TVH-owned hardware resources after @c avcodec_free_context has been
 * called on both decoder and encoder contexts (see @c tvh_context_destroy).
 */
void
hwaccels_context_destroy(TVHContext *self);

#endif // TVH_TRANSCODING_TRANSCODE_HWACCELS_H__
