/*
 *  Teletext parsing functions
 *  Copyright (C) 2007 Andreas Ã–man
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>

#include "tvheadend.h"
#include "packet.h"
#include "streaming.h"
#include "service.h"
#include "input.h"
#include "parser_teletext.h"

/**
 *
 */
typedef struct tt_mag {
  int ttm_curpage;
  int64_t ttm_current_pts;
  uint8_t ttm_charset[2];
  uint8_t ttm_national;
  uint8_t ttm_page[23*40 + 1];
} tt_mag_t;


/**
 *
 */
typedef struct tt_private {
  tt_mag_t ttp_mags[8];

  int ttp_rundown_valid;
  uint8_t ttp_rundown[23*40 + 1];

} tt_private_t;


static void teletext_rundown_copy(tt_private_t *ttp, tt_mag_t *ttm);

static void teletext_rundown_scan(mpegts_service_t *t, tt_private_t *ttp);

#define bitreverse(b) \
(((b) * 0x0202020202ULL & 0x010884422010ULL) % 1023)

static const uint8_t hamtable[] = {
  0x01, 0xff, 0x81, 0x01, 0xff, 0x00, 0x01, 0xff, 
  0xff, 0x02, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x07, 
  0xff, 0x00, 0x01, 0xff, 0x00, 0x80, 0xff, 0x00, 
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x03, 0xff, 
  0xff, 0x0c, 0x01, 0xff, 0x04, 0xff, 0xff, 0x07, 
  0x06, 0xff, 0xff, 0x07, 0xff, 0x07, 0x07, 0x87, 
  0x06, 0xff, 0xff, 0x05, 0xff, 0x00, 0x0d, 0xff, 
  0x86, 0x06, 0x06, 0xff, 0x06, 0xff, 0xff, 0x07, 
  0xff, 0x02, 0x01, 0xff, 0x04, 0xff, 0xff, 0x09, 
  0x02, 0x82, 0xff, 0x02, 0xff, 0x02, 0x03, 0xff, 
  0x08, 0xff, 0xff, 0x05, 0xff, 0x00, 0x03, 0xff, 
  0xff, 0x02, 0x03, 0xff, 0x03, 0xff, 0x83, 0x03, 
  0x04, 0xff, 0xff, 0x05, 0x84, 0x04, 0x04, 0xff, 
  0xff, 0x02, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x07, 
  0xff, 0x05, 0x05, 0x85, 0x04, 0xff, 0xff, 0x05, 
  0x06, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x03, 0xff, 
  0xff, 0x0c, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x09, 
  0x0a, 0xff, 0xff, 0x0b, 0x8a, 0x0a, 0x0a, 0xff, 
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x0d, 0xff, 
  0xff, 0x0b, 0x0b, 0x8b, 0x0a, 0xff, 0xff, 0x0b, 
  0x0c, 0x8c, 0xff, 0x0c, 0xff, 0x0c, 0x0d, 0xff, 
  0xff, 0x0c, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x07, 
  0xff, 0x0c, 0x0d, 0xff, 0x0d, 0xff, 0x8d, 0x0d, 
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x0d, 0xff, 
  0x08, 0xff, 0xff, 0x09, 0xff, 0x09, 0x09, 0x89, 
  0xff, 0x02, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x09, 
  0x88, 0x08, 0x08, 0xff, 0x08, 0xff, 0xff, 0x09, 
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x03, 0xff, 
  0xff, 0x0c, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x09, 
  0x0f, 0xff, 0x8f, 0x0f, 0xff, 0x0e, 0x0f, 0xff, 
  0x08, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x0d, 0xff, 
  0xff, 0x0e, 0x0f, 0xff, 0x0e, 0x8e, 0xff, 0x0e, 
};

/*
 * Teletext character set according to ETS 300 706, Section 15.
 */
#define CHARSET_NONE             0
#define CHARSET_LATIN_G0         1
#define CHARSET_LATIN_G2         2
#define CHARSET_CYRILLIC_1_G0    3
#define CHARSET_CYRILLIC_2_G0    4
#define CHARSET_CYRILLIC_3_G0    5
#define CHARSET_CYRILLIC_G2      6
#define CHARSET_GREEK_G0         7
#define CHARSET_GREEK_G2         8
#define CHARSET_ARABIC_G0        9
#define CHARSET_ARABIC_G2        10
#define CHARSET_HEBREW_G0        11
#define CHARSET_BLOCK_MOSAIC_G1  12
#define CHARSET_SMOOTH_MOSAIC_G3 13
#define CHARSET_LAST             CHARSET_SMOOTH_MOSAIC_G3

/*
 * Teletext Latin G0 national option subsets according to
 * ETS 300 706, Section 15.2; Section 15.6.2 Table 36.
 */
#define SUBSET_NONE            0
#define SUBSET_CZECH_SLOVAK    1  /* Cesky / Slovencina */
#define SUBSET_ENGLISH         2  /* English */
#define SUBSET_ESTONIAN        3  /* Eesti */
#define SUBSET_FRENCH          4  /* Français */
#define SUBSET_GERMAN          5  /* German / Deutch */
#define SUBSET_ITALIAN         6  /* Italiano */
#define SUBSET_LETT_LITH       7  /* Lettish / Lietuviskai */
#define SUBSET_POLISH          8  /* Polski */
#define SUBSET_PORTUG_SPANISH  9  /* Português / Español */
#define SUBSET_RUMANIAN        10 /* Româna */
#define SUBSET_SERB_CRO_SLO    11 /* Srpski / Hrvatski / Slovenscina */
#define SUBSET_SWE_FIN_HUN     12 /* Svenska / Suomi / Magyar */
#define SUBSET_TURKISH         13 /* Türkçe */
#define SUBSET_LAST            SUBSET_TURKISH

#define SUBSET_CHARMAP_COUNT   13

/*
 *  Teletext font descriptors
 *
 *  ETS 300 706 Table 32, 33, 34
 *
 *  Note the fourth byte is unused (it's just a pad)
 */
uint8_t char_table[88][4] = {
  /* Western and Central Europe */
  /* 00 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ENGLISH        },
  /* 01 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_GERMAN         },
  /* 02 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_SWE_FIN_HUN    },
  /* 03 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ITALIAN        },
  /* 04 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_FRENCH         },
  /* 05 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_PORTUG_SPANISH },
  /* 06 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_CZECH_SLOVAK   },
  /* 07 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },

  /* Eastern Europe */
  /* 08 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_POLISH         },
  /* 09 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_GERMAN         },
  /* 10 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_SWE_FIN_HUN    },
  /* 11 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ITALIAN        },
  /* 12 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_FRENCH         },
  /* 13 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 14 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_CZECH_SLOVAK   },
  /* 15 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },

  /* Western Europe and Turkey */
  /* 16 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ENGLISH        },
  /* 17 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_GERMAN         },
  /* 18 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_SWE_FIN_HUN    },
  /* 19 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ITALIAN        },
  /* 20 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_FRENCH         },
  /* 21 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_PORTUG_SPANISH },
  /* 22 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_TURKISH        },
  /* 23 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },

  /* Central and Southeast Europe */
  /* 24 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 25 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 26 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 27 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 28 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 29 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_SERB_CRO_SLO   },
  /* 30 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_NONE           },
  /* 31 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_RUMANIAN       },

  /* Cyrillic */
  /* 32 */ { CHARSET_CYRILLIC_1_G0, CHARSET_CYRILLIC_G2, SUBSET_NONE   },
  /* 33 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_GERMAN         },
  /* 34 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_ESTONIAN       },
  /* 35 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_LETT_LITH      },
  /* 36 */ { CHARSET_CYRILLIC_2_G0, CHARSET_CYRILLIC_G2, SUBSET_NONE   },
  /* 37 */ { CHARSET_CYRILLIC_3_G0, CHARSET_CYRILLIC_G2, SUBSET_NONE   },
  /* 38 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_CZECH_SLOVAK   },
  /* 39 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },

  /* Unused */
  /* 40 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 41 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 42 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 43 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 44 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 45 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 46 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 47 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },

  /* Misc */
  /* 48 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 49 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 50 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 51 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 52 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 53 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 54 */ { CHARSET_LATIN_G0, CHARSET_LATIN_G2, SUBSET_TURKISH        },
  /* 55 */ { CHARSET_GREEK_G0, CHARSET_GREEK_G2, SUBSET_NONE           },

  /* Unused */
  /* 56 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 57 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 58 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 59 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 60 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 61 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 62 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 63 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },

  /* Arabic */
  /* 64 */ { CHARSET_LATIN_G0, CHARSET_ARABIC_G2, SUBSET_ENGLISH       },
  /* 65 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 66 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 67 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 68 */ { CHARSET_LATIN_G0, CHARSET_ARABIC_G2, SUBSET_FRENCH        },
  /* 69 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 70 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 71 */ { CHARSET_ARABIC_G0, CHARSET_ARABIC_G2, SUBSET_NONE         },

  /* Unused */
  /* 72 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 73 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 74 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 75 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 76 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 77 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 78 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 79 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },

  /* Misc */
  /* 80 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 81 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 82 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 83 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 84 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 85 */ { CHARSET_HEBREW_G0, CHARSET_ARABIC_G2, SUBSET_NONE         },
  /* 86 */ { CHARSET_NONE, CHARSET_NONE, SUBSET_NONE                   },
  /* 87 */ { CHARSET_ARABIC_G0, CHARSET_ARABIC_G2, SUBSET_NONE         },
};

/*
 *  Teletext character set
 *
 *  (Similar to ISO 6937 -
 *   ftp://dkuug.dk/i18n/charmaps/ISO_6937)
 */

/*
 *  ETS 300 706 Table 36: Latin National Option Sub-sets
 *
 *  Latin G0 character code to Unicode mapping per national subset,
 *  unmodified codes (SUBSET_NONE) in row zero.
 *
 *  [13][0] Turkish currency symbol not in Unicode, use '$'.
 */
static const uint16_t
teletext_subset[SUBSET_LAST+1][SUBSET_CHARMAP_COUNT] = {
  { 0x0023, 0x0024, 0x0040, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x007B, 0x007C, 0x007D, 0x007E },
  { 0x0023, 0x016F, 0x010D, 0x0165, 0x017E, 0x00FD, 0x00ED, 0x0159,
    0x00E9, 0x00E1, 0x011B, 0x00FA, 0x0161 },
  { 0x00A3, 0x0024, 0x0040, 0x2190, 0x00BD, 0x2192, 0x2191, 0x0023,
    0x2014, 0x00BC, 0x2016, 0x00BE, 0x00F7 },
  { 0x0023, 0x00F5, 0x0160, 0x00C4, 0x00D6, 0x017D, 0x00DC, 0x00D5,
    0x0161, 0x00E4, 0x00F6, 0x017E, 0x00FC },
  { 0x00E9, 0x00EF, 0x00E0, 0x00EB, 0x00EA, 0x00F9, 0x00EE, 0x0023,
    0x00E8, 0x00E2, 0x00F4, 0x00FB, 0x00E7 },
  { 0x0023, 0x0024, 0x00A7, 0x00C4, 0x00D6, 0x00DC, 0x005E, 0x005F,
    0x00B0, 0x00E4, 0x00F6, 0x00FC, 0x00DF },
  { 0x00A3, 0x0024, 0x00E9, 0x00B0, 0x00E7, 0x2192, 0x2191, 0x0023,
    0x00F9, 0x00E0, 0x00F2, 0x00E8, 0x00EC },
  { 0x0023, 0x0024, 0x0160, 0x0117, 0x0229, 0x017D, 0x010D, 0x016B,
    0x0161, 0x0105, 0x0173, 0x017E, 0x012F },
  { 0x0023, 0x0144, 0x0105, 0x01B5, 0x015A, 0x0141, 0x0107, 0x00F3,
    0x0119, 0x017C, 0x015B, 0x0142, 0x017A },
  { 0x00E7, 0x0024, 0x00A1, 0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA,
    0x00BF, 0x00FC, 0x00F1, 0x00E8, 0x00E0 },
  { 0x0023, 0x00A4, 0x0162, 0x00C2, 0x015E, 0x01CD, 0x00CD, 0x0131,
    0x0163, 0x00E2, 0x015F, 0X01CE, 0x00EE },
  { 0x0023, 0x00CB, 0x010C, 0x0106, 0x017D, 0x00D0, 0x0160, 0x00EB,
    0x010D, 0x0107, 0x017E, 0x00F0, 0x0161 },
  { 0x0023, 0x00A4, 0x00C9, 0x00C4, 0x00D6, 0x00C5, 0x00DC, 0x005F,
    0x00E9, 0x00E4, 0x00F6, 0x00E5, 0x00FC },
  { 0x0024, 0x011F, 0x0130, 0x015E, 0x00D6, 0x00C7, 0x00DC, 0x011E,
    0x0131, 0x015F, 0x00F6, 0x00E7, 0x00FC }
};

/*
 *  ETS 300 706 Table 37: Latin G2 Supplementary Set
 *
 *  0x49 seems to be dot below; not in Unicode (except combining), use U+002E.
 */
static const uint16_t
latin_g2[96] = {
  /* 0x20 */ 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x0024, 0x00A5, 0x0023, 0x00A7,
  /* 0x28 */ 0x00A4, 0x2018, 0x201C, 0x00AB, 0x2190, 0x2191, 0x2192, 0x2193,
  /* 0x30 */ 0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00D7, 0x00B5, 0x00B6, 0x00B7,
  /* 0x38 */ 0x00F7, 0x2019, 0x201D, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
  /* 0x40 */ 0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
  /* 0x48 */ 0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
  /* 0x50 */ 0x2014, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
  /* 0x58 */ 0x0251, 0x0020, 0x0020, 0x0020, 0x215B, 0x215C, 0x215D, 0x215E,
  /* 0x60 */ 0x2126, 0x00C6, 0x00D0, 0x00AA, 0x0126, 0x0020, 0x0132, 0x013F,
  /* 0x68 */ 0x0141, 0x00D8, 0x0152, 0x00BA, 0x00DE, 0x0166, 0x014A, 0x0149,
  /* 0x70 */ 0x0138, 0x00E6, 0x0111, 0x00F0, 0x0127, 0x0131, 0x0133, 0x0140,
  /* 0x78 */ 0x0142, 0x00F8, 0x0153, 0x00DF, 0x00FE, 0x0167, 0x014B, 0x25A0
};

/*
 *  ETS 300 706 Table 38: Cyrillic G0 Primary Set - Option 1 - Serbian/Croatian
 */
static const uint16_t
cyrillic_1_g0[64] = {
  /* 0x40 */ 0x0427, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
  /* 0x48 */ 0x0425, 0x0418, 0x0408, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
  /* 0x50 */ 0x041F, 0x040C, 0x0420, 0x0421, 0x0422, 0x0423, 0x0412, 0x0403,
  /* 0x58 */ 0x0409, 0x040A, 0x0417, 0x040B, 0x0416, 0x0402, 0x0428, 0x040F,
  /* 0x60 */ 0x0447, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
  /* 0x68 */ 0x0445, 0x0438, 0x0458, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
  /* 0x70 */ 0x043F, 0x045C, 0x0440, 0x0441, 0x0442, 0x0443, 0x0432, 0x0453,
  /* 0x78 */ 0x0459, 0x045A, 0x0437, 0x045B, 0x0436, 0x0452, 0x0448, 0x25A0
};

/*
 *  ETS 300 706 Table 39: Cyrillic G0 Primary Set - Option 2 - Russian/Bulgarian
 */
static const uint16_t
cyrillic_2_g0[64] = {
  /* 0x40 */ 0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
  /* 0x48 */ 0x0425, 0x0418, 0x040D, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
  /* 0x50 */ 0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
  /* 0x58 */ 0x042C, 0x042A, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042B,
  /* 0x60 */ 0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
  /* 0x68 */ 0x0445, 0x0438, 0x045D, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
  /* 0x70 */ 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
  /* 0x78 */ 0x044C, 0x044A, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x25A0
};

/*
 *  ETS 300 706 Table 40: Cyrillic G0 Primary Set - Option 3 - Ukrainian
 */
static const uint16_t
cyrillic_3_g0[64] = {
  /* 0x40 */ 0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
  /* 0x48 */ 0x0425, 0x0418, 0x040D, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
  /* 0x50 */ 0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
  /* 0x58 */ 0x042C, 0x0406, 0x0417, 0x0428, 0x0404, 0x0429, 0x0427, 0x0407,
  /* 0x60 */ 0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
  /* 0x68 */ 0x0445, 0x0438, 0x045D, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
  /* 0x70 */ 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
  /* 0x78 */ 0x044C, 0x0456, 0x0437, 0x0448, 0x0454, 0x0449, 0x0447, 0x25A0
};

/*
 *  ETS 300 706 Table 41: Cyrillic G2 Supplementary Set
 */
static const uint16_t
cyrillic_g2[96] = {
  /* 0x20 */ 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x0020, 0x00A5, 0x0023, 0x00A7,
  /* 0x28 */ 0x0020, 0x2018, 0x201C, 0x00AB, 0x2190, 0x2191, 0x2192, 0x2193,
  /* 0x30 */ 0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00D7, 0x00B5, 0x00B6, 0x00B7,
  /* 0x38 */ 0x00F7, 0x2019, 0x201D, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
  /* 0x40 */ 0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
  /* 0x48 */ 0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
  /* 0x50 */ 0x2014, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
  /* 0x58 */ 0x0251, 0x0141, 0x0142, 0x00DF, 0x215B, 0x215C, 0x215D, 0x215E,
  /* 0x60 */ 0x0044, 0x0045, 0x0046, 0x0047, 0x0049, 0x004A, 0x004B, 0x004C,
  /* 0x68 */ 0x004E, 0x0051, 0x0052, 0x0053, 0x0055, 0x0056, 0x0057, 0x005A,
  /* 0x70 */ 0x0064, 0x0065, 0x0066, 0x0067, 0x0069, 0x006A, 0x006B, 0x006C,
  /* 0x78 */ 0x006E, 0x0071, 0x0072, 0x0073, 0x0075, 0x0076, 0x0077, 0x007A
};

/*
 *  ETS 300 706 Table 42: Greek G0 Primary Set
 */
static const uint16_t
greek_g0[64] = {
  /* 0x40 */ 0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
  /* 0x48 */ 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
  /* 0x50 */ 0x03A0, 0x03A1, 0x0374, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7,
  /* 0x58 */ 0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
  /* 0x60 */ 0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
  /* 0x68 */ 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
  /* 0x70 */ 0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
  /* 0x78 */ 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0x25A0
};

/*
 *  ETS 300 706 Table 43: Greek G2 Supplementary Set
 */
static const uint16_t
greek_g2[96] = {
  /* 0x20 */ 0x00A0, 0x0061, 0x0062, 0x00A3, 0x0065, 0x0068, 0x0069, 0x00A7,
  /* 0x28 */ 0x003A, 0x2018, 0x201C, 0x006B, 0x2190, 0x2191, 0x2192, 0x2193,
  /* 0x30 */ 0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0078, 0x006D, 0x006E, 0x0070,
  /* 0x38 */ 0x00F7, 0x2019, 0x201D, 0x0074, 0x00BC, 0x00BD, 0x00BE, 0x0078,
  /* 0x40 */ 0x0020, 0x02CB, 0x02CA, 0x02C6, 0x02DC, 0x02C9, 0x02D8, 0x02D9,
  /* 0x48 */ 0x00A8, 0x002E, 0x02DA, 0x02CF, 0x02CD, 0x02DD, 0x02DB, 0x02C7,
  /* 0x50 */ 0x003F, 0x00B9, 0x00AE, 0x00A9, 0x2122, 0x266A, 0x20A0, 0x2030,
  /* 0x58 */ 0x0251, 0x038A, 0x038E, 0x038F, 0x215B, 0x215C, 0x215D, 0x215E,
  /* 0x60 */ 0x0043, 0x0044, 0x0046, 0x0047, 0x004A, 0x004C, 0x0051, 0x0052,
  /* 0x68 */ 0x0053, 0x0055, 0x0056, 0x0057, 0x0059, 0x005A, 0x0386, 0x0389,
  /* 0x70 */ 0x0063, 0x0064, 0x0066, 0x0067, 0x006A, 0x006C, 0x0071, 0x0072,
  /* 0x78 */ 0x0073, 0x0075, 0x0076, 0x0077, 0x0079, 0x007A, 0x0388, 0x25A0
};

/*
 *  ETS 300 706 Table 44: Arabic G0 Primary Set
 */
static const uint16_t
arabic_g0[96] = {
  /* 0x20 */ 0x0020, 0x0021, 0x0022, 0x00A3, 0x0024, 0x0025, 0x0000, 0x0000,
  /* 0x28 */ 0x0029, 0x0028, 0x002A, 0x002B, 0x060C, 0x002D, 0x002E, 0x002F,
  /* 0x30 */ 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
  /* 0x38 */ 0x0038, 0x0039, 0x003A, 0x061B, 0x003E, 0x003D, 0x003C, 0x061F,
  /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x50 */ 0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
  /* 0x58 */ 0x0638, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0023,
  /* 0x60 */ 0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
  /* 0x68 */ 0x0648, 0x0649, 0x064A, 0x062B, 0x062D, 0x062C, 0x062E, 0x0000,
  /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x25A0
};

/*
 *  ETS 300 706 Table 45: Arabic G2 Supplementary Set
 */
static const uint16_t
arabic_g2[96] = {
  /* 0x20 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x28 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x30 */ 0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
  /* 0x38 */ 0x0668, 0x0669, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x40 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x48 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x50 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x58 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x60 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x68 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x70 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  /* 0x78 */ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/*
 *  ETS 300 706 Table 46: Hebrew G0 Primary Set
 */
static const uint16_t
hebrew_g0[37] = {
  /* 0x5b */                         0x2190, 0x00BD, 0x2192, 0x2191, 0x0023,
  /* 0x60 */ 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7,
  /* 0x68 */ 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
  /* 0x70 */ 0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7,
  /* 0x78 */ 0x05E8, 0x05E9, 0x05EA, 0x20AA, 0x2016, 0x00BE, 0x00F7, 0x25A0
};

static const uint8_t* COLOR_MAP[8] = {
  // black (grey for visibility), red, green, yellow, blue, magenta, cyan, white
  (uint8_t*) "<font color=\"#888888\">",
  (uint8_t*) "<font color=\"#ff0000\">",
  (uint8_t*) "<font color=\"#00ff00\">",
  (uint8_t*) "<font color=\"#ffff00\">",
  (uint8_t*) "<font color=\"#0000ff\">",
  (uint8_t*) "<font color=\"#ff00ff\">",
  (uint8_t*) "<font color=\"#00ffff\">",
  (uint8_t*) "<font color=\"#ffffff\">"
};
static const uint8_t* CLOSE_FONT = (uint8_t*) "</font>";

/**
 * cs Teletext character set as listed in ETS 300 706 section 15.
 * ss National character subset as listed in section 15, only
 *     applicable to character set CHARSET_LATIN_G0, ignored otherwise.
 * c  Character code in range 0x20 ... 0x7F.
 *
 * Translate Teletext character code to Unicode.
 */
static uint16_t
teletext_to_ucs2(uint8_t cs, uint8_t ss, uint8_t c)
{
  int i;

  if (c < 0x20 || c > 0x7f)
    return 0;

  switch (cs) {
  case CHARSET_LATIN_G0:
    /* shortcut */
    if (0xF8000019UL & (1 << (c & 31))) {
      if (ss > 0) {
        assert(ss < 14);
        for (i = 0; i < SUBSET_CHARMAP_COUNT; i++)
          if (c == teletext_subset[0][i])
            return teletext_subset[ss][i];
      }
      if (c == 0x24)
        return 0x00A4u;
      else if (c == 0x7C)
        return 0x00A6u;
      else if (c == 0x7F)
        return 0x25A0u;
    }
    return c;

  case CHARSET_LATIN_G2:
    return latin_g2[c - 0x20];

  case CHARSET_CYRILLIC_1_G0:
    return c < 0x40 ? c : cyrillic_1_g0[c - 0x40];

  case CHARSET_CYRILLIC_2_G0:
    if (c == 0x26)
      return 0x044Bu;
    else if (c < 0x40)
      return c;
    else
      return cyrillic_2_g0[c - 0x40];

  case CHARSET_CYRILLIC_3_G0:
    if (c == 0x26)
      return 0x00EFu;
    else if (c < 0x40)
      return c;
    else
      return cyrillic_3_g0[c - 0x40];

  case CHARSET_CYRILLIC_G2:
    return cyrillic_g2[c - 0x20];

  case CHARSET_GREEK_G0:
    if (c == 0x3C)
      return 0x00ABu;
    else if (c == 0x3E)
      return 0x00BBu;
    else if (c < 0x40)
      return c;
    else
      return greek_g0[c - 0x40];

  case CHARSET_GREEK_G2:
    return greek_g2[c - 0x20];

  case CHARSET_ARABIC_G0:
    return arabic_g0[c - 0x20];

  case CHARSET_ARABIC_G2:
    return arabic_g2[c - 0x20];

  case CHARSET_HEBREW_G0:
    return c < 0x5b ? c : hebrew_g0[c - 0x5b];

  case CHARSET_BLOCK_MOSAIC_G1:
    return 0;

  case CHARSET_SMOOTH_MOSAIC_G3:
    return 0;

  default:
    return 0;
  }
}

static inline uint8_t
ham_decode8(uint8_t a)
{
  return hamtable[a] & 0xf;
}

static uint8_t
ham_decode16(uint8_t *p)
{
  return (ham_decode8(p[1]) << 4) | ham_decode8(p[0]);
}

static uint32_t
ham_decode24(uint8_t *p)
{
  /* note: no parity checking / correction */
  uint32_t r = ((p[0] >> 2) & 1) | ((p[0] & 0xf0) >> 3);
  r |= (((uint32_t)p[1]) & 0x7f) << 5;
  r |= (((uint32_t)p[2]) & 0x7f) << 13;
  return r;
}

/**
 *
 */
static time_t
tt_construct_unix_time(uint8_t *buf)
{
  time_t t, r[3], v[3];
  int i;
  struct tm tm;

  t = dispatch_clock;
  localtime_r(&t, &tm);

  tm.tm_hour = atoi((char *)buf);
  tm.tm_min = atoi((char *)buf + 3);
  tm.tm_sec = atoi((char *)buf + 6);

  r[0] = mktime(&tm);
  tm.tm_mday--;
  r[1] = mktime(&tm);
  tm.tm_mday += 2;
  r[2] = mktime(&tm);

  for(i = 0; i < 3; i++)
    v[i] = labs(r[i] - t);
  
  if(v[0] < v[1] && v[0] < v[2])
    return r[0];

  if(v[1] < v[2] && v[1] < v[0])
    return r[1];

  return r[2];
}


/**
 *
 */
static int
is_tt_clock(const uint8_t *str)
{
  return 
    isdigit(str[0]) && isdigit(str[1]) && str[2] == ':' &&
    isdigit(str[3]) && isdigit(str[4]) && str[5] == ':' &&
    isdigit(str[6]) && isdigit(str[7]);
}


/**
 *
 */
static int
update_tt_clock(mpegts_service_t *t, const uint8_t *buf)
{
  uint8_t str[10];
  int i;
  time_t ti;

  for(i = 0; i < 8; i++)
    str[i] = buf[i] & 0x7f;
  str[8] = 0;

  if(!is_tt_clock(str))
    return 0;

  ti = tt_construct_unix_time(str);
  if(t->s_tt_clock == ti)
    return 0;

  t->s_tt_clock = ti;
  //  printf("teletext clock is: %s", ctime(&ti));
  return 1;
}

static uint8_t *
get_cset(uint8_t off)
{
  if (off >= ARRAY_SIZE(char_table))
    return NULL;
  if (char_table[off][0] == CHARSET_NONE)
    return NULL;
  return char_table[off];
}

static void
extract_subtitle(mpegts_service_t *t, elementary_stream_t *st,
		 tt_mag_t *ttm, int64_t pts)
{
  int i, j, start, off = 0;
  int k, current_color, is_font_tag_open, use_color_subs = 1;
  uint8_t sub[2000];
  uint8_t *cset, ch;
  int is_box = 0;

  if ((cset = get_cset((ttm->ttm_charset[0] & ~7) + ttm->ttm_national)) == NULL)
    if ((cset = get_cset(ttm->ttm_charset[0])) == NULL)
      cset = char_table[0]; /* fallback */

  for (i = 0; i < 23; i++) {
    is_font_tag_open = 0;
    current_color = 7; /* white = default */
    start = off;

    for (j = 0; j < 40; j++) {
      ch = ttm->ttm_page[40 * i + j];

      switch(ch) {
      case 0 ... 7:
        if (use_color_subs) {
          if (ch != current_color) {
            if (is_font_tag_open) {
              for (k = 0; k < 7; k++) {
                if (off < sizeof(sub))
                  sub[off++] = CLOSE_FONT[k];
              }
              is_font_tag_open = 0;
            }
            /* no need for font-tag for default-color */
            if (ch != 7) {
              for (k = 0; k < 22; k++) {
                if (off < sizeof(sub))
                  sub[off++] = COLOR_MAP[ch][k];
              }
              is_font_tag_open = 1;
            }
            current_color = ch;
          }
        }
	break;

      case 0x0a:
	is_box = 0;
	break;

      case 0x0b:
	is_box = 1;
	break;

      default:
	if (ch >= 0x20 && is_box && (start != off || ch > 0x20)) {

	  uint16_t ucs2 = teletext_to_ucs2(cset[0], cset[2], ch);

          /* ucs2 -> utf8 conversion */
          if (ucs2 == 0) {
            /* nothing */
          } else if (ucs2 < 0x80) {
            if (off < sizeof(sub))
              sub[off++] = ucs2;
          } else if (ucs2 < 0x800) {
            if (off + 1 < sizeof(sub)) {
              sub[off++] = (ucs2 >> 6)   | 0xc0;
              sub[off++] = (ucs2 & 0x3f) | 0x80;
            }
          } else {
            assert(ucs2 < 0xd800 || ucs2 >= 0xe000);
            if (off + 2 < sizeof(sub)) {
              sub[off++] =  (ucs2 >> 12)        | 0xe0;
              sub[off++] = ((ucs2 >> 6) & 0x3f) | 0x80;
              sub[off++] =  (ucs2       & 0x3f) | 0x80;
            }
          }
	}
      }
    }
    if (use_color_subs && is_font_tag_open) {
      for (k = 0; k < 7; k++) {
        if (off < sizeof(sub))
          sub[off++] = CLOSE_FONT[k];
      }
      is_font_tag_open = 0;
    }
    if(start != off && off < sizeof(sub))
      sub[off++] = '\n';
  }

  if(off == 0 && st->es_blank)
    return; // Avoid multiple blank subtitles

  st->es_blank = !off;

  if(st->es_curpts == pts)
    pts++; // Avoid non-monotonic PTS

  st->es_curpts = pts;

  sub[off++] = 0;
  
  th_pkt_t *pkt = pkt_alloc(sub, off, pts, pts);
  pkt->pkt_componentindex = st->es_index;

  streaming_pad_deliver(&t->s_streaming_pad, streaming_msg_create_pkt(pkt));

  /* Decrease our own reference to the packet */
  pkt_ref_dec(pkt);
}

/**
 *
 */
#if 0
static void
dump_page(tt_mag_t *ttm)
{
  int i, j, v;
  char buf[41];
  
  printf("------------------------------------------------\n");
  printf("------------------------------------------------\n");
  for(i = 0; i < 23; i++) {

    for(j = 0; j < 40; j++) {
      v = ttm->ttm_page[40 * i + j];
      v &= 0x7f;
      if(v < 32)
	v = ' ';
      buf[j] = v;
    }
    buf[j] = 0;
    printf("%s   | %x %x %x\n", buf,
	   ttm->ttm_page[40 * i + 0],
	   ttm->ttm_page[40 * i + 1],
	   ttm->ttm_page[40 * i + 2]);
  }
}
#endif


static void
tt_subtitle_deliver(mpegts_service_t *t, elementary_stream_t *parent, tt_mag_t *ttm)
{
  elementary_stream_t *st;

  if(ttm->ttm_current_pts == PTS_UNSET)
    return;

  TAILQ_FOREACH(st, &t->s_components, es_link) {
     if(parent->es_pid == st->es_parent_pid &&
	ttm->ttm_curpage == st->es_pid -  PID_TELETEXT_BASE) {
       extract_subtitle(t, st, ttm, ttm->ttm_current_pts);
     }
  }
}

/**
 *
 */
static void
tt_decode_line(mpegts_service_t *t, elementary_stream_t *st, uint8_t *buf)
{
  uint8_t mpag, line, s12, c;
  int page, magidx, i;
  tt_mag_t *ttm;
  tt_private_t *ttp;
  uint32_t trip1, trip2;

  if(st->es_priv == NULL) {
    /* Allocate privdata for reassembly */
    ttp = st->es_priv = calloc(1, sizeof(tt_private_t));
  } else {
    ttp = st->es_priv;
  }

  mpag   = ham_decode16(buf);
  magidx = mpag & 7;
  ttm    = &ttp->ttp_mags[magidx];

  line = mpag >> 3;

  switch(line) {
  case 0:
    if(ttm->ttm_curpage != 0) {

      tt_subtitle_deliver(t, st, ttm);

      if(ttm->ttm_curpage == 192)
	teletext_rundown_copy(ttp, ttm);

      memset(ttm->ttm_page, ' ', 23 * 40);
      ttm->ttm_curpage = 0;
    }

    if((page = ham_decode16(buf + 2)) == 0xff)
      return;

    /* The page is BDC encoded, mag 0 is displayed as page 800+ */
    page = (magidx ?: 8) * 100 + (page >> 4) * 10 + (page & 0xf);

    ttm->ttm_curpage = page;

    s12 = ham_decode16(buf + 4);
    c   = ham_decode16(buf + 8);

    /* The national bits are reversed in specification, Table 32 */
    ttm->ttm_national = ((c >> 7) & 1) |
                        ((c >> 5) & 2) |
                        ((c >> 3) & 4);

    if(s12 & 0x80) {
      /* Erase page */
      memset(ttm->ttm_page, ' ', 23 * 40);
    }

    if(update_tt_clock(t, buf + 34))
      teletext_rundown_scan(t, ttp);

    ttm->ttm_current_pts = t->s_current_pts;
    break;

  case 1 ... 23:
    for(i = 0; i < 40; i++) {
      c = buf[i + 2] & 0x7f;
      ttm->ttm_page[i + 40 * (line - 1)] = c;
    }
    break;

  case 28:
  case 29:
    if (ham_decode8(buf[2]) != 0) break; /* designation must be 0 */
    trip1 = ham_decode24(buf + 3);
    trip2 = ham_decode24(buf + 6);
    /* extract the character set designation and national option selection bits */
    ttm->ttm_charset[0] = (trip1 >> 7) & 0x7f;
    ttm->ttm_charset[1] = (trip1 >> 14) | ((trip2 & 7) << 4);
    break;

  default:
    break;
  }
}


/**
 *
 */
void
teletext_input
  (mpegts_service_t *t, elementary_stream_t *st, const uint8_t *data, int len)
{
  int j;
  uint8_t buf[42];

  data++; len--; /* first byte: 0x10 ? */

  for (; len >= 46; data += 46, len -= 46) {
    if (*data == 2 || *data == 3) {
      for(j = 0; j < 42; j++)
	buf[j] = bitreverse(data[4 + j]);
      tt_decode_line(t, st, buf);
    }
 }
}



/**
 * Swedish TV4 rundown dump (page 192)
 *

  Starttid Titel                L{ngd      | 3 3 53
 ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,   | 14 2c 2c
  19:00:00 TV4Nyheterna         00:14:00   | d 7 31
                                           | 20 20 20
 ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,   | 14 2c 2c
  19:14:00 Lokala nyheter       00:03:00   | 1 1 31
  19:17:00 TV4Nyheterna 19.00 2 00:02:02   | 7 7 31
  19:19:02 Vinjett              00:00:03   | 6 6 31
  19:19:05 Reklam               00:02:30   | 3 3 31
  19:21:35 Vinjett              00:00:06   | 6 6 31
  19:21:41 LOKAL BILLBOARD      00:00:16   | 1 1 31
  19:21:57 Lokalt v{der         00:01:00   | 1 1 31
  19:22:57 S4NVMT1553           00:00:15   | 6 6 31
  19:23:12 V{der                00:02:00   | 7 7 31
  19:25:12 Trailer              00:01:00   | 2 2 31
  19:26:12 Vinjett              00:00:03   | 6 6 31
  19:26:15 Reklam               00:02:20   | 3 3 31
  19:28:35 Vinjett              00:00:07   | 6 6 31
  19:28:42 Hall} 16:9           00:00:30   | 7 7 31
 ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,   | 14 2c 2c
                                           | 20 20 20
TV4                                        | 54 56 34
                                           | 20 20 20
*/


static int
tt_time_to_len(const uint8_t *buf)
{
  int l;
  char str[10];


  memcpy(str, buf, 8);
  str[2] = 0;
  str[5] = 0;
  str[8] = 0;
  
  l = atoi(str + 0) * 3600 + atoi(str + 3) * 60 + atoi(str + 6);
  return l;
}

/*
 * Decode the Swedish TV4 teletext rundown page to figure out if we are 
 * currently in a commercial break
 */
static void
teletext_rundown_copy(tt_private_t *ttp, tt_mag_t *ttm)
{
  /* Sanity check */
  if(memcmp((char *)ttm->ttm_page + 2, "Starttid", strlen("Starttid")) ||
     memcmp((char *)ttm->ttm_page + 11, "Titel", strlen("Titel")) ||
     memcmp((char *)ttm->ttm_page + 21 * 40, "TV4", strlen("TV4")))
    return;
  
  memcpy(ttp->ttp_rundown, ttm->ttm_page, 23 * 40);
  ttp->ttp_rundown_valid = 1;
}


static void
teletext_rundown_scan(mpegts_service_t *t, tt_private_t *ttp)
{
  int i;
  uint8_t *l;
  time_t now = t->s_tt_clock, start, stop;
  th_commercial_advice_t ca;

  if(ttp->ttp_rundown_valid == 0)
    return;

  if(t->s_dvb_svcname &&
     strcmp("TV4", t->s_dvb_svcname) &&
     strcmp("TV4 HD", t->s_dvb_svcname))
    return;

  for(i = 0; i < 23; i++) {
    l = ttp->ttp_rundown + 40 * i;
    if((l[1] & 0xf0) != 0x00 || !is_tt_clock(l + 32) || !is_tt_clock(l + 2))
      continue;
    
    if(!memcmp(l + 11, "Nyhetspuff", strlen("Nyhetspuff")))
      ca = COMMERCIAL_YES;
    else
      ca = (l[1] & 0xf) == 7 ? COMMERCIAL_NO : COMMERCIAL_YES;

    start = tt_construct_unix_time(l + 2);
    stop  = start + tt_time_to_len(l + 32);
    
    if(start <= now && stop > now)
      t->s_tt_commercial_advice = ca;
  }
}
