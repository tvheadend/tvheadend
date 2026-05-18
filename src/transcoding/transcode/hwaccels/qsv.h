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


#ifndef TVH_TRANSCODING_TRANSCODE_HWACCELS_QSV_H__
#define TVH_TRANSCODING_TRANSCODE_HWACCELS_QSV_H__


#include "tvheadend.h"

#include <libavcodec/avcodec.h>


/* decoding ================================================================= */

int
qsv_decode_setup_context(AVCodecContext *avctx);

int
qsv_get_filters(TVHContext *self, char *filter, size_t filter_len);

int
qsv_get_download(TVHContext *self, int skip_format, char *filter, size_t filter_len);

/* encoding ================================================================= */

int
qsv_encode_setup_context(AVCodecContext *avctx);

int
qsv_get_upload(TVHContext *self, char *filter, size_t filter_len);

/* module =================================================================== */

void
qsv_decode_destroy(TVHContext *ctx);

void
qsv_done(void);


#endif // TVH_TRANSCODING_TRANSCODE_HWACCELS_QSV_H__
