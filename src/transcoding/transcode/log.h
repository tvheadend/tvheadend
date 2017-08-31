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
            (ssc)->ssc_index, \
            (ssc)->ssc_type, \
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
            ((self)->iavctx) ? (self)->iavctx->codec->name : "<unknown>", \
            ((self)->oavctx) ? (self)->oavctx->codec->name : "<unknown>", \
            ##__VA_ARGS__); \
    } while (0)


#endif // TVH_TRANSCODING_TRANSCODE_LOG_H__
