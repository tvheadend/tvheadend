/**
 *  Transcoding
 *  Copyright (C) 2013 John Törnblom
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
#pragma once

#include "tvheadend.h"
#include "htsmsg.h"

typedef struct transcoder_prop {
  char     tp_vcodec[32];
  char     tp_vcodec_preset[32];
  char     tp_acodec[32];
  char     tp_scodec[32];

  int8_t   tp_channels;
  int32_t  tp_vbitrate;
  int32_t  tp_abitrate;
  char     tp_language[4];
  int32_t  tp_resolution;

  long     tp_nrprocessors;
  char     tp_src_vcodec[128];
} transcoder_props_t;

extern uint32_t transcoding_enabled;

streaming_target_t *transcoder_create (streaming_target_t *output);
void                transcoder_destroy(streaming_target_t *tr);

htsmsg_t *transcoder_get_capabilities(int experimental);
void transcoder_set_properties  (streaming_target_t *tr,
				 transcoder_props_t *prop);


void transcoding_init(void);
