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
    enum AVSampleFormat altfmt = AV_SAMPLE_FMT_NONE;
    int i;

    if (!tvh_context_get_int_opt(opts, "sample_fmt", &ofmt) &&
        ofmt == AV_SAMPLE_FMT_NONE) {
        if (sample_fmts) {
            for (i = 0; (ofmt = sample_fmts[i]) != AV_SAMPLE_FMT_NONE; i++) {
                if (ofmt == ifmt) {
                    break;
                }
            }
            altfmt = (ofmt != AV_SAMPLE_FMT_NONE) ? ofmt : sample_fmts[0];
        }
        else {
            altfmt = ifmt;
        }
    }
    if (tvhtrace_enabled())
      tvh_context_log(self, LOG_TRACE, "audio format selection: old %s, alt %s, in %s",
                      av_get_sample_fmt_name(ofmt),
                      av_get_sample_fmt_name(altfmt),
                      av_get_sample_fmt_name(ifmt));
    return ofmt == AV_SAMPLE_FMT_NONE ? altfmt : ofmt;
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
    if (tvhtrace_enabled())
      tvh_context_log(self, LOG_TRACE, "audio rate selection: old %i, alt %i, in %i",
                      orate, altrate, irate);
    return orate ? orate : (altrate ? altrate : 48000);
}


#if LIBAVCODEC_VERSION_MAJOR > 59
/**
 * Build the libavfilter chain string used between @c abuffer and @c abuffersink
 * for FFmpeg/libavcodec major version &gt; 59.
 *
 * The result is a comma-separated list of filter descriptions allocated into
 * @p *filters (caller must free via whatever owns the graph string lifecycle).
 *
 * Pieces (in join order):
 * - @c asetpts=N/SR/TB — normalizes PTS on the audio link (sample rate / time base).
 * - Optional @c aresample=@<output_sample_rate@> when input and output sample rates
 *   differ (in this source the condition is written as never taken; adjust if you
 *   re-enable rate mismatch handling here).
 * - Optional @c aformat=… when input and output differ in @c sample_fmt and/or
 *   channel count: adds @c sample_fmts=… and/or @c channel_layouts=… as required
 *   by the @c aformat filter.
 *
 * @param self    Transcode context; uses @c iavctx / @c oavctx sample format, rate,
 *                and channel layout to decide which segments are needed.
 * @param filters Receives a newly built filter string (e.g. from @c str_join), or
 *                is left untouched on failure.
 * @return        0 on success; -1 if a buffer would overflow or allocation/join fails.
 */
static int
_audio_filters_get_filters(TVHContext *self, char **filters)
{
    char sample_rate[32];
    char sample_fmt[32];
    char obuf[64];
    char channel_layouts[64];
    char aformat[128];
    char asetpts[24];
    int aformat_tokens = 0;

    // sample_rate
    memset(sample_rate, 0, sizeof(sample_rate));
    if ((self->iavctx->sample_rate != self->oavctx->sample_rate) &&
        str_snprintf(sample_rate, sizeof(sample_rate), "aresample=%d",

                    self->oavctx->sample_rate))
        return -1;    

    // https://ayosec.github.io/ffmpeg-filters-docs/8.1/Filters/Audio/aformat.html
    // sample_fmt
    memset(sample_fmt, 0, sizeof(sample_fmt));
    if (self->iavctx->sample_fmt != self->oavctx->sample_fmt) {
        if(str_snprintf(sample_fmt, sizeof(sample_fmt), "sample_fmts=%s",
                     av_get_sample_fmt_name(self->oavctx->sample_fmt)))
            return -1;
        else 
            aformat_tokens++; 
    }

    // channel_layouts
    memset(channel_layouts, 0, sizeof(channel_layouts));
    if (self->iavctx->ch_layout.nb_channels != self->oavctx->ch_layout.nb_channels) {
        av_channel_layout_describe(&self->oavctx->ch_layout, obuf, sizeof(obuf));
        if(str_snprintf(channel_layouts, sizeof(channel_layouts), "channel_layouts=%s", obuf)) 
            return -1;
        else 
            aformat_tokens++; 
    }

    memset(aformat, 0, sizeof(aformat));
    switch (aformat_tokens) {
        case 2:
            // we have to update both settings
            if(str_snprintf(aformat, sizeof(aformat), "aformat=%s:%s", sample_fmt, channel_layouts))
                return -1;
            break;
        case 1:
            // we have to update only one setting
            if(str_snprintf(aformat, sizeof(aformat), "aformat=%s%s", sample_fmt, channel_layouts)) 
                return -1;
            break;
        default:
            break;
    }

    memset(asetpts, 0, sizeof(asetpts));
    // we have to timestamp
    if(str_snprintf(asetpts, sizeof(asetpts), "asetpts=N/SR/TB"))
        return -1;

    // context filters
    if (!(*filters = str_join(",", asetpts, sample_rate, aformat, NULL)))
        return -1;
    
    return 0;
}

static void
_audio_context_channel_layout(TVHContext *self, AVDictionary **opts, AVChannelLayout *dst)
{
    const AVChannelLayout *channel_layouts = tvh_codec_profile_audio_get_channel_layouts(self->profile);
    AVChannelLayout ilayout = {0};
    AVChannelLayout olayout;
    av_channel_layout_default(&olayout, 0);
    AVChannelLayout altlayout;
    av_channel_layout_default(&altlayout, 0);
    int ch_layout_u_mask = 0, i = 0;
    char obuf[64], abuf[64], ibuf[64];
    // setup ilayout
    if (av_channel_layout_copy(&ilayout, &self->iavctx->ch_layout)){
        tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from input stream");
        goto _audio_context_channel_layout_exit;
    }
    // setup olayout
    if (!tvh_context_get_int_opt(opts, "ch_layout_u_mask", &ch_layout_u_mask) &&
        // only if value selected is not auto
        ch_layout_u_mask &&
        av_channel_layout_from_mask(&olayout, ch_layout_u_mask)){
        tvh_context_log(self, LOG_ERR, "unable to initialize output layout from bitmask");
        goto _audio_context_channel_layout_exit;
    }

    if (!channel_layouts) {
        tvh_context_log(self, LOG_ERR, "channel_layouts is not available");
        if (av_channel_layout_copy(&olayout, &ilayout)){
            tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from input layout to output layout");
            goto _audio_context_channel_layout_exit;
        }
    }
    // match input with output based on number of channels
    if (olayout.nb_channels >= ilayout.nb_channels) {
        // input layout will fit in output layout
        if (av_channel_layout_copy(&olayout, &ilayout)){
            tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from input layout to output layout");
            goto _audio_context_channel_layout_exit;
        }
    }
    // order of selection: 
    // 1. try olayout
    if (olayout.order == AV_CHANNEL_ORDER_UNSPEC){
        // prepare the alternative layout
        while (channel_layouts && channel_layouts[i].nb_channels != 0) {
            if (channel_layouts[i].nb_channels <= ilayout.nb_channels) {
                //altlayout = channel_layouts[i];
                if (av_channel_layout_copy(&altlayout, &channel_layouts[i])){
                    tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from channel_layouts to alternative layout");
                    goto _audio_context_channel_layout_exit;
                }
            }
            i++;
        }
        // 2. try altlayout
        if (altlayout.order == AV_CHANNEL_ORDER_UNSPEC){
            // 3. default to stereo
            if (av_channel_layout_copy(&olayout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO)){
                tvh_context_log(self, LOG_ERR, "unable to setup ch_layout stereo layout");
            }
        }
        else
            // use altlayout
            if (av_channel_layout_copy(&olayout, &altlayout)){
                tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from alternative layout to output layout");
            }
    }

_audio_context_channel_layout_exit:
    // logging
    if (tvhtrace_enabled()) {
        strcpy(obuf, "none");
        av_channel_layout_describe(&olayout, obuf, sizeof(obuf));
        strcpy(abuf, "none");
        av_channel_layout_describe(&altlayout, abuf, sizeof(abuf));
        strcpy(ibuf, "none");
        av_channel_layout_describe(&ilayout, ibuf, sizeof(ibuf));
        tvh_context_log(self, LOG_TRACE, "audio layout selection: in %s, alt %s, out %s",
                                                                     ibuf,   abuf,   obuf);
    }
    if (av_channel_layout_copy(dst, &olayout)){
        tvh_context_log(self, LOG_ERR, "unable to copy ch_layout from output layout to destination layout");
    }
    // clean
    av_channel_layout_uninit(&ilayout);
    av_channel_layout_uninit(&altlayout);
    av_channel_layout_uninit(&olayout);
}
#else
static uint64_t
_audio_context_channel_layout(TVHContext *self, AVDictionary **opts)
{
    const uint64_t *channel_layouts =
        tvh_codec_profile_audio_get_channel_layouts(self->profile);
    uint64_t ilayout = self->iavctx->channel_layout;
    uint64_t altlayout = 0, olayout = 0;
    int tmp = 0, ichannels = 0, i;
    char obuf[64], abuf[64], ibuf[64];

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
    if (tvhtrace_enabled()) {
        strcpy(obuf, "none");
        av_get_channel_layout_string(obuf, sizeof(obuf), 0, olayout);
        strcpy(abuf, "none");
        av_get_channel_layout_string(abuf, sizeof(abuf), 0, altlayout);
        strcpy(ibuf, "none");
        av_get_channel_layout_string(ibuf, sizeof(ibuf), 0, ilayout);
        tvh_context_log(self, LOG_TRACE, "audio layout selection: old %s, alt %s, in %s",
                        obuf, abuf, ibuf);
    }
    return olayout ? olayout : (altlayout ? altlayout : AV_CH_LAYOUT_STEREO);
}
#endif


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
#if LIBAVCODEC_VERSION_MAJOR > 59
    _audio_context_channel_layout(self, opts, &self->oavctx->ch_layout);
    if (self->oavctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        tvh_context_log(self, LOG_ERR,
                        "audio encoder has no suitable channel layout");
        return -1;
    }
#else
    self->oavctx->channel_layout = _audio_context_channel_layout(self, opts);
    if (!self->oavctx->channel_layout) {
        tvh_context_log(self, LOG_ERR,
                        "audio encoder has no suitable channel layout");
        return -1;
    }
    self->oavctx->channels =
        av_get_channel_layout_nb_channels(self->oavctx->channel_layout);
#endif
    return 0;
}


static int
tvh_audio_context_open_filters(TVHContext *self, AVDictionary **opts)
{
#if LIBAVCODEC_VERSION_MAJOR > 59
    char *filters2 = NULL;

    // filters
    if (_audio_filters_get_filters(self, &filters2)) {
        tvherror(LS_TRANSCODE, "filters: audio: function _audio_filters_get_filters() returned with error.");
        str_clear(filters2);
        return -1;
    }
    
    tvhinfo(LS_TRANSCODE, "filters: audio: '%s'", filters2);
#endif
    char source_args[128];
    char filters[16];
    char layout[32];
    int resample = (self->iavctx->sample_rate != self->oavctx->sample_rate);

    // source args
    memset(source_args, 0, sizeof(source_args));
#if LIBAVCODEC_VERSION_MAJOR > 59
    av_channel_layout_describe(&self->iavctx->ch_layout, layout, sizeof(layout));
#else
    av_get_channel_layout_string(layout, sizeof(layout), self->iavctx->channels, self->iavctx->channel_layout);
#endif
    if (str_snprintf(source_args, sizeof(source_args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
            self->iavctx->time_base.num,
            self->iavctx->time_base.den,
            self->iavctx->sample_rate,
            av_get_sample_fmt_name(self->iavctx->sample_fmt),
            layout)) {
#if LIBAVCODEC_VERSION_MAJOR > 59
        str_clear(filters2);
#endif
        return -1;
    }

    // resample also if there should be a format conversion
    if (self->iavctx->sample_fmt != self->oavctx->sample_fmt)
        resample = 1;

    // context filters
    memset(filters, 0, sizeof(filters));
    if (str_snprintf(filters, sizeof(filters), "%s",
                     (resample) ? "aresample" : "anull")) {
#if LIBAVCODEC_VERSION_MAJOR > 59
        str_clear(filters2);
#endif
        return -1;
    }

#if LIBAVCODEC_VERSION_MAJOR > 59
    char ch_layout[64];
    av_channel_layout_describe(&self->oavctx->ch_layout, ch_layout, sizeof(ch_layout));
    int ret = -1;
    if (self->profile->has_support_for_filter2)
        ret = tvh_context_open_filters2(self,
            "abuffer",                                             // source
            (filters2 && filters2[0] != '\0') ? filters2 : "anull",// filters
            "abuffersink"                                          // sink
        );
    else 
        ret = tvh_context_open_filters(self,
            "abuffer", source_args,                           // source
            filters,                                          // filters
            "abuffersink",                                    // sink
            "ch_layouts",   AV_OPT_SET_STRING,                // sink option: channel_layout
            sizeof(ch_layout),
            ch_layout,
            "sample_fmts",  AV_OPT_SET_BIN,                   // sink option: sample_fmt
            sizeof(self->oavctx->sample_fmt),
            &self->oavctx->sample_fmt,
            "sample_rates", AV_OPT_SET_BIN,                   // sink option: sample_rate
            sizeof(self->oavctx->sample_rate),
            &self->oavctx->sample_rate,
            NULL);                                            // _IMPORTANT!_
    str_clear(filters2);
#else
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
#endif
    return ret;
}

/**
 * Audio half of the transcoder open state machine: dispatches @p phase to the
 * right setup step for the audio path.
 *
 * Handled phases:
 * - PREPARE_ENCODER — set output sample format/rate/time base, channel layout,
 *   and other @c AVCodecContext fields before filters/encoder open
 *   (@c tvh_audio_context_open_encoder).
 * - OPEN_FILTERS — build the libavfilter graph (@c abuffer → filters → @c abuffersink),
 *   including resample/aformat when needed (@c tvh_audio_context_open_filters).
 * - OPEN_ENCODER — runs after @c avcodec_open2 on the encoder in @c tvh_context_open:
 *   refuses @c frame_size == 0, then fills @c self->delta (PTS step per encoded
 *   frame in @c iavctx time base) and sets @c abuffersink frame size to match
 *   the encoder frame size.
 *
 * Phases such as PREPARE_DECODER, OPEN_DECODER, and NOTIFY_GLOBALHEADER are not
 * handled here; @c tvh_context_open invokes this callback only for encoder-side
 * steps that the audio context owns. Unhandled @p phase is a no-op (returns 0).
 *
 * @param self  Transcode context (shared with generic open in @c context.c).
 * @param phase Which open step is being executed (@c TVHOpenPhase).
 * @param opts  Codec/profile options (used by @c PREPARE_ENCODER).
 * @return      0 on success or no-op; negative @c AVERROR (e.g. @c EINVAL) if
 *              the encoder reports @c frame_size == 0 in @c OPEN_ENCODER, or
 *              any error returned by the encoder/filter helpers.
 */
static int
tvh_audio_context_open(TVHContext *self, TVHOpenPhase phase, AVDictionary **opts)
{
    switch (phase) {
        case PREPARE_ENCODER:
            return tvh_audio_context_open_encoder(self, opts);
        case OPEN_FILTERS:
            return tvh_audio_context_open_filters(self, opts);
        case OPEN_ENCODER:
            self->delta = av_rescale_q_rnd(self->oavctx->frame_size,
                                           self->oavctx->time_base,
                                           self->iavctx->time_base,
                                           AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            av_buffersink_set_frame_size(self->oavfltctx, self->oavctx->frame_size);
            break;
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
    // only if packet duration is valid we passed on
    if (avpkt->duration > 0)
        pkt->pkt_duration = avpkt->duration;
#if LIBAVCODEC_VERSION_MAJOR > 59
    pkt->a.pkt_channels = self->oavctx->ch_layout.nb_channels;
#else
    pkt->a.pkt_channels = self->oavctx->channels;
#endif
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
