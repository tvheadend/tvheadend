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

#include "tvheadend.h"
#include "streaming.h"
#include "epg.h"
#include "service.h"
#include "input/mpegts/dvb.h"
#include "muxer_pass.h"
#include "dvr/dvr.h"

typedef struct pass_muxer {
  muxer_t;

  /* File descriptor stuff */
  int   pm_fd;
  int   pm_seekable;
  int   pm_error;

  /* Filename is also used for logging */
  char *pm_filename;

  /* TS muxing */
  uint8_t   pm_flags;
  uint8_t   pm_pat_cc;
  uint16_t  pm_pmt_pid;
  uint16_t  pm_service_id;
  uint32_t  pm_streams[256];  /* lookup table identifying which streams to include in the PMT */
} pass_muxer_t;

static inline void
set_pid_bit(uint32_t* pm_streams, uint16_t pid)
{
  pm_streams[pid >> 5] |= 1 << (pid & 31);
}

static inline int
check_pid_bit(uint32_t* pm_streams, uint16_t pid)
{
  return pm_streams[pid >> 5] & (1 << (pid & 31));
}

/*
 * Rewrite a PAT packet to only include the service included in the transport stream.
 *
 * This is complicated by the need to deal with PATs that span more than one transport
 * stream packet.  In that scenario, we replace the 2nd and subsequent PAT packets with
 * NULL packets (PID 0x1fff).
 *
 */

static int
pass_muxer_rewrite_pat(pass_muxer_t* pm, unsigned char* tsb)
{
  uint32_t crc32;
  int pusi = tsb[1]  & 0x40;

  /* NULL packet */
  if (!pusi) {
    printf("NULL PAT\n");
    tsb[1] = 0x1f;
    memset(tsb+2, 0xff, 186);
    return 0;
  }

  /* Ignore Next (TODO: should we wipe it?) */
  if (!(tsb[10] & 0x1)) {
    printf("NEXT PAT\n");
    return 0;
  }
    
  /* Some sanity checks */
  if (tsb[4]) {
    tvherror("pass", "Unsupported PAT format - pointer_to_data %d", tsb[4]);
    return 1;
  }
  if (tsb[12]) {
    tvherror("pass", "Multi-section PAT not supported");
    return 2;
  }

  /* Rewrite continuity counter, in case this is a multi-packet PAT (we discard all but the first packet) */
  tsb[3] = (tsb[3] & 0xf0) | pm->pm_pat_cc;
  pm->pm_pat_cc = (pm->pm_pat_cc + 1) & 0xf;

  tsb[6] = 0;
  tsb[7] = 13; /* section_length (number of bytes after this field, including CRC) */

  tsb[13] = (pm->pm_service_id & 0xff00) >> 8;
  tsb[14] = pm->pm_service_id & 0x00ff;
  tsb[15] = 0xe0 | ((pm->pm_pmt_pid & 0x1f00) >> 8);
  tsb[16] = pm->pm_pmt_pid & 0x00ff;

  crc32 = tvh_crc32(tsb+5, 12, 0xffffffff);
  tsb[17] = (crc32 & 0xff000000) >> 24;
  tsb[18] = (crc32 & 0x00ff0000) >> 16;
  tsb[19] = (crc32 & 0x0000ff00) >>  8;
  tsb[20] = crc32 & 0x000000ff;

  printf("rewrite PAT\n");

  memset(tsb + 21, 0xff, 167); /* Wipe rest of packet */

  return 0;
}

static int
pass_muxer_rewrite_pmt(pass_muxer_t* pm, unsigned char* tsb)
{
  int i, prog_len;
  uint32_t crc32;
  int pusi      = tsb[1] & 0x40;
  int sect_len  = ((tsb[6] & 0x0f) << 8) | tsb[7];
  tvhlog_hexdump("pass", tsb, 188);

  /* NULL packet */
  if (!pusi) {
    printf("NULL PMT\n");
    tsb[1] = 0x1f;
    memset(tsb+2, 0xff, 186);
    return 0;
  }
  
  /* Ignore Next (TODO: should we wipe it?) */
  if (!(tsb[10] & 0x1)) {
    printf("NEXT PMT\n");
    return 0;
  }
    
  /* Some sanity checks */
  if (tsb[4]) {
    tvherror("pass", "Unsupported PMT format - pointer_to_data %d", tsb[4]);
    return 1;
  }
  if (tsb[12]) {
    tvherror("pass", "Multi-section PMT not supported");
    return 2;
  }
  if (sect_len > (188 - 7 - 4)) {
    tvherror("pass", "Multi-packet PMT not supported - sect_len %d", sect_len);
    return 3;
  }

  prog_len = ((tsb[15] & 0x0f) << 8) | tsb[16];
  i        = 17 + prog_len;

  while (i < (7 + sect_len - 4)) {
    //int stream_type = tsb[i];
    int pid    = ((tsb[i+1] & 0x1f) << 8) | tsb[i+2];
    int es_len = ((tsb[i+3] & 0x0f) << 8) | tsb[i+4];

    /* Keep */
    if (check_pid_bit(pm->pm_streams, pid)) {
      i += 5 + es_len;

    /* Drop */
    } else {
      memmove(tsb + i, tsb + i + 5 + es_len, (188 - i - 5 - es_len));
      sect_len -= (5 + es_len);
    }
  }

  printf("rewrite PMT, orig len %d new len %d i %d", tsb[7], sect_len, i);

  tsb[7] = sect_len;

  crc32 = tvh_crc32(tsb+5, i-5, 0xffffffff);
  tsb[i++] = (crc32 >> 24) & 0xff;
  tsb[i++] = (crc32 >> 16) & 0xff;
  tsb[i++] = (crc32 >>  8) & 0xff;
  tsb[i++] = (crc32 >>  0) & 0xff;

  return 0;
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
  pm->pm_pmt_pid = ss->ss_pmt_pid;
  pm->pm_service_id = ss->ss_service_id;

  /* Store the PIDs of all the components */
  if (pm->pm_flags & MUX_REWRITE_PMT) {
    int i;
    memset(pm->pm_streams,0,1024);
    for (i=0;i<ss->ss_num_components;i++) {
      /* SCT_TEXTSUB (text extracted from teletext) streams are virtual and have an invalid PID */
      if (ss->ss_components[i].ssc_pid < 8192) {
        set_pid_bit(pm->pm_streams,ss->ss_components[i].ssc_pid);
      }
    }
  }

  return 0;
}


/**
 * Init the passthrough muxer with streams
 */
static int
pass_muxer_init(muxer_t* m, const struct streaming_start *ss, const char *name)
{
  return pass_muxer_reconfigure(m, ss);
}


/**
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
pass_muxer_open_stream(muxer_t *m, int fd)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;

  pm->pm_fd       = fd;
  pm->pm_seekable = 0;
  pm->pm_filename = strdup("Live stream");

  return 0;
}


/**
 * Open the file and set the file descriptor
 */
static int
pass_muxer_open_file(muxer_t *m, const char *filename)
{
  int fd;
  pass_muxer_t *pm = (pass_muxer_t*)m;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(fd < 0) {
    pm->pm_error = errno;
    tvhlog(LOG_ERR, "pass", "%s: Unable to create file, open failed -- %s",
	   filename, strerror(errno));
    pm->m_errors++;
    return -1;
  }

  pm->pm_seekable = 1;
  pm->pm_fd       = fd;
  pm->pm_filename = strdup(filename);
  return 0;
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
    tvhlog(LOG_ERR, "pass", "%s: Write failed -- %s", pm->pm_filename, 
	   strerror(errno));
    m->m_errors++;
  }
}


/**
 * Write TS packets to the file descriptor
 */
static void
pass_muxer_write_ts(muxer_t *m, pktbuf_t *pb)
{
  pass_muxer_t *pm = (pass_muxer_t*)m;
  unsigned char* tsb;
  
  // Note: this code currently assumes PAT/PMT will never span 1 packet

  /* Rewrite PAT/PMT in operation */
  if (pm->pm_flags & (MUX_REWRITE_PAT | MUX_REWRITE_PMT)) {

    tsb = pb->pb_data;
    while (tsb < pb->pb_data + pb->pb_size) {
      int pid = (tsb[1] & 0x1f) << 8 | tsb[2];

      /* PAT */
      if (pm->pm_flags & MUX_REWRITE_PAT && pid == 0) {
        if (pass_muxer_rewrite_pat(pm, tsb)) {
          tvherror("pass", "PAT rewrite failed, disabling");
          pm->pm_flags &= ~MUX_REWRITE_PAT;
        }

      /* PMT */
      } else if (pm->pm_flags & MUX_REWRITE_PMT && pid == pm->pm_pmt_pid) {
        if (pass_muxer_rewrite_pmt(pm, tsb)) {
          tvherror("pass", "PMT rewrite failed, disabling");
          pm->pm_flags &= ~MUX_REWRITE_PMT;
        }
      }

      tsb += 188;
    }
  }

  pass_muxer_write(m, pb->pb_data, pb->pb_size);
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
pass_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb)
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

  if(pm->pm_seekable && close(pm->pm_fd)) {
    pm->pm_error = errno;
    tvhlog(LOG_ERR, "pass", "%s: Unable to create file, open failed -- %s",
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

  free(pm);
}


/**
 * Create a new passthrough muxer
 */
muxer_t*
pass_muxer_create(muxer_container_type_t mc, muxer_config_t *m_cfg)
{
  pass_muxer_t *pm;

  if(mc != MC_PASS && mc != MC_RAW)
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
  /* Copy any configuration values we are interested in */
  if ((mc == MC_PASS) && (m_cfg))
    pm->pm_flags = m_cfg->dvr_flags;

  return (muxer_t *)pm;
}

