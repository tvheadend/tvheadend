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

#include "internals.h"

#if ENABLE_HWACCELS
#include "hwaccels/hwaccels.h"
#endif


static int
_video_filters_hw_pix_fmt(enum AVPixelFormat pix_fmt)
{
    const AVPixFmtDescriptor *desc;

    if ((desc = av_pix_fmt_desc_get(pix_fmt)) &&
        (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        return 1;
    }
    return 0;
}


static int
_video_filters_get_filters(TVHContext *self, AVDictionary **opts, char **filters)
{
    char download[48];
    char deint[8];
    char hw_deint[64];
    char scale[24];
    char hw_scale[64];
    char upload[48];
    char hw_denoise[64];
    char hw_sharpness[64];
    int ihw = _video_filters_hw_pix_fmt(self->iavctx->pix_fmt);
    int ohw = _video_filters_hw_pix_fmt(self->oavctx->pix_fmt);
    int filter_scale = (self->iavctx->height != self->oavctx->height);
    int filter_deint = 0, filter_download = 0, filter_upload = 0;
#if ENABLE_HWACCELS
    int filter_denoise = 0;
    int filter_sharpness = 0;
#endif

    if (tvh_context_get_int_opt(opts, "tvh_filter_deint", &filter_deint)) {
        return -1;
    }
#if ENABLE_VAAPI
    filter_denoise = self->profile->filter_hw_denoise;
    filter_sharpness = self->profile->filter_hw_sharpness;
#endif
    //  in --> out  |  download   |   upload 
    // -------------|-------------|------------
    //  hw --> hw   |     0       |     0
    //  sw --> hw   |     0       |     1
    //  hw --> sw   |     1       |     0
    //  sw --> sw   |     0       |     0
    filter_download = (ihw && (!ohw)) ? 1 : 0;
    filter_upload = ((!ihw) && ohw) ? 1 : 0;

    memset(deint, 0, sizeof(deint));
    memset(hw_deint, 0, sizeof(hw_deint));
#if ENABLE_HWACCELS
    if (filter_deint) {
        // when hwaccel is enabled we have two options:
        if (ihw) {
            // hw deint
            hwaccels_get_deint_filter(self->iavctx, hw_deint, sizeof(hw_deint));
        }
        else {
            // sw deint
            if (str_snprintf(deint, sizeof(deint), "yadif")) {
                return -1;
            }
        }
    }
#else
    if (filter_deint) {
        if (str_snprintf(deint, sizeof(deint), "yadif")) {
            return -1;
        }
    }
#endif

    memset(scale, 0, sizeof(scale));
    memset(hw_scale, 0, sizeof(hw_scale));
#if ENABLE_HWACCELS
    if (filter_scale) {
        // when hwaccel is enabled we have two options:
        if (ihw) {
            // hw scale
            hwaccels_get_scale_filter(self->iavctx, self->oavctx, hw_scale, sizeof(hw_scale));
        }
        else {
            // sw scale
            if (str_snprintf(scale, sizeof(scale), "scale=w=-2:h=%d",
                         self->oavctx->height)) {
                return -1;
            }
        }
    }
#else
    if (filter_scale) {
        if (str_snprintf(scale, sizeof(scale), "scale=w=-2:h=%d",
                         self->oavctx->height)) {
            return -1;
        }
    }
#endif

    memset(hw_denoise, 0, sizeof(hw_denoise));
#if ENABLE_HWACCELS
    if (filter_denoise) {
        // used only when hwaccel is enabled
        if (ihw) {
            // hw scale
            hwaccels_get_denoise_filter(self->iavctx, filter_denoise, hw_denoise, sizeof(hw_denoise));
        }
    }
#endif

    memset(hw_sharpness, 0, sizeof(hw_sharpness));
#if ENABLE_HWACCELS
    if (filter_sharpness) {
        // used only when hwaccel is enabled
        if (ihw) {
            // hw scale
            hwaccels_get_sharpness_filter(self->iavctx, filter_sharpness, hw_sharpness, sizeof(hw_sharpness));
        }
    }
#endif

#if ENABLE_HWACCELS
    // no filter required.
#else
    if (deint[0] == '\0' && scale[0] == '\0') {
        filter_download = filter_upload = 0;
    }
#endif

    memset(download, 0, sizeof(download));
    if (filter_download &&
        str_snprintf(download, sizeof(download), "hwdownload,format=pix_fmts=%s",
                     av_get_pix_fmt_name(self->iavctx->sw_pix_fmt))) {
        return -1;
    }

    memset(upload, 0, sizeof(upload));
    if (filter_upload &&
        str_snprintf(upload, sizeof(upload), "format=pix_fmts=%s|%s,hwupload",
                     av_get_pix_fmt_name(self->oavctx->sw_pix_fmt),
                     av_get_pix_fmt_name(self->oavctx->pix_fmt))) {
        return -1;
    }

    if (!(*filters = str_join(",", hw_deint, hw_scale, hw_denoise, hw_sharpness, download, deint, scale, upload, NULL))) {
        return -1;
    }

    return 0;
}


static int
tvh_video_context_open_decoder(TVHContext *self, AVDictionary **opts)
{
#if ENABLE_HWACCELS
    int hwaccel = -1;
    if ((hwaccel = tvh_codec_profile_video_get_hwaccel(self->profile)) < 0) {
        return -1;
    }
    if (hwaccel) {
        self->iavctx->get_format = hwaccels_decode_get_format;
    }
    mystrset(&self->hw_accel_device, self->profile->device);
#endif
    self->iavctx->time_base = av_make_q(1, 90000); // MPEG-TS uses a fixed timebase of 90kHz
    return 0;
}


static int
tvh_video_context_notify_gh(TVHContext *self)
{
    /* notify global headers that we're live */
    /* the video packets might be delayed */
    th_pkt_t *pkt = NULL;

    pkt = pkt_alloc(self->stream->type, NULL, 0,
                    self->src_pkt->pkt_pts,
                    self->src_pkt->pkt_dts,
                    self->src_pkt->pkt_pcr);
    if (pkt) {
        return tvh_context_deliver(self, pkt);
    }
    return -1;
}


static int
tvh_video_context_open_encoder(TVHContext *self, AVDictionary **opts)
{
    AVRational ticks_per_frame;

    if (tvh_context_get_int_opt(opts, "pix_fmt", &self->oavctx->pix_fmt) ||
        tvh_context_get_int_opt(opts, "width", &self->oavctx->width) ||
        tvh_context_get_int_opt(opts, "height", &self->oavctx->height)) {
        return -1;
    }

#if ENABLE_HWACCELS
#if ENABLE_FFMPEG4_TRANSCODING
    // hwaccel is the user input for Hardware acceleration from Codec parameteres
    int hwaccel = -1;
    if ((hwaccel = tvh_codec_profile_video_get_hwaccel(self->profile)) < 0) {
        return -1;
    }
    if (_video_filters_hw_pix_fmt(self->oavctx->pix_fmt)){
        // encoder is hw accelerated
        if (hwaccel) {
            // decoder is hw accelerated 
            // --> we initialize encoder from decoder as recomanded in: 
            // ffmpeg-6.1.1/doc/examples/vaapi_transcode.c line 169
            if (hwaccels_initialize_encoder_from_decoder(self->iavctx, self->oavctx)) {
                return -1;
            }
        }
        else {
            // decoder is sw
            // --> we initialize as recommended in:
            // ffmpeg-6.1.1/doc/examples/vaapi_encode.c line 145
            if (hwaccels_encode_setup_context(self->oavctx)) {
                return -1;
            }
        }
    }
#else
    if (hwaccels_encode_setup_context(self->oavctx, self->profile->low_power)) {
        return -1;
    }
#endif // from ENABLE_FFMPEG4_TRANSCODING
#endif // from ENABLE_HWACCELS

    // XXX: is this a safe assumption?
    if (!self->iavctx->framerate.num) {
        self->iavctx->framerate = av_make_q(30, 1);
    }
    self->oavctx->framerate = self->iavctx->framerate;
    self->oavctx->ticks_per_frame = (90000 * self->iavctx->framerate.den) / self->iavctx->framerate.num; // We assume 90kHz as timebase which is mandatory for MPEG-TS
    ticks_per_frame = av_make_q(self->oavctx->ticks_per_frame, 1);
    self->oavctx->time_base = av_inv_q(av_mul_q(
        self->oavctx->framerate, ticks_per_frame));
    self->oavctx->gop_size = ceil(av_q2d(av_inv_q(av_mul_q(
        self->oavctx->time_base, ticks_per_frame))));
    self->oavctx->gop_size *= 3;

    self->oavctx->sample_aspect_ratio = self->iavctx->sample_aspect_ratio;
    return 0;
}


static int
tvh_video_context_open_filters(TVHContext *self, AVDictionary **opts)
{
    char source_args[128];
    char *filters = NULL;

    // source args
    memset(source_args, 0, sizeof(source_args));
    if (str_snprintf(source_args, sizeof(source_args),
            "video_size=%dx%d:pix_fmt=%s:time_base=%d/%d:pixel_aspect=%d/%d",
            self->iavctx->width,
            self->iavctx->height,
            av_get_pix_fmt_name(self->iavctx->pix_fmt),
            self->iavctx->time_base.num,
            self->iavctx->time_base.den,
            self->iavctx->sample_aspect_ratio.num,
            self->iavctx->sample_aspect_ratio.den)) {
        return -1;
    }

    // filters
    if (_video_filters_get_filters(self, opts, &filters)) {
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR > 59
    int ret = tvh_context_open_filters(self,
        "buffer", source_args,                                  // source
        strlen(filters) ? filters : "null",                     // filters
        "buffersink",                                           // sink
        "pix_fmts", AV_OPT_SET_BIN,                             // sink option: pix_fmt
        sizeof(self->oavctx->pix_fmt), &self->oavctx->pix_fmt,
        NULL);                                                  // _IMPORTANT!_
#else
    int ret = tvh_context_open_filters(self,
        "buffer", source_args,              // source
        strlen(filters) ? filters : "null", // filters
        "buffersink",                       // sink
        "pix_fmts", &self->oavctx->pix_fmt, // sink option: pix_fmt
        sizeof(self->oavctx->pix_fmt),
        NULL);                              // _IMPORTANT!_
#endif
    str_clear(filters);
    return ret;
}


static int
tvh_video_context_open(TVHContext *self, TVHOpenPhase phase, AVDictionary **opts)
{
    switch (phase) {
        case OPEN_DECODER:
            return tvh_video_context_open_decoder(self, opts);
        case OPEN_DECODER_POST:
            return tvh_video_context_notify_gh(self);
        case OPEN_ENCODER:
            return tvh_video_context_open_encoder(self, opts);
        case OPEN_ENCODER_POST:
            return tvh_video_context_open_filters(self, opts);
        default:
            break;
    }
    return 0;
}


static int
tvh_video_context_encode(TVHContext *self, AVFrame *avframe)
{
    avframe->pts = avframe->best_effort_timestamp;
    if (avframe->pts <= self->pts) {
        tvh_context_log(self, LOG_WARNING,
                        "Invalid pts (%"PRId64") <= last (%"PRId64"), dropping frame",
                        avframe->pts, self->pts);
        return AVERROR(EAGAIN);
    }
    self->pts = avframe->pts;
#if LIBAVUTIL_VERSION_MAJOR > 58 || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR > 2)
    if (avframe->flags & AV_FRAME_FLAG_INTERLACED) {
        self->oavctx->field_order =
            (avframe->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) ? AV_FIELD_TB : AV_FIELD_BT;
    }
    else {
        self->oavctx->field_order = AV_FIELD_PROGRESSIVE;
    }
#else
    if (avframe->interlaced_frame) {
        self->oavctx->field_order =
            avframe->top_field_first ? AV_FIELD_TB : AV_FIELD_BT;
    }
    else {
        self->oavctx->field_order = AV_FIELD_PROGRESSIVE;
    }
#endif
    return 0;
}


static int
tvh_video_context_ship(TVHContext *self, AVPacket *avpkt)
{
    if (avpkt->size < 0 || avpkt->pts < avpkt->dts) {
        tvh_context_log(self, LOG_ERR, "encode failed");
        return -1;
    }
    return avpkt->size;
}


static int
tvh_video_context_wrap(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{   
    enum AVPictureType pict_type = self->oavframe->pict_type;

    if (pict_type == AV_PICTURE_TYPE_NONE && avpkt->flags & AV_PKT_FLAG_KEY) {
        pict_type = AV_PICTURE_TYPE_I;
    }
    if (pict_type == AV_PICTURE_TYPE_NONE) {
        // some codecs do not set pict_type but set AV_FRAME_FLAG_KEY, in this case,
        // we assume that when AV_FRAME_FLAG_KEY is set the frame is an I-frame
        // (all the others are assumed to be P-frames)
#if LIBAVUTIL_VERSION_MAJOR > 58 || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR > 2)
        if (self->oavframe->flags & AV_FRAME_FLAG_KEY) {
            pict_type = AV_PICTURE_TYPE_I;
        }
        else {
            pict_type = AV_PICTURE_TYPE_P;
        }
#else
        if (self->oavframe->key_frame) {
            pict_type = AV_PICTURE_TYPE_I;
        }
        else {
            pict_type = AV_PICTURE_TYPE_P;
        }
#endif
    }

    switch (pict_type) {
        case AV_PICTURE_TYPE_I:
            pkt->v.pkt_frametype = PKT_I_FRAME;
            break;
        case AV_PICTURE_TYPE_P:
            pkt->v.pkt_frametype = PKT_P_FRAME;
            break;
        case AV_PICTURE_TYPE_B:
            pkt->v.pkt_frametype = PKT_B_FRAME;
            break;
        default:
            tvh_context_log(self, LOG_WARNING, "unknown picture type: %d",
                            pict_type);
            break;
    }
    pkt->pkt_duration   = avpkt->duration;
    pkt->pkt_commercial = self->src_pkt->pkt_commercial;
    pkt->v.pkt_field      = (self->oavctx->field_order > AV_FIELD_PROGRESSIVE);
    pkt->v.pkt_aspect_num = self->src_pkt->v.pkt_aspect_num;
    pkt->v.pkt_aspect_den = self->src_pkt->v.pkt_aspect_den;
    return 0;
}


static void
tvh_video_context_close(TVHContext *self)
{
#if ENABLE_HWACCELS
    hwaccels_encode_close_context(self->oavctx);
    hwaccels_decode_close_context(self->iavctx);
#endif
}


TVHContextType TVHVideoContext = {
    .media_type = AVMEDIA_TYPE_VIDEO,
    .open       = tvh_video_context_open,
    .encode     = tvh_video_context_encode,
    .ship       = tvh_video_context_ship,
    .wrap       = tvh_video_context_wrap,
    .close      = tvh_video_context_close,
};
