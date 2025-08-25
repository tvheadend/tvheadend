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

static int
tvh_context_open(TVHContext *self, TVHOpenPhase phase)
{
    AVCodecContext *avctx = NULL;
    TVHContextHelper *helper = NULL;
    AVDictionary *opts = NULL;
    int ret = 0;

    switch (phase) {
        case OPEN_DECODER:
            avctx = self->iavctx;
            helper = tvh_decoder_helper_find(avctx->codec);
            break;
        case OPEN_ENCODER:
            avctx = self->oavctx;
            helper = self->helper = tvh_encoder_helper_find(avctx->codec);
            ret = tvh_codec_profile_open(self->profile, &opts);
            break;
        default:
            tvh_context_log(self, LOG_ERR, "invalid open phase");
            ret = AVERROR(EINVAL);
            break;
    }
    if (!ret) {
        _context_print_opts(self, opts);
        ret = tvh_context_get_int_opt(&opts, "tvh_require_meta",
                                      &self->require_meta);
        if (!ret && !(ret = _context_open(self, phase, &opts)) && // pre open
            !(ret = (helper && helper->open) ? helper->open(self, &opts) : 0) && // pre open
            !(ret = avcodec_open2(avctx, NULL, &opts))) { // open
            ret = _context_open(self, phase + 1, &opts); // post open
        }
        _context_print_opts(self, opts);
    }
    if (opts) {
        av_dict_free(&opts);
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
        pkt = pkt_alloc(self->stream->type, avpkt->data, avpkt->size, avpkt->pts, avpkt->dts, 0);
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
        ret = tvh_context_open(self, OPEN_ENCODER);
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
        ret = tvh_context_open(self, OPEN_DECODER);
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
        if (self->oavframe) {
            av_frame_free(&self->oavframe);
            self->oavframe = NULL;
        }
        if (self->iavframe) {
            av_frame_free(&self->iavframe);
            self->iavframe = NULL;
        }
        if (self->oavctx) {
            avcodec_free_context(&self->oavctx); // frees extradata
            self->oavctx = NULL;
        }
        if (self->iavctx) {
            avcodec_free_context(&self->iavctx);
            self->iavctx = NULL;
        }
        self->type = NULL;
        self->profile = NULL;
        self->stream = NULL;
        free(self->hw_accel_device);
        free(self);
        self = NULL;
    }
}
