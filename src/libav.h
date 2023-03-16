/*
 *  tvheadend, libav utils
 *  Copyright (C) 2012 John Törnblom
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef LIBAV_H_
#define LIBAV_H_

#include "build.h"

#if ENABLE_LIBAV

#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include "esstream.h"

/*
Older versions of ffmpeg/libav don't have the AV_* prefix

For version info please see:
https://github.com/libav/libav/blob/a7153444df9040bf6ae103e0bbf6104b66f974cb/doc/APIchanges#L450-455
https://github.com/FFmpeg/FFmpeg/blob/97478ef5fe7dd2ff8da98e381de4a6b2b979b485/doc/APIchanges#L811-816
http://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=104e10fb426f903ba9157fdbfe30292d0e4c3d72

This list must be updated every time we use a new AV_CODEC_ID
*/
#if LIBAVCODEC_VERSION_MAJOR < 54 || (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR < 25)
#define AVCodecID CodecID
#define AV_CODEC_ID_AAC          CODEC_ID_AAC
#define AV_CODEC_ID_AC3          CODEC_ID_AC3
#define AV_CODEC_ID_DVB          CODEC_ID_DVB
#define AV_CODEC_ID_EAC3         CODEC_ID_EAC3
#define AV_CODEC_ID_H264         CODEC_ID_H264
#define AV_CODEC_ID_MP2          CODEC_ID_MP2
#define AV_CODEC_ID_MPEG2VIDEO   CODEC_ID_MPEG2VIDEO
#define AV_CODEC_ID_NONE         CODEC_ID_NONE
#define AV_CODEC_ID_TEXT         CODEC_ID_TEXT
#define AV_CODEC_ID_VORBIS       CODEC_ID_VORBIS
#define AV_CODEC_ID_VP8          CODEC_ID_VP8
#define AV_CODEC_ID_DVB_SUBTITLE CODEC_ID_DVB_SUBTITLE
#define AV_CODEC_ID_DVB_TELETEXT CODEC_ID_DVB_TELETEXT
#endif

enum AVCodecID streaming_component_type2codec_id(streaming_component_type_t type);
streaming_component_type_t codec_id2streaming_component_type(enum AVCodecID id);
void libav_set_loglevel(void);
void libav_vaapi_init_context(void *context);
void libav_init(void);
void libav_done(void);

#else

static inline void libav_set_loglevel(void) { };
static inline void libav_vaapi_init_context(void *context) { };
static inline void libav_init(void) { };
static inline void libav_done(void) { };

#endif

#endif
