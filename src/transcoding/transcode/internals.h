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


#ifndef TVH_TRANSCODING_TRANSCODE_INTERNALS_H__
#define TVH_TRANSCODING_TRANSCODE_INTERNALS_H__


#include "log.h"

#include "transcoding/codec.h"
#include "transcoding/memutils.h"

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/pixdesc.h>


#define tvh_st_t streaming_target_t
#define tvh_ss_t streaming_start_t
#define tvh_sm_t streaming_message_t


extern TVHCodecProfile *tvh_codec_profile_copy;


typedef struct tvh_transcoder TVHTranscoder;
typedef struct tvh_stream TVHStream;
typedef struct tvh_context_type TVHContextType;
typedef struct tvh_context TVHContext;
typedef struct tvh_context_helper TVHContextHelper;

// post phase _MUST_ be == phase + 1
typedef enum {
    OPEN_DECODER,
    OPEN_DECODER_POST,
    OPEN_ENCODER,
    OPEN_ENCODER_POST
} TVHOpenPhase;

#if LIBAVCODEC_VERSION_MAJOR > 59
// this is needed to separate av_opt_set-s from _context_filters_apply_sink_options
typedef enum {
    AV_OPT_SET_UNKNOWN,
    AV_OPT_SET_BIN,
    AV_OPT_SET_STRING
} av_opt_set_type;
#endif

/* TVHTranscoder ============================================================ */

SLIST_HEAD(TVHStreams, tvh_stream);

struct tvh_transcoder {
    tvh_st_t input;
    struct TVHStreams streams;
    uint32_t id;
    tvh_st_t *output;
    TVHCodecProfile *profiles[AVMEDIA_TYPE_NB];
    char *src_codecs[AVMEDIA_TYPE_NB];
};

int
tvh_transcoder_deliver(TVHTranscoder *self, th_pkt_t *pkt);

TVHTranscoder *
tvh_transcoder_create(tvh_st_t *output,
                      const char **profiles,
                      const char **src_codecs);

void
tvh_transcoder_destroy(TVHTranscoder *self);


/* TVHStream ================================================================ */

struct tvh_stream {
    TVHTranscoder *transcoder;
    int id;
    int index;
    tvh_sct_t type;
    TVHContext *context;
    int is_copy;
    SLIST_ENTRY(tvh_stream) link;
};

void
tvh_stream_stop(TVHStream *self, int flush);

int
tvh_stream_handle(TVHStream *self, th_pkt_t *pkt);

int
tvh_stream_deliver(TVHStream *self, th_pkt_t *pkt);

TVHStream *
tvh_stream_create(TVHTranscoder *transcoder, TVHCodecProfile *profile,
                  tvh_ssc_t *ssc, const char *src_codecs);

void
tvh_stream_destroy(TVHStream *self);


/* TVHContextType =========================================================== */

typedef int (*tvh_context_open_meth)(TVHContext *, TVHOpenPhase, AVDictionary **);
typedef int (*tvh_context_decode_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_encode_meth)(TVHContext *, AVFrame *);
typedef int (*tvh_context_ship_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_wrap_meth)(TVHContext *, AVPacket *, th_pkt_t *);
typedef void (*tvh_context_close_meth)(TVHContext *);

struct tvh_context_type {
    enum AVMediaType media_type;
    tvh_context_open_meth open;
    tvh_context_decode_meth decode;
    tvh_context_encode_meth encode;
    tvh_context_ship_meth ship;
    tvh_context_wrap_meth wrap;
    tvh_context_close_meth close;
    SLIST_ENTRY(tvh_context_type) link;
};

void
tvh_context_types_register(void);

void
tvh_context_types_forget(void);


/* TVHContext =============================================================== */

struct tvh_context {
    TVHStream *stream;
    TVHCodecProfile *profile;
    TVHContextType *type;
    AVCodecContext *iavctx;
    AVCodecContext *oavctx;
    AVFrame *iavframe;
    AVFrame *oavframe;
    pktbuf_t *input_gh;
    TVHContextHelper *helper; // encoder helper
    AVFilterGraph *avfltgraph;
    AVFilterContext *iavfltctx;
    AVFilterContext *oavfltctx;
    th_pkt_t *src_pkt;
    int require_meta;
    int64_t pts;
    // only for audio
    int64_t duration;
    int64_t delta;
    uint8_t sri;
    // hardware acceleration
    char *hw_accel_device;
    AVBufferRef *hw_device_ref;
    void *hw_accel_ictx;
    AVBufferRef *hw_device_octx;
};

int
tvh_context_get_int_opt(AVDictionary **opts, const char *key, int *value);

int
tvh_context_get_str_opt(AVDictionary **opts, const char *key, char **value);

void
tvh_context_close(TVHContext *self, int flush);

/* __VA_ARGS__ = NULL terminated list of sink options
   sink option = (const char *name, av_opt_set_type opt_set_type, int size, const uint8_t *value) */
int
tvh_context_open_filters(TVHContext *self,
                         const char *source_name, const char *source_args,
                         const char *filters, const char *sink_name, ...);

int
tvh_context_handle(TVHContext *self, th_pkt_t *pkt);

int
tvh_context_deliver(TVHContext *self, th_pkt_t *pkt);

TVHContext *
tvh_context_create(TVHStream *stream, TVHCodecProfile *profile,
                   const AVCodec *iavcodec, const AVCodec *oavcodec, pktbuf_t *input_gh);

void
tvh_context_destroy(TVHContext *self);


/* TVHContextHelper ========================================================= */

typedef int (*tvh_context_helper_open_meth)(TVHContext *, AVDictionary **);
typedef th_pkt_t *(*tvh_context_helper_pack_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_helper_meta_meth)(TVHContext *, AVPacket *, th_pkt_t *);

struct tvh_context_helper {
    enum AVMediaType type;
    enum AVCodecID id;
    tvh_context_helper_open_meth open;
    tvh_context_helper_pack_meth pack;
    tvh_context_helper_meta_meth meta;
    SLIST_ENTRY(tvh_context_helper) link;
};

TVHContextHelper *
tvh_decoder_helper_find(const AVCodec *avcodec);

TVHContextHelper *
tvh_encoder_helper_find(const AVCodec *avcodec);

void
tvh_context_helpers_register(void);

void
tvh_context_helpers_forget(void);


#endif // TVH_TRANSCODING_TRANSCODE_INTERNALS_H__
