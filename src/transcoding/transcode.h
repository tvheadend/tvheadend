/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
 */

#ifndef TVH_TRANSCODING_TRANSCODE_H__
#define TVH_TRANSCODING_TRANSCODE_H__


#include "tvheadend.h"
#include "streaming.h"


/* TVHTranscoder ============================================================ */

streaming_target_t *
transcoder_create(streaming_target_t *output,
                  const char **profiles,
                  const char **src_codecs);

void
transcoder_destroy(streaming_target_t *st);


/* module level ============================================================= */

void
transcode_init(void);

void
transcode_done(void);


#endif // TVH_TRANSCODING_TRANSCODE_H__
