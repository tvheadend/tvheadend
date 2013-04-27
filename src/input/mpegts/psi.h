/*
 *  Multicasted IPTV Input
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef PSI_H_
#define PSI_H_

#include "htsmsg.h"
#include "streaming.h"

#define PSI_SECTION_SIZE 5000

// TODO: tidy this up
struct mpegts_service;
struct mpegts_table;

typedef void (section_handler_t)(const uint8_t *data, size_t len, void *opaque);

typedef struct psi_section {
  int ps_offset;
  int ps_lock;
  uint8_t ps_data[PSI_SECTION_SIZE];
} psi_section_t;


void psi_section_reassemble(psi_section_t *ps, const uint8_t *tsb, int crc,
			    section_handler_t *cb, void *opaque);

int psi_parse_pmt
  (struct mpegts_service *t, const uint8_t *ptr, int len, int chksvcid,
   int delete);

const char *psi_caid2name(uint16_t caid);

void psi_load_service_settings(htsmsg_t *m, struct mpegts_service *t);
void psi_save_service_settings(htsmsg_t *m, struct mpegts_service *t);

int psi_pat_callback
  (struct mpegts_table *mt, const uint8_t *ptr, int len, int tableid);

#endif /* PSI_H_ */
