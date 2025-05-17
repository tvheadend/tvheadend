/*
 *  tvheadend - Codec Profiles
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


#include "transcoding/codec/internals.h"
#include <fcntl.h>
#include <sys/ioctl.h>

#include "idnode.h"
#include "htsmsg.h"
#include "transcoding/codec/vainfo.h"

/* defines */
#define VAAPI_ENC_PLATFORM_UNCONSTRAINED	      0
#define VAAPI_ENC_PLATFORM_INTEL			      1
#define VAAPI_ENC_PLATFORM_AMD   			      2

#define VAAPI_ENC_PARAMS_RC_SKIP                  -1
#define VAAPI_ENC_PARAMS_RC_AUTO                  0
#define VAAPI_ENC_PARAMS_RC_CONSTQP               1
#define VAAPI_ENC_PARAMS_RC_CBR                   2
#define VAAPI_ENC_PARAMS_RC_VBR                   3
#define VAAPI_ENC_PARAMS_RC_ICQ                   4
#define VAAPI_ENC_PARAMS_RC_QVBR                  5
#define VAAPI_ENC_PARAMS_RC_AVBR                  6

#define VAAPI_ENC_LEVEL_H264_SKIP                 -100
#define VAAPI_ENC_LEVEL_H264_1                    10
#define VAAPI_ENC_LEVEL_H264_11                   11
#define VAAPI_ENC_LEVEL_H264_12                   12
#define VAAPI_ENC_LEVEL_H264_13                   13
#define VAAPI_ENC_LEVEL_H264_2                    20
#define VAAPI_ENC_LEVEL_H264_21                   21
#define VAAPI_ENC_LEVEL_H264_22                   22
#define VAAPI_ENC_LEVEL_H264_3                    30
#define VAAPI_ENC_LEVEL_H264_31                   31
#define VAAPI_ENC_LEVEL_H264_32                   32
#define VAAPI_ENC_LEVEL_H264_4                    40
#define VAAPI_ENC_LEVEL_H264_41                   41
#define VAAPI_ENC_LEVEL_H264_42                   42
#define VAAPI_ENC_LEVEL_H264_5                    50
#define VAAPI_ENC_LEVEL_H264_51                   51
#define VAAPI_ENC_LEVEL_H264_52                   52
#define VAAPI_ENC_LEVEL_H264_6                    60
#define VAAPI_ENC_LEVEL_H264_61                   61
#define VAAPI_ENC_LEVEL_H264_62                   62

#define VAAPI_ENC_HEVC_TIER_SKIP			      -1
#define VAAPI_ENC_HEVC_TIER_MAIN			      0
#define VAAPI_ENC_HEVC_TIER_HIGH 			      1

#define VAAPI_ENC_LEVEL_HEVC_SKIP                 -100
#define VAAPI_ENC_LEVEL_HEVC_1                    30
#define VAAPI_ENC_LEVEL_HEVC_2                    60
#define VAAPI_ENC_LEVEL_HEVC_21                   63
#define VAAPI_ENC_LEVEL_HEVC_3                    90
#define VAAPI_ENC_LEVEL_HEVC_31                   93
#define VAAPI_ENC_LEVEL_HEVC_4                    120
#define VAAPI_ENC_LEVEL_HEVC_41                   123
#define VAAPI_ENC_LEVEL_HEVC_5                    150
#define VAAPI_ENC_LEVEL_HEVC_51                   153
#define VAAPI_ENC_LEVEL_HEVC_52                   156
#define VAAPI_ENC_LEVEL_HEVC_6                    180
#define VAAPI_ENC_LEVEL_HEVC_61                   183
#define VAAPI_ENC_LEVEL_HEVC_62                   186

#define VAAPI_ENC_B_REFERENCE_SKIP      0
#define VAAPI_ENC_B_REFERENCE_I_P       1
#define VAAPI_ENC_B_REFERENCE_I_P_B     2

#define UI_CODEC_AVAILABLE_OFFSET       0
#define UI_MAX_B_FRAMES_OFFSET          1
#define UI_MAX_QUALITY_OFFSET           4

#define TVH_CODEC_PROFILE_VAAPI_UI(codec) \
    (vainfo_encoder_isavailable(VAINFO_##codec)) + \
    (vainfo_encoder_maxBfreames(VAINFO_##codec) << UI_MAX_B_FRAMES_OFFSET) + \
    (vainfo_encoder_maxQuality(VAINFO_##codec) << UI_MAX_QUALITY_OFFSET)

/* hts ==================================================================== */

static htsmsg_t *
platform_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Unconstrained"),  VAAPI_ENC_PLATFORM_UNCONSTRAINED },
        { N_("Intel"),          VAAPI_ENC_PLATFORM_INTEL },
        { N_("AMD"),            VAAPI_ENC_PLATFORM_AMD },
    };
    return strtab2htsmsg(tab, 1, lang);
}


static htsmsg_t *
b_reference_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"),               VAAPI_ENC_B_REFERENCE_SKIP },
        { N_("use P- or I-frames"), VAAPI_ENC_B_REFERENCE_I_P },
        { N_("use any frame"),      VAAPI_ENC_B_REFERENCE_I_P_B },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
rc_mode_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"),   VAAPI_ENC_PARAMS_RC_SKIP },
        { N_("auto"),   VAAPI_ENC_PARAMS_RC_AUTO },
        { N_("CQP"),    VAAPI_ENC_PARAMS_RC_CONSTQP },
        { N_("CBR"),    VAAPI_ENC_PARAMS_RC_CBR },
        { N_("VBR"),    VAAPI_ENC_PARAMS_RC_VBR },
        { N_("ICQ"),    VAAPI_ENC_PARAMS_RC_ICQ },
        { N_("QVBR"),   VAAPI_ENC_PARAMS_RC_QVBR },
        { N_("AVBR"),   VAAPI_ENC_PARAMS_RC_AVBR },
    };
    return strtab2htsmsg(tab, 1, lang);
}

// h264

static htsmsg_t *
h264_level_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), VAAPI_ENC_LEVEL_H264_SKIP },
        { N_("1"),    VAAPI_ENC_LEVEL_H264_1 },
        { N_("1.1"),  VAAPI_ENC_LEVEL_H264_11 },
        { N_("1.2"),  VAAPI_ENC_LEVEL_H264_12 },
        { N_("1.3"),  VAAPI_ENC_LEVEL_H264_13 },
        { N_("2"),    VAAPI_ENC_LEVEL_H264_2 },
        { N_("2.1"),  VAAPI_ENC_LEVEL_H264_21 },
        { N_("2.2"),  VAAPI_ENC_LEVEL_H264_22 },
        { N_("3"),    VAAPI_ENC_LEVEL_H264_3 },
        { N_("3.1"),  VAAPI_ENC_LEVEL_H264_31 },
        { N_("3.2"),  VAAPI_ENC_LEVEL_H264_32 },
        { N_("4"),    VAAPI_ENC_LEVEL_H264_4 },
        { N_("4.1"),  VAAPI_ENC_LEVEL_H264_41 },
        { N_("4.2"),  VAAPI_ENC_LEVEL_H264_42 },
        { N_("5"),    VAAPI_ENC_LEVEL_H264_5 },
        { N_("5.1"),  VAAPI_ENC_LEVEL_H264_51 },
        { N_("5.2"),  VAAPI_ENC_LEVEL_H264_52 },
        { N_("6"),    VAAPI_ENC_LEVEL_H264_6 },
        { N_("6.1"),  VAAPI_ENC_LEVEL_H264_61 },
        { N_("6.2"),  VAAPI_ENC_LEVEL_H264_62 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

// hevc 

static htsmsg_t *
hevc_tier_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"),   VAAPI_ENC_HEVC_TIER_SKIP },
        { N_("main"),   VAAPI_ENC_HEVC_TIER_MAIN },
        { N_("high"),   VAAPI_ENC_HEVC_TIER_HIGH },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
hevc_level_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), VAAPI_ENC_LEVEL_HEVC_SKIP },
        { N_("1"),    VAAPI_ENC_LEVEL_HEVC_1 },
        { N_("2"),    VAAPI_ENC_LEVEL_HEVC_2 },
        { N_("2.1"),  VAAPI_ENC_LEVEL_HEVC_21 },
        { N_("3"),    VAAPI_ENC_LEVEL_HEVC_3 },
        { N_("3.1"),  VAAPI_ENC_LEVEL_HEVC_31 },
        { N_("4"),    VAAPI_ENC_LEVEL_HEVC_4 },
        { N_("4.1"),  VAAPI_ENC_LEVEL_HEVC_41 },
        { N_("5"),    VAAPI_ENC_LEVEL_HEVC_5 },
        { N_("5.1"),  VAAPI_ENC_LEVEL_HEVC_51 },
        { N_("5.2"),  VAAPI_ENC_LEVEL_HEVC_52 },
        { N_("6"),    VAAPI_ENC_LEVEL_HEVC_6 },
        { N_("6.1"),  VAAPI_ENC_LEVEL_HEVC_61 },
        { N_("6.2"),  VAAPI_ENC_LEVEL_HEVC_62 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

/* vaapi ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int qp;
    int quality;
    int global_quality;
    int async_depth;
/**
 * VAAPI Encoder availablity.
 * @note
 * return: 
 * bit0 - will show if normal encoder is available (VAEntrypointEncSlice)
 */
    int ui;
/**
 * VAAPI Encoder Low power availablity.
 * @note
 * return: 
 * bit0 - will show if low power encoder is available (VAEntrypointEncSliceLP)
 */
    int uilp;
/**
 * VAAPI Frame used as reference for B-frame [b_depth]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#toc-VAAPI-encoders
 * @note
 * int: 
 * 0 - skip
 * 1 - all B-frames will refer only to P- or I-frames
 * 2 - multiple layers of B-frames will be present
 */
    int b_reference;
/**
 * VAAPI Maximum consecutive B-frame [bf]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#toc-VAAPI-encoders
 * @note
 * int: 
 * 0 - no B-Frames allowed
 * >0 - number of consecutive B-frames (valid with b_reference = 1 --> "use P- or I-frames")
 */
    int desired_b_depth;
/**
 * VAAPI Maximum bitrate [maxrate]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#toc-VAAPI-encoders
 * @note
 * int: 
 * VALUE - max bitrate in bps
 */
    double max_bit_rate;
/**
 * VAAPI Maximum bitrate [maxrate]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#toc-VAAPI-encoders
 * @note
 * double: 
 * VALUE - max bitrate in bps
 */
    double bit_rate_scale_factor;
/**
 * VAAPI Platform hardware [not ffmpeg parameter]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#toc-VAAPI-encoders
 * @note
 * int: 
 * 0 - Unconstrained (usefull for debug)
 * 1 - Intel
 * 2 - AMD
 */
    int platform;
    int loop_filter_level;
    int loop_filter_sharpness;
    double buff_factor;
    int rc_mode;
    int tier;
    int level;
    int qmin;
    int qmax;
    int super_frame;
} tvh_codec_profile_vaapi_t;

#if defined(__linux__)
#include <linux/types.h>
#include <asm/ioctl.h>
#else
#include <sys/ioccom.h>
#include <sys/types.h>
typedef size_t   __kernel_size_t;
#endif

typedef struct drm_version {
   int version_major;        /**< Major version */
   int version_minor;        /**< Minor version */
   int version_patchlevel;   /**< Patch level */
   __kernel_size_t name_len; /**< Length of name buffer */
   char *name;               /**< Name of driver */
   __kernel_size_t date_len; /**< Length of date buffer */
   char *date;               /**< User-space buffer to hold date */
   __kernel_size_t desc_len; /**< Length of desc buffer */
   char *desc;               /**< User-space buffer to hold desc */
} drm_version_t;

#define DRM_IOCTL_VERSION _IOWR('d', 0x00, struct drm_version)


static int
probe_vaapi_device(const char *device, char *name, size_t namelen)
{
    drm_version_t dv;
    char dname[128];
    int fd;

    if ((fd = open(device, O_RDWR)) < 0)
        return -1;
    memset(&dv, 0, sizeof(dv));
    memset(dname, 0, sizeof(dname));
    dv.name = dname;
    dv.name_len = sizeof(dname)-1;
    if (ioctl(fd, DRM_IOCTL_VERSION, &dv) < 0) {
        close(fd);
        return -1;
    }
    snprintf(name, namelen, "%s v%d.%d.%d (%s)",
             dv.name, dv.version_major, dv.version_minor,
             dv.version_patchlevel, device);
    close(fd);
    return 0;
}

static htsmsg_t *
tvh_codec_profile_vaapi_device_list(void *obj, const char *lang)
{
    static const char *renderD_fmt = "/dev/dri/renderD%d";
    static const char *card_fmt = "/dev/dri/card%d";
    htsmsg_t *result = htsmsg_create_list();
    char device[PATH_MAX];
    char name[128];
    int i, dev_num;

    for (i = 0; i < 32; i++) {
        dev_num = i + 128;
        snprintf(device, sizeof(device), renderD_fmt, dev_num);
        if (probe_vaapi_device(device, name, sizeof(name)) == 0)
            htsmsg_add_msg(result, NULL, htsmsg_create_key_val(device, name));
    }
    for (i = 0; i < 32; i++) {
        dev_num = i + 128;
        snprintf(device, sizeof(device), card_fmt, dev_num);
        if (probe_vaapi_device(device, name, sizeof(name)) == 0)
            htsmsg_add_msg(result, NULL, htsmsg_create_key_val(device, name));
    }
    return result;
}

#define TVH_CODEC_PROFILE_VAAPI_CODEC_UI(codec_ui, codec_name) \
    static const int tvh_codec_profile_vaapi_##codec_ui##_ui(void) \
    { \
        return TVH_CODEC_PROFILE_VAAPI_UI(codec_name); \
    }

// static const int tvh_codec_profile_vaapi_h264_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(h264, H264)

// static const int tvh_codec_profile_vaapi_hevc_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(hevc, HEVC)

// static const int tvh_codec_profile_vaapi_vp8_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(vp8, VP8)

// static const int tvh_codec_profile_vaapi_vp9_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(vp9, VP9)

// static const int tvh_codec_profile_vaapi_h264lp_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(h264lp, H264_LOW_POWER)

// static const int tvh_codec_profile_vaapi_hevclp_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(hevclp, HEVC_LOW_POWER)

// static const int tvh_codec_profile_vaapi_vp8lp_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(vp8lp, VP8_LOW_POWER)

// static const int tvh_codec_profile_vaapi_vp9lp_ui(void)
TVH_CODEC_PROFILE_VAAPI_CODEC_UI(vp9lp, VP9_LOW_POWER)

static int
tvh_codec_profile_vaapi_open(tvh_codec_profile_vaapi_t *self,
                             AVDictionary **opts)
{
    // pix_fmt
    AV_DICT_SET_PIX_FMT(opts, self->pix_fmt, AV_PIX_FMT_VAAPI);
    return 0;
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_vaapi_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_vaapi",
        .ic_caption    = N_("vaapi"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "platform",     // Don't change
                .name     = N_("Platform"),
                .desc     = N_("Select platform"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, platform),
                .list     = platform_get_list,
                .def.i    = VAAPI_ENC_PLATFORM_UNCONSTRAINED,
            },
            {
                .type     = PT_STR,
                .id       = "device",     // Don't change
                .name     = N_("Device name"),
                .desc     = N_("Device name (e.g. /dev/dri/renderD128)."),
                .group    = 3,
                .off      = offsetof(tvh_codec_profile_vaapi_t, device),
                .list     = tvh_codec_profile_vaapi_device_list,
            },
            {
                .type     = PT_BOOL,
                .id       = "low_power",     // Don't change
                .name     = N_("Low Power"),
                .desc     = N_("Set low power mode.[if disabled will not send 'low power' parameter to libav]"),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, low_power),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "async_depth",     // Don't change
                .name     = N_("Maximum processing parallelism"),
                .desc     = N_("Set maximum process in parallel (0=skip, 2=default).[driver must implement vaSyncBuffer function]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, async_depth),
                .intextra = INTEXTRA_RANGE(0, 64, 1),
                .def.i    = 2,
            },
            {
                .type     = PT_INT,
                .id       = "desired_b_depth",     // Don't change
                .name     = N_("Maximum B-frame"),
                .desc     = N_("Maximum B-frame depth (from 0 to 7, -1=skip) (default 0)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, desired_b_depth),
                .intextra = INTEXTRA_RANGE(-1, 7, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "b_reference",     // Don't change
                .name     = N_("B-frame reference"),
                .desc     = N_("Frame used as reference for B-frame (default 0)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, b_reference),
                .list     = b_reference_get_list,
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "rc_mode",     // Don't change
                .name     = N_("Rate control mode"),
                .desc     = N_("Set rate control mode"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, rc_mode),
                .list     = rc_mode_get_list,
                .def.i    = VAAPI_ENC_PARAMS_RC_AUTO,
            },
            {
                .type     = PT_INT,
                .id       = "qp",     // Don't change
                .name     = N_("Constant QP"),
                .group    = 3,
                .desc     = N_("Fixed QP of P frames (from 0 to 52, 0=skip).[if disabled will not send paramter to libav]"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, qp),
                .intextra = INTEXTRA_RANGE(0, 52, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qmin",     // Don't change
                .name     = N_("Minimum QP"),
                .group    = 5,
                .desc     = N_("Minimum QP of P frames (from 0 to 52, 0=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, qmin),
                .intextra = INTEXTRA_RANGE(0, 52, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qmax",     // Don't change
                .name     = N_("Maximum QP"),
                .group    = 5,
                .desc     = N_("Maximum QP of P frames (from 0 to 52, 0=skip)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, qmax),
                .intextra = INTEXTRA_RANGE(0, 52, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate",     // Don't change
                .name     = N_("Bitrate (kb/s)"),
                .desc     = N_("Target bitrate (0=skip).[if disabled will not send paramter to libav]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "max_bit_rate",     // Don't change
                .name     = N_("Max bitrate (kb/s)"),
                .desc     = N_("Maximum bitrate (0=skip).[if disabled will not send paramter to libav]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, max_bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "buff_factor",     // Don't change
                .name     = N_("Buffer size factor"),
                .desc     = N_("Size of transcoding buffer (buffer=bitrate*2048*factor). Good factor is 3."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, buff_factor),
                .def.d    = 3,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate_scale_factor",     // Don't change
                .name     = N_("Bitrate scale factor"),
                .desc     = N_("Bitrate & Max bitrate scaler with resolution (0=no scale; 1=proportional_change). Relative to 480."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, bit_rate_scale_factor),
                .def.d    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "hw_denoise",     // Don't change
                .name     = N_("Denoise"),
                .group    = 2,
                .desc     = N_("Denoise only available with Hardware Acceleration (from 0 to 64, 0=skip, 0 default)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, filter_hw_denoise),
                .intextra = INTEXTRA_RANGE(0, 64, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "hw_sharpness",     // Don't change
                .name     = N_("Sharpness"),
                .group    = 2,
                .desc     = N_("Sharpness only available with Hardware Acceleration (from 0 to 64, 0=skip, 44 default)"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, filter_hw_sharpness),
                .intextra = INTEXTRA_RANGE(0, 64, 1),
                .def.i    = 44,
            },
            {
                .type     = PT_INT,
                .id       = "quality",     // Don't change
                .name     = N_("Quality"),
                .desc     = N_("Set encode quality (trades off against speed, "
                               "higher is faster) [-1=skip 0-15]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, quality),
                .intextra = INTEXTRA_RANGE(-1, 15, 1),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_open,
};


/* h264_vaapi =============================================================== */

static const AVProfile vaapi_h264_profiles[] = {
    { FF_AV_PROFILE_H264_CONSTRAINED_BASELINE, "Constrained Baseline" },
    { FF_AV_PROFILE_H264_MAIN,                 "Main" },
    { FF_AV_PROFILE_H264_HIGH,                 "High" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_h264_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (int_bitrate > int_max_bitrate) {
        tvherror(LS_VAAPI, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
        int_max_bitrate = int_bitrate;
    }
    tvhinfo(LS_VAAPI, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // https://wiki.libav.org/Hardware/vaapi
    // https://blog.wmspanel.com/2017/03/vaapi-libva-support-nimble-streamer.html
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=h264_vaapi
    // h264_vaapi AVOptions:
    // -low_power         <boolean>    E..V....... Use low-power encoding mode (only available on some platforms; may not support all encoding features) (default false)
    // -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from 0 to INT_MAX) (default 0)
    // -b_depth           <int>        E..V....... Maximum B-frame reference depth (from 1 to INT_MAX) (default 1)
    // -rc_mode           <int>        E..V....... Set rate control mode (from 0 to 6) (default auto)
    //    auto            0            E..V....... Choose mode automatically based on other parameters
    //    CQP             1            E..V....... Constant-quality
    //    CBR             2            E..V....... Constant-bitrate
    //    VBR             3            E..V....... Variable-bitrate
    //    ICQ             4            E..V....... Intelligent constant-quality
    //    QVBR            5            E..V....... Quality-defined variable-bitrate
    //    AVBR            6            E..V....... Average variable-bitrate
    // -qp                <int>        E..V....... Constant QP (for P-frames; scaled by qfactor/qoffset for I/B) (from 0 to 52) (default 0)
    // -quality           <int>        E..V....... Set encode quality (trades off against speed, higher is faster) (from -1 to INT_MAX) (default -1)
    // -coder             <int>        E..V....... Entropy coder type (from 0 to 1) (default cabac)
    //    cavlc           0            E..V.......
    //    cabac           1            E..V.......
    //    vlc             0            E..V.......
    //    ac              1            E..V.......
    // -aud               <boolean>    E..V....... Include AUD (default false)
    // -sei               <flags>      E..V....... Set SEI to include (default identifier+timing+recovery_point)
    //    identifier                   E..V....... Include encoder version identifier
    //    timing                       E..V....... Include timing parameters (buffering_period and pic_timing)
    //    recovery_point               E..V....... Include recovery points where appropriate
    // -profile           <int>        E..V....... Set profile (profile_idc and constraint_set*_flag) (from -99 to 65535) (default -99)
    //    constrained_baseline 578          E..V.......
    //    main            77           E..V.......
    //    high            100          E..V.......
    // -level             <int>        E..V....... Set level (level_idc) (from -99 to 255) (default -99)
    //    1               10           E..V.......
    //    1.1             11           E..V.......
    //    1.2             12           E..V.......
    //    1.3             13           E..V.......
    //    2               20           E..V.......
    //    2.1             21           E..V.......
    //    2.2             22           E..V.......
    //    3               30           E..V.......
    //    3.1             31           E..V.......
    //    3.2             32           E..V.......
    //    4               40           E..V.......
    //    4.1             41           E..V.......
    //    4.2             42           E..V.......
    //    5               50           E..V.......
    //    5.1             51           E..V.......
    //    5.2             52           E..V.......
    //    6               60           E..V.......
    //    6.1             61           E..V.......
    //    6.2             62           E..V.......
    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    int tempSupport = 0;
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            // Uncontrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->low_power) {
                tempSupport = vainfo_encoder_maxBfreames(VAINFO_H264_LOW_POWER);
            }
            else {
                tempSupport = vainfo_encoder_maxBfreames(VAINFO_H264);
            }
            if (tempSupport) {
                if (self->b_reference) {
                    // b_depth
                    AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
                }
                if (self->desired_b_depth >= 0) {
                    // max_b_frames
                    AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                }
            }
            else {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", 0, AV_DICT_DONT_OVERWRITE);
            }

            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if (self->bit_rate) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->max_bit_rate) {
                            AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->low_power) {
                tempSupport = vainfo_encoder_maxQuality(VAINFO_H264_LOW_POWER);
            }
            else {
                tempSupport = vainfo_encoder_maxQuality(VAINFO_H264);
            }
            if (tempSupport && (self->quality >= 0)) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            AV_DICT_SET_INT(opts, "bf", 0, 0);
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    return 0;
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_vaapi_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_h264",
        .ic_caption    = N_("vaapi_h264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "level",     // Don't change
                .name     = N_("Level"),
                .desc     = N_("Set level (level_idc)"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, level),
                .list     = h264_level_get_list,
                .def.i    = VAAPI_ENC_LEVEL_H264_3,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "ui",     // Don't change
                .name     = N_("User Interface"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, ui),
                .def.dyn_i= tvh_codec_profile_vaapi_h264_ui,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "uilp",     // Don't change
                .name     = N_("User Interface (low power)"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, uilp),
                .def.dyn_i= tvh_codec_profile_vaapi_h264lp_ui,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_h264_open,
};


TVHVideoCodec tvh_codec_vaapi_h264 = {
    .name     = "h264_vaapi",
    .size     = sizeof(tvh_codec_profile_vaapi_t),
    .idclass  = &codec_profile_vaapi_h264_class,
    .profiles = vaapi_h264_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};


/* hevc_vaapi =============================================================== */

static const AVProfile vaapi_hevc_profiles[] = {
    { FF_AV_PROFILE_HEVC_MAIN,     "Main" },
    { FF_AV_PROFILE_HEVC_MAIN_10,  "Main 10" },
    { FF_AV_PROFILE_HEVC_REXT,     "Rext" },
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_hevc_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (int_bitrate > int_max_bitrate) {
        tvherror(LS_VAAPI, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
        int_max_bitrate = int_bitrate;
    }
    tvhinfo(LS_VAAPI, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // https://wiki.libav.org/Hardware/vaapi
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=hevc_vaapi
    //   h265_vaapi AVOptions:
    // -low_power         <boolean>    E..V....... Use low-power encoding mode (only available on some platforms; may not support all encoding features) (default false)
    // -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from 0 to INT_MAX) (default 0)
    // -b_depth           <int>        E..V....... Maximum B-frame reference depth (from 1 to INT_MAX) (default 1)
    // -rc_mode           <int>        E..V....... Set rate control mode (from 0 to 6) (default auto)
    //    auto            0            E..V....... Choose mode automatically based on other parameters
    //    CQP             1            E..V....... Constant-quality
    //    CBR             2            E..V....... Constant-bitrate
    //    VBR             3            E..V....... Variable-bitrate
    //    ICQ             4            E..V....... Intelligent constant-quality
    //    QVBR            5            E..V....... Quality-defined variable-bitrate
    //    AVBR            6            E..V....... Average variable-bitrate
    // -qp                <int>        E..V....... Constant QP (for P-frames; scaled by qfactor/qoffset for I/B) (from 0 to 52) (default 0)
    // -aud               <boolean>    E..V....... Include AUD (default false)
    // -profile           <int>        E..V....... Set profile (general_profile_idc) (from -99 to 255) (default -99)
    //    main            1            E..V.......
    //    main10          2            E..V.......
    //    rext            4            E..V.......
    // -tier              <int>        E..V....... Set tier (general_tier_flag) (from 0 to 1) (default main)
    //    main            0            E..V.......
    //    high            1            E..V.......
    // -level             <int>        E..V....... Set level (general_level_idc) (from -99 to 255) (default -99)
    //    1               30           E..V.......
    //    2               60           E..V.......
    //    2.1             63           E..V.......
    //    3               90           E..V.......
    //    3.1             93           E..V.......
    //    4               120          E..V.......
    //    4.1             123          E..V.......
    //    5               150          E..V.......
    //    5.1             153          E..V.......
    //    5.2             156          E..V.......
    //    6               180          E..V.......
    //    6.1             183          E..V.......
    //    6.2             186          E..V.......
    // -sei               <flags>      E..V....... Set SEI to include (default hdr)
    //    hdr                          E..V....... Include HDR metadata for mastering display colour volume and content light level information
    // -tiles             <image_size> E..V....... Tile columns x rows

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            if (self->tier >= 0) {
                AV_DICT_SET_INT(opts, "tier", self->tier, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if (self->bit_rate) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->max_bit_rate) {
                            AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->tier >= 0) {
                AV_DICT_SET_INT(opts, "tier", self->tier, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            AV_DICT_SET_INT(opts, "bf", 0, 0);
            if (self->tier >= 0) {
                AV_DICT_SET_INT(opts, "tier", self->tier, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->level != -100) {
                AV_DICT_SET_INT(opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_hevc",
        .ic_caption    = N_("vaapi_hevc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "tier",     // Don't change
                .name     = N_("Tier"),
                .desc     = N_("Set tier (general_tier_flag) (from 0 to 1) (default main)"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, tier),
                .list     = hevc_tier_get_list,
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "level",     // Don't change
                .name     = N_("Level"),
                .desc     = N_("Set level (general_level_idc)"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, level),
                .list     = hevc_level_get_list,
                .def.i    = VAAPI_ENC_LEVEL_HEVC_3,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "ui",     // Don't change
                .name     = N_("User Interface"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, ui),
                .def.dyn_i= tvh_codec_profile_vaapi_hevc_ui,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "uilp",     // Don't change
                .name     = N_("User Interface (low power)"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, uilp),
                .def.dyn_i= tvh_codec_profile_vaapi_hevclp_ui,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_hevc_open,
};


TVHVideoCodec tvh_codec_vaapi_hevc = {
    .name     = "hevc_vaapi",
    .size     = sizeof(tvh_codec_profile_vaapi_t),
    .idclass  = &codec_profile_vaapi_hevc_class,
    .profiles = vaapi_hevc_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};


/* vp8_vaapi =============================================================== */

static const AVProfile vaapi_vp8_profiles[] = {
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_vp8_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (int_bitrate > int_max_bitrate) {
        tvherror(LS_VAAPI, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
        int_max_bitrate = int_bitrate;
    }
    tvhinfo(LS_VAAPI, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // https://wiki.libav.org/Hardware/vaapi
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=vp8_vaapi
    //   vp8_vaapi AVOptions:
    // -low_power         <boolean>    E..V....... Use low-power encoding mode (only available on some platforms; may not support all encoding features) (default false)
    // -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from 0 to INT_MAX) (default 0)
    // -b_depth           <int>        E..V....... Maximum B-frame reference depth (from 1 to INT_MAX) (default 1)
    // -rc_mode           <int>        E..V....... Set rate control mode (from 0 to 6) (default auto)
    //    auto            0            E..V....... Choose mode automatically based on other parameters
    //    CQP             1            E..V....... Constant-quality
    //    CBR             2            E..V....... Constant-bitrate
    //    VBR             3            E..V....... Variable-bitrate
    //    ICQ             4            E..V....... Intelligent constant-quality
    //    QVBR            5            E..V....... Quality-defined variable-bitrate
    //    AVBR            6            E..V....... Average variable-bitrate
    // -loop_filter_level <int>        E..V....... Loop filter level (from 0 to 63) (default 16)
    // -loop_filter_sharpness <int>        E..V....... Loop filter sharpness (from 0 to 15) (default 4)

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if (self->bit_rate) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->max_bit_rate) {
                            AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            AV_DICT_SET_INT(opts, "bf", 0, 0);
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_vp8_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_vp8",
        .ic_caption    = N_("vaapi_vp8"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "global_quality",     // Don't change
                .name     = N_("Global Quality"),
                .desc     = N_("Set encode global quality [-1=skip 0-127]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, global_quality),
                .intextra = INTEXTRA_RANGE(-1, 127, 1),
                .def.i    = 40,
            },
            {
                .type     = PT_INT,
                .id       = "loop_filter_level",     // Don't change
                .name     = N_("Loop filter level"),
                .desc     = N_("Set Loop filter level (-1=skip from 0 to 63) [default 16]"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, loop_filter_level),
                .intextra = INTEXTRA_RANGE(-1, 63, 1),
                .def.i    = 16,
            },
            {
                .type     = PT_INT,
                .id       = "loop_filter_sharpness",     // Don't change
                .name     = N_("Loop filter sharpness"),
                .desc     = N_("Set Loop filter sharpness (-1=skip from 0 to 15) [default 4]"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, loop_filter_sharpness),
                .intextra = INTEXTRA_RANGE(-1, 15, 1),
                .def.i    = 4,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "ui",     // Don't change
                .name     = N_("User Interface"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, ui),
                .def.dyn_i= tvh_codec_profile_vaapi_vp8_ui,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "uilp",     // Don't change
                .name     = N_("User Interface (low power)"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, uilp),
                .def.dyn_i= tvh_codec_profile_vaapi_vp8lp_ui,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_vp8_open,
};


TVHVideoCodec tvh_codec_vaapi_vp8 = {
    .name     = "vp8_vaapi",
    .size     = sizeof(tvh_codec_profile_vaapi_t),
    .idclass  = &codec_profile_vaapi_vp8_class,
    .profiles = vaapi_vp8_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};

/* vp9_vaapi =============================================================== */

static const AVProfile vaapi_vp9_profiles[] = {
    { FF_AV_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_vp9_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (int_bitrate > int_max_bitrate) {
        tvherror(LS_VAAPI, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", int_bitrate / 1024, int_max_bitrate / 1024, int_bitrate / 1024);
        int_max_bitrate = int_bitrate;
    }
    tvhinfo(LS_VAAPI, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // https://wiki.libav.org/Hardware/vaapi
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=vp9_vaapi
    //   vp9_vaapi AVOptions:
    // -low_power         <boolean>    E..V....... Use low-power encoding mode (only available on some platforms; may not support all encoding features) (default false)
    // -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from 0 to INT_MAX) (default 0)
    // -b_depth           <int>        E..V....... Maximum B-frame reference depth (from 1 to INT_MAX) (default 1)
    // -rc_mode           <int>        E..V....... Set rate control mode (from 0 to 6) (default auto)
    //    auto            0            E..V....... Choose mode automatically based on other parameters
    //    CQP             1            E..V....... Constant-quality
    //    CBR             2            E..V....... Constant-bitrate
    //    VBR             3            E..V....... Variable-bitrate
    //    ICQ             4            E..V....... Intelligent constant-quality
    //    QVBR            5            E..V....... Quality-defined variable-bitrate
    //    AVBR            6            E..V....... Average variable-bitrate
    // -loop_filter_level <int>        E..V....... Loop filter level (from 0 to 63) (default 16)
    // -loop_filter_sharpness <int>        E..V....... Loop filter sharpness (from 0 to 15) (default 4)

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->super_frame) {
                // according to example from https://trac.ffmpeg.org/wiki/Hardware/VAAPI
                // -bsf:v vp9_raw_reorder,vp9_superframe
                AV_DICT_SET(opts, "bsf", "vp9_raw_reorder,vp9_superframe", AV_DICT_DONT_OVERWRITE);
            }
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->super_frame) {
                // according to example from https://trac.ffmpeg.org/wiki/Hardware/VAAPI
                // -bsf:v vp9_raw_reorder,vp9_superframe
                AV_DICT_SET(opts, "bsf", "vp9_raw_reorder,vp9_superframe", AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if (self->bit_rate) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->max_bit_rate) {
                            AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if (self->bit_rate) {
                AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
            }
            if (self->max_bit_rate) {
                AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qp) {
                AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
            }
            AV_DICT_SET_INT(opts, "bf", 0, 0);
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_level >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
            }
            if (self->loop_filter_sharpness >= 0) {
                AV_DICT_SET_INT(opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
            }
            if (self->async_depth) {
                AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmin) {
                AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
            }
            if (self->qmax) {
                AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
            }
            break;
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_vp9_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_vp9",
        .ic_caption    = N_("vaapi_vp9"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "global_quality",     // Don't change
                .name     = N_("Global Quality"),
                .desc     = N_("Set encode global quality [-1=skip 0-255]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, global_quality),
                .intextra = INTEXTRA_RANGE(-1, 255, 1),
                .def.i    = 40,
            },
            {
                .type     = PT_INT,
                .id       = "loop_filter_level",     // Don't change
                .name     = N_("Loop filter level"),
                .desc     = N_("Set Loop filter level (-1=skip from 0 to 63) [default 16]"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, loop_filter_level),
                .intextra = INTEXTRA_RANGE(-1, 63, 1),
                .def.i    = 16,
            },
            {
                .type     = PT_INT,
                .id       = "loop_filter_sharpness",     // Don't change
                .name     = N_("Loop filter sharpness"),
                .desc     = N_("Set Loop filter sharpness (-1=skip from 0 to 15) [default 4]"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, loop_filter_sharpness),
                .intextra = INTEXTRA_RANGE(-1, 15, 1),
                .def.i    = 4,
            },
            {
                .type     = PT_BOOL,
                .id       = "super_frame",              // Don't change
                .name     = N_("Super Frame"),
                .desc     = N_("Enable Super Frame for vp9 encoder.[default disabled]"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, super_frame),
                .def.i    = 0,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "ui",     // Don't change
                .name     = N_("User Interface"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, ui),
                .def.dyn_i= tvh_codec_profile_vaapi_vp9_ui,
            },
            {
                .type     = PT_DYN_INT,
                .id       = "uilp",     // Don't change
                .name     = N_("User Interface (low power)"),
                .desc     = N_("User Interface (bits will show what features are available)."),
                .group    = 3,
                .opts     = PO_PHIDDEN,
                .off      = offsetof(tvh_codec_profile_vaapi_t, uilp),
                .def.dyn_i= tvh_codec_profile_vaapi_vp9lp_ui,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_vp9_open,
};


TVHVideoCodec tvh_codec_vaapi_vp9 = {
    .name     = "vp9_vaapi",
    .size     = sizeof(tvh_codec_profile_vaapi_t),
    .idclass  = &codec_profile_vaapi_vp9_class,
    .profiles = vaapi_vp9_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};
