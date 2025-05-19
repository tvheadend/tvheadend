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

#if ENABLE_FFMPEG4_TRANSCODING
typedef struct tvh_vaapi_context_t {
    int width;
    int height;
    AVBufferRef *hw_device_ref;
    AVBufferRef *hw_frame_ref;
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
            self->hw_device_ref = NULL;
        }
        if (self->hw_frame_ref) {
            av_buffer_unref(&self->hw_frame_ref);
            self->hw_frame_ref = NULL;
        }
        free(self);
        self = NULL;
    }
}
#else
typedef struct tvh_vaapi_device {
    char *hw_device_name;
    AVBufferRef *hw_device_ref;
    LIST_ENTRY(tvh_vaapi_device) link;
} TVHVADevice;


typedef struct tvh_vaapi_context_t {
    void *display;
    uint32_t config_id;
    uint32_t context_id;
    const char *logpref;
    VAEntrypoint entrypoint;
    enum AVPixelFormat io_format;
    enum AVPixelFormat sw_format;
    int width;
    int height;
    AVBufferRef *hw_device_ref;
    AVBufferRef *hw_frames_ref;
} TVHVAContext;


static LIST_HEAD(, tvh_vaapi_device) tvhva_devices;


static AVBufferRef *
tvhva_init(const char *device)
{
    TVHVADevice *vad;

    if (tvh_str_default(device, NULL) == NULL)
        device = "/dev/dri/renderD128";
    LIST_FOREACH(vad, &tvhva_devices, link) {
        if (strcmp(vad->hw_device_name, device) == 0)
            break;
    }
    if (vad == NULL) {
        vad = calloc(1, sizeof(*vad));
        vad->hw_device_name = strdup(device);
        LIST_INSERT_HEAD(&tvhva_devices, vad, link);
    }
    if (vad->hw_device_ref)
        return vad->hw_device_ref;
    tvhtrace(LS_VAAPI, "trying device: %s", device);
    if (av_hwdevice_ctx_create(&vad->hw_device_ref, AV_HWDEVICE_TYPE_VAAPI,
                               device, NULL, 0)) {
        tvherror(LS_VAAPI,
                 "failed to create a context for device: %s", device);
        return NULL;
    }
    tvhtrace(LS_VAAPI, "successful context creation for device: %s", device);
    return vad->hw_device_ref;
}


static void
tvhva_done()
{
    TVHVADevice *vad;
    
    while ((vad = LIST_FIRST(&tvhva_devices)) != NULL) {
        LIST_REMOVE(vad, link);
        av_buffer_unref(&vad->hw_device_ref);
        free(vad->hw_device_name);
        free(vad);
    }
}


/* TVHVAContext ============================================================= */

static void
tvhva_context_destroy(TVHVAContext *self)
{
    if (self) {
        if (self->context_id != VA_INVALID_ID) {
            vaDestroyContext(self->display, self->context_id);
            self->context_id = VA_INVALID_ID;
        }
        if (self->config_id != VA_INVALID_ID) {
            vaDestroyConfig(self->display, self->config_id);
            self->config_id = VA_INVALID_ID;
        }
        self->display = NULL;
        if (self->hw_frames_ref) {
            av_buffer_unref(&self->hw_frames_ref);
            self->hw_frames_ref = NULL;
        }
        if (self->hw_device_ref) {
            av_buffer_unref(&self->hw_device_ref);
            self->hw_device_ref = NULL;
        }
        free(self);
        self = NULL;
    }
}


static VADisplay *
tvhva_context_display(TVHVAContext *self, AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    AVHWDeviceContext *hw_device_ctx = NULL;

    if (!ctx->hw_device_ref &&
        !(ctx->hw_device_ref = tvhva_init(ctx->hw_accel_device))) {
        return NULL;
    }
    if (!(self->hw_device_ref = av_buffer_ref(ctx->hw_device_ref))) {
        return NULL;
    }
    hw_device_ctx = (AVHWDeviceContext*)self->hw_device_ref->data;
    return ((AVVAAPIDeviceContext *)hw_device_ctx->hwctx)->display;
}


static VAProfile
tvhva_context_profile(TVHVAContext *self, AVCodecContext *avctx)
{
    VAStatus va_res = VA_STATUS_ERROR_UNKNOWN;
    VAProfile profile = VAProfileNone, check = VAProfileNone, *profiles = NULL;
    int i, j, profiles_max, profiles_len;

    switch (avctx->codec->id) {
        case AV_CODEC_ID_MPEG2VIDEO:
            switch (avctx->profile) {
                case FF_AV_PROFILE_UNKNOWN:
                case FF_AV_PROFILE_MPEG2_MAIN:
                    check = VAProfileMPEG2Main;
                    break;
                case FF_AV_PROFILE_MPEG2_SIMPLE:
                    check = VAProfileMPEG2Simple;
                    break;
                default:
                    break;
            }
            break;
        case AV_CODEC_ID_H264:
            switch (avctx->profile) {
                case FF_AV_PROFILE_UNKNOWN:
                case FF_AV_PROFILE_H264_HIGH:
                    check = VAProfileH264High;
                    break;
                case FF_AV_PROFILE_H264_CONSTRAINED_BASELINE:
                    check = VAProfileH264ConstrainedBaseline;
                    break;
                case FF_AV_PROFILE_H264_MAIN:
                    check = VAProfileH264Main;
                    break;
                default:
                    break;
            }
            break;
        case AV_CODEC_ID_HEVC:
            switch (avctx->profile) {
                case FF_AV_PROFILE_UNKNOWN:
                case FF_AV_PROFILE_HEVC_MAIN:
                    check = VAProfileHEVCMain;
                    break;
                case FF_AV_PROFILE_HEVC_MAIN_10:
                case FF_AV_PROFILE_HEVC_REXT:
                    check = VAProfileHEVCMain10;
                    break;
                default:
                    break;
            }
            break;
        case AV_CODEC_ID_VP8:
            switch (avctx->profile) {
                case FF_AV_PROFILE_UNKNOWN:
                    check = VAProfileVP8Version0_3;
                    break;
                default:
                    break;
            }
            break;
        case AV_CODEC_ID_VP9:
            switch (avctx->profile) {
                case FF_AV_PROFILE_UNKNOWN:
                case FF_AV_PROFILE_VP9_0:
                    check = VAProfileVP9Profile0;
                    break;
                case FF_AV_PROFILE_VP9_1:
                    check = VAProfileVP9Profile1;
                    break;
                case FF_AV_PROFILE_VP9_2:
                    check = VAProfileVP9Profile2;
                    break;
                case FF_AV_PROFILE_VP9_3:
                    check = VAProfileVP9Profile3;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (check != VAProfileNone &&
        (profiles_max = vaMaxNumProfiles(self->display)) > 0 &&
        (profiles = calloc(profiles_max, sizeof(VAProfile)))) {
        for (j = 0; j < profiles_max; j++) {
            profiles[j] = VAProfileNone;
        }
        if (profiles_max == 0) {
            tvherror(LS_VAAPI, "%s: vaMaxNumProfiles() returned %d; vaapi doesn't have any profiles available, run: $ vainfo", self->logpref, profiles_max);
        }
        va_res = vaQueryConfigProfiles(self->display,
                                       profiles, &profiles_len);
        if (va_res == VA_STATUS_SUCCESS) {
            for (i = 0; i < profiles_len; i++) {
                if (profiles[i] == check) {
                    profile = check;
                    break;
                }
            }
        }
        else {
            tvherror(LS_VAAPI, "%s: va_res != VA_STATUS_SUCCESS; Failed to query profiles: %d (%s), run: $ vainfo", self->logpref, va_res, vaErrorStr(va_res));
        }
        free(profiles);
    }

    return profile;
}


static int
tvhva_context_check_profile(TVHVAContext *self, VAProfile profile)
{
    VAStatus va_res = VA_STATUS_ERROR_UNKNOWN;
    VAEntrypoint *entrypoints = NULL;
    int res = -1, i, entrypoints_max, entrypoints_len;

    entrypoints_max = vaMaxNumEntrypoints(self->display);

    if (entrypoints_max > 0) {
        entrypoints = calloc(entrypoints_max, sizeof(VAEntrypoint));
        va_res = vaQueryConfigEntrypoints(self->display, profile, entrypoints, &entrypoints_len);
        if (va_res == VA_STATUS_SUCCESS) {
            for (i = 0; i < entrypoints_len; i++) {
                if (entrypoints[i] == self->entrypoint) {
                    res = 0;
                    break;
                }
            }
        }
        else {
            tvherror(LS_VAAPI, "%s: va_res != VA_STATUS_SUCCESS; Failed to query entrypoints: %d (%s), run: $ vainfo", self->logpref, va_res, vaErrorStr(va_res));
        }
        if (res != 0) {
            // before giving up we swap VAEntrypointEncSliceLP with VAEntrypointEncSlice or viceversa
            if (self->entrypoint == VAEntrypointEncSlice) {
                // we try VAEntrypointEncSliceLP
                self->entrypoint = VAEntrypointEncSliceLP;
                va_res = vaQueryConfigEntrypoints(self->display, profile, entrypoints, &entrypoints_len);
                if (va_res == VA_STATUS_SUCCESS) {
                    for (i = 0; i < entrypoints_len; i++) {
                        if (entrypoints[i] == self->entrypoint) {
                            res = 0;
                            break;
                        }
                    }
                }
            }
            else {
                // we try VAEntrypointEncSlice
                self->entrypoint = VAEntrypointEncSlice;
                va_res = vaQueryConfigEntrypoints(self->display, profile, entrypoints, &entrypoints_len);
                if (va_res == VA_STATUS_SUCCESS) {
                    for (i = 0; i < entrypoints_len; i++) {
                        if (entrypoints[i] == self->entrypoint) {
                            res = 0;
                            break;
                        }
                    }
                }
            }
        }
        free(entrypoints);
    }
    else {
        tvherror(LS_VAAPI, "%s: vaMaxNumEntrypoints() returned %d; vaapi doesn't have any entrypoints available, run: $ vainfo", self->logpref, entrypoints_max);
    }
    return res;
}


static unsigned int
tvhva_get_format(enum AVPixelFormat pix_fmt)
{
    switch (pix_fmt) {
        //case AV_PIX_FMT_YUV420P: // the cake is a lie
        case AV_PIX_FMT_NV12:
            return VA_RT_FORMAT_YUV420;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_UYVY422:
        case AV_PIX_FMT_YUYV422:
            return VA_RT_FORMAT_YUV422;
        case AV_PIX_FMT_GRAY8:
            return VA_RT_FORMAT_YUV400;
        default:
            return 0;
    }
}


static int
tvhva_context_config(TVHVAContext *self, VAProfile profile, unsigned int format)
{
    VAStatus va_res = VA_STATUS_ERROR_UNKNOWN;
    VAConfigAttrib attrib = { VAConfigAttribRTFormat, VA_ATTRIB_NOT_SUPPORTED };

    // vaCreateConfig
    va_res = vaGetConfigAttributes(self->display, profile, self->entrypoint,
                                   &attrib, 1);
    if (va_res != VA_STATUS_SUCCESS) {
        tvherror(LS_VAAPI, "%s: vaGetConfigAttributes: %s",
                 self->logpref, vaErrorStr(va_res));
        return -1;
    }
    if (attrib.value == VA_ATTRIB_NOT_SUPPORTED || !(attrib.value & format)) {
        tvherror(LS_VAAPI, "%s: unsupported VA_RT_FORMAT", self->logpref);
        return -1;
    }
    attrib.value = format;
    va_res = vaCreateConfig(self->display, profile, self->entrypoint,
                            &attrib, 1, &self->config_id);
    if (va_res != VA_STATUS_SUCCESS) {
        tvherror(LS_VAAPI, "%s: vaCreateConfig: %s",
                 self->logpref, vaErrorStr(va_res));
        return -1;
    }
    return 0;
}


static int
tvhva_context_check_constraints(TVHVAContext *self)
{
    AVVAAPIHWConfig *va_config = NULL;
    AVHWFramesConstraints *hw_constraints = NULL;
    enum AVPixelFormat sw_format = AV_PIX_FMT_NONE;
    const AVPixFmtDescriptor *sw_desc = NULL, *io_desc = NULL;
    int i, ret = 0;

    if (!(va_config = av_hwdevice_hwconfig_alloc(self->hw_device_ref))) {
        tvherror(LS_VAAPI, "%s: failed to allocate hwconfig", self->logpref);
        return AVERROR(ENOMEM);
    }
    va_config->config_id = self->config_id;

    hw_constraints =
        av_hwdevice_get_hwframe_constraints(self->hw_device_ref, va_config);
    if (!hw_constraints) {
        tvherror(LS_VAAPI, "%s: failed to get constraints", self->logpref);
        av_freep(&va_config);
        return -1;
    }

    if (self->io_format != AV_PIX_FMT_NONE) {
        for (i = 0; hw_constraints->valid_sw_formats[i] != AV_PIX_FMT_NONE; i++) {
            sw_format = hw_constraints->valid_sw_formats[i];
            if (sw_format == self->io_format) {
                self->sw_format = sw_format;
                break;
            }
        }
        if (self->sw_format == AV_PIX_FMT_NONE &&
            (io_desc = av_pix_fmt_desc_get(self->io_format))) {
            for (i = 0; hw_constraints->valid_sw_formats[i] != AV_PIX_FMT_NONE; i++) {
                sw_format = hw_constraints->valid_sw_formats[i];
                if ((sw_desc = av_pix_fmt_desc_get(sw_format)) &&
                    sw_desc->nb_components == io_desc->nb_components &&
                    sw_desc->log2_chroma_w == io_desc->log2_chroma_w &&
                    sw_desc->log2_chroma_h == io_desc->log2_chroma_h) {
                    self->sw_format = sw_format;
                    break;
                }
            }
        }
    }
    if (self->sw_format == AV_PIX_FMT_NONE) {
        tvherror(LS_VAAPI, "%s: VAAPI hardware does not support pixel format: %s",
                 self->logpref, av_get_pix_fmt_name(self->io_format));
        ret = AVERROR(EINVAL);
        goto end;
    }

    // Ensure the picture size is supported by the hardware.
    if (self->width < hw_constraints->min_width ||
        self->height < hw_constraints->min_height ||
        self->width > hw_constraints->max_width ||
        self->height > hw_constraints->max_height) {
        tvherror(LS_VAAPI, "%s: VAAPI hardware does not support image "
                 "size %dx%d (constraints: width %d-%d height %d-%d).",
                 self->logpref, self->width, self->height,
                 hw_constraints->min_width, hw_constraints->max_width,
                 hw_constraints->min_height, hw_constraints->max_height);
        ret = AVERROR(EINVAL);
    }

end:
    av_hwframe_constraints_free(&hw_constraints);
    av_freep(&va_config);
    return ret;
}


static int
tvhva_context_setup(TVHVAContext *self, AVCodecContext *avctx)
{
    VAProfile profile = VAProfileNone;
    unsigned int format = 0;
    VAStatus va_res = VA_STATUS_ERROR_UNKNOWN;
    AVHWFramesContext *hw_frames_ctx = NULL;
    AVVAAPIFramesContext *va_frames = NULL;

    if (!(self->display = tvhva_context_display(self, avctx))) {
        return -1;
    }

    libav_vaapi_init_context(self->display);
    // 
    profile = tvhva_context_profile(self, avctx);

    if (profile == VAProfileNone) {
        tvherror(LS_VAAPI, "%s: tvhva_context_profile() returned VAProfileNone for %s",
                 self->logpref,
                 avctx->codec->name);
        return -1;
    }

    if (tvhva_context_check_profile(self, profile)) {
        tvherror(LS_VAAPI, "%s: tvhva_context_check_profile() check failed for codec: %s --> codec not available",
                 self->logpref,
                 avctx->codec->name);
        return -1;
    }
    if (!(format = tvhva_get_format(self->io_format))) {
        tvherror(LS_VAAPI, "%s: unsupported pixel format: %s",
                 self->logpref,
                 av_get_pix_fmt_name(self->io_format));
        return -1;
    }

    if (tvhva_context_config(self, profile, format) ||
        tvhva_context_check_constraints(self)) {
        return -1;
    }

    if (!(self->hw_frames_ref = av_hwframe_ctx_alloc(self->hw_device_ref))) {
        tvherror(LS_VAAPI, "%s: failed to create VAAPI frame context.",
                 self->logpref);
        return AVERROR(ENOMEM);
    }
    hw_frames_ctx = (AVHWFramesContext*)self->hw_frames_ref->data;
    hw_frames_ctx->format = AV_PIX_FMT_VAAPI;
    hw_frames_ctx->sw_format = self->sw_format;
    hw_frames_ctx->width = self->width;
    hw_frames_ctx->height = self->height;
    hw_frames_ctx->initial_pool_size = 32;

    if (av_hwframe_ctx_init(self->hw_frames_ref) < 0) {
        tvherror(LS_VAAPI, "%s: failed to initialise VAAPI frame context",
                 self->logpref);
        return -1;
    }

    // vaCreateContext
    if (self->entrypoint == VAEntrypointVLD) { // decode only
        va_frames = hw_frames_ctx->hwctx;
        va_res = vaCreateContext(self->display, self->config_id,
                                 self->width, self->height,
                                 VA_PROGRESSIVE,
                                 va_frames->surface_ids, va_frames->nb_surfaces,
                                 &self->context_id);
        if (va_res != VA_STATUS_SUCCESS) {
            tvherror(LS_VAAPI, "%s: vaCreateContext: %s",
                     self->logpref, vaErrorStr(va_res));
            return -1;
        }
    }

    if (!(avctx->hw_frames_ctx = av_buffer_ref(self->hw_frames_ref))) {
        return AVERROR(ENOMEM);
    }

    avctx->sw_pix_fmt = self->sw_format;
    avctx->thread_count = 1;

    return 0;
}


static TVHVAContext *
tvhva_context_create(const char *logpref,
                     AVCodecContext *avctx, VAEntrypoint entrypoint)
{
    TVHVAContext *self = NULL;
    enum AVPixelFormat pix_fmt;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_VAAPI, "%s: failed to allocate vaapi context", logpref);
        return NULL;
    }
    self->logpref = logpref;
    self->display = NULL;
    self->config_id = VA_INVALID_ID;
    self->context_id = VA_INVALID_ID;
    self->entrypoint = entrypoint;
    pix_fmt = self->entrypoint == VAEntrypointVLD ?
                        avctx->sw_pix_fmt : avctx->pix_fmt;
    if (pix_fmt == AV_PIX_FMT_YUV420P ||
        pix_fmt == AV_PIX_FMT_VAAPI) {
        self->io_format = AV_PIX_FMT_NV12;
    }
    else {
        self->io_format = pix_fmt;
    }
    if (self->io_format == AV_PIX_FMT_NONE) {
        tvherror(LS_VAAPI, "%s: failed to get pix_fmt for vaapi context "
                           "(sw_pix_fmt: %s, pix_fmt: %s)",
                           logpref,
                           av_get_pix_fmt_name(avctx->sw_pix_fmt),
                           av_get_pix_fmt_name(avctx->pix_fmt));
        free(self);
        return NULL;
    }
    self->sw_format = AV_PIX_FMT_NONE;
    self->width = avctx->coded_width;
    self->height = avctx->coded_height;
    self->hw_device_ref = NULL;
    self->hw_frames_ref = NULL;
    if (tvhva_context_setup(self, avctx)) {
        tvhva_context_destroy(self);
        return NULL;
    }
    return self;
}
#endif


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

#if ENABLE_FFMPEG4_TRANSCODING
    TVHVAContext *self = NULL;
    int ret = -1;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_VAAPI, "Decode: Failed to allocate VAAPI context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }

    /* Open VAAPI device and create an AVHWDeviceContext for it*/
    if ((ret = av_hwdevice_ctx_create(&self->hw_device_ref, AV_HWDEVICE_TYPE_VAAPI, ctx->hw_accel_device, NULL, 0)) < 0) {
        tvherror(LS_VAAPI, "Decode: Failed to Open VAAPI device and create an AVHWDeviceContext for device: "
                            "%s with error code: %s", 
                            ctx->hw_accel_device, av_err2str(ret));
        return ret;
    }

    /* set hw_frames_ctx for decoder's AVCodecContext */
    avctx->hw_device_ctx = av_buffer_ref(self->hw_device_ref);
    if (!avctx->hw_device_ctx) {
        tvherror(LS_VAAPI, "Decode: Failed to create a hardware device reference for device: %s.", 
                        ctx->hw_accel_device);
        return AVERROR(ENOMEM);
    }
    ctx->hw_accel_ictx = self;
    avctx->get_buffer2 = vaapi_get_buffer2;
    avctx->pix_fmt = AV_PIX_FMT_VAAPI;
#else
    if (!(ctx->hw_accel_ictx =
          tvhva_context_create("decode", avctx, VAEntrypointVLD))) {
        return -1;
    }

    avctx->get_buffer2 = vaapi_get_buffer2;
#if LIBAVCODEC_VERSION_MAJOR < 60
    avctx->thread_safe_callbacks = 0;
#endif
#endif 

    return 0;
}


void
vaapi_decode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;

#if ENABLE_FFMPEG4_TRANSCODING
    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        avctx->hw_device_ctx = NULL;
    }
#endif
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
    snprintf(filter, filter_len, "deinterlace_vaapi");
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

#if ENABLE_FFMPEG4_TRANSCODING
// lifted from ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 41
static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int err = 0;

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        tvherror(LS_VAAPI, "Encode: Failed to create VAAPI frame context.");
        return AVERROR(ENOMEM);
    }
    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = ctx->width;
    frames_ctx->height    = ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((err = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        tvherror(LS_VAAPI, "Encode: Failed to initialize VAAPI frame context."
                "Error code: %s",av_err2str(err));
        av_buffer_unref(&hw_frames_ref);
        return err;
    }
    ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!ctx->hw_frames_ctx) {
        err = AVERROR(ENOMEM);
        tvherror(LS_VAAPI, "Encode: Failed to create a hardware device reference."
                "Error code: %s",av_err2str(err));
    }
    av_buffer_unref(&hw_frames_ref);
    return err;
}
#endif

int
#if ENABLE_FFMPEG4_TRANSCODING
vaapi_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *self = NULL;
    int ret = 0;

    if (!(self = calloc(1, sizeof(TVHVAContext)))) {
        tvherror(LS_VAAPI, "Encode: Failed to allocate VAAPI context (TVHVAContext)");
        return AVERROR(ENOMEM);
    }

    /* Open VAAPI device and create an AVHWDeviceContext for it*/
    if ((ret = av_hwdevice_ctx_create(&self->hw_frame_ref, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0)) < 0) {
        tvherror(LS_VAAPI, "Encode: Failed to open VAAPI device and create an AVHWDeviceContext for it."
                "Error code: %s",av_err2str(ret));
        return ret;
    }

    /* set hw_frames_ctx for encoder's AVCodecContext */
    if ((ret = set_hwframe_ctx(avctx, self->hw_frame_ref)) < 0) {
        tvherror(LS_VAAPI, "Encode: Failed to set hwframe context."
                "Error code: %s",av_err2str(ret));
        return ret;
    }
    ctx->hw_device_octx = av_buffer_ref(self->hw_frame_ref);
#else
vaapi_encode_setup_context(AVCodecContext *avctx, int low_power)
{
    TVHContext *ctx = avctx->opaque;
    TVHVAContext *hwaccel_context = NULL;
    VAEntrypoint entrypoint;

    if (low_power) {
        entrypoint = VAEntrypointEncSliceLP;
    }
    else {
        entrypoint = VAEntrypointEncSlice;
    }

    if (!(hwaccel_context =
          tvhva_context_create("encode", avctx, entrypoint))) {
        return -1;
    }
    if (!(ctx->hw_device_octx = av_buffer_ref(hwaccel_context->hw_device_ref))) {
        tvhva_context_destroy(hwaccel_context);
        return AVERROR(ENOMEM);
    }
    tvhva_context_destroy(hwaccel_context);
#endif
    return 0;
}


void
vaapi_encode_close_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    av_buffer_unref(&ctx->hw_device_octx);
    ctx->hw_device_octx = NULL;
#if ENABLE_FFMPEG4_TRANSCODING
    if (avctx->hw_device_ctx) {
        av_buffer_unref(&avctx->hw_device_ctx);
        avctx->hw_device_ctx = NULL;
    }
    if (avctx->hw_frames_ctx) {
        av_buffer_unref(&avctx->hw_frames_ctx);
        avctx->hw_frames_ctx = NULL;
    }
#endif
}


/* module =================================================================== */

void
vaapi_done()
{
    tvhva_done();
}
