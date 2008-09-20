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

/**
 *
 */
void streaming_pad_init(streaming_pad_t *sp, pthread_mutex_t *mutex);

void streaming_target_init(streaming_target_t *st);

void streaming_target_connect(streaming_pad_t *sp, streaming_target_t *st);

void streaming_target_disconnect(streaming_target_t *st);

void streaming_pad_deliver_packet(streaming_pad_t *sp, th_pkt_t *pkt);

#endif /* STREAMING_H_ */
