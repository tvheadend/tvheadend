/*
 *  Matroska muxer
 *  Copyright (C) 2005 Mike Matsnev
 *  Copyright (C) 2010 Andreas Öman
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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "tvhead.h"
#include "streaming.h"
#include "dvr.h"
#include "mkmux.h"
#include "ebml.h"

TAILQ_HEAD(mk_cue_queue, mk_cue);

#define MATROSKA_TIMESCALE 1000000 // in nS


static const Rational mpeg_tc = {1, 90000};
static const Rational mkv_tc  = {1, 1000};
static const Rational ns_tc  = {1, 1000000000};

/**
 *
 */
typedef struct mk_track {
  int index;
  int enabled;
  int merge;
  int type;
  int tracknum;
  int64_t nextpts;
} mk_track;

/**
 *
 */
struct mk_cue {
  TAILQ_ENTRY(mk_cue) link;
  int64_t ts;
  int tracknum;
  off_t cluster_pos;
};

/**
 *
 */
struct mk_mux {
  int fd;
  int error;
  off_t fdpos; // Current position in file

  mk_track *tracks;
  int ntracks;

  int64_t totduration;

  htsbuf_queue_t *cluster;
  int64_t cluster_tc;
  off_t cluster_pos;


  off_t segment_pos;

  off_t segmentinfo_pos;
  off_t trackinfo_pos;
  off_t metadata_pos;
  off_t cue_pos;

  int addcue;

  struct mk_cue_queue cues;

  char uuid[16];
  char *title;
};


/**
 *
 */
static htsbuf_queue_t *
mk_build_ebmlheader(void)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x4286, 1);
  ebml_append_uint(q, 0x42f7, 1);
  ebml_append_uint(q, 0x42f2, 4);
  ebml_append_uint(q, 0x42f3, 8);
  ebml_append_string(q, 0x4282, "matroska");
  ebml_append_uint(q, 0x4287, 2);
  ebml_append_uint(q, 0x4285, 2);
  return q;
}


/**
 *
 */
static void
getuuid(char *id)
{
  int r, fd = open("/dev/urandom", O_RDONLY);
  if(fd != -1) {
    r = read(fd, id, 16);
    close(fd);
    if(r == 16)
      return;
  }
  memset(id, 0x55, 16);
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_segment_info(mk_mux_t *mkm)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  extern char *htsversion_full;
  char app[128];

  snprintf(app, sizeof(app), "HTS Tvheadend %s", htsversion_full);

  ebml_append_bin(q, 0x73a4, mkm->uuid, sizeof(mkm->uuid));
  ebml_append_string(q, 0x7ba9, mkm->title);
  ebml_append_string(q, 0x4d80, "HTS Tvheadend Matroska muxer");
  ebml_append_string(q, 0x5741, app);
  ebml_append_uint(q, 0x2ad7b1, MATROSKA_TIMESCALE);
  if(mkm->totduration)
    ebml_append_float(q, 0x4489, (float)mkm->totduration);
  else
    ebml_append_pad(q, 7); // Must be equal to floatingpoint duration
  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_tracks(mk_mux_t *mkm, const struct streaming_start *ss)
{
  const streaming_start_component_t *ssc;
  const char *codec_id;
  int i, tracktype;
  htsbuf_queue_t *q = htsbuf_queue_alloc(0), *t;
  int tracknum = 0;

  mkm->tracks = calloc(1, sizeof(mk_track) * ss->ss_num_components);
  mkm->ntracks = ss->ss_num_components;
  
  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    mkm->tracks[i].index = ssc->ssc_index;
    mkm->tracks[i].type  = ssc->ssc_type;
    mkm->tracks[i].nextpts = PTS_UNSET;

    switch(ssc->ssc_type) {
    case SCT_MPEG2VIDEO:
      tracktype = 1;
      codec_id = "V_MPEG2";
      mkm->tracks[i].merge = 1;
      break;

    case SCT_H264:
      tracktype = 1;
      codec_id = "V_MPEG4/ISO/AVC";
      break;

    case SCT_MPEG2AUDIO:
      tracktype = 2;
      codec_id = "A_MPEG/L2";
      break;

    case SCT_AC3:
      tracktype = 2;
      codec_id = "A_AC3";
      break;

    case SCT_AAC:
      tracktype = 2;
      codec_id = "A_AAC";
      break;

    default:
      continue;
    }

    mkm->tracks[i].enabled = 1;
    mkm->tracks[i].tracknum = tracknum;
    tracknum++;

    t = htsbuf_queue_alloc(0);

    ebml_append_uint(t, 0xd7, mkm->tracks[i].tracknum);
    ebml_append_uint(t, 0x73c5, mkm->tracks[i].tracknum);
    ebml_append_uint(t, 0x83, tracktype);
    ebml_append_uint(t, 0x9c, 0); // Lacing
    ebml_append_string(t, 0x86, codec_id);
    
    if(ssc->ssc_lang[0])
      ebml_append_string(t, 0x22b59c, ssc->ssc_lang);
    
    if(ssc->ssc_global_header_len) {
      switch(ssc->ssc_type) {
      case SCT_H264:
      case SCT_MPEG2VIDEO:
      case SCT_AAC:
	ebml_append_bin(t, 0x63a2, ssc->ssc_global_header,
		       ssc->ssc_global_header_len);
	break;
      }
    }

    if(ssc->ssc_frameduration) {
      int d = rescale_q(ssc->ssc_frameduration, mpeg_tc, ns_tc);
      ebml_append_uint(t, 0x23e383, d);
    }
    

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      htsbuf_queue_t *vi = htsbuf_queue_alloc(0);

      ebml_append_uint(vi, 0xb0, ssc->ssc_width);
      ebml_append_uint(vi, 0xba, ssc->ssc_height);

      ebml_append_master(t, 0xe0, vi);
    }
    ebml_append_master(q, 0xae, t);
  }
  return q;
}


/**
 *
 */
static void
mk_write_to_fd(mk_mux_t *mkm, htsbuf_queue_t *hq)
{
  htsbuf_data_t *hd;
  int i = 0;

  TAILQ_FOREACH(hd, &hq->hq_q, hd_link)
    i++;

  struct iovec *iov = alloca(sizeof(struct iovec) * i);

  i = 0;
  TAILQ_FOREACH(hd, &hq->hq_q, hd_link) {
    iov[i  ].iov_base = hd->hd_data     + hd->hd_data_off;
    iov[i++].iov_len  = hd->hd_data_len - hd->hd_data_off;
  }
  
  if(writev(mkm->fd, iov, i) != hq->hq_size) {
    mkm->error = errno;
  } else {
    mkm->fdpos += hq->hq_size;
  }
}


/**
 *
 */
static void
mk_write_queue(mk_mux_t *mkm, htsbuf_queue_t *q)
{
  if(!mkm->error)
    mk_write_to_fd(mkm, q);

  htsbuf_queue_flush(q);
}


/**
 *
 */
static void
mk_write_master(mk_mux_t *mkm, uint32_t id, htsbuf_queue_t *p)
{
  htsbuf_queue_t q;
  htsbuf_queue_init(&q, 0);
  ebml_append_master(&q, id, p);
  mk_write_queue(mkm, &q);
}


/**
 *
 */
static void
mk_write_segment(mk_mux_t *mkm)
{
  htsbuf_queue_t q;
  char ff = 0xff;
  htsbuf_queue_init(&q, 0);

  ebml_append_id(&q, 0x18538067);
  htsbuf_append(&q, &ff, 1);
  
  mk_write_queue(mkm, &q);
}


/**
 *
 */
static htsbuf_queue_t *
build_tag_string(const char *name, const char *value,
		 int targettype, const char *targettypename)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  htsbuf_queue_t *t = htsbuf_queue_alloc(0);
  htsbuf_queue_t *st = htsbuf_queue_alloc(0);

  if(targettype == 0 && targettypename == NULL) {
    ebml_append_void(t);
  } else {
    if(targettype)
      ebml_append_uint(t, 0x68ca, targettype);
    if(targettypename)
      ebml_append_string(t, 0x63ca, targettypename);
  }
  ebml_append_master(q, 0x63c0, t);

  ebml_append_string(st, 0x45a3, name);
  ebml_append_string(st, 0x4487, value);
  ebml_append_master(q, 0x67c8, st);
  return q;
}


/**
 *
 */
static htsbuf_queue_t *
build_tag_int(const char *name, int value,
	      int targettype, const char *targettypename)
{
  char str[64];
  snprintf(str, sizeof(str), "%d", value);
  return build_tag_string(name, str, targettype, targettypename);
}


/**
 *
 */
static void
addtag(htsbuf_queue_t *q, htsbuf_queue_t *t)
{
  ebml_append_master(q, 0x7373, t);
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_metadata(const dvr_entry_t *de)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  if(de->de_channel != NULL)
    addtag(q, build_tag_string("TVCHANNEL", de->de_channel->ch_name, 0, NULL));

  if(de->de_episode.ee_onscreen)
    addtag(q, build_tag_string("SYNOPSIS", 
			       de->de_episode.ee_onscreen, 0, NULL));

  if(de->de_title != NULL)
    addtag(q, build_tag_string("SUMMARY", de->de_title, 0, NULL));

  if(de->de_episode.ee_season)
    addtag(q, build_tag_int("PART_NUMBER", de->de_episode.ee_season,
			    60, "SEASON"));

  if(de->de_episode.ee_episode)
    addtag(q, build_tag_int("PART_NUMBER", de->de_episode.ee_episode,
			    0, NULL));

  if(de->de_episode.ee_part)
    addtag(q, build_tag_int("PART_NUMBER", de->de_episode.ee_part,
			    40, "PART"));

  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_one_metaseek(mk_mux_t *mkm, uint32_t id, off_t pos)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  
  ebml_append_idid(q, 0x53ab, id);
  ebml_append_uint(q, 0x53ac, pos - mkm->segment_pos);
  return q;
}

/**
 *
 */
static htsbuf_queue_t *
mk_build_metaseek(mk_mux_t *mkm)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  if(mkm->segmentinfo_pos)
    ebml_append_master(q, 0x4dbb, 
		       mk_build_one_metaseek(mkm, 0x1549a966,
					     mkm->segmentinfo_pos));
  
  if(mkm->trackinfo_pos)
    ebml_append_master(q, 0x4dbb, 
		       mk_build_one_metaseek(mkm, 0x1654ae6b, 
					     mkm->trackinfo_pos));

  if(mkm->metadata_pos)
    ebml_append_master(q, 0x4dbb, 
		       mk_build_one_metaseek(mkm, 0x1254c367, 
					     mkm->metadata_pos));

  if(mkm->cue_pos)
    ebml_append_master(q, 0x4dbb, 
		       mk_build_one_metaseek(mkm, 0x1c53bb6b,
					     mkm->cue_pos));
  return q;
}

/**
 *
 */
static void
mk_write_metaseek(mk_mux_t *mkm, int first)
{
  htsbuf_queue_t q;

  htsbuf_queue_init(&q, 0);

  ebml_append_master(&q, 0x114d9b74, mk_build_metaseek(mkm));

  assert(q.hq_size < 498);

  ebml_append_pad(&q, 500 - q.hq_size);
  
  if(first) {
    mk_write_to_fd(mkm, &q);
  } else {
    off_t prev = mkm->fdpos;
    if(lseek(mkm->fd, mkm->segment_pos, SEEK_SET) == (off_t) -1)
      mkm->error = errno;

    mk_write_queue(mkm, &q);
    mkm->fdpos = prev;
    if(lseek(mkm->fd, mkm->fdpos, SEEK_SET) == (off_t) -1)
      mkm->error = errno;
   
  }
  htsbuf_queue_flush(&q);
}

/**
 *
 */
mk_mux_t *
mk_mux_create(const char *filename,
	      const struct streaming_start *ss,
	      const struct dvr_entry *de)
{
  mk_mux_t *mkm;
  int fd;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(fd == -1)
    return NULL;

  mkm = calloc(1, sizeof(struct mk_mux));
  getuuid(mkm->uuid);
  mkm->fd = fd;
  mkm->title = strdup(de->de_title);
  TAILQ_INIT(&mkm->cues);

  mk_write_master(mkm, 0x1a45dfa3, mk_build_ebmlheader());
  mk_write_segment(mkm);

  mkm->segment_pos = mkm->fdpos;
  mk_write_metaseek(mkm, 1); // Must be first in segment


  mkm->segmentinfo_pos = mkm->fdpos;
  mk_write_master(mkm, 0x1549a966, mk_build_segment_info(mkm));

  mkm->trackinfo_pos = mkm->fdpos;
  mk_write_master(mkm, 0x1654ae6b, mk_build_tracks(mkm, ss));

  mkm->metadata_pos = mkm->fdpos;
  mk_write_master(mkm, 0x1254c367, mk_build_metadata(de));

  mk_write_metaseek(mkm, 0);

  return mkm;
}


/**
 *
 */
static void
addcue(mk_mux_t *mkm, int64_t pts, int tracknum)
{
  struct mk_cue *mc = malloc(sizeof(struct mk_cue));
  mc->ts = pts;
  mc->tracknum = tracknum;
  mc->cluster_pos = mkm->cluster_pos;
  TAILQ_INSERT_TAIL(&mkm->cues, mc, link);
}


/**
 *
 */
static void
mk_close_cluster(mk_mux_t *mkm)
{
  if(mkm->cluster != NULL)
    mk_write_master(mkm, 0x1f43b675, mkm->cluster);
  mkm->cluster = NULL;
}


/**
 *
 */
static void
mk_write_frame_i(mk_mux_t *mkm, mk_track *t, th_pkt_t *pkt)
{
  int64_t pts = pkt->pkt_pts, delta, nxt;
  unsigned char c_delta_flags[3];

  int keyframe  = pkt->pkt_frametype < PKT_P_FRAME;
  int skippable = pkt->pkt_frametype == PKT_B_FRAME;
  int vkeyframe = SCT_ISVIDEO(t->type) && keyframe;

  uint8_t *data;
  size_t len;
  const int clusersizemax = 2000000;

  if(pts == PTS_UNSET)
    // This is our best guess, it might be wrong but... oh well
    pts = t->nextpts;

  if(pts != PTS_UNSET) {
    t->nextpts = pts + (pkt->pkt_duration >> pkt->pkt_field);
	
    nxt = rescale_q(t->nextpts, mpeg_tc, mkv_tc);
    pts = rescale_q(pts,        mpeg_tc, mkv_tc);

    if(mkm->totduration < nxt)
      mkm->totduration = nxt;

    delta = pts - mkm->cluster_tc;
    if(delta > 32767ll || delta < -32768ll)
      mk_close_cluster(mkm);

    else if(vkeyframe && (delta > 30000ll || delta < -30000ll))
      mk_close_cluster(mkm);

  } else {
    return;
  }

  if(vkeyframe && mkm->cluster && mkm->cluster->hq_size > clusersizemax/4)
    mk_close_cluster(mkm);

  else if(mkm->cluster && mkm->cluster->hq_size > clusersizemax)
    mk_close_cluster(mkm);

  if(mkm->cluster == NULL) {
    mkm->cluster_tc = pts;
    mkm->cluster = htsbuf_queue_alloc(0);

    mkm->cluster_pos = mkm->fdpos;
    mkm->addcue = 1;

    ebml_append_uint(mkm->cluster, 0xe7, mkm->cluster_tc);
    delta = 0;
  }

  if(mkm->addcue && vkeyframe) {
    mkm->addcue = 0;
    addcue(mkm, pts, t->tracknum);
  }


  data = pkt->pkt_payload;
  len  = pkt->pkt_payloadlen;

  if(t->type == SCT_AAC) {
    // Skip ADTS header
    if(len < 7)
      return;
      
    len -= 7;
    data += 7;
  }


  ebml_append_id(mkm->cluster, 0xa3 ); // SimpleBlock
  ebml_append_size(mkm->cluster, len + 4);
  ebml_append_size(mkm->cluster, t->tracknum);

  c_delta_flags[0] = delta >> 8;
  c_delta_flags[1] = delta;
  c_delta_flags[2] = (keyframe << 7) | skippable;
  htsbuf_append(mkm->cluster, c_delta_flags, 3);
  htsbuf_append(mkm->cluster, data, len);
}


/**
 *
 */
void
mk_mux_write_pkt(mk_mux_t *mkm, struct th_pkt *pkt)
{
  int i;
  mk_track *t = NULL;
  for(i = 0; i < mkm->ntracks;i++) {
    if(mkm->tracks[i].index == pkt->pkt_componentindex &&
       mkm->tracks[i].enabled) {
      t = &mkm->tracks[i];
      break;
    }
  }
  
  if(t != NULL) {
    if(t->merge)
      pkt = pkt_merge_global(pkt);
    mk_write_frame_i(mkm, t, pkt);
  }
  
  pkt_ref_dec(pkt);
}


/**
 *
 */
static void
mk_write_cues(mk_mux_t *mkm)
{
  struct mk_cue *mc;
  htsbuf_queue_t *q, *c, *p;

  if(TAILQ_FIRST(&mkm->cues) == NULL)
    return;

  q = htsbuf_queue_alloc(0);

  while((mc = TAILQ_FIRST(&mkm->cues)) != NULL) {
    TAILQ_REMOVE(&mkm->cues, mc, link);

    c = htsbuf_queue_alloc(0);
    
    ebml_append_uint(c, 0xb3, mc->ts);

    p = htsbuf_queue_alloc(0);
    ebml_append_uint(p, 0xf7, mc->tracknum);
    ebml_append_uint(p, 0xf1, mc->cluster_pos - mkm->segment_pos);

    ebml_append_master(c, 0xb7, p);
    ebml_append_master(q, 0xbb, c);
    free(mc);
  }

  mkm->cue_pos = mkm->fdpos;
  mk_write_master(mkm, 0x1c53bb6b, q);
}

/**
 *
 */
void
mk_mux_close(mk_mux_t *mkm)
{
  mk_close_cluster(mkm);
  mk_write_cues(mkm);
  mk_write_metaseek(mkm, 0);

  // Rewrite segment info to update duration
  if(lseek(mkm->fd, mkm->segmentinfo_pos, SEEK_SET) == mkm->segmentinfo_pos)
    mk_write_master(mkm, 0x1549a966, mk_build_segment_info(mkm));

  close(mkm->fd);
  free(mkm->tracks);
  free(mkm->title);
  free(mkm);
}
