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


#define AV_DICT_SET_QP(d, v, a) \
    AV_DICT_SET_INT((d), "qp", (v) ? (v) : (a), AV_DICT_DONT_OVERWRITE)


/* vaapi ==================================================================== */

typedef struct {
    TVHVideoCodecProfile;
    int qp;
    int quality;
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

static const codec_profile_class_t codec_profile_vaapi_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_video_class,
        .ic_class      = "codec_profile_vaapi",
        .ic_caption    = N_("vaapi"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_STR,
                .id       = "device",
                .name     = N_("Device name"),
                .desc     = N_("Device name (e.g. /dev/dri/renderD129)."),
                .group    = 3,
                .off      = offsetof(tvh_codec_profile_vaapi_t, device),
                .list     = tvh_codec_profile_vaapi_device_list,
            },
            {
                .type     = PT_DBL,
                .id       = "bit_rate",
                .name     = N_("Bitrate (kb/s) (0=auto)"),
                .desc     = N_("Target bitrate."),
                .group    = 3,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(TVHCodecProfile, bit_rate),
                .def.d    = 0,
            },
            {
                .type     = PT_INT,
                .id       = "qp",
                .name     = N_("Constant QP (0=auto)"),
                .group    = 3,
                .desc     = N_("Fixed QP of P frames [0-52]."),
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, qp),
                .intextra = INTEXTRA_RANGE(0, 52, 1),
                .def.i    = 0,
            },
            {}
        }
    },
    .open = (codec_profile_open_meth)tvh_codec_profile_vaapi_open,
};


/* h264_vaapi =============================================================== */

static const AVProfile vaapi_h264_profiles[] = {
    { FF_PROFILE_H264_BASELINE,             "Baseline" },
    { FF_PROFILE_H264_CONSTRAINED_BASELINE, "Constrained Baseline" },
    { FF_PROFILE_H264_MAIN,                 "Main" },
    { FF_PROFILE_H264_HIGH,                 "High" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_h264_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // bit_rate or qp
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_QP(opts, self->qp, 20);
    }
    AV_DICT_SET_INT(opts, "quality", self->quality, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_h264_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_h264",
        .ic_caption    = N_("vaapi_h264"),
        .ic_properties = (const property_t[]){
            {
                .type     = PT_INT,
                .id       = "quality",
                .name     = N_("Quality (0=auto)"),
                .desc     = N_("Set encode quality (trades off against speed, "
                               "higher is faster) [0-8]."),
                .group    = 5,
                .opts     = PO_EXPERT,
                .get_opts = codec_profile_class_get_opts,
                .off      = offsetof(tvh_codec_profile_vaapi_t, quality),
                .intextra = INTEXTRA_RANGE(0, 8, 1),
                .def.i    = 0,
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
};


/* hevc_vaapi =============================================================== */

static const AVProfile vaapi_hevc_profiles[] = {
    { FF_PROFILE_HEVC_MAIN, "Main" },
    { FF_PROFILE_UNKNOWN },
};

static int
tvh_codec_profile_vaapi_hevc_open(tvh_codec_profile_vaapi_t *self,
                                  AVDictionary **opts)
{
    // bit_rate or qp
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_QP(opts, self->qp, 25);
    }
    // max_b_frames
    // XXX: remove when b-frames handling in vaapi_encode is fixed
    AV_DICT_SET_INT(opts, "bf", 0, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_hevc_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_hevc",
        .ic_caption    = N_("vaapi_hevc")
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
    // bit_rate or qp
    if (self->bit_rate) {
        AV_DICT_SET_BIT_RATE(opts, self->bit_rate);
    }
    else {
        AV_DICT_SET_QP(opts, self->qp, 25);
    }
    // force zero here, until encoder is fixed
    AV_DICT_SET_INT(opts, "bf", 0, 0);
    return 0;
}


static const codec_profile_class_t codec_profile_vaapi_vp8_class = {
    {
        .ic_super      = (idclass_t *)&codec_profile_vaapi_class,
        .ic_class      = "codec_profile_vaapi_vp8",
        .ic_caption    = N_("vaapi_vp8")
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
