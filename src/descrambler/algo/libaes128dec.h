/*
 * libaes128dec.h
 *
 * Created on: Jun 22, 2014
 *      Author: spdfrk1
 */

#ifndef LIBAES128DEC_H_
#define LIBAES128DEC_H_

#include <stdint.h>
#include "build.h"

#if ENABLE_SSL

void *aes128_get_priv_struct(void);
void aes128_free_priv_struct(void *keys);
void aes128_set_control_words(void *keys, const uint8_t *even, const uint8_t *odd);
void aes128_set_even_control_word(void *keys, const uint8_t *even);
void aes128_set_odd_control_word(void *keys, const uint8_t *odd);
void aes128_decrypt_packet(void *keys, const uint8_t *pkt);

#else

// empty functions
static inline void *aes128_get_priv_struct(void)  { return NULL; };
static inline void aes128_free_priv_struct(void *keys) { return; };
static inline void aes128_set_control_words(void *keys, const uint8_t *even, const uint8_t *odd) { return; };
static inline void aes128_set_even_control_word(void *keys, const uint8_t *even) { return; };
static inline void aes128_set_odd_control_word(void *keys, const uint8_t *odd) { return; };
static inline void aes128_decrypt_packet(void *keys, const uint8_t *pkt) { return; };

#endif

#endif /* LIBAES128DEC_H_ */
