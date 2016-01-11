/*
 *  Tvheadend
 *  Copyright (C) 2015 Jaroslav Kysela
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
  CARD_UNKNOWN
} card_type_t;

const char *caid2name(uint16_t caid);
uint16_t name2caid(const char *str);

card_type_t detect_card_type(const uint16_t caid);

#endif /* __TVH_CAID_H__ */
