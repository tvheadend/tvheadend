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

  /* TS muxing */
  uint8_t  pm_rewrite_sdt;
  uint8_t  pm_rewrite_eit;

  uint16_t pm_pmt_pid;
  uint16_t pm_src_sid;
  uint16_t pm_dst_sid;

  mpegts_psi_table_t pm_pat;
  mpegts_psi_table_t pm_pmt;
  mpegts_psi_table_t pm_sdt;
  mpegts_psi_table_t pm_eit;

} pass_muxer_t;


static void
pass_muxer_write(muxer_t *m, const void *data, size_t size);

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
  out[7] = 0;

  out[8] = (pm->pm_dst_sid & 0xff00) >> 8;
  out[9] = pm->pm_dst_sid & 0x00ff;
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
      if (ssc->ssc_pid == pid)
        break;
    }
    if (i < pm->pm_ss->ss_num_components) {
      memcpy(out + ol, buf, 5 + l);
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
  while (len >= 5) {
    sid = (buf[0] << 8) | buf[1];
    l = (buf[3] & 0x0f) << 8 | buf[4];
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
pass_muxer_eit_cb(mpegts_psi_table_t *mt, const uint8_t *buf, int len)
{
  pass_muxer_t *pm;
  uint16_t sid;
  uint8_t *sbuf, *out;
  int olen;

  /* filter out the other transponders */
  if ((buf[0] < 0x50 && buf[0] != 0x4e) || buf[0] > 0x5f || len < 14)
    return;

  pm = (pass_muxer_t*)mt->mt_opaque;
  sid = (buf[3] << 8) | buf[4];
  if (sid != pm->pm_src_sid)
    return;

  /* TODO: set free_CA_mode bit to zero */

  sbuf = malloc(len + 4);
  memcpy(sbuf, buf, len);
  sbuf[3] = pm->pm_dst_sid >> 8;
  sbuf[4] = pm->pm_dst_sid & 0xff;

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

    has_video |= SCT_ISVIDEO(ssc->ssc_type);
    has_audio |= SCT_ISAUDIO(ssc->ssc_type);
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
  if (pm->m_config.u.pass.m_rewrite_sid > 0)
    pm->pm_dst_sid   = pm->m_config.u.pass.m_rewrite_sid;
  else
    pm->pm_dst_sid   = ss->ss_service_id;
  pm->pm_pmt_pid     = ss->ss_pmt_pid;
  pm->pm_rewrite_sdt = !!pm->m_config.u.pass.m_rewrite_sdt;
  pm->pm_rewrite_eit = !!pm->m_config.u.pass.m_rewrite_eit;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    if (!SCT_ISVIDEO(ssc->ssc_type) && !SCT_ISAUDIO(ssc->ssc_type))
      continue;
    if (ssc->ssc_pid == DVB_SDT_PID && pm->pm_rewrite_sdt) {
      tvhwarn(LS_PASS, "SDT PID shared with A/V, rewrite disabled");
      pm->pm_rewrite_sdt = 0;
    }
    if (ssc->ssc_pid == DVB_EIT_PID && pm->pm_rewrite_eit) {
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

  if(pm->pm_error) {
    pm->m_errors++;
  } else if(tvh_write(pm->pm_fd, data, size)) {
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
static void
pass_muxer_write_ts(muxer_t *m, pktbuf_t *pb)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  int l, pid;
  uint8_t *tsb, *pkt = pktbuf_ptr(pb);
  size_t  len = pktbuf_len(pb), len2;
  
  /* Rewrite PAT/PMT in operation */
  if (pm->m_config.u.pass.m_rewrite_pat || pm->m_config.u.pass.m_rewrite_pmt ||
      pm->pm_rewrite_sdt || pm->pm_rewrite_eit) {

    for (tsb = pktbuf_ptr(pb), len2 = pktbuf_len(pb), len = 0;
         len2 > 0; tsb += l, len2 -= l) {

      pid = (tsb[1] & 0x1f) << 8 | tsb[2];
      l = mpegts_word_count(tsb, len2, 0x001FFF00);

      /* Process */
      if ( (pm->m_config.u.pass.m_rewrite_pat && pid == DVB_PAT_PID) ||
           (pm->m_config.u.pass.m_rewrite_pmt && pid == pm->pm_pmt_pid) ||
           (pm->pm_rewrite_sdt && pid == DVB_SDT_PID) ||
           (pm->pm_rewrite_eit && pid == DVB_EIT_PID) ) {

        /* Flush */
        if (len)
          pass_muxer_write(m, pkt, len);

        /* Store new start point (after these packets) */
        pkt = tsb + l;
        len = 0;

        /* PAT */
        if (pid == DVB_PAT_PID) {

          dvb_table_parse(&pm->pm_pat, "-", tsb, l, 1, 0, pass_muxer_pat_cb);

        /* SDT */
        } else if (pid == DVB_SDT_PID) {
        
          dvb_table_parse(&pm->pm_sdt, "-", tsb, l, 1, 0, pass_muxer_sdt_cb);

        /* EIT */
        } else if (pid == DVB_EIT_PID) {
        
          dvb_table_parse(&pm->pm_eit, "-", tsb, l, 1, 0, pass_muxer_eit_cb);

        /* PMT */
        } else {

          dvb_table_parse(&pm->pm_pmt, "-", tsb, l, 1, 0, pass_muxer_pmt_cb);

        }

      /* Record */
      } else {
        len += l;
      }
    }
  }

  if (len)
    pass_muxer_write(m, pkt, len);
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

  dvb_table_parse_done(&pm->pm_pat);
  dvb_table_parse_done(&pm->pm_pmt);
  dvb_table_parse_done(&pm->pm_sdt);
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
  dvb_table_parse_init(&pm->pm_eit, "pass-eit", LS_TBL_PASS, DVB_EIT_PID,
                       0, 0, pm);

  return (muxer_t *)pm;
}

