/*
 *  Stream plumbing, connects individual streaming components to each other
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef STREAMING_H_
#define STREAMING_H_

#include "tvhead.h"
#include "packet.h"
#include "htsmsg.h"


typedef struct streaming_start_component {
  int ssc_index;
  int ssc_type;
  char ssc_lang[4];
  uint16_t ssc_composition_id;
  uint16_t ssc_ancillary_id;
} streaming_start_component_t;



typedef struct streaming_start {
  int ss_refcount;

  int ss_num_components;

  source_info_t ss_si;

  streaming_start_component_t ss_components[0];

} streaming_start_t;


/**
 *
 */
void streaming_pad_init(streaming_pad_t *sp);

void streaming_target_init(streaming_target_t *st,
			   st_callback_t *cb, void *opaque,
			   int reject_filter);

void streaming_queue_init(streaming_queue_t *sq);

void streaming_queue_clear(struct streaming_message_queue *q);

void streaming_target_connect(streaming_pad_t *sp, streaming_target_t *st);

void streaming_target_disconnect(streaming_pad_t *sp, streaming_target_t *st);

void streaming_pad_deliver(streaming_pad_t *sp, streaming_message_t *sm);

void streaming_msg_free(streaming_message_t *sm);

streaming_message_t *streaming_msg_clone(streaming_message_t *src);

streaming_message_t *streaming_msg_create(streaming_message_type_t type);

streaming_message_t *streaming_msg_create_data(streaming_message_type_t type, 
					       void *data);

streaming_message_t *streaming_msg_create_code(streaming_message_type_t type, 
					       int code);

streaming_message_t *streaming_msg_create_pkt(th_pkt_t *pkt);

#define streaming_target_deliver(st, sm) ((st)->st_cb((st)->st_opaque, (sm)))

void streaming_start_unref(streaming_start_t *ss);

int streaming_pad_probe_type(streaming_pad_t *sp, 
			     streaming_message_type_t smt);

const char *streaming_code2txt(int code);

#endif /* STREAMING_H_ */
