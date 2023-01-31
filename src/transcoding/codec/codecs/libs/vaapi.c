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

/* hts ==================================================================== */

static htsmsg_t *
platform_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("Unconstrained"),  0 },
        { N_("Intel"),          1 },
        { N_("AMD"),            2 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
rc_mode_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"),  -1 },
        { N_("auto"),   0 },
        { N_("CQP"),    1 },
        { N_("CBR"),    2 },
        { N_("VBR"),    3 },
        { N_("ICQ"),    4 },
        { N_("QVBR"),   5 },
        { N_("AVBR"),   6 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

// h264

static htsmsg_t *
h264_level_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), -100 },
        { N_("1"),     10 },
        { N_("1.1"),   11 },
        { N_("1.2"),   12 },
        { N_("1.3"),   13 },
        { N_("2"),     20 },
        { N_("2.1"),   21 },
        { N_("2.2"),   22 },
        { N_("3"),     30 },
        { N_("3.1"),   31 },
        { N_("3.2"),   32 },
        { N_("4"),     40 },
        { N_("4.1"),   41 },
        { N_("4.2"),   42 },
        { N_("5"),     50 },
        { N_("5.1"),   51 },
        { N_("5.2"),   52 },
        { N_("6"),     60 },
        { N_("6.1"),   61 },
        { N_("6.2"),   62 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

// hevc 

static htsmsg_t *
hevc_tier_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"),   -1 },
        { N_("main"),   0 },
        { N_("high"),   1 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
hevc_level_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), -100 },
        { N_("1"),     30 },
        { N_("2"),     60 },
        { N_("2.1"),   63 },
        { N_("3"),     90 },
        { N_("3.1"),   93 },
        { N_("4"),     120 },
        { N_("4.1"),   123 },
        { N_("5"),     150 },
        { N_("5.1"),   153 },
        { N_("5.2"),   156 },
        { N_("6"),     180 },
        { N_("6.1"),   183 },
        { N_("6.2"),   186 },
    };
    return strtab2htsmsg(tab, 1, lang);
}

/* vaapi ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int qp;
    int quality;
    int async_depth;
    int desired_b_depth;
    double max_bit_rate;
    double bit_rate_scale_factor;
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
                .def.i    = 0,
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
                .desc     = N_("Set low power mode.[if disabled will not send parameter to libav, if enabled codec will disable B frames]"),
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
                .desc     = N_("Maximum B-frame reference depth (from 1 to 3) (default 1)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, desired_b_depth),
                .intextra = INTEXTRA_RANGE(0, 3, 1),
                .def.i    = 1,
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
                .def.i    = 0,
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
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_open,
};


/* h264_vaapi =============================================================== */

static const AVProfile vaapi_h264_profiles[] = {
    { FF_PROFILE_H264_CONSTRAINED_BASELINE, "Constrained Baseline" },
    { FF_PROFILE_H264_MAIN,                 "Main" },
    { FF_PROFILE_H264_HIGH,                 "High" },
    { FF_PROFILE_UNKNOWN },
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
    if (self->rc_mode >= 0) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case 0:
            // Uncontrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
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
        case 1:
            // Intel
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case -1:
                    // same like 0 but is not sending 'rc_mode'
                case 0:
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
                case 1:
                case 4:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 2:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 3:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 5:
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
                case 6:
                    // for variable bitrate: AVBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
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
        case 2:
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
                .id       = "quality",     // Don't change
                .name     = N_("Quality"),
                .desc     = N_("Set encode quality (trades off against speed, "
                               "higher is faster) [-1=skip 0-7]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, quality),
                .intextra = INTEXTRA_RANGE(-1, 7, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "level",     // Don't change
                .name     = N_("Level"),
                .desc     = N_("Set level (level_idc) (from -99 to 255)"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, level),
                .list     = h264_level_get_list,
                .def.i    = -100,
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
    { FF_PROFILE_HEVC_MAIN, "Main" },
    { FF_PROFILE_HEVC_MAIN_10, "Main 10" },
    { FF_PROFILE_HEVC_REXT, "Rext" },
    { FF_PROFILE_UNKNOWN },
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

    if (self->rc_mode >= 0) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case 0:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
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
        case 1:
            // Intel
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case -1:
                    // same like 0 but is not sending 'rc_mode'
                case 0:
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
                case 1:
                case 4:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 2:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 3:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 5:
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
                case 6:
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
        case 2:
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
                .desc     = N_("Set level (general_level_idc) (from -99 to 255)"),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, level),
                .list     = hevc_level_get_list,
                .def.i    = -100,
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
    { FF_PROFILE_UNKNOWN },
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

    if (self->rc_mode >= 0) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case 0:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
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
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
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
        case 1:
            // Intel
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case -1:
                    // same like 0 but is not sending 'rc_mode'
                case 0:
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
                case 1:
                case 4:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 2:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 3:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 5:
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
                case 6:
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
        case 2:
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
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
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
                .id       = "quality",     // Don't change
                .name     = N_("Global Quality"),
                .desc     = N_("Set encode quality [-1=skip 0-127]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, quality),
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
    { FF_PROFILE_UNKNOWN },
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

    if (self->rc_mode >= 0) {
        AV_DICT_SET_INT(opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case 0:
            // Unconstrained --> will allow any combination of parameters (valid or invalid)
            // this mode is usefull fur future platform and for debugging.
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
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
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
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
        case 1:
            // Intel
            if ((self->low_power == 0) && (self->desired_b_depth)) {
                AV_DICT_SET_INT(opts, "b_depth", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                // max_b_frames
                AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->super_frame) {
                // according to example from https://trac.ffmpeg.org/wiki/Hardware/VAAPI
                // -bsf:v vp9_raw_reorder,vp9_superframe
                AV_DICT_SET(opts, "bsf", "vp9_raw_reorder,vp9_superframe", AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case -1:
                    // same like 0 but is not sending 'rc_mode'
                case 0:
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
                case 1:
                case 4:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 2:
                    // for constant bitrate: CBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 3:
                    // for variable bitrate: VBR we use bitrate
                    if (self->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case 5:
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
                case 6:
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
        case 2:
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
                AV_DICT_SET_INT(opts, "global_quality", self->quality, AV_DICT_DONT_OVERWRITE);
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
                .id       = "quality",     // Don't change
                .name     = N_("Global Quality"),
                .desc     = N_("Set encode quality [-1=skip 0-255]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, quality),
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
