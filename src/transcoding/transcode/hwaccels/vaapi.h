/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
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
