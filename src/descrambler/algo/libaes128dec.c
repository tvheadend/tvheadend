/*
 * libaesdec.c
 *
 *  Created on: Jun 22, 2014
 *      Author: spdfrk1
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "openssl/aes.h"

#include "libaes128dec.h"

/* key structure */
typedef struct aes128_priv {
  AES_KEY keys[2]; /* 0 = even, 1 = odd */
} aes128_priv_t;

/* even cw represents one full 128-bit AES key */
void aes128_set_even_control_word(void *keys, const uint8_t *pk)
{
  AES_set_decrypt_key(pk, 128, &((aes128_priv_t *) keys)->keys[0]);
}

/* odd cw represents one full 128-bit AES key */
void aes128_set_odd_control_word(void *keys, const uint8_t *pk)
{
  AES_set_decrypt_key(pk, 128, &((aes128_priv_t *) keys)->keys[1]);
}

/* set control words */
void aes128_set_control_words(void *keys,
                           const uint8_t *ev,
                           const uint8_t *od)
{
  AES_set_decrypt_key(ev, 128, &((aes128_priv_t *) keys)->keys[0]);
  AES_set_decrypt_key(od, 128, &((aes128_priv_t *) keys)->keys[1]);
}

/* allocate key structure */
void * aes128_get_priv_struct(void)
{
  aes128_priv_t *keys;

  keys = (aes128_priv_t *) malloc(sizeof(aes128_priv_t));
  if (keys) {
    static const uint8_t pk[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    aes128_set_control_words(keys, pk, pk);
  }
  return keys;
}

/* free key structure */
void aes128_free_priv_struct(void *keys)
{
  free(keys);
}

/* decrypt */
void aes128_decrypt_packet(void *keys, const uint8_t *pkt)
{
  uint_fast8_t ev_od = 0;
  uint_fast8_t xc0, offset;
  AES_KEY *k;

  // skip reserved and not encrypted pkt
  if (((xc0 = pkt[3]) & 0x80) == 0)
    return;

  ev_od = (xc0 & 0x40) >> 6; // 0 even, 1 odd
  ((uint8_t *)pkt)[3] = xc0 & 0x3f;  // consider it decrypted now
  if (xc0 & 0x20) { // incomplete packet
    offset = 4 + pkt[4] + 1;
    if (offset + 16 > 188) { // decrypted==encrypted!
      return;  // this doesn't need more processing
    }
  } else {
    offset = 4;
  }

  k = &((aes128_priv_t *) keys)->keys[ev_od];
  for (; offset <= (188 - 16); offset += 16) {
    AES_ecb_encrypt(pkt + offset, (uint8_t *)(pkt + offset), k, AES_DECRYPT);
  }
}
