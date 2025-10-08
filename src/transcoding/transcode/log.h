/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
 */

#ifndef TVH_TRANSCODING_TRANSCODE_LOG_H__
#define TVH_TRANSCODING_TRANSCODE_LOG_H__


#define tvh_transcoder_log(self, level, fmt, ...) \
    do { \
        tvhlog((level), LS_TRANSCODE, "%04X: " fmt, \
            (((self)->id) & 0xffff), \
            ##__VA_ARGS__); \
    } while (0)

#define _stream_log(level, fmt, transcoder, id, type, ...) \
    do { \
        tvh_transcoder_log((transcoder), (level), "%02d:%s: " fmt, \
            (id), \
            streaming_component_type2txt((type)), \
            ##__VA_ARGS__); \
    } while (0)

#define tvh_ssc_log(ssc, level, fmt, transcoder, ...) \
    do { \
        _stream_log((level), fmt, \
            (transcoder), \
            (ssc)->es_index, \
            (ssc)->es_type, \
            ##__VA_ARGS__); \
    } while (0)

#define tvh_stream_log(self, level, fmt, ...) \
    do { \
        _stream_log((level), fmt, \
            (self)->transcoder, \
            (self)->id, \
            (self)->type, \
            ##__VA_ARGS__); \
    } while (0)

#define tvh_context_log(self, level, fmt, ...) \
    do { \
        tvh_stream_log((self)->stream, (level), "[%s => %s]: " fmt, \
            ((self)->iavctx && (self)->iavctx->codec) ? (self)->iavctx->codec->name : "<unknown>", \
            ((self)->oavctx && (self)->oavctx->codec) ? (self)->oavctx->codec->name : "<unknown>", \
            ##__VA_ARGS__); \
    } while (0)


#endif // TVH_TRANSCODING_TRANSCODE_LOG_H__
