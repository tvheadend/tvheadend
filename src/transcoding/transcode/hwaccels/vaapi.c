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

#include "transcoding/codec/internals.h"
#include "../internals.h"
#include "vaapi.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/pixdesc.h>
#include <va/va.h>

#define ver2

typedef struct tvh_vaapi_context_t {
    AVBufferRef *hw_device_ref;
} TVHVAContext;


/* TVHVAContext ============================================================= */

static void
tvh_va_context_destroy(TVHVAContext *self)
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
#ifdef ver2
#else
// lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 41
static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to create VAAPI frame context.");
        return AVERROR(ENOMEM);
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to initialize VAAPI frame context."
                "Error code: %s",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        tvherror_transcode(LST_VAAPI, "Decode: Failed to create a hardware frame context."
                "Error code: %s",av_err2str(err));
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}
#endif

static int vaapi_get_scale_filter(AVCodecContext *iavctx, int height, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "scale_vaapi=w=-2:h=%d", height)){
        return -1;
    }
    return 0;
}

static int vaapi_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
{
    const TVHContext *ctx = avctx->opaque;

    // Map user selected rate (0=frame,1=field) to VAAPI rate (1=frame,2=field)
    int rate = (((TVHVideoCodecProfile *)ctx->profile)->deinterlace_field_rate == 1) ? 2 : 1;
    int enable_auto = (((TVHVideoCodecProfile *)ctx->profile)->deinterlace_enable_auto == 1) ? 1 : 0;
    int mode = ((TVHVideoCodecProfile *)ctx->profile)->deinterlace_vaapi_mode;
    mode = (mode < 0 || mode > 4) ? 0 : mode;  // check we have valid deinterlace_vaapi mode

    if (str_snprintf(filter, filter_len, "deinterlace_vaapi=mode=%d:rate=%d:auto=%d",
                                         mode, rate, enable_auto)) {
        return -1;
    }
    return 0;
}

static int vaapi_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "denoise_vaapi=%d", value)){
        return -1;
    }
    return 0;
}

static int vaapi_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "sharpness_vaapi=%d", value)){
        return -1;
    }
    return 0;
}


/* decoding ================================================================= */

int
vaapi_decode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *self = NULL;
    int ret = -1;

    // clean up required if you call decode twice
    if (ctx->hw_accel_ictx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        av_buffer_unref(&avctx->hw_frames_ctx);
        vaapi_decode_destroy(ctx);
    }

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to allocate VAAPI context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 237
    // Open VAAPI device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_VAAPI, ctx->hw_accel_device, NULL, 0)) < 0) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to Open VAAPI device and create an AVHWDeviceContext for device: "
                            "%s with error code: %s", 
                            ctx->hw_accel_device, av_err2str(ret));
        tvh_va_context_destroy(self);
        return ret;
    }

    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 95
    // set hw_device_ctx for decoder's AVCodecContext
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ref);
    if (!avctx->hw_device_ctx) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to create a hardware device reference for device: %s.", 
                        ctx->hw_accel_device);
        tvh_va_context_destroy(self);
        return AVERROR(ENOMEM);
    }

#ifdef ver2
    // 2. CRITICAL: Create the Frames Context
    if (!avctx->hw_frames_ctx) {
        AVBufferRef *frames_ref = av_hwframe_ctx_alloc(self->hw_device_ref);
        if (!frames_ref) {
            ret = AVERROR(ENOMEM);
            tvherror_transcode(LST_VAAPI, "Decode: Failed to initialize HW Frames "
                            "with error code: %s", av_err2str(ret));
            av_buffer_unref(&avctx->hw_device_ctx);
            tvh_va_context_destroy(self);
            return ret;
        }
        int width = avctx->coded_width;
        int height = avctx->coded_height;
        // check if coded dimensions is populated
        if (width == 0 || height == 0){
            width = avctx->width;
            height = avctx->height;
        }

        AVHWFramesContext *frames_ctx = (AVHWFramesContext *)frames_ref->data;
        frames_ctx->format    = AV_PIX_FMT_VAAPI;
        frames_ctx->sw_format = AV_PIX_FMT_NV12;
        // Hardware dimensions MUST be aligned (usually 32 for VAAPI)
        // TODO: we should make this adjustable for Intel Gen12+ (Xe) sometime is required 128 or 256
        frames_ctx->width     = FFALIGN(width,  32);
        frames_ctx->height    = FFALIGN(height, 32);
        frames_ctx->initial_pool_size = 32; // Pre-allocate 32 surfaces

        ret = av_hwframe_ctx_init(frames_ref);
        if (ret < 0) {
            tvherror_transcode(LST_VAAPI, "Decode: Failed to initialize HW Frames "
                            "with error code: %s", av_err2str(ret));
            av_buffer_unref(&avctx->hw_device_ctx);
            av_buffer_unref(&frames_ref);
            tvh_va_context_destroy(self);
            return ret;
        }

        // Attach the pool to the decoder
        avctx->hw_frames_ctx = frames_ref;
    }

    
#else
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 152
    /* set hw_frames_ctx for decoder's AVCodecContext */
    if ((ret = set_hwframe_ctx(avctx, avctx->hw_device_ctx)) < 0) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to set hwframe context."
                                      "Error code: %s", av_err2str(ret));
        av_buffer_unref(&avctx->hw_device_ctx);
        tvh_va_context_destroy(self);
        return ret;
    }
#endif
    
    ctx->hw_accel_ictx = self;
    avctx->pix_fmt = AV_PIX_FMT_VAAPI;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    return 0;
}


/**
 * @brief Constructs a VAAPI hardware video filter chain string.
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
vaapi_get_filters(TVHContext *self, char *filter, size_t filter_len)    
{
    // NOTE: filter_len > sum of all string --> previously used strcat()
    // Now using snprintf for guaranteed buffer safety
    char hw_deint[64];
    char hw_scale[64];
    char hw_denoise[64];
    char hw_sharpness[64];
    size_t len=0;

    int height = ((TVHVideoCodecProfile *)self->profile)->size.den;

    int filter_scale = (self->iavctx->height != height);
    int filter_deint = ((TVHVideoCodecProfile *)self->profile)->deinterlace;
    int filter_denoise = ((TVHVideoCodecProfile *)self->profile)->filter_hw_denoise;
    int filter_sharpness = ((TVHVideoCodecProfile *)self->profile)->filter_hw_sharpness;

    memset(hw_deint, 0, sizeof(hw_deint));
    if (filter_deint){
        if (vaapi_get_deint_filter(self->iavctx, hw_deint, sizeof(hw_deint))){
            tvherror_transcode(LST_VAAPI, "Filter: vaapi_get_deint_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_deint);
        }
    }
    
    memset(hw_scale, 0, sizeof(hw_scale));
    if (filter_scale){ 
        if(vaapi_get_scale_filter(self->iavctx, height, hw_scale, sizeof(hw_scale))){
            tvherror_transcode(LST_VAAPI, "Filter: vaapi_get_scale_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_scale);
        }
    }
    
    memset(hw_denoise, 0, sizeof(hw_denoise));
    if (filter_denoise) { 
        if (vaapi_get_denoise_filter(self->iavctx, filter_denoise, hw_denoise, sizeof(hw_denoise))){
            tvherror_transcode(LST_VAAPI, "Filter: vaapi_get_denoise_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_denoise);
        }
    }

    memset(hw_sharpness, 0, sizeof(hw_sharpness));
    if (filter_sharpness){
        if (vaapi_get_sharpness_filter(self->iavctx, filter_sharpness, hw_sharpness, sizeof(hw_sharpness))){
            tvherror_transcode(LST_VAAPI, "Filter: vaapi_get_sharpness_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(filter, filter_len);
            snprintf(filter + len, filter_len - len, "%s%s", (len > 0) ? "," : "", hw_sharpness);
        }
    }
    return 0;
}

int
vaapi_get_download(TVHContext *self, int skip_format, char *filter, size_t filter_len)
{
    if (skip_format) {
        if (str_snprintf(filter, filter_len, "hwdownload")) {
            return -1;
        }
    }
    else {
        if (str_snprintf(filter, filter_len, "hwdownload,format=pix_fmts=%s",
                        av_get_pix_fmt_name(self->oavctx->sw_pix_fmt))) {
            return -1;
        }
    }
    return 0;
}

/* encoding ================================================================= */

int
vaapi_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    int ret = 0;

    // this is required in case encode is called twice
    if (ctx->hw_device_octx)
        av_buffer_unref(&ctx->hw_device_octx);
    // 1. Create Device Context
    // Open VAAPI device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0)) < 0) {
        tvherror_transcode(LST_VAAPI, "Encode: Failed to open VAAPI device and create an AVHWDeviceContext for it."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    avctx->hw_device_ctx = av_buffer_ref(ctx->hw_device_octx);
    if (!avctx->hw_device_ctx) {
        av_buffer_unref(&ctx->hw_device_octx);
        ctx->hw_device_octx = NULL;
        ret = AVERROR(ENOMEM);
        tvherror_transcode(LST_VAAPI, "Encode: Failed to create a hardware device context."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    
    // 2. CRITICAL: Frames Context are created in tvh_context_open_filters2()

    avctx->pix_fmt = AV_PIX_FMT_VAAPI;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    return ret;
}


/* module =================================================================== */

void
vaapi_decode_destroy(TVHContext *ctx)
{
    if (!ctx)
        return;
    tvh_va_context_destroy(ctx->hw_accel_ictx);
    ctx->hw_accel_ictx = NULL;
}

void
vaapi_done()
{
    /* nothing to do */
}
