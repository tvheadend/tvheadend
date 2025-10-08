/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2016 Tvheadend
 *
 * tvheadend - Transcoding
 */

#include "transcoding/transcode.h"
#include "internals.h"

#if ENABLE_HWACCELS
#include "hwaccels/hwaccels.h"
#endif


/* TVHTranscoder ============================================================ */

streaming_target_t *
transcoder_create(streaming_target_t *output,
                  const char **profiles,
                  const char **src_profiles)
{
    return (streaming_target_t *)tvh_transcoder_create(output, profiles, src_profiles);
}


void
transcoder_destroy(streaming_target_t *st)
{
    tvh_transcoder_destroy((TVHTranscoder *)st);
}


/* module level ============================================================= */

void
transcode_init()
{
    tvh_context_types_register();
    tvh_context_helpers_register();
#if ENABLE_HWACCELS
    hwaccels_init();
#endif
}

void
transcode_done()
{
#if ENABLE_HWACCELS
    hwaccels_done();
#endif
    tvh_context_helpers_forget();
    tvh_context_types_forget();
}
