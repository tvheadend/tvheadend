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

#include "parsers/parsers.h"
#include "parsers/bitstream.h"
#include "parsers/parser_avc.h"
#include "parsers/parser_h264.h"
#include "parsers/parser_hevc.h"


SLIST_HEAD(TVHContextHelpers, tvh_context_helper);
static struct TVHContextHelpers tvh_decoder_helpers;
static struct TVHContextHelpers tvh_encoder_helpers;


#define tvh_context_helper_register(list, helper, kind) \
    do { \
        if ((helper)->type <= AVMEDIA_TYPE_UNKNOWN || \
            (helper)->type >= AVMEDIA_TYPE_NB) { \
            tvherror(LS_TRANSCODE, \
                     "incomplete/wrong definition for " #helper " helper"); \
            return; \
        } \
        SLIST_INSERT_HEAD((list), (helper), link); \
        tvhinfo(LS_TRANSCODE, "'" #helper "' %s helper registered", (kind)); \
    } while (0)

#define tvh_decoder_helper_register(helper) \
    do { \
        if (!(helper)->open) { \
            tvherror(LS_TRANSCODE, \
                     "incomplete/wrong definition for " #helper " helper"); \
            return; \
        } \
        tvh_context_helper_register(&tvh_decoder_helpers, helper, "decoder"); \
    } while (0)

#define tvh_encoder_helper_register(helper) \
    do { \
        if (!((helper)->open || (helper)->pack || (helper)->meta)) { \
            tvherror(LS_TRANSCODE, \
                     "incomplete/wrong definition for " #helper " helper"); \
            return; \
        } \
        tvh_context_helper_register(&tvh_encoder_helpers, helper, "encoder"); \
    } while (0)


static TVHContextHelper *
tvh_context_helper_find(struct TVHContextHelpers *list, const AVCodec *codec)
{
    TVHContextHelper *helper = NULL;

    SLIST_FOREACH(helper, list, link) {
        if (helper->type == codec->type && helper->id == codec->id) {
            return helper;
        }
    }
    return NULL;
}


/* decoders ================================================================= */

/* shared by H264, AAC and VORBIS */
static int
tvh_extradata_open(TVHContext *self, AVDictionary **opts)
{
    size_t extradata_size = 0;

    if (!(extradata_size = pktbuf_len(self->input_gh))) {
        return AVERROR(EAGAIN);
    }
    if (extradata_size >= TVH_INPUT_BUFFER_MAX_SIZE) {
        tvh_context_log(self, LOG_ERR, "extradata too big");
        return AVERROR(EOVERFLOW);
    }
    if (self->iavctx->extradata) {
        tvh_context_log(self, LOG_ERR, "extradata already set!");
        return AVERROR(EALREADY);
    }
    if (!(self->iavctx->extradata =
              av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE))) {
        tvh_context_log(self, LOG_ERR, "failed to alloc extradata");
        return AVERROR(ENOMEM);
    }
    memcpy(self->iavctx->extradata, pktbuf_ptr(self->input_gh), extradata_size);
    self->iavctx->extradata_size = extradata_size;
    return 0;
}


/* H264 */
static TVHContextHelper TVHH264Decoder = {
    .type = AVMEDIA_TYPE_VIDEO,
    .id   = AV_CODEC_ID_H264,
    .open = tvh_extradata_open,
};

/* THEORA */
static TVHContextHelper TVHTHEORADecoder = {
    .type = AVMEDIA_TYPE_VIDEO,
    .id   = AV_CODEC_ID_THEORA,
    .open = tvh_extradata_open,
};


/* AAC */
static TVHContextHelper TVHAACDecoder = {
    .type = AVMEDIA_TYPE_AUDIO,
    .id   = AV_CODEC_ID_AAC,
    .open = tvh_extradata_open,
};


/* VORBIS */
static TVHContextHelper TVHVORBISDecoder = {
    .type = AVMEDIA_TYPE_AUDIO,
    .id   = AV_CODEC_ID_VORBIS,
    .open = tvh_extradata_open,
};


/* OPUS */
static TVHContextHelper TVHOPUSDecoder = {
    .type = AVMEDIA_TYPE_AUDIO,
    .id   = AV_CODEC_ID_OPUS,
    .open = tvh_extradata_open,
};


static void
tvh_decoder_helpers_register()
{
    SLIST_INIT(&tvh_decoder_helpers);
    /* video */
    tvh_decoder_helper_register(&TVHH264Decoder);
    tvh_decoder_helper_register(&TVHTHEORADecoder);
    /* audio */
    tvh_decoder_helper_register(&TVHAACDecoder);
    tvh_decoder_helper_register(&TVHVORBISDecoder);
    tvh_decoder_helper_register(&TVHOPUSDecoder);
}


/* encoders ================================================================= */

/* MPEG2VIDEO */
static int
tvh_mpeg2video_meta(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{
    const uint8_t *data = avpkt->data;
    size_t size = 12; // header size

    if ((avpkt->flags & AV_PKT_FLAG_KEY)) {
        // sequence header
        if (avpkt->size < size || RB32(data) != 0x000001b3) { // SEQ_START_CODE
            tvh_context_log(self, LOG_ERR,
                "MPEG2 header: missing sequence header");
            return -1;
        }
        // load intra quantiser matrix
        if (data[size-1] & 0x02) {
            if (avpkt->size < size + 64) {
                tvh_context_log(self, LOG_ERR,
                    "MPEG2 header: missing load intra quantiser matrix");
                return -1;
            }
            size += 64;
        }
        // load non-intra quantiser matrix
        if (data[size-1] & 0x01) {
            if (avpkt->size < size + 64) {
                tvh_context_log(self, LOG_ERR,
                    "MPEG2 header: missing load non-intra quantiser matrix");
                return -1;
            }
            size += 64;
        }
        // sequence extension
        if (avpkt->size < size + 10 || RB32(data + size) != 0x000001b5) { // EXT_START_CODE
            tvh_context_log(self, LOG_ERR,
                "MPEG2 header: missing sequence extension");
            return -1;
        }
        size += 10;
        // sequence display extension
        if (RB32(data + size) == 0x000001b5) { // EXT_START_CODE
            if (avpkt->size < size + 12) {
                tvh_context_log(self, LOG_ERR,
                    "MPEG2 header: missing sequence display extension");
                return -1;
            }
            size += 12;
        }
        // group of pictures
        if (avpkt->size < size + 8 || RB32(data + size) != 0x000001b8) { // GOP_START_CODE
            tvh_context_log(self, LOG_ERR,
                "MPEG2 header: missing group of pictures");
            return -1;
        }
        size += 8;
        if (!(pkt->pkt_meta = pktbuf_alloc(data, size))) {
            return -1;
        }
        return 0;
    }
    return 1;
}

static TVHContextHelper TVHMPEG2VIDEOEncoder = {
    .type   = AVMEDIA_TYPE_VIDEO,
    .id     = AV_CODEC_ID_MPEG2VIDEO,
    .meta   = tvh_mpeg2video_meta,
};


/* H264 */
static int
tvh_h264_meta(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{
    if ((avpkt->flags & AV_PKT_FLAG_KEY) && (avpkt->size > 6) &&
        (RB32(avpkt->data) == 0x00000001 || RB24(avpkt->data) == 0x000001)) {
        const uint8_t *p = avpkt->data;
        const uint8_t *end = p + avpkt->size;
        const uint8_t *nal_start, *nal_end;
        int size = 0;

        nal_start = avc_find_startcode(p, end);
        for (;;) {
            while (nal_start < end && !*(nal_start++));
            if (nal_start == end) {
                break;
            }
            nal_end = avc_find_startcode(nal_start, end);
            int nal_len = nal_end - nal_start;
            uint8_t nal_type = (nal_start[0] & 0x1f);
            if ((nal_type == H264_NAL_SPS &&
                 nal_len >= 4 && nal_len <= UINT16_MAX) ||
                (nal_type == H264_NAL_PPS &&
                 nal_len <= UINT16_MAX)) {
                size = nal_end - p;
                nal_start = nal_end;
                continue;
            }
            break;
        }
        if (size) {
            if (!(pkt->pkt_meta = pktbuf_alloc(avpkt->data, size))) {
                return -1;
            }
            return 0;
        }
    }
    return 1;
}

static TVHContextHelper TVHH264Encoder = {
    .type = AVMEDIA_TYPE_VIDEO,
    .id   = AV_CODEC_ID_H264,
    .meta = tvh_h264_meta,
};


/* HEVC */
static int
tvh_hevc_meta(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{

    if ((avpkt->flags & AV_PKT_FLAG_KEY) && (avpkt->size >= 6) &&
        (*avpkt->data != 1) &&
        (RB24(avpkt->data) == 1 || RB32(avpkt->data) == 1)) {
        const uint8_t *p = avpkt->data;
        const uint8_t *end = p + avpkt->size;
        const uint8_t *nal_start, *nal_end;
        int size = 0;

        nal_start = avc_find_startcode(p, end);
        for (;;) {
            while (nal_start < end && !*(nal_start++));
            if (nal_start == end) {
                break;
            }
            nal_end = avc_find_startcode(nal_start, end);
            uint8_t nal_type = ((nal_start[0] >> 1) & 0x3f);
            switch (nal_type) {
                case HEVC_NAL_VPS:
                case HEVC_NAL_SPS:
                case HEVC_NAL_PPS:
                case HEVC_NAL_SEI_PREFIX:
                case HEVC_NAL_SEI_SUFFIX:
                    size = nal_end - p;
                    nal_start = nal_end;
                    continue;
                default:
                    break;
            }
            break;
        }
        if (size) {
            if (!(pkt->pkt_meta = pktbuf_alloc(avpkt->data, size))) {
                return -1;
            }
            return 0;
        }
    }
    return 1;
}

static TVHContextHelper TVHHEVCEncoder = {
    .type = AVMEDIA_TYPE_VIDEO,
    .id   = AV_CODEC_ID_HEVC,
    .meta = tvh_hevc_meta,
};


/* AAC */
static void
tvh_aac_pack_adts_header(TVHContext *self, pktbuf_t *pb)
{
    // XXX: this really should happen in the muxer
    int chan_conf = (self->oavctx->channels == 8) ? 7 : self->oavctx->channels;
    bitstream_t bs;

    // https://wiki.multimedia.cx/index.php?title=ADTS
    init_wbits(&bs, pktbuf_ptr(pb), 56);     // 7 bytes
    put_bits(&bs, 0xfff, 12);                /* syncword */
    put_bits(&bs, 0, 1);                     /* version */
    put_bits(&bs, 0, 2);                     /* layer */
    put_bits(&bs, 1, 1);                     /* protection absent */
    put_bits(&bs, self->oavctx->profile, 2); /* profile (aot-1) */
    put_bits(&bs, self->sri, 4);             /* sample rate index */
    put_bits(&bs, 0, 1);                     /* private bit */
    put_bits(&bs, chan_conf, 3);             /* channel configuration */
    put_bits(&bs, 0, 1);                     /* originality */
    put_bits(&bs, 0, 1);                     /* home */
    put_bits(&bs, 0, 1);                     /* copyrighted id bit */
    put_bits(&bs, 0, 1);                     /* copyright id start */
    put_bits(&bs, pktbuf_len(pb), 13);       /* frame length */
    put_bits(&bs, 0x7ff, 11);                /* buffer fullness */
    put_bits(&bs, 0, 2);                     /* rdbs in frame (-1)*/
}

static th_pkt_t *
tvh_aac_pack(TVHContext *self, AVPacket *avpkt)
{
    static const size_t header_size = 7, max_size = ((1 << 13) - 1);
    size_t pkt_size = 0;
    th_pkt_t *pkt = NULL;

    // afaict: we write an adts header if there isn't one already, why would
    // there be one in the first place?.
    // originally there was a check for avpkt->size < 2, I don't get it.
    // max aac frame size = 768 bytes per channel, max writable size 13 bits
    if (avpkt->size > (768 * self->oavctx->channels)) {
        tvh_context_log(self, LOG_WARNING,
            "packet size (%d) > aac max frame size (%d for %d channels)",
            avpkt->size, (768 * self->oavctx->channels),
            self->oavctx->channels);
    }
    if ((pkt_size = avpkt->size + header_size) > max_size) {
        tvh_context_log(self, LOG_ERR, "aac frame data too big");
    }
    else if (avpkt->data[0] != 0xff || (avpkt->data[1] & 0xf0) != 0xf0) {
        if ((pkt = pkt_alloc(self->stream->type, NULL, pkt_size, avpkt->pts, avpkt->dts, 0))) {
            tvh_aac_pack_adts_header(self, pkt->pkt_payload);
            memcpy(pktbuf_ptr(pkt->pkt_payload) + header_size,
                   avpkt->data, avpkt->size);
        }
    }
    else {
        pkt = pkt_alloc(self->stream->type, avpkt->data, avpkt->size, avpkt->pts, avpkt->dts, 0);
    }
    return pkt;
}

static TVHContextHelper TVHAACEncoder = {
    .type = AVMEDIA_TYPE_AUDIO,
    .id   = AV_CODEC_ID_AAC,
    .pack = tvh_aac_pack,
};


static void
tvh_encoder_helpers_register()
{
    SLIST_INIT(&tvh_encoder_helpers);
    /* video */
    tvh_encoder_helper_register(&TVHMPEG2VIDEOEncoder);
    tvh_encoder_helper_register(&TVHH264Encoder);
    tvh_encoder_helper_register(&TVHHEVCEncoder);
    /* audio */
    tvh_encoder_helper_register(&TVHAACEncoder);
}


/* public =================================================================== */

TVHContextHelper *
tvh_decoder_helper_find(const AVCodec *codec)
{
    return tvh_context_helper_find(&tvh_decoder_helpers, codec);
}


TVHContextHelper *
tvh_encoder_helper_find(const AVCodec *codec)
{
    return tvh_context_helper_find(&tvh_encoder_helpers, codec);
}


void
tvh_context_helpers_register()
{
    tvh_decoder_helpers_register();
    tvh_encoder_helpers_register();
}


void
tvh_context_helpers_forget()
{
    tvhinfo(LS_TRANSCODE, "forgetting context helpers");
    while (!SLIST_EMPTY(&tvh_encoder_helpers)) {
        SLIST_REMOVE_HEAD(&tvh_encoder_helpers, link);
    }
    while (!SLIST_EMPTY(&tvh_decoder_helpers)) {
        SLIST_REMOVE_HEAD(&tvh_decoder_helpers, link);
    }
}
