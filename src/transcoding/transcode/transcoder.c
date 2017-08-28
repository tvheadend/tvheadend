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


static TVHCodecProfile _codec_profile_copy = { .name = "copy" };
TVHCodecProfile *tvh_codec_profile_copy = &_codec_profile_copy;


/* utils ==================================================================== */

static enum AVMediaType
ssc_get_media_type(tvh_ssc_t *ssc)
{
    if (ssc->ssc_disabled) {
        return AVMEDIA_TYPE_UNKNOWN;
    }
    if (SCT_ISVIDEO(ssc->ssc_type)) {
        return AVMEDIA_TYPE_VIDEO;
    }
    if (SCT_ISAUDIO(ssc->ssc_type)) {
        return AVMEDIA_TYPE_AUDIO;
    }
    if (SCT_ISSUBTITLE(ssc->ssc_type)) {
        return AVMEDIA_TYPE_SUBTITLE;
    }
    return AVMEDIA_TYPE_UNKNOWN;
}


static int
stream_count(tvh_ss_t *ss, TVHCodecProfile **profiles)
{
    int i, count = 0;
    tvh_ssc_t *ssc = NULL;

    for (i = 0; i < ss->ss_num_components; i++) {
        if ((ssc = &ss->ss_components[i]) &&
             ssc_get_media_type(ssc) != AVMEDIA_TYPE_UNKNOWN) {
            count++;
        }
    }
    return count;
}


static TVHCodecProfile *
_find_profile(const char *name)
{
    if (!strcmp(name, "copy")) {
        return tvh_codec_profile_copy;
    }
    return codec_find_profile(name);
}


/* TVHTranscoder ============================================================ */

static void
tvh_transcoder_handle(TVHTranscoder *self, th_pkt_t *pkt)
{
    TVHStream *stream = NULL;

    SLIST_FOREACH(stream, &self->streams, link) {
        if (pkt->pkt_componentindex == stream->index) {
            if (tvh_stream_handle(stream, pkt)) {
                tvh_stream_stop(stream, 0);
            }
            break;
        }
    }
}


static tvh_ss_t *
tvh_transcoder_start(TVHTranscoder *self, tvh_ss_t *ss_src)
{
    // TODO: rewrite, include language selection
    tvh_ss_t *ss = NULL;
    tvh_ssc_t *ssc_src = NULL, *ssc = NULL;
    TVHCodecProfile *profile = NULL;
    TVHStream *stream = NULL;
    int i, j, count = stream_count(ss_src, self->profiles);
    enum AVMediaType media_type;

    ss = calloc(1, (sizeof(tvh_ss_t) + (sizeof(tvh_ssc_t) * count)));
    if (ss) {
        ss->ss_refcount = 1;
        ss->ss_pcr_pid = ss_src->ss_pcr_pid;
        ss->ss_pmt_pid = ss_src->ss_pmt_pid;
        service_source_info_copy(&ss->ss_si, &ss_src->ss_si);
        ss->ss_num_components = count;
        for (i = j = 0; i < ss_src->ss_num_components && j < count; i++) {
            ssc_src = &ss_src->ss_components[i];
            ssc = &ss->ss_components[j];
            if (ssc) {
                media_type = ssc_get_media_type(ssc_src);
                if (media_type != AVMEDIA_TYPE_UNKNOWN) {
                    profile = self->profiles[media_type];
                    *ssc = *ssc_src;
                    j++;
                    if ((stream = tvh_stream_create(self, profile, ssc,
                                                    self->src_codecs[media_type]))) {
                        SLIST_INSERT_HEAD(&self->streams, stream, link);
                    }
                    else {
                        tvh_ssc_log(ssc, LOG_ERR, "==> Filtered out", self);
                    }
                }
                else {
                    tvh_ssc_log(ssc, LOG_INFO, "==> Filtered out", self);
                }
            }
        }
    }
    return ss;
}


static void
tvh_transcoder_stop(TVHTranscoder *self, int flush)
{
    TVHStream *stream = NULL;

    SLIST_FOREACH(stream, &self->streams, link) {
        tvh_stream_stop(stream, flush);
    }
}


static void
tvh_transcoder_stream(void *opaque, tvh_sm_t *msg)
{
    TVHTranscoder *self = opaque;
    tvh_ss_t *ss = NULL;

    switch (msg->sm_type) {
        case SMT_PACKET:
            if(msg->sm_data) {
                tvh_transcoder_handle(self, msg->sm_data);
                TVHPKT_CLEAR(msg->sm_data);
            }
            streaming_msg_free(msg);
            break;
        case SMT_START:
            if (msg->sm_data) {
                ss = tvh_transcoder_start(self, msg->sm_data);
                streaming_start_unref(msg->sm_data);
                msg->sm_data = ss;
            }
            streaming_target_deliver2(self->output, msg);
            break;
        case SMT_STOP:
            tvh_transcoder_stop(self, 1);
            /* !!! FALLTHROUGH !!! */
        default:
            streaming_target_deliver2(self->output, msg);
            break;
    }
}


static int
tvh_transcoder_setup(TVHTranscoder *self,
                     const char **profiles,
                     const char **src_codecs)
{
    const char *profile = NULL;
    int i;

    for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
        if ((profile = profiles[i]) && strlen(profile)) {
            if (!(self->profiles[i] = _find_profile(profile))) {
                tvh_transcoder_log(self, LOG_ERR,
                                   "failed to find codec profile: '%s'", profile);
                return -1;
            }
            if (src_codecs[i])
                self->src_codecs[i] = strdup(src_codecs[i]);
        }
    }
    return 0;
}


static htsmsg_t *
tvh_transcoder_info(void *opaque, htsmsg_t *list)
{
  TVHTranscoder *self = opaque;
  streaming_target_t *st = self->output;
  htsmsg_add_str(list, NULL, "transcoder input");
  return st->st_ops.st_info(st->st_opaque, list);
}


/* exposed */

int
tvh_transcoder_deliver(TVHTranscoder *self, th_pkt_t *pkt)
{
    tvh_sm_t *msg = NULL;

    if (!(msg = streaming_msg_create_pkt(pkt))) { // takes ownership of pkt
        tvh_transcoder_log(self, LOG_ERR, "failed to create message");
        return -1;
    }
    streaming_target_deliver2(self->output, msg);
    return 0;
}


static streaming_ops_t tvh_transcoder_ops = {
  .st_cb   = tvh_transcoder_stream,
  .st_info = tvh_transcoder_info
};


TVHTranscoder *
tvh_transcoder_create(tvh_st_t *output,
                      const char **profiles,
                      const char **src_codecs)
{
    static uint32_t id = 0;
    TVHTranscoder *self = NULL;

    if (!(self = calloc(1, sizeof(TVHTranscoder)))) {
        tvherror(LS_TRANSCODE, "failed to allocate transcoder");
        return NULL;
    }
    SLIST_INIT(&self->streams);
    self->id = ++id;
    if (!self->id) {
        self->id = ++id;
    }
    self->output = output;
    if (tvh_transcoder_setup(self, profiles, src_codecs)) {
        tvh_transcoder_destroy(self);
        return NULL;
    }
    streaming_target_init(&self->input, &tvh_transcoder_ops, self, 0);
    return self;
}


void
tvh_transcoder_destroy(TVHTranscoder *self)
{
    TVHStream *stream = NULL;
    int i;

    if (self) {
        tvh_transcoder_stop(self, 0);
        self->output = NULL;
        while (!SLIST_EMPTY(&self->streams)) {
            stream = SLIST_FIRST(&self->streams);
            SLIST_REMOVE_HEAD(&self->streams, link);
            tvh_stream_destroy(stream);
        }
        for (i = 0; i < AVMEDIA_TYPE_NB; i++)
          free(self->src_codecs[i]);
        free(self);
        self = NULL;
    }
}
