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

void
hwaccels_decode_close_context(AVCodecContext *avctx);

int
hwaccels_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx,
                          char *filter, size_t filter_len);

int
hwaccels_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len);

int
hwaccels_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len);

int
hwaccels_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len);


/* encoding ================================================================= */
int
hwaccels_initialize_encoder_from_decoder(const AVCodecContext *iavctx, AVCodecContext *oavctx);

int
hwaccels_encode_setup_context(AVCodecContext *avctx);

void
hwaccels_encode_close_context(AVCodecContext *avctx);


/* module =================================================================== */

void
hwaccels_init(void);

void
hwaccels_done(void);


#endif // TVH_TRANSCODING_TRANSCODE_HWACCELS_H__
