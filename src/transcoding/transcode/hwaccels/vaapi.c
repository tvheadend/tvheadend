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

#include "../../codec/internals.h"
#include "../internals.h"
#include "vaapi.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/pixdesc.h>

#include <va/va.h>


typedef struct tvh_vaapi_context_t {
    int width;
    int height;
    AVBufferRef *hw_device_ref;
} TVHVAContext;


static void
tvhva_done()
{
    /* nothing to do */
}


/* TVHVAContext ============================================================= */

static void
tvhva_context_destroy(TVHVAContext *self)
{
    if (self) {
        if (self->hw_device_ref) {
            av_buffer_unref(&self->hw_device_ref);
        }
        free(self);
        self = NULL;
    }
}


/* decoding ================================================================= */

static int
vaapi_get_buffer2(AVCodecContext *avctx, AVFrame *avframe, int flags)
{
    if (!(avctx->codec->capabilities & AV_CODEC_CAP_DR1)) {
        return avcodec_default_get_buffer2(avctx, avframe, flags);
    }
    return av_hwframe_get_buffer(avctx->hw_frames_ctx, avframe, 0);
}


int
vaapi_decode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;

    TVHVAContext *self = NULL;
    int ret = -1;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to allocate VAAPI context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 237
    /* Open VAAPI device and create an AVHWDeviceContext for it*/
    if ((ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_VAAPI, ctx->hw_accel_device, NULL, 0)) < 0) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to Open VAAPI device and create an AVHWDeviceContext for device: "
                            "%s with error code: %s", 
                            ctx->hw_accel_device, av_err2str(ret));
        free(self);
        self = NULL;
        return ret;
    }
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 95
    /* set hw_frames_ctx for decoder's AVCodecContext */
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ref);
    if (!avctx->hw_device_ctx) {
        tvherror_transcode(LST_VAAPI, "Decode: Failed to create a hardware device reference for device: %s.", 
                        ctx->hw_accel_device);
        // unref hw_device_ref
        av_buffer_unref(&self->hw_device_ref);
        self->hw_device_ref = NULL;
        free(self);
        self = NULL;
        return AVERROR(ENOMEM);
    }
    ctx->hw_accel_ictx = self;
    avctx->get_buffer2 = vaapi_get_buffer2;
    avctx->pix_fmt = AV_PIX_FMT_VAAPI;
    return 0;
}


void
vaapi_decode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;


    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
    }
    tvhva_context_destroy(ctx->hw_accel_ictx);
}


int
vaapi_get_scale_filter(AVCodecContext *iavctx, AVCodecContext *oavctx,
                       char *filter, size_t filter_len)
{
    snprintf(filter, filter_len, "scale_vaapi=w=%d:h=%d", oavctx->width, oavctx->height);
    return 0;
}


int
vaapi_get_deint_filter(AVCodecContext *avctx, char *filter, size_t filter_len)
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


int
vaapi_get_denoise_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    snprintf(filter, filter_len, "denoise_vaapi=%d", value);
    return 0;
}


int
vaapi_get_sharpness_filter(AVCodecContext *avctx, int value, char *filter, size_t filter_len)
{
    snprintf(filter, filter_len, "sharpness_vaapi=%d", value);
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
        tvherror_transcode(LST_VAAPI, "Encode: Failed to create VAAPI frame context.");
        return AVERROR(ENOMEM);
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror_transcode(LST_VAAPI, "Encode: Failed to initialize VAAPI frame context."
                "Error code: %s",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        tvherror_transcode(LST_VAAPI, "Encode: Failed to create a hardware device reference."
                "Error code: %s",av_err2str(err));
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}


int
vaapi_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    int ret = 0;

    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 127
    /* Open VAAPI device and create an AVHWDeviceContext for it*/
    if ((ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0)) < 0) {
        tvherror_transcode(LST_VAAPI, "Encode: Failed to open VAAPI device and create an AVHWDeviceContext for it."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    // lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 152
    /* set hw_frames_ctx for encoder's AVCodecContext */
    if ((ret = set_hwframe_ctx(avctx, ctx->hw_device_octx)) < 0) {
        tvherror_transcode(LST_VAAPI, "Encode: Failed to set hwframe context."
                "Error code: %s",av_err2str(ret));
        av_buffer_unref(&ctx->hw_device_octx);
    }
    return ret;
}


void
vaapi_encode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    // hw_device_octx is initialized for software decoding (line 764)
    if (ctx->hw_device_octx) {
        av_buffer_unref(&ctx->hw_device_octx);
    }
    // avctx->hw_device_ctx is cleaned up only if ffmpeg exit with error and didn't perform the clean up
    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
    }
    if (avctx->hw_frames_ctx) {
        av_buffer_unref(&avctx->hw_frames_ctx);
    }
}


/* module =================================================================== */

void
vaapi_done()
{
    tvhva_done();
}
