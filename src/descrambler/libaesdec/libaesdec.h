/*
 * libaesdec.h
 *
 *  Created on: Jun 22, 2014
 *      Author: spdfrk1
 */

#ifndef LIBAESDEC_H_
#define LIBAESDEC_H_

void *aes_get_key_struct(void);
void aes_free_key_struct(void *keys);
void aes_set_control_words(void *keys, const unsigned char *even, const unsigned char *odd);
void aes_set_even_control_word(void *keys, const unsigned char *even);
void aes_set_odd_control_word(void *keys, const unsigned char *odd);
void aes_decrypt_packet(void *keys, unsigned char *packet);

#endif /* LIBAESDEC_H_ */
