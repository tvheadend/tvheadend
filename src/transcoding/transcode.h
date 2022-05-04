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
