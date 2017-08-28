/*
 *  tvheadend - Transcoding
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
