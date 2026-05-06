/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2026 Tvheadend
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

#include "transcoding/codec/internals.h"
#include "../internals.h"
#include "nv.h"

#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>


typedef struct tvh_nv_context_t {
    AVBufferRef *hw_device_ref;
} TVNVContext;


/* TVNVContext ============================================================= */

static void
tvh_nv_context_destroy(TVNVContext *self)
{
    if (self) {
        if (self->hw_device_ref) {
            av_buffer_unref(&self->hw_device_ref);
        }
        free(self);
        self = NULL;
    }
}


/* internal ================================================================= */

// lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 41
static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to create CUDA frame context.");
        return AVERROR(ENOMEM);
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_CUDA;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to initialize CUDA frame context."
                "Error code: %s",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        tvherror_transcode(LST_NVENC, "Decode: Failed to create a hardware frame context."
                "Error code: %s",av_err2str(err));
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}

static int nv_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx,
                       char *filter, size_t filter_len)
{
    // https://ffmpeg.org/ffmpeg-filters.html#scale_005fcuda-1
    if (str_snprintf(filter, filter_len, "scale_cuda=w=%d:h=%d", oavctx->width, oavctx->height)){
        return -1;
    }
    return 0;
}

static int nv_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
{
    const TVHContext *ctx = avctx->opaque;
    // https://ffmpeg.org/ffmpeg-filters.html#yadif_005fcuda
    if (str_snprintf(filter, filter_len, "yadif_cuda=mode=%d:deint=%d",
                     ((TVHVideoCodecProfile *)ctx->profile)->deinterlace_field_rate,
                     ((TVHVideoCodecProfile *)ctx->profile)->deinterlace_enable_auto )) {
        return -1;
    }
    return 0;
}

/* decoding ================================================================= */

int
nv_decode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;

    TVNVContext *self = NULL;
    int ret = -1;

    if (!(self = calloc(1, sizeof(TVNVContext)))) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to allocate CUDA context (TVNVContext)");
        return AVERROR(ENOMEM);
    }
    // Open NV device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_CUDA, ctx->hw_accel_device, NULL, 0)) < 0) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to Open CUDA device and create an AVHWDeviceContext for device: "
                            "%s with error code: %s", 
                            ctx->hw_accel_device, av_err2str(ret));
        free(self);
        self = NULL;
        return ret;
    }
    // set hw_frames_ctx for decoder's AVCodecContext
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ref);
    if (!avctx->hw_device_ctx) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to create a hardware device reference for device: %s.", 
                        ctx->hw_accel_device);
        // unref hw_device_ref
        av_buffer_unref(&self->hw_device_ref);
        self->hw_device_ref = NULL;
        free(self);
        self = NULL;
        return AVERROR(ENOMEM);
    }
    // set hw_frames_ctx for decoder's AVCodecContext
    if ((ret = set_hwframe_ctx(avctx, avctx->hw_device_ctx)) < 0) {
        tvherror_transcode(LST_NVENC, "Decode: Failed to set hwframe context."
                                      "Error code: %s", av_err2str(ret));
        av_buffer_unref(&avctx->hw_device_ctx);
        // unref hw_device_ref
        av_buffer_unref(&self->hw_device_ref);
        self->hw_device_ref = NULL;
        free(self);
        self = NULL;
        return ret;
    }
    if (ctx->hw_accel_ictx) {
        nv_decode_destroy(ctx);
    }
    ctx->hw_accel_ictx = self;
    avctx->pix_fmt = AV_PIX_FMT_CUDA;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    return 0;
}


/**
 * @brief Constructs a NV hardware video filter chain string.
 *
 * This function evaluates the current transcoding profile and input/output
 * contexts to determine which hardware filters (deinterlacing, scaling, 
 * denoising, and sharpness) are required. It then fetches the individual 
 * filter strings and concatenates them into a single, comma-separated 
 * filter string safe for use with FFmpeg/libavcodec.
 *
 * @param self       Pointer to the TVHContext containing the transcoding state,
 * input/output AV contexts, and profile configuration.
 * @param filter     Pointer to the destination buffer where the final comma-separated
 * filter string will be written.
 * @param filter_len The maximum size (in bytes) of the destination filter buffer.
 *
 * @return 0 on success, or -1 if an error occurred while retrieving any of the 
 * individual hardware filter strings.
 */
int
nv_get_filters(TVHContext *self, char *filter, size_t filter_len)    
{
    // NOTE: filter_len > sum of all string --> previously used strcat()
    // Now using snprintf for guaranteed buffer safety
    char hw_deint[64];
    char hw_scale[64];
    size_t len=0;

    int filter_scale = (self->iavctx->height != self->oavctx->height);
    int filter_deint = ((TVHVideoCodecProfile *)self->profile)->deinterlace;

    memset(hw_deint, 0, sizeof(hw_deint));
    if (filter_deint){
        if (nv_get_deint_filter(self->iavctx, hw_deint, sizeof(hw_deint))){
            tvherror_transcode(LST_NVENC, "Filter: nv_get_deint_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_deint);
        }
    }
    
    memset(hw_scale, 0, sizeof(hw_scale));
    if (filter_scale){ 
        if(nv_get_scale_filter(self->iavctx, self->oavctx, hw_scale, sizeof(hw_scale))){
            tvherror_transcode(LST_NVENC, "Filter: nv_get_scale_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_scale);
        }
    }
    return 0;
}


/* encoding ================================================================= */

int
nv_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    int ret = 0;

    // this is required in case encode is called twice
    if (ctx->hw_device_octx)
        av_buffer_unref(&ctx->hw_device_octx);
    // Open NV device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_CUDA, NULL, NULL, 0)) < 0) {
        tvherror_transcode(LST_NVENC, "Encode: Failed to open CUDA device and create an AVHWDeviceContext for it."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    avctx->hw_device_ctx = av_buffer_ref(ctx->hw_device_octx);
    if (!avctx->hw_device_ctx) {
        av_buffer_unref(&ctx->hw_device_octx);
        ctx->hw_device_octx = NULL;
        ret = AVERROR(ENOMEM);
        tvherror_transcode(LST_NVENC, "Encode: Failed to create a hardware device context."
                "Error code: %s",av_err2str(ret));
        return ret;
    }

    avctx->pix_fmt = AV_PIX_FMT_CUDA;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    return ret;
}


/* module =================================================================== */

void
nv_decode_destroy(TVHContext *ctx)
{
    if (!ctx)
        return;
    tvh_nv_context_destroy(ctx->hw_accel_ictx);
    ctx->hw_accel_ictx = NULL;
}

void
nv_done()
{
    /* nothing to do */
}