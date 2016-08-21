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


struct tvh_transcoder_t;
typedef struct tvh_transcoder_t TVHTranscoder;

struct tvh_stream_t;
typedef struct tvh_stream_t TVHStream;

struct tvh_context_type_t;
typedef struct tvh_context_type_t TVHContextType;

struct tvh_context_t;
typedef struct tvh_context_t TVHContext;

struct tvh_context_helper_t;
typedef struct tvh_context_helper_t TVHContextHelper;

// post phase _MUST_ be == phase + 1
typedef enum {
    OPEN_DECODER,
    OPEN_DECODER_POST,
    OPEN_ENCODER,
    OPEN_ENCODER_POST
} TVHOpenPhase;


/* TVHTranscoder ============================================================ */

SLIST_HEAD(TVHStreams, tvh_stream_t);

typedef struct tvh_transcoder_t {
    tvh_st_t input;
    struct TVHStreams streams;
    uint32_t id;
    tvh_st_t *output;
    TVHCodecProfile *profiles[AVMEDIA_TYPE_NB];
} TVHTranscoder;

int
tvh_transcoder_deliver(TVHTranscoder *self, th_pkt_t *pkt);

TVHTranscoder *
tvh_transcoder_create(tvh_st_t *output, const char **profiles);

void
tvh_transcoder_destroy(TVHTranscoder *self);


/* TVHStream ================================================================ */

typedef struct tvh_stream_t {
    TVHTranscoder *transcoder;
    int id;
    int index;
    tvh_sct_t type;
    TVHContext *context;
    SLIST_ENTRY(tvh_stream_t) link;
} TVHStream;

void
tvh_stream_stop(TVHStream *self, int flush);

int
tvh_stream_handle(TVHStream *self, th_pkt_t *pkt);

int
tvh_stream_deliver(TVHStream *self, th_pkt_t *pkt);

TVHStream *
tvh_stream_create(TVHTranscoder *transcoder, TVHCodecProfile *profile,
                  tvh_ssc_t *ssc);

void
tvh_stream_destroy(TVHStream *self);


/* TVHContextType =========================================================== */

typedef int (*tvh_context_open_meth)(TVHContext *, TVHOpenPhase, AVDictionary **);
typedef int (*tvh_context_decode_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_encode_meth)(TVHContext *, AVFrame *);
typedef int (*tvh_context_ship_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_wrap_meth)(TVHContext *, AVPacket *, th_pkt_t *);
typedef void (*tvh_context_close_meth)(TVHContext *);

typedef struct tvh_context_type_t {
    enum AVMediaType media_type;
    tvh_context_open_meth open;
    tvh_context_decode_meth decode;
    tvh_context_encode_meth encode;
    tvh_context_ship_meth ship;
    tvh_context_wrap_meth wrap;
    tvh_context_close_meth close;
    SLIST_ENTRY(tvh_context_type_t) link;
} TVHContextType;

void
tvh_context_types_register(void);

void
tvh_context_types_forget(void);


/* TVHContext =============================================================== */

typedef struct tvh_context_t {
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
    // only for audio
    int64_t duration;
    int64_t delta;
    int64_t pts;
    uint8_t sri;
} TVHContext;

int
tvh_context_get_int_opt(AVDictionary **opts, const char *key, int *value);

int
tvh_context_get_str_opt(AVDictionary **opts, const char *key, char **value);

void
tvh_context_close(TVHContext *self, int flush);

/* __VA_ARGS__ = NULL terminated list of sink options
   sink option = (const char *name, const uint8_t *value, int size) */
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
                   AVCodec *iavcodec, AVCodec *oavcodec, pktbuf_t *input_gh);

void
tvh_context_destroy(TVHContext *self);


/* TVHContextHelper ========================================================= */

typedef int (*tvh_context_helper_open_meth)(TVHContext *, AVDictionary **);
typedef th_pkt_t *(*tvh_context_helper_pack_meth)(TVHContext *, AVPacket *);
typedef int (*tvh_context_helper_meta_meth)(TVHContext *, AVPacket *, th_pkt_t *);

typedef struct tvh_context_helper_t {
    enum AVMediaType type;
    enum AVCodecID id;
    tvh_context_helper_open_meth open;
    tvh_context_helper_pack_meth pack;
    tvh_context_helper_meta_meth meta;
    SLIST_ENTRY(tvh_context_helper_t) link;
} TVHContextHelper;

TVHContextHelper *
tvh_decoder_helper_find(const AVCodec *avcodec);

TVHContextHelper *
tvh_encoder_helper_find(const AVCodec *avcodec);

void
tvh_context_helpers_register(void);

void
tvh_context_helpers_forget(void);


#endif // TVH_TRANSCODING_TRANSCODE_INTERNALS_H__
