/*
 * libaesdec.h
 *
 *  Created on: Jun 22, 2014
 *      Author: spdfrk1
 */

#ifndef LIBAESDEC_H_
#define LIBAESDEC_H_

#include "build.h"

#if ENABLE_SSL

void *aes_get_key_struct(void);
void aes_free_key_struct(void *keys);
void aes_set_control_words(void *keys, const unsigned char *even, const unsigned char *odd);
void aes_set_even_control_word(void *keys, const unsigned char *even);
void aes_set_odd_control_word(void *keys, const unsigned char *odd);
void aes_decrypt_packet(void *keys, unsigned char *packet);

#else

// empty functions
static inline void *aes_get_key_struct(void)  { return 0; };
static inline void aes_free_key_struct(void *keys) { return; };
static inline void aes_set_control_words(void *keys, const unsigned char *even, const unsigned char *odd) { return; };
static inline void aes_set_even_control_word(void *keys, const unsigned char *even) { return; };
static inline void aes_set_odd_control_word(void *keys, const unsigned char *odd) { return; };
static inline void aes_decrypt_packet(void *keys, unsigned char *packet) { return; };

#endif

#endif /* LIBAESDEC_H_ */
