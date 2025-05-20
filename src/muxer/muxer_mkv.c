/*
 *  Matroska muxer
 *  Copyright (C) 2005 Mike Matsnev
 *  Copyright (C) 2010 Andreas Öman
 *
 *  tvheadend, wrapper for the builtin dvr muxer
 *  Copyright (C) 2012 John Törnblom
 *
 *  code merge, fixes, enhancements
 *  Copyright (C) 2014,2015,2016,2017 Jaroslav Kysela
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
#include "ebml.h"
#include "lang_codes.h"
#include "epg.h"
#include "parsers/parsers.h"
#include "parsers/parser_avc.h"
#include "parsers/parser_hevc.h"
#include "muxer_mkv.h"

#include "epggrab.h"  //Needed to see if global processing of parental rating labels is enabled.

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
  int hevc;
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
  uint32_t uuid;
  int64_t ts;
} mk_chapter_t;

typedef struct mk_muxer {
  muxer_t;
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
  int64_t cluster_last_close;
  int64_t cluster_maxsize;

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

  uint8_t uuid[16];
  char *title;

  int webm;
  int dvbsub_reorder;
  int dvbsub_skip;

  struct th_pktref_queue holdq;
} mk_muxer_t;

/* --- */

static int mk_mux_insert_chapter(mk_muxer_t *mk);

/*
 *
 */
static int mk_pktref_cmp(const void *_a, const void *_b)
{
  const th_pktref_t *a = _a, *b = _b;
  return a->pr_pkt->pkt_pts - b->pr_pkt->pkt_pts;
}

/*
 *
 */
static htsbuf_queue_t *
mk_build_ebmlheader(mk_muxer_t *mk)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x4286, 1);
  ebml_append_uint(q, 0x42f7, 1);
  ebml_append_uint(q, 0x42f2, 4);
  ebml_append_uint(q, 0x42f3, 8);
  ebml_append_string(q, 0x4282, mk->webm ? "webm" : "matroska");
  ebml_append_uint(q, 0x4287, 2);
  ebml_append_uint(q, 0x4285, 2);
  return q;
}


/**
 * lifted from avpriv_split_xiph_headers() in libavcodec/xiph.c
 */
static int
mk_split_xiph_headers(uint8_t *extradata, int extradata_size,
                      int first_header_size, uint8_t *header_start[3],
                      int header_len[3])
{
    int i;

    if (extradata_size >= 6 && RB16(extradata) == first_header_size) {
        int overall_len = 6;
        for (i=0; i<3; i++) {
            header_len[i] = RB16(extradata);
            extradata += 2;
            header_start[i] = extradata;
            extradata += header_len[i];
            if (overall_len > extradata_size - header_len[i])
                return -1;
            overall_len += header_len[i];
        }
    } else if (extradata_size >= 3 && extradata_size < INT_MAX - 0x1ff && extradata[0] == 2) {
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
mk_build_segment_info(mk_muxer_t *mk)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  char app[128];

  snprintf(app, sizeof(app), "Tvheadend %s", tvheadend_version);

  if(!mk->webm)
    ebml_append_bin(q, 0x73a4, mk->uuid, sizeof(mk->uuid));   //0x73a4 = SegmentUUID

  if(!mk->webm)
    ebml_append_string(q, 0x7ba9, mk->title);                 //0x7ba9 = Title

  ebml_append_string(q, 0x4d80, "Tvheadend Matroska muxer");  //0x4d80 = MuxingApp
  ebml_append_string(q, 0x5741, app);                         //0x5741 = WritingApp
  ebml_append_uint(q, 0x2ad7b1, MATROSKA_TIMESCALE);          //0x2ad7b1 = TimestampScale

  if(mk->totduration)
    ebml_append_float(q, 0x4489, (float)mk->totduration);     //0x4489 = Duration
  else
    ebml_append_pad(q, 7); // Must be equal to floatingpoint duration
  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_tracks(mk_muxer_t *mk, streaming_start_t *ss)
{
  streaming_start_component_t *ssc;
  const char *codec_id;
  int i, tracktype;
  htsbuf_queue_t *q = htsbuf_queue_alloc(0), *t;
  int tracknum = 0;
  uint8_t buf4[4];
  uint32_t bit_depth = 0;
  mk_track_t *tr;

  mk->tracks = calloc(1, sizeof(mk_track_t) * ss->ss_num_components);
  mk->ntracks = ss->ss_num_components;
  mk->cluster_maxsize = 4000000;
  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    tr = &mk->tracks[i];

    tr->disabled = ssc->ssc_disabled;
    tr->index = ssc->es_index;

    if(tr->disabled)
      continue;

    tr->type = ssc->es_type;
    tr->channels = ssc->es_channels;
    tr->aspect_num = ssc->es_aspect_num;
    tr->aspect_den = ssc->es_aspect_den;
    tr->commercial = COMMERCIAL_UNKNOWN;
    tr->sri = ssc->es_sri;
    tr->nextpts = PTS_UNSET;

    if (mk->webm && ssc->es_type != SCT_VP8 && ssc->es_type != SCT_VORBIS)
      tvhwarn(LS_MKV, "WEBM format supports only VP8+VORBIS streams (detected %s)",
              streaming_component_type2txt(ssc->es_type));

    switch(ssc->es_type) {
    case SCT_MPEG2VIDEO:
      tracktype = 1;
      codec_id = "V_MPEG2";
      break;

    case SCT_H264:
      tracktype = 1;
      codec_id = "V_MPEG4/ISO/AVC";
      tr->avc = 1;
      mk->cluster_maxsize = 10000000;
      break;

    case SCT_HEVC:
      tracktype = 1;
      codec_id = "V_MPEGH/ISO/HEVC";
      mk->cluster_maxsize = 20000000;
      tr->hevc = 1;
      break;

    case SCT_VP8:
      tracktype = 1;
      codec_id = "V_VP8";
      mk->cluster_maxsize = 10000000;
      break;

    case SCT_VP9:
      tracktype = 1;
      codec_id = "V_VP9";
      mk->cluster_maxsize = 10000000;
      break;

    case SCT_THEORA:
      tracktype = 1;
      codec_id = "V_THEORA";
      mk->cluster_maxsize = 5242880;
      break;

    case SCT_MPEG2AUDIO:
      tracktype = 2;
      codec_id  = "A_MPEG/L2";
      if (ssc->es_audio_version == 3)
        codec_id = "A_MPEG/L3";
      else if (ssc->es_audio_version == 1)
        codec_id = "A_MPEG/L1";
      break;

    case SCT_AC3:
      tracktype = 2;
      codec_id = "A_AC3";
      break;

    case SCT_EAC3:
      tracktype = 2;
      codec_id = "A_EAC3";
      break;

    case SCT_AC4:
      tracktype = 2;
      codec_id = "A_AC4";
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

    case SCT_OPUS:
      tracktype = 2;
      codec_id = "A_OPUS";
      break;

    case SCT_FLAC:
      tracktype = 2;
      codec_id = "A_FLAC";
      break;

    case SCT_DVBSUB:
      if (mk->dvbsub_skip)
        goto disable;
      tracktype = 0x11;
      codec_id = "S_DVBSUB";
      break;

    case SCT_TEXTSUB:
      tracktype = 0x11;
      codec_id = "S_TEXT/UTF8";
      break;

    default:
disable:
      ssc->ssc_muxer_disabled = 1;
      tr->disabled = 1;
      continue;
    }

    tr->tracknum = ++tracknum;
    tr->tracktype = tracktype;
    mk->has_video |= (tracktype == 1);

    t = htsbuf_queue_alloc(0);

    ebml_append_uint(t, 0xd7, tr->tracknum);
    ebml_append_uint(t, 0x73c5, tr->tracknum);
    ebml_append_uint(t, 0x83, tracktype);
    ebml_append_uint(t, 0x9c, 0); // Lacing
    ebml_append_string(t, 0x86, codec_id);

    if(ssc->es_lang[0])
      ebml_append_string(t, 0x22b59c, ssc->es_lang);

    switch(ssc->es_type) {
    case SCT_HEVC:
    case SCT_H264:
    case SCT_MPEG2VIDEO:
    case SCT_MP4A:
    case SCT_AAC:
    case SCT_OPUS:
      if(ssc->ssc_gh) {
        sbuf_t hdr;
        sbuf_init(&hdr);
        if (tr->avc) {
          isom_write_avcc(&hdr, pktbuf_ptr(ssc->ssc_gh),
                                pktbuf_len(ssc->ssc_gh));
        } else if (tr->hevc) {
          isom_write_hvcc(&hdr, pktbuf_ptr(ssc->ssc_gh),
                                pktbuf_len(ssc->ssc_gh));
        } else {
          sbuf_append(&hdr, pktbuf_ptr(ssc->ssc_gh),
                            pktbuf_len(ssc->ssc_gh));
        }
        ebml_append_bin(t, 0x63a2, hdr.sb_data, hdr.sb_ptr);
        sbuf_free(&hdr);
      }
      break;

    case SCT_THEORA:
    case SCT_VORBIS:
      if(ssc->ssc_gh) {
        htsbuf_queue_t *cp;
        uint8_t *header_start[3];
        int header_len[3];
        int j;
        int first_header_size = ssc->es_type == SCT_VORBIS ? 30 : 42;

        if(mk_split_xiph_headers(pktbuf_ptr(ssc->ssc_gh), pktbuf_len(ssc->ssc_gh),
                                 first_header_size, header_start, header_len)) {
          tvherror(LS_MKV, "failed to split xiph headers");
          break;
        }
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
      buf4[0] = ssc->es_composition_id >> 8;
      buf4[1] = ssc->es_composition_id;
      buf4[2] = ssc->es_ancillary_id >> 8;
      buf4[3] = ssc->es_ancillary_id;
      ebml_append_bin(t, 0x63a2, buf4, 4);
      break;
    }

    if(SCT_ISVIDEO(ssc->es_type)) {
      htsbuf_queue_t *vi = htsbuf_queue_alloc(0);

      if(ssc->es_frame_duration) {
        int d = ts_rescale(ssc->es_frame_duration, 1000000000);
        ebml_append_uint(t, 0x23e383, d);
      }
      ebml_append_uint(vi, 0xb0, ssc->es_width);
      ebml_append_uint(vi, 0xba, ssc->es_height);

      if (ssc->es_aspect_num && ssc->es_aspect_den) {
        if (mk->webm) {
          ebml_append_uint(vi, 0x54b0, (ssc->es_height * ssc->es_aspect_num) / ssc->es_aspect_den);
          ebml_append_uint(vi, 0x54ba, ssc->es_height);
          ebml_append_uint(vi, 0x54b2, 0); // DisplayUnit: pixels because DAR is not supported by webm
        } else {
          ebml_append_uint(vi, 0x54b0, ssc->es_aspect_num);
          ebml_append_uint(vi, 0x54ba, ssc->es_aspect_den);
          ebml_append_uint(vi, 0x54b2, 3); // DisplayUnit: DAR
        }
      }

      ebml_append_master(t, 0xe0, vi);
    }

    if(SCT_ISAUDIO(ssc->es_type)) {
      htsbuf_queue_t *au = htsbuf_queue_alloc(0);

      ebml_append_float(au, 0xb5, sri_to_rate(ssc->es_sri));
      if (ssc->es_ext_sri)
        ebml_append_float(au, 0x78b5, sri_to_rate(ssc->es_ext_sri - 1));
      ebml_append_uint(au, 0x9f, ssc->es_channels);
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
mk_write_to_fd(mk_muxer_t *mk, htsbuf_queue_t *hq)
{
  htsbuf_data_t *hd;
  int i = 0;
  off_t oldpos = mk->fdpos;

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
    if((r = writev(mk->fd, iov, iovcnt)) == -1) {
      if (ERRNO_AGAIN(errno))
        continue;
      mk->error = errno;
      return -1;
    }
    mk->fdpos += r;
    i -= iovcnt;
    iov += iovcnt;
  } while(i);

  if (mk->seekable)
    muxer_cache_update((muxer_t *)mk, mk->fd, oldpos, 0);

  return 0;
}


/**
 *
 */
static void
mk_write_queue(mk_muxer_t *mk, htsbuf_queue_t *q)
{
  if(!mk->error && mk_write_to_fd(mk, q) && !MC_IS_EOS_ERROR(mk->error))
    tvherror(LS_MKV, "%s: Write failed -- %s", mk->filename, strerror(errno));

  htsbuf_queue_flush(q);
}


/**
 *
 */
static void
mk_write_master(mk_muxer_t *mk, uint32_t id, htsbuf_queue_t *p)
{
  htsbuf_queue_t q;
  htsbuf_queue_init(&q, 0);
  ebml_append_master(&q, id, p);
  mk_write_queue(mk, &q);
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
mk_write_segment_header(mk_muxer_t *mk, int64_t size)
{
  htsbuf_queue_t *q;
  q = mk_build_segment_header(size);
  mk_write_queue(mk, q);
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
mk_build_edition_entry(mk_muxer_t *mk)
{
  mk_chapter_t *ch;
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_uint(q, 0x45bd, 0); //EditionFlagHidden
  ebml_append_uint(q, 0x45db, 1); //EditionFlagDefault

  TAILQ_FOREACH(ch, &mk->chapters, link) {
    ebml_append_master(q, 0xB6, mk_build_one_chapter(ch));
  }

  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_chapters(mk_muxer_t *mk)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_master(q, 0x45b9, mk_build_edition_entry(mk));

  return q;
}

/**
 *
 */
static void
mk_write_chapters(mk_muxer_t *mk)
{
  if(TAILQ_FIRST(&mk->chapters) == NULL)
    return;

  mk->chapters_pos = mk->fdpos;
  mk_write_master(mk, 0x1043a770, mk_build_chapters(mk));   //0x1043a770 = Chapters
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
  ebml_append_uint(t, 0x68ca, targettype ?: 50);      //0x68ca = TargetTypeValue

  if(targettypename)
    ebml_append_string(t, 0x63ca, targettypename);    //0x63ca = TargetType
  ebml_append_master(q, 0x63c0, t);                   //0x63c0 = Targets

  ebml_append_string(st, 0x45a3, name);               //0x45a3 = TagName
  ebml_append_string(st, 0x4487, value);              //0x4487 = TagString
  ebml_append_uint(st, 0x4484, 1);                    //0x4484 = TagDefault
  ebml_append_string(st, 0x447a, lang ?: "und");      //0x447a = TagLanguage

  ebml_append_master(q, 0x67c8, st);                  //0x67c8 = SimpleTag
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

  //Note: May 2025 DMC - If the recording is manual or from a timer
  //then only the 'comment' will be available, 'de' and 'ebc' will be null.

  htsbuf_queue_t *q = htsbuf_queue_alloc(0);
  char datestr[64], ctype[100], temp_rating[128];
  const epg_genre_t *eg = NULL;
  epg_genre_t eg0;
  struct tm tm;
  time_t t;
  channel_t *ch = ebc ? ebc->channel : NULL;
  lang_str_t *ls = NULL, *ls2 = NULL, *ls3 = NULL;
  const char *lang;
  const lang_code_list_t *langs;
  epg_episode_num_t num;
  
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
  } else if (ebc) {
    eg = LIST_FIRST(&ebc->genre);
  }
  if(eg && epg_genre_get_str(eg, 1, 0, ctype, 100, NULL))
    addtag(q, build_tag_string("CONTENT_TYPE", ctype, NULL, 0, NULL));

  //Only add the 'LAW_RATING' tag if 1) rating label processing is enabled globally
  //AND 2) this EPG entry has a rating label worth adding.
  if(epggrab_conf.epgdb_processparentallabels && ebc && ebc->rating_label)
  {
    //If the rating label for this event is enabled AND it has a display_label.
    if(ebc->rating_label->rl_enabled && ebc->rating_label->rl_display_label)
    {
      //Build a fallback string with only the rating label.
      snprintf(temp_rating, sizeof(temp_rating), "%s", ebc->rating_label->rl_display_label);

      //If there is a country code, build a string that includes the country code
      //eg: 'PG (AUS)'
      if(ebc->rating_label->rl_enabled && ebc->rating_label->rl_country)
      {
        snprintf(temp_rating, sizeof(temp_rating), "%s (%s)", ebc->rating_label->rl_display_label, ebc->rating_label->rl_country);
      }
      //Otherwise, if there is a authority name, build a string that includes the authority
      //eg: 'PG (ACMA)'
      else if (ebc->rating_label->rl_enabled && ebc->rating_label->rl_authority)
      {
        snprintf(temp_rating, sizeof(temp_rating), "%s (%s)", ebc->rating_label->rl_display_label, ebc->rating_label->rl_authority);
      }

      addtag(q, build_tag_string("LAW_RATING", temp_rating, NULL, 0, NULL));
    }
  }//END we got a rating label from the EPG

  if(ch)
    addtag(q, build_tag_string("TVCHANNEL",
                               channel_get_name(ch, channel_blank_name), NULL, 0, NULL));

  if (ebc && ebc->summary)
    ls = ebc->summary;
  if(de && de->de_desc)
    ls2 = de->de_desc;
  else if (ebc && ebc->description)
    ls2 = ebc->description;

  if(de && de->de_subtitle)
    ls3 = de->de_subtitle;
  else if (ebc && ebc->subtitle)
    ls3 = ebc->subtitle;

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
  
  if (ls3) {
    lang_str_ele_t *e;
    RB_FOREACH(e, ls3, link)
      addtag(q, build_tag_string("SUBTITLE", e->str, e->lang, 0, NULL));
  }
  
  epg_broadcast_get_epnum(ebc, &num);
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

  if (comment) {
    lang = "eng";
    if ((langs = lang_code_split(NULL)) != NULL) {
      lang = tvh_strdupa(langs->codes[0]->code2b);
    }
    addtag(q, build_tag_string("COMMENT", comment, lang, 0, NULL));
  }
  else if (de && de->de_comment) {  //Add the DVR recording comment if one exists and an explicit comment is omitted
    //If the DVR entry has a Title, 'borrow' the language code(s) for the comment
    if (de->de_title) {
      lang_str_ele_t *e;
      RB_FOREACH(e, de->de_title, link)
        addtag(q, build_tag_string("COMMENT", de->de_comment, e->lang, 0, NULL));
    }
    else //Otherwise, just use 'eng' as the default.
    {
      lang = "eng";
      if ((langs = lang_code_split(NULL)) != NULL) {
        lang = tvh_strdupa(langs->codes[0]->code2b);
      }

      addtag(q, build_tag_string("COMMENT", de->de_comment, lang, 0, NULL));      
    }
  }//END extra comment

  return q;
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_one_metaseek(mk_muxer_t *mk, uint32_t id, off_t pos)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  ebml_append_idid(q, 0x53ab, id);
  ebml_append_uint(q, 0x53ac, pos - mk->segment_pos);
  return q;
}


/**
 *
 */
static void
mk_append_master(mk_muxer_t *mk, htsbuf_queue_t *q,
                 off_t id1, off_t id2, off_t val)
{
  ebml_append_master(q, id1, mk_build_one_metaseek(mk, id2, val));

}

/**
 *
 */
static htsbuf_queue_t *
mk_build_metaseek(mk_muxer_t *mk)
{
  htsbuf_queue_t *q = htsbuf_queue_alloc(0);

  if(mk->segmentinfo_pos)
    mk_append_master(mk, q, 0x4dbb, 0x1549a966, mk->segmentinfo_pos);
  if(mk->trackinfo_pos)
    mk_append_master(mk, q, 0x4dbb, 0x1654ae6b, mk->trackinfo_pos);
  if(mk->metadata_pos)
    mk_append_master(mk, q, 0x4dbb, 0x1254c367, mk->metadata_pos);
  if(mk->cue_pos)
    mk_append_master(mk, q, 0x4dbb, 0x1c53bb6b, mk->cue_pos);
  if(mk->chapters_pos)
    mk_append_master(mk, q, 0x4dbb, 0x1043a770, mk->chapters_pos);
  return q;
}


/**
 *
 */
static void
mk_write_metaseek(mk_muxer_t *mk, int first)
{
  htsbuf_queue_t q;

  htsbuf_queue_init(&q, 0);

  ebml_append_master(&q, 0x114d9b74, mk_build_metaseek(mk));

  assert(q.hq_size < 498);

  ebml_append_pad(&q, 500 - q.hq_size);

  if(first) {
    mk_write_to_fd(mk, &q);
  } else if(mk->seekable) {
    off_t prev = mk->fdpos;
    mk->fdpos = mk->segment_pos;
    if(lseek(mk->fd, mk->segment_pos, SEEK_SET) == (off_t) -1)
      mk->error = errno;

    mk_write_queue(mk, &q);
    mk->fdpos = prev;
    if(lseek(mk->fd, mk->fdpos, SEEK_SET) == (off_t) -1)
      mk->error = errno;
  }
  htsbuf_queue_flush(&q);
}


/**
 *
 */
static htsbuf_queue_t *
mk_build_segment(mk_muxer_t *mk,
		 streaming_start_t *ss)
{
  htsbuf_queue_t *p = htsbuf_queue_alloc(0);

  assert(mk->segment_pos != 0);

  // Meta seek
  if(mk->seekable)
    ebml_append_pad(p, 500);

  mk->segmentinfo_pos = mk->segment_pos + p->hq_size;
  ebml_append_master(p, 0x1549a966, mk_build_segment_info(mk));

  mk->trackinfo_pos = mk->segment_pos + p->hq_size;
  ebml_append_master(p, 0x1654ae6b, mk_build_tracks(mk, ss));

  return p;
}


/**
 *
 */
static void
addcue(mk_muxer_t *mk, int64_t pts, int tracknum)
{
  mk_cue_t *mc = malloc(sizeof(mk_cue_t));
  mc->ts = pts;
  mc->tracknum = tracknum;
  mc->cluster_pos = mk->cluster_pos;
  TAILQ_INSERT_TAIL(&mk->cues, mc, link);
}


/**
 *
 */
static void
mk_add_chapter0(mk_muxer_t *mk, uint32_t uuid, int64_t ts)
{
  mk_chapter_t *ch;

  ch = malloc(sizeof(mk_chapter_t));

  ch->uuid = uuid;
  ch->ts = ts;

  TAILQ_INSERT_TAIL(&mk->chapters, ch, link);
}

/**
 *
 */
static void
mk_add_chapter(mk_muxer_t *mk, int64_t ts)
{
  mk_chapter_t *ch;
  int uuid;

  ch = TAILQ_LAST(&mk->chapters, mk_chapter_queue);
  if(ch) {
    /* don't add a new chapter if the previous one was added less than 5s ago */
    if(ts - ch->ts < 5000)
      return;

    uuid = ch->uuid + 1;
  } else {
    uuid = 1;
    /* create first chapter at zero position */
    if (ts >= 5000) {
      mk_add_chapter0(mk, uuid++, 0);
    } else {
      ts = 0;
    }
  }
  mk_add_chapter0(mk, uuid, ts);
}

/**
 *
 */
static void
mk_close_cluster(mk_muxer_t *mk)
{
  if(mk->cluster != NULL)
    mk_write_master(mk, 0x1f43b675, mk->cluster);
  mk->cluster = NULL;
  mk->cluster_last_close = mclk();
}


/**
 *
 */
static void
mk_write_frame_i(mk_muxer_t *mk, mk_track_t *t, th_pkt_t *pkt)
{
  int64_t pts = pkt->pkt_pts, delta, nxt;
  unsigned char c_delta_flags[3];
  const int video = t->tracktype == 1;
  const int audio = t->tracktype == 2;
  int keyframe = 0, skippable = 0;

  if (video) {
    keyframe  = pkt->v.pkt_frametype < PKT_P_FRAME;
    skippable = pkt->v.pkt_frametype == PKT_B_FRAME;
  }

  uint8_t *data = pktbuf_ptr(pkt->pkt_payload);
  size_t len = pktbuf_len(pkt->pkt_payload);

  if(!data || len <= 0)
    return;

  if(pts == PTS_UNSET)
    // This is our best guess, it might be wrong but... oh well
    pts = t->nextpts;

  if(pts != PTS_UNSET) {
    t->nextpts = pts + (pkt->pkt_duration >> (video ? pkt->v.pkt_field : 0));

    nxt = ts_rescale(t->nextpts, 1000000000 / MATROSKA_TIMESCALE);
    pts = ts_rescale(pts,        1000000000 / MATROSKA_TIMESCALE);

    if((t->tracktype == 1 || t->tracktype == 2) && mk->totduration < nxt)
      mk->totduration = nxt;

    delta = pts - mk->cluster_tc;
    if((keyframe || !mk->has_video) && (delta > 30000ll || delta < -30000ll))
      mk_close_cluster(mk);

  } else {
    return;
  }

  if(mk->cluster) {

    if(keyframe &&
       (mk->cluster->hq_size > mk->cluster_maxsize ||
        mk->cluster_last_close + sec2mono(1) < mclk()))
      mk_close_cluster(mk);

    else if(!mk->has_video &&
            (mk->cluster->hq_size > mk->cluster_maxsize/40 ||
             mk->cluster_last_close + sec2mono(1) < mclk()))
      mk_close_cluster(mk);

    else if(mk->cluster->hq_size > mk->cluster_maxsize)
      mk_close_cluster(mk);

  }

  if(mk->cluster == NULL) {
    mk->cluster_tc = pts;
    mk->cluster = htsbuf_queue_alloc(0);

    mk->cluster_pos = mk->fdpos;
    mk->addcue = 1;

    ebml_append_uint(mk->cluster, 0xe7, mk->cluster_tc);
    delta = 0;
  }

  if(mk->addcue && keyframe) {
    mk->addcue = 0;
    addcue(mk, pts, t->tracknum);
  }

  if(t->type == SCT_AAC || t->type == SCT_MP4A) {
    // Skip ADTS header
    if(len < 7)
      return;

    len -= 7;
    data += 7;
  }

  ebml_append_id(mk->cluster, 0xa3 ); // SimpleBlock
  ebml_append_size(mk->cluster, len + 4);
  ebml_append_size(mk->cluster, t->tracknum);

  c_delta_flags[0] = delta >> 8;
  c_delta_flags[1] = delta;
  if (audio && pkt->a.pkt_keyframe) keyframe = 1;
  c_delta_flags[2] = (keyframe << 7) | skippable;
  htsbuf_append(mk->cluster, c_delta_flags, 3);
  htsbuf_append(mk->cluster, data, len);
}


/**
 *
 */
static void
mk_write_cues(mk_muxer_t *mk)
{
  mk_cue_t *mc;
  htsbuf_queue_t *q, *c, *p;

  if(TAILQ_FIRST(&mk->cues) == NULL)
    return;

  q = htsbuf_queue_alloc(0);

  while((mc = TAILQ_FIRST(&mk->cues)) != NULL) {
    TAILQ_REMOVE(&mk->cues, mc, link);

    c = htsbuf_queue_alloc(0);

    ebml_append_uint(c, 0xb3, mc->ts);

    p = htsbuf_queue_alloc(0);
    ebml_append_uint(p, 0xf7, mc->tracknum);
    ebml_append_uint(p, 0xf1, mc->cluster_pos - mk->segment_pos);

    ebml_append_master(c, 0xb7, p);
    ebml_append_master(q, 0xbb, c);
    free(mc);
  }

  mk->cue_pos = mk->fdpos;
  mk_write_master(mk, 0x1c53bb6b, q);
}


/**
 * Append a packet to the muxer
 */
static int
mk_mux_write_pkt(mk_muxer_t *mk, th_pkt_t *pkt)
{
  int i, mark;
  mk_track_t *t = NULL;
  th_pkt_t *opkt, *tpkt;

  for (i = 0; i < mk->ntracks; i++) {
    t = &mk->tracks[i];
    if (t->index == pkt->pkt_componentindex && !t->disabled)
      break;
  }

  if(i >= mk->ntracks || pkt->pkt_payload == NULL) {
    pkt_ref_dec(pkt);
    return mk->error;
  }

  if (mk->dvbsub_reorder &&
      pkt->pkt_type == SCT_DVBSUB &&
      pts_diff(pkt->pkt_pcr, pkt->pkt_pts) > 90000) {
    tvhtrace(LS_MKV, "insert pkt to holdq: pts %"PRId64", pcr %"PRId64", diff %"PRId64"\n", pkt->pkt_pcr, pkt->pkt_pts, pts_diff(pkt->pkt_pcr, pkt->pkt_pts));
    pktref_enqueue_sorted(&mk->holdq, pkt, mk_pktref_cmp);
    return mk->error;
  }

  mark = 0;
  if(SCT_ISAUDIO(pkt->pkt_type)) {
    while ((opkt = pktref_first(&mk->holdq)) != NULL) {
      if (pts_diff(pkt->pkt_pts, opkt->pkt_pts) > 90000)
        break;
      opkt = pktref_get_first(&mk->holdq);
      tvhtrace(LS_MKV, "hold push, pts %"PRId64", audio pts %"PRId64"\n", opkt->pkt_pts, pkt->pkt_pts);
      tpkt = pkt_copy_shallow(opkt);
      pkt_ref_dec(opkt);
      tpkt->pkt_pcr = tpkt->pkt_pts;
      mk_mux_write_pkt(mk, tpkt);
    }
    if(pkt->a.pkt_channels != t->channels &&
       pkt->a.pkt_channels) {
      mark = 1;
      t->channels = pkt->a.pkt_channels;
    }
    if(pkt->a.pkt_sri != t->sri &&
       pkt->a.pkt_sri) {
      mark = 1;
      t->sri = pkt->a.pkt_sri;
      t->ext_sri = pkt->a.pkt_ext_sri;
    }
  } else if (SCT_ISVIDEO(pkt->pkt_type)) {
    if(pkt->v.pkt_aspect_num != t->aspect_num &&
       pkt->v.pkt_aspect_num) {
      mark = 1;
      t->aspect_num = pkt->v.pkt_aspect_num;
    }
    if(pkt->v.pkt_aspect_den != t->aspect_den &&
       pkt->v.pkt_aspect_den) {
      mark = 1;
      t->aspect_den = pkt->v.pkt_aspect_den;
    }
  }
  if(pkt->pkt_commercial != t->commercial &&
     pkt->pkt_commercial != COMMERCIAL_UNKNOWN) {
    mark = 1;
    t->commercial = pkt->pkt_commercial;
  }

  if(mark)
    mk_mux_insert_chapter(mk);

  if(t->hevc) {
    pkt = hevc_convert_pkt(opkt = pkt);
    pkt_ref_dec(opkt);
  } else if(t->avc) {
    pkt = avc_convert_pkt(opkt = pkt);
    pkt_ref_dec(opkt);
  }

  mk_write_frame_i(mk, t, pkt);

  pkt_ref_dec(pkt);

  return mk->error;
}


/**
 * Insert a new chapter at the current location
 */
static int
mk_mux_insert_chapter(mk_muxer_t *mk)
{
  if(mk->totduration != PTS_UNSET)
    mk_add_chapter(mk, mk->totduration);

  return mk->error;
}


/**
 * Close the muxer
 */
static int
mk_mux_close(mk_muxer_t *mk)
{
  int64_t totsize;
  mk_close_cluster(mk);
  mk_write_cues(mk);
  mk_write_chapters(mk);

  mk_write_metaseek(mk, 0);
  totsize = mk->fdpos;

  if(mk->seekable) {
    // Rewrite segment info to update duration
    if(lseek(mk->fd, mk->segmentinfo_pos, SEEK_SET) == mk->segmentinfo_pos)
      mk_write_master(mk, 0x1549a966, mk_build_segment_info(mk));
    else {
      mk->error = errno;
      tvherror(LS_MKV, "%s: Unable to write duration, seek failed -- %s",
	       mk->filename, strerror(errno));
    }

    // Rewrite segment header to update total size
    if(lseek(mk->fd, mk->segment_header_pos, SEEK_SET) == mk->segment_header_pos) {
      mk_write_segment_header(mk, totsize - mk->segment_header_pos - 12);
    } else {
      mk->error = errno;
      tvherror(LS_MKV, "%s: Unable to write total size, seek failed -- %s",
	       mk->filename, strerror(errno));
    }

    if(close(mk->fd)) {
      mk->error = errno;
      tvherror(LS_MKV, "%s: Unable to close the file descriptor, close failed -- %s",
	       mk->filename, strerror(errno));
    }
  }

  return mk->error;
}

/**
 * Figure out the mimetype
 */
static const char*
mkv_muxer_mime(muxer_t* m, const struct streaming_start *ss)
{
  int i;
  int has_audio;
  int has_video;
  const streaming_start_component_t *ssc;

  has_audio = 0;
  has_video = 0;

  for(i=0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    if(ssc->ssc_disabled)
      continue;

    has_video |= SCT_ISVIDEO(ssc->es_type);
    has_audio |= SCT_ISAUDIO(ssc->es_type);
  }

  if(has_video)
    return muxer_container_type2mime(m->m_config.m_type, 1);
  else if(has_audio)
    return muxer_container_type2mime(m->m_config.m_type, 0);
  else
    return muxer_container_type2mime(MC_UNKNOWN, 0);
}

/**
 * Init the muxer with a title and some tracks
 */
static int
mkv_muxer_init(muxer_t *m, streaming_start_t *ss, const char *name)
{
  mk_muxer_t *mk = (mk_muxer_t *)m;
  htsbuf_queue_t q, *a;

  uuid_random(mk->uuid, sizeof(mk->uuid));

  if(name)
    mk->title = strdup(name);
  else
    mk->title = strdup(mk->filename);

  TAILQ_INIT(&mk->cues);
  TAILQ_INIT(&mk->chapters);

  htsbuf_queue_init(&q, 0);

  ebml_append_master(&q, 0x1a45dfa3, mk_build_ebmlheader(mk));

  mk->segment_header_pos = q.hq_size;
  a = mk_build_segment_header(0);
  htsbuf_appendq(&q, a);
  htsbuf_queue_free(a);

  mk->segment_pos = q.hq_size;
  a = mk_build_segment(mk, ss);
  htsbuf_appendq(&q, a);
  htsbuf_queue_free(a);

  mk_write_queue(mk, &q);

  htsbuf_queue_flush(&q);

  return mk->error;
}


/**
 * Insert a new chapter at the current location
 */
static int
mkv_muxer_add_marker(muxer_t* m)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;

  if(mk_mux_insert_chapter(mk)) {
    mk->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Multisegment matroska files do exist but I am not sure if they are supported
 * by many media players. For now, we'll treat it as an error.
 */
static int
mkv_muxer_reconfigure(muxer_t* m, const struct streaming_start *ss)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;

  mk->m_errors++;

  return -1;
}


/**
 * Open the muxer as a stream muxer (using a non-seekable socket)
 */
static int
mkv_muxer_open_stream(muxer_t *m, int fd)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;

  mk->filename = strdup("Live stream");
  mk->fd = fd;
  mk->cluster_maxsize = 0;
  mk->totduration = 0;

  return 0;
}


/**
 * Open a file
 */
static int
mkv_muxer_open_file(muxer_t *m, const char *filename)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;
  int fd, permissions = mk->m_config.m_file_permissions;

  tvhtrace(LS_MKV, "Creating file \"%s\" with file permissions \"%o\"",
           filename, permissions);

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, permissions);

  if(fd < 0) {
    mk->error = errno;
    tvherror(LS_MKV, "%s: Unable to create file, open failed -- %s",
	     mk->filename, strerror(errno));
    mk->m_errors++;
    return -1;
  }

  /* bypass umask settings */
  if (fchmod(fd, permissions))
    tvherror(LS_MKV, "%s: Unable to change permissions -- %s",
             filename, strerror(errno));

  mk->filename = strdup(filename);
  mk->fd = fd;
  mk->cluster_maxsize = 2000000;
  mk->seekable = 1;
  mk->totduration = 0;

  return 0;
}


/**
 * Write a packet to the muxer
 */
static int
mkv_muxer_write_pkt(muxer_t *m, streaming_message_type_t smt, void *data)
{
  th_pkt_t *pkt = (th_pkt_t*)data;
  mk_muxer_t *mk = (mk_muxer_t*)m;
  int r;

  assert(smt == SMT_PACKET);

  if((r = mk_mux_write_pkt(mk, pkt)) != 0) {
    if (MC_IS_EOS_ERROR(r))
      mk->m_eos = 1;
    mk->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Append meta data when a channel changes its programme
 */
static int
mkv_muxer_write_meta(muxer_t *m, struct epg_broadcast *eb,
                     const char *comment)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;
  htsbuf_queue_t q;

  if(!mk->metadata_pos)
    mk->metadata_pos = mk->fdpos;

  htsbuf_queue_init(&q, 0);
  ebml_append_master(&q, 0x1254c367, _mk_build_metadata(NULL, eb, comment));
  mk_write_queue(mk, &q);

  if (mk->error) {
    mk->m_errors++;
    return -1;
  }

  return 0;
}


/**
 * Close the muxer and append trailer to output
 */
static int
mkv_muxer_close(muxer_t *m)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;

  if(mk_mux_close(mk)) {
    mk->m_errors++;
    return -1;
  }

  pktref_clear_queue(&mk->holdq);

  return 0;
}


/**
 * Free all memory associated with the muxer
 */
static void
mkv_muxer_destroy(muxer_t *m)
{
  mk_muxer_t *mk = (mk_muxer_t*)m;
  mk_chapter_t *ch;

  pktref_clear_queue(&mk->holdq);

  while((ch = TAILQ_FIRST(&mk->chapters)) != NULL) {
    TAILQ_REMOVE(&mk->chapters, ch, link);
    free(ch);
  }

  free(mk->filename);
  free(mk->tracks);
  free(mk->title);
  muxer_config_free(&mk->m_config);
  muxer_hints_free(mk->m_hints);
  free(mk);
}

/**
 * Create a new builtin muxer
 */
muxer_t*
mkv_muxer_create(const muxer_config_t *m_cfg,
                 const muxer_hints_t *hints)
{
  mk_muxer_t *mk;
  const char *agent = hints ? hints->mh_agent : NULL;

  if(m_cfg->m_type != MC_MATROSKA && m_cfg->m_type != MC_WEBM)
    return NULL;

  mk = calloc(1, sizeof(mk_muxer_t));
  mk->m_open_stream  = mkv_muxer_open_stream;
  mk->m_open_file    = mkv_muxer_open_file;
  mk->m_mime         = mkv_muxer_mime;
  mk->m_init         = mkv_muxer_init;
  mk->m_reconfigure  = mkv_muxer_reconfigure;
  mk->m_add_marker   = mkv_muxer_add_marker;
  mk->m_write_meta   = mkv_muxer_write_meta;
  mk->m_write_pkt    = mkv_muxer_write_pkt;
  mk->m_close        = mkv_muxer_close;
  mk->m_destroy      = mkv_muxer_destroy;
  mk->webm           = m_cfg->m_type == MC_WEBM;
  mk->dvbsub_reorder = m_cfg->u.mkv.m_dvbsub_reorder;
  mk->fd             = -1;

  /*
   * VLC has no support for MKV S_DVBSUB codec format
   */
  if (agent)
    mk->dvbsub_skip  = strstr(agent, "LibVLC/") != NULL;

  TAILQ_INIT(&mk->holdq);

  return (muxer_t*)mk;
}
