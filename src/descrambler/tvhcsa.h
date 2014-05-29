/*
 *  tvheadend - CSA wrapper
 *  Copyright (C) 2013 Adam Sutton
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

#ifndef __TVH_CSA_H__
#define __TVH_CSA_H__

struct mpegts_service;
struct elementary_stream;

#include "tvheadend.h"

#include <stdint.h>
#if ENABLE_DVBCSA
#include <dvbcsa/dvbcsa.h>
#else
#include "ffdecsa/FFdecsa.h"
#endif

typedef struct tvhcsa
{

  /**
   * CSA
   */
  int      csa_cluster_size;
  uint8_t *csa_tsbcluster;
  int      csa_fill;

#if ENABLE_DVBCSA
  struct dvbcsa_bs_batch_s *csa_tsbbatch_even;
  struct dvbcsa_bs_batch_s *csa_tsbbatch_odd;
  int csa_fill_even;
  int csa_fill_odd;

  struct dvbcsa_bs_key_s *csa_key_even;
  struct dvbcsa_bs_key_s *csa_key_odd;
#else
  void *csa_keys;
#endif
  
} tvhcsa_t;

#if ENABLE_DVBCSA

#define tvhcsa_set_key_even(csa, cw)\
  dvbcsa_bs_key_set(cw, (csa)->csa_key_even)

#define tvhcsa_set_key_odd(csa, cw)\
  dvbcsa_bs_key_set(cw, (csa)->csa_key_odd)

#else

#define tvhcsa_set_key_even(csa, cw)\
  set_even_control_word((csa)->csa_keys, cw)

#define tvhcsa_set_key_odd(csa, cw)\
  set_odd_control_word((csa)->csa_keys, cw)

#endif

void
tvhcsa_descramble
  ( tvhcsa_t *csa, struct mpegts_service *s, struct elementary_stream *st,
    const uint8_t *tsb );

void tvhcsa_init    ( tvhcsa_t *csa );
void tvhcsa_destroy ( tvhcsa_t *csa );

#endif /* __TVH_CSA_H__ */
