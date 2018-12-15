/*
 *  MPEG TS Program Specific Information code
 *  Copyright (C) 2007 - 2010 Andreas Öman
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

#ifndef __DVB_PSI_PMT_H
#define __DVB_PSI_PMT_H 1

/*
 * PMT processing
 */

/* PMT update reason flags */
#define PMT_UPDATE_PCR                (1<<0)
#define PMT_UPDATE_NEW_STREAM         (1<<1)
#define PMT_UPDATE_STREAM_CHANGE      (1<<2)
#define PMT_UPDATE_STREAM_DELETED     (1<<3)
#define PMT_UPDATE_LANGUAGE           (1<<4)
#define PMT_UPDATE_AUDIO_TYPE         (1<<5)
#define PMT_UPDATE_AUDIO_VERSION      (1<<6)
#define PMT_UPDATE_FRAME_DURATION     (1<<7)
#define PMT_UPDATE_COMPOSITION_ID     (1<<8)
#define PMT_UPDATE_ANCILLARY_ID       (1<<9)
#define PMT_UPDATE_NEW_CA_STREAM      (1<<10)
#define PMT_UPDATE_NEW_CAID           (1<<11)
#define PMT_UPDATE_CA_PROVIDER_CHANGE (1<<12)
#define PMT_UPDATE_CAID_DELETED       (1<<13)
#define PMT_UPDATE_CAID_PID           (1<<14)
#define PMT_REORDERED                 (1<<15)

uint32_t dvb_psi_parse_pmt
  (mpegts_table_t *mt, const char *nicename, elementary_set_t *set,
   const uint8_t *ptr, int len);

#endif
