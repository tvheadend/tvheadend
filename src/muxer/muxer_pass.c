/*
 *  tvheadend, simple muxer that just passes the input along
 *  Copyright (C) 2012 John Törnblom
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "service.h"
#include "input/mpegts/dvb.h"
#include "muxer_pass.h"
#include "spawn.h"

#define PASS_PID_COUNT 8192

typedef struct pass_muxer {
  muxer_t;

  /* File descriptor stuff */
  off_t pm_off;
  int   pm_fd;
  int   pm_ofd;
  int   pm_seekable;
  int   pm_error;
  int   pm_spawn_pid;

  /* Filename is also used for logging */
  char *pm_filename;

  /* Streaming components */
  streaming_start_t *pm_ss;
  uint16_t          *pm_pid_map;
  int8_t            *pm_pid_cc;
  uint8_t           *pm_pid_rewrite_cc;
  uint8_t           *pm_pid_buf;
  size_t             pm_pid_buf_size;
  uint8_t            pm_pid_active;

  /* TS muxing */
  uint8_t  pm_rewrite_sdt;
  uint8_t  pm_rewrite_nit;
  uint8_t  pm_rewrite_eit;

  uint16_t pm_pmt_pid;
  uint16_t pm_src_sid;
  uint16_t pm_dst_sid;
  uint16_t pm_src_tsid;
  uint16_t pm_dst_tsid;
  uint16_t pm_src_onid;
  uint16_t pm_dst_onid;

  mpegts_psi_table_t pm_pat;
  mpegts_psi_table_t pm_pmt;
  mpegts_psi_table_t pm_sdt;
  mpegts_psi_table_t pm_nit;
  mpegts_psi_table_t pm_eit;

} pass_muxer_t;


static void
pass_muxer_write(muxer_t *m, const void *data, size_t size);

static uint16_t
pass_muxer_map_pid(const pass_muxer_t *pm, uint16_t pid)
{
  if (pm->pm_pid_map == NULL || pid >= PASS_PID_COUNT)
    return pid;
  return pm->pm_pid_map[pid];
}

static int
pass_muxer_component_match(const streaming_start_component_t *old,
                           const streaming_start_component_t *new)
{
  int score;

  if (old->es_type != new->es_type)
    return -1;

  score = old->es_index == new->es_index ? 8 : 0;
  if (!memcmp(old->es_lang, new->es_lang, sizeof(old->es_lang)))
    score += 4;
  if (old->es_audio_type == new->es_audio_type)
    score += 2;
  if (old->es_parent_pid == new->es_parent_pid)
    score++;
  return score;
}

static int
pass_muxer_stable_component(const streaming_start_component_t *ssc)
{
  return ssc->es_pid >= 0 && ssc->es_pid < PASS_PID_COUNT &&
         (SCT_ISAV(ssc->es_type) || ssc->es_type == SCT_TELETEXT ||
          ssc->es_type == SCT_DVBSUB || ssc->es_type == SCT_PCR);
}

static int
pass_muxer_pid_conflict(const streaming_start_t *ss,
                        const streaming_start_component_t *ssc,
                        uint16_t output_pid)
{
  int i;

  if (output_pid == ssc->es_pid)
    return 0;
  if (output_pid == DVB_PAT_PID || output_pid == ss->ss_pmt_pid ||
      output_pid == ss->ss_pcr_pid)
    return 1;
  for (i = 0; i < ss->ss_num_components; i++)
    if (&ss->ss_components[i] != ssc &&
        ss->ss_components[i].es_pid == output_pid)
      return 1;
  return 0;
}

static uint16_t *
pass_muxer_pid_map_create(void)
{
  uint16_t *map;
  int i;

  map = malloc(sizeof(*map) * PASS_PID_COUNT);
  if (map == NULL)
    return NULL;
  for (i = 0; i < PASS_PID_COUNT; i++)
    map[i] = i;
  return map;
}

static int
pass_muxer_pid_state_init(pass_muxer_t *pm, uint16_t *map)
{
  int8_t *cc;
  uint8_t *rewrite_cc;

  cc = malloc(PASS_PID_COUNT);
  rewrite_cc = calloc(PASS_PID_COUNT, sizeof(*rewrite_cc));
  if (cc == NULL || rewrite_cc == NULL) {
    free(cc);
    free(rewrite_cc);
    return -1;
  }
  memset(cc, -1, PASS_PID_COUNT);
  pm->pm_pid_map = map;
  pm->pm_pid_cc = cc;
  pm->pm_pid_rewrite_cc = rewrite_cc;
  return 0;
}

static int
pass_muxer_best_component(const pass_muxer_t *pm,
                          const streaming_start_component_t *new,
                          const uint8_t *used)
{
  const streaming_start_component_t *old;
  int best;
  int best_score;
  int i;
  int score;

  best = -1;
  best_score = -1;
  for (i = 0; i < pm->pm_ss->ss_num_components; i++) {
    old = &pm->pm_ss->ss_components[i];
    if (used[i] || !pass_muxer_stable_component(old))
      continue;
    score = pass_muxer_component_match(old, new);
    if (score > best_score) {
      best = i;
      best_score = score;
    }
  }
  return best;
}

static void
pass_muxer_map_component(pass_muxer_t *pm, const streaming_start_t *ss,
                         const streaming_start_component_t *new,
                         uint16_t *map, uint8_t *used, uint8_t *claimed)
{
  const streaming_start_component_t *old;
  int best;
  uint16_t output_pid;

  if (!pass_muxer_stable_component(new))
    return;
  best = pass_muxer_best_component(pm, new, used);
  if (best < 0)
    return;

  old = &pm->pm_ss->ss_components[best];
  output_pid = pass_muxer_map_pid(pm, old->es_pid);
  if (output_pid >= PASS_PID_COUNT || claimed[output_pid] ||
      pass_muxer_pid_conflict(ss, new, output_pid))
    return;

  used[best] = 1;
  claimed[output_pid] = 1;
  map[new->es_pid] = output_pid;
  if (new->es_pid == output_pid)
    return;

  pm->pm_pid_active = 1;
  pm->pm_pid_rewrite_cc[output_pid] = 1;
  tvhdebug(LS_PASS, "%s: remap input PID %d (%s) to stable PID %d",
           pm->pm_filename ?: "Pass muxer", new->es_pid,
           streaming_component_type2txt(new->es_type), output_pid);
}

static void
pass_muxer_update_pid_map(pass_muxer_t *pm, const streaming_start_t *ss)
{
  uint16_t *map;
  uint8_t *used;
  uint8_t claimed[PASS_PID_COUNT] = { 0 };
  int i;

  if (!pm->pm_seekable || !pm->m_config.u.pass.m_rewrite_pmt)
    return;

  map = pass_muxer_pid_map_create();
  if (map == NULL)
    return;

  if (pm->pm_pid_map == NULL) {
    if (pass_muxer_pid_state_init(pm, map))
      free(map);
    return;
  }

  if (pm->pm_ss == NULL) {
    free(pm->pm_pid_map);
    pm->pm_pid_map = map;
    return;
  }

  used = calloc(pm->pm_ss->ss_num_components, sizeof(*used));
  if (used == NULL) {
    free(map);
    return;
  }
  for (i = 0; i < ss->ss_num_components; i++)
    pass_muxer_map_component(pm, ss, &ss->ss_components[i], map, used, claimed);

  free(used);
  free(pm->pm_pid_map);
  pm->pm_pid_map = map;
}

static void
pass_muxer_track_cc(pass_muxer_t *pm, const uint8_t *tsb, int len, uint16_t pid)
{
  if (pm->pm_pid_cc == NULL || pid >= PASS_PID_COUNT)
    return;
  while (len >= 188) {
    pm->pm_pid_cc[pid] = tsb[3] & 0x0f;
    tsb += 188;
    len -= 188;
  }
}

static void
pass_muxer_remap_ts(pass_muxer_t *pm, uint8_t *pkt, int len,
                    uint16_t output_pid)
{
  int adaptation_control;
  int cc;
  int left;

  for (left = len; left >= 188; pkt += 188, left -= 188) {
    adaptation_control = (pkt[3] >> 4) & 0x03;
    cc = pkt[3] & 0x0f;
    if (pm->pm_pid_cc && pm->pm_pid_cc[output_pid] >= 0) {
      cc = pm->pm_pid_cc[output_pid];
      if (adaptation_control == 1 || adaptation_control == 3)
        cc = (cc + 1) & 0x0f;
      pkt[3] = (pkt[3] & 0xf0) | cc;
    }
    pkt[1] = (pkt[1] & 0xe0) | (output_pid >> 8);
    pkt[2] = output_pid & 0xff;
    if (pm->pm_pid_cc)
      pm->pm_pid_cc[output_pid] = cc;
  }
}

/*
 * Rewrite a PAT packet to only include the service included in the transport stream.
 */
static void
pass_muxer_pat_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint8_t out[16], *ob;
  int ol, l;

  if (buf[0])
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;

  memcpy(out, buf, 5 + 3);

  out[1] = 0x80;
  out[2] = 13; /* section_length (number of bytes after this field, including CRC) */
  out[3] = pm->pm_dst_tsid >> 8;
  out[4] = pm->pm_dst_tsid;
  out[7] = 0;

  out[8] = pm->pm_dst_sid >> 8;
  out[9] = pm->pm_dst_sid;
  out[10] = 0xe0 | ((pm->pm_pmt_pid & 0x1f00) >> 8);
  out[11] = pm->pm_pmt_pid & 0x00ff;

  ol = dvb_table_append_crc32(out, 12, sizeof(out));

  if (ol > 0 && (l = dvb_table_remux(mt, out, ol, &ob)) > 0) {
    pass_muxer_write((muxer_t *)pm, ob, l);
    free(ob);
  }
}

/*
 *
 */
static void
pass_muxer_pmt_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint8_t out[1024], *ob;
  uint16_t sid, pid;
  int l, ol, i;
  const streaming_start_component_t *ssc;

  memcpy(out, buf, ol = 3);
  buf += ol;
  len -= ol;

  sid = (buf[0] << 8) | buf[1];
  l = (buf[7] & 0x0f) << 8 | buf[8];

  if (l > len - 9)
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;
  if (sid != pm->pm_src_sid)
    return;

  out[ol + 0] = pm->pm_dst_sid >> 8;
  out[ol + 1] = pm->pm_dst_sid & 0xff;
  memcpy(out + ol + 2, buf + 2, 7);
  pid = (buf[5] & 0x1f) << 8 | buf[6];
  pid = pass_muxer_map_pid(pm, pid);
  out[ol + 5] = (out[ol + 5] & 0xe0) | (pid >> 8);
  out[ol + 6] = pid & 0xff;

  ol  += 9;     /* skip common descriptors */
  buf += 9 + l;
  len -= 9 + l;

  /* no common descriptors */
  out[7+3] &= 0xf0;
  out[8+3] = 0;

  while (len >= 5) {
    pid = (buf[1] & 0x1f) << 8 | buf[2];
    l   = (buf[3] & 0xf) << 8 | buf[4];

    if (l > len - 5)
      return;

    if (sizeof(out) < ol + l + 5 + 4 /* crc */) {
      tvherror(LS_PASS, "PMT entry too long (%i)", l);
      return;
    }

    for (i = 0; i < pm->pm_ss->ss_num_components; i++) {
      ssc = &pm->pm_ss->ss_components[i];
      if (ssc->es_pid == pid)
        break;
    }
    if (i < pm->pm_ss->ss_num_components) {
      memcpy(out + ol, buf, 5 + l);
      pid = pass_muxer_map_pid(pm, pid);
      out[ol + 1] = (out[ol + 1] & 0xe0) | (pid >> 8);
      out[ol + 2] = pid & 0xff;
      ol += 5 + l;
    }

    buf += 5 + l;
    len -= 5 + l;
  }

  /* update section length */
  out[1] = (out[1] & 0xf0) | ((ol + 4 - 3) >> 8);
  out[2] = (ol + 4 - 3) & 0xff;

  ol = dvb_table_append_crc32(out, ol, sizeof(out));

  if (ol > 0 && (l = dvb_table_remux(mt, out, ol, &ob)) > 0) {
    pass_muxer_write((muxer_t *)pm, ob, l);
    free(ob);
  }
}

/*
 *
 */
static void
pass_muxer_sdt_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint8_t out[1024], *ob;
  uint16_t sid;
  int l, ol;

  /* filter out the other transponders */
  if (buf[0] != 0x42)
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;
  ol   = 8 + 3;
  memcpy(out, buf, ol);
  buf += ol;
  len -= ol;

  out[3] = pm->pm_dst_tsid >> 8;
  out[4] = pm->pm_dst_tsid;
  out[8] = pm->pm_dst_onid >> 8;
  out[9] = pm->pm_dst_onid;

  while (len >= 5) {
    sid = (buf[0] << 8) | buf[1];
    l = (buf[3] & 0x0f) << 8 | buf[4];
    if (l > len - 5)
      return;
    if (sid != pm->pm_src_sid) {
      buf += l + 5;
      len -= l + 5;
      continue;
    }
    if (sizeof(out) < ol + l + 5 + 4 /* crc */) {
      tvherror(LS_PASS, "SDT entry too long (%i)", l);
      return;
    }
    out[ol + 0] = pm->pm_dst_sid >> 8;
    out[ol + 1] = pm->pm_dst_sid & 0xff;
    memcpy(out + ol + 2, buf + 2, l + 3);
    /* set free CA */
    out[ol + 3] = out[ol + 3] & ~0x10;
    ol += l + 5;
    break;
  }

  /* update section length */
  out[1] = (out[1] & 0xf0) | ((ol + 4 - 3) >> 8);
  out[2] = (ol + 4 - 3) & 0xff;

  ol = dvb_table_append_crc32(out, ol, sizeof(out));

  if (ol > 0 && (l = dvb_table_remux(mt, out, ol, &ob)) > 0) {
    pass_muxer_write((muxer_t *)pm, ob, l);
    free(ob);
  }
}

/*
 *
 */
static void
pass_muxer_nit_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint8_t out[4096], *ob, dtag;
  int l, ol, lptr, dlen;

  /* filter out the other networks */
  if (buf[0] != 0x40)
    return;

  if (len < 10)
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;

  memcpy(out, buf, ol = 10);
  l = (buf[8] & 0x0f) << 8 | buf[9];
  buf += 10;
  len -= 10;

  if (pm->m_config.u.pass.m_rewrite_sid > 0) {
    out[3] = 0;
    out[4] = 1;
  }

  while (l > 1 && len > 1) {
    dtag = buf[0];
    dlen = buf[1];
    if (dtag == DVB_DESC_PRIVATE_DATA) {
      if (sizeof(out) - 32 < ol) {
        tvherror(LS_PASS, "NIT entry too long (%i)", ol);
        return;
      }
      memcpy(out + ol, buf, dlen + 2);
      ol += dlen + 2;
    }
    dlen += 2;
    buf += dlen;
    len -= dlen;
      l -= dlen;
  }
  out[8] &= 0xf0;
  out[8] |= ((ol - 10) >> 8) & 0x0f;
  out[9] = (ol - 10) & 0xff;

  if (sizeof(out) - 32 < ol) {
    tvherror(LS_PASS, "NIT entry too long (%i)", ol);
    return;
  }

  /* mux info length */
  lptr = ol;
  ol += 2;

  /* mux info */
  out[ol++] = pm->pm_dst_tsid >> 8;
  out[ol++] = pm->pm_dst_tsid;
  out[ol++] = pm->pm_dst_onid >> 8;
  out[ol++] = pm->pm_dst_onid;
  /* mux tags */
  out[ol++] = 0xf0;
  out[ol++] = 5;
  out[ol++] = DVB_DESC_SERVICE_LIST;
  out[ol++] = 3;
  out[ol++] = pm->pm_dst_sid >> 8;
  out[ol++] = pm->pm_dst_sid;
  out[ol++] = 0x11;
  /* update section length */
  out[lptr+0] = 0xf0 | (ol - lptr - 2) >> 8;
  out[lptr+1] = (ol - lptr - 2) & 0xff;

  /* update section length */
  out[1] = (out[1] & 0xf0) | ((ol + 4 - 3) >> 8);
  out[2] = (ol + 4 - 3) & 0xff;

  ol = dvb_table_append_crc32(out, ol, sizeof(out));

  if (ol > 0 && (l = dvb_table_remux(mt, out, ol, &ob)) > 0) {
    pass_muxer_write((muxer_t *)pm, ob, l);
    free(ob);
  }
}

/*
 *
 */
static void
pass_muxer_eit_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint16_t sid;
  uint8_t *sbuf, *out;
  int olen;

  /* filter out wrong tables */
  if (buf[0] < 0x4e || buf[0] > 0x6f || len < 14)
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;
  sid = (buf[3] << 8) | buf[4];

  /* TODO: set free_CA_mode bit to zero */

  sbuf = malloc(len + 4);
  memcpy(sbuf, buf, len);
  sbuf[3] = pm->pm_dst_sid >> 8;
  sbuf[4] = pm->pm_dst_sid;
  sbuf[8] = pm->pm_dst_tsid >> 8;
  sbuf[9] = pm->pm_dst_tsid;
  sbuf[10] = pm->pm_dst_onid >> 8;
  sbuf[11] = pm->pm_dst_onid;

  if (sid != pm->pm_src_sid) {
    len = 14; /* no events, just keep the SI tables consistent */
    sbuf[1] &= 0xf0;
    sbuf[1] |= ((len - 3 + 4) >> 8) & 0x0f;
    sbuf[2] = (len - 3 + 4) & 0xff;
  }

  len = dvb_table_append_crc32(sbuf, len, len + 4);
  if (len > 0 && (olen = dvb_table_remux(mt, sbuf, len, &out)) > 0) {
    pass_muxer_write((muxer_t *)pm, out, olen);
    free(out);
  }

  free(sbuf);
}

/**
 * Figure out the mime-type for the muxed data stream
 */
static const char*
pass_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  int i;
  int has_audio;
  int has_video;
  muxer_container_type_t mc;
  const streaming_start_component_t *ssc;
  const source_info_t *si = &ss->ss_si;
  const char *mime = m->m_config.u.pass.m_mime;

  if (mime && mime[0])
    return mime;

  has_audio = 0;
  has_video = 0;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    has_video |= SCT_ISVIDEO(ssc->es_type);
    has_audio |= SCT_ISAUDIO(ssc->es_type);
  }

  if(si->si_type == S_MPEG_TS)
    mc = MC_MPEGTS;
  else if(si->si_type == S_MPEG_PS)
    mc = MC_MPEGPS;
  else
    mc = MC_UNKNOWN;

  if(has_video)
    return muxer_container_type2mime(mc, 1);
  else if(has_audio)
    return muxer_container_type2mime(mc, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}


/**
 * Generate the pmt and pat from a streaming start message
 */
static int
pass_muxer_reconfigure(muxer_t* m, const struct streaming_start *ss)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  const streaming_start_component_t *ssc;
  int i;

  pm->pm_src_sid     = ss->ss_service_id;
  pm->pm_src_tsid    = ss->ss_si.si_tsid;
  pm->pm_src_onid    = ss->ss_si.si_onid;
  if (pm->m_config.u.pass.m_rewrite_sid > 0) {
    pm->pm_dst_sid   = pm->m_config.u.pass.m_rewrite_sid;
    pm->pm_dst_tsid  = 1;
    pm->pm_dst_onid  = 1;
  } else {
    pm->pm_dst_sid   = pm->pm_src_sid;
    pm->pm_dst_tsid  = pm->pm_src_tsid;
    pm->pm_dst_onid  = pm->pm_src_onid;
  }
  pm->pm_pmt_pid     = ss->ss_pmt_pid;
  pm->pm_rewrite_sdt = !!pm->m_config.u.pass.m_rewrite_sdt;
  pm->pm_rewrite_nit = !!pm->m_config.u.pass.m_rewrite_nit;
  pm->pm_rewrite_eit = !!pm->m_config.u.pass.m_rewrite_eit;

  pass_muxer_update_pid_map(pm, ss);

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    if (!SCT_ISVIDEO(ssc->es_type) && !SCT_ISAUDIO(ssc->es_type))
      continue;
    if (ssc->es_pid == DVB_SDT_PID && pm->pm_rewrite_sdt) {
      tvhwarn(LS_PASS, "SDT PID shared with A/V, rewrite disabled");
      pm->pm_rewrite_sdt = 0;
    }
    if (ssc->es_pid == DVB_NIT_PID && pm->pm_rewrite_nit) {
      tvhwarn(LS_PASS, "NIT PID shared with A/V, rewrite disabled");
      pm->pm_rewrite_nit = 0;
    }
    if (ssc->es_pid == DVB_EIT_PID && pm->pm_rewrite_eit) {
      tvhwarn(LS_PASS, "EIT PID shared with A/V, rewrite disabled");
      pm->pm_rewrite_eit = 0;
    }
  }


  if (pm->m_config.u.pass.m_rewrite_pmt) {

    if (pm->pm_ss)
      streaming_start_unref(pm->pm_ss);
    pm->pm_ss = streaming_start_copy(ss);

    dvb_table_parse_done(&pm->pm_pmt);
    dvb_table_parse_init(&pm->pm_pmt, "pass-pmt", LS_TBL_PASS, pm->pm_pmt_pid,
                         DVB_PMT_BASE, DVB_PMT_MASK, pm);
  }

  return 0;
}


/**
 * Init the passthrough muxer with streams
 */
static int
pass_muxer_init(muxer_t* m, struct streaming_start *ss, const char *name)
{
  return pass_muxer_reconfigure(m, ss);
}

/**
 * Open the spawned task on demand
 */
static int
pass_muxer_open2(pass_muxer_t *pm)
{
  const char *cmdline = pm->m_config.u.pass.m_cmdline;
  char **argv = NULL;

  pm->pm_spawn_pid = -1;
  if (cmdline && cmdline[0]) {
    argv = NULL;
    if (spawn_parse_args(&argv, 64, cmdline, NULL))
      goto error;
    if (spawn_with_passthrough(argv[0], argv, NULL, pm->pm_ofd, &pm->pm_fd, &pm->pm_spawn_pid, 1)) {
      tvherror(LS_PASS, "Unable to start pipe '%s' (wrong executable?)", cmdline);
      goto error;
    }
    spawn_free_args(argv);
  } else {
    pm->pm_fd = pm->pm_ofd;
  }
  return 0;

error:
  if (argv)
    spawn_free_args(argv);
  pm->pm_error = ENOMEM;
  return -1;
}

/**
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
pass_muxer_open_stream(muxer_t *m, int fd)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  pm->pm_off      = 0;
  pm->pm_ofd      = fd;
  pm->pm_seekable = 0;
  pm->pm_filename = strdup("Live stream");

  return pass_muxer_open2(pm);
}


/**
 * Open the file and set the file descriptor
 */
static int
pass_muxer_open_file(muxer_t *m, const char *filename)
{
  int fd;
  pass_muxer_t *pm = (pass_muxer_t*)m;

  tvhtrace(LS_PASS, "Creating file \"%s\" with file permissions \"%o\"", filename, pm->m_config.m_file_permissions);
 
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, pm->m_config.m_file_permissions);

  if(fd < 0) {
    pm->pm_error = errno;
    tvherror(LS_PASS, "%s: Unable to create file, open failed -- %s",
	     filename, strerror(errno));
    pm->m_errors++;
    return -1;
  }

  /* bypass umask settings */
  if (fchmod(fd, pm->m_config.m_file_permissions))
    tvherror(LS_PASS, "%s: Unable to change permissions -- %s",
             filename, strerror(errno));

  pm->pm_off      = 0;
  pm->pm_seekable = 1;
  pm->pm_ofd      = fd;
  pm->pm_filename = strdup(filename);

  return pass_muxer_open2(pm);
}


/**
 * Write data to the file descriptor
 */
static void
pass_muxer_write(muxer_t *m, const void *data, size_t size)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  int ret;

  if(pm->pm_error) {
    pm->m_errors++;
    return;
  } 
  
  if (pm->m_config.m_output_chunk > 0) {
    ret = tvh_write_in_chunks(pm->pm_fd, data, size, pm->m_config.m_output_chunk);
  } else {
    ret = tvh_write(pm->pm_fd, data, size);
  }

  if(ret) {
    pm->pm_error = errno;
    if (!MC_IS_EOS_ERROR(errno))
      tvherror(LS_PASS, "%s: Write failed -- %s", pm->pm_filename,
	       strerror(errno));
    else
      /* this is an end-of-streaming notification */
      m->m_eos = 1;
    m->m_errors++;
    if (pm->pm_seekable) {
      muxer_cache_update(m, pm->pm_fd, pm->pm_off, 0);
      pm->pm_off = lseek(pm->pm_fd, 0, SEEK_CUR);
    }
  } else {
    if (pm->pm_seekable)
      muxer_cache_update(m, pm->pm_fd, pm->pm_off, 0);
    pm->pm_off += size;
  }
}


/**
 * Write TS packets to the file descriptor
 */
static int
pass_muxer_rewrite_enabled(const pass_muxer_t *pm)
{
  return pm->m_config.u.pass.m_rewrite_pat ||
         pm->m_config.u.pass.m_rewrite_pmt ||
         pm->pm_rewrite_sdt || pm->pm_rewrite_nit || pm->pm_rewrite_eit;
}

static int
pass_muxer_output_buffer(pass_muxer_t *pm, const uint8_t *src, size_t len)
{
  uint8_t *buf;

  if (!pm->pm_pid_active)
    return 0;
  if (pm->pm_pid_buf_size < len) {
    buf = realloc(pm->pm_pid_buf, len);
    if (buf == NULL) {
      pm->pm_error = ENOMEM;
      pm->m_errors++;
      return -1;
    }
    pm->pm_pid_buf = buf;
    pm->pm_pid_buf_size = len;
  }
  memcpy(pm->pm_pid_buf, src, len);
  return 0;
}

static int
pass_muxer_rewrite_pid(const pass_muxer_t *pm, int pid)
{
  return (pm->m_config.u.pass.m_rewrite_pat && pid == DVB_PAT_PID) ||
         (pm->m_config.u.pass.m_rewrite_pmt && pid == pm->pm_pmt_pid) ||
         (pm->pm_rewrite_sdt && pid == DVB_SDT_PID) ||
         (pm->pm_rewrite_nit && pid == DVB_NIT_PID) ||
         (pm->pm_rewrite_eit && pid == DVB_EIT_PID);
}

static void
pass_muxer_parse_tables(pass_muxer_t *pm, const uint8_t *src, int len, int pid)
{
  if (pid == DVB_PAT_PID)
    dvb_table_parse(&pm->pm_pat, "-", src, len, 1, 0, pass_muxer_pat_cb);
  else if (pid == DVB_SDT_PID)
    dvb_table_parse(&pm->pm_sdt, "-", src, len, 1, 0, pass_muxer_sdt_cb);
  else if (pid == DVB_NIT_PID)
    dvb_table_parse(&pm->pm_nit, "-", src, len, 1, 0, pass_muxer_nit_cb);
  else if (pid == DVB_EIT_PID)
    dvb_table_parse(&pm->pm_eit, "-", src, len, 1, 0, pass_muxer_eit_cb);

  if (pid == pm->pm_pmt_pid)
    dvb_table_parse(&pm->pm_pmt, "-", src, len, 1, 0, pass_muxer_pmt_cb);
}

static void
pass_muxer_flush_table(muxer_t *m, pass_muxer_t *pm, const uint8_t *pkt,
                       size_t write_len, const uint8_t *src, int len, int pid)
{
  if (write_len)
    pass_muxer_write(m, pkt, write_len);
  pass_muxer_parse_tables(pm, src, len, pid);
}

static void
pass_muxer_process_payload(pass_muxer_t *pm, const uint8_t *src, size_t offset,
                           int len, int pid)
{
  uint16_t output_pid;

  output_pid = pass_muxer_map_pid(pm, pid);
  if (output_pid != pid ||
      (pm->pm_pid_rewrite_cc && pm->pm_pid_rewrite_cc[output_pid]))
    pass_muxer_remap_ts(pm, pm->pm_pid_buf + offset, len, output_pid);
  else
    pass_muxer_track_cc(pm, src, len, output_pid);
}

static void
pass_muxer_write_ts(muxer_t *m, pktbuf_t *pb)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  int len;
  int pid;
  const uint8_t *out;
  const uint8_t *pkt;
  const uint8_t *src;
  size_t left;
  size_t offset;
  size_t write_len;

  src = pktbuf_ptr(pb);
  pkt = src;
  write_len = pktbuf_len(pb);
  if (pass_muxer_rewrite_enabled(pm)) {
    if (pass_muxer_output_buffer(pm, src, write_len))
      return;

    out = pm->pm_pid_active ? pm->pm_pid_buf : src;
    pkt = out;
    left = write_len;
    offset = 0;
    write_len = 0;
    while (left > 0) {
      pid = (src[1] & 0x1f) << 8 | src[2];
      len = mpegts_word_count(src, left, 0x001FFF00);
      if (pass_muxer_rewrite_pid(pm, pid)) {
        pass_muxer_flush_table(m, pm, pkt, write_len, src, len, pid);
        pkt = out + len;
        write_len = 0;
      } else {
        pass_muxer_process_payload(pm, src, offset, len, pid);
        write_len += len;
      }
      src += len;
      out += len;
      offset += len;
      left -= len;
    }
  }

  if (write_len)
    pass_muxer_write(m, pkt, write_len);
}


/**
 * Write a packet directly to the file descriptor
 */
static int
pass_muxer_write_pkt(muxer_t *m, streaming_message_type_t smt, void *data)
{
  pktbuf_t *pb = (pktbuf_t*)data;
  pass_muxer_t *pm = (pass_muxer_t*)m;

  assert(smt == SMT_MPEGTS);

  switch(smt) {
  case SMT_MPEGTS:
    pass_muxer_write_ts(m, pb);
    break;
  default:
    //TODO: add support for v4l (MPEG-PS)
    break;
  }

  pktbuf_ref_dec(pb);

  return pm->pm_error;
}


/**
 * NOP
 */
static int
pass_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb, const char *comment)
{
  return 0;
}


/**
 * Close the file descriptor
 */
static int
pass_muxer_close(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_spawn_pid > 0)
    spawn_kill(pm->pm_spawn_pid, tvh_kill_to_sig(pm->m_config.u.pass.m_killsig),
               pm->m_config.u.pass.m_killtimeout);
  if(pm->pm_seekable && close(pm->pm_ofd)) {
    pm->pm_error = errno;
    tvherror(LS_PASS, "%s: Unable to close file, close failed -- %s",
	     pm->pm_filename, strerror(errno));
    pm->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Free all memory associated with the muxer
 */
static void
pass_muxer_destroy(muxer_t *m)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  if(pm->pm_filename)
    free(pm->pm_filename);

  if (pm->pm_ss)
    streaming_start_unref(pm->pm_ss);

  free(pm->pm_pid_map);
  free(pm->pm_pid_cc);
  free(pm->pm_pid_rewrite_cc);
  free(pm->pm_pid_buf);

  dvb_table_parse_done(&pm->pm_pat);
  dvb_table_parse_done(&pm->pm_pmt);
  dvb_table_parse_done(&pm->pm_sdt);
  dvb_table_parse_done(&pm->pm_nit);
  dvb_table_parse_done(&pm->pm_eit);

  muxer_config_free(&pm->m_config);
  muxer_hints_free(pm->m_hints);
  free(pm);
}


/**
 * Create a new passthrough muxer
 */
muxer_t*
pass_muxer_create(const muxer_config_t *m_cfg,
                  const muxer_hints_t *hints)
{
  pass_muxer_t *pm;

  if(m_cfg->m_type != MC_PASS && m_cfg->m_type != MC_RAW)
    return NULL;

  pm = calloc(1, sizeof(pass_muxer_t));
  pm->m_open_stream  = pass_muxer_open_stream;
  pm->m_open_file    = pass_muxer_open_file;
  pm->m_init         = pass_muxer_init;
  pm->m_reconfigure  = pass_muxer_reconfigure;
  pm->m_mime         = pass_muxer_mime;
  pm->m_write_meta   = pass_muxer_write_meta;
  pm->m_write_pkt    = pass_muxer_write_pkt;
  pm->m_close        = pass_muxer_close;
  pm->m_destroy      = pass_muxer_destroy;
  pm->pm_fd          = -1;
  pm->pm_ofd         = -1;
  pm->pm_spawn_pid   = -1;

  dvb_table_parse_init(&pm->pm_pat, "pass-pat", LS_TBL_PASS, DVB_PAT_PID,
                       DVB_PAT_BASE, DVB_PAT_MASK, pm);
  dvb_table_parse_init(&pm->pm_pmt, "pass-pmt", LS_TBL_PASS, 100,
                       DVB_PMT_BASE, DVB_PMT_MASK, pm);
  dvb_table_parse_init(&pm->pm_sdt, "pass-sdt", LS_TBL_PASS, DVB_SDT_PID,
                       DVB_SDT_BASE, DVB_SDT_MASK, pm);
  dvb_table_parse_init(&pm->pm_nit, "pass-nit", LS_TBL_PASS, DVB_NIT_PID,
                       DVB_NIT_BASE, DVB_NIT_MASK, pm);
  dvb_table_parse_init(&pm->pm_eit, "pass-eit", LS_TBL_PASS, DVB_EIT_PID,
                       0, 0, pm);

  if (m_cfg->u.pass.m_rewrite_sid > 0)
    pm->m_caps |= MC_CAP_ANOTHER_SERVICE;

  return (muxer_t *)pm;
}
