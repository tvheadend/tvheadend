/*
 *  tvheadend - Codec Profiles
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


#include "transcoding/codec/internals.h"
#include <fcntl.h>
#include <sys/ioctl.h>

#include "idnode.h"
#include "htsmsg.h"

/* defines */

// for preset
#define QSV_ENC_PRESET_SKIP         0
#define QSV_ENC_PRESET_VERYSLOW     1
#define QSV_ENC_PRESET_SLOWER       2
#define QSV_ENC_PRESET_SLOW         3
#define QSV_ENC_PRESET_MEDIUM       4
#define QSV_ENC_PRESET_FAST         5
#define QSV_ENC_PRESET_FASTER       6
#define QSV_ENC_PRESET_VERYFAST     7
// for b_strategy
#define QSV_ENC_B_STRATEGY_SKIP     -1
#define QSV_ENC_B_STRATEGY_OFF      0
#define QSV_ENC_B_STRATEGY_ON       1


/* hts ==================================================================== */


static htsmsg_t *
preset_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), QSV_ENC_PRESET_SKIP },
        { N_("veryslow"),  QSV_ENC_PRESET_VERYSLOW },
        { N_("slower"),  QSV_ENC_PRESET_SLOWER },
        { N_("slow"),  QSV_ENC_PRESET_SLOW },
        { N_("medium"),  QSV_ENC_PRESET_MEDIUM },
        { N_("fast"),  QSV_ENC_PRESET_FAST },
        { N_("faster"),  QSV_ENC_PRESET_FASTER },
        { N_("veryfast"),  QSV_ENC_PRESET_VERYFAST },
    };
    return strtab2htsmsg(tab, 1, lang);
}


static htsmsg_t *
b_strategy_get_list( void *o, const char *lang )
{
    static const struct strtab tab[] = {
        { N_("skip"), QSV_ENC_B_STRATEGY_SKIP },
        { N_("disabled"),  QSV_ENC_B_STRATEGY_OFF },
        { N_("enabled"),  QSV_ENC_B_STRATEGY_ON },
    };
    return strtab2htsmsg(tab, 1, lang);
}

/* qsv ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int qp;
    int quality;
    int async_depth;
/**
 * QSV Preset - This option itemizes a range of choices from veryfast (best speed) to veryslow (best quality). [preset]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#QSV-Encoders
 * @note
 * int: 
 * 0 - skip
 * 1 - ‘veryslow’
 * 2 - ‘slower’
 * 3 - ‘slow’
 * 4 - ‘medium’
 * 5 - ‘fast’
 * 6 - ‘faster’
 * 7 - ‘veryfast’
 */
    int preset;
    int desired_b_depth;
/**
 * QSV b_strategy - This option controls usage of B frames as reference. [b_strategy]
 * -b_strategy        <int>        E..V....... Strategy to choose between I/P/B-frames (from -1 to 1) (default -1)
 * @note
 * int: 
 * -1 - not set
 * 0 - don't use B pyramid
 * 1 - use B pyramid
 */
    int b_strategy;
    double max_bit_rate;
    double bit_rate_scale_factor;
/**
 * QSV adaptive_b - This flag controls changing of frame type from B to P. [adaptive_b]
 * -adaptive_b        <int>        E..V....... Adaptive B-frame placement (from -1 to 1) (default -1)
 * @note
 * int: 
 * 0 - don't use adaptive B
 * 1 - use adaptive B
 */
    int adaptive_b;
    int loop_filter_level;
    int loop_filter_sharpness;
    double buff_factor;
    int global_quality;
    int qmin;
    int qmax;
    int super_frame;
} tvh_codec_profile_qsv_t;

#if defined(__linux__)
#include <linux/types.h>
#include <asm/ioctl.h>
#else
#include <sys/ioccom.h>
#include <sys/types.h>
typedef size_t   __kernel_size_t;
#endif

/*
typedef struct drm_version {
   int version_major;        //< Major version /
   int version_minor;        //< Minor version /
   int version_patchlevel;   //< Patch level /
   __kernel_size_t name_len; //< Length of name buffer /
   char *name;               //< Name of driver /
   __kernel_size_t date_len; //< Length of date buffer /
   char *date;               //< User-space buffer to hold date /
   __kernel_size_t desc_len; //< Length of desc buffer /
   char *desc;               //< User-space buffer to hold desc /
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
*/

static int
tvh_codec_profile_qsv_open(tvh_codec_profile_qsv_t *self,
                             AVDictionary **opts)
{
    // pix_fmt
    AV_DICT_SET_PIX_FMT(opts, self->pix_fmt, AV_PIX_FMT_QSV);
    return 0;
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_qsv_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_qsv",
        .ic_caption    = N_("qsv"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_BOOL,
                .id       = "low_power",     // Don't change
                .name     = N_("Low Power"),
                .desc     = N_("Set low power mode.[if disabled will not send parameter to libav, if enabled codec will disable B frames]"),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, low_power),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "async_depth",     // Don't change
                .name     = N_("Maximum processing parallelism"),
                .desc     = N_("Set maximum process in parallel (0=skip, 2=default).[driver must implement vaSyncBuffer function]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, async_depth),
                .intextra = INTEXTRA_RANGE(0, 64, 1),
                .def.i    = 2,
            },
            {
                .type     = PT_INT,
                .id       = "preset",     // Don't change
                .name     = N_("Preset"),
                .desc     = N_("This option itemizes a range of choices from veryfast (best speed) to veryslow (best quality) [0=skip 0-7]."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, preset),
                .list     = preset_get_list,
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "desired_b_depth",     // Don't change
                .name     = N_("Maximum B-frame"),
                .desc     = N_("Maximum B-frame reference depth (from 1 to 3) (default 1)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, desired_b_depth),
                .intextra = INTEXTRA_RANGE(0, 7, 1),
                .def.i    = 1,
            },
            {
                .type     = PT_INT,
                .id       = "b_strategy",     // Don't change
                .name     = N_("B-Pyramid"),
                .desc     = N_("B-Pyramid (from -1 to 1) (default -1)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, b_strategy),
                .list     = b_strategy_get_list,
                .def.i    = -1,
            },
            {
                .type     = PT_INT,
                .id       = "global_quality",     // Don't change
                .name     = N_("global quality"),
                .desc     = N_("Set global quality"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, global_quality),
                .intextra = INTEXTRA_RANGE(0, 51, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qp",     // Don't change
                .name     = N_("Constant QP"),
                .group    = 3,
                .desc     = N_("Fixed QP of P frames (from 0 to 52, 0=skip).[if '0' will not send paramter to libav]"),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, qp),
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
                .off      = offsetof(tvh_codec_profile_qsv_t, qmin),
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
                .off      = offsetof(tvh_codec_profile_qsv_t, qmax),
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
                .off      = offsetof(tvh_codec_profile_qsv_t, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "max_bit_rate",     // Don't change
                .name     = N_("Max bitrate (kb/s)"),
                .desc     = N_("Maximum bitrate (0=skip).[if disabled will not send paramter to libav]"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, max_bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_DBL,
                .id       = "buff_factor",     // Don't change
                .name     = N_("Buffer size factor"),
                .desc     = N_("Size of transcoding buffer (buffer=bitrate*2048*factor). Good factor is 3."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, buff_factor),
                .def.d    = 3,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate_scale_factor",     // Don't change
                .name     = N_("Bitrate scale factor"),
                .desc     = N_("Bitrate & Max bitrate scaler with resolution (0=no scale; 1=proportional_change). Relative to 480."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, bit_rate_scale_factor),
                .def.d    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_qsv_open,
};


/* h264_qsv =============================================================== */
/*
 * QSV Profile -  [profile]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#QSV-Encoders
 * @note
 * int: 
 * 0   - unknown
 * 66  - baseline
 * 77  - main
 * 100 - high
*/
static const AVProfile qsv_h264_profiles[] = {
    { FF_PROFILE_H264_CONSTRAINED_BASELINE, "Constrained Baseline" },
    { FF_PROFILE_H264_MAIN,                 "Main" },
    { FF_PROFILE_H264_HIGH,                 "High" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_qsv_h264_open(tvh_codec_profile_qsv_t *self, AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    tvhinfo(LS_QSV, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=h264_qsv
/*
ffmpeg -hide_banner -h encoder=h264_qsv
Encoder h264_qsv [H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10 (Intel Quick Sync Video acceleration)]:
    General capabilities: delay hybrid
    Threading capabilities: none
    Supported hardware devices: qsv qsv qsv
    Supported pixel formats: nv12 p010le qsv
h264_qsv encoder AVOptions:
  -async_depth       <int>        E..V....... Maximum processing parallelism (from 1 to INT_MAX) (default 4)
  -avbr_accuracy     <int>        E..V....... Accuracy of the AVBR ratecontrol (unit of tenth of percent) (from 1 to 65535) (default 1)
  -avbr_convergence  <int>        E..V....... Convergence of the AVBR ratecontrol (unit of 100 frames) (from 1 to 65535) (default 1)
  -preset            <int>        E..V....... (from 1 to 7) (default medium)
     veryfast        7            E..V.......
     faster          6            E..V.......
     fast            5            E..V.......
     medium          4            E..V.......
     slow            3            E..V.......
     slower          2            E..V.......
     veryslow        1            E..V.......
  -forced_idr        <boolean>    E..V....... Forcing I frames as IDR frames (default false)
  -low_power         <boolean>    E..V....... enable low power mode(experimental: many limitations by mfx version, BRC modes, etc.) (default auto)
  -rdo               <int>        E..V....... Enable rate distortion optimization (from -1 to 1) (default -1)
  -max_frame_size    <int>        E..V....... Maximum encoded frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_frame_size_i  <int>        E..V....... Maximum encoded I frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_frame_size_p  <int>        E..V....... Maximum encoded P frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_slice_size    <int>        E..V....... Maximum encoded slice size in bytes (from -1 to INT_MAX) (default -1)
  -bitrate_limit     <int>        E..V....... Toggle bitrate limitations (from -1 to 1) (default -1)
  -mbbrc             <int>        E..V....... MB level bitrate control (from -1 to 1) (default -1)
  -extbrc            <int>        E..V....... Extended bitrate control (from -1 to 1) (default -1)
  -adaptive_i        <int>        E..V....... Adaptive I-frame placement (from -1 to 1) (default -1)
  -adaptive_b        <int>        E..V....... Adaptive B-frame placement (from -1 to 1) (default -1)
  -p_strategy        <int>        E..V....... Enable P-pyramid: 0-default 1-simple 2-pyramid(bf need to be set to 0). (from 0 to 2) (default 0)
  -b_strategy        <int>        E..V....... Strategy to choose between I/P/B-frames (from -1 to 1) (default -1)
  -dblk_idc          <int>        E..V....... This option disable deblocking. It has value in range 0~2. (from 0 to 2) (default 0)
  -low_delay_brc     <boolean>    E..V....... Allow to strictly obey avg frame size (default auto)
  -max_qp_i          <int>        E..V....... Maximum video quantizer scale for I frame (from -1 to 51) (default -1)
  -min_qp_i          <int>        E..V....... Minimum video quantizer scale for I frame (from -1 to 51) (default -1)
  -max_qp_p          <int>        E..V....... Maximum video quantizer scale for P frame (from -1 to 51) (default -1)
  -min_qp_p          <int>        E..V....... Minimum video quantizer scale for P frame (from -1 to 51) (default -1)
  -max_qp_b          <int>        E..V....... Maximum video quantizer scale for B frame (from -1 to 51) (default -1)
  -min_qp_b          <int>        E..V....... Minimum video quantizer scale for B frame (from -1 to 51) (default -1)
  -cavlc             <boolean>    E..V....... Enable CAVLC (default false)
  -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from 0 to INT_MAX) (default 0)
  -pic_timing_sei    <boolean>    E..V....... Insert picture timing SEI with pic_struct_syntax element (default true)
  -single_sei_nal_unit <int>        E..V....... Put all the SEI messages into one NALU (from -1 to 1) (default -1)
  -max_dec_frame_buffering <int>        E..V....... Maximum number of frames buffered in the DPB (from 0 to 65535) (default 0)
  -look_ahead        <boolean>    E..V....... Use VBR algorithm with look ahead (default false)
  -look_ahead_depth  <int>        E..V....... Depth of look ahead in number frames (from 0 to 100) (default 0)
  -look_ahead_downsampling <int>        E..V....... Downscaling factor for the frames saved for the lookahead analysis (from 0 to 3) (default unknown)
     unknown         0            E..V.......
     auto            0            E..V.......
     off             1            E..V.......
     2x              2            E..V.......
     4x              3            E..V.......
  -int_ref_type      <int>        E..V....... Intra refresh type. B frames should be set to 0. (from -1 to 65535) (default -1)
     none            0            E..V.......
     vertical        1            E..V.......
     horizontal      2            E..V.......
  -int_ref_cycle_size <int>        E..V....... Number of frames in the intra refresh cycle (from -1 to 65535) (default -1)
  -int_ref_qp_delta  <int>        E..V....... QP difference for the refresh MBs (from -32768 to 32767) (default -32768)
  -recovery_point_sei <int>        E..V....... Insert recovery point SEI messages (from -1 to 1) (default -1)
  -int_ref_cycle_dist <int>        E..V....... Distance between the beginnings of the intra-refresh cycles in frames (from -1 to 32767) (default -1)
  -profile           <int>        E..V....... (from 0 to INT_MAX) (default unknown)
     unknown         0            E..V.......
     baseline        66           E..V.......
     main            77           E..V.......
     high            100          E..V.......
  -a53cc             <boolean>    E..V....... Use A53 Closed Captions (if available) (default true)
  -aud               <boolean>    E..V....... Insert the Access Unit Delimiter NAL (default false)
  -mfmode            <int>        E..V....... Multi-Frame Mode (from 0 to 2) (default auto)
     off             1            E..V.......
     auto            2            E..V.......
  -repeat_pps        <boolean>    E..V....... repeat pps for every frame (default false)
*/
    if (self->global_quality) {
        AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
    }
    if (self->b_strategy >= 0) {
        // b_depth
        AV_DICT_SET_INT(opts, "b_strategy", self->b_strategy, AV_DICT_DONT_OVERWRITE);
    }
    if (self->desired_b_depth >= 0) {
        // max_b_frames
        AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
    }
    if (self->low_power) {
        AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
    }
    if (self->bit_rate) {
        AV_DICT_SET_INT(opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
        AV_DICT_SET_INT(opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
    }
    if (self->max_bit_rate) {
        AV_DICT_SET_INT(opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    if (self->adaptive_b) {
        AV_DICT_SET_INT(opts, "adaptive_b", self->adaptive_b, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qp) {
        AV_DICT_SET_INT(opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
    }
    if (self->quality >= 0) {
        AV_DICT_SET_INT(opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
    }
    if (self->async_depth) {
        AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
    }
    if (self->preset) {
        AV_DICT_SET_INT(opts, "preset", self->preset, AV_DICT_DONT_OVERWRITE);
    }
    if (self->look_ahead_depth) {
        if (self->look_ahead_depth < (self->desired_b_depth + 1))
            // we have to force look_ahead_depth to bf + 1
            // https://forum.level1techs.com/t/ffmpeg-av1-encoding-using-intel-arc-gpu-tips/205120/2
            AV_DICT_SET_INT(opts, "look_ahead_depth", self->desired_b_depth + 1, AV_DICT_DONT_OVERWRITE);
        else
            AV_DICT_SET_INT(opts, "look_ahead_depth", self->look_ahead_depth, AV_DICT_DONT_OVERWRITE);
        AV_DICT_SET_INT(opts, "look_ahead", 1, AV_DICT_DONT_OVERWRITE);
        AV_DICT_SET_INT(opts, "look_ahead_downsampling", 1, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmin) {
        AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmax) {
        AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    return 0;
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_qsv_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_qsv_class,
        .ic_class      = "codec_profile_qsv_h264",
        .ic_caption    = N_("qsv_h264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "look_ahead_depth",     // Don't change
                .name     = N_("Depth of look ahead"),
                .desc     = N_("Depth of look ahead in number frames [0=skip 1-100]."),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, look_ahead_depth),
                .intextra = INTEXTRA_RANGE(0, 100, 1),
                .def.i    = 0,
            },
            {
                .type     = PT_BOOL,
                .id       = "adaptive_b",     // Don't change
                .name     = N_("AdaptiveB"),
                .desc     = N_("This flag controls changing of frame type from B to P."),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, adaptive_b),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_qsv_h264_open,
};


TVHVideoCodec tvh_codec_qsv_h264 = {
    .name     = "h264_qsv",
    .size     = sizeof(tvh_codec_profile_qsv_t),
    .idclass  = &codec_profile_qsv_h264_class,
    .profiles = qsv_h264_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};


/* hevc_qsv =============================================================== */
/*
 * QSV Profile -  [profile]
 * https://www.ffmpeg.org/ffmpeg-codecs.html#QSV-Encoders
 * @note
 * int: 
 * -99 - unknown
 *  1  - main
 *  2  - main10
 *  3  - mainsp
 *  4  - rext
 *  5  - scc
*/
static const AVProfile qsv_hevc_profiles[] = {
    { FF_PROFILE_HEVC_MAIN,                 "Main" },
    { FF_PROFILE_HEVC_MAIN_10,              "Main 10" },
    { FF_PROFILE_HEVC_MAIN_STILL_PICTURE,   "Main Still Picture" },
    { FF_PROFILE_HEVC_REXT,                 "REXT" },
    { FF_PROFILE_HEVC_SCC,                  "SCC" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_qsv_hevc_open(tvh_codec_profile_qsv_t *self, AVDictionary **opts)
{
    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    int int_bitrate = (int)((double)(self->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    int int_buffer_size = (int)((double)(self->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480));
    int int_max_bitrate = (int)((double)(self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(self->size.den) - 480.0) / 480.0)));
    tvhinfo(LS_QSV, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", int_bitrate / 1024, int_buffer_size / 1024, int_max_bitrate / 1024);
    // to find available parameters use:
    // ffmpeg -hide_banner -h encoder=hevc_qsv
/*
ffmpeg -hide_banner -h encoder=hevc_qsv
Encoder hevc_qsv [HEVC (Intel Quick Sync Video acceleration)]:
    General capabilities: delay hybrid
    Threading capabilities: none
    Supported hardware devices: qsv qsv qsv
    Supported pixel formats: nv12 p010le yuyv422 y210le qsv bgra x2rgb10le
hevc_qsv encoder AVOptions:
  -async_depth       <int>        E..V....... Maximum processing parallelism (from 1 to INT_MAX) (default 4)
  -avbr_accuracy     <int>        E..V....... Accuracy of the AVBR ratecontrol (unit of tenth of percent) (from 1 to 65535) (default 1)
  -avbr_convergence  <int>        E..V....... Convergence of the AVBR ratecontrol (unit of 100 frames) (from 1 to 65535) (default 1)
  -preset            <int>        E..V....... (from 1 to 7) (default medium)
     veryfast        7            E..V.......
     faster          6            E..V.......
     fast            5            E..V.......
     medium          4            E..V.......
     slow            3            E..V.......
     slower          2            E..V.......
     veryslow        1            E..V.......
  -forced_idr        <boolean>    E..V....... Forcing I frames as IDR frames (default false)
  -low_power         <boolean>    E..V....... enable low power mode(experimental: many limitations by mfx version, BRC modes, etc.) (default auto)
  -rdo               <int>        E..V....... Enable rate distortion optimization (from -1 to 1) (default -1)
  -max_frame_size    <int>        E..V....... Maximum encoded frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_frame_size_i  <int>        E..V....... Maximum encoded I frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_frame_size_p  <int>        E..V....... Maximum encoded P frame size in bytes (from -1 to INT_MAX) (default -1)
  -max_slice_size    <int>        E..V....... Maximum encoded slice size in bytes (from -1 to INT_MAX) (default -1)
  -mbbrc             <int>        E..V....... MB level bitrate control (from -1 to 1) (default -1)
  -extbrc            <int>        E..V....... Extended bitrate control (from -1 to 1) (default -1)
  -p_strategy        <int>        E..V....... Enable P-pyramid: 0-default 1-simple 2-pyramid(bf need to be set to 0). (from 0 to 2) (default 0)
  -b_strategy        <int>        E..V....... Strategy to choose between I/P/B-frames (from -1 to 1) (default -1)
  -dblk_idc          <int>        E..V....... This option disable deblocking. It has value in range 0~2. (from 0 to 2) (default 0)
  -low_delay_brc     <boolean>    E..V....... Allow to strictly obey avg frame size (default auto)
  -max_qp_i          <int>        E..V....... Maximum video quantizer scale for I frame (from -1 to 51) (default -1)
  -min_qp_i          <int>        E..V....... Minimum video quantizer scale for I frame (from -1 to 51) (default -1)
  -max_qp_p          <int>        E..V....... Maximum video quantizer scale for P frame (from -1 to 51) (default -1)
  -min_qp_p          <int>        E..V....... Minimum video quantizer scale for P frame (from -1 to 51) (default -1)
  -max_qp_b          <int>        E..V....... Maximum video quantizer scale for B frame (from -1 to 51) (default -1)
  -min_qp_b          <int>        E..V....... Minimum video quantizer scale for B frame (from -1 to 51) (default -1)
  -idr_interval      <int>        E..V....... Distance (in I-frames) between IDR frames (from -1 to INT_MAX) (default 0)
     begin_only      -1           E..V....... Output an IDR-frame only at the beginning of the stream
  -load_plugin       <int>        E..V....... A user plugin to load in an internal session (from 0 to 2) (default hevc_hw)
     none            0            E..V.......
     hevc_sw         1            E..V.......
     hevc_hw         2            E..V.......
  -load_plugins      <string>     E..V....... A :-separate list of hexadecimal plugin UIDs to load in an internal session (default "")
  -look_ahead_depth  <int>        E..V....... Depth of look ahead in number frames, available when extbrc option is enabled (from 0 to 100) (default 0)
  -profile           <int>        E..V....... (from 0 to INT_MAX) (default unknown)
     unknown         0            E..V.......
     main            1            E..V.......
     main10          2            E..V.......
     mainsp          3            E..V.......
     rext            4            E..V.......
     scc             9            E..V.......
  -gpb               <boolean>    E..V....... 1: GPB (generalized P/B frame); 0: regular P frame (default true)
  -tile_cols         <int>        E..V....... Number of columns for tiled encoding (from 0 to 65535) (default 0)
  -tile_rows         <int>        E..V....... Number of rows for tiled encoding (from 0 to 65535) (default 0)
  -recovery_point_sei <int>        E..V....... Insert recovery point SEI messages (from -1 to 1) (default -1)
  -aud               <boolean>    E..V....... Insert the Access Unit Delimiter NAL (default false)
  -pic_timing_sei    <boolean>    E..V....... Insert picture timing SEI with pic_struct_syntax element (default true)
  -transform_skip    <int>        E..V....... Turn this option ON to enable transformskip (from -1 to 1) (default -1)
  -int_ref_type      <int>        E..V....... Intra refresh type. B frames should be set to 0 (from -1 to 65535) (default -1)
     none            0            E..V.......
     vertical        1            E..V.......
     horizontal      2            E..V.......
  -int_ref_cycle_size <int>        E..V....... Number of frames in the intra refresh cycle (from -1 to 65535) (default -1)
  -int_ref_qp_delta  <int>        E..V....... QP difference for the refresh MBs (from -32768 to 32767) (default -32768)
  -int_ref_cycle_dist <int>        E..V....... Distance between the beginnings of the intra-refresh cycles in frames (from -1 to 32767) (default -1)
*/

    if (self->global_quality) {
        AV_DICT_SET_INT(opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
    }
    if (self->b_strategy >= 0) {
        // b_depth
        AV_DICT_SET_INT(opts, "b_strategy", self->b_strategy, AV_DICT_DONT_OVERWRITE);
    }
    if (self->desired_b_depth >= 0) {
        // max_b_frames
        AV_DICT_SET_INT(opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
    }
    if (self->low_power) {
        AV_DICT_SET_INT(opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
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
    if (self->async_depth) {
        AV_DICT_SET_INT(opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
    }
    if (self->preset) {
        AV_DICT_SET_INT(opts, "preset", self->preset, AV_DICT_DONT_OVERWRITE);
    }
    if (self->look_ahead_depth) {
        if (self->look_ahead_depth < (self->desired_b_depth + 1))
            // we have to force look_ahead_depth to bf + 1
            // https://forum.level1techs.com/t/ffmpeg-av1-encoding-using-intel-arc-gpu-tips/205120/2
            AV_DICT_SET_INT(opts, "look_ahead_depth", self->desired_b_depth + 1, AV_DICT_DONT_OVERWRITE);
        else
            AV_DICT_SET_INT(opts, "look_ahead_depth", self->look_ahead_depth, AV_DICT_DONT_OVERWRITE);
        AV_DICT_SET_INT(opts, "extbrc", 1, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmin) {
        AV_DICT_SET_INT(opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmax) {
        AV_DICT_SET_INT(opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
    }
    // force keyframe every 3 sec.
    AV_DICT_SET(opts, "force_key_frames", "expr:gte(t,n_forced*3)", AV_DICT_DONT_OVERWRITE);
    //AV_DICT_SET(opts, "filter_complex", "[0:v]setpts=N/FRAME_RATE/TB[v]", AV_DICT_DONT_OVERWRITE);
    return 0;
}

// NOTE:
// the names below are used in codec.js (/src/webui/static/app/codec.js)
static const codec_profile_class_t codec_profile_qsv_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_qsv_class,
        .ic_class      = "codec_profile_qsv_hevc",
        .ic_caption    = N_("qsv_hevc"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "look_ahead_depth",     // Don't change
                .name     = N_("Depth of look ahead"),
                .desc     = N_("Depth of look ahead in number frames [0=skip 1-100]."),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, look_ahead_depth),
                .intextra = INTEXTRA_RANGE(0, 100, 1),
                .def.i    = 0,
            },
            /*
            {
                .type     = PT_BOOL,
                .id       = "adaptive_b",     // Don't change
                .name     = N_("AdaptiveB"),
                .desc     = N_("This flag controls changing of frame type from B to P."),
                .group    = 3,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_qsv_t, adaptive_b),
                .def.i    = 0,
            },
            */
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_qsv_hevc_open,
};


TVHVideoCodec tvh_codec_qsv_hevc = {
    .name     = "hevc_qsv",
    .size     = sizeof(tvh_codec_profile_qsv_t),
    .idclass  = &codec_profile_qsv_hevc_class,
    .profiles = qsv_hevc_profiles,
    .profile_init = tvh_codec_profile_video_init,
    .profile_destroy = tvh_codec_profile_video_destroy,
};








/*
ffmpeg -hide_banner -h encoder=vp9_qsv
Encoder vp9_qsv [VP9 video (Intel Quick Sync Video acceleration)]:
    General capabilities: delay hybrid
    Threading capabilities: none
    Supported hardware devices: qsv qsv qsv
    Supported pixel formats: nv12 p010le qsv
vp9_qsv encoder AVOptions:
  -async_depth       <int>        E..V....... Maximum processing parallelism (from 1 to INT_MAX) (default 4)
  -avbr_accuracy     <int>        E..V....... Accuracy of the AVBR ratecontrol (unit of tenth of percent) (from 1 to 65535) (default 1)
  -avbr_convergence  <int>        E..V....... Convergence of the AVBR ratecontrol (unit of 100 frames) (from 1 to 65535) (default 1)
  -preset            <int>        E..V....... (from 1 to 7) (default medium)
     veryfast        7            E..V.......
     faster          6            E..V.......
     fast            5            E..V.......
     medium          4            E..V.......
     slow            3            E..V.......
     slower          2            E..V.......
     veryslow        1            E..V.......
  -forced_idr        <boolean>    E..V....... Forcing I frames as IDR frames (default false)
  -low_power         <boolean>    E..V....... enable low power mode(experimental: many limitations by mfx version, BRC modes, etc.) (default auto)
  -profile           <int>        E..V....... (from 0 to INT_MAX) (default unknown)
     unknown         0            E..V.......
     profile0        1            E..V.......
     profile1        2            E..V.......
     profile2        3            E..V.......
     profile3        4            E..V.......
  -tile_cols         <int>        E..V....... Number of columns for tiled encoding (from 0 to 32) (default 0)
  -tile_rows         <int>        E..V....... Number of rows for tiled encoding (from 0 to 4) (default 0)
*/