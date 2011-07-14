/**
 *  Transcoding
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

#include "tvheadend.h"
#include "streaming.h"
#include "transcode.h"

typedef struct transcoder_t {
  streaming_target_t t_input;  // must be first
  streaming_target_t *t_output;
  
  // transcoder private stuff

} transcoder_t;


/**
 *
 */
static void
transcoder_input(void *opaque, streaming_message_t *sm)
{
  transcoder_t *t = opaque;

  switch(sm->sm_type) {
  case SMT_PACKET:
    break;

  case SMT_START:
    break;

  case SMT_STOP:
    break;

  case SMT_EXIT:
  case SMT_SERVICE_STATUS:
  case SMT_NOSTART:
  case SMT_MPEGTS:
    break;
  }

  streaming_target_deliver2(t->t_output, sm);
}


/**
 *
 */
streaming_target_t *
transcoder_create(streaming_target_t *output)
{
  transcoder_t *t = calloc(1, sizeof(transcoder_t));
  t->t_output = output;

  streaming_target_init(&t->t_input, transcoder_input, t, 0);
  return &t->t_input;
}


/**
 *
 */
void
transcoder_destroy(streaming_target_t *st)
{
  transcoder_t *t = (transcoder_t *)st;

  // cleanup
  free(t);
}
