/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2026 Tvheadend
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
#include "../internals.h"
#include "v4l2-m2m.h"

#include <libavutil/hwcontext.h>
 
#define ALIGN 32 // 32 for NV12

typedef struct tvh_drm_context_t {
    AVBufferRef *hw_device_ref;
} TVDRMContext;


/* TVDRMContext ============================================================= */

static void
tvh_drm_context_destroy(TVDRMContext *self)
{
    if (self) {
        if (self->hw_device_ref) {
            av_buffer_unref(&self->hw_device_ref);
        }
        free(self);
        self = NULL;
    }
}

/* internal ================================================================= */


/* decoding ================================================================= */

int
v4l2m2m_get_download(TVHContext *self, char *filter, size_t filter_len)
{
    if (str_snprintf(filter, filter_len, "format=pix_fmts=%s",
                    av_get_pix_fmt_name(self->iavctx->sw_pix_fmt))) {
        return -1;
    }
    return 0;
}

/* encoding ================================================================= */

int
v4l2m2m_get_filters(TVHContext *self, char *filter, size_t filter_len)    
{
    return 0;
}

int
v4l2m2m_encode_setup_context(AVCodecContext *avctx)
{
    TVHContext *ctx = avctx->opaque;
    int ret = 0;

    // Use the DRM device (render node)
    ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/renderD128", NULL, 0);
    if (ret < 0) {
        // Fallback to /dev/dri/card0 if renderD128 isn't available
        ret = av_hwdevice_ctx_create(&ctx->hw_device_octx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/card0", NULL, 0);
    }

    avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
    avctx->sw_pix_fmt = AV_PIX_FMT_NV12;
    ctx->use_pkt_data_new_extradata = 1;
    return ret;
}

int
v4l2m2m_get_upload(TVHContext *self, char *filter, size_t filter_len) {
    if (str_snprintf(filter, filter_len, "format=nv12,pad=width=ceil(iw/32)*32:height=ceil(ih/32)*32:x=(ow-iw)/2:y=(oh-ih)/2:color=black")) {
        return -1;
    }
    return 0;
}

/* module =================================================================== */

void
v4l2m2m_decode_destroy(TVHContext *ctx)
{
    if (!ctx)
        return;
    tvh_drm_context_destroy(ctx->hw_accel_ictx);
    ctx->hw_accel_ictx = NULL;
}

void
v4l2m2m_done()
{
    /* nothing to do */
}
