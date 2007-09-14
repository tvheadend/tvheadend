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

#define PSI_SECTION_SIZE 4096

typedef struct psi_section {
  int ps_offset;
  uint8_t ps_data[PSI_SECTION_SIZE];
} psi_section_t;


int psi_section_reassemble(psi_section_t *ps, uint8_t *data, int len,
			   int pusi, int chkcrc);

int psi_parse_pat(th_transport_t *t, uint8_t *ptr, int len,
		  pid_section_callback_t *pmt_callback);

int psi_parse_pmt(th_transport_t *t, uint8_t *ptr, int len, int chksvcid);

#endif /* PSI_H_ */
