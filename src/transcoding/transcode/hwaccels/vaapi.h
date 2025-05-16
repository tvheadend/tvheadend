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


#ifndef TVH_TRANSCODING_TRANSCODE_HWACCELS_VAAPI_H__
#define TVH_TRANSCODING_TRANSCODE_HWACCELS_VAAPI_H__


#include "tvheadend.h"

#include <libavcodec/avcodec.h>


/* decoding ================================================================= */

int
vaapi_decode_setup_context(AVCodecContext *avctx);

void
vaapi_decode_close_context(AVCodecContext *avctx);

int
vaapi_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx, char *filter, size_t filter_len);

int
vaapi_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len);

int
vaapi_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len);

int
vaapi_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len);


/* encoding ================================================================= */

int
#if ENABLE_FFMPEG4_TRANSCODING
vaapi_encode_setup_context(AVCodecContext *avctx);
#else
vaapi_encode_setup_context(AVCodecContext *avctx, int low_power);
#endif

void
vaapi_encode_close_context(AVCodecContext *avctx);


/* module =================================================================== */

void
vaapi_done(void);


#endif // TVH_TRANSCODING_TRANSCODE_HWACCELS_VAAPI_H__
