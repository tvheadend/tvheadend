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

#include "packet.h"
#include "htsmsg.h"
#include "service.h"


typedef struct streaming_start_component {
  int ssc_index;
  int ssc_type;
  char ssc_lang[4];
  uint8_t ssc_audio_type;
  uint8_t ssc_audio_version;
  uint16_t ssc_composition_id;
  uint16_t ssc_ancillary_id;
  uint16_t ssc_pid;
  int16_t ssc_width;
  int16_t ssc_height;
  int16_t ssc_aspect_num;
  int16_t ssc_aspect_den;
  uint8_t ssc_sri;
  uint8_t ssc_ext_sri;
  uint8_t ssc_channels;
  uint8_t ssc_disabled;
  uint8_t ssc_muxer_disabled;
  
  pktbuf_t *ssc_gh;

  int ssc_frameduration;

} streaming_start_component_t;



typedef struct streaming_start {
  int ss_refcount;

  int ss_num_components;

  source_info_t ss_si;

  uint16_t ss_pcr_pid;
  uint16_t ss_pmt_pid;
  uint16_t ss_service_id;

  streaming_start_component_t ss_components[0];

} streaming_start_t;


/**
 *
 */
void streaming_pad_init(streaming_pad_t *sp);

void streaming_target_init(streaming_target_t *st,
			   streaming_ops_t *ops, void *opaque,
			   int reject_filter);

void streaming_queue_init
  (streaming_queue_t *sq, int reject_filter, size_t maxsize);

void streaming_queue_clear(struct streaming_message_queue *q);

void streaming_queue_deinit(streaming_queue_t *sq);

void streaming_queue_remove(streaming_queue_t *sq, streaming_message_t *sm);

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

static inline void
streaming_target_deliver(streaming_target_t *st, streaming_message_t *sm)
  { st->st_ops.st_cb(st->st_opaque, sm); }

void streaming_target_deliver2(streaming_target_t *st, streaming_message_t *sm);

static inline void streaming_start_ref(streaming_start_t *ss)
{
  atomic_add(&ss->ss_refcount, 1);
}

void streaming_start_unref(streaming_start_t *ss);

streaming_start_t *streaming_start_copy(const streaming_start_t *src);

static inline int
streaming_pad_probe_type(streaming_pad_t *sp, streaming_message_type_t smt)
{
  return (sp->sp_reject_filter & SMT_TO_MASK(smt)) == 0;
}

const char *streaming_code2txt(int code);

streaming_start_component_t *streaming_start_component_find_by_index(streaming_start_t *ss, int idx);

void streaming_init(void);
void streaming_done(void);

#endif /* STREAMING_H_ */
