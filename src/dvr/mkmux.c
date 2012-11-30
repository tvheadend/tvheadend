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

#include "tvheadend.h"
#include "streaming.h"
#include "dvr.h"
#include "mkmux.h"
#include "ebml.h"

extern int dvr_iov_max;

TAILQ_HEAD(mk_cue_queue, mk_cue);

#define MATROSKA_TIMESCALE 1000000 // in nS


/**
 *
 */
typedef struct mk_track {
  int index;
  int enabled;
  int merge;
  int type;
  int tracknum;
  int disabled;
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
  char *filename;
  int error;
  off_t fdpos; // Current position in file
  int seekable;

  mk_track *tracks;
  int ntracks;
  int has_video;

  int64_t totduration;

  htsbuf_queue_t *cluster;
  int64_t cluster_tc;
  off_t cluster_pos;
  int cluster_maxsize;

  off_t segment_header_pos;

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
  char app[128];

  snprintf(app, sizeof(app), "Tvheadend %s", tvheadend_version);

  ebml_append_bin(q, 0x73a4, mkm->uuid, sizeof(mkm->uuid));
  ebml_append_string(q, 0x7ba9, mkm->title);
  ebml_append_string(q, 0x4d80, "Tvheadend Matroska muxer");
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
  uint8_t buf4[4];

  mkm->tracks = calloc(1, sizeof(mk_track) * ss->ss_num_components);
  mkm->ntracks = ss->ss_num_components;
  
  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    mkm->tracks[i].disabled = ssc->ssc_disabled;

    if(ssc->ssc_disabled)
      continue;

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

    case SCT_EAC3:
      tracktype = 2;
      codec_id = "A_EAC3";
      break;

    case SCT_MP4A:
    case SCT_AAC:
      tracktype = 2;
      codec_id = "A_AAC";
      break;

    case SCT_DVBSUB:
      tracktype = 0x11;
      codec_id = "S_DVBSUB";
      break;

    case SCT_TEXTSUB:
      tracktype = 0x11;
      codec_id = "S_TEXT/UTF8";
      break;

    default:
      continue;
    }

    mkm->tracks[i].enabled = 1;
    tracknum++;
    mkm->tracks[i].tracknum = tracknum;
    mkm->has_video |= (tracktype == 1);
    
    t = htsbuf_queue_alloc(0);

    ebml_append_uint(t, 0xd7, mkm->tracks[i].tracknum);
    ebml_append_uint(t, 0x73c5, mkm->tracks[i].tracknum);
    ebml_append_uint(t, 0x83, tracktype);
    ebml_append_uint(t, 0x9c, 0); // Lacing
    ebml_append_string(t, 0x86, codec_id);
    
    if(ssc->ssc_lang[0])
      ebml_append_string(t, 0x22b59c, ssc->ssc_lang);
    
    switch(ssc->ssc_type) {
    case SCT_H264:
    case SCT_MPEG2VIDEO:
    case SCT_MP4A:
    case SCT_AAC:
      if(ssc->ssc_gh)
	ebml_append_bin(t, 0x63a2, 
			pktbuf_ptr(ssc->ssc_gh),
			pktbuf_len(ssc->ssc_gh));
      break;
      
    case SCT_DVBSUB:
      buf4[0] = ssc->ssc_composition_id >> 8;
      buf4[1] = ssc->ssc_composition_id;
      buf4[2] = ssc->ssc_ancillary_id >> 8;
      buf4[3] = ssc->ssc_ancillary_id;
      ebml_append_bin(t, 0x63a2, buf4, 4);
      break;
    }

    if(ssc->ssc_frameduration) {
      int d = ts_rescale(ssc->ssc_frameduration, 1000000000);
      ebml_append_uint(t, 0x23e383, d);
    }
    

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      htsbuf_queue_t *vi = htsbuf_queue_alloc(0);

      ebml_append_uint(vi, 0xb0, ssc->ssc_width);
      ebml_append_uint(vi, 0xba, ssc->ssc_height);

      if(ssc->ssc_aspect_num && ssc->ssc_aspect_den) {
	ebml_append_uint(vi, 0x54b2, 3); // Display width/height is in DAR
	ebml_append_uint(vi, 0x54b0, ssc->ssc_aspect_num);
	ebml_append_uint(vi, 0x54ba, ssc->ssc_aspect_den);

      }

      ebml_append_master(t, 0xe0, vi);
    }

    if(SCT_ISAUDIO(ssc->ssc_type)) {
      htsbuf_queue_t *au = htsbuf_queue_alloc(0);

      ebml_append_float(au, 0xb5, sri_to_rate(ssc->ssc_sri));
      ebml_append_uint(au, 0x9f, ssc->ssc_channels);

      ebml_append_master(t, 0xe1, au);
    }


    ebml_append_master(q, 0xae, t);
  }
  return q;
}


/**
 *
 */
static int
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

  do {
    ssize_t r;
    int iovcnt = i < dvr_iov_max ? i : dvr_iov_max;
    if((r = writev(mkm->fd, iov, iovcnt)) == -1) {
      mkm->error = errno;
      return -1;
    }
    mkm->fdpos += r;
    i -= iovcnt;
    iov += iovcnt;
  } while(i);

  return 0;
}


/**
 *
 */
static void
mk_write_queue(mk_mux_t *mkm, htsbuf_queue_t *q)
{
  if(!mkm->error && mk_write_to_fd(mkm, q))
    tvhlog(LOG_ERR, "mkv", "%s: Write failed -- %s", mkm->filename, 
	   strerror(errno));

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
static htsbuf_queue_t *
mk_build_segment_header(int64_t size)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  uint8_t u8[8];

  ebml_append_id(q, 0x18538067);
  
  u8[0] = 1;
  if(size == 0) {
    memset(u8+1, 0xff, 7);
  } else {
    u8[1] = size >> 56;
    u8[2] = size >> 48;
    u8[3] = size >> 32;
    u8[4] = size >> 24;
    u8[5] = size >> 16;
    u8[6] = size >> 8;
    u8[7] = size;
  }
  htsbuf_append(q, &u8, 8);
  
  return q;
}


/**
 *
 */
static void
mk_write_segment_header(mk_mux_t *mkm, int64_t size)
{
  mk_write_queue(mkm, mk_build_segment_header(size));
}


/**
 *
 */
static htsbuf_queue_t *
build_tag_string(const char *name, const char *value, const char *lang,
		 int targettype, const char *targettypename)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  htsbuf_queue_t *st = htsbuf_queue_alloc(0);

  htsbuf_queue_t *t = htsbuf_queue_alloc(0);
  ebml_append_uint(t, 0x68ca, targettype ?: 50);

  if(targettypename)
    ebml_append_string(t, 0x63ca, targettypename);
  ebml_append_master(q, 0x63c0, t);

  ebml_append_string(st, 0x45a3, name);
  ebml_append_string(st, 0x4487, value);
  ebml_append_uint(st, 0x4484, 1);
  ebml_append_string(st, 0x447a, lang ?: "und");

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
  return build_tag_string(name, str, NULL, targettype, targettypename);
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
_mk_build_metadata(const dvr_entry_t *de, const epg_broadcast_t *ebc)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  char datestr[64], ctype[100];
  const epg_genre_t *eg = NULL;
  struct tm tm;
  localtime_r(de ? &de->de_start : &ebc->start, &tm);
  epg_episode_t *ee = NULL;
  channel_t *ch;
  lang_str_t *ls = NULL;

  if (ebc)               ee = ebc->episode;
  else if (de->de_bcast) ee = de->de_bcast->episode;

  if (de) ch = de->de_channel;
  else    ch = ebc->channel;

  snprintf(datestr, sizeof(datestr),
	   "%04d-%02d-%02d %02d:%02d:%02d",
	   tm.tm_year + 1900,
	   tm.tm_mon + 1,
	   tm.tm_mday,
	   tm.tm_hour,
	   tm.tm_min,
	   tm.tm_sec);

  addtag(q, build_tag_string("DATE_BROADCASTED", datestr, NULL, 0, NULL));

  addtag(q, build_tag_string("ORIGINAL_MEDIA_TYPE", "TV", NULL, 0, NULL));

  if(de && de->de_content_type.code) {
    eg = &de->de_content_type;
  } else if (ee) {
    eg = LIST_FIRST(&ee->genre);
  }
  if(eg && epg_genre_get_str(eg, 1, 0, ctype, 100))
    addtag(q, build_tag_string("CONTENT_TYPE", ctype, NULL, 0, NULL));

  if(ch)
    addtag(q, build_tag_string("TVCHANNEL", ch->ch_name, NULL, 0, NULL));

  if(de && de->de_desc)
    ls = de->de_desc;
  else if (ee && ee->description)
    ls = ee->description;
  else if (ee && ee->summary)
    ls = ee->summary;
  if (ls) {
    lang_str_ele_t *e;
    RB_FOREACH(e, ls, link)
      addtag(q, build_tag_string("SUMMARY", e->str, e->lang, 0, NULL));
  }

  if (ee) {
    epg_episode_num_t num;
    epg_episode_get_epnum(ee, &num);
    if(num.e_num)
      addtag(q, build_tag_int("PART_NUMBER", num.e_num,
			       0, NULL));
    if(num.s_num)
      addtag(q, build_tag_int("PART_NUMBER", num.s_num,
			       60, "SEASON"));
    if(num.p_num)
      addtag(q, build_tag_int("PART_NUMBER", num.p_num,
			       40, "PART"));
    if (num.text)
      addtag(q, build_tag_string("SYNOPSIS", 
			       num.text, NULL, 0, NULL));
  }

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
  } else if(mkm->seekable) {
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
static htsbuf_queue_t *
mk_build_segment(mk_mux_t *mkm, 
		 const struct streaming_start *ss)
{
  htsbuf_queue_t *p = htsbuf_queue_alloc(0);

  assert(mkm->segment_pos != 0);

  // Meta seek
  if(mkm->seekable)
    ebml_append_pad(p, 500);

  mkm->segmentinfo_pos = mkm->segment_pos + p->hq_size;
  ebml_append_master(p, 0x1549a966, mk_build_segment_info(mkm));

  mkm->trackinfo_pos = mkm->segment_pos + p->hq_size;
  ebml_append_master(p, 0x1654ae6b, mk_build_tracks(mkm, ss));
  
  return p;
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

  uint8_t *data = pktbuf_ptr(pkt->pkt_payload);
  size_t len = pktbuf_len(pkt->pkt_payload);
  const int clusersizemax = 2000000;

  if(!data || len <= 0)
    return;

  if(pts == PTS_UNSET)
    // This is our best guess, it might be wrong but... oh well
    pts = t->nextpts;

  if(pts != PTS_UNSET) {
    t->nextpts = pts + (pkt->pkt_duration >> pkt->pkt_field);
	
    nxt = ts_rescale(t->nextpts, 1000000000 / MATROSKA_TIMESCALE);
    pts = ts_rescale(pts,        1000000000 / MATROSKA_TIMESCALE);

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

  if(vkeyframe && mkm->cluster && mkm->cluster->hq_size > mkm->cluster_maxsize)
    mk_close_cluster(mkm);

  else if(!mkm->has_video && mkm->cluster && mkm->cluster->hq_size > clusersizemax/40)
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

  if(t->type == SCT_AAC || t->type == SCT_MP4A) {
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
mk_mux_t *mk_mux_create(void)
{
  mk_mux_t *mkm = calloc(1, sizeof(struct mk_mux));

  mkm->fd = -1;

  return mkm;
}


/**
 *
 */
int
mk_mux_open_stream(mk_mux_t *mkm, int fd)
{
  mkm->filename = strdup("Live stream");
  mkm->fd = fd;
  mkm->cluster_maxsize = 0;

  return 0;
}


/**
 *
 */
int
mk_mux_open_file(mk_mux_t *mkm, const char *filename)
{
  int fd;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if(fd < 0) {
    mkm->error = errno;
    tvhlog(LOG_ERR, "mkv", "%s: Unable to create file, open failed -- %s",
	   mkm->filename, strerror(errno));
    return mkm->error;
  }

  mkm->filename = strdup(filename);
  mkm->fd = fd;
  mkm->cluster_maxsize = 2000000/4;
  mkm->seekable = 1;

  return 0;
}


/**
 * Init the muxer with a title and some tracks
 */
int
mk_mux_init(mk_mux_t *mkm, const char *title, const struct streaming_start *ss)
{
  htsbuf_queue_t q;

  getuuid(mkm->uuid);

  if(title)
    mkm->title = strdup(title);
  else
    mkm->title = strdup(mkm->filename);

  TAILQ_INIT(&mkm->cues);

  htsbuf_queue_init(&q, 0);

  ebml_append_master(&q, 0x1a45dfa3, mk_build_ebmlheader());
  
  mkm->segment_header_pos = q.hq_size;
  htsbuf_appendq(&q, mk_build_segment_header(0));

  mkm->segment_pos = q.hq_size;
  htsbuf_appendq(&q, mk_build_segment(mkm, ss));
 
  mk_write_queue(mkm, &q);

  return mkm->error;
}


/**
 * Append a packet to the muxer
 */
int
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
  
  if(t != NULL && !t->disabled) {
    if(t->merge)
      pkt = pkt_merge_header(pkt);
    mk_write_frame_i(mkm, t, pkt);
  }
  
  pkt_ref_dec(pkt);

  return mkm->error;
}


/**
 * Append epg data to the muxer
 */
int
mk_mux_write_meta(mk_mux_t *mkm, const dvr_entry_t *de, 
		  const epg_broadcast_t *ebc)
{
  htsbuf_queue_t q;

  if(!mkm->metadata_pos)
    mkm->metadata_pos = mkm->fdpos;

  htsbuf_queue_init(&q, 0);
  ebml_append_master(&q, 0x1254c367, _mk_build_metadata(de, ebc));
  mk_write_queue(mkm, &q);

  return mkm->error;
}


/**
 * Close the muxer
 */
int
mk_mux_close(mk_mux_t *mkm)
{
  int64_t totsize;
  mk_close_cluster(mkm);
  mk_write_cues(mkm);

  mk_write_metaseek(mkm, 0);
  totsize = mkm->fdpos;

  if(mkm->seekable) {
    // Rewrite segment info to update duration
    if(lseek(mkm->fd, mkm->segmentinfo_pos, SEEK_SET) == mkm->segmentinfo_pos)
      mk_write_master(mkm, 0x1549a966, mk_build_segment_info(mkm));
    else {
      mkm->error = errno;
      tvhlog(LOG_ERR, "mkv", "%s: Unable to write duration, seek failed -- %s",
	     mkm->filename, strerror(errno));
    }

    // Rewrite segment header to update total size
    if(lseek(mkm->fd, mkm->segment_header_pos, SEEK_SET) == mkm->segment_header_pos) {
      mk_write_segment_header(mkm, totsize - mkm->segment_header_pos - 12);
    } else {
      mkm->error = errno;
      tvhlog(LOG_ERR, "mkv", "%s: Unable to write total size, seek failed -- %s",
	     mkm->filename, strerror(errno));
    }

    if(close(mkm->fd)) {
      mkm->error = errno;
      tvhlog(LOG_ERR, "mkv", "%s: Unable to close the file descriptor, close failed -- %s",
	     mkm->filename, strerror(errno));
    }
  }

  return mkm->error;
}


/**
 * Free all memory associated with the muxer.
 */
void
mk_mux_destroy(mk_mux_t *mkm)
{
  free(mkm->filename);
  free(mkm->tracks);
  free(mkm->title);
  free(mkm);
}
