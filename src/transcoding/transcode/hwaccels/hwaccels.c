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

#include "hwaccels.h"
#include "../internals.h"

#if ENABLE_VAAPI
#include "vaapi.h"
#endif

#include <libavutil/pixdesc.h>


/* decoding ================================================================= */

#if LIBAVCODEC_VERSION_MAJOR < 58
/* lifted from libavcodec/utils.c */
static AVHWAccel *
find_hwaccel(enum AVCodecID codec_id, enum AVPixelFormat pix_fmt)
{
    AVHWAccel *hwaccel = NULL;
    while ((hwaccel = av_hwaccel_next(hwaccel))) {
        if (hwaccel->id == codec_id && hwaccel->pix_fmt == pix_fmt) {
            return hwaccel;
        }
    }
    return NULL;
}
static inline int check_pix_fmt(AVCodecContext *avctx, enum AVPixelFormat pix_fmt)
{
    return find_hwaccel(avctx->codec_id, pix_fmt) == NULL;
}
#else
static const AVCodecHWConfig *
find_hwconfig(const AVCodec *codec, enum AVPixelFormat pix_fmt)
{
    const AVCodecHWConfig *hwcfg = NULL;
    int i;

    for (i = 0;; i++) {
        hwcfg = avcodec_get_hw_config(codec, i);
        if (!hwcfg)
            break;
        if (hwcfg->pix_fmt == pix_fmt)
            return hwcfg;
    }
    return NULL;
}
static inline int check_pix_fmt(AVCodecContext *avctx, enum AVPixelFormat pix_fmt)
{
    return find_hwconfig(avctx->codec, pix_fmt) == NULL;
}
#endif

static int
hwaccels_decode_setup_context(AVCodecContext *avctx,
                              const enum AVPixelFormat pix_fmt)
{
    const AVPixFmtDescriptor *desc;

    if (check_pix_fmt(avctx, pix_fmt)) {
        desc = av_pix_fmt_desc_get(pix_fmt);
        tvherror(LS_TRANSCODE, "no HWAccel for the pixel format '%s'", desc ? desc->name : "<unk>");
        return AVERROR(ENOENT);
    }
    switch (pix_fmt) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
            return vaapi_decode_setup_context(avctx);
#endif
        default:
            break;
    }
    return -1;
}


enum AVPixelFormat
hwaccels_decode_get_format(AVCodecContext *avctx,
                           const enum AVPixelFormat *pix_fmts)
{
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    const AVPixFmtDescriptor *desc;
    int i;

    for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        pix_fmt = pix_fmts[i];
        if ((desc = av_pix_fmt_desc_get(pix_fmt))) {
            tvhtrace(LS_TRANSCODE, "hwaccels: [%s] trying pix_fmt: %s", avctx->codec->name, desc->name);
            if ((desc->flags & AV_PIX_FMT_FLAG_HWACCEL) &&
                !hwaccels_decode_setup_context(avctx, pix_fmt)) {
                break;
            }
        }
    }
    return pix_fmt;
}


void
hwaccels_decode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;

    if (ctx->hw_accel_ictx) {
        switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                vaapi_decode_close_context(avctx);
                break;
#endif
            default:
                break;
        }
        ctx->hw_accel_ictx = NULL;
    }
}


int
hwaccels_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx,
                          char *filter, size_t filter_len)
{
    TVHContext *ctx = iavctx->opaque;

    if (ctx->hw_accel_ictx) {
        switch (iavctx->pix_fmt) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_scale_filter(iavctx, oavctx, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}


int
hwaccels_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
{
    TVHContext *ctx = avctx->opaque;

    if (ctx->hw_accel_ictx) {
        switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_deint_filter(avctx, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}

int
hwaccels_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    TVHContext *ctx = avctx->opaque;

    if (ctx->hw_accel_ictx) {
        switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_denoise_filter(avctx, value, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}

int
hwaccels_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    TVHContext *ctx = avctx->opaque;

    if (ctx->hw_accel_ictx) {
        switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
            case AV_PIX_FMT_VAAPI:
                return vaapi_get_sharpness_filter(avctx, value, filter, filter_len);
#endif
            default:
                break;
        }
    }
    
    return -1;
}

/* encoding ================================================================= */

#if ENABLE_FFMPEG4_TRANSCODING
int
hwaccels_initialize_encoder_from_decoder(const AVCodecContext *iavctx, AVCodecContext *oavctx)
{
    switch (iavctx->pix_fmt) {
        case AV_PIX_FMT_VAAPI:
            /* we need to ref hw_frames_ctx of decoder to initialize encoder's codec.
            Only after we get a decoded frame, can we obtain its hw_frames_ctx */
            oavctx->hw_frames_ctx = av_buffer_ref(iavctx->hw_frames_ctx);
            if (!oavctx->hw_frames_ctx) {
                return AVERROR(ENOMEM);
            }
            return 0;
        case AV_PIX_FMT_YUV420P:
            break;
        default:
            break;
    }
    return 0;
}
#endif

int
#if ENABLE_FFMPEG4_TRANSCODING
hwaccels_encode_setup_context(AVCodecContext *avctx)
#else
hwaccels_encode_setup_context(AVCodecContext *avctx, int low_power)
#endif
{
    switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
#if ENABLE_FFMPEG4_TRANSCODING
            return vaapi_encode_setup_context(avctx);
#else
            return vaapi_encode_setup_context(avctx, low_power);
#endif
#endif
        default:
            break;
    }
    return 0;
}


void
hwaccels_encode_close_context(AVCodecContext *avctx)
{
    switch (avctx->pix_fmt) {
#if ENABLE_VAAPI
        case AV_PIX_FMT_VAAPI:
            vaapi_encode_close_context(avctx);
            break;
#endif
        default:
            break;
    }
}


/* module =================================================================== */

void
hwaccels_init(void)
{
}


void
hwaccels_done(void)
{
#if ENABLE_VAAPI
    vaapi_done();
#endif
}
