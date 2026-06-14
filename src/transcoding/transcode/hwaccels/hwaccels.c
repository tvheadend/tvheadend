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

#if ENABLE_NVENC
#include "nv.h"
#endif

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
    TVHContext *ctx = avctx->opaque;

    if (check_pix_fmt(avctx, pix_fmt)) {
        desc = av_pix_fmt_desc_get(pix_fmt);
        tvherror(LS_TRANSCODE, "no HWAccel for the pixel format '%s'", desc ? desc->name : "<unk>");
        return AVERROR(ENOENT);
    }
    switch (ctx->iavhwdevtype) {
#if ENABLE_NVENC
        case AV_HWDEVICE_TYPE_CUDA:
            return nv_decode_setup_context(avctx);
#endif
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            return vaapi_decode_setup_context(avctx);
#endif
        default:
            break;
    }
    return -1;
}

static int is_pix_fmt_in_list(const enum AVPixelFormat check_pix_fmt, const enum AVPixelFormat *pix_fmts) {
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    int i;
    for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        pix_fmt = pix_fmts[i];
        if (check_pix_fmt == pix_fmt)
            return 1;
    }
    return 0;
}

/**
 * @brief Negotiates and selects the optimal pixel format for decoding.
 *
 * This function is used as the 'get_format' callback for the FFmpeg decoder context. 
 * Its primary goal is to select a hardware-accelerated pixel format if available 
 * and supported by the current configuration.
 *
 * The selection process follows two distinct logic paths based on the profile:
 * 1. **Modern API Path (New Implementation)**: If the profile supports 'filter2', 
 * it iterates through the codec's hardware configurations. It looks for a 
 * match between the required hardware device type and a configuration using 
 * a hardware device context. If a match is found and the context is 
 * successfully set up, the hardware pixel format is returned.
 * 2. **Legacy Path (Old Implementation)**: If the modern path is unavailable or 
 * not supported, it iterates through the provided list of candidate pixel 
 * formats looking for any format marked with the `AV_PIX_FMT_FLAG_HWACCEL` 
 * flag. It attempts to initialize the hardware context for the first valid 
 * match found.
 *
 * If no hardware acceleration path is successful, the function falls back to the 
 * first available software format provided in the `pix_fmts` list.
 *
 * @param avctx    The codec context being initialized for decoding.
 * @param pix_fmts A null-terminated list of possible pixel formats acceptable 
 * by the decoder.
 *
 * @return The selected AVPixelFormat. Returns a hardware pixel format if 
 * acceleration setup is successful; otherwise, returns the software 
 * fallback format.
 */
enum AVPixelFormat
hwaccels_decode_get_format(AVCodecContext *avctx,
                           const enum AVPixelFormat *pix_fmts)
{
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
    const AVPixFmtDescriptor *desc;
    int i;

    TVHContext *ctx = avctx->opaque;
    if ((ctx->profile)->has_support_for_filter2) {
        // new implementation
        // Guard: delay HW frames context init until dimensions known.
        // MPEG2 (and others) can call get_format() before sequence header -> width/height still 0.
        if ((avctx->width <= 0 || avctx->height <= 0) &&
            (avctx->coded_width <= 0 || avctx->coded_height <= 0)) {
            // Pick first non-hwaccel format as a safe SW fallback for now.
            for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
                const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(pix_fmts[i]);
                if (!d) continue;
                if (!(d->flags & AV_PIX_FMT_FLAG_HWACCEL))
                    return pix_fmts[i];
            }
            return pix_fmts[0]; // last-resort
        }
        // 1. Iterate through all HW configs supported by this codec
        for (i = 0; ; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(avctx->codec, i);
            if (!config) break;
            // 2. If the current format in the list matches a format in a 
            //    Hardware Device Context config, it's a hardware path
            //    that is matching with input hw device type
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == ctx->iavhwdevtype && 
                is_pix_fmt_in_list(config->pix_fmt, pix_fmts) &&
                !hwaccels_decode_setup_context(avctx, config->pix_fmt)) {
                // Because it came from a HW_DEVICE_CTX config, we know 
                // it's the hardware-accelerated path.
                if (!avctx->hw_frames_ctx) {
                    // hw_frames_ctx is not initialized most likely will fail
                    tvhwarn(LS_LIBAV,  "Decoder hw_frames_ctx is not available for pix_fmt = %s", av_get_pix_fmt_name(config->pix_fmt));
                }
                tvhtrace(LS_TRANSCODE, "hwaccels: [%s] trying pix_fmt: %s", avctx->codec->name, av_get_pix_fmt_name(config->pix_fmt));
                // return pix_fmt
                return config->pix_fmt; 
            }
        }
    }
    // old implementation
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


int
hwaccels_decode_get_filters(TVHContext *self, char *filter, size_t filter_len)    
{

    switch (self->iavhwdevtype) {
#if ENABLE_NVENC
        case AV_HWDEVICE_TYPE_CUDA:
            return nv_get_filters(self, filter, filter_len);
#endif
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            return vaapi_get_filters(self, filter, filter_len);
#endif
        default:
            break;
    }
    
    return 0;
}


int
hwaccels_download(TVHContext *self, char *filter, size_t filter_len, int skip_format)    
{

    switch (self->iavhwdevtype) {
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            // VAAPI will use default : 
            // NOTE: this must be left last
#endif
        default:
            if (skip_format) {
                if (str_snprintf(filter, filter_len, "hwdownload")) {
                    return -1;
                }
            }
            else {
                if (str_snprintf(filter, filter_len, "hwdownload,format=pix_fmts=%s",
                                av_get_pix_fmt_name(self->iavctx->sw_pix_fmt))) {
                    return -1;
                }
            }
            break;
    }
    return 0;
}


int
hwaccels_upload(TVHContext *self, char *filter, size_t filter_len)    
{

    switch (self->oavhwdevtype) {
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            // VAAPI will use default : 
            // NOTE: this must be left last
#endif
        default:
            if (str_snprintf(filter, filter_len, "format=pix_fmts=%s,hwupload",  // =extra_hw_frames=16
                            av_get_pix_fmt_name(self->oavctx->sw_pix_fmt))) {
                return -1;
            }
            break;
    }
    
    return 0;
}


/* encoding ================================================================= */

int
hwaccels_encode_setup_context(TVHContext *self)
{
    switch (self->oavhwdevtype) {
#if ENABLE_NVENC
        case AV_HWDEVICE_TYPE_CUDA:
            return nv_encode_setup_context(self->oavctx);
#endif
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            return vaapi_encode_setup_context(self->oavctx);
#endif
        default:
            break;
    }
    return 0;
}


void
hwaccels_context_destroy(TVHContext *self)
{
    if (!self)
        return;
    switch (self->iavhwdevtype) {
#if ENABLE_NVENC
        case AV_HWDEVICE_TYPE_CUDA:
            nv_decode_destroy(self);
            break;
#endif
#if ENABLE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI:
            vaapi_decode_destroy(self);
            break;
#endif
        default:
            break;
    }
    if (self->hw_device_octx)
        av_buffer_unref(&self->hw_device_octx);
}


/* module =================================================================== */

void
hwaccels_init(void)
{
}


void
hwaccels_done(void)
{
#if ENABLE_NVENC
    nv_done();
#endif
#if ENABLE_VAAPI
    vaapi_done();
#endif
}
