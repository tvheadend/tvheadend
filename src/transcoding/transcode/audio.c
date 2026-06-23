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
// TODO: remove late on
#include "packet.h"


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
static int
_audio_encoder_frame_size(TVHContext *self)
{
    const char *name;
    if (!self->oavctx->codec ||
        (self->oavctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE))
        return 0;
    if (self->oavctx->frame_size > 0)
        return self->oavctx->frame_size;
    /* OPEN_FILTERS runs before avcodec_open2(); use known fixed frame sizes. */
    name = self->oavctx->codec->name;
    if (!name)
        return 0;
    if (!strcmp(name, "aac") || !strcmp(name, "libfdk_aac"))
        return 1024;
    if (!strcmp(name, "mp2"))
        return 1152;
    return 0;
}

static int
_audio_ch_layout_differs(TVHContext *self)
{
    if (self->iavctx->ch_layout.nb_channels != self->oavctx->ch_layout.nb_channels)
        return 1;
    if (self->iavctx->ch_layout.nb_channels &&
        self->iavctx->ch_layout.u.mask != self->oavctx->ch_layout.u.mask)
        return 1;
    return 0;
}

static int
_audio_needs_reframe_asetpts(TVHContext *self)
{
    int out_fs = _audio_encoder_frame_size(self);
    int in_fs  = self->iavctx->frame_size;
    if (out_fs <= 0)
        return 0;
    if (in_fs <= 0)
        return 1;
    return in_fs != out_fs;
}

static int
_audio_needs_timebase_asetpts(TVHContext *self)
{
    AVRational flt_tb;

    if (!self->profile->has_support_for_filter2)
        return 0;

    flt_tb = av_make_q(1, self->iavctx->sample_rate);
    return av_cmp_q(self->iavctx->time_base, flt_tb) != 0;
}

static int
_audio_requires_asetpts(TVHContext *self)
{
    TVHAudioCodecProfile *ap = (TVHAudioCodecProfile *)self->profile;

    if (ap->asetpts == 2)          /* force off */
        return 0;
    if (ap->asetpts == 1)          /* force on */
        return 1;
    /* ap->asetpts == 0  →  "auto" */
    return _audio_needs_reframe_asetpts(self)
        || (self->iavctx->sample_rate != self->oavctx->sample_rate)
        || (self->iavctx->sample_fmt != self->oavctx->sample_fmt)
        || _audio_ch_layout_differs(self)
        || _audio_needs_timebase_asetpts(self);
}

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
        str_snprintf(sample_rate, sizeof(sample_rate), "aresample=%d", self->oavctx->sample_rate))
        return -1;    

    // https://ayosec.github.io/ffmpeg-filters-docs/8.1/Filters/Audio/aformat.html
    // sample_fmt
    memset(sample_fmt, 0, sizeof(sample_fmt));
    if (self->iavctx->sample_fmt != self->oavctx->sample_fmt) {
        if(str_snprintf(sample_fmt, sizeof(sample_fmt), "sample_fmts=%s", av_get_sample_fmt_name(self->oavctx->sample_fmt)))
            return -1;
        else 
            aformat_tokens++; 
    }

    // channel_layouts
    memset(channel_layouts, 0, sizeof(channel_layouts));
    // if channel number is different OR
    //    channel number is the same but the layout is different
    // NOTE: this is required to force conversion from 5.1(side) to 5.1(back) or vice versa
    if (_audio_ch_layout_differs(self)) {
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
    if (_audio_requires_asetpts(self) &&
        // normalizes PTS on the audio link (sample rate / time base) by counting samples.
        str_snprintf(asetpts, sizeof(asetpts), "asetpts=N/SR/TB"))
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
    if (!ret && self->oavctx->frame_size > 0) {
        // TODO : remove later on
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
 * - PREPARE_ENCODER — configure output @c AVCodecContext (sample format, rate,
 *   time base, channel layout) via @c tvh_audio_context_open_encoder.
 * - OPEN_FILTERS — build the libavfilter graph (@c abuffer → filters →
 *   @c abuffersink), including @c asetpts / @c aresample / @c aformat when needed
 *   (@c tvh_audio_context_open_filters).
 * - OPEN_ENCODER — post @c avcodec_open2 hook: for fixed-frame encoders only,
 *   sets @c abuffersink frame size to @c oavctx->frame_size when @c frame_size > 0.
 *   Variable-frame codecs (e.g. Opus, Vorbis) are left to the filter graph.
 *
 * Decoder-side phases (@c PREPARE_DECODER, @c OPEN_DECODER, @c NOTIFY_GLOBALHEADER)
 * are handled in @c context.c, not here. Unknown @p phase is a no-op (returns 0).
 *
 * @param self  Transcode context.
 * @param phase Open step (@c TVHOpenPhase).
 * @param opts  Codec/profile options (used by @c PREPARE_ENCODER).
 * @return      0 on success or no-op; negative @c AVERROR from
 *              @c tvh_audio_context_open_encoder or @c tvh_audio_context_open_filters.
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
            if (!(self->oavctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) &&
                (self->oavctx->frame_size > 0)){
                av_buffersink_set_frame_size(self->oavfltctx, self->oavctx->frame_size);
            }
            break;
        default:
            break;
    }
    return 0;
}


static int
tvh_audio_context_decode(TVHContext *self, AVPacket *avpkt)
{
    //tvh_context_log(self, LOG_WARNING, "AVPacket->pts = %"PRId64" ", avpkt->pts);
    int64_t prev_pts = self->pts;
    int64_t new_pts = avpkt->pts - self->duration;
    
    int64_t drift = new_pts - prev_pts;

    if (drift < 0)
        drift = -drift;

    // Tolerance: 3 AC3 frames at 48 kHz (3 × 1536 = 4608 samples).
    // IPTV streams often have slightly irregular packet durations causing
    // sub-frame PTS drift of up to 2880 samples (2 AC3 frames).  Rather than
    // hard-jumping self->pts on every small deviation (which produces an
    // audible skip), absorb deviations up to 3 frames by correcting
    // self->duration instead, keeping the clock continuous.  Only log +
    // hard-reset for genuine discontinuities larger than that.
    // Small drift: re-anchor duration, keep PTS (AC3 etc.)
    if (prev_pts && drift <= 4608) {
        self->duration = avpkt->pts - prev_pts + avpkt->duration;
        self->pts = prev_pts;
        return 0;
    }

    // Real problems only — not filter vs packet offset
    if (prev_pts > 0) {
        if (new_pts < prev_pts - 4608) {
            tvh_context_log(self, LOG_WARNING, "Audio PTS went backward (%" PRId64 " -> %" PRId64 ")", prev_pts, new_pts);
        } else if (new_pts - prev_pts > 90000 * 2) {  // > 2 s gap @ 90 kHz
            tvh_context_log(self, LOG_WARNING, "Audio discontinuity (%" PRId64 " -> %" PRId64 "), gap=%" PRId64, prev_pts, new_pts, new_pts - prev_pts);
        }
    }
    self->pts = new_pts;
    self->duration += avpkt->duration;
    return 0;
}


static int
tvh_audio_context_encode(TVHContext *self, AVFrame *avframe)
{
    AVRational src_time_base;
    int64_t flt_pts;
    int64_t consumed;
    if (!self || !self->oavctx || !avframe)
        return -1;
    // Filter output timebase (same idea as tvh_video_context_encode)
#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (self->oavfltctx && self->oavfltctx->nb_inputs > 0) {
        src_time_base = self->oavfltctx->inputs[0]->time_base;
    } else
    {
#endif
        // abuffer is configured with iavctx->time_base in open_filters()
        src_time_base = self->iavctx->time_base;
        if (!src_time_base.den)
            src_time_base = av_make_q(1, 90000);
#if LIBAVUTIL_VERSION_MAJOR >= 58
    }
#endif
    if (tvhtrace_enabled()) {
        tvh_context_log(self, LOG_TRACE,
            "Filter frame: pts=%" PRId64 ", nb_samples=%d, time_base=%d/%d",
            avframe->pts, avframe->nb_samples,
            src_time_base.num, src_time_base.den);
    }
    // Use PTS from aresample/abuffersink; fallback only if missing
    // Only if there is no last_flt_pts yet (e.g. first frame), fall back to decode self->pts.
    flt_pts = avframe->pts;
    if (flt_pts == AV_NOPTS_VALUE) {
        if (self->last_flt_pts && avframe->nb_samples > 0) {
            flt_pts = self->last_flt_pts + av_rescale_q(avframe->nb_samples, av_make_q(1, self->oavctx->sample_rate), src_time_base);
        } else if (self->pts != AV_NOPTS_VALUE && self->pts != 0) {
            flt_pts = self->pts;  // first frame: decode packet timeline
        } else {
            return AVERROR(EINVAL);
        }
        avframe->pts = flt_pts;
    }
    //check for duplicate PTS from the filter
    if (flt_pts != AV_NOPTS_VALUE && self->last_flt_pts &&
        flt_pts <= self->last_flt_pts)
        return AVERROR(EAGAIN);
    // Track last PTS in filter/input time_base
    self->last_flt_pts = flt_pts;
    // Rescale PTS: filter time_base -> encoder time_base (1/sample_rate)
    if (av_cmp_q(src_time_base, self->oavctx->time_base) != 0) {
        avframe->pts = av_rescale_q(flt_pts, src_time_base, self->oavctx->time_base);
    } else {
        avframe->pts = flt_pts;
    }
#if LIBAVUTIL_VERSION_MAJOR >= 58
    if (avframe->duration > 0 &&
        av_cmp_q(src_time_base, self->oavctx->time_base) != 0) {
        avframe->duration = av_rescale_q(avframe->duration, src_time_base, self->oavctx->time_base);
    }
#else
    if (avframe->pkt_duration > 0 &&
        av_cmp_q(src_time_base, self->oavctx->time_base) != 0) {
        avframe->pkt_duration = av_rescale_q(avframe->pkt_duration, src_time_base, self->oavctx->time_base);
    }
#endif
    if (tvhtrace_enabled()) {
        tvh_context_log(self, LOG_TRACE,
            "Rescaled audio {%d/%d}->{%d/%d}: pts=%" PRId64 ", nb_samples=%d",
            src_time_base.num, src_time_base.den,
            self->oavctx->time_base.num, self->oavctx->time_base.den,
            avframe->pts, avframe->nb_samples);
    }
    // Duration bookkeeping in input time_base, from actual samples
    if (avframe->nb_samples > 0 && self->iavctx->time_base.den) {
        consumed = av_rescale_q(avframe->nb_samples, av_make_q(1, self->oavctx->sample_rate), self->iavctx->time_base);
        if (consumed > 0)
            self->duration -= consumed;
    }
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
    static const AVRational mpeg_ts_tb = { 1, 90000 };
    if (avpkt->pts != AV_NOPTS_VALUE)
        pkt->pkt_pts = av_rescale_q(avpkt->pts,
                                    self->oavctx->time_base,
                                    mpeg_ts_tb);
    if (avpkt->dts != AV_NOPTS_VALUE)
        pkt->pkt_dts = av_rescale_q(avpkt->dts,
                                    self->oavctx->time_base,
                                    mpeg_ts_tb);
    if (avpkt->duration > 0)
        pkt->pkt_duration = av_rescale_q(avpkt->duration,
                                         self->oavctx->time_base,
                                         mpeg_ts_tb);
#if LIBAVCODEC_VERSION_MAJOR > 59
    pkt->a.pkt_channels = self->oavctx->ch_layout.nb_channels;
#else
    pkt->a.pkt_channels = self->oavctx->channels;
#endif
    pkt->a.pkt_sri      = self->sri;
// TODO remove
#if 0  /* set to 0 to disable audio PTS debug */
    {
        static uint32_t aud_ts_log_cnt;
        char ptsb[24], dtsb[24], durpb[24];
        int64_t delta_pts = 0;
        static int64_t last_pkt_pts;

        if ((++aud_ts_log_cnt % 25) == 0) {
            if (last_pkt_pts != PTS_UNSET && pkt->pkt_pts != PTS_UNSET)
                delta_pts = pkt->pkt_pts - last_pkt_pts;
            if (pkt->pkt_pts != PTS_UNSET)
                last_pkt_pts = pkt->pkt_pts;

            tvh_context_log(self, LOG_ERR,
                "audio wrap [%s]: avpkt pts=%" PRId64 " dts=%" PRId64
                " dur=%" PRId64 " (enc tb %d/%d) ->"
                " pkt_pts=%s pkt_dts=%s pkt_duration=%s"
                " delta_pts=%" PRId64 " size=%d",
                self->oavctx->codec ? self->oavctx->codec->name : "?",
                avpkt->pts, avpkt->dts, avpkt->duration,
                self->oavctx->time_base.num, self->oavctx->time_base.den,
                pts_to_string(pkt->pkt_pts, ptsb),
                pts_to_string(pkt->pkt_dts, dtsb),
                pts_to_string(pkt->pkt_duration, durpb),
                delta_pts, avpkt->size);
        }
    }
#endif
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
