/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * tvheadend, EMM reassemble functions
 */

#ifndef __TVH_EMM_REASS_H__
#define __TVH_EMM_REASS_H__

#include "tvheadend.h"
#include "descrambler/caid.h"

typedef struct emm_provider emm_provider_t;
typedef struct emm_reass emm_reass_t;

struct emm_provider {
  uint32_t id;
  uint8_t sa[8];
  union {
    struct {
      void       *shared_mux;
      uint8_t    *shared_emm;
      int         shared_len;
    } viacess[2];
  } u;
};

#define EMM_CACHE_SIZE (1<<5)
#define EMM_CACHE_MASK (EMM_CACHE_SIZE-1)

typedef void (*emm_send_t)
  (void *aux, const uint8_t *radata, int ralen, void *mux);

struct emm_reass {
  int subsys;

  uint16_t caid;
  card_type_t type;

  int providers_count;
  emm_provider_t *providers;
  uint8_t ua[8];

  int emm_cache_write;
  int emm_cache_count;
  uint32_t emm_cache[EMM_CACHE_SIZE];

  union {
    struct {
      void       *shared_mux;
      uint8_t    *shared_emm;
      int         shared_len;
    } cryptoworks;
  } u;

  void (*do_emm)(emm_reass_t *ra, const uint8_t *data, int len,
                 void *mux, emm_send_t send, void *aux);
};

void emm_filter(emm_reass_t *ra, const uint8_t *data, int len,
                void *mux, emm_send_t send, void *aux);
emm_provider_t *emm_find_provider(emm_reass_t *ra, uint32_t provid);
void emm_reass_init(emm_reass_t *ra, int subsys, uint16_t caid);
void emm_reass_done(emm_reass_t *ra);

#endif /* __TVH_EMM_REASS_H__ */
