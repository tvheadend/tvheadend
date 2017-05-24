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

//-----key structure
struct aes_keys_t {
  AES_KEY even;
  AES_KEY odd;
};

//-----even cw represents one full 128-bit AES key
void aes_set_even_control_word(void *keys, const unsigned char *pk) {
  AES_set_decrypt_key(pk, 128, &((struct aes_keys_t *) keys)->even);
}

//-----odd cw represents one full 128-bit AES key
void aes_set_odd_control_word(void *keys, const unsigned char *pk) {
  AES_set_decrypt_key(pk, 128, &((struct aes_keys_t *) keys)->odd);
}

//-----set control words
void aes_set_control_words(void *keys, const unsigned char *ev,
		const unsigned char *od) {
  AES_set_decrypt_key(ev, 128, &((struct aes_keys_t *) keys)->even);
  AES_set_decrypt_key(od, 128, &((struct aes_keys_t *) keys)->odd);
}

//-----allocate key structure
void * aes_get_key_struct(void) {
  struct aes_keys_t *keys = (struct aes_keys_t *) malloc(
			sizeof(struct aes_keys_t));
  if (keys) {
    static const unsigned char pk[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    aes_set_control_words(keys, pk, pk);
  }
  return keys;
}

//-----free key structure
void aes_free_key_struct(void *keys) {
  if (keys)
    free(keys);
}

//----- decrypt
void aes_decrypt_packet(void *keys, unsigned char *packet) {
  unsigned char *pkt;
  unsigned char ev_od = 0;
  int xc0, offset, n;
  pkt = packet;
  AES_KEY k;

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

  if (ev_od == 0) {
    k = ((struct aes_keys_t *) keys)->even;
  } else {
    k = ((struct aes_keys_t *) keys)->odd;
  }

  for (; offset <= (188 - 16); offset += 16) {
    AES_ecb_encrypt(pkt + offset, pkt + offset, &k, AES_DECRYPT);
  }
}
