/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
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
#if ENABLE_FFMPEG4_TRANSCODING
int
hwaccels_initialize_encoder_from_decoder(const AVCodecContext *iavctx, AVCodecContext *oavctx);
#endif

int
#if ENABLE_FFMPEG4_TRANSCODING
hwaccels_encode_setup_context(AVCodecContext *avctx);
#else
hwaccels_encode_setup_context(AVCodecContext *avctx, int low_power);
#endif

void
hwaccels_encode_close_context(AVCodecContext *avctx);


/* module =================================================================== */

void
hwaccels_init(void);

void
hwaccels_done(void);


#endif // TVH_TRANSCODING_TRANSCODE_HWACCELS_H__
