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

enum CODEC{
    H264 = 1,
    HEVC = 2,
    VP8 = 3,
    VP9 = 4
};
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

/* internal ==================================================================== */

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
    const TVHVideoCodecProfile *vp = (TVHVideoCodecProfile *)self;
    
    // pix_fmt
    AV_DICT_SET_PIX_FMT(LST_VAAPI, opts, vp->pix_fmt, AV_PIX_FMT_VAAPI);
    return 0;
}

/**
 * Derive VAAPI encoder rate-control numbers from the UI profile and output height.
 *
 * Reads @c TVHCodecProfile::bit_rate (kb/s) and @c TVHVideoCodecProfile::size.den
 * (treated here as the active picture height in pixels), together with
 * @c tvh_codec_profile_vaapi_t::bit_rate_scale_factor, @c max_bit_rate, and
 * @c buff_factor. All three outputs are integer values in the same “scaled kb/s”
 * space the rest of this file uses when passing @c b / @c bufsize / @c maxrate into
 * libav (see the @c *1024 / @c *2048 factors and the info log that divides by 1024).
 *
 * Resolution scaling: relative to a 480-line baseline, the effective target bitrate
 * is multiplied by @c 1 + bit_rate_scale_factor * (height - 480) / 480 so higher
 * resolutions increase requested rate and buffer size.
 *
 * Buffer size additionally scales by @c buff_factor (forced to @c 3 if unset or
 * non-positive).
 *
 * If the computed average bitrate exceeds the computed max bitrate, logs an error
 * and raises @p max_bitrate to @p bitrate so the pair is always consistent.
 *
 * @param self         VAAPI codec profile (also carries base @c TVHCodecProfile /
 *                     @c TVHVideoCodecProfile fields).
 * @param bitrate      Out: scaled average / target bitrate.
 * @param buffer_size  Out: scaled VBV buffer size (@c bufsize).
 * @param max_bitrate  Out: scaled peak / max rate; may be bumped up to @p *bitrate.
 */
static void
compute_bitrates(tvh_codec_profile_vaapi_t *self, int *bitrate, int *buffer_size, int *max_bitrate){
    const TVHCodecProfile *p = (TVHCodecProfile *)self;
    const TVHVideoCodecProfile *vp = (TVHVideoCodecProfile *)self;

    // to avoid issues we have this check:
    if (self->buff_factor <= 0) {
        self->buff_factor = 3;
    }
    *bitrate = (int)((p->bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(vp->size.den) - 480.0) / 480.0)));
    *buffer_size = (int)((p->bit_rate) * 2048.0 * self->buff_factor * (1.0 + self->bit_rate_scale_factor * ((double)(vp->size.den) - 480.0) / 480));
    *max_bitrate = (int)((self->max_bit_rate) * 1024.0 * (1.0 + (self->bit_rate_scale_factor * ((double)(vp->size.den) - 480.0) / 480.0)));
    // force max_bitrate to be >= with bitrate (to avoid crash)
    if (*bitrate > *max_bitrate) {
        tvherror_transcode(LST_VAAPI, "Bitrate %d kbps is greater than Max bitrate %d kbps, increase Max bitrate to %d kbps", 
            *bitrate / 1024, *max_bitrate / 1024, *bitrate / 1024);
        *max_bitrate = *bitrate;
    }
    tvhinfo_transcode(LST_VAAPI, "Bitrate = %d kbps; Buffer size = %d kbps; Max bitrate = %d kbps", 
        *bitrate / 1024, *buffer_size / 1024, *max_bitrate / 1024);
}

/**
 * Append “baseline” libavcodec / VAAPI encoder options for the unconstrained platform
 * profile into @p opts.
 *
 * All entries use @c AV_DICT_DONT_OVERWRITE so existing dictionary keys win.
 *
 * Behavior by @p codec:
 * - @c VP8 / @c VP9 — if @c loop_filter_level / @c loop_filter_sharpness are @c >= 0,
 *   set @c loop_filter_level and @c loop_filter_sharpness respectively.
 * - Any codec — if @c async_depth is non-zero, set @c async_depth.
 * - @c H264 / @c HEVC — if @c level is not the sentinel @c -100, set @c level.
 * - Any codec — if @c qmin / @c qmax are non-zero, set @c qmin / @c qmax.
 *
 * @param self  VAAPI profile carrying the tuning fields above.
 * @param opts  Target option dictionary for @c avcodec_open2 (or helper that forwards it).
 * @param codec Which elementary codec is being configured (@c H264, @c HEVC, @c VP8, @c VP9, …).
 * @return      Always @c 0 (no failure path).
 */
static int
setup_baseline_opts_unconstrained(const tvh_codec_profile_vaapi_t *self, AVDictionary **opts, enum CODEC codec){
    if (codec == VP8 || codec == VP9) {
        if (self->loop_filter_level >= 0) {
            AV_DICT_SET_INT(LST_VAAPI, opts, "loop_filter_level", self->loop_filter_level, AV_DICT_DONT_OVERWRITE);
        }
        if (self->loop_filter_sharpness >= 0) {
            AV_DICT_SET_INT(LST_VAAPI, opts, "loop_filter_sharpness", self->loop_filter_sharpness, AV_DICT_DONT_OVERWRITE);
        }
    }
    if (self->async_depth) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "async_depth", self->async_depth, AV_DICT_DONT_OVERWRITE);
    }
    if ((codec == H264 || codec == HEVC) &&
        self->level != -100) {
            AV_DICT_SET_INT(LST_VAAPI, opts, "level", self->level, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmin) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "qmin", self->qmin, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qmax) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "qmax", self->qmax, AV_DICT_DONT_OVERWRITE);
    }
    return 0;
}

/**
 * Push rate-control and fixed-QP libavcodec options into @p opts for the unconstrained
 * VAAPI platform path.
 *
 * Uses @c AV_DICT_DONT_OVERWRITE on every key so callers can pre-seed the dictionary.
 *
 * - If the base profile has a non-zero @c bit_rate (@c TVHCodecProfile), sets
 *   @c b to @p bitrate and @c bufsize to @p buffer_size (typically produced by
 *   @ref compute_bitrates).
 * - If @c max_bit_rate on the VAAPI profile is non-zero, sets @c maxrate to @p max_bitrate.
 * - If @c qp on the VAAPI profile is non-zero, sets @c qp.
 *
 * @param self         VAAPI codec profile (and embedded @c TVHCodecProfile).
 * @param opts         Encoder option dictionary.
 * @param bitrate      Target average bitrate for @c b (same units as @ref compute_bitrates).
 * @param buffer_size  VBV buffer size for @c bufsize.
 * @param max_bitrate  Peak bitrate for @c maxrate.
 * @return             Always @c 0 (no failure path in this helper).
 */
static int
setup_bitrate_qp_opts_unconstrained(const tvh_codec_profile_vaapi_t *self, AVDictionary **opts, int bitrate, int buffer_size, int max_bitrate){
    const TVHCodecProfile *p = (TVHCodecProfile *)self;

    if (p->bit_rate) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "b", bitrate, AV_DICT_DONT_OVERWRITE);
        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", buffer_size, AV_DICT_DONT_OVERWRITE);
    }
    if (self->max_bit_rate) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", max_bitrate, AV_DICT_DONT_OVERWRITE);
    }
    if (self->qp) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
    }
    return 0;
}

/**
 * Layered helper for the unconstrained VAAPI platform: applies bitrate / QP options,
 * then a few codec-specific toggles, then baseline tuning.
 *
 * Steps:
 * 1. @ref setup_bitrate_qp_opts_unconstrained — forwards @p bitrate, @p buffer_size,
 *    and @p max_bitrate into @c b / @c bufsize / @c maxrate (and optional @c qp).
 * 2. @c HEVC only — if @c tier >= 0, sets @c tier.
 * 3. If @c quality >= 0, sets @c quality.
 * 4. @c VP8 / @c VP9 only — if @c global_quality >= 0, sets @c global_quality.
 * 5. If @c low_power is non-zero, sets @c low_power.
 * 6. @ref setup_baseline_opts_unconstrained — note: this is invoked with a hard-coded
 *    @c H264 @p codec argument, not the outer @p codec, so VP9/HEVC baseline fields
 *    (e.g. @c level / @c qmin) follow the H264 branch rules inside that helper.
 *
 * @param self         VAAPI profile.
 * @param opts         Encoder option dictionary.
 * @param codec        Elementary codec being opened (@c HEVC, @c VP8, @c VP9, …).
 * @param bitrate      Passed through to @ref setup_bitrate_qp_opts_unconstrained.
 * @param buffer_size  Passed through to @ref setup_bitrate_qp_opts_unconstrained.
 * @param max_bitrate  Passed through to @ref setup_bitrate_qp_opts_unconstrained.
 * @return             @c 0 on success; otherwise the first non-zero return from a callee
 *                     (today only the baseline helper can propagate, and it always returns 0).
 */
static int
setup_common_opts_unconstrained(const tvh_codec_profile_vaapi_t *self, AVDictionary **opts, enum CODEC codec, int bitrate, int buffer_size, int max_bitrate){
    int ret = 0;

    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, bitrate, buffer_size, max_bitrate))) {
        return ret;
    }

    if ((codec == HEVC) &&
        self->tier >= 0) {
            AV_DICT_SET_INT(LST_VAAPI, opts, "tier", self->tier, AV_DICT_DONT_OVERWRITE);
    }
    if (self->quality >= 0) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
    }
    if ((codec == VP8 || codec == VP9) &&
        self->global_quality >= 0) {
            AV_DICT_SET_INT(LST_VAAPI, opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
    }
    if (self->low_power) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
    }
    if ((ret = setup_baseline_opts_unconstrained(self, opts, codec))) {
        return ret;
    }
    return 0;
}

/**
 * Populate @p opts for @c VAAPI_ENC_PLATFORM_UNCONSTRAINED: “anything goes” mode for
 * new platforms or debugging (invalid combinations are not filtered here).
 *
 * Does the following in order:
 * - If @c b_reference is non-zero, set @c b_depth.
 * - If @c desired_b_depth >= 0, set @c bf (max B-frames).
 * - For @c VP8 or @c VP9, if @c super_frame is enabled, set @c bsf to
 *   @c "vp9_raw_reorder,vp9_superframe" (per FFmpeg VAAPI VP9 notes).
 * - Calls @ref setup_common_opts_unconstrained with a hard-coded @c H264 @c codec
 *   argument for the inner layer—not the outer @p codec—so the common / baseline
 *   branch behaves like the H264 path regardless of whether you are configuring
 *   VP9, HEVC, etc. (worth revisiting if that is unintentional).
 *
 * @param self         VAAPI profile.
 * @param opts         Encoder option dictionary.
 * @param codec        Which codec is being set up (@c H264, @c HEVC, @c VP8, @c VP9, …);
 *                     only affects the VP super-frame @c bsf branch today.
 * @param bitrate      Passed through to @ref setup_common_opts_unconstrained.
 * @param buffer_size  Passed through to @ref setup_common_opts_unconstrained.
 * @param max_bitrate  Passed through to @ref setup_common_opts_unconstrained.
 * @return             @c 0 on success, or the first non-zero return from the common
 *                     helper (currently always @c 0 from downstream helpers).
 */
static int
setup_opts_unconstrained(const tvh_codec_profile_vaapi_t *self, AVDictionary **opts, enum CODEC codec, int bitrate, int buffer_size, int max_bitrate){
    int ret = 0;

    // Uncontrained --> will allow any combination of parameters (valid or invalid)
    // this mode is useful for future platform and for debugging.
    if (self->b_reference) {
        // b_depth
        AV_DICT_SET_INT(LST_VAAPI, opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
    }
    if (self->desired_b_depth >= 0) {
        // max_b_frames
        AV_DICT_SET_INT(LST_VAAPI, opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
    }
    if ((codec == VP8 || codec == VP9) &&
        self->super_frame) {
            // according to example from https://trac.ffmpeg.org/wiki/Hardware/VAAPI
            // -bsf:v vp9_raw_reorder,vp9_superframe
            AV_DICT_SET(LST_VAAPI, opts, "bsf", "vp9_raw_reorder,vp9_superframe", AV_DICT_DONT_OVERWRITE);
    }
    if ((ret = setup_common_opts_unconstrained(self, opts, codec, bitrate, buffer_size, max_bitrate))) {
        return ret;
    }
    return 0;
}

/* external ==================================================================== */

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
                .off      = offsetof(TVHCodecProfile, device),
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
                .id       = "gop_size",     // Don't change
                .name     = N_("GOP size"),
                .desc     = N_("Sets the Group of Pictures (GOP) size in frame (default 0 is 3 sec.)"),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHVideoCodecProfile, gop_size),
                .intextra = INTEXTRA_RANGE(0, 1000, 1),
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
                .off      = offsetof(TVHCodecProfile, bit_rate),
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
                .off      = offsetof(TVHVideoCodecProfile, filter_hw_denoise),
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
                .off      = offsetof(TVHVideoCodecProfile, filter_hw_sharpness),
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
    TVHCodecProfile *p = (TVHCodecProfile *)self;
    int int_bitrate = 0;
    int int_buffer_size = 0;
    int int_max_bitrate = 0;
    int ret = 0;

    compute_bitrates(self, &int_bitrate, &int_buffer_size, &int_max_bitrate);

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    int tempSupport = 0;
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            if ((ret = setup_opts_unconstrained(self, opts, H264, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
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
                    AV_DICT_SET_INT(LST_VAAPI, opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
                }
                if (self->desired_b_depth >= 0) {
                    // max_b_frames
                    AV_DICT_SET_INT(LST_VAAPI, opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
                }
            }
            else {
                // max_b_frames
                AV_DICT_SET_INT(LST_VAAPI, opts, "bf", 0, AV_DICT_DONT_OVERWRITE);
            }

            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
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
                AV_DICT_SET_INT(LST_VAAPI, opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if ((ret = setup_baseline_opts_unconstrained(self, opts, H264))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if ((ret = setup_common_opts_unconstrained(self, opts, H264, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            AV_DICT_SET_INT(LST_VAAPI, opts, "bf", 0, 0);
            break;
    }
    // force keyframe at t=0 and t=1s (used for flashing the encoder)
    AV_DICT_SET(LST_VAAPI, opts, "force_key_frames", "expr:eq(t,0)+eq(t,1)", AV_DICT_DONT_OVERWRITE);
    p->has_support_for_filter2 = 1;
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
    TVHCodecProfile *p = (TVHCodecProfile *)self;
    int int_bitrate = 0;
    int int_buffer_size = 0;
    int int_max_bitrate = 0;
    int ret = 0;

    compute_bitrates(self, &int_bitrate, &int_buffer_size, &int_max_bitrate);

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            if ((ret = setup_opts_unconstrained(self, opts, HEVC, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(LST_VAAPI, opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(LST_VAAPI, opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP and ICQ we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->tier >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "tier", self->tier, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->low_power) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if ((ret = setup_baseline_opts_unconstrained(self, opts, HEVC))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if ((ret = setup_common_opts_unconstrained(self, opts, HEVC, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            AV_DICT_SET_INT(LST_VAAPI, opts, "bf", 0, 0);
            break;
    }
    // force keyframe at t=0 and t=1s (used for flashing the encoder)
    AV_DICT_SET(LST_VAAPI, opts, "force_key_frames", "expr:eq(t,0)+eq(t,1)", AV_DICT_DONT_OVERWRITE);
    p->has_support_for_filter2 = 1;
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
    TVHCodecProfile *p = (TVHCodecProfile *)self;
    int int_bitrate = 0;
    int int_buffer_size = 0;
    int int_max_bitrate = 0;
    int ret = 0;

    compute_bitrates(self, &int_bitrate, &int_buffer_size, &int_max_bitrate);

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            if ((ret = setup_opts_unconstrained(self, opts, VP8, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(LST_VAAPI, opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(LST_VAAPI, opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->low_power) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if ((ret = setup_baseline_opts_unconstrained(self, opts, VP8))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if ((ret = setup_common_opts_unconstrained(self, opts, VP8, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            AV_DICT_SET_INT(LST_VAAPI, opts, "bf", 0, 0);

            break;
    }
    // force keyframe at t=0 and t=1s (used for flashing the encoder)
    AV_DICT_SET(LST_VAAPI, opts, "force_key_frames", "expr:eq(t,0)+eq(t,1)", AV_DICT_DONT_OVERWRITE);
    p->has_support_for_filter2 = 1;
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
    TVHCodecProfile *p = (TVHCodecProfile *)self;
    int int_bitrate = 0;
    int int_buffer_size = 0;
    int int_max_bitrate = 0;
    int ret = 0;

    compute_bitrates(self, &int_bitrate, &int_buffer_size, &int_max_bitrate);

    if (self->rc_mode != VAAPI_ENC_PARAMS_RC_SKIP) {
        AV_DICT_SET_INT(LST_VAAPI, opts, "rc_mode", self->rc_mode, AV_DICT_DONT_OVERWRITE);
    }
    switch (self->platform) {
        case VAAPI_ENC_PLATFORM_UNCONSTRAINED:
            if ((ret = setup_opts_unconstrained(self, opts, VP9, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_INTEL:
            // Intel
            if (self->b_reference) {
                // b_depth
                AV_DICT_SET_INT(LST_VAAPI, opts, "b_depth", self->b_reference, AV_DICT_DONT_OVERWRITE);
            }
            if (self->desired_b_depth >= 0) {
                // max_b_frames
                AV_DICT_SET_INT(LST_VAAPI, opts, "bf", self->desired_b_depth, AV_DICT_DONT_OVERWRITE);
            }
            if (self->super_frame) {
                // according to example from https://trac.ffmpeg.org/wiki/Hardware/VAAPI
                // -bsf:v vp9_raw_reorder,vp9_superframe
                AV_DICT_SET(LST_VAAPI, opts, "bsf", "vp9_raw_reorder,vp9_superframe", AV_DICT_DONT_OVERWRITE);
            }
            if (self->quality >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "quality", self->quality, AV_DICT_DONT_OVERWRITE);
            }
            if (self->global_quality >= 0) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "global_quality", self->global_quality, AV_DICT_DONT_OVERWRITE);
            }
            switch (self->rc_mode) {
                case VAAPI_ENC_PARAMS_RC_SKIP:
                    // same like 0 but is not sending 'rc_mode'
                case VAAPI_ENC_PARAMS_RC_AUTO:
                    // for auto --> let the driver decide as requested by documentation
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CONSTQP:
                case VAAPI_ENC_PARAMS_RC_ICQ:
                    // for constant quality: CQP we use qp
                    if (self->qp) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "qp", self->qp, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_CBR:
                    // for constant bitrate: CBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_VBR:
                    // for variable bitrate: VBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "bufsize", int_buffer_size, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_QVBR:
                    // for variable bitrate: QVBR we use bitrate + qp
                    if ((ret = setup_bitrate_qp_opts_unconstrained(self, opts, int_bitrate, int_buffer_size, int_max_bitrate))) {
                        return ret;
                    }
                    break;
                case VAAPI_ENC_PARAMS_RC_AVBR:
                    // for variable bitrate: AVBR we use bitrate
                    if (p->bit_rate && self->buff_factor) {
                        AV_DICT_SET_INT(LST_VAAPI, opts, "b", int_bitrate, AV_DICT_DONT_OVERWRITE);
                        AV_DICT_SET_INT(LST_VAAPI, opts, "maxrate", int_max_bitrate, AV_DICT_DONT_OVERWRITE);
                    }
                    break;
            }
            if (self->low_power) {
                AV_DICT_SET_INT(LST_VAAPI, opts, "low_power", self->low_power, AV_DICT_DONT_OVERWRITE);
            }
            if ((ret = setup_baseline_opts_unconstrained(self, opts, VP9))) {
                return ret;
            }
            break;
        case VAAPI_ENC_PLATFORM_AMD:
            // AMD --> will allow any combination of parameters
            // I am unable to confirm this platform because I don't have the HW
            // Is only going to override bf to 0 (as highlited by the previous implementation)
            if ((ret = setup_common_opts_unconstrained(self, opts, VP9, int_bitrate, int_buffer_size, int_max_bitrate))) {
                return ret;
            }
            AV_DICT_SET_INT(LST_VAAPI, opts, "bf", 0, 0);
            break;
    }
    // force keyframe at t=0 and t=1s (used for flashing the encoder)
    AV_DICT_SET(LST_VAAPI, opts, "force_key_frames", "expr:eq(t,0)+eq(t,1)", AV_DICT_DONT_OVERWRITE);
    p->has_support_for_filter2 = 1;
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
