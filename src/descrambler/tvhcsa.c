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

#include "tvhcsa.h"
#include "input.h"
#include "input/mpegts/tsdemux.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

static void
tvhcsa_aes_descramble
  ( tvhcsa_t *csa, struct mpegts_service *s, const uint8_t *tsb )
{
	aes_decrypt_packet(csa->csa_aes_keys, (unsigned char *)tsb);
}

static void
tvhcsa_des_descramble
  ( tvhcsa_t *csa, struct mpegts_service *s, const uint8_t *tsb )
{
#if ENABLE_DVBCSA
  uint8_t *pkt;
  int xc0;
  int ev_od;
  int len;
  int offset;
  int n;
  int i;
  const uint8_t *t0;

  pkt = csa->csa_tsbcluster + csa->csa_fill * 188;
  memcpy(pkt, tsb, 188);
  csa->csa_fill++;

  do { // handle this packet
    xc0 = pkt[3] & 0xc0;
    if(xc0 == 0x00) { // clear
      break;
    }
    if(xc0 == 0x40) { // reserved
      break;
    }
    if(xc0 == 0x80 || xc0 == 0xc0) { // encrypted
      ev_od = (xc0 & 0x40) >> 6; // 0 even, 1 odd
      pkt[3] &= 0x3f;  // consider it decrypted now
      if(pkt[3] & 0x20) { // incomplete packet
        offset = 4 + pkt[4] + 1;
        len = 188 - offset;
        n = len >> 3;
        // FIXME: //residue = len - (n << 3);
        if(n == 0) { // decrypted==encrypted!
          break; // this doesn't need more processing
        }
      } else {
        len = 184;
        offset = 4;
        // FIXME: //n = 23;
        // FIXME: //residue = 0;
      }
      if(ev_od == 0) {
        csa->csa_tsbbatch_even[csa->csa_fill_even].data = pkt + offset;
        csa->csa_tsbbatch_even[csa->csa_fill_even].len = len;
        csa->csa_fill_even++;
      } else {
        csa->csa_tsbbatch_odd[csa->csa_fill_odd].data = pkt + offset;
        csa->csa_tsbbatch_odd[csa->csa_fill_odd].len = len;
        csa->csa_fill_odd++;
      }
    }
  } while(0);

  if(csa->csa_fill != csa->csa_cluster_size)
    return;

  if(csa->csa_fill_even) {
    csa->csa_tsbbatch_even[csa->csa_fill_even].data = NULL;
    dvbcsa_bs_decrypt(csa->csa_key_even, csa->csa_tsbbatch_even, 184);
    csa->csa_fill_even = 0;
  }
  if(csa->csa_fill_odd) {
    csa->csa_tsbbatch_odd[csa->csa_fill_odd].data = NULL;
    dvbcsa_bs_decrypt(csa->csa_key_odd, csa->csa_tsbbatch_odd, 184);
    csa->csa_fill_odd = 0;
  }

  t0 = csa->csa_tsbcluster;

  for(i = 0; i < csa->csa_fill; i++) {
	  ts_recv_packet2(s, t0);
	  t0 += 188;
  }
  csa->csa_fill = 0;

#else
  int r;
  unsigned char *vec[3];

  memcpy(csa->csa_tsbcluster + csa->csa_fill * 188, tsb, 188);
  csa->csa_fill++;

  if(csa->csa_fill != csa->csa_cluster_size)
    return;

  while(1) {

    vec[0] = csa->csa_tsbcluster;
    vec[1] = csa->csa_tsbcluster + csa->csa_fill * 188;
    vec[2] = NULL;
    
    r = decrypt_packets(csa->csa_keys, vec);
    if(r > 0) {
      int i;
      const uint8_t *t0 = csa->csa_tsbcluster;

      for(i = 0; i < r; i++) {
	      ts_recv_packet2(s, t0);
	      t0 += 188;
      }

      r = csa->csa_fill - r;
      assert(r >= 0);

      if(r > 0)
	      memmove(csa->csa_tsbcluster, t0, r * 188);
      csa->csa_fill = r;
    } else {
      csa->csa_fill = 0;
    }
    break;
  }
#endif
}

int
tvhcsa_set_type( tvhcsa_t *csa, int type )
{
  if (csa->csa_type == type)
    return 0;
  if (csa->csa_descramble)
    return -1;
  switch (type) {
  case DESCRAMBLER_DES:
    csa->csa_descramble = tvhcsa_des_descramble;
    csa->csa_keylen     = 8;
    break;
  case DESCRAMBLER_AES:
    csa->csa_descramble = tvhcsa_aes_descramble;
    csa->csa_keylen     = 16;
    break;
  default:
    assert(0);
  }
  csa->csa_type = type;
  return 0;
}


void tvhcsa_set_key_even( tvhcsa_t *csa, const uint8_t *even )
{
  switch (csa->csa_type) {
  case DESCRAMBLER_DES:
#if ENABLE_DVBCSA
    dvbcsa_bs_key_set(even, csa->csa_key_even);
#else
    set_even_control_word((csa)->csa_keys, even);
#endif
    break;
  case DESCRAMBLER_AES:
    aes_set_even_control_word(csa->csa_aes_keys, even);
    break;
  default:
    assert(0);
  }
}

void tvhcsa_set_key_odd( tvhcsa_t *csa, const uint8_t *odd )
{
  assert(csa->csa_type);
  switch (csa->csa_type) {
  case DESCRAMBLER_DES:
#if ENABLE_DVBCSA
    dvbcsa_bs_key_set(odd, csa->csa_key_odd);
#else
    set_odd_control_word((csa)->csa_keys, odd);
#endif
    break;
  case DESCRAMBLER_AES:
	aes_set_odd_control_word(csa->csa_aes_keys, odd);
    break;
  default:
    assert(0);
  }
}

void
tvhcsa_init ( tvhcsa_t *csa )
{
  csa->csa_type          = 0;
  csa->csa_keylen        = 0;
#if ENABLE_DVBCSA
  csa->csa_cluster_size  = dvbcsa_bs_batch_size();
#else
  csa->csa_cluster_size  = get_suggested_cluster_size();
#endif
  csa->csa_tsbcluster    = malloc(csa->csa_cluster_size * 188);
#if ENABLE_DVBCSA
  csa->csa_tsbbatch_even = malloc((csa->csa_cluster_size + 1) *
                                   sizeof(struct dvbcsa_bs_batch_s));
  csa->csa_tsbbatch_odd  = malloc((csa->csa_cluster_size + 1) *
                                   sizeof(struct dvbcsa_bs_batch_s));
  csa->csa_key_even      = dvbcsa_bs_key_alloc();
  csa->csa_key_odd       = dvbcsa_bs_key_alloc();
#else
  csa->csa_keys          = get_key_struct();
#endif
}

void
tvhcsa_destroy ( tvhcsa_t *csa )
{
#if ENABLE_DVBCSA
  dvbcsa_bs_key_free(csa->csa_key_odd);
  dvbcsa_bs_key_free(csa->csa_key_even);
  free(csa->csa_tsbbatch_odd);
  free(csa->csa_tsbbatch_even);
#else
  free_key_struct(csa->csa_keys);
#endif
  free(csa->csa_tsbcluster);
}
