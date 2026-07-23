/*
 *  Tvheadend - T2-MI decapsulation (ETSI TS 102 773)
 *
 *  Copyright (C) 2026 Tvheadend Project
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

#ifndef __TVH_T2MI_DECAP_H__
#define __TVH_T2MI_DECAP_H__

#include <stdint.h>
#include <stddef.h>

/*
 * Extracts an inner MPEG-TS from a carrier PID of an outer MPEG-TS.
 * Two carrier formats are supported:
 *
 *   T2MI_DECAP_FORMAT_T2MI - DVB-T2 Modulator Interface packets
 *     (ETSI TS 102 773) delivered via DVB data piping (EN 301 192);
 *     the inner TS is rebuilt from the baseband frames of one PLP.
 *
 *   T2MI_DECAP_FORMAT_PIPE - plain "TS in TS" data piping where the
 *     carrier PID payload bytes are the inner TS byte stream
 *     (used e.g. by the Abertis/Cellnex DTT distribution on Hispasat).
 *
 *   T2MI_DECAP_FORMAT_AUTO probes the stream and locks on whichever
 *   format is detected first.
 *
 * The engine has no tvheadend dependencies and only requires aligned
 * 188-byte packets of the (descrambled) carrier PID as input.
 */

#define T2MI_DECAP_FORMAT_AUTO   0
#define T2MI_DECAP_FORMAT_T2MI   1
#define T2MI_DECAP_FORMAT_PIPE   2

#define T2MI_DECAP_PLP_AUTO      (-1)

typedef struct t2mi_decap t2mi_decap_t;

/* Inner TS output: len is always a multiple of 188 */
typedef void (*t2mi_decap_output_cb)(void *aux, const uint8_t *tsb, int len);

/* Diagnostics: printf-style, severity 0=debug 1=info 2=warning 3=error */
typedef void (*t2mi_decap_log_cb)(void *aux, int severity, const char *fmt, ...);

typedef struct t2mi_decap_stats {
  uint64_t in_packets;        /* outer carrier TS packets consumed */
  uint64_t out_packets;       /* inner TS packets produced */
  uint64_t t2mi_packets;      /* valid T2-MI packets */
  uint64_t bb_frames;         /* baseband frames processed (selected PLP) */
  uint64_t crc32_errors;      /* T2-MI packets dropped on CRC-32 */
  uint64_t crc8_errors;       /* baseband frames dropped on BBHEADER CRC-8 */
  uint64_t cc_errors;         /* outer continuity errors */
  uint64_t resyncs;           /* piping resynchronizations */
  uint64_t dnp_packets;       /* null packets deleted upstream (NPD) */
  int      format;            /* locked format, T2MI_DECAP_FORMAT_* */
  int      plp;               /* selected PLP or -1 */
  uint32_t plp_seen_mask[8];  /* bitmask of PLP ids seen in BB frames */
} t2mi_decap_stats_t;

t2mi_decap_t *t2mi_decap_create(int format, int plp,
                                t2mi_decap_output_cb output, void *output_aux,
                                t2mi_decap_log_cb log, void *log_aux);
void t2mi_decap_destroy(t2mi_decap_t *td);
void t2mi_decap_reset(t2mi_decap_t *td);

/* Feed one aligned 188-byte packet of the carrier PID */
void t2mi_decap_input(t2mi_decap_t *td, const uint8_t *tsb);

const t2mi_decap_stats_t *t2mi_decap_get_stats(t2mi_decap_t *td);

#endif /* __TVH_T2MI_DECAP_H__ */
