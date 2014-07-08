/*
 * libaesdec.c
 *
 *  Created on: Jun 22, 2014
 *      Author: root
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "openssl/aes.h"

#include "libaesdec.h"

struct aes_keys_t {
	AES_KEY even;
	AES_KEY odd; // Reserved for future use
};

unsigned char * keybuffer;

// Even and Odd cw represent one full 128-bit AES key
void aes_set_even_control_word(void *keys, const unsigned char *pk) {
	memcpy(keybuffer, pk, 8);
	AES_set_decrypt_key(keybuffer, 128, &((struct aes_keys_t *) keys)->even);
}

void aes_set_odd_control_word(void *keys, const unsigned char *pk) {
	memcpy(keybuffer + 8, pk, 8);
	AES_set_decrypt_key(keybuffer, 128, &((struct aes_keys_t *) keys)->even);
}

//-----set control words
void aes_set_control_words(void *keys, const unsigned char *ev,
		const unsigned char *od) {
	memcpy(keybuffer, ev, 8);
	memcpy(keybuffer + 8, od, 8);
	AES_set_decrypt_key(keybuffer, 128, &((struct aes_keys_t *) keys)->even);
}

//-----key structure

void *aes_get_key_struct(void) {
	keybuffer = calloc(16, 1);
	struct aes_keys_t *keys = (struct aes_keys_t *) malloc(
			sizeof(struct aes_keys_t));
	if (keys) {
		static const unsigned char pk[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		aes_set_control_words(keys, pk, pk);
	}
	return keys;
}

void aes_free_key_struct(void *keys) {
	free(keybuffer);
	return free(keys);
}

//----- decrypt

void aes_decrypt_packet(void *keys, unsigned char *packet) {
	unsigned char *pkt;
	int xc0, len, offset, n;

	pkt = packet;
	AES_KEY k;

	// TODO check all flags
		xc0 = pkt[3] & 0xc0;

		if (xc0 == 0x00) {//skip clear pkt
			return;
		}
		if (xc0 == 0x40) { //skip reserved pkt
			return;
		}
	
	if (xc0 == 0x80 || xc0 == 0xc0) { // encrypted 
		pkt[3] &= 0x3f;  // consider it decrypted now
		if (pkt[3] & 0x20) { // incomplete packet
				offset = 4 + pkt[4] + 1;
				len = 188 - offset;
				n = len >> 3;
				//residue = len - (n << 3);
				if (n == 0) { // decrypted==encrypted!
					//DBG(fprintf(stderr,"DECRYPTED MINI!\n"));
					return;  // this doesn't need more processing
				}
                return; // TODO Handle incomplete packets?
		}
	}
	else {
		return;
	}

	k = ((struct aes_keys_t *) keys)->even;

	// TODO room for improvement?
	int i;
	for (i = 4; i <= 164; i += 16) {
		AES_ecb_encrypt(pkt + i, pkt + i, &k, AES_DECRYPT);
	}
	return;
}
