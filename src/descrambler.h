/*
 *  Tvheadend
 *  Copyright (C) 2013 Andreas Öman
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

#ifndef __TVH_DESCRAMBLER_H__
#define __TVH_DESCRAMBLER_H__

#include <stdint.h>

#include "queue.h"

struct service;
struct elementary_stream;

/**
 * Descrambler superclass
 *
 * Created/Destroyed on per-transport basis upon transport start/stop
 */
typedef struct th_descrambler {
  LIST_ENTRY(th_descrambler) td_service_link;

  void (*td_table)(struct th_descrambler *d, struct service *t,
		   struct elementary_stream *st, 
		   const uint8_t *section, int section_len);

  int (*td_descramble)(struct th_descrambler *d, struct service *t,
		       struct elementary_stream *st, const uint8_t *tsb);

  void (*td_stop)(struct th_descrambler *d);

} th_descrambler_t;


/**
 * List of CA ids
 */
typedef struct caid {
  LIST_ENTRY(caid) link;

  uint8_t delete_me;
  uint16_t caid;
  uint32_t providerid;

} caid_t;

LIST_HEAD(caid_list, caid);

void descrambler_init          ( void );
void descrambler_service_start ( struct service *t );
const char *descrambler_caid2name(uint16_t caid);
uint16_t descrambler_name2caid(const char *str);

#endif /* __TVH_DESCRAMBLER_H__ */

/* **************************************************************************
 * Editor
 *
 * vim:sts=2:ts=2:sw=2:et
 * *************************************************************************/
