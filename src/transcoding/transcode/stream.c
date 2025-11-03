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
#include "../codec/internals.h"


/* TVHStream ================================================================ */

static int
tvh_stream_is_copy(TVHCodecProfile *profile, tvh_ssc_t *ssc,
                   const char *src_codecs)
{
    const char *txtname;
    htsmsg_t *list;
    int r;

    /* if the source codec is in the list, do the stream copy only */
    if (src_codecs && *src_codecs != '\0' && *src_codecs != '-') {
        list = htsmsg_csv_2_list(src_codecs, ',');
        if (list) {
            txtname = streaming_component_type2txt(ssc->es_type);
            r = htsmsg_is_string_in_list(list, txtname);
            htsmsg_destroy(list);
            if (r)
                goto cont;
        }
        return 1;
    }
cont:
    if (profile == tvh_codec_profile_copy) {
        return 1;
    }
    return tvh_codec_profile_is_copy(profile, ssc);
}


static int
tvh_stream_setup(TVHStream *self, TVHCodecProfile *profile, tvh_ssc_t *ssc)
{
    enum AVCodecID icodec_id = streaming_component_type2codec_id(ssc->es_type);
    const AVCodec *icodec = NULL, *ocodec = NULL;

    if (icodec_id == AV_CODEC_ID_NONE) {
        tvh_stream_log(self, LOG_ERR, "unknown decoder id for '%s'",
                       streaming_component_type2txt(ssc->es_type));
        return -1;
    }
#if ENABLE_MMAL | ENABLE_NVENC | ENABLE_VAAPI
    int hwaccel = -1;
    int hwaccel_details = -1;
    if (SCT_ISVIDEO(ssc->es_type)) {
        if (((hwaccel         = tvh_codec_profile_video_get_hwaccel(profile)) < 0) ||
            ((hwaccel_details = tvh_codec_profile_video_get_hwaccel_details(profile)) < 0)) {
            return -1;
        }
#if ENABLE_MMAL
        if (idnode_is_instance(&profile->idnode, (idclass_t *)&codec_profile_video_class) &&
            hwaccel &&
            ((hwaccel_details == HWACCEL_AUTO && strstr(profile->codec_name, "mmal")) || hwaccel_details == HWACCEL_PRIORITIZE_MMAL)) {
            if (icodec_id == AV_CODEC_ID_H264) {
                icodec = avcodec_find_decoder_by_name("h264_mmal");
            } else if (icodec_id == AV_CODEC_ID_MPEG2VIDEO) {
                icodec = avcodec_find_decoder_by_name("mpeg2_mmal");
            }
        }
#endif
#if ENABLE_NVENC
        if (!icodec && 
            idnode_is_instance(&profile->idnode, (idclass_t *)&codec_profile_video_class) &&
            hwaccel &&
            ((hwaccel_details == HWACCEL_AUTO && strstr(profile->codec_name, "nvenc")) || hwaccel_details == HWACCEL_PRIORITIZE_NVDEC)) {
            // https://developer.nvidia.com/video-codec-sdk
            if (icodec_id == AV_CODEC_ID_MPEG2VIDEO) {
                icodec = avcodec_find_decoder_by_name("mpeg2_cuvid");
            } else if (icodec_id == AV_CODEC_ID_H264) {
                icodec = avcodec_find_decoder_by_name("h264_cuvid");
            } else if (icodec_id == AV_CODEC_ID_HEVC) {
                icodec = avcodec_find_decoder_by_name("hevc_cuvid");
            } else if (icodec_id == AV_CODEC_ID_VP9) {
                icodec = avcodec_find_decoder_by_name("vp9_cuvid");
            } else if (icodec_id == AV_CODEC_ID_VP8) {
                icodec = avcodec_find_decoder_by_name("vp8_cuvid");
            }
        }
#endif
#if ENABLE_VAAPI
        if (!icodec && 
            idnode_is_instance(&profile->idnode, (idclass_t *)&codec_profile_video_class) &&
            hwaccel &&
            (hwaccel_details == HWACCEL_PRIORITIZE_VAAPI)) {
            if (icodec_id == AV_CODEC_ID_MPEG2VIDEO) {
                icodec = avcodec_find_decoder_by_name("mpeg2_vaapi");
            } else if (icodec_id == AV_CODEC_ID_H264) {
                icodec = avcodec_find_decoder_by_name("h264_vaapi");
            } else if (icodec_id == AV_CODEC_ID_HEVC) {
                icodec = avcodec_find_decoder_by_name("hevc_vaapi");
            } else if (icodec_id == AV_CODEC_ID_VP9) {
                icodec = avcodec_find_decoder_by_name("vp9_vaapi");
            } else if (icodec_id == AV_CODEC_ID_VP8) {
                icodec = avcodec_find_decoder_by_name("vp8_vaapi");
            }
        }
#endif
    }
#endif // from ENABLE_MMAL | ENABLE_NVENC | ENABLE_VAAPI
    if (!icodec && !(icodec = avcodec_find_decoder(icodec_id))) {
        tvh_stream_log(self, LOG_ERR, "failed to find decoder for '%s'",
                       streaming_component_type2txt(ssc->es_type));
        return -1;
    }
    if (!(ocodec = tvh_codec_profile_get_avcodec(profile))) {
        tvh_stream_log(self, LOG_ERR, "profile '%s' is disabled",
                       tvh_codec_profile_get_name(profile));
        return -1;
    }
    if (icodec->type == AVMEDIA_TYPE_UNKNOWN ||
        icodec->type != ocodec->type) { // is this even possible?
        tvh_stream_log(self, LOG_ERR, "unknown or mismatch media type");
        return -1;
    }

    if (!(self->context = tvh_context_create(self, profile,
                                             icodec, ocodec, ssc->ssc_gh))) {
        return -1;
    }
    self->type = ssc->es_type = codec_id2streaming_component_type(ocodec->id);
    if (ssc->es_type == SCT_UNKNOWN) {
        tvh_stream_log(self, LOG_ERR, "unable to translate AV type %s [%d] to SCT!", ocodec->name, ocodec->id);
    }
    ssc->ssc_gh = NULL;
    return 0;
}


/* exposed */

void
tvh_stream_stop(TVHStream *self, int flush)
{
    if (self->index >= 0) {
        if (self->context) {
            tvh_context_close(self->context, flush);
        }
        self->index = -1;
    }
}


int
tvh_stream_handle(TVHStream *self, th_pkt_t *pkt)
{
    if (pkt->pkt_payload && self->context) {
        return tvh_context_handle(self->context, pkt);
    }
    pkt_ref_inc(pkt);
    return tvh_transcoder_deliver(self->transcoder, pkt);
}


int
tvh_stream_deliver(TVHStream *self, th_pkt_t *pkt)
{
    pkt->pkt_componentindex = self->index;
    return tvh_transcoder_deliver(self->transcoder, pkt);
}


TVHStream *
tvh_stream_create(TVHTranscoder *transcoder, TVHCodecProfile *profile,
                  tvh_ssc_t *ssc, const char *src_codecs)
{
    TVHStream *self = NULL;
    int is_copy = -1;

    if (!(self = calloc(1, sizeof(TVHStream)))) {
        tvh_ssc_log(ssc, LOG_ERR, "failed to allocate stream", transcoder);
        return NULL;
    }
    self->transcoder = transcoder;
    self->id = self->index = ssc->es_index;
    self->type = ssc->es_type;
    if ((is_copy = tvh_stream_is_copy(profile, ssc, src_codecs)) > 0) {
        self->is_copy = 1;
        if (ssc->ssc_gh) {
            pktbuf_ref_inc(ssc->ssc_gh);
        }
    }
    else if (is_copy < 0 || tvh_stream_setup(self, profile, ssc)) {
        tvh_stream_destroy(self);
        return NULL;
    }
    return self;
}


void
tvh_stream_destroy(TVHStream *self)
{
    if (self) {
        tvh_stream_stop(self, 0);
        if (self->context) {
            tvh_context_destroy(self->context);
            self->context = NULL;
        }
        self->transcoder = NULL;
        free(self);
        self = NULL;
    }
}
