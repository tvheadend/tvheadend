/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2023 Tvheadend
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


#ifndef TVH_TRANSCODING_TRANSCODE_CODEC_VAINFO_H__
#define TVH_TRANSCODING_TRANSCODE_CODEC_VAINFO_H__


/**
 * VAINFO VAINFO_DONT_SHOW_LOGS.
 *
 * @note
 * Will not print output with detected codec/profiles
 * 
 */
#define VAINFO_DONT_SHOW_LOGS   0

/**
 * VAINFO VAINFO_SHOW_LOGS.
 *
 * @note
 * Will print output with detected codec/profiles
 * 
 */
#define VAINFO_SHOW_LOGS        1


/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for H264
 */
#define VAINFO_H264                 1
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for H264 low power
 */
#define VAINFO_H264_LOW_POWER       2
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for HEVC
 */
#define VAINFO_HEVC                 3
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for HEVC low power
 */
#define VAINFO_HEVC_LOW_POWER       4
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for VP8
 */
#define VAINFO_VP8                  5
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for VP8 low power
 */
#define VAINFO_VP8_LOW_POWER        6
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for VP9
 */
#define VAINFO_VP9                  7
/**
 * VAINFO CODEC DEFINE.
 * @note
 * Define used when calling functions for VP9 low power
 */
#define VAINFO_VP9_LOW_POWER        8

/**
 * VAINFO initialize.
 *
 * @note
 * Initialize all internal variables according to VAAPI advertised feature
 * parameter: show_log
 * 1 = will show vainfo logs with available VAAPI entries
 * 0 = no logs generated
 * 
 */
int vainfo_init(int show_log);


/**
 * VAINFO Encoder availablity.
 *
 * @note
 * param: CODEC_ID
 *
 * return: 
 * 0 - if encoder is not available
 * 1 - if encoder is available
 * 
 */
int vainfo_encoder_isavailable(int codec);


/**
 * VAINFO Encoder support for B frames.
 *
 * @note
 * return: 
 * 0 - if encoder is not supporting B frames
 * > 0 - if encoder is supporting B frames and how many (MAX = 7)
 * 
 */
int vainfo_encoder_maxBfreames(int codec);


/**
 * VAINFO Encoder max Quality.
 *
 * @note
 * return: 
 * 0 - if encoder is not supporting Quality
 * 1 - if encoder is supporting Quality and how much (MAX = 15)
 * 
 */
int vainfo_encoder_maxQuality(int codec);


/**
 * VAINFO deinitialize.
 *
 * @note
 * Return all variables to default state
 * 
 */
void vainfo_deinit(void);

#endif // TVH_TRANSCODING_TRANSCODE_CODEC_VAINFO_H__