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
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "tvheadend.h"
#include "streaming.h"
#include "dvr/dvr.h"
#include "mkmux.h"
#include "ebml.h"
#include "lang_codes.h"
#include "parsers/parser_avc.h"

extern int dvr_iov_max;

TAILQ_HEAD(mk_cue_queue, mk_cue);
TAILQ_HEAD(mk_chapter_queue, mk_chapter);

#define MATROSKA_TIMESCALE 1000000 // in nS


/**
 *
 */
typedef struct mk_track {
  int index;
  int avc;
  int type;
  int tracknum;
  int tracktype;
  int disabled;
  int64_t nextpts;

  uint8_t channels;
  uint8_t sri;
  uint8_t ext_sri;

  uint16_t aspect_num;
  uint16_t aspect_den;

  uint8_t commercial;
} mk_track_t;

/**
 *
 */
typedef struct mk_cue {
  TAILQ_ENTRY(mk_cue) link;
  int64_t ts;
  int tracknum;
  off_t cluster_pos;
} mk_cue_t;

/**
 *
 */
typedef struct mk_chapter {
  TAILQ_ENTRY(mk_chapter) link;
  int uuid;
  int64_t ts;
} mk_chapter_t;

/**
 *
 */
struct mk_mux {
  muxer_t *m;
  int fd;
  char *filename;
  int error;
  off_t fdpos; // Current position in file
  int seekable;

  mk_track_t *tracks;
  int ntracks;
  int has_video;

  int64_t totduration;

  htsbuf_queue_t *cluster;
  int64_t cluster_tc;
  off_t cluster_pos;
  int cluster_maxsize;
  time_t cluster_last_close;

  off_t segment_header_pos;

  off_t segment_pos;

  off_t segmentinfo_pos;
  off_t trackinfo_pos;
  off_t metadata_pos;
  off_t cue_pos;
  off_t chapters_pos;

  int addcue;

  struct mk_cue_queue cues;
  struct mk_chapter_queue chapters;

  char uuid[16];
  char *title;

  int webm;
};


/**
 *
 */
static htsbuf_queue_t *
mk_build_ebmlheader(mk_mux_t *mkm)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x4286, 1);
  ebml_append_uint(q, 0x42f7, 1);
  ebml_append_uint(q, 0x42f2, 4);
  ebml_append_uint(q, 0x42f3, 8);
  ebml_append_string(q, 0x4282, mkm->webm ? "webm" : "matroska");
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
static int 
mk_split_vorbis_headers(uint8_t *extradata, int extradata_size, 
			uint8_t *header_start[3],  int header_len[3])
{
  int i;
  if (extradata_size >= 3 && extradata_size < INT_MAX - 0x1ff && extradata[0] == 2) {
    int overall_len = 3;
    extradata++;
    for (i=0; i<2; i++, extradata++) {
      header_len[i] = 0;
      for (; overall_len < extradata_size && *extradata==0xff; extradata++) {
	header_len[i] += 0xff;
	overall_len   += 0xff + 1;
      }
      header_len[i] += *extradata;
      overall_len   += *extradata;
      if (overall_len > extradata_size)
	return -1;
    }
    header_len[2] = extradata_size - overall_len;
    header_start[0] = extradata;
    header_start[1] = header_start[0] + header_len[0];
    header_start[2] = header_start[1] + header_len[1];
  } else {
    return -1;
  }
  return 0;
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

  if(!mkm->webm)
    ebml_append_bin(q, 0x73a4, mkm->uuid, sizeof(mkm->uuid));

  if(!mkm->webm)
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
mk_build_tracks(mk_mux_t *mkm, const streaming_start_t *ss)
{
  const streaming_start_component_t *ssc;
  const char *codec_id;
  int i, tracktype;
  htsbuf_queue_t *q = htsbuf_queue_alloc(0), *t;
  int tracknum = 0;
  uint8_t buf4[4];
  uint32_t bit_depth = 0;
  mk_track_t *tr;

  mkm->tracks = calloc(1, sizeof(mk_track_t) * ss->ss_num_components);
  mkm->ntracks = ss->ss_num_components;
  
  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    tr = &mkm->tracks[i];

    tr->disabled = ssc->ssc_disabled;
    tr->index = ssc->ssc_index;

    if(tr->disabled)
      continue;

    tr->type = ssc->ssc_type;
    tr->channels = ssc->ssc_channels;
    tr->aspect_num = ssc->ssc_aspect_num;
    tr->aspect_den = ssc->ssc_aspect_den;
    tr->commercial = COMMERCIAL_UNKNOWN;
    tr->sri = ssc->ssc_sri;
    tr->nextpts = PTS_UNSET;

    if (mkm->webm && ssc->ssc_type != SCT_VP8 && ssc->ssc_type != SCT_VORBIS)
      tvhwarn("mkv", "WEBM format supports only VP8+VORBIS streams (detected %s)",
              streaming_component_type2txt(ssc->ssc_type));

    switch(ssc->ssc_type) {
    case SCT_MPEG2VIDEO:
      tracktype = 1;
      codec_id = "V_MPEG2";
      break;

    case SCT_H264:
      tracktype = 1;
      codec_id = "V_MPEG4/ISO/AVC";
      tr->avc = 1;
      break;

    case SCT_VP8:
      tracktype = 1;
      codec_id = "V_VP8";
      break;

    case SCT_VP9:
      tracktype = 1;
      codec_id = "V_VP9";
      break;

    case SCT_HEVC:
      tvherror("mkv", "HEVC (H265) codec is not suppored for Matroska muxer (work in progress)");
      continue;

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
      bit_depth = 16;
      break;

    case SCT_VORBIS:
      tracktype = 2;
      codec_id = "A_VORBIS";
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
      tr->disabled = 1;
      continue;
    }

    tr->tracknum = ++tracknum;
    tr->tracktype = tracktype;
    mkm->has_video |= (tracktype == 1);
    
    t = htsbuf_queue_alloc(0);

    ebml_append_uint(t, 0xd7, tr->tracknum);
    ebml_append_uint(t, 0x73c5, tr->tracknum);
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
      if(ssc->ssc_gh) {
        sbuf_t hdr;
        sbuf_init(&hdr);
        if (tr->avc) {
          isom_write_avcc(&hdr, pktbuf_ptr(ssc->ssc_gh),
                                pktbuf_len(ssc->ssc_gh));
        } else {
          sbuf_append(&hdr, pktbuf_ptr(ssc->ssc_gh),
                            pktbuf_len(ssc->ssc_gh));
        }
        ebml_append_bin(t, 0x63a2, hdr.sb_data, hdr.sb_ptr);
        sbuf_free(&hdr);
      }
      break;
      
    case SCT_VORBIS:
      if(ssc->ssc_gh) {
	htsbuf_queue_t *cp;
	uint8_t *header_start[3];
	int header_len[3];
	int j;
	if(mk_split_vorbis_headers(pktbuf_ptr(ssc->ssc_gh), 
				   pktbuf_len(ssc->ssc_gh),
				   header_start, 
				   header_len) < 0)
	  break;

	cp = htsbuf_queue_alloc(0);

	ebml_append_xiph_size(cp, 2);

	for (j = 0; j < 2; j++)
	  ebml_append_xiph_size(cp, header_len[j]);

	for (j = 0; j < 3; j++)
	  htsbuf_append(cp, header_start[j], header_len[j]);

	ebml_append_master(t, 0x63a2, cp);
      }
      break;

    case SCT_DVBSUB:
      buf4[0] = ssc->ssc_composition_id >> 8;
      buf4[1] = ssc->ssc_composition_id;
      buf4[2] = ssc->ssc_ancillary_id >> 8;
      buf4[3] = ssc->ssc_ancillary_id;
      ebml_append_bin(t, 0x63a2, buf4, 4);
      break;
    }

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      htsbuf_queue_t *vi = htsbuf_queue_alloc(0);

      if(ssc->ssc_frameduration) {
        int d = ts_rescale(ssc->ssc_frameduration, 1000000000);
        ebml_append_uint(t, 0x23e383, d);
      }
      ebml_append_uint(vi, 0xb0, ssc->ssc_width);
      ebml_append_uint(vi, 0xba, ssc->ssc_height);

      if(mkm->webm && ssc->ssc_aspect_num && ssc->ssc_aspect_den) {
	// DAR is not supported by webm
	ebml_append_uint(vi, 0x54b2, 1);
	ebml_append_uint(vi, 0x54b0, (ssc->ssc_height * ssc->ssc_aspect_num) / ssc->ssc_aspect_den);
	ebml_append_uint(vi, 0x54ba, ssc->ssc_height);
      } else if(ssc->ssc_aspect_num && ssc->ssc_aspect_den) {
	ebml_append_uint(vi, 0x54b2, 3); // Display width/height is in DAR
	ebml_append_uint(vi, 0x54b0, ssc->ssc_aspect_num);
	ebml_append_uint(vi, 0x54ba, ssc->ssc_aspect_den);
      }

      ebml_append_master(t, 0xe0, vi);
    }

    if(SCT_ISAUDIO(ssc->ssc_type)) {
      htsbuf_queue_t *au = htsbuf_queue_alloc(0);

      ebml_append_float(au, 0xb5, sri_to_rate(ssc->ssc_sri));
      if (ssc->ssc_ext_sri)
        ebml_append_float(au, 0x78b5, sri_to_rate(ssc->ssc_ext_sri - 1));
      ebml_append_uint(au, 0x9f, ssc->ssc_channels);
      if (bit_depth)
        ebml_append_uint(au, 0x6264, bit_depth);

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
  off_t oldpos = mkm->fdpos;

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

  muxer_cache_update(mkm->m, mkm->fd, oldpos, 0);

  return 0;
}


/**
 *
 */
static void
mk_write_queue(mk_mux_t *mkm, htsbuf_queue_t *q)
{
  if(!mkm->error && mk_write_to_fd(mkm, q) && !MC_IS_EOS_ERROR(mkm->error))
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
  htsbuf_queue_t *q;
  q = mk_build_segment_header(size);
  mk_write_queue(mkm, q);
  htsbuf_queue_free(q);
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_one_chapter(mk_chapter_t *ch)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x73C4, ch->uuid);
  ebml_append_uint(q, 0x91, ch->ts * MATROSKA_TIMESCALE);
  ebml_append_uint(q, 0x98, 0); //ChapterFlagHidden
  ebml_append_uint(q, 0x4598, 1); //ChapterFlagEnabled

  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_edition_entry(mk_mux_t *mkm)
{
  mk_chapter_t *ch;
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x45bd, 0); //EditionFlagHidden
  ebml_append_uint(q, 0x45db, 1); //EditionFlagDefault

  TAILQ_FOREACH(ch, &mkm->chapters, link) {
    ebml_append_master(q, 0xB6, mk_build_one_chapter(ch));
  }

  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_chapters(mk_mux_t *mkm)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_master(q, 0x45b9, mk_build_edition_entry(mkm));

  return q;
}

/**
 *
 */
static void
mk_write_chapters(mk_mux_t *mkm)
{
  if(TAILQ_FIRST(&mkm->chapters) == NULL)
    return;

  mkm->chapters_pos = mkm->fdpos;
  mk_write_master(mkm, 0x1043a770, mk_build_chapters(mkm));
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
_mk_build_metadata(const dvr_entry_t *de, const epg_broadcast_t *ebc,
                   const char *comment)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  char datestr[64], ctype[100];
  const epg_genre_t *eg = NULL;
  epg_genre_t eg0;
  struct tm tm;
  time_t t;
  epg_episode_t *ee = NULL;
  channel_t *ch = NULL;
  lang_str_t *ls = NULL, *ls2 = NULL;
  const char **langs, *lang;

  if (ebc)                     ee = ebc->episode;
  else if (de && de->de_bcast) ee = de->de_bcast->episode;

  if (de)       ch = de->de_channel;
  else if (ebc) ch = ebc->channel;

  if (de || ebc) {
    localtime_r(de ? &de->de_start : &ebc->start, &tm);
  } else {
    t = time(NULL);
    localtime_r(&t, &tm);
  }
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

  if(de && de->de_content_type) {
    memset(&eg0, 0, sizeof(eg0));
    eg0.code = de->de_content_type;
    eg = &eg0;
  } else if (ee) {
    eg = LIST_FIRST(&ee->genre);
  }
  if(eg && epg_genre_get_str(eg, 1, 0, ctype, 100, NULL))
    addtag(q, build_tag_string("CONTENT_TYPE", ctype, NULL, 0, NULL));

  if(ch)
    addtag(q, build_tag_string("TVCHANNEL", 
                               channel_get_name(ch), NULL, 0, NULL));

  if (ee && ee->summary)
    ls = ee->summary;
  else if (ebc && ebc->summary)
    ls = ebc->summary;

  if(de && de->de_desc)
    ls2 = de->de_desc;
  else if (ee && ee->description)
    ls2 = ee->description;
  else if (ebc && ebc->description)
    ls2 = ebc->description;

  if (!ls)
    ls = ls2;

  if (ls) {
    lang_str_ele_t *e;
    RB_FOREACH(e, ls, link)
      addtag(q, build_tag_string("SUMMARY", e->str, e->lang, 0, NULL));
  }

  if (ls2 && ls != ls2) {
    lang_str_ele_t *e;
    RB_FOREACH(e, ls2, link)
      addtag(q, build_tag_string("DESCRIPTION", e->str, e->lang, 0, NULL));
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

  if (comment) {
    lang = "eng";
    if ((langs = lang_code_split(NULL)) && langs[0])
      lang = tvh_strdupa(langs[0]);
    free(langs);

    addtag(q, build_tag_string("COMMENT", comment, lang, 0, NULL));
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

  if(mkm->chapters_pos)
    ebml_append_master(q, 0x4dbb, 
		       mk_build_one_metaseek(mkm, 0x1043a770,
					     mkm->chapters_pos));
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
    mkm->fdpos = mkm->segment_pos;
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
		 const streaming_start_t *ss)
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
  mk_cue_t *mc = malloc(sizeof(mk_cue_t));
  mc->ts = pts;
  mc->tracknum = tracknum;
  mc->cluster_pos = mkm->cluster_pos;
  TAILQ_INSERT_TAIL(&mkm->cues, mc, link);
}


/**
 *
 */
static void
mk_add_chapter(mk_mux_t *mkm, int64_t ts)
{
  mk_chapter_t *ch;
  int uuid;

  ch = TAILQ_LAST(&mkm->chapters, mk_chapter_queue);
  if(ch) {
    // don't add a new chapter if the previous one was
    // added less than 10s ago
    if(ts - ch->ts < 10000)
      return;

    uuid = ch->uuid + 1;
  }
  else {
    uuid = 1;
  }

  ch = malloc(sizeof(mk_chapter_t));

  ch->uuid = uuid;
  ch->ts = ts;

  TAILQ_INSERT_TAIL(&mkm->chapters, ch, link);
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
  mkm->cluster_last_close = dispatch_clock;
}


/**
 *
 */
static void
mk_write_frame_i(mk_mux_t *mkm, mk_track_t *t, th_pkt_t *pkt)
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

    if((t->tracktype == 1 || t->tracktype == 2) && mkm->totduration < nxt)
      mkm->totduration = nxt;

    delta = pts - mkm->cluster_tc;
    if(delta > 32767ll || delta < -32768ll)
      mk_close_cluster(mkm);

    else if(vkeyframe && (delta > 30000ll || delta < -30000ll))
      mk_close_cluster(mkm);

  } else {
    return;
  }

  if(vkeyframe && mkm->cluster &&
     (mkm->cluster->hq_size > mkm->cluster_maxsize ||
      mkm->cluster_last_close + 1 < dispatch_clock))
    mk_close_cluster(mkm);

  else if(!mkm->has_video && mkm->cluster &&
          (mkm->cluster->hq_size > clusersizemax/40 ||
           mkm->cluster_last_close + 1 < dispatch_clock))
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
  mk_cue_t *mc;
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
mk_mux_t *mk_mux_create(muxer_t *m, int webm)
{
  mk_mux_t *mkm = calloc(1, sizeof(mk_mux_t));

  mkm->m = m;
  mkm->webm = webm;
  mkm->fd   = -1;

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
mk_mux_open_file(mk_mux_t *mkm, const char *filename, int permissions)
{
  int fd;

  tvhtrace("mkv", "Creating file \"%s\" with file permissions \"%o\"", filename, permissions);
  
  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, permissions);
  
  if(fd < 0) {
    mkm->error = errno;
    tvhlog(LOG_ERR, "mkv", "%s: Unable to create file, open failed -- %s",
	   mkm->filename, strerror(errno));
    return mkm->error;
  }

  /* bypass umask settings */
  if (fchmod(fd, permissions))
    tvhlog(LOG_ERR, "mkv", "%s: Unable to change permissions -- %s",
           filename, strerror(errno));
                     

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
mk_mux_init(mk_mux_t *mkm, const char *title, const streaming_start_t *ss)
{
  htsbuf_queue_t q, *a;

  getuuid(mkm->uuid);

  if(title)
    mkm->title = strdup(title);
  else
    mkm->title = strdup(mkm->filename);

  TAILQ_INIT(&mkm->cues);
  TAILQ_INIT(&mkm->chapters);

  htsbuf_queue_init(&q, 0);

  ebml_append_master(&q, 0x1a45dfa3, mk_build_ebmlheader(mkm));
  
  mkm->segment_header_pos = q.hq_size;
  a = mk_build_segment_header(0);
  htsbuf_appendq(&q, a);
  htsbuf_queue_free(a);

  mkm->segment_pos = q.hq_size;
  a = mk_build_segment(mkm, ss);
  htsbuf_appendq(&q, a);
  htsbuf_queue_free(a);
 
  mk_write_queue(mkm, &q);

  htsbuf_queue_flush(&q);

  return mkm->error;
}


/**
 * Append a packet to the muxer
 */
int
mk_mux_write_pkt(mk_mux_t *mkm, th_pkt_t *pkt)
{
  int i, mark;
  mk_track_t *t = NULL;
  th_pkt_t *opkt;

  for (i = 0; i < mkm->ntracks; i++) {
    t = &mkm->tracks[i];
    if (t->index == pkt->pkt_componentindex && !t->disabled)
      break;
  }
  
  if(i >= mkm->ntracks || pkt->pkt_payload == NULL) {
    pkt_ref_dec(pkt);
    return mkm->error;
  }

  mark = 0;
  if(pkt->pkt_channels != t->channels &&
     pkt->pkt_channels) {
    mark = 1;
    t->channels = pkt->pkt_channels;
  }
  if(pkt->pkt_aspect_num != t->aspect_num && 
     pkt->pkt_aspect_num) {
    mark = 1;
    t->aspect_num = pkt->pkt_aspect_num;
  }
  if(pkt->pkt_aspect_den != t->aspect_den && 
     pkt->pkt_aspect_den) {
    mark = 1;
    t->aspect_den = pkt->pkt_aspect_den;
  }
  if(pkt->pkt_sri != t->sri && 
     pkt->pkt_sri) {
    mark = 1;
    t->sri = pkt->pkt_sri;
    t->ext_sri = pkt->pkt_ext_sri;
  }
  if(pkt->pkt_commercial != t->commercial && 
     pkt->pkt_commercial != COMMERCIAL_UNKNOWN) {
    mark = 1;
    t->commercial = pkt->pkt_commercial;
  }

  if(mark)
    mk_mux_insert_chapter(mkm);

  if(t->avc) {
    pkt = avc_convert_pkt(opkt = pkt);
    pkt_ref_dec(opkt);
  }

  mk_write_frame_i(mkm, t, pkt);

  pkt_ref_dec(pkt);

  return mkm->error;
}


/**
 * Append epg data to the muxer
 */
int
mk_mux_write_meta(mk_mux_t *mkm, const dvr_entry_t *de, 
		  const epg_broadcast_t *ebc,
		  const char *comment)
{
  htsbuf_queue_t q;

  if(!mkm->metadata_pos)
    mkm->metadata_pos = mkm->fdpos;

  htsbuf_queue_init(&q, 0);
  ebml_append_master(&q, 0x1254c367, _mk_build_metadata(de, ebc, comment));
  mk_write_queue(mkm, &q);

  return mkm->error;
}


/**
 * Insert a new chapter at the current location
 */
int
mk_mux_insert_chapter(mk_mux_t *mkm)
{
  if(mkm->totduration != PTS_UNSET)
    mk_add_chapter(mkm, mkm->totduration);

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
  mk_write_chapters(mkm);

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
  mk_chapter_t *ch;

  while((ch = TAILQ_FIRST(&mkm->chapters)) != NULL) {
    TAILQ_REMOVE(&mkm->chapters, ch, link);
    free(ch);
  }

  free(mkm->filename);
  free(mkm->tracks);
  free(mkm->title);
  free(mkm);
}
