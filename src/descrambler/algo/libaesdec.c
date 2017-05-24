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

#include "libaesdec.h"

/* key structure */
typedef struct aes_priv {
  AES_KEY keys[2]; /* 0 = even, 1 = odd */
} aes_priv_t;

/* even cw represents one full 128-bit AES key */
void aes_set_even_control_word(void *keys, const uint8_t *pk)
{
  AES_set_decrypt_key(pk, 128, &((aes_priv_t *) keys)->keys[0]);
}

/* odd cw represents one full 128-bit AES key */
void aes_set_odd_control_word(void *keys, const uint8_t *pk)
{
  AES_set_decrypt_key(pk, 128, &((aes_priv_t *) keys)->keys[1]);
}

/* set control words */
void aes_set_control_words(void *keys,
                           const uint8_t *ev,
                           const uint8_t *od)
{
  AES_set_decrypt_key(ev, 128, &((aes_priv_t *) keys)->keys[0]);
  AES_set_decrypt_key(od, 128, &((aes_priv_t *) keys)->keys[1]);
}

/* allocate key structure */
void * aes_get_key_struct(void)
{
  aes_priv_t *keys;

  keys = (aes_priv_t *) malloc(sizeof(aes_priv_t));
  if (keys) {
    static const uint8_t pk[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    aes_set_control_words(keys, pk, pk);
  }
  return keys;
}

/* free key structure */
void aes_free_key_struct(void *keys)
{
  free(keys);
}

/* decrypt */
void aes_decrypt_packet(void *keys, uint8_t *pkt)
{
  uint8_t ev_od = 0;
  uint_fast8_t xc0, offset, n;
  AES_KEY *k;

  xc0 = pkt[3] & 0xc0;

  //skip clear pkt
  if (xc0 == 0x00)
    return;

  //skip reserved pkt
  if (xc0 == 0x40)
    return;

  if (xc0 == 0x80 || xc0 == 0xc0) { // encrypted
    ev_od = (xc0 & 0x40) >> 6; // 0 even, 1 odd
    pkt[3] &= 0x3f;  // consider it decrypted now
    if (pkt[3] & 0x20) { // incomplete packet
      offset = 4 + pkt[4] + 1;
      n = (188 - offset) >> 4;
      if (n == 0) { // decrypted==encrypted!
        return;  // this doesn't need more processing
      }
    } else {
      offset = 4;
    }
  } else {
    return;
  }

  k = &((aes_priv_t *) keys)->keys[ev_od];
  for (; offset <= (188 - 16); offset += 16) {
    AES_ecb_encrypt(pkt + offset, pkt + offset, k, AES_DECRYPT);
  }
}
