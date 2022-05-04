/*
 * libaesdec.h
 *
 *  Created on: Jun 22, 2014
 *      Author: spdfrk1
 */

#ifndef LIBAESDEC_H_
#define LIBAESDEC_H_

#include <stdint.h>
#include "build.h"

#if ENABLE_SSL

void *aes_get_priv_struct(void);
void aes_free_priv_struct(void *keys);
void aes_set_control_words(void *keys, const uint8_t *even, const uint8_t *odd);
void aes_set_even_control_word(void *keys, const uint8_t *even);
void aes_set_odd_control_word(void *keys, const uint8_t *odd);
void aes_decrypt_packet(void *keys, const uint8_t *pkt);

#else

// empty functions
static inline void *aes_get_priv_struct(void)  { return NULL; };
static inline void aes_free_priv_struct(void *keys) { return; };
static inline void aes_set_control_words(void *keys, const uint8_t *even, const uint8_t *odd) { return; };
static inline void aes_set_even_control_word(void *keys, const uint8_t *even) { return; };
static inline void aes_set_odd_control_word(void *keys, const uint8_t *odd) { return; };
static inline void aes_decrypt_packet(void *keys, const uint8_t *pkt) { return; };

#endif

#endif /* LIBAESDEC_H_ */
