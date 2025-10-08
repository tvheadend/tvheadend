/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * Tvheadend
 */

#ifndef __TVH_CAID_H__
#define __TVH_CAID_H__

/**
 * cards for which emm updates are handled
 */

typedef enum {
  CARD_IRDETO,
  CARD_DRE,
  CARD_CONAX,
  CARD_SECA,
  CARD_VIACCESS,
  CARD_NAGRA,
  CARD_NDS,
  CARD_CRYPTOWORKS,
  CARD_BULCRYPT,
  CARD_STREAMGUARD,
  CARD_GRIFFIN,
  CARD_BETACRYPT,
  CARD_DVN,
  CARD_TONGFANG,
  CARD_UNKNOWN
} card_type_t;

const char *caid2name(uint16_t caid);
uint16_t name2caid(const char *str);

card_type_t detect_card_type(const uint16_t caid);

static inline int caid_is_irdeto(uint16_t caid) { return (caid >> 8) == 0x06; }
static inline int caid_is_videoguard(uint16_t caid) { return (caid >> 8) == 0x09; }
static inline int caid_is_powervu(uint16_t caid) { return (caid >> 8) == 0x0e; }
static inline int caid_is_betacrypt(uint16_t caid) { return (caid >> 8) == 0x17; }
static inline int caid_is_dvn(uint16_t caid) { return caid == 0x4a30; }

#endif /* __TVH_CAID_H__ */
