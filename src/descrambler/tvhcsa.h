/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2013 Adam Sutton
 *
 * tvheadend - CSA wrapper
 */

#ifndef __TVH_CSA_H__
#define __TVH_CSA_H__

struct mpegts_service;
struct elementary_stream;

#include <stdint.h>
#include "build.h"
#if ENABLE_DVBCSA
#include <dvbcsa/dvbcsa.h>
#endif
#include "tvhlog.h"

typedef struct tvhcsa
{

  /**
   * CSA
   */
  int      csa_type;   /*< see DESCRAMBLER_* defines */
  int      csa_keylen;
  void   (*csa_descramble)
              ( struct tvhcsa *csa, struct mpegts_service *s,
                const uint8_t *tsb, int len );
  void   (*csa_flush)
              ( struct tvhcsa *csa, struct mpegts_service *s );

  int      csa_cluster_size;
  uint8_t *csa_tsbcluster;
  int      csa_fill;
  int      csa_fill_size;
  uint8_t  csa_ecm;

#if ENABLE_DVBCSA
  struct dvbcsa_bs_batch_s *csa_tsbbatch_even;
  struct dvbcsa_bs_batch_s *csa_tsbbatch_odd;
  int csa_fill_even;
  int csa_fill_odd;

  struct dvbcsa_bs_key_s *csa_key_even;
  struct dvbcsa_bs_key_s *csa_key_odd;
#endif
  void *csa_priv;
  tvhlog_limit_t tvhcsa_loglimit;

} tvhcsa_t;

#if ENABLE_TVHCSA

int  tvhcsa_set_type( tvhcsa_t *csa, struct mpegts_service *s, int type );

void tvhcsa_set_key_even( tvhcsa_t *csa, const uint8_t *even );
void tvhcsa_set_key_odd ( tvhcsa_t *csa, const uint8_t *odd );

void tvhcsa_init    ( tvhcsa_t *csa );
void tvhcsa_destroy ( tvhcsa_t *csa );

#else

static inline int tvhcsa_set_type( tvhcsa_t *csa, struct mpegts_service *s, int type ) { return -1; }

static inline void tvhcsa_set_key_even( tvhcsa_t *csa, const uint8_t *even ) { };
static inline void tvhcsa_set_key_odd ( tvhcsa_t *csa, const uint8_t *odd ) { };

static inline void tvhcsa_init ( tvhcsa_t *csa ) { };
static inline void tvhcsa_destroy ( tvhcsa_t *csa ) { };

#endif

#if ENABLE_DVBCSA
typedef void* (*dvbcsa_dl_bs_key_set_type)(const unsigned char ecm, const dvbcsa_cw_t cw, struct dvbcsa_bs_key_s *key);
void dvbcsa_bs_key_set_wrap(const unsigned char ecm, const dvbcsa_cw_t cw, struct dvbcsa_bs_key_s *key);
#endif

#endif /* __TVH_CSA_H__ */
