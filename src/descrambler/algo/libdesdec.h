/*
 * libdesdec.h
 */

#ifndef LIBDESDEC_H_
#define LIBDESDEC_H_

#include <stdint.h>
#include "build.h"

#if ENABLE_SSL

void *des_get_priv_struct(void);
void des_free_priv_struct(void *priv);
void des_set_control_words(void *priv, const uint8_t *even, const uint8_t *odd);
void des_set_even_control_word(void *priv, const uint8_t *even);
void des_set_odd_control_word(void *priv, const uint8_t *odd);
void des_decrypt_packet(void *priv, const uint8_t *pkt);

#else

// empty functions
static inline void *des_get_priv_struct(void)  { return NULL; };
static inline void des_free_priv_struct(void *priv) { return; };
static inline void des_set_control_words(void *priv, const uint8_t *even, const uint8_t *odd) { return; };
static inline void des_set_even_control_word(void *priv, const uint8_t *even) { return; };
static inline void des_set_odd_control_word(void *priv, const uint8_t *odd) { return; };
static inline void des_decrypt_packet(void *priv, const uint8_t *pkt) { return; };

#endif

#endif /* LIBDESDEC_H_ */
