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

#include "tvheadend.h"
#include "descrambler/caid.h"

struct caid_tab {
  const char *name;
  uint16_t caid;
  uint16_t mask;
};

static struct caid_tab caidnametab[] = {
  { "Seca",             0x0100, 0xff00 },
  { "CCETT",            0x0200, 0xff00 },
  { "Deutsche Telekom", 0x0300, 0xff00 },
  { "Eurodec",          0x0400, 0xff00 },
  { "Viaccess",         0x0500, 0xff00 },
  { "Irdeto",           0x0600, 0xff00 },
  { "Jerroldgi",        0x0700, 0xff00 },
  { "Matra",            0x0800, 0xff00 },
  { "NDS",              0x0900, 0xff00 },
  { "Nokia",            0x0A00, 0xff00 },
  { "Conax",            0x0B00, 0xff00 },
  { "NTL",              0x0C00, 0xff00 },
  { "CryptoWorks",      0x0D00, 0xff80 },
  { "CryptoWorks ICE",	0x0D80, 0xff80 },
  { "PowerVu",          0x0E00, 0xff00 },
  { "Sony",             0x0F00, 0xff00 },
  { "Tandberg",         0x1000, 0xff00 },
  { "Thompson",         0x1100, 0xff00 },
  { "TV-Com",           0x1200, 0xff00 },
  { "HPT",              0x1300, 0xff00 },
  { "HRT",              0x1400, 0xff00 },
  { "IBM",              0x1500, 0xff00 },
  { "Nera",             0x1600, 0xff00 },
  { "BetaCrypt",        0x1700, 0xff00 },
  { "NagraVision",      0x1800, 0xff00 },
  { "Titan",            0x1900, 0xff00 },
  { "Telefonica",       0x2000, 0xff00 },
  { "Stentor",          0x2100, 0xff00 },
  { "Tadiran Scopus",   0x2200, 0xff00 },
  { "BARCO AS",         0x2300, 0xff00 },
  { "StarGuide",        0x2400, 0xff00 },
  { "Mentor",           0x2500, 0xff00 },
  { "EBU",              0x2600, 0xff00 },
  { "GI",               0x4700, 0xff00 },
  { "Telemann",         0x4800, 0xff00 },
  { "DGCrypt",          0x4abf, 0xffff },
  { "StreamGuard",      0x4ad2, 0xffff },
  { "DRECrypt",         0x4ae0, 0xffff },
  { "DRECrypt2",        0x4ae1, 0xffff },
  { "Bulcrypt",         0x4aee, 0xffff },
  { "TongFang",         0x4b00, 0xff00 },
  { "Griffin",          0x5500, 0xffe0 },
  { "Bulcrypt",         0x5581, 0xffff },
  { "Verimatrix",       0x5601, 0xffff },
  { "DRECrypt",         0x7be0, 0xffff },
  { "DRECrypt2",        0x7be1, 0xffff },
};

const char *
caid2name(uint16_t caid)
{
  const char *s = NULL;
  static char __thread buf[20];
  struct caid_tab *tab;
  int i;

  for (i = 0; i < ARRAY_SIZE(caidnametab); i++) {
    tab = &caidnametab[i];
    if ((caid & tab->mask) == tab->caid) {
      s = tab->name;
      break;
    }
  }
  if(s != NULL)
    return s;
  snprintf(buf, sizeof(buf), "0x%x", caid);
  return buf;
}

uint16_t
name2caid(const char *s)
{
  int i, r = -1;
  struct caid_tab *tab;

  for (i = 0; i < ARRAY_SIZE(caidnametab); i++) {
    tab = &caidnametab[i];
    if (strcmp(tab->name, s) == 0) {
      r = tab->caid;
      break;
    }
  }

  return (r < 0) ? strtol(s, NULL, 0) : r;
}

/**
 * Detects the cam card type
 * If you want to add another card, have a look at
 * http://www.dvbservices.com/identifiers/ca_system_id?page=3
 *
 * based on the equivalent in sasc-ng
 */
card_type_t
detect_card_type(const uint16_t caid)
{
  uint8_t c_sys = caid >> 8;

  switch(caid) {
    case 0x4ad2:
      return CARD_STREAMGUARD;
    case 0x5581:
    case 0x4aee:
      return CARD_BULCRYPT;
    case 0x5500 ... 0x551a:
      return CARD_GRIFFIN;
  }
  
  switch(c_sys) {
    case 0x17:
    case 0x06:
      return CARD_IRDETO;
    case 0x05:
      return CARD_VIACCESS;
    case 0x0b:
      return CARD_CONAX;
    case 0x01:
      return CARD_SECA;
    case 0x4a:
      return CARD_DRE;
    case 0x18:
      return CARD_NAGRA;
    case 0x09:
      return CARD_NDS;
    case 0x0d:
      return CARD_CRYPTOWORKS;
    default:
      return CARD_UNKNOWN;
  }
}
