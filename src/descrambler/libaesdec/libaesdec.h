/*
 * libaesdec.h
 *
 *  Created on: Jun 22, 2014
 *      Author: root
 */

#ifndef LIBAESDEC_H_
#define LIBAESDEC_H_

void libaes_init(void);
int libaes_get_internal_parallelism(void);
int libaes_get_suggested_cluster_size(void);
void * libaes_get_key_struct(void);
void libaes_free_key_struct(void *keys);
void libaes_set_even_control_word(void *keys, const unsigned char *even);
void libaes_set_odd_control_word(void *keys, const unsigned char *odd);
int libaes_decrypt_packets(void *keys, unsigned char **cluster);

// -- alloc & free the key structure
void *aes_get_key_struct(void);
void aes_free_key_struct(void *keys);

// -- set aes keys, 16 bytes each
void aes_set_control_words(void *keys, const unsigned char *even, const unsigned char *odd);

// -- set even aes key, 16 bytes
void aes_set_even_control_word(void *keys, const unsigned char *even);

// -- set odd aes key, 16 bytes
void aes_set_odd_control_word(void *keys, const unsigned char *odd);

// -- get aes keys, 16 bytes each
//void get_control_words(void *keys, unsigned char *even, unsigned char *odd);

// -- decrypt TS packet
void aes_decrypt_packet(void *keys, unsigned char *packet);


#endif /* LIBAESDEC_H_ */
