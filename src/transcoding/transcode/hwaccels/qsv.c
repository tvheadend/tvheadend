/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2024 Tvheadend
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


#include "../internals.h"
#include "qsv.h"
#include "vaapi.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_qsv.h>
#include <libavutil/pixdesc.h>

#include <va/va.h>


typedef struct tvh_qsv_context_t {
    //void *loader;
    //enum AVPixelFormat io_format;
    //enum AVPixelFormat sw_format;
    int width;
    int height;
    AVBufferRef *hw_device_ctx;
    AVBufferRef *hw_frame_ref;
} TVHVAContext;


static void
tvhva_done()
{
    /*
    TVHVADevice *vad;
    
    while ((vad = LIST_FIRST(&tvhva_devices)) != NULL) {
        LIST_REMOVE(vad, link);
        av_buffer_unref(&vad->hw_device_ref);
        free(vad->hw_device_name);
        free(vad);
    }
    */
}


/* TVHVAContext ============================================================= */

static void
tvhva_context_destroy(TVHVAContext *self)
{
    if (self) {
        if (self->hw_device_ctx) {
            av_buffer_unref(&self->hw_device_ctx);
            self->hw_device_ctx = NULL;
        }
        free(self);
        self = NULL;
    }
}


/* decoding ================================================================= */


static int
qsv_get_buffer2(AVCodecContext *avctx, AVFrame *avframe, int flags)
{
    if (!(avctx->codec->capabilities & AV_CODEC_CAP_DR1)) {
        return avcodec_default_get_buffer2(avctx, avframe, flags);
    }
    return av_hwframe_get_buffer(avctx->hw_frames_ctx, avframe, 0);
}

int
qsv_decode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *self = NULL;
    int ret = -1;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_QSV, "Decode: Failed to allocate qsv context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }

    //if (av_hwdevice_ctx_create(&self->hw_device_ctx, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0)) {
    ret = av_hwdevice_ctx_create(&self->hw_device_ctx, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0);
    if (ret < 0) {
        tvherror(LS_QSV, "Decode: Failed to create a context with error code: %s", av_err2str(ret));
        return AVERROR(ENOMEM);
    }
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ctx);
    if (!avctx->hw_device_ctx) {
        tvherror(LS_QSV, "Decode: A hardware device reference create failed.");
        return AVERROR(ENOMEM);
    }
    ctx->hw_accel_ictx = self;
    avctx->get_buffer2 = qsv_get_buffer2;
    return 0;
}

// saved in txt file

void
qsv_decode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        avctx->hw_device_ctx = NULL;
    }
    tvhva_context_destroy(ctx->hw_accel_ictx);
}


int
qsv_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx,
                       char *filter, size_t filter_len)
{
    snprintf(filter, filter_len, "scale_qsv=w=%d:h=%d:mode=2", oavctx->width, oavctx->height);
    return 0;
}


int
qsv_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
{
    snprintf(filter, filter_len, "deinterlace_qsv=mode=2");
    return 0;
}

/* encoding ================================================================= */

// lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 41
static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        tvherror(LS_QSV, "Encode: Failed to create QSV frame context.");
        return -1;
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_QSV;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 32;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror(LS_QSV, "Encode: Failed to initialize QSV frame context."
                "Error code: %s\n",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        fprintf(stderr, "Encode: Failed to create a hardware device reference.");
        err = AVERROR(ENOMEM);
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}


int
qsv_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *self = NULL;
    int ret = 0;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_QSV, "Encode: Failed to allocate QSV context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }
    ret = av_hwdevice_ctx_create(&self->hw_frame_ref, AV_HWDEVICE_TYPE_QSV, "auto", NULL, 0);
    /* set hw_frames_ctx for encoder's AVCodecContext */
    if ((ret = set_hwframe_ctx(avctx, self->hw_frame_ref)) < 0) {
        tvherror(LS_QSV, "Encode: Failed to set hwframe context.");
        return -1;
    }
    ctx->hw_frame_octx = av_buffer_ref(self->hw_frame_ref);
    //avctx->extra_hw_frames = 32;

/*
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *hwaccel_context = NULL;
    enum AVPixelFormat pix_fmt;
    TVHVAContext *self = NULL;

    //AVHWFramesContext *hw_frames_ctx = NULL;

    int ret = 0;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_QSV, "Encode: Failed to allocate qsv context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }

    pix_fmt = avctx->pix_fmt;
    if (pix_fmt == AV_PIX_FMT_YUV420P ||
        pix_fmt == AV_PIX_FMT_QSV) {
        self->io_format = AV_PIX_FMT_NV12;
    }
    else {
        self->io_format = pix_fmt;
    }
    
    if (self->io_format == AV_PIX_FMT_NONE) {
        tvherror(LS_QSV, "Encode: Failed to get pix_fmt for qsv context (sw_pix_fmt: %s, pix_fmt: %s)",
                           av_get_pix_fmt_name(avctx->sw_pix_fmt), av_get_pix_fmt_name(avctx->pix_fmt));
        free(self);
        ctx->hw_accel_ictx = NULL;
        return AVERROR(ENOMEM);
    }

    self->sw_format = AV_PIX_FMT_NV12; // avctx->sw_pix_fmt; //  AV_PIX_FMT_NONE;
    self->width = avctx->coded_width;
    self->height = avctx->coded_height;
    self->hw_device_ref = NULL;
    self->hw_frames_ctx = NULL;

    if (!ctx->hw_device_ref &&
        !(ctx->hw_device_ref = tvhva_init(ctx->hw_accel_device))) {
        return AVERROR(ENOMEM);
    }
    if (!(self->hw_device_ref = av_buffer_ref(ctx->hw_device_ref))) {
        return AVERROR(ENOMEM);
    }

    ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_QSV, NULL, NULL, 0);
    if (ret < 0) {
        tvherror(LS_QSV, "Encode: Failed to create a QSV device. Error code: %s\n", av_err2str(ret));
        return AVERROR(ENOMEM);
    }

    avctx->pix_fmt = AV_PIX_FMT_QSV;

    if (!(self->hw_frames_ctx = av_hwframe_ctx_alloc(self->hw_device_ref))) {
        tvherror(LS_QSV, "Encode: Failed to create QSV frame context.");
        return AVERROR(ENOMEM);
    }
    pix_fmt = avctx->pix_fmt;
    if (pix_fmt == AV_PIX_FMT_YUV420P ||
        pix_fmt == AV_PIX_FMT_QSV) {
        self->io_format = AV_PIX_FMT_NV12;
    }
    else {
        self->io_format = pix_fmt;
    }
    if (self->io_format == AV_PIX_FMT_NONE) {
        tvherror(LS_QSV, "Encode: Failed to get pix_fmt for qsv context (sw_pix_fmt: %s, pix_fmt: %s)",
                           av_get_pix_fmt_name(avctx->sw_pix_fmt), av_get_pix_fmt_name(avctx->pix_fmt));
        free(self);
        return AVERROR(ENOMEM);
    }

    AVHWFramesContext *hw_frames_ctx = NULL;
    AVQSVFramesContext *hw_frames_hwctx = NULL;

    hw_frames_ctx = (AVHWFramesContext*)self->hw_frames_ctx->data;
    hw_frames_hwctx = hw_frames_ctx->hwctx;

    hw_frames_ctx->format = AV_PIX_FMT_QSV;
    hw_frames_ctx->sw_format = self->sw_format;
    hw_frames_ctx->width = self->width;
    hw_frames_ctx->height = self->height;
    hw_frames_ctx->initial_pool_size = 32;

    hw_frames_hwctx->frame_type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

    tvhinfo(LS_QSV, "before av_hwframe_ctx_init()");
    if (av_hwframe_ctx_init(self->hw_frames_ctx) < 0) {
        tvherror(LS_QSV, "Encode: Failed to initialise QSV frame context");
        return -1;
    }
    tvhinfo(LS_QSV, "success av_hwframe_ctx_init()");

    // vaCreateContext
    //hw_frames_hwctx = hw_frames_ctx->hwctx;

    if (!(avctx->hw_frames_ctx = av_buffer_ref(self->hw_frames_ctx))) {
        return AVERROR(ENOMEM);
    }
    tvhinfo(LS_QSV, "success hw_frames_ctx: av_buffer_ref()");

    //avctx->sw_pix_fmt = self->sw_format;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    avctx->thread_count = 1;

    hwaccel_context = self;

    if (!(hwaccel_context)) {
        return -1;
    }
    if (!(ctx->hw_device_octx = av_buffer_ref(hwaccel_context->hw_device_ref))) {
        tvhva_context_destroy(hwaccel_context);
        return AVERROR(ENOMEM);
    }
    tvhva_context_destroy(hwaccel_context);
*/

/*
    // ERROR] (null): failed to get pix_fmt for qsv context (sw_pix_fmt: nv12, pix_fmt: vaapi)
    tvhinfo(LS_QSV, "get pix_fmt for qsv context (sw_pix_fmt: %s, pix_fmt: %s)",
                           av_get_pix_fmt_name(avctx->sw_pix_fmt),
                           av_get_pix_fmt_name(avctx->pix_fmt));

    //av_hwframe_transfer_get_formats(avctx->hw_frames_ctx,);
*/
    return 0;

}


void
qsv_encode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    av_buffer_unref(&ctx->hw_frame_octx);
    ctx->hw_frame_octx = NULL;
    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        avctx->hw_device_ctx = NULL;
    }
    if (avctx->hw_frames_ctx) {
        av_buffer_unref(&avctx->hw_frames_ctx);
        avctx->hw_frames_ctx = NULL;
    }
    /*
    TVHContext *ctx = avctx->opaque;
    av_buffer_unref(&ctx->hw_device_octx);
    ctx->hw_device_octx = NULL;
    */
    //if (avctx->hw_device_ctx) {
    //    av_buffer_unref(&avctx->hw_device_ctx);
    //    avctx->hw_device_ctx = NULL;
    //}
    //if (avctx->hw_frames_ctx) {
    //    av_buffer_unref(&avctx->hw_frames_ctx);
    //    avctx->hw_frames_ctx = NULL;
    //}
}


/* module =================================================================== */

void
qsv_done()
{
    tvhva_done();
}
