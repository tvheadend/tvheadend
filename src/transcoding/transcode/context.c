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

#if LIBAVCODEC_VERSION_MAJOR > 59
#include <libavutil/avstring.h>
#endif

#include <libavutil/opt.h>


#define TVH_BUFFERSRC_FLAGS AV_BUFFERSRC_FLAG_PUSH | AV_BUFFERSRC_FLAG_KEEP_REF
#define TVH_BUFFERSINK_FLAGS AV_BUFFERSINK_FLAG_NO_REQUEST


/* TVHContextType =========================================================== */

extern TVHContextType TVHVideoContext;
extern TVHContextType TVHAudioContext;


SLIST_HEAD(TVHContextTypes, tvh_context_type);
static struct TVHContextTypes tvh_context_types;


static void
tvh_context_type_register(TVHContextType *self)
{
    if (self->media_type <= AVMEDIA_TYPE_UNKNOWN ||
        self->media_type >= AVMEDIA_TYPE_NB ||
        !self->ship) {
        tvherror(LS_TRANSCODE, "incomplete/wrong definition for context type");
        return;
    }
    SLIST_INSERT_HEAD(&tvh_context_types, self, link);
    tvhinfo(LS_TRANSCODE, "'%s' context type registered",
            av_get_media_type_string(self->media_type));
}


static TVHContextType *
tvh_context_type_find(enum AVMediaType media_type)
{
    TVHContextType *type = NULL;

    SLIST_FOREACH(type, &tvh_context_types, link) {
        if (type->media_type == media_type) {
            return type;
        }
    }
    return NULL;
}


void
tvh_context_types_register()
{
    SLIST_INIT(&tvh_context_types);
    tvh_context_type_register(&TVHVideoContext);
    tvh_context_type_register(&TVHAudioContext);
}


void
tvh_context_types_forget()
{
    tvhinfo(LS_TRANSCODE, "forgetting context types");
    while (!SLIST_EMPTY(&tvh_context_types)) {
        SLIST_REMOVE_HEAD(&tvh_context_types, link);
    }
}


/* TVHContext =============================================================== */

// wrappers

static void
_context_print_opts(TVHContext *self, AVDictionary *opts)
{
    char *buffer = NULL;

    if (opts) {
        av_dict_get_string(opts, &buffer, '=', ',');
        tvh_context_log(self, LOG_DEBUG, "opts: %s", buffer);
        av_freep(&buffer);
    }
}


static int
_context_filters_apply_sink_options(TVHContext *self, va_list ap)
{
    const char *opt_name = NULL;
    const uint8_t *opt_val = NULL;
#if LIBAVCODEC_VERSION_MAJOR > 59
    const char *opt_val_char = NULL;
    av_opt_set_type opt_type = AV_OPT_SET_UNKNOWN;
    char err_desciption[32];
#endif
    int opt_size = 0;
    int ret = -1;

    while ((opt_name = va_arg(ap, const char *))) {
#if LIBAVCODEC_VERSION_MAJOR > 59
        opt_type = (av_opt_set_type) va_arg(ap, int);
        opt_size = va_arg(ap, int);
        if (opt_type == AV_OPT_SET_BIN) {
            opt_val = va_arg(ap, const uint8_t *);
            ret = av_opt_set_bin(self->oavfltctx, opt_name, opt_val, opt_size, AV_OPT_SEARCH_CHILDREN);}
        else {
            if (opt_type == AV_OPT_SET_STRING) {
                opt_val_char = va_arg(ap, const char *);
                ret = av_opt_set(self->oavfltctx, opt_name, opt_val_char, AV_OPT_SEARCH_CHILDREN);} 
            else {
                tvh_context_log(self, LOG_ERR, "filters: failed to set option: '%s' with error: 'AV_OPT_SET_UNKNOWN'", opt_name);
                return ret;
            }
        }
        if (ret) {
            switch (ret) {
                case AVERROR_OPTION_NOT_FOUND:
                    str_snprintf(err_desciption, sizeof(err_desciption), "AVERROR_OPTION_NOT_FOUND");
                    break;
                case AVERROR(EINVAL):
                    str_snprintf(err_desciption, sizeof(err_desciption), "AVERROR(EINVAL)");
                    break;
                case AVERROR(ENOMEM):
                    str_snprintf(err_desciption, sizeof(err_desciption), "AVERROR(ENOMEM)");
                    break;
                default:
                    str_snprintf(err_desciption, sizeof(err_desciption), "UNKNOWN ERROR");
                    break;
            }
            tvh_context_log(self, LOG_ERR, "filters: failed to set option: '%s' with error: '%s'", opt_name, err_desciption);
        }
#else
        opt_val = va_arg(ap, const uint8_t *);
        opt_size = va_arg(ap, int);
        if ((ret = av_opt_set_bin(self->oavfltctx, opt_name, opt_val, opt_size,
                                  AV_OPT_SEARCH_CHILDREN))) {
            tvh_context_log(self, LOG_ERR, "filters: failed to set option: '%s'",
                            opt_name);
            break;
        }
#endif
    }
    return ret;
}


static int
_context_open(TVHContext *self, TVHOpenPhase phase, AVDictionary **opts)
{
    return (self->type->open) ? self->type->open(self, phase, opts) : 0;
}


static int
_context_decode(TVHContext *self, AVPacket *avpkt)
{
    return (self->type->decode) ? self->type->decode(self, avpkt) : 0;
}


static int
_context_encode(TVHContext *self, AVFrame *avframe)
{
    return (self->type->encode) ? self->type->encode(self, avframe) : 0;
    /*int ret = -1;

    if (!(ret = (self->type->encode) ? self->type->encode(self, avframe) : 0) &&
        (self->helper && self->helper->encode)) {
        ret = self->helper->encode(self, avframe);
    }
    return ret;*/
}


static int
_context_meta(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{
    if ((avpkt->flags & AV_PKT_FLAG_KEY) == 0)
        return 0;
    if (self->require_meta) {
        if (self->helper && self->helper->meta) {
            self->require_meta = self->helper->meta(self, avpkt, pkt);
            if (self->require_meta == 0)
                self->require_meta = 1;
        }
        else if (self->oavctx->extradata_size) {
            pkt->pkt_meta = pktbuf_alloc(self->oavctx->extradata,
                                         self->oavctx->extradata_size);
            self->require_meta = (pkt->pkt_meta) ? 1 : -1;
        }
    }
    return (self->require_meta < 0) ? -1 : 0;
}

#if LIBAVCODEC_VERSION_MAJOR > 59
/**
 * Builds a linear filter graph from a comma-separated string,
 * injecting the hardware context where necessary.
 * * @param graph The allocated AVFilterGraph
 * @param filter_str The comma-separated filter string (e.g., "format=nv12,hwupload,scale")
 * @param in_ctx The input filter context (e.g., the "buffer" filter)
 * @param out_ctx The output filter context (e.g., the "buffersink" filter)
 * @param hw_device_octx The hardware device context to inject
 * @return 0 on success, negative AVERROR on failure
 * NOTE: is not working with complex filters
 */
static int build_linear_hw_filter_graph(AVFilterGraph *graph, const char *filter_str, 
                                 AVFilterContext *in_ctx, AVFilterContext *out_ctx, 
                                 AVBufferRef *hw_device_octx) {
    int ret = 0;
    AVFilterContext *prev_ctx = in_ctx;
    
    // Duplicate the string because av_strtok modifies it
    char *str_copy = av_strdup(filter_str);
    if (!str_copy) return AVERROR(ENOMEM);

    char *saveptr = NULL;
    // Tokenize the string by commas
    char *filter_token = av_strtok(str_copy, ",", &saveptr);

    while (filter_token) {
        char *filter_name = filter_token;
        char *filter_args = NULL;

        // Find the first '=' to split the filter name from its arguments
        char *eq_ptr = strchr(filter_name, '=');
        if (eq_ptr) {
            *eq_ptr = '\0';       // Null-terminate the name
            filter_args = eq_ptr + 1; // The rest is the arguments string
        }

        // 1. Find the filter by name
        const AVFilter *filter = avfilter_get_by_name(filter_name);
        if (!filter) {
            // Log error: Filter not found
            ret = AVERROR_FILTER_NOT_FOUND;
            goto cleanup;
        }

        // 2. Create the filter (Allocates context AND calls init)
        AVFilterContext *current_ctx = NULL;
        
        // --- THE CRITICAL INJECTION STEP ---
        // If the filter requires a hardware context (like hwupload), we cannot use 
        // avfilter_graph_create_filter directly because it calls init() immediately.
        // Instead, we must use the two-step allocation/init process manually.
        
        current_ctx = avfilter_graph_alloc_filter(graph, filter, filter_name);
        if (!current_ctx) {
            ret = AVERROR(ENOMEM);
            goto cleanup;
        }

        // Inject the hardware context BEFORE initialization
        if (hw_device_octx && (strstr(filter->name, "hwupload") || strstr(filter->name, "hwmap"))) {
            current_ctx->hw_device_ctx = av_buffer_ref(hw_device_octx);
            if (!current_ctx->hw_device_ctx) {
                ret = AVERROR(ENOMEM);
                goto cleanup;
            }
        }

        // Now initialize the filter with its arguments
        ret = avfilter_init_str(current_ctx, filter_args);
        if (ret < 0) {
            // Log error: Failed to initialize filter
            goto cleanup;
        }

        // 3. Link this filter to the previous one in the chain
        if (prev_ctx) {
            ret = avfilter_link(prev_ctx, 0, current_ctx, 0);
            if (ret < 0) {
                // Log error: Failed to link filters
                goto cleanup;
            }
        }

        prev_ctx = current_ctx;
        
        // Get the next filter token
        filter_token = av_strtok(NULL, ",", &saveptr);
    }

    // Finally, link the last created filter to the output context (buffersink)
    if (prev_ctx && out_ctx) {
        ret = avfilter_link(prev_ctx, 0, out_ctx, 0);
        if (ret < 0) {
            // Log error: Failed to link to output
            goto cleanup;
        }
    }

cleanup:
    av_free(str_copy);
    return ret;
}
#endif


static int
_context_wrap(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{
    return (self->type->wrap) ? self->type->wrap(self, avpkt, pkt) : 0;
}


// creation

static AVCodecContext *
tvh_context_alloc_avctx(TVHContext *context, const AVCodec *avcodec)
{
    AVCodecContext *avctx = NULL;

    if ((avctx = avcodec_alloc_context3(avcodec))) {
        avctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        avctx->opaque = context;
    }
    return avctx;
}


static int
tvh_context_setup(TVHContext *self, const AVCodec *iavcodec, const AVCodec *oavcodec)
{
    enum AVMediaType media_type = iavcodec->type;
    const char *media_type_name = av_get_media_type_string(media_type);

    if (!(self->type = tvh_context_type_find(media_type))) {
        tvh_stream_log(self->stream, LOG_ERR,
                       "failed to find context type for '%s' media type",
                       media_type_name ? media_type_name : "<unknown>");
        return -1;
    }
    if (!(self->iavctx = tvh_context_alloc_avctx(self, iavcodec)) ||
        !(self->oavctx = tvh_context_alloc_avctx(self, oavcodec)) ||
        !(self->iavframe = av_frame_alloc()) ||
        !(self->oavframe = av_frame_alloc())) {
        tvh_stream_log(self->stream, LOG_ERR,
                       "failed to allocate AVCodecContext/AVFrame");
        return -1;
    }
    return 0;
}


// open
/**
 * Central open step for a @c TVHContext: validates which @c AVCodecContext is in
 * scope for @p phase, then runs the matching libav setup sequence.
 *
 * Phase 1 selects the working context and refuses to continue if no @c codec
 * is bound:
 * - Decoder phases (@c PREPARE_DECODER, @c OPEN_DECODER, @c NOTIFY_GLOBALHEADER)
 *   use @c self->iavctx.
 * - Encoder phases (@c PREPARE_ENCODER, @c OPEN_FILTERS, @c OPEN_ENCODER)
 *   use @c self->oavctx.
 *
 * Phase 2 (per @p phase):
 * - @c PREPARE_DECODER — read @c tvh_require_meta, call @c _context_open (media-type
 *   hooks), then optional decoder @c helper->open.
 * - @c OPEN_DECODER — @c avcodec_open2 on the decoder, then @c _context_open for
 *   post-open work; logs options on success.
 * - @c NOTIFY_GLOBALHEADER — @c _context_open only; frees @c self->temp_opts afterward.
 * - @c PREPARE_ENCODER — profile @c tvh_codec_profile_open, @c tvh_require_meta,
 *   @c _context_open, then optional encoder @c helper->open; stores @c self->helper.
 * - @c OPEN_FILTERS — @c _context_open only (typically builds the libavfilter graph).
 * - @c OPEN_ENCODER — @c avcodec_open2 on the encoder, then @c _context_open (e.g. audio
 *   computes @c delta / sink frame size after @c frame_size is known); frees
 *   @c self->temp_opts afterward.
 *
 * Intended call order for a full pipeline is implemented by @c tvh_context_open_decoder
 * and @c tvh_context_open_encoder (decoder: prepare → open → notify; encoder:
 * prepare → filters → encoder).
 *
 * @param self  Transcode context (decoder, encoder, profile, temp options).
 * @param phase Which @c TVHOpenPhase step to execute.
 * @return      0 on success; a negative @c AVERROR or other error code from the failing
 *              libav or Tvheadend step; @c AVERROR(EINVAL) for unknown phase or missing codec.
 */
static int
tvh_context_open(TVHContext *self, TVHOpenPhase phase)
{
    AVCodecContext *avctx = NULL;
    TVHContextHelper *helper = NULL;
    int ret = 0;

    // Phase 1: we check that we have codecs
    switch (phase) {
        case PREPARE_DECODER:
        case OPEN_DECODER:
        case NOTIFY_GLOBALHEADER:
            avctx = self->iavctx;
            if (!avctx->codec) {
                tvh_context_log(self, LOG_ERR, "no decoder available");
                return AVERROR(EINVAL);
            }
            break;
        case PREPARE_ENCODER:
        case OPEN_FILTERS:
        case OPEN_ENCODER:
            avctx = self->oavctx;
            if (!avctx->codec) {
                tvh_context_log(self, LOG_ERR, "no encoder available");
                return AVERROR(EINVAL);
            }
            break;
        default:
            tvh_context_log(self, LOG_ERR, "invalid open phase: %d", (int)phase);
            ret = AVERROR(EINVAL);
            break;
    }
    // Phase 2: we call proper functions (according to the state machine)
    switch (phase) {
        case PREPARE_DECODER:
            helper = tvh_decoder_helper_find(avctx->codec);
            if (!(ret = tvh_context_get_int_opt(&self->temp_opts, "tvh_require_meta", &self->require_meta)) &&
                !(ret = _context_open(self, phase, &self->temp_opts))){
                ret = (helper && helper->open) ? helper->open(self, &self->temp_opts) : 0;
            } else {
                tvh_context_log(self, LOG_ERR, "OpenPhase PREPARE_DECODER returned error: %d", ret);
            }
            break;
        case OPEN_DECODER:
            if (!(ret = avcodec_open2(avctx, NULL, &self->temp_opts)) && 
                !(ret = _context_open(self, phase, &self->temp_opts))){
                // print the dict for decoder
                _context_print_opts(self, self->temp_opts);
            } else {
                tvh_context_log(self, LOG_ERR, "OpenPhase OPEN_DECODER returned error: %d", ret);
            }
            break;
        case NOTIFY_GLOBALHEADER:
            if ((ret = _context_open(self, phase, &self->temp_opts))){
                tvh_context_log(self, LOG_ERR, "OpenPhase NOTIFY_GLOBALHEADER returned error: %d", ret);
            }
            // we no longer need dict for decoder
            if (self->temp_opts) {
                av_dict_free(&self->temp_opts);
                self->temp_opts = NULL;
            }
            break;
        case PREPARE_ENCODER:
            helper = self->helper = tvh_encoder_helper_find(avctx->codec);
            if (!(ret = tvh_codec_profile_open(self->profile, &self->temp_opts)) &&
                !(ret = tvh_context_get_int_opt(&self->temp_opts, "tvh_require_meta", &self->require_meta)) &&
                !(ret = _context_open(self, phase, &self->temp_opts)) && // pre open
                !(ret = (helper && helper->open) ? helper->open(self, &self->temp_opts) : 0)){  // pre open
                // print the dict for decoder
                _context_print_opts(self, self->temp_opts);
            } else {
                tvh_context_log(self, LOG_ERR, "OpenPhase PREPARE_ENCODER returned error: %d", ret);
            }
            break;
        case OPEN_FILTERS:
            if ((ret = _context_open(self, phase, &self->temp_opts))){
                tvh_context_log(self, LOG_ERR, "OpenPhase OPEN_FILTERS returned error: %d", ret);
            }
            break;
        case OPEN_ENCODER:
            if (!(ret = avcodec_open2(avctx, NULL, &self->temp_opts)) && 
                !(ret = _context_open(self, phase, &self->temp_opts))){
                // print the dict for encoder
                _context_print_opts(self, self->temp_opts);
            } else {
                tvh_context_log(self, LOG_ERR, "OpenPhase OPEN_ENCODER returned error: %d", ret);
            }
            // we no longer need dict for encoder
            if (self->temp_opts) {
                av_dict_free(&self->temp_opts);
                self->temp_opts = NULL;
            }
            break;
        default:
            tvh_context_log(self, LOG_ERR, "invalid open phase: %d", (int)phase);
            ret = AVERROR(EINVAL);
            break;
    }
    return ret;     
}

/**
 * Run the encoder-side open sequence for @p self in the correct order.
 *
 * Calls @ref tvh_context_open with:
 * 1. @c PREPARE_ENCODER — profile and type-specific preparation (@c sample_fmt,
 *    rates, layouts, bit rate / quality, HW hooks, etc.).
 * 2. @c OPEN_FILTERS — build and configure the libavfilter graph feeding the encoder
 *    (only if step 1 succeeds).
 * 3. @c OPEN_ENCODER — @c avcodec_open2 on the output codec, then type-specific
 *    post-open work (only if step 2 succeeds).
 *
 * On failure, logs which phase failed and returns that error; later phases are skipped.
 *
 * @param self Transcode context whose @c oavctx and filter graph are being set up.
 * @return     0 if all three phases succeed; otherwise the first non-zero return value
 *             from @ref tvh_context_open (typically a negative @c AVERROR).
 */
static int
tvh_context_open_encoder(TVHContext *self){
    int ret = 0;

    if ((ret = tvh_context_open(self, PREPARE_ENCODER))){
        tvh_context_log(self, LOG_ERR, "PREPARE_ENCODER returned error: %d", ret);
    }
    if (!(ret) && (ret = tvh_context_open(self, OPEN_FILTERS))){
        tvh_context_log(self, LOG_ERR, "OPEN_FILTERS returned error: %d", ret);
    }
    if (!(ret) && (ret = tvh_context_open(self, OPEN_ENCODER))){
        tvh_context_log(self, LOG_ERR, "OPEN_ENCODER returned error: %d", ret);
    }
    return ret;
}

/**
 * Run the decoder-side open sequence for @p self in the correct order.
 *
 * Calls @c tvh_context_open with:
 * 1. @c PREPARE_DECODER — type-specific decoder preparation and optional decoder
 *    helper setup (@c _context_open plus @c helper->open when present).
 * 2. @c OPEN_DECODER — @c avcodec_open2 on the input codec, then decoder post-open
 *    (@c _context_open) if step 1 succeeded.
 * 3. @c NOTIFY_GLOBALHEADER — global-header / extradata notification via
 *    @c _context_open if step 2 succeeded; @c tvh_context_open then clears
 *    @c self->temp_opts for the decoder path.
 *
 * On failure, logs which phase failed and returns that error; later phases are skipped.
 *
 * @param self Transcode context whose @c iavctx is being opened.
 * @return     0 if all three phases succeed; otherwise the first non-zero return value
 *             from @c tvh_context_open (typically a negative @c AVERROR).
 */
static int
tvh_context_open_decoder(TVHContext *self){
    int ret = 0;

    if ((ret = tvh_context_open(self, PREPARE_DECODER))){
        tvh_context_log(self, LOG_ERR, "PREPARE_DECODER returned error: %d", ret);
    }
    if (!(ret) && (ret = tvh_context_open(self, OPEN_DECODER))){
        tvh_context_log(self, LOG_ERR, "OPEN_DECODER returned error: %d", ret);
    }
    if (!(ret) && (ret = tvh_context_open(self, NOTIFY_GLOBALHEADER))){
        tvh_context_log(self, LOG_ERR, "NOTIFY_GLOBALHEADER returned error: %d", ret);
    }
    return ret;
}


// shipping

static th_pkt_t *
tvh_context_pack(TVHContext *self, AVPacket *avpkt)
{
    th_pkt_t *pkt = NULL;

    if (self->helper && self->helper->pack) {
        pkt = self->helper->pack(self, avpkt);
    } else {
        // th_pkt_t timestamps are expected to be in MPEG-TS 90kHz ticks.
        // Encoder packets are in self->oavctx->time_base and can change if filter is changing the frame_rate
        // (for example deinterlace_qsv can double the frame rate),
        // so rescale here.
        const AVRational tb90k = { 1, 90000 };
        int64_t pts90 = avpkt->pts;
        int64_t dts90 = avpkt->dts;
        if (pts90 != AV_NOPTS_VALUE)
            pts90 = av_rescale_q_rnd(pts90, self->oavctx->time_base, tb90k,
                                    AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        if (dts90 != AV_NOPTS_VALUE)
            dts90 = av_rescale_q_rnd(dts90, self->oavctx->time_base, tb90k,
                                    AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        int64_t dur90 = 0;
        if (avpkt->duration > 0) {
            dur90 = av_rescale_q_rnd(avpkt->duration, self->oavctx->time_base, tb90k,
                                    AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        } else if (self->oavctx->framerate.num > 0 && self->oavctx->framerate.den > 0) {
            dur90 = av_rescale_q(1, av_inv_q(self->oavctx->framerate), tb90k); // 3000@30fps, 1500@60fps
        }

        pkt = pkt_alloc(self->stream->type, avpkt->data, avpkt->size, pts90, dts90, 0);
        if (pkt && dur90 > 0)
            pkt->pkt_duration = dur90;
    }
    if (!pkt) {
        tvh_context_log(self, LOG_ERR, "failed to create packet");
    } else {
        // FIXME: ugly hack
        if (pkt->pkt_pcr == 0)
            pkt->pkt_pcr = pkt->pkt_dts;
        if (_context_meta(self, avpkt, pkt) || _context_wrap(self, avpkt, pkt))
            TVHPKT_CLEAR(pkt);
    }
    return pkt;
}


static int
tvh_context_ship(TVHContext *self, AVPacket *avpkt)
{
    th_pkt_t *pkt = NULL;
    int ret = -1;

    if (((ret = self->type->ship(self, avpkt)) > 0) &&
        (pkt = tvh_context_pack(self, avpkt))) {
        ret = tvh_context_deliver(self, pkt);
    }
    if ((ret = (ret != 0) ? -1 : ret)) {
        av_packet_unref(avpkt);
    }
    return ret;
}


// encoding

static int
tvh_context_receive_packet(TVHContext *self)
{
    AVPacket avpkt;
    int ret = -1;

    memset(&avpkt, 0, sizeof(avpkt));
    avpkt.pts = AV_NOPTS_VALUE;
    avpkt.dts = AV_NOPTS_VALUE;
    avpkt.pos = -1;

    while ((ret = avcodec_receive_packet(self->oavctx, &avpkt)) != AVERROR(EAGAIN)) {
        if (ret || (ret = tvh_context_ship(self, &avpkt))) {
            break;
        }
    }
    return (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) ? 0 : ret;
}


static int
tvh_context_encode_frame(TVHContext *self, AVFrame *avframe)
{
    int ret = -1;

    if (!(ret = avframe ? _context_encode(self, avframe) : 0)) {
        while ((ret = avcodec_send_frame(self->oavctx, avframe)) == AVERROR(EAGAIN)) {
            if ((ret = tvh_context_receive_packet(self))) {
                break;
            }
        }
    }
    if (!ret) {
        ret = tvh_context_receive_packet(self);
    }
    if ((ret = (ret == AVERROR_EOF) ? 0 : ret)) {
        av_frame_unref(avframe);
    }
    return ret;
}


static int
tvh_context_pull_frame(TVHContext *self, AVFrame *avframe)
{
    int ret = -1;

    av_frame_unref(avframe);
    ret = av_buffersink_get_frame_flags(self->oavfltctx, avframe, TVH_BUFFERSINK_FLAGS);
    return (ret > 0) ? 0 : ret;
}


static int
tvh_context_push_frame(TVHContext *self, AVFrame *avframe)
{
    int ret = -1;

    ret = av_buffersrc_add_frame_flags(self->iavfltctx, avframe, TVH_BUFFERSRC_FLAGS);
    return (ret > 0) ? 0 : ret;
}


static int
tvh_context_encode(TVHContext *self, AVFrame *avframe)
{
    int ret = 0;

    if (!avcodec_is_open(self->oavctx)) {
        ret = tvh_context_open_encoder(self);
    }
    if (!ret && !(ret = tvh_context_push_frame(self, avframe))) {
        while ((ret = tvh_context_pull_frame(self, self->oavframe)) != AVERROR(EAGAIN)) {
            if (ret || (ret = tvh_context_encode_frame(self, self->oavframe))) {
                break;
            }
        }
    }
    if ((ret = (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) ? 0 : ret)) {
        av_frame_unref(avframe);
    }
    return ret;
}


// decoding

static int
tvh_context_receive_frame(TVHContext *self, AVFrame *avframe)
{
    int ret = -1;

    while ((ret = avcodec_receive_frame(self->iavctx, avframe)) != AVERROR(EAGAIN)) {
        if (ret || (ret = tvh_context_encode(self, avframe))) {
            break;
        }
    }
    return (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) ? 0 : ret;
}


static int
tvh_context_decode_packet(TVHContext *self, const AVPacket *avpkt)
{
    int ret = -1;

    while ((ret = avcodec_send_packet(self->iavctx, avpkt)) == AVERROR(EAGAIN)) {
        if ((ret = tvh_context_receive_frame(self, self->iavframe))) {
            break;
        }
    }
    if (!ret) {
        ret = tvh_context_receive_frame(self, self->iavframe);
    }
    return (ret == AVERROR_EOF) ? 0 : ret;
}


static int
tvh_context_decode(TVHContext *self, AVPacket *avpkt)
{
    int ret = 0;

    if (!avcodec_is_open(self->iavctx)) {
        ret = tvh_context_open_decoder(self);
    }
    if (!ret && !(ret = _context_decode(self, avpkt))) {
        ret = tvh_context_decode_packet(self, avpkt);
    }
    return (ret == AVERROR(EAGAIN) || ret == AVERROR_INVALIDDATA ||
            ret == AVERROR(EIO)    || ret == AVERROR(EINVAL) ) ? 0 : ret;
}


static void
tvh_context_flush(TVHContext *self)
{
    tvh_context_decode_packet(self, NULL);
    tvh_context_encode_frame(self, NULL);
}


/* exposed */

int
tvh_context_get_int_opt(AVDictionary **opts, const char *key, int *value)
{
    AVDictionaryEntry *entry = NULL;

    if (*opts &&
        (entry = av_dict_get(*opts, key, NULL, AV_DICT_MATCH_CASE))) {
        *value = atoi(entry->value);
        return (av_dict_set(opts, key, NULL, 0) < 0) ? -1 : 0;
    }
    return 0;
}


int
tvh_context_get_str_opt(AVDictionary **opts, const char *key, char **value)
{
    AVDictionaryEntry *entry = NULL;

    if (*opts &&
        (entry = av_dict_get(*opts, key, NULL, AV_DICT_MATCH_CASE))) {
        *value = strdup(entry->value);
        return (av_dict_set(opts, key, NULL, 0) < 0) ? -1 : 0;
    }
    return 0;
}


void
tvh_context_close(TVHContext *self, int flush)
{
    if (flush) {
        tvh_context_flush(self);
    }
    if (self->type->close) {
        self->type->close(self);
    }
}

#if LIBAVCODEC_VERSION_MAJOR > 59
int
tvh_context_open_filters2(TVHContext *self,
                         const char *source_name,
                         const char *filters, 
                         const char *sink_name)
{
    static const AVClass logclass = {
        .class_name = "TVHGraph",
        .version    = 1,
    };
    struct {
        const AVClass *class;
    } logctx = { &logclass };
    const AVFilter *iavflt = NULL;
    const AVFilter *oavflt = NULL;
    AVBufferSrcParameters *par = NULL;
    int ret = -1;
    enum AVMediaType stream_type = AVMEDIA_TYPE_UNKNOWN;

    tvh_context_log(self, LOG_DEBUG, "filters: '%s'", filters);

    if (!(self->avfltgraph = avfilter_graph_alloc()) ||
        !(par = av_buffersrc_parameters_alloc())) {
        ret = AVERROR(ENOMEM);
        goto finish;
    }

// source
    if (!(iavflt = avfilter_get_by_name(source_name))) {
        tvh_context_log(self, LOG_ERR, "filters: source filter'%s' not found", source_name);
        ret = AVERROR_FILTER_NOT_FOUND;
        goto finish;
    }

    // 1. ALLOCATE (No init yet, so it won't fail due to missing args)
    self->iavfltctx = avfilter_graph_alloc_filter(self->avfltgraph, iavflt, "in");
    if (!self->iavfltctx) {
        tvh_context_log(self, LOG_ERR, "filters: failed to create 'in' filter");
        goto finish;
    }

    // additional buffersrc params
    memset(par, 0, sizeof(AVBufferSrcParameters));
    
    switch (self->iavctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            stream_type = AVMEDIA_TYPE_VIDEO;
            break;
        case AVMEDIA_TYPE_AUDIO:
            stream_type = AVMEDIA_TYPE_AUDIO;
            break;
        default:
            break;
    }

    if (stream_type == AVMEDIA_TYPE_UNKNOWN) {
        ret = -1;
        tvherror(LS_TRANSCODE, "filter can't process media type: %s", av_get_media_type_string(self->iavctx->codec_type));
        goto finish;
    }

    // 2. CONFIGURE PARAMETERS
    if (stream_type == AVMEDIA_TYPE_VIDEO) {
        // for Video:
        int int_pix_fmt = self->iavctx->pix_fmt;
        AVRational sample_aspect_ratio = self->iavctx->sample_aspect_ratio;
        if (self->iavframe && (sample_aspect_ratio.num <= 0 || sample_aspect_ratio.den <= 0)) {
            // try to extract SAR from AVFrame
            tvh_context_log(self, LOG_DEBUG, "codec %s is not advertising sample_aspect_ratio", self->iavctx->codec->name);
            sample_aspect_ratio = self->iavframe->sample_aspect_ratio;
            if (sample_aspect_ratio.num <= 0 || sample_aspect_ratio.den <= 0) {
                tvh_context_log(self, LOG_DEBUG, "frame %s is not advertising sample_aspect_ratio", self->iavctx->codec->name);
                if (self->sample_aspect_ratio.num && self->sample_aspect_ratio.den) {
                    // We have SAR from TVH
                    sample_aspect_ratio = (AVRational){self->sample_aspect_ratio.num, self->sample_aspect_ratio.den};
                    tvherror(LS_TRANSCODE, 
                    //tvh_context_log(self, LOG_DEBUG,
                        "Manually set SAR to %d/%d based on TVH SAR %d:%d",
                            sample_aspect_ratio.num, sample_aspect_ratio.den,
                            self->sample_aspect_ratio.num, self->sample_aspect_ratio.den);
                }
                // we feed back the SAR to iavctx
                self->iavframe->sample_aspect_ratio = sample_aspect_ratio;
            }
            if (sample_aspect_ratio.num <= 0 || sample_aspect_ratio.den <= 0) {
                // set SAR to 1,1
                tvh_context_log(self, LOG_DEBUG, "input stream is not advertising sample_aspect_ratio, default to {1,1}");
                // Fallback: If TVH doesn't know, default to square pixels
                sample_aspect_ratio = (AVRational){1, 1};
            }
            // we feed back the SAR to iavctx
            self->iavctx->sample_aspect_ratio = sample_aspect_ratio;
        }
        // Basic video properties
        par->width                = self->iavctx->width;
        par->height               = self->iavctx->height;
        par->sample_aspect_ratio  = sample_aspect_ratio;
        par->frame_rate           = self->iavctx->framerate;
        
        // CRITICAL: Metadata synchronization (fixes the FFmpeg 7 warnings)
        par->time_base            = av_make_q(self->iavctx->time_base.num, self->iavctx->time_base.den); // or stream->time_base
#if LIBAVCODEC_VERSION_MAJOR > 60
        par->color_range          = self->iavctx->color_range;  // Fixes "range: tv" warning
        par->color_space          = self->iavctx->colorspace;   // Fixes "csp: unknown" warning
        // to be added later on
        //par->color_primaries = self->iavctx->color_primaries;
        //par->color_trc       = self->iavctx->color_trc;
#endif
        // If using hardware acceleration (VAAPI/CUDA/etc), pass the frames context
        if (self->iavctx->hw_frames_ctx){
            // Increment the reference count so the filter "owns" a piece of the hardware context
            par->hw_frames_ctx = av_buffer_ref(self->iavctx->hw_frames_ctx);
            
            if (!par->hw_frames_ctx) {
                // Handle allocation error
                ret = AVERROR(ENOMEM);
                goto finish;
            }
            AVHWFramesContext *frames_ctx = (AVHWFramesContext*)self->iavctx->hw_frames_ctx->data;
            int_pix_fmt = (int)frames_ctx->format;
        }
        par->format               = int_pix_fmt;

        char logs[256];
        str_snprintf(logs, sizeof(logs),
            "filter video in configuration:"
            " video_size=%dx%d"
            ",pix_fmt=%s"
            ",time_base=%d/%d"
            ",frame_rate=%d/%d"
            ",pixel_aspect:%d/%d"
#if LIBAVCODEC_VERSION_MAJOR > 60
            ",color_range:%s"
            ",color_space:%s"
#endif
            ,self->iavctx->width, self->iavctx->height
            ,av_get_pix_fmt_name(int_pix_fmt)
            ,self->iavctx->time_base.num, self->iavctx->time_base.den
            ,self->iavctx->framerate.num, self->iavctx->framerate.den
            ,sample_aspect_ratio.num, sample_aspect_ratio.den
#if LIBAVCODEC_VERSION_MAJOR > 60
            ,av_color_range_name(self->iavctx->color_range)
            ,av_color_space_name(self->iavctx->colorspace)
#endif
        );
        // to be removed when we remove has_support_for_filter2
        tvhinfo(LS_TRANSCODE, 
        //tvh_context_log(self, LOG_DEBUG,
                          "%s", logs);

    } else if (stream_type == AVMEDIA_TYPE_AUDIO) {
        // for Audio:
        // Basic audio properties
        par->format      = self->iavctx->sample_fmt;
        par->sample_rate = self->iavctx->sample_rate;
        par->time_base   = (AVRational){1, self->iavctx->sample_rate};

        // Copy channel layout (Crucial for FFmpeg 7/8)
        ret = av_channel_layout_copy(&par->ch_layout, &self->iavctx->ch_layout);
        if (ret < 0) {
            tvh_context_log(self, LOG_ERR, "filters: failed to set av_channel_layout with error %d", ret);
            goto finish;
        }

        char logs[256];
        memset(logs, 0, sizeof(logs));
        char buf[64];
        memset(buf, 0, sizeof(buf));
        av_channel_layout_describe(&par->ch_layout, buf, sizeof(buf));
        str_snprintf(logs, sizeof(logs),
            "filter audio in configuration:"
            " ch_layout=%s"
            ",sample_fmt=%s"
            ",sample_rate=%d"
            ",time_base=%d/%d"
            ,buf
            ,av_get_sample_fmt_name(par->format)
            ,par->sample_rate
            ,par->time_base.num, par->time_base.den
        );
        // to be removed when we remove has_support_for_filter2
        tvhinfo(LS_TRANSCODE, 
        //tvh_context_log(self, LOG_DEBUG,
                          "%s", logs);
    } else tvherror(LS_TRANSCODE, "filter can process only 'buffer' and 'abuffer', most likely will fail.");

    // Push the parameters into the uninitialized filter context
    ret = av_buffersrc_parameters_set(self->iavfltctx, par);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to set AVFilterContext parameters with error %d", ret);
        goto finish;
    }

    // 3. MANUALLY INITIALIZE
    // Now that the params are inside, we call init with NULL
    ret = avfilter_init_str(self->iavfltctx, NULL);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to initialize AVFilterContext with error %d", ret);
        goto finish;
    }

// sink
    if (!(oavflt = avfilter_get_by_name(sink_name))) {
        tvh_context_log(self, LOG_ERR, "filters: sink filter '%s' not found", sink_name);
        ret = AVERROR_FILTER_NOT_FOUND;
        goto finish;
    }

    ret = avfilter_graph_create_filter(&self->oavfltctx, oavflt, "out", NULL, NULL, self->avfltgraph);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to create 'out' filter with error %d", ret);
        goto finish;
    }

    // filters
    // Build the linear chain and inject the hardware context 
    if (filters && *filters) {
        ret = build_linear_hw_filter_graph(self->avfltgraph, filters, 
                                           self->iavfltctx, self->oavfltctx, 
                                           self->hw_device_octx);
        if (ret < 0) {
            // Log error
            tvh_context_log(self, LOG_ERR, "filters: fail during build_linear_hw_filter_graph() %d", ret);
            goto finish;
        }
    } else {
        // If there's no filter string, just link input directly to output
        ret = avfilter_link(self->iavfltctx, 0, self->oavfltctx, 0);
        if (ret < 0) {
            // Log error
            tvh_context_log(self, LOG_ERR, "filters: fail during build_linear_hw_filter_graph() %d", ret);
            goto finish;
        }
    }

    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to add filters: '%s' with error %d", filters, ret);
        goto finish;
    }

    // insert aditional filters required for conversions
    avfilter_graph_set_auto_convert(self->avfltgraph, AVFILTER_AUTO_CONVERT_ALL);

    //
    if ((ret = avfilter_graph_config(self->avfltgraph, &logctx)) < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to config filter graph with error %d", ret);
        goto finish;
    }

    stream_type = av_buffersink_get_type(self->oavfltctx);

    // 1. CONFIGURE PARAMETERS
    if (stream_type == AVMEDIA_TYPE_VIDEO) {
        // for Video:
        // Extract negotiated parameters from the sink context and pass them to the encoder context.
        self->oavctx->pix_fmt   = av_buffersink_get_format(self->oavfltctx);
        self->oavctx->width     = av_buffersink_get_w(self->oavfltctx);
        self->oavctx->height    = av_buffersink_get_h(self->oavfltctx);
        // Important: The filter graph may change the timebase
        self->oavctx->time_base = av_buffersink_get_time_base(self->oavfltctx);
        self->oavctx->framerate = av_buffersink_get_frame_rate(self->oavfltctx);
#if LIBAVCODEC_VERSION_MAJOR > 60
        // Get the output link to access color metadata
        const AVFilterLink *outlink = self->oavfltctx->inputs[0];
        // 1. Extract Color Range (MPEG/TV vs JPEG/PC)
        self->oavctx->color_range = outlink->color_range;
        // 2. Extract Color Space (e.g., BT.709, BT.2020)
        self->oavctx->colorspace = outlink->colorspace;
        // 3. Extract Color Primaries and Transfer Characteristics (Recommended)
        // to be added later on
        //self->oavctx->color_primaries = outlink->color_primaries;
        //self->oavctx->color_trc       = outlink->color_trc;
#endif
        // Handle Sample Aspect Ratio (SAR)
        AVRational sar = av_buffersink_get_sample_aspect_ratio(self->oavfltctx);
        if (sar.num > 0 && sar.den > 0)
            self->oavctx->sample_aspect_ratio = sar;

        // 2. THE HARDWARE LINK
        // Query the hardware frames context from the sink.
        AVBufferRef *hw_frames_ctx = av_buffersink_get_hw_frames_ctx(self->oavfltctx);
        
        if (hw_frames_ctx) {
            // We must create a new reference to the context. 
            // The encoder context will take ownership of this reference. 
            self->oavctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
            if (!self->oavctx->hw_frames_ctx) {
                ret = AVERROR(ENOMEM);
                goto finish;
            }
            
            // Optimization: Some encoders (like VAAPI) benefit from knowing 
            // the underlying software format being "hidden" by the hardware surface.
            const AVHWFramesContext *frames_fctx = (AVHWFramesContext *)hw_frames_ctx->data;
            self->oavctx->sw_pix_fmt = frames_fctx->sw_format;
            
            // Log it so you can see it's working
            tvh_context_log(self, LOG_DEBUG, "filters: filter sink provided HW frames (pix_fmt: %s, sw_pix_fmt: %s)", 
                av_get_pix_fmt_name(self->oavctx->pix_fmt), av_get_pix_fmt_name(self->oavctx->sw_pix_fmt));
        }
        char logs[256];
        str_snprintf(logs, sizeof(logs),
            "filter video out configuration:"
            " video_size=%dx%d"
            ",pix_fmt=%s"
            ",time_base=%d/%d"
            ",frame_rate=%d/%d"
            ",pixel_aspect:%d/%d"
#if LIBAVCODEC_VERSION_MAJOR > 60
            ",color_range:%s"
            ",color_space:%s"
#endif
            ,self->oavctx->width, self->oavctx->height
            ,av_get_pix_fmt_name(self->oavctx->pix_fmt)
            ,self->oavctx->time_base.num, self->oavctx->time_base.den
            ,self->oavctx->framerate.num, self->oavctx->framerate.den
            ,self->oavctx->sample_aspect_ratio.num, self->oavctx->sample_aspect_ratio.den
#if LIBAVCODEC_VERSION_MAJOR > 60
            ,av_color_range_name(outlink->color_range)
            ,av_color_space_name(outlink->colorspace)
#endif
        );
        // to be removed when we remove has_support_for_filter2
        tvhinfo(LS_TRANSCODE, 
        //tvh_context_log(self, LOG_DEBUG,
                          "%s", logs);

    } else if (stream_type == AVMEDIA_TYPE_AUDIO) {
        // for Audio:
        // Extract negotiated parameters from the sink context and pass them to the encoder context.
        self->oavctx->sample_rate = av_buffersink_get_sample_rate(self->oavfltctx);
        self->oavctx->sample_fmt = av_buffersink_get_format(self->oavfltctx);
        // Correct way to set up the audio encoder
        self->oavctx->time_base = (AVRational){1, self->oavctx->sample_rate};   // CRITICAL

        ret = av_buffersink_get_ch_layout(self->oavfltctx, &self->oavctx->ch_layout);
        if (ret < 0) {
            tvh_context_log(self, LOG_ERR, "filters: failed to config AVCodecContext ch_layout with error %d", ret);
            goto finish;
        }

        char logs[256];
        memset(logs, 0, sizeof(logs));
        char buf[64];
        memset(buf, 0, sizeof(buf));
        av_channel_layout_describe(&self->oavctx->ch_layout, buf, sizeof(buf));
        str_snprintf(logs, sizeof(logs),
            "filter audio out configuration:"
            " ch_layout=%s"
            ",sample_fmt=%s"
            ",sample_rate=%d"
            ",time_base=%d/%d"
            ,buf
            ,av_get_sample_fmt_name(self->oavctx->sample_fmt)
            ,self->oavctx->sample_rate
            ,self->oavctx->time_base.num, self->oavctx->time_base.den
        );
        // to be removed when we remove has_support_for_filter2
        tvhinfo(LS_TRANSCODE, 
        //tvh_context_log(self, LOG_DEBUG,
                          "%s", logs);

    } else tvherror(LS_TRANSCODE, "filter can process only 'buffersink' and 'abuffersink', most likely will fail.");

    if (ret >= 0 && tvhtrace_enabled()) {
        char *graph = avfilter_graph_dump(self->avfltgraph, NULL);
        char *str, *token, *saveptr = NULL;
        for (str = graph; ; str = NULL) {
          token = strtok_r(str, "\n", &saveptr);
          if (token == NULL)
            break;
          tvh_context_log(self, LOG_TRACE, "filters dump: %s", token);
        }
        av_freep(&graph);
    }

finish:
    if (par) {
        av_buffer_unref(&par->hw_frames_ctx);
        av_freep(&par);
    }
    return (ret > 0) ? 0 : ret;
}
#endif
int
tvh_context_open_filters(TVHContext *self,
                         const char *source_name, const char *source_args,
                         const char *filters, const char *sink_name, ...)
{
    static const AVClass logclass = {
        .class_name = "TVHGraph",
        .version    = 1,
    };
    struct {
        const AVClass *class;
    } logctx = { &logclass };
    const AVFilter *iavflt = NULL, *oavflt = NULL;
    AVFilterInOut *iavfltio = NULL, *oavfltio = NULL;
    AVBufferSrcParameters *par = NULL;
    int i, ret = -1;

    tvh_context_log(self, LOG_DEBUG, "filters: source args: '%s'", source_args);
    tvh_context_log(self, LOG_DEBUG, "filters: filters: '%s'", filters);

    if (!(self->avfltgraph = avfilter_graph_alloc()) ||
        !(iavfltio = avfilter_inout_alloc()) ||
        !(oavfltio = avfilter_inout_alloc()) ||
        !(par = av_buffersrc_parameters_alloc())) {
        ret = AVERROR(ENOMEM);
        goto finish;
    }

    // source
    if (!(iavflt = avfilter_get_by_name(source_name))) {
        tvh_context_log(self, LOG_ERR, "filters: source filter'%s' not found",
                        source_name);
        ret = AVERROR_FILTER_NOT_FOUND;
        goto finish;
    }
    ret = avfilter_graph_create_filter(&self->iavfltctx, iavflt, "in",
                                       source_args, NULL, self->avfltgraph);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to create 'in' filter");
        goto finish;
    }

    // additional buffersrc params
    if (!strcmp("buffer", source_name) && self->iavctx->hw_frames_ctx) { // hmm...
        memset(par, 0, sizeof(AVBufferSrcParameters));
        par->format = AV_PIX_FMT_NONE;
        par->hw_frames_ctx = self->iavctx->hw_frames_ctx;
        ret = av_buffersrc_parameters_set(self->iavfltctx, par);
        if (ret < 0) {
            tvh_context_log(self, LOG_ERR,
                            "filters: failed to set additional parameters");
            goto finish;
        }
    }

    // sink
    if (!(oavflt = avfilter_get_by_name(sink_name))) {
        tvh_context_log(self, LOG_ERR, "filters: sink filter '%s' not found",
                        sink_name);
        ret = AVERROR_FILTER_NOT_FOUND;
        goto finish;
    }
    ret = avfilter_graph_create_filter(&self->oavfltctx, oavflt, "out",
                                       NULL, NULL, self->avfltgraph);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to create 'out' filter");
        goto finish;
    }

    // sink options
    va_list ap;
    va_start(ap, sink_name);
    ret = _context_filters_apply_sink_options(self, ap);
    va_end(ap);
    if (ret) {
        goto finish;
    }

    // Endpoints for the filter graph.
    iavfltio->name       = av_strdup("out");
    iavfltio->filter_ctx = self->oavfltctx;
    iavfltio->pad_idx    = 0;
    iavfltio->next       = NULL;

    oavfltio->name       = av_strdup("in");
    oavfltio->filter_ctx = self->iavfltctx;
    oavfltio->pad_idx    = 0;
    oavfltio->next       = NULL;

    if (!iavfltio->name || !oavfltio->name) {
        ret = AVERROR(ENOMEM);
        goto finish;
    }

    // filters
    ret = avfilter_graph_parse_ptr(self->avfltgraph, filters,
                                   &iavfltio, &oavfltio, NULL);
    if (ret < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to add filters: '%s'",
                        filters);
        goto finish;
    }

    // additional filtergraph params
    if (!strcmp("buffer", source_name) && self->hw_device_octx) {
        for (i = 0; i < self->avfltgraph->nb_filters; i++) {
            if (!(self->avfltgraph->filters[i]->hw_device_ctx =
                      av_buffer_ref(self->hw_device_octx))) {
                ret = -1;
                goto finish;
            }
        }
    }

    avfilter_graph_set_auto_convert(self->avfltgraph,
                                    AVFILTER_AUTO_CONVERT_ALL);

    if ((ret = avfilter_graph_config(self->avfltgraph, &logctx)) < 0) {
        tvh_context_log(self, LOG_ERR, "filters: failed to config filter graph");
    }

    if (ret >= 0 && tvhtrace_enabled()) {
        char *graph = avfilter_graph_dump(self->avfltgraph, NULL);
        char *str, *token, *saveptr = NULL;
        for (str = graph; ; str = NULL) {
          token = strtok_r(str, "\n", &saveptr);
          if (token == NULL)
            break;
          tvh_context_log(self, LOG_TRACE, "filters dump: %s", token);
        }
        av_freep(&graph);
    }

finish:
    if (par) {
        av_freep(&par);
    }
    if (iavfltio) {
        avfilter_inout_free(&iavfltio);
    }
    if (oavfltio) {
        avfilter_inout_free(&oavfltio);
    }
    return (ret > 0) ? 0 : ret;
}


int
tvh_context_deliver(TVHContext *self, th_pkt_t *pkt)
{
    return tvh_stream_deliver(self->stream, pkt);
}


int
tvh_context_handle(TVHContext *self, th_pkt_t *pkt)
{
    int ret = 0;
    uint8_t *data = NULL;
    size_t size = 0;
    AVPacket avpkt;

    if ((size = pktbuf_len(pkt->pkt_payload)) && pktbuf_ptr(pkt->pkt_payload)) {
        if (size >= TVH_INPUT_BUFFER_MAX_SIZE) {
            tvh_context_log(self, LOG_ERR, "packet payload too big");
            ret = AVERROR(EOVERFLOW);
        }
        else if (!(data = pktbuf_copy_data(pkt->pkt_payload))) {
            tvh_context_log(self, LOG_ERR, "failed to copy packet payload");
            ret = AVERROR(ENOMEM);
        }
        else {
            memset(&avpkt, 0, sizeof(avpkt));
            avpkt.pts = AV_NOPTS_VALUE;
            avpkt.dts = AV_NOPTS_VALUE;
            avpkt.pos = -1;
            if ((ret = av_packet_from_data(&avpkt, data, size))) { // takes ownership of data
                tvh_context_log(self, LOG_ERR,
                                "failed to allocate AVPacket buffer");
                av_freep(data);
            }
            else {
                if (!self->input_gh && pkt->pkt_meta) {
                    pktbuf_ref_inc(pkt->pkt_meta);
                    self->input_gh = pkt->pkt_meta;
                }
                avpkt.pts = pkt->pkt_pts;
                avpkt.dts = pkt->pkt_dts;
                avpkt.duration = pkt->pkt_duration;
                TVHPKT_SET(self->src_pkt, pkt);
                ret = tvh_context_decode(self, &avpkt);
                av_packet_unref(&avpkt); // will free data
            }
        }
    }
    return ret;
}


TVHContext *
tvh_context_create(TVHStream *stream, TVHCodecProfile *profile,
                   const AVCodec *iavcodec, const AVCodec *oavcodec, pktbuf_t *input_gh)
{
    TVHContext *self = NULL;

    if (!(self = calloc(1, sizeof(TVHContext)))) {
        tvh_stream_log(stream, LOG_ERR, "failed to allocate context");
        return NULL;
    }
    self->stream = stream;
    self->profile = profile;
    if (tvh_context_setup(self, iavcodec, oavcodec)) {
        tvh_context_destroy(self);
        return NULL;
    }
    if (input_gh) {
        pktbuf_ref_inc(input_gh);
        self->input_gh = input_gh;
    }
    return self;
}


void
tvh_context_destroy(TVHContext *self)
{
    if (self) {
        TVHPKT_CLEAR(self->src_pkt);
        if (self->avfltgraph) {
            avfilter_graph_free(&self->avfltgraph); // frees filter contexts
            self->oavfltctx = NULL;
            self->iavfltctx = NULL;
            self->avfltgraph = NULL;
        }
        if (self->helper) {
            self->helper = NULL;
        }
        if (self->input_gh) {
            pktbuf_ref_dec(self->input_gh);
            self->input_gh = NULL;
        }
        if (self->oavctx) {
            avcodec_free_context(&self->oavctx); // frees extradata
            self->oavctx = NULL;
        }
        if (self->iavctx) {
            avcodec_free_context(&self->iavctx);
            self->iavctx = NULL;
        }
        if (self->oavframe) {
            av_frame_free(&self->oavframe);
            self->oavframe = NULL;
        }
        if (self->iavframe) {
            av_frame_free(&self->iavframe);
            self->iavframe = NULL;
        }
#if ENABLE_HWACCELS
        hwaccels_context_destroy(self);
#endif
        self->type = NULL;
        self->profile = NULL;
        self->stream = NULL;
        free(self->hw_accel_device);
        free(self);
        self = NULL;
    }
}
