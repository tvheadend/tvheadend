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
#include "qsv.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_qsv.h>
#include <libavutil/pixdesc.h>
#include <va/va.h>

#define ver2

typedef struct tvh_qsv_context_t {
    AVBufferRef *hw_device_ref;
} TVHQSVContext;


/* TVHQSVContext ============================================================= */

static void
tvh_qsv_context_destroy(TVHQSVContext *self)
{
    if (self) {
        if (self->hw_device_ref) {
            av_buffer_unref(&self->hw_device_ref);
            self->hw_device_ref = NULL;
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
        tvherror_transcode(LST_QSV, "Decode: Failed to create QSV frame context.");
        return AVERROR(ENOMEM);
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_QSV;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = FFALIGN(ctx->coded_width,  32);
    frames_ctx->height    = FFALIGN(ctx->coded_height, 32);
    frames_ctx->initial_pool_size = 32; // Pre-allocate 32 surfaces
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror_transcode(LST_QSV, "Decode: Failed to initialize QSV frame context."
                "Error code: %s",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        tvherror_transcode(LST_QSV, "Decode: Failed to create a hardware frame context."
                "Error code: %s",av_err2str(err));
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}
#endif

static int qsv_get_scale_filter(AVCodecContext *iavctx, int height, char *filter, size_t filter_len)
{
    // NOTE: vpp_qsv does not accept -2 (but -1 does the same thing)
    if (str_snprintf(filter, filter_len, "w=-1:h=%d:scale_mode=2", height)){
        return -1;
    }
    return 0;
}

static int qsv_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
{
    const TVHContext *ctx = avctx->opaque;

    int mode = ((TVHVideoCodecProfile *)ctx->profile)->deinterlace_qsv_mode;
    mode = (mode < 0 || mode > 2) ? 0 : mode;  // check we have valid deinterlace_qsv mode

    if (str_snprintf(filter, filter_len, "deinterlace=%d", mode)) {
        return -1;
    }
    return 0;
}


static int qsv_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "denoise=%d", value)){
        return -1;
    }
    return 0;
}

static int qsv_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "detail=%d", value)){
        return -1;
    }
    return 0;
}


/* decoding ================================================================= */

int qsv_decode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    TVHQSVContext *self = NULL;
    int ret = -1;

    // clean up required if you call decode twice
    if (ctx->hw_accel_ictx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        av_buffer_unref(&avctx->hw_frames_ctx);
        qsv_decode_destroy(ctx);
    }

    if (!(self = calloc(1, sizeof(TVHQSVContext)))) {
        tvherror_transcode(LST_QSV, "Decode: Failed to allocate QSV context (TVHQSVContext)");
        return AVERROR(ENOMEM);
    }
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 237
    // 1. Create Device Context
    // Open QSV device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0))) {
        tvherror_transcode(LST_QSV, "Decode: Failed to Open QSV device and create an AVHWDeviceContext "
                            "with error code: %s", av_err2str(ret));
        tvh_qsv_context_destroy(self);
        return ret;
    }

    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 95
    /* set hw_frames_ctx for decoder's AVCodecContext */
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ref);
    if (!avctx->hw_device_ctx) {
        tvherror_transcode(LST_QSV, "Decode: Failed to create a hardware device reference using 'auto'");
        tvh_qsv_context_destroy(self);
        return AVERROR(ENOMEM);
    }

#ifdef ver2
    // 2. CRITICAL: Create the Frames Context
    if (!avctx->hw_frames_ctx) {
        AVBufferRef *frames_ref = av_hwframe_ctx_alloc(self->hw_device_ref);
        if (!frames_ref) {
            ret = AVERROR(ENOMEM);
            tvherror_transcode(LST_QSV, "Decode: Failed to initialize HW Frames "
                            "with error code: %s", av_err2str(ret));
            av_buffer_unref(&avctx->hw_device_ctx);
            tvh_qsv_context_destroy(self);
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
        frames_ctx->format    = AV_PIX_FMT_QSV;
        frames_ctx->sw_format = AV_PIX_FMT_NV12;
        // Hardware dimensions MUST be aligned (usually 32 for QSV)
        // TODO: we should make this adjustable for Intel Gen12+ (Xe) sometime is required 128 or 256
        frames_ctx->width     = FFALIGN(width,  32);
        frames_ctx->height    = FFALIGN(height, 32);
        frames_ctx->initial_pool_size = 32; // Pre-allocate 32 surfaces

        ret = av_hwframe_ctx_init(frames_ref);
        if (ret < 0) {
            tvherror_transcode(LST_QSV, "Decode: Failed to initialize HW Frames "
                            "with error code: %s", av_err2str(ret));
            av_buffer_unref(&avctx->hw_device_ctx);
            av_buffer_unref(&frames_ref);
            tvh_qsv_context_destroy(self);
            return ret;
        }

        // Attach the pool to the decoder
        avctx->hw_frames_ctx = frames_ref;
    }
#else
    
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 152
    /* set hw_frames_ctx for decoder's AVCodecContext */
    if ((ret = set_hwframe_ctx(avctx, avctx->hw_device_ctx)) < 0) {
        tvherror_transcode(LST_QSV, "Decode: Failed to set hwframe context."
                                      "Error code: %s", av_err2str(ret));
        av_buffer_unref(&avctx->hw_device_ctx);
        tvh_qsv_context_destroy(self);
        return ret;
    }
#endif
    
    ctx->hw_accel_ictx = self;
    avctx->pix_fmt = AV_PIX_FMT_QSV;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    return 0;
}

int
qsv_get_filters(TVHContext *self, char *filter, size_t filter_len)    
{
#define TEMP_FILTER_LEN 64*4
    // NOTE: filter_len > sum of all string
    // Now using snprintf for guaranteed buffer safety
    char hw_deint[64];
    char hw_scale[64];
    char hw_denoise[64];
    char hw_sharpness[64];
    char temp_filter[TEMP_FILTER_LEN];
    memset(temp_filter, 0, sizeof(temp_filter));
    size_t len=0;

    int height = ((TVHVideoCodecProfile *)self->profile)->size.den;

    int filter_scale = (self->iavctx->height != height);
    int filter_deint = ((TVHVideoCodecProfile *)self->profile)->deinterlace;
    int filter_denoise = ((TVHVideoCodecProfile *)self->profile)->filter_hw_denoise;
    int filter_sharpness = ((TVHVideoCodecProfile *)self->profile)->filter_hw_sharpness;

    memset(hw_deint, 0, sizeof(hw_deint));
    if (filter_deint){
        if (qsv_get_deint_filter(self->iavctx, hw_deint, sizeof(hw_deint))){
            tvherror_transcode(LST_QSV, "Filter: qsv_get_deint_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(temp_filter, TEMP_FILTER_LEN);
            snprintf(temp_filter + len, TEMP_FILTER_LEN - len, "%s%s", (len > 0) ? ":" : "", hw_deint);
        }
    }
    
    memset(hw_scale, 0, sizeof(hw_scale));
    if (filter_scale){ 
        if(qsv_get_scale_filter(self->iavctx, height, hw_scale, sizeof(hw_scale))){
            tvherror_transcode(LST_QSV, "Filter: qsv_get_scale_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(temp_filter, TEMP_FILTER_LEN);
            snprintf(temp_filter + len, TEMP_FILTER_LEN - len, "%s%s", (len > 0) ? ":" : "", hw_scale);
        }
    }
    
    memset(hw_denoise, 0, sizeof(hw_denoise));
    if (filter_denoise) { 
        if (qsv_get_denoise_filter(self->iavctx, filter_denoise, hw_denoise, sizeof(hw_denoise))){
            tvherror_transcode(LST_QSV, "Filter: qsv_get_denoise_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(temp_filter, TEMP_FILTER_LEN);
            snprintf(temp_filter + len, TEMP_FILTER_LEN - len, "%s%s", (len > 0) ? ":" : "", hw_denoise);
        }
    }

    memset(hw_sharpness, 0, sizeof(hw_sharpness));
    if (filter_sharpness){
        if (qsv_get_sharpness_filter(self->iavctx, filter_sharpness, hw_sharpness, sizeof(hw_sharpness))){
            tvherror_transcode(LST_QSV, "Filter: qsv_get_sharpness_filter() returned with error.");
            return -1;
        }
        else {
            len = strnlen(temp_filter, TEMP_FILTER_LEN);
            snprintf(temp_filter + len, TEMP_FILTER_LEN - len, "%s%s", (len > 0) ? ":" : "", hw_sharpness);
        }
    }
    
    len = strnlen(temp_filter, TEMP_FILTER_LEN);
    if (len) {
        // only if we have filters we add "vpp_qsv=" and we copy the string
        snprintf(filter, filter_len, "vpp_qsv=%s", temp_filter);
    }
    return 0;
}


int
qsv_get_download(TVHContext *self, int skip_format, char *filter, size_t filter_len)
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
qsv_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    int ret = 0;

    // this is required in case encode is called twice
    if (ctx->hw_device_octx)
        av_buffer_unref(&ctx->hw_device_octx);
    // 1. Create Device Context
    // Open QSV device and create an AVHWDeviceContext for it
    if ((ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0))) {
        tvherror_transcode(LST_QSV, "Encode: Failed to Open QSV device and create an AVHWDeviceContext"
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    avctx->hw_device_ctx = av_buffer_ref(ctx->hw_device_octx);
    if (!avctx->hw_device_ctx) {
        av_buffer_unref(&ctx->hw_device_octx);
        ctx->hw_device_octx = NULL;
        ret = AVERROR(ENOMEM);
        tvherror_transcode(LST_QSV, "Encode: Failed to create a hardware device context."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    
    // 2. CRITICAL: Frames Context are created in tvh_context_open_filters2()

    avctx->pix_fmt = AV_PIX_FMT_QSV;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;

    return 0;
}

int
qsv_get_upload(TVHContext *self, char *filter, size_t filter_len) {
    if (str_snprintf(filter, filter_len, "format=pix_fmts=%s,hwupload=extra_hw_frames=100",  // 
                    av_get_pix_fmt_name(self->oavctx->sw_pix_fmt))) {
        return -1;
    }
    return 0;
}

/* module =================================================================== */

void
qsv_decode_destroy(TVHContext *ctx)
{
    if (!ctx)
        return;
    tvh_qsv_context_destroy(ctx->hw_accel_ictx);
    ctx->hw_accel_ictx = NULL;
}

void
qsv_done()
{
    /* nothing to do */
}
