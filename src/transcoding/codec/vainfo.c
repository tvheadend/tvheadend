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


#include "vainfo.h"
#include "internals.h"

#if ENABLE_VAAPI
#include <va/va.h>
#include <fcntl.h>
#include <va/va_drm.h>
#include <va/va_str.h>
#endif

#define CODEC_IS_AVAILABLE      1
#define CODEC_IS_NOT_AVAILABLE  0
#define MIN_B_FRAMES            0
#define MAX_B_FRAMES            7
#define MIN_QUALITY             0
#define MAX_QUALITY             15

/* external =================================================================== */
#if ENABLE_VAAPI
// this variable is loaded from config: Enable vaapi detection
extern int vainfo_probe_enabled;
#endif

/* internal =================================================================== */
int encoder_h264_isavailable = 0;
int encoder_h264lp_isavailable = 0;
int encoder_hevc_isavailable = 0;
int encoder_hevclp_isavailable = 0;
int encoder_vp8_isavailable = 0;
int encoder_vp8lp_isavailable = 0;
int encoder_vp9_isavailable = 0;
int encoder_vp9lp_isavailable = 0;

int encoder_h264_maxBfreames = 0;
int encoder_h264lp_maxBfreames = 0;
int encoder_hevc_maxBfreames = 0;
int encoder_hevclp_maxBfreames = 0;
int encoder_vp8_maxBfreames = 0;
int encoder_vp8lp_maxBfreames = 0;
int encoder_vp9_maxBfreames = 0;
int encoder_vp9lp_maxBfreames = 0;

int encoder_h264_maxQuality = 0;
int encoder_h264lp_maxQuality = 0;
int encoder_hevc_maxQuality = 0;
int encoder_hevclp_maxQuality = 0;
int encoder_vp8_maxQuality = 0;
int encoder_vp8lp_maxQuality = 0;
int encoder_vp9_maxQuality = 0;
int encoder_vp9lp_maxQuality = 0;

#if ENABLE_VAAPI
/**
 * VAINFO was initialized
 * @note
 * return: 
 * 0 - not initialized --> will return invalid data
 * 1 - initialized properly
 * 
 * NOTE: initialization was performed in /src/transcoding/codec/codec.c
 */
int init_done = 0;

int init(int show_log);

static int drm_fd = -1;

static VADisplay
va_open_display_drm(void)
{
    VADisplay va_dpy;
    int i;
    static const char *drm_device_paths[] = {
        "/dev/dri/renderD128",
        "/dev/dri/card0",
        "/dev/dri/renderD129",
        "/dev/dri/card1",
        NULL
    };
    for (i = 0; drm_device_paths[i]; i++) {
        drm_fd = open(drm_device_paths[i], O_RDWR);
        if (drm_fd < 0)
            continue;
        va_dpy = vaGetDisplayDRM(drm_fd);
        if (va_dpy)
            return va_dpy;

        close(drm_fd);
        drm_fd = -1;
    }
    return NULL;
}

static void
va_close_display_drm(VADisplay va_dpy)
{
    if (drm_fd < 0)
        return;

    close(drm_fd);
    drm_fd = -1;
}


static int 
get_config_attributes(VADisplay va_dpy, VAProfile profile, VAEntrypoint entrypoint, int show_log, int codec)
{

    VAStatus va_status;
    int i, temp;

    VAConfigAttrib attrib_list[VAConfigAttribTypeMax];
    int max_num_attributes = VAConfigAttribTypeMax;

    for (i = 0; i < max_num_attributes; i++) {
        attrib_list[i].type = i;
    }

    va_status = vaGetConfigAttributes(va_dpy,
                                      profile, entrypoint,
                                      attrib_list, max_num_attributes);
    if (VA_STATUS_ERROR_UNSUPPORTED_PROFILE == va_status ||
        VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT == va_status)
        return -1;

    if (attrib_list[VAConfigAttribEncMaxRefFrames].value & (~VA_ATTRIB_NOT_SUPPORTED)) {
        if (show_log) {
            tvhinfo_transcode(LST_VAINFO, "            %-35s: l0=%d, l1=%d", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncMaxRefFrames].type),
                attrib_list[VAConfigAttribEncMaxRefFrames].value & 0xffff, 
                (attrib_list[VAConfigAttribEncMaxRefFrames].value >> 16) & 0xffff);
        }
        temp = (attrib_list[VAConfigAttribEncMaxRefFrames].value >> 16) & 0xffff;
        if (temp)
            // this value has to be increased by 1
            temp++;
        // limit to max space available in ui
        if (temp > MAX_B_FRAMES) {
            tvherror_transcode(LST_VAINFO, "show_config_attributes() failed to set max B frames (vainfo:%d --> max=%d)", temp, MAX_B_FRAMES);
            temp = MAX_B_FRAMES;
        }
        switch (codec) {
            case VAINFO_H264:
                encoder_h264_maxBfreames = temp;
                break;
            case VAINFO_H264_LOW_POWER:
                encoder_h264lp_maxBfreames = temp;
                break;
            case VAINFO_HEVC:
                encoder_hevc_maxBfreames = temp;
                break;
            case VAINFO_HEVC_LOW_POWER:
                encoder_hevclp_maxBfreames = temp;
                break;
            case VAINFO_VP8:
                encoder_vp8_maxBfreames = temp;
                break;
            case VAINFO_VP8_LOW_POWER:
                encoder_vp8lp_maxBfreames = temp;
                break;
            case VAINFO_VP9:
                encoder_vp9_maxBfreames = temp;
                break;
            case VAINFO_VP9_LOW_POWER:
                encoder_vp9lp_maxBfreames = temp;
                break;
            default:
                tvherror_transcode(LST_VAINFO, "codec not available: codec=%d", codec);
                break;
        }
    }

    if (attrib_list[VAConfigAttribEncQualityRange].value != VA_ATTRIB_NOT_SUPPORTED) {
        if (show_log) {
            tvhinfo_transcode(LST_VAINFO, "            %-35s: number of supported quality levels is %d", vaConfigAttribTypeStr(attrib_list[VAConfigAttribEncQualityRange].type),
                attrib_list[VAConfigAttribEncQualityRange].value <= 1 ? 1 : attrib_list[VAConfigAttribEncQualityRange].value);
        }
        temp = attrib_list[VAConfigAttribEncQualityRange].value <= 1 ? 1 : attrib_list[VAConfigAttribEncQualityRange].value;
        // limit to max space available in ui
        if (temp > MAX_QUALITY) {
            tvherror_transcode(LST_VAINFO, "show_config_attributes() failed to set max quality (vainfo:%d --> max=%d)", temp, MAX_QUALITY);
            temp = MAX_QUALITY;
        }
        switch (codec) {
            case VAINFO_H264:
                encoder_h264_maxQuality = temp;
                break;
            case VAINFO_H264_LOW_POWER:
                encoder_h264lp_maxQuality = temp;
                break;
            case VAINFO_HEVC:
                encoder_hevc_maxQuality = temp;
                break;
            case VAINFO_HEVC_LOW_POWER:
                encoder_hevclp_maxQuality = temp;
                break;
            case VAINFO_VP8:
                encoder_vp8_maxQuality = temp;
                break;
            case VAINFO_VP8_LOW_POWER:
                encoder_vp8lp_maxQuality = temp;
                break;
            case VAINFO_VP9:
                encoder_vp9_maxQuality = temp;
                break;
            case VAINFO_VP9_LOW_POWER:
                encoder_vp9lp_maxQuality = temp;
                break;
            default:
                tvherror_transcode(LST_VAINFO, "codec not available: codec=%d", codec);
                break;
        }
    }
    return 0;
}

int init(int show_log)
{
    VADisplay va_dpy;
    VAStatus va_status;
    int major_version, minor_version;
    int ret_val;
    const char *driver;
    int num_entrypoint = 0;
    VAEntrypoint entrypoint, *entrypoints = NULL;
    int num_profiles, max_num_profiles, i;
    VAProfile profile, *profile_list = NULL;

    va_dpy = va_open_display_drm();
    if (NULL == va_dpy) {
        tvherror_transcode(LST_VAINFO, "vaGetDisplay() failed");
        ret_val = 2;                                                      \
        goto error_open_display;
    }

    va_status = vaInitialize(va_dpy, &major_version, &minor_version);
    if (va_status != VA_STATUS_SUCCESS) { 
        tvherror_transcode(LST_VAINFO, "vaInitialize failed with error code %d (%s)", va_status, vaErrorStr(va_status));
        ret_val = 3;
        goto error_Initialize;
    }
    if (show_log)
        tvhinfo_transcode(LST_VAINFO, "VA-API version: %d.%d", major_version, minor_version);

    driver = vaQueryVendorString(va_dpy);
    if (show_log)
        tvhinfo_transcode(LST_VAINFO, "Driver version: %s", driver ? driver : "<unknown>");

    num_entrypoint = vaMaxNumEntrypoints(va_dpy);
    entrypoints = malloc(num_entrypoint * sizeof(VAEntrypoint));
    if (!entrypoints) {
        tvherror_transcode(LST_VAINFO, "Failed to allocate memory for entrypoint list");
        ret_val = -1;
        goto error_entrypoints;
    }

    max_num_profiles = vaMaxNumProfiles(va_dpy);
    profile_list = malloc(max_num_profiles * sizeof(VAProfile));

    if (!profile_list) {
        tvherror_transcode(LST_VAINFO, "Failed to allocate memory for profile list");
        ret_val = 5;
        goto error_profile_list;
    }

    va_status = vaQueryConfigProfiles(va_dpy, profile_list, &num_profiles);
    if (va_status != VA_STATUS_SUCCESS) { 
        tvherror_transcode(LST_VAINFO, "vaQueryConfigProfiles failed with error code %d (%s)", va_status, vaErrorStr(va_status));
        ret_val = 6;
        goto error_QueryConfigProfiles;
    }

    for (i = 0; i < num_profiles; i++) {
        profile = profile_list[i];
        va_status = vaQueryConfigEntrypoints(va_dpy, profile, entrypoints,
                                                &num_entrypoint);
        if (va_status == VA_STATUS_ERROR_UNSUPPORTED_PROFILE)
            continue;

        if (va_status != VA_STATUS_SUCCESS) { 
            tvherror_transcode(LST_VAINFO, "vaQueryConfigEntrypoints failed with error code %d (%s)", va_status, vaErrorStr(va_status));
            ret_val = 4;
            goto error_QueryConfigProfiles;
        }

        for (entrypoint = 0; entrypoint < num_entrypoint; entrypoint++) {
            if (show_log)
                tvhinfo_transcode(LST_VAINFO, "       %-32s:	%s", vaProfileStr(profile), vaEntrypointStr(entrypoints[entrypoint]));
            // h264
            if (profile == VAProfileH264High || profile == VAProfileH264ConstrainedBaseline || profile == VAProfileH264Main) {
                if (entrypoints[entrypoint] == VAEntrypointEncSlice) {
                    encoder_h264_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_H264);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
                if (entrypoints[entrypoint] == VAEntrypointEncSliceLP) {
                    encoder_h264lp_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_H264_LOW_POWER);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
            }
            // hevc
            if (profile == VAProfileHEVCMain || profile == VAProfileHEVCMain10) {
                if (entrypoints[entrypoint] == VAEntrypointEncSlice) {
                    encoder_hevc_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_HEVC);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
                if (entrypoints[entrypoint] == VAEntrypointEncSliceLP) {
                    encoder_hevclp_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_HEVC_LOW_POWER);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
            }
            // vp8
            if (profile == VAProfileVP8Version0_3) {
                if (entrypoints[entrypoint] == VAEntrypointEncSlice) {
                    encoder_vp8_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_VP8);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
                if (entrypoints[entrypoint] == VAEntrypointEncSliceLP) {
                    encoder_vp8lp_isavailable = 1;
                    // extract attributes
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_VP8_LOW_POWER);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
            }
            // vp9
            if (profile == VAProfileVP9Profile0 || profile == VAProfileVP9Profile1 || profile == VAProfileVP9Profile2 || profile == VAProfileVP9Profile3) {
                if (entrypoints[entrypoint] == VAEntrypointEncSlice) {
                    encoder_vp9_isavailable = 1;
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_VP9);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
                if (entrypoints[entrypoint] == VAEntrypointEncSliceLP) {
                    encoder_vp9lp_isavailable = 1;
                    ret_val = get_config_attributes(va_dpy, profile_list[i], entrypoints[entrypoint], show_log, VAINFO_VP9_LOW_POWER);
                    if (ret_val) {
                        tvherror_transcode(LST_VAINFO, "Failed to get config attributes (error %d)", ret_val);
                    }
                }
            }
        }
    }
    init_done = 1;
    ret_val = 0;

error_QueryConfigProfiles:
error_profile_list:
    free(profile_list);
error_entrypoints:
    free(entrypoints);
error_Initialize:
    vaTerminate(va_dpy);
error_open_display:
    va_close_display_drm(va_dpy);
    // if we had any errors we enable show all codecs (for debugging)
    if (ret_val) {
        encoder_h264_isavailable = 1;
        encoder_h264lp_isavailable = 1;
        encoder_hevc_isavailable = 1;
        encoder_hevclp_isavailable = 1;
        encoder_vp8_isavailable = 1;
        encoder_vp8lp_isavailable = 1;
        encoder_vp9_isavailable = 1;
        encoder_vp9lp_isavailable = 1;
    }

    return ret_val;
}
#endif

/* exposed =================================================================== */

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
int vainfo_init(int show_log)
{
#if ENABLE_VAAPI
    int ret = init(show_log);
    if (ret) {
        tvherror_transcode(LST_VAINFO, "vainfo_init() error: %d", ret);
        return ret;
    }
#endif
    return 0;
}

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
int vainfo_encoder_isavailable(int codec)
{
#if ENABLE_VAAPI
    if (vainfo_probe_enabled) {
        if (!init_done)
            tvherror_transcode(LST_VAINFO, "vainfo_init() was not run or generated errors");
        switch (codec) {
            case VAINFO_H264:
                return encoder_h264_isavailable;
            case VAINFO_H264_LOW_POWER:
                return encoder_h264lp_isavailable;
            case VAINFO_HEVC:
                return encoder_hevc_isavailable;
            case VAINFO_HEVC_LOW_POWER:
                return encoder_hevclp_isavailable;
            case VAINFO_VP8:
                return encoder_vp8_isavailable;
            case VAINFO_VP8_LOW_POWER:
                return encoder_vp8lp_isavailable;
            case VAINFO_VP9:
                return encoder_vp9_isavailable;
            case VAINFO_VP9_LOW_POWER:
                return encoder_vp9lp_isavailable;
            default:
                tvherror_transcode(LST_VAINFO, "codec not available: codec=%d", codec);
                return CODEC_IS_NOT_AVAILABLE;
        }
    }
    else
#endif
        return CODEC_IS_AVAILABLE;
}


/**
 * VAINFO Encoder support for B frames.
 *
 * @note
 * return: 
 * 0 - if encoder is not supporting B frames
 * > 0 - if encoder is supporting B frames and how many (MAX = 7)
 * 
 */
int vainfo_encoder_maxBfreames(int codec)
{
#if ENABLE_VAAPI
    if (vainfo_probe_enabled) {
        if (!init_done)
            tvherror_transcode(LST_VAINFO, "vainfo_init() was not run or generated errors");
        switch (codec) {
            case VAINFO_H264:
                return encoder_h264_maxBfreames;
            case VAINFO_H264_LOW_POWER:
                return encoder_h264lp_maxBfreames;
            case VAINFO_HEVC:
                return encoder_hevc_maxBfreames;
            case VAINFO_HEVC_LOW_POWER:
                return encoder_hevclp_maxBfreames;
            case VAINFO_VP8:
                return encoder_vp8_maxBfreames;
            case VAINFO_VP8_LOW_POWER:
                return encoder_vp8lp_maxBfreames;
            case VAINFO_VP9:
                return encoder_vp9_maxBfreames;
            case VAINFO_VP9_LOW_POWER:
                return encoder_vp9lp_maxBfreames;
            default:
                tvherror_transcode(LST_VAINFO, "codec not available: codec=%d", codec);
                return MIN_B_FRAMES;
        }
    }
    else
#endif
        return MAX_B_FRAMES;
}


/**
 * VAINFO Encoder max Quality.
 *
 * @note
 * return: 
 * 0 - if encoder is not supporting Quality
 * 1 - if encoder is supporting Quality and how much (MAX = 15)
 * 
 */
int vainfo_encoder_maxQuality(int codec)
{
#if ENABLE_VAAPI
    if (vainfo_probe_enabled) {
        if (!init_done)
            tvherror_transcode(LST_VAINFO, "vainfo_init() was not run or generated errors");
        switch (codec) {
            case VAINFO_H264:
                return encoder_h264_maxQuality;
            case VAINFO_H264_LOW_POWER:
                return encoder_h264lp_maxQuality;
            case VAINFO_HEVC:
                return encoder_hevc_maxQuality;
            case VAINFO_HEVC_LOW_POWER:
                return encoder_hevclp_maxQuality;
            case VAINFO_VP8:
                return encoder_vp8_maxQuality;
            case VAINFO_VP8_LOW_POWER:
                return encoder_vp8lp_maxQuality;
            case VAINFO_VP9:
                return encoder_vp9_maxQuality;
            case VAINFO_VP9_LOW_POWER:
                return encoder_vp9lp_maxQuality;
            default:
                tvherror_transcode(LST_VAINFO, "codec not available: codec=%d", codec);
                return MIN_QUALITY;
        }
    }
    else
#endif
        return MAX_QUALITY;
}


/**
 * VAINFO deinitialize.
 *
 * @note
 * Return all variables to default state
 * 
 */
void vainfo_deinit()
{
    // this function should not be called
}