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

/*
 * References:
 *   - ETSI TS 102 773 (T2-MI packet format, encapsulation in MPEG-2 TS)
 *   - ETSI EN 302 755 clause 5.1 (mode adaptation, BBHEADER, HEM/NM)
 *   - ETSI EN 301 192 clause 4 (data piping)
 * The reconstruction logic follows the same model as TSDuck's T2MIDemux
 * (BSD-2-Clause), with the NPD/DNP and Normal Mode handling done per the
 * EN 302 755 text.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "t2mi_decap.h"

#define TS_PKT              188
#define T2MI_HEADER_SIZE    6
#define T2MI_CRC_SIZE       4
#define T2MI_MAX_PACKET     (T2MI_HEADER_SIZE + 8192 + T2MI_CRC_SIZE)
#define T2MI_BUF_SIZE       (2 * T2MI_MAX_PACKET + TS_PKT)
#define BBFRAME_SUBHDR      3
#define BBHEADER_SIZE       10
#define UP_MAX              256           /* max transmitted user packet unit */
#define PIPE_BUF_SIZE       4096
#define OUT_BUF_SIZE        (96 * TS_PKT)
#define PROBE_PKTS          256

/* T2-MI packet types (TS 102 773 table 1) */
#define T2MI_TYPE_BBFRAME   0x00

/* log-once flags */
#define T2MI_LO_NONTS       (1 << 0)
#define T2MI_LO_BADUPL      (1 << 1)
#define T2MI_LO_NOLOCK      (1 << 2)
#define T2MI_LO_MODE        (1 << 3)

struct t2mi_decap {
  int      cfg_format;
  int      cfg_plp;

  t2mi_decap_output_cb output;
  void    *output_aux;
  t2mi_decap_log_cb log;
  void    *log_aux;

  t2mi_decap_stats_t stats;

  uint32_t log_once;

  /* outer TS state */
  int      last_cc;

  /* T2-MI packet reassembly */
  uint8_t  t2buf[T2MI_BUF_SIZE];
  int      t2len;
  int      t2sync;
  int      last_pkt_count;
  int      t2mi_valid;          /* CRC-valid packets seen (probe counter) */

  /* baseband frame -> inner TS state (selected PLP) */
  uint8_t  frag[UP_MAX];
  int      fraglen;
  int      frag_valid;

  /* TS piping state */
  uint8_t  pipebuf[PIPE_BUF_SIZE];
  int      pipelen;
  int      pipesync;
  int      pipe_run;            /* consecutive stride hits (probe counter) */

  /* format autodetection */
  uint8_t *probe;
  int      probe_cnt;
  int      probing;

  /* output accumulation */
  uint8_t  out[OUT_BUF_SIZE];
  int      outlen;
};

/* **************************************************************************
 * CRC helpers (MPEG-2 CRC-32 and EN 302 755 CRC-8, both MSB first)
 * *************************************************************************/

static uint32_t t2mi_crc32_tab[256];
static uint8_t  t2mi_crc8_tab[256];
static int      t2mi_crc_ready = 0;

static void
t2mi_crc_init(void)
{
  uint32_t c32;
  uint8_t c8;
  int i, j;

  if (t2mi_crc_ready)
    return;
  for (i = 0; i < 256; i++) {
    c32 = (uint32_t)i << 24;
    c8  = (uint8_t)i;
    for (j = 0; j < 8; j++) {
      c32 = (c32 & 0x80000000) ? (c32 << 1) ^ 0x04c11db7 : (c32 << 1);
      c8  = (c8  & 0x80)       ? (c8  << 1) ^ 0xd5       : (c8  << 1);
    }
    t2mi_crc32_tab[i] = c32;
    t2mi_crc8_tab[i]  = c8;
  }
  t2mi_crc_ready = 1;
}

static uint32_t
t2mi_crc32(const uint8_t *data, int len)
{
  uint32_t crc = 0xffffffff;
  while (len-- > 0)
    crc = (crc << 8) ^ t2mi_crc32_tab[((crc >> 24) ^ *data++) & 0xff];
  return crc;
}

static uint8_t
t2mi_crc8(const uint8_t *data, int len)
{
  uint8_t crc = 0;
  while (len-- > 0)
    crc = t2mi_crc8_tab[crc ^ *data++];
  return crc;
}

/* **************************************************************************
 * Logging / output helpers
 * *************************************************************************/

#define t2mi_log(td, sev, fmt...) \
  do { if ((td)->log) (td)->log((td)->log_aux, sev, ##fmt); } while (0)

#define t2mi_log_once(td, flag, sev, fmt...) \
  do { \
    if (((td)->log_once & (flag)) == 0) { \
      (td)->log_once |= (flag); \
      t2mi_log(td, sev, ##fmt); \
    } \
  } while (0)

static void
t2mi_decap_out_flush(t2mi_decap_t *td)
{
  if (td->outlen > 0) {
    if (td->output)
      td->output(td->output_aux, td->out, td->outlen);
    td->outlen = 0;
  }
}

static void
t2mi_decap_out_packet(t2mi_decap_t *td, const uint8_t *hdr, int hdrlen,
                      const uint8_t *data, int datalen)
{
  if (td->probing)
    return;
  if (td->outlen + TS_PKT > OUT_BUF_SIZE)
    t2mi_decap_out_flush(td);
  if (hdrlen > 0)
    memcpy(td->out + td->outlen, hdr, hdrlen);
  memcpy(td->out + td->outlen + hdrlen, data, datalen);
  td->outlen += hdrlen + datalen;
  td->stats.out_packets++;
}

/* **************************************************************************
 * Baseband frame processing (T2-MI packet type 0x00)
 * *************************************************************************/

static void
t2mi_decap_emit_up(t2mi_decap_t *td, const uint8_t *up, int up_size, int hem,
                   int npd)
{
  static const uint8_t sync = 0x47;

  if (hem) {
    /* [187 payload bytes][DNP] - sync byte removed upstream */
    t2mi_decap_out_packet(td, &sync, 1, up, TS_PKT - 1);
    if (npd)
      td->stats.dnp_packets += up[TS_PKT - 1];
  } else {
    /* [CRC-8 in sync position][187 payload bytes][ISSY][DNP] */
    t2mi_decap_out_packet(td, &sync, 1, up + 1, TS_PKT - 1);
    if (npd)
      td->stats.dnp_packets += up[up_size - 1];
  }
}

static void
t2mi_decap_bbframe(t2mi_decap_t *td, const uint8_t *pay, int paylen)
{
  const uint8_t *bb, *df;
  uint8_t plp_id, matype1, mode;
  int hem, npd, upl, dfl, syncd, df_bytes, up_size, pos, need;

  if (paylen < BBFRAME_SUBHDR + BBHEADER_SIZE)
    return;

  plp_id = pay[1];
  td->stats.plp_seen_mask[plp_id >> 5] |= 1U << (plp_id & 31);

  /* PLP selection */
  if (td->stats.plp < 0) {
    if (td->cfg_plp != T2MI_DECAP_PLP_AUTO && td->cfg_plp != plp_id)
      return;
    if (!td->probing) {
      td->stats.plp = plp_id;
      t2mi_log(td, 1, "PLP %d selected", plp_id);
    }
  } else if (plp_id != td->stats.plp) {
    return;
  }

  bb = pay + BBFRAME_SUBHDR;
  matype1 = bb[0];

  /* CRC-8 of the first 9 BBHEADER bytes XOR MODE (EN 302 755 5.1.7) */
  mode = t2mi_crc8(bb, 9) ^ bb[9];
  if (mode & ~1) {
    td->stats.crc8_errors++;
    td->frag_valid = 0;
    return;
  }
  hem = mode == 1;

  if ((matype1 >> 6) != 3) {
    /* GFPS/GCS/GSE - no transport stream to extract */
    t2mi_log_once(td, T2MI_LO_NONTS, 2,
                  "PLP %d does not carry a transport stream (TS/GS %d)",
                  plp_id, matype1 >> 6);
    return;
  }
  npd = (matype1 >> 2) & 1;

  upl   = (bb[2] << 8) | bb[3];
  dfl   = (bb[4] << 8) | bb[5];
  syncd = (bb[7] << 8) | bb[8];

  if (!td->probing)
    t2mi_log_once(td, T2MI_LO_MODE, 0,
                  "PLP %d mode adaptation: %s, %s%sUPL %d, DFL %d bits",
                  plp_id, hem ? "high efficiency" : "normal",
                  ((matype1 >> 3) & 1) ? "ISSY, " : "",
                  npd ? "NPD, " : "", upl, dfl);

  if (hem) {
    /* sync byte removed; ISSY (if any) is carried in the reused
     * UPL/SYNC header fields, not in the data field */
    up_size = TS_PKT - 1 + (npd ? 1 : 0);
  } else {
    up_size = upl / 8;
    if ((upl & 7) || up_size < TS_PKT || up_size > UP_MAX) {
      t2mi_log_once(td, T2MI_LO_BADUPL, 2,
                    "unsupported user packet length %d bits", upl);
      td->frag_valid = 0;
      return;
    }
  }

  df = bb + BBHEADER_SIZE;
  df_bytes = (dfl + 7) / 8;
  if (df_bytes > paylen - BBFRAME_SUBHDR - BBHEADER_SIZE)
    df_bytes = paylen - BBFRAME_SUBHDR - BBHEADER_SIZE;

  td->stats.bb_frames++;

  if (syncd == 0xffff) {
    /* no user packet starts in this data field - pure continuation */
    if (td->frag_valid && td->fraglen > 0) {
      if (td->fraglen + df_bytes <= up_size) {
        memcpy(td->frag + td->fraglen, df, df_bytes);
        td->fraglen += df_bytes;
        if (td->fraglen == up_size) {
          t2mi_decap_emit_up(td, td->frag, up_size, hem, npd);
          td->fraglen = 0;
        }
      } else {
        td->frag_valid = 0;
      }
    }
    return;
  }

  pos = syncd / 8;
  if (pos > df_bytes) {
    td->frag_valid = 0;
    return;
  }

  /* complete the user packet fragmented across baseband frames */
  if (td->frag_valid && td->fraglen > 0) {
    need = up_size - td->fraglen;
    if (need == pos) {
      memcpy(td->frag + td->fraglen, df, need);
      t2mi_decap_emit_up(td, td->frag, up_size, hem, npd);
    }
    /* else: inconsistent SYNCD, drop the fragment silently */
  }
  td->fraglen = 0;
  td->frag_valid = 1;

  /* whole user packets */
  while (pos + up_size <= df_bytes) {
    t2mi_decap_emit_up(td, df + pos, up_size, hem, npd);
    pos += up_size;
  }

  /* trailing fragment continues in the next baseband frame */
  if (pos < df_bytes) {
    td->fraglen = df_bytes - pos;
    memcpy(td->frag, df + pos, td->fraglen);
  }
}

/* **************************************************************************
 * T2-MI packet layer
 * *************************************************************************/

static void
t2mi_decap_t2mi_packet(t2mi_decap_t *td, const uint8_t *pkt, int paylen)
{
  /* a gap in the T2-MI packet counter invalidates any user packet
   * fragment carried over from a previous baseband frame, even when
   * the lost packet belonged to another PLP or packet type */
  if (td->last_pkt_count >= 0 &&
      ((td->last_pkt_count + 1) & 0xff) != pkt[1])
    td->frag_valid = 0;
  td->last_pkt_count = pkt[1];

  td->stats.t2mi_packets++;
  td->t2mi_valid++;

  if (pkt[0] == T2MI_TYPE_BBFRAME)
    t2mi_decap_bbframe(td, pkt + T2MI_HEADER_SIZE, paylen);
}

static void
t2mi_decap_t2mi_reset(t2mi_decap_t *td)
{
  td->t2len = 0;
  td->t2sync = 0;
  td->frag_valid = 0;
  td->fraglen = 0;
}

static void
t2mi_decap_t2mi_input(t2mi_decap_t *td, const uint8_t *pay, int paylen,
                      int pusi, int cc_error)
{
  int pf, start, bits, pb, tot;
  uint32_t crc;

  if (cc_error) {
    /* incomplete T2-MI packet lost - resynchronize on the next PUSI */
    t2mi_decap_t2mi_reset(td);
  }

  if (paylen <= 0)
    return;

  if (pusi) {
    pf = pay[0];
    if (1 + pf > paylen) {
      t2mi_decap_t2mi_reset(td);
      return;
    }
    if (!td->t2sync) {
      /* drop the tail of the previous (unseen) packet */
      pay += 1 + pf;
      paylen -= 1 + pf;
      td->t2sync = 1;
      td->t2len = 0;
    } else {
      /* pointer byte is redundant while in sync */
      pay += 1;
      paylen -= 1;
    }
  } else if (!td->t2sync) {
    return;
  }

  if (td->t2len + paylen > T2MI_BUF_SIZE) {
    t2mi_decap_t2mi_reset(td);
    return;
  }
  memcpy(td->t2buf + td->t2len, pay, paylen);
  td->t2len += paylen;

  start = 0;
  while (td->t2len - start >= T2MI_HEADER_SIZE) {
    bits = (td->t2buf[start + 4] << 8) | td->t2buf[start + 5];
    pb   = (bits + 7) / 8;
    tot  = T2MI_HEADER_SIZE + pb + T2MI_CRC_SIZE;
    if (start + tot > td->t2len)
      break;
    crc = (td->t2buf[start + 6 + pb] << 24) |
          (td->t2buf[start + 7 + pb] << 16) |
          (td->t2buf[start + 8 + pb] << 8) |
           td->t2buf[start + 9 + pb];
    if (t2mi_crc32(td->t2buf + start, T2MI_HEADER_SIZE + pb) != crc) {
      td->stats.crc32_errors++;
      t2mi_decap_t2mi_reset(td);
      return;
    }
    t2mi_decap_t2mi_packet(td, td->t2buf + start, pb);
    start += tot;
  }
  if (start > 0) {
    td->t2len -= start;
    memmove(td->t2buf, td->t2buf + start, td->t2len);
  }
}

/* **************************************************************************
 * Plain TS piping layer
 * *************************************************************************/

static void
t2mi_decap_pipe_input(t2mi_decap_t *td, const uint8_t *pay, int paylen)
{
  int i, keep;

  if (paylen <= 0)
    return;

  if (td->pipelen + paylen > PIPE_BUF_SIZE) {
    /* should not happen - the parser below always drains the buffer */
    td->pipelen = 0;
    td->pipesync = 0;
  }
  memcpy(td->pipebuf + td->pipelen, pay, paylen);
  td->pipelen += paylen;

again:
  if (!td->pipesync) {
    for (i = 0; i + TS_PKT < td->pipelen; i++) {
      if (td->pipebuf[i] == 0x47 && td->pipebuf[i + TS_PKT] == 0x47) {
        td->pipesync = 1;
        td->stats.resyncs++;
        if (i > 0) {
          td->pipelen -= i;
          memmove(td->pipebuf, td->pipebuf + i, td->pipelen);
        }
        break;
      }
    }
    if (!td->pipesync) {
      /* keep only the last (unverifiable) packet's worth of bytes */
      keep = TS_PKT + 1;
      if (td->pipelen > keep) {
        memmove(td->pipebuf, td->pipebuf + td->pipelen - keep, keep);
        td->pipelen = keep;
      }
      return;
    }
  }

  i = 0;
  while (i + TS_PKT <= td->pipelen) {
    if (td->pipebuf[i] != 0x47) {
      /* lost the byte alignment */
      td->pipesync = 0;
      td->pipe_run = 0;
      td->pipelen -= i;
      memmove(td->pipebuf, td->pipebuf + i, td->pipelen);
      goto again;
    }
    t2mi_decap_out_packet(td, NULL, 0, td->pipebuf + i, TS_PKT);
    td->pipe_run++;
    i += TS_PKT;
  }
  if (i > 0) {
    td->pipelen -= i;
    memmove(td->pipebuf, td->pipebuf + i, td->pipelen);
  }
}

/* **************************************************************************
 * Input, format autodetection
 * *************************************************************************/

static void
t2mi_decap_input_locked(t2mi_decap_t *td, const uint8_t *tsb)
{
  const uint8_t *pay;
  int paylen, afc, pusi, cc, cc_error = 0;

  afc = (tsb[3] >> 4) & 3;
  if ((afc & 1) == 0)             /* no payload */
    return;

  cc = tsb[3] & 0x0f;
  if (td->last_cc >= 0 && ((td->last_cc + 1) & 0x0f) != cc) {
    td->stats.cc_errors++;
    cc_error = 1;
  }
  td->last_cc = cc;

  if (afc & 2) {
    paylen = TS_PKT - 5 - tsb[4];
    pay = tsb + 5 + tsb[4];
  } else {
    paylen = TS_PKT - 4;
    pay = tsb + 4;
  }
  if (paylen <= 0)
    return;

  pusi = (tsb[1] & 0x40) ? 1 : 0;

  if (td->stats.format == T2MI_DECAP_FORMAT_T2MI) {
    t2mi_decap_t2mi_input(td, pay, paylen, pusi, cc_error);
  } else {
    if (cc_error)
      td->pipesync = 0;
    t2mi_decap_pipe_input(td, pay, paylen);
  }
}

static void
t2mi_decap_lock_format(t2mi_decap_t *td, int format)
{
  uint8_t *probe = td->probe;
  int cnt = td->probe_cnt, i;

  td->stats.format = format;
  td->probing = 0;
  td->probe = NULL;
  td->probe_cnt = 0;

  t2mi_log(td, 1, "carrier format detected: %s",
           format == T2MI_DECAP_FORMAT_T2MI ?
           "T2-MI (TS 102 773)" : "plain TS data piping");

  /* replay the probe window through the selected parser */
  td->last_cc = -1;
  td->last_pkt_count = -1;
  t2mi_decap_t2mi_reset(td);
  td->pipelen = 0;
  td->pipesync = 0;

  if (probe) {
    for (i = 0; i < cnt; i++)
      t2mi_decap_input_locked(td, probe + i * TS_PKT);
    free(probe);
  }
}

static void
t2mi_decap_input_probe(t2mi_decap_t *td, const uint8_t *tsb)
{
  const uint8_t *pay;
  int paylen, afc, pusi, cc, cc_error = 0;

  if (td->probe == NULL) {
    td->probe = malloc(PROBE_PKTS * TS_PKT);
    if (td->probe == NULL)
      return;
    td->probe_cnt = 0;
  }
  memcpy(td->probe + td->probe_cnt * TS_PKT, tsb, TS_PKT);
  td->probe_cnt++;

  /* run both trial parsers */
  afc = (tsb[3] >> 4) & 3;
  if (afc & 1) {
    cc = tsb[3] & 0x0f;
    if (td->last_cc >= 0 && ((td->last_cc + 1) & 0x0f) != cc)
      cc_error = 1;
    td->last_cc = cc;
    if (afc & 2) {
      paylen = TS_PKT - 5 - tsb[4];
      pay = tsb + 5 + tsb[4];
    } else {
      paylen = TS_PKT - 4;
      pay = tsb + 4;
    }
    if (paylen > 0) {
      pusi = (tsb[1] & 0x40) ? 1 : 0;
      t2mi_decap_t2mi_input(td, pay, paylen, pusi, cc_error);
      if (td->t2mi_valid >= 2) {
        t2mi_decap_lock_format(td, T2MI_DECAP_FORMAT_T2MI);
        return;
      }
      t2mi_decap_pipe_input(td, pay, paylen);
      if (td->pipe_run >= 5) {
        t2mi_decap_lock_format(td, T2MI_DECAP_FORMAT_PIPE);
        return;
      }
    }
  }

  if (td->probe_cnt >= PROBE_PKTS) {
    t2mi_log_once(td, T2MI_LO_NOLOCK, 2,
                  "unable to detect carrier format "
                  "(scrambled or unsupported stream?)");
    /* drop the oldest half of the window and keep trying */
    td->probe_cnt = PROBE_PKTS / 2;
    memmove(td->probe, td->probe + (PROBE_PKTS / 2) * TS_PKT,
            (PROBE_PKTS / 2) * TS_PKT);
  }
}

void
t2mi_decap_input(t2mi_decap_t *td, const uint8_t *tsb)
{
  td->stats.in_packets++;

  if (td->stats.format == T2MI_DECAP_FORMAT_AUTO) {
    td->probing = 1;
    t2mi_decap_input_probe(td, tsb);
  } else {
    t2mi_decap_input_locked(td, tsb);
  }

  t2mi_decap_out_flush(td);
}

/* **************************************************************************
 * Lifecycle
 * *************************************************************************/

t2mi_decap_t *
t2mi_decap_create(int format, int plp,
                  t2mi_decap_output_cb output, void *output_aux,
                  t2mi_decap_log_cb log, void *log_aux)
{
  t2mi_decap_t *td;

  t2mi_crc_init();

  td = calloc(1, sizeof(*td));
  if (td == NULL)
    return NULL;
  td->cfg_format = format;
  td->cfg_plp = plp;
  td->output = output;
  td->output_aux = output_aux;
  td->log = log;
  td->log_aux = log_aux;
  t2mi_decap_reset(td);
  return td;
}

void
t2mi_decap_reset(t2mi_decap_t *td)
{
  free(td->probe);
  td->probe = NULL;
  td->probe_cnt = 0;
  td->probing = 0;
  td->last_cc = -1;
  td->last_pkt_count = -1;
  td->t2mi_valid = 0;
  td->pipe_run = 0;
  td->pipelen = 0;
  td->pipesync = 0;
  td->outlen = 0;
  td->log_once = 0;
  t2mi_decap_t2mi_reset(td);
  memset(&td->stats, 0, sizeof(td->stats));
  td->stats.format = td->cfg_format;
  td->stats.plp = -1;   /* locked on the first matching baseband frame */
}

void
t2mi_decap_destroy(t2mi_decap_t *td)
{
  if (td == NULL)
    return;
  free(td->probe);
  free(td);
}

const t2mi_decap_stats_t *
t2mi_decap_get_stats(t2mi_decap_t *td)
{
  return &td->stats;
}
