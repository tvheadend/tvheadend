/*
 *  tvheadend, EMM reassembly functions
 *  Copyright (C) 2007 Andreas Öman
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

#include "emm_reass.h"
#include "descrambler/caid.h"

#define EMM_CACHE_SIZE (1<<5)
#define EMM_CACHE_MASK (EMM_CACHE_SIZE-1)

#define PROVIDERS_FOREACH(ra, i, ep) \
  for (i = 0, ep = (ra)->providers; i < (ra)->providers_count; i++, ep++)

/**
 *
 */
static void
emm_cache_insert(emm_reass_t *ra, uint32_t crc)
{
  /* evict the oldest entry */
  ra->emm_cache[ra->emm_cache_write++] = crc;
  ra->emm_cache_write &= EMM_CACHE_MASK;
  if (ra->emm_cache_count < EMM_CACHE_SIZE)
    ra->emm_cache_count++;
}

static int
emm_cache_lookup(emm_reass_t *ra, uint32_t crc)
{
  int i;
  for (i = 0; i < ra->emm_cache_count; i++)
    if (ra->emm_cache[i] == crc)
      return 1;
  return 0;
}

/**
 * conax emm handler
 */
static void
emm_conax
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  emm_provider_t *ep;
  int i;

  if (len < 10)
    return;
  if (data[0] == 0x82) {
    PROVIDERS_FOREACH(ra, i, ep)
      if (memcmp(&data[3], &ep->sa[1], 7) == 0) {
        send(aux, data, len, mux);
        break;
      }
  }
}

/**
 * irdeto emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
static void
emm_irdeto
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int i, emm_mode, emm_len, match = 0;
  emm_provider_t *ep;

  if (len < 4)
    return;

  emm_mode = data[3] >> 3;
  emm_len = data[3] & 0x07;

  if (4 + emm_len > len)
    return;

  if (emm_mode & 0x10){
    // try to match card
    match = (emm_mode == ra->ua[4] &&
             (!emm_len || // zero length
              !memcmp(&data[4], &ra->ua[5], emm_len))); // exact match
  } else {
    // try to match provider
    PROVIDERS_FOREACH(ra, i, ep) {
      match = (emm_mode == ep->sa[4] &&
               (!emm_len || // zero length
                !memcmp(&data[4], &ep->sa[5], emm_len)));
      // exact match
      if (match) break;
    }
  }

  if (match)
    send(aux, data, len, mux);
}


/**
 * seca emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
static void
emm_seca
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int i, match = 0;
  emm_provider_t *ep;

  if (len < 1)
    return;

  if (data[0] == 0x82) {         // unique emm
    if (len < 9)
      return;
    match = memcmp(&data[3], &ra->ua[2], 6) == 0;
  } else if (data[0] == 0x84) {  // shared emm
    if (len < 8)
      return;
    /* XXX this part is untested but should do no harm */
    PROVIDERS_FOREACH(ra, i, ep)
      if (memcmp(&data[5], &ep->sa[5], 3) == 0) {
        match = 1;
        break;
      }
  } else if (data[0] == 0x83) {  // global emm -> seca3
    match = 1;
  }

  if (match)
    send(aux, data, len, mux);
}

/**
 * viaccess emm handler
 * inspired by opensasc-ng, https://opensvn.csie.org/traccgi/opensascng/
 */
static
const uint8_t * nano_start(const uint8_t * data, int len)
{
  int l;

  switch(data[0]) {
  case 0x88: l = 8; break;
  case 0x8e: l = 7; break;
  case 0x8c:
  case 0x8d: l = 3; break;
  case 0x80:
  case 0x81: l = 4; break;
  default: return NULL;
  }
  return l < len ? data + l : NULL;
}

static
const uint8_t * nano_checknano90fromnano(const uint8_t * data, int len)
{
  if(data && len > 4 && data[0]==0x90 && data[1]==0x03) return data;
  return NULL;
}

static
const uint8_t * nano_checknano90(const uint8_t * data, int len)
{
  const uint8_t *start = nano_start(data, len);
  if (start)
    return nano_checknano90fromnano(start, len - (start - data));
  return NULL;
}

static
int sort_nanos(uint8_t *dest, const uint8_t *src, int len)
{
  int w = 0, c = -1;

  while (1) {
    int j, n;
    n = 0x100;
    for (j = 0; j < len;) {
      int l = src[j + 1] + 2;
      if (src[j] == c) {
        if (w + l > len)
          return -1;
        memcpy(dest + w, src + j, l);
        w += l;
      }
      else if (src[j] > c && src[j] < n) {
        n = src[j];
      }
      j += l;
    }
    if (n == 0x100) {
      break;
    }
    c = n;
  }
  return 0;
}

static int via_provider_id(const uint8_t * data, int len)
{
  const uint8_t * tmp;
  tmp = nano_checknano90(data, len);
  if (!tmp) return 0;
  return (tmp[2] << 16) | (tmp[3] << 8) | (tmp[4]&0xf0);
}

#define EP_VIACCESS(ep, idx) (ep)->u.viacess[idx]

static void
emm_viaccess
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int i, idx = 0, olen = len, match = 0;
  emm_provider_t *ep;
  uint32_t id;

  if (len < 3)
    return;

  /* Get SCT len */
  len = 3 + ((data[1] & 0x0f) << 8) + data[2];

  if (len > olen)
    return;

  if (data[0] == 0x8c || data[0] == 0x8d) {

    /* Match provider id */
    id = via_provider_id(data, len);
    if (!id) return;

    PROVIDERS_FOREACH(ra, i, ep)
      if (ep->id == id) {
        match = 1;
        break;
      }
    if (!match) return;

    idx = data[0] - 0x8c;
    EP_VIACCESS(ep, idx).shared_len = 0;
    free(EP_VIACCESS(ep, idx).shared_emm);
    if ((EP_VIACCESS(ep, idx).shared_emm = (uint8_t*)malloc(len)) != NULL) {
      EP_VIACCESS(ep, idx).shared_len = len;
      memcpy(EP_VIACCESS(ep, idx).shared_emm, data, len);
      EP_VIACCESS(ep, idx).shared_mux = mux;
    }
  } else if (data[0] == 0x8e) {
    /* Match SA and provider in shared */
    PROVIDERS_FOREACH(ra, i, ep) {
      if (memcmp(&data[3], &ep->sa[4], 3)) continue;
      if ((data[6]&2)) continue;
      for (idx = 0; idx < 2; idx++)
        if (EP_VIACCESS(ep, idx).shared_emm &&
            EP_VIACCESS(ep, idx).shared_mux == mux) {
          id = via_provider_id(EP_VIACCESS(ep, idx).shared_emm,
                               EP_VIACCESS(ep, idx).shared_len);
          if (id == ep->id) {
            match = 1;
            break;
          }
        }
      if (match) break;
    }
    if (match) {
      uint8_t *tmp, *ass2;
      const uint8_t *ass;
      uint32_t crc;
      int l;

      tmp = alloca(len + EP_VIACCESS(ep, idx).shared_len);
      ass = nano_start(data, len);
      if (ass == NULL) return;
      len -= (ass - data);
      if((data[6] & 2) == 0)  {
        int addrlen = len - 8;
        len=0;
        tmp[len++] = 0x9e;
        tmp[len++] = addrlen;
        memcpy(&tmp[len], &ass[0], addrlen); len += addrlen;
        tmp[len++] = 0xf0;
        tmp[len++] = 0x08;
        memcpy(&tmp[len],&ass[addrlen],8); len += 8;
      } else {
        memcpy(tmp, ass, len);
      }
      ass = nano_start(EP_VIACCESS(ep, idx).shared_emm,
                       EP_VIACCESS(ep, idx).shared_len);
      if (ass == NULL) return;
      l = EP_VIACCESS(ep, idx).shared_len - (ass - EP_VIACCESS(ep, idx).shared_emm);
      memcpy(&tmp[len], ass, l); len += l;

      ass2 = (uint8_t*) alloca(len+7);
      if(ass2 == NULL)
        return;

      memcpy(ass2, data, 7);
      if (sort_nanos(ass2 + 7, tmp, len))
        return;

      free(EP_VIACCESS(ep, idx).shared_emm);
      EP_VIACCESS(ep, idx).shared_emm = NULL;
      EP_VIACCESS(ep, idx).shared_len = 0;

      /* Set SCT len */
      len += 4;
      ass2[1] = (len>>8) | 0x70;
      ass2[2] = len & 0xff;
      len += 3;

      crc = tvh_crc32(ass2, len, 0xffffffff);
      if (!emm_cache_lookup(ra, crc)) {
        tvhdebug(LS_CWC,
                "Send EMM "
                "%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x"
                "...%02x.%02x.%02x.%02x",
                ass2[0], ass2[1], ass2[2], ass2[3],
                ass2[4], ass2[5], ass2[6], ass2[7],
                ass2[len-4], ass2[len-3], ass2[len-2], ass2[len-1]);
        send(aux, ass2, len, mux);
        emm_cache_insert(ra, crc);
      }
    }
  }
}

/**
 * dre emm handler
 */
static void
emm_dre
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int i, match = 0;
  emm_provider_t *ep;

  if (len < 1)
    return;

  if (data[0] == 0x87) {
    if (len < 7)
      return;
    match = memcmp(&data[3], &ra->ua[4], 4) == 0;
  } else if (data[0] == 0x86) {
    PROVIDERS_FOREACH(ra, i, ep)
      if (memcmp(&data[40], &ep->sa[4], 4) == 0) {
   /* if (memcmp(&data[3], &ep->sa[4], 1) == 0) { */
        match = 1;
        break;
      }
  }

  if (match)
    send(aux, data, len, mux);
}

static void
emm_nagra
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int match = 0;
  uint8_t hexserial[4];

  if (data[0] == 0x83) {        // unique|shared
    hexserial[0] = data[5];
    hexserial[1] = data[4];
    hexserial[2] = data[3];
    hexserial[3] = data[6];
    if (memcmp(hexserial, &ra->ua[4], (data[7] == 0x10) ? 3 : 4) == 0)
      match = 1;
  } else if (data[0] == 0x82) { // global
    match = 1;
  }

  if (match)
    send(aux, data, len, mux);
}

static void
emm_nds
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int i, match = 0, serial_count;
  uint8_t emmtype;

  if (len < 4)
    return;

  serial_count = ((data[3] >> 4) & 3) + 1;
  emmtype      = (data[3] & 0xC0) >> 6;

  if (emmtype == 1 || emmtype == 2) { // unique|shared
    for (i = 0; i < serial_count; i++) {
      if (memcmp(&data[i * 4 + 4], &ra->ua[4], 5 - emmtype) == 0) {
        match = 1;
        break;
      }
    }
  } else if (emmtype == 0) {          // global
    match = 1;
  }

  if (match)
    send(aux, data, len, mux);
}

/**
 * streamguard emm handler
 */
static void
emm_streamguard
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  //todo
  int i, match = 0;
  emm_provider_t *ep;

  if (len < 1)
    return;

  tvhinfo(LS_CWC, "emm_streamguard streamguard card data emm get,here lots of works todo...");

  if (data[0] == 0x87) {
    match = memcmp(&data[3], &ra->ua[4], 4) == 0;
  } else if (data[0] == 0x86) {
    PROVIDERS_FOREACH(ra, i, ep)
      if (memcmp(&data[40], &ep->sa[4], 4) == 0) {
   /* if (memcmp(&data[3], &cwc->cwc_providers[i].sa[4], 1) == 0) { */
        match = 1;
        break;
      }
  }

  if (match)
    send(aux, data, len, mux);
}

#define RA_CRYPTOWORKS(ra) (ra)->u.cryptoworks

static void
emm_cryptoworks
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int match = 0;

  if (len < 1)
    return;

  switch (data[0]) {
  case 0x82: /* unique */
    match = len >= 10 && memcmp(data + 5, ra->ua + 3, 5) == 0;
    break;
  case 0x84: /* emm-sh */
    if (len >= 12 && memcmp(data + 5, ra->ua + 3, 4) == 0) {
      if (RA_CRYPTOWORKS(ra).shared_emm) {
        free(RA_CRYPTOWORKS(ra).shared_emm);
        RA_CRYPTOWORKS(ra).shared_len = 0;
      }
      if ((RA_CRYPTOWORKS(ra).shared_emm = malloc(len)) != NULL) {
        RA_CRYPTOWORKS(ra).shared_len = len;
        memcpy(RA_CRYPTOWORKS(ra).shared_emm, data, len);
        RA_CRYPTOWORKS(ra).shared_mux = mux;
      }
    }
    break;
  case 0x86: /* emm-sb */
    if (len < 5)
      return;
    if (RA_CRYPTOWORKS(ra).shared_emm && RA_CRYPTOWORKS(ra).shared_mux == mux) {
      /* python: EMM_SH[0:12] + EMM_SB[5:-1] + EMM_SH[12:-1] */
      uint32_t elen = len - 5 + RA_CRYPTOWORKS(ra).shared_len - 12;
      uint8_t *tmp = malloc(elen);
      uint8_t *composed = tmp ? malloc(elen + 12) : NULL;
      if (composed) {
        memcpy(tmp, data + 5, len - 5);
        memcpy(tmp + len - 5, RA_CRYPTOWORKS(ra).shared_emm + 12,
                                RA_CRYPTOWORKS(ra).shared_len - 12);
        memcpy(composed, RA_CRYPTOWORKS(ra).shared_emm, 12);
        sort_nanos(composed + 12, tmp, elen);
        composed[1] = ((elen + 9) >> 8) | 0x70;
        composed[2] = (elen + 9) & 0xff;
        send(aux, composed, elen + 12, mux);
        free(composed);
        free(tmp);
      } else if (tmp)
        free(tmp);

      free(RA_CRYPTOWORKS(ra).shared_emm);
      RA_CRYPTOWORKS(ra).shared_emm = NULL;
      RA_CRYPTOWORKS(ra).shared_len = 0;
    }
    break;
  case 0x88: /* global */
  case 0x89: /* global */
    match = 1;
    break;
  default:
    break;
  }

  if (match)
    send(aux, data, len, mux);
}

static void
emm_bulcrypt
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  int match = 0;

  if (len < 1)
    return;

  switch (data[0]) {
  case 0x82: /* unique - bulcrypt (1 card) */
  case 0x8a: /* unique - polaris  (1 card) */
  case 0x85: /* unique - bulcrypt (4 cards) */
  case 0x8b: /* unique - polaris  (4 cards) */
    match = len >= 10 && memcmp(data + 3, ra->ua + 2, 3) == 0;
    break;
  case 0x84: /* shared - (1024 cards) */
    match = len >= 10 && memcmp(data + 3, ra->ua + 2, 2) == 0;
    break;
  }

  if (match)
    send(aux, data, len, mux);
}

static void
emm_griffin
  (emm_reass_t *ra, const uint8_t *data, int len, void *mux,
   emm_send_t send, void *aux)
{
  emm_provider_t *ep;
  int i;

  if (len < 1)
    return;

  switch (data[0]) {
  case 0x82:
  case 0x83:
    PROVIDERS_FOREACH(ra, i, ep)
      if (memcmp(&data[3], &ep->sa[0], 4) == 0) {
        send(aux, data, len, mux);
        break;
      }
    break;
  }
}

void
emm_filter(emm_reass_t *ra, const uint8_t *data, int len, void *mux,
           emm_send_t send, void *aux)
{
  tvhtrace(LS_CWC, "emm filter : %s - len %d mux %p", caid2name(ra->caid), len, mux);
  tvhlog_hexdump(LS_CWC, data, len);
  if (ra->do_emm)
    ra->do_emm(ra, data, len, mux, send, aux);
}

void
emm_reass_init(emm_reass_t *ra, uint16_t caid)
{
  memset(ra, 0, sizeof(*ra));
  ra->caid = caid;
  ra->type = detect_card_type(caid);
  switch (ra->type) {
  case CARD_CONAX:       ra->do_emm = emm_conax;        break;
  case CARD_IRDETO:      ra->do_emm = emm_irdeto;       break;
  case CARD_SECA:        ra->do_emm = emm_seca;         break;
  case CARD_VIACCESS:    ra->do_emm = emm_viaccess;     break;
  case CARD_DRE:         ra->do_emm = emm_dre;          break;
  case CARD_NAGRA:       ra->do_emm = emm_nagra;        break;
  case CARD_NDS:         ra->do_emm = emm_nds;          break;
  case CARD_CRYPTOWORKS: ra->do_emm = emm_cryptoworks;  break;
  case CARD_BULCRYPT:    ra->do_emm = emm_bulcrypt;     break;
  case CARD_STREAMGUARD: ra->do_emm = emm_streamguard;  break;
  case CARD_GRIFFIN:     ra->do_emm = emm_griffin;      break;
  default: break;
  }
}

void
emm_reass_done(emm_reass_t *ra)
{
  emm_provider_t *ep;
  int i;

  PROVIDERS_FOREACH(ra, i, ep) {
    if (ra->type == CARD_VIACCESS) {
      free(EP_VIACCESS(ep, 0).shared_emm);
      free(EP_VIACCESS(ep, 0).shared_emm);
    }
  }
  free(ra->providers);
  if (ra->type == CARD_CRYPTOWORKS)
    free(RA_CRYPTOWORKS(ra).shared_emm);
  memset(&ra, 0, sizeof(ra));
}

/*
 *
 */

#ifdef EMM_REASS_TEST

/*
 * Compile tvh and type:
 *   gcc -o emmra -DEMM_REASS_TEST -Isrc -Ibuild.linux -Wall src/descrambler/emm_reass.c
 * The output binary is named 'emmra' .
 *
 * See 'USER DATA' bellow.
 */

#include <ctype.h>
#include "caid.c"

int tvhlog_level = TVHLOG_OPT_ALL;

uint32_t tvh_crc32(const uint8_t *data, size_t datalen, uint32_t crc)
{
  /* just for debugging purposes, not real crc32 */
  while (datalen-- > 0)
    crc += *data++;
  return crc;
}

void tvhlogv (const char *file, int line,
              int notify, int severity,
              const char *subsys, const char *fmt, va_list *args )
{
  fprintf(stderr, "LOG[%s]: ", subsys);
  if (args) {
    vfprintf(stderr, fmt, *args);
    fprintf(stderr, "\n");
  } else {
    fprintf(stderr, "%s\n", fmt);
  }
}

void _tvhlog (const char *file, int line,
              int notify, int severity,
              const char *subsys, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  tvhlogv(file, line, notify, severity, subsys, fmt, &args);
  va_end(args);
}

#define HEXDUMP_WIDTH 16
void
_tvhlog_hexdump(const char *file, int line,
                int notify, int severity,
                const char *subsys,
                const uint8_t *data, ssize_t len )
{
  int i, c;
  char str[1024];

  /* Assume that severify was validated before call */

  /* Build and log output */
  while (len > 0) {
    c = 0;
    for (i = 0; i < HEXDUMP_WIDTH; i++) {
      if (i >= len)
        tvh_strlcatf(str, sizeof(str), c, "   ");
      else
        tvh_strlcatf(str, sizeof(str), c, "%02X ", data[i]);
    }
    for (i = 0; i < HEXDUMP_WIDTH; i++) {
      if (i < len) {
        if (data[i] < ' ' || data[i] > '~')
          str[c] = '.';
        else
          str[c] = data[i];
      } else
        str[c] = ' ';
      c++;
    }
    str[c] = '\0';
    tvhlogv(file, line, notify, severity, subsys, str, NULL);
    len  -= HEXDUMP_WIDTH;
    data += HEXDUMP_WIDTH;
  }
}

/*
 *  USER DATA
 *
 *  Hexa strings can be joined 01020a or delimited by any character 01:02:0A.
 */

#define CAID    0x0500
#define PROVID  0x040820
#define UA      "00:00:00:00:00:00:00"
#define SA      "00:00:00:00:A1:A2:A3"

static char *emms[] = {
  "8C 70 0C 90 03 04 08 21 A9 05 46 8B 46 BA 04",
  "8D 70 0C 90 03 04 08 21 A9 05 46 8D 46 BC 04",
  "8E 70 2C A1 A2 A3 00 00 00 00 00 00 00 00 00 00"
  "60 00 00 00 00 00 00 00 00 00 00 00 00 01 FF FF"
  "FF 5F FF FF FE 7F FF 02 B1 FC 60 FA 8A 36 86",
  "8E 70 2C A1 A2 A3 00 00 00 00 00 00 00 00 00 00"
  "60 00 00 00 00 00 00 00 00 00 00 00 00 01 FF FF"
  "FF 5F FF FF FE 7F FF 02 B1 FC 60 FA 8A 36 86",
  NULL
};

/*
 *  END OF USER DATA
 */

static int
decode_hexa(uint8_t *dst, int len, const char *src)
{
  int res = 0, i;

  while (1) {
    while (*src && !isxdigit(*src))
      src++;
    if (*src == '\0')
      break;
    if (!isxdigit(src[1]))
      break;
    if (sscanf(src, "%02x", &i) > 0) {
      *dst = i;
      dst++;
      res++;
      src += 2;
      if (res >= len)
        break;
    } else {
      break;
    }
  }

  return res;
}

static void
emm_result(void *aux, const uint8_t *radata, int ralen, void *mux)
{
  tvhlog_hexdump("emm-*send*", radata, ralen);
}

int main(int argc, char *argv[])
{
  uint8_t emm[1024];
  emm_reass_t ra;
  char **p;
  uint8_t *sa;
  int l;

  emm_reass_init(&ra, CAID);
  decode_hexa(ra.ua, sizeof(ra.ua), UA);
  ra.providers_count = 1;
  ra.providers = calloc(ra.providers_count, sizeof(emm_provider_t));
  ra.providers->id = PROVID;
  decode_hexa(ra.providers->sa, sizeof(ra.providers->sa), SA);

  tvhinfo("config", "CAID = %04x", CAID);
  tvhinfo("config", "PROVID = %06x", PROVID);
  tvhinfo("config", "UA = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
          ra.ua[0], ra.ua[1], ra.ua[2], ra.ua[3],
          ra.ua[4], ra.ua[5], ra.ua[6], ra.ua[7]);
  sa = ra.providers->sa;
  tvhinfo("config", "SA = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
          sa[0], sa[1], sa[2], sa[3],
          sa[4], sa[5], sa[6], sa[7]);

  for (p = emms; *p; p++) {
    l = decode_hexa(emm, sizeof(emm), *p);
    emm_filter(&ra, emm, l, (void *)1, emm_result, NULL);
  }

  emm_reass_done(&ra);
  return 0;
}

#endif
