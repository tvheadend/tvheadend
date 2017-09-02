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


static enum AVSampleFormat
_audio_context_sample_fmt(TVHContext *self, AVDictionary **opts)
{
    const enum AVSampleFormat *sample_fmts =
        tvh_codec_profile_audio_get_sample_fmts(self->profile);
    enum AVSampleFormat ifmt = self->iavctx->sample_fmt;
    enum AVSampleFormat ofmt = AV_SAMPLE_FMT_NONE;
    int i;

    if (!tvh_context_get_int_opt(opts, "sample_fmt", &ofmt) &&
        ofmt == AV_SAMPLE_FMT_NONE) {
        if (sample_fmts) {
            for (i = 0; (ofmt = sample_fmts[i]) != AV_SAMPLE_FMT_NONE; i++) {
                if (ofmt == ifmt) {
                    break;
                }
            }
            ofmt = (ofmt != AV_SAMPLE_FMT_NONE) ? ofmt : sample_fmts[0];
        }
        else {
            ofmt = ifmt;
        }
    }
    return ofmt;
}


static int
_audio_context_sample_rate(TVHContext *self, AVDictionary **opts)
{
    const int *sample_rates =
        tvh_codec_profile_audio_get_sample_rates(self->profile);
    int irate = self->iavctx->sample_rate, altrate = 0, orate = 0, i;

    if (!tvh_context_get_int_opt(opts, "sample_rate", &orate) &&
        !orate && sample_rates) {
        for (i = 0; (orate = sample_rates[i]); i++) {
            if (orate == irate) {
                break;
            }
            if (orate < irate) {
                altrate = orate;
            }
        }
    }
    return orate ? orate : (altrate ? altrate : 48000);
}


static uint64_t
_audio_context_channel_layout(TVHContext *self, AVDictionary **opts)
{
    const uint64_t *channel_layouts =
        tvh_codec_profile_audio_get_channel_layouts(self->profile);
    uint64_t ilayout = self->iavctx->channel_layout;
    uint64_t altlayout = 0, olayout = 0;
    int tmp = 0, ichannels = 0, i;


    if (!tvh_context_get_int_opt(opts, "channel_layout", &tmp) &&
        !(olayout = tmp) && channel_layouts) {
        ichannels = av_get_channel_layout_nb_channels(ilayout);
        for (i = 0; (olayout = channel_layouts[i]); i++) {
            if (olayout == ilayout) {
                break;
            }
            if (av_get_channel_layout_nb_channels(olayout) <= ichannels) {
                altlayout = olayout;
            }
        }
    }
    return olayout ? olayout : (altlayout ? altlayout : AV_CH_LAYOUT_STEREO);
}


static int
tvh_audio_context_open_encoder(TVHContext *self, AVDictionary **opts)
{
    // XXX: is this a safe assumption?
    if (!self->iavctx->time_base.den) {
        self->iavctx->time_base = av_make_q(1, 90000);
    }
    // sample_fmt
    self->oavctx->sample_fmt = _audio_context_sample_fmt(self, opts);
    if (self->oavctx->sample_fmt == AV_SAMPLE_FMT_NONE) {
        tvh_context_log(self, LOG_ERR,
                        "audio encoder has no suitable sample format");
        return -1;
    }
    // sample_rate
    self->oavctx->sample_rate = _audio_context_sample_rate(self, opts);
    if (!self->oavctx->sample_rate) {
        tvh_context_log(self, LOG_ERR,
                        "audio encoder has no suitable sample rate");
        return -1;
    }
    self->oavctx->time_base = av_make_q(1, self->oavctx->sample_rate);
    self->sri = rate_to_sri(self->oavctx->sample_rate);
    // channel_layout
    self->oavctx->channel_layout = _audio_context_channel_layout(self, opts);
    if (!self->oavctx->channel_layout) {
        tvh_context_log(self, LOG_ERR,
                        "audio encoder has no suitable channel layout");
        return -1;
    }
    self->oavctx->channels =
        av_get_channel_layout_nb_channels(self->oavctx->channel_layout);
    return 0;
}


static int
tvh_audio_context_open_filters(TVHContext *self, AVDictionary **opts)
{
    char source_args[128];
    char filters[16];
    int resample = (self->iavctx->sample_rate != self->oavctx->sample_rate);

    // source args
    memset(source_args, 0, sizeof(source_args));
    if (str_snprintf(source_args, sizeof(source_args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%d:channel_layout=0x%"PRIx64,
            self->iavctx->time_base.num,
            self->iavctx->time_base.den,
            self->iavctx->sample_rate,
            self->iavctx->sample_fmt,
            self->iavctx->channel_layout)) {
        return -1;
    }

    // resample also if there should be a format conversion
    if (self->iavctx->sample_fmt != self->oavctx->sample_fmt)
        resample = 1;

    // context filters
    memset(filters, 0, sizeof(filters));
    if (str_snprintf(filters, sizeof(filters), "%s",
                     (resample) ? "aresample" : "anull")) {
        return -1;
    }

    int ret = tvh_context_open_filters(self,
        "abuffer", source_args,                           // source
        filters,                                          // filters
        "abuffersink",                                    // sink
        "channel_layouts", &self->oavctx->channel_layout, // sink option: channel_layout
        sizeof(self->oavctx->channel_layout),
        "sample_fmts", &self->oavctx->sample_fmt,         // sink option: sample_fmt
        sizeof(self->oavctx->sample_fmt),
        "sample_rates", &self->oavctx->sample_rate,       // sink option: sample_rate
        sizeof(self->oavctx->sample_rate),
        NULL);                                            // _IMPORTANT!_
    if (!ret) {
        av_buffersink_set_frame_size(self->oavfltctx, self->oavctx->frame_size);
    }
    return ret;
}


static int
tvh_audio_context_open(TVHContext *self, TVHOpenPhase phase, AVDictionary **opts)
{
    switch (phase) {
        case OPEN_ENCODER:
            return tvh_audio_context_open_encoder(self, opts);
        case OPEN_ENCODER_POST:
            self->delta = av_rescale_q_rnd(self->oavctx->frame_size,
                                           self->oavctx->time_base,
                                           self->iavctx->time_base,
                                           AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            return tvh_audio_context_open_filters(self, opts);
        default:
            break;
    }
    return 0;
}


static int
tvh_audio_context_decode(TVHContext *self, AVPacket *avpkt)
{
    int64_t prev_pts = self->pts;
    int64_t new_pts = avpkt->pts - self->duration;

    // rounding error?
    if (new_pts - 10 <= prev_pts && new_pts + 10 >= prev_pts) {
      prev_pts = new_pts;
    }

    if (prev_pts != (self->pts = new_pts) && prev_pts > 12000) {
        tvh_context_log(self, LOG_WARNING, "Detected framedrop in audio (%"PRId64" != %"PRId64")", prev_pts, new_pts);
    }
    self->duration += avpkt->duration;
    return 0;
}


static int
tvh_audio_context_encode(TVHContext *self, AVFrame *avframe)
{
    avframe->nb_samples = self->oavctx->frame_size;
    avframe->pts = self->pts;
    self->pts += self->delta;
    self->duration -= self->delta;
    return 0;
}


static int
tvh_audio_context_ship(TVHContext *self, AVPacket *avpkt)
{
    return (avpkt->pts >= 0);
}


static int
tvh_audio_context_wrap(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{
    pkt->pkt_duration   = avpkt->duration;
    pkt->a.pkt_channels = self->oavctx->channels;
    pkt->a.pkt_sri      = self->sri;
    return 0;
}


TVHContextType TVHAudioContext = {
    .media_type = AVMEDIA_TYPE_AUDIO,
    .open       = tvh_audio_context_open,
    .decode     = tvh_audio_context_decode,
    .encode     = tvh_audio_context_encode,
    .ship       = tvh_audio_context_ship,
    .wrap       = tvh_audio_context_wrap,
};
