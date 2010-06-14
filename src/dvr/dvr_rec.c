/*
 *  Digital Video Recorder
 *  Copyright (C) 2008 Andreas Öman
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

#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h> /* basename */

#include <libavutil/avstring.h>
#include <libavcodec/avcodec.h>

#include "htsstr.h"

#include "tvhead.h"
#include "streaming.h"
#include "dvr.h"
#include "spawn.h"
#include "transports.h"

static const AVRational mpeg_tc = {1, 90000};

typedef struct dvr_rec_stream {
  LIST_ENTRY(dvr_rec_stream) drs_link;

  int drs_source_index;
  AVStream *drs_lavf_stream;
  
  int drs_decoded;

} dvr_rec_stream_t;


/**
 *
 */
static void *dvr_thread(void *aux);
static void dvr_thread_new_pkt(dvr_entry_t *de, th_pkt_t *pkt);
static void dvr_spawn_postproc(dvr_entry_t *de);
static void dvr_thread_epilog(dvr_entry_t *de);


const static int prio2weight[5] = {
  [DVR_PRIO_IMPORTANT]   = 500,
  [DVR_PRIO_HIGH]        = 400,
  [DVR_PRIO_NORMAL]      = 300,
  [DVR_PRIO_LOW]         = 200,
  [DVR_PRIO_UNIMPORTANT] = 100,
};

/**
 *
 */
void
dvr_rec_subscribe(dvr_entry_t *de)
{
  char buf[100];
  int weight;

  assert(de->de_s == NULL);

  snprintf(buf, sizeof(buf), "DVR: %s", de->de_title);

  streaming_queue_init(&de->de_sq);

  pthread_create(&de->de_thread, NULL, dvr_thread, de);

  if(de->de_pri < 5)
    weight = prio2weight[de->de_pri];
  else
    weight = 300;

  de->de_s = subscription_create_from_channel(de->de_channel, weight,
					      buf, &de->de_sq.sq_st, 0);
}

/**
 *
 */
void
dvr_rec_unsubscribe(dvr_entry_t *de, int stopcode)
{
  assert(de->de_s != NULL);

  subscription_unsubscribe(de->de_s);

  streaming_target_deliver(&de->de_sq.sq_st, streaming_msg_create(SMT_EXIT));
  
  pthread_join(de->de_thread, NULL);
  de->de_s = NULL;

  de->de_last_error = stopcode;
}


/**
 *
 */
static int
makedirs(const char *path)
{
  struct stat st;
  char *p;
  int l, r;

  if(stat(path, &st) == 0 && S_ISDIR(st.st_mode)) 
    return 0; /* Dir already there */

  if(mkdir(path, 0777) == 0)
    return 0; /* Dir created ok */

  if(errno == ENOENT) {

    /* Parent does not exist, try to create it */
    /* Allocate new path buffer and strip off last directory component */

    l = strlen(path);
    p = alloca(l + 1);
    memcpy(p, path, l);
    p[l--] = 0;
  
    for(; l >= 0; l--)
      if(p[l] == '/')
	break;
    if(l == 0) {
      return ENOENT;
    }
    p[l] = 0;

    if((r = makedirs(p)) != 0)
      return r;
  
    /* Try again */
    if(mkdir(path, 0777) == 0)
      return 0; /* Dir created ok */
  }
  r = errno;

  tvhlog(LOG_ERR, "dvr", "Unable to create directory \"%s\" -- %s",
	 path, strerror(r));
  return r;
}


/**
 * Replace various chars with a dash
 */
static void
cleanupfilename(char *s)
{
  int i, len = strlen(s);
  for(i = 0; i < len; i++) { 
    if(s[i] == '/' || s[i] == ':' || s[i] == '\\' || s[i] == '<' ||
       s[i] == '>' || s[i] == '|' || s[i] == '*' || s[i] == '?')
      s[i] = '-';

    if((dvr_flags & DVR_WHITESPACE_IN_TITLE) && s[i] == ' ')
      s[i] = '-';	
  }
}

/**
 * Filename generator
 *
 * - convert from utf8
 * - avoid duplicate filenames
 *
 */
static int
pvr_generate_filename(dvr_entry_t *de)
{
  char fullname[1000];
  char path[500];
  int tally = 0;
  struct stat st;
  char *filename;
  struct tm tm;

  filename = strdup(de->de_ititle);
  cleanupfilename(filename);

  av_strlcpy(path, dvr_storage, sizeof(path));

  /* Append per-day directory */

  if(dvr_flags & DVR_DIR_PER_DAY) {
    localtime_r(&de->de_start, &tm);
    strftime(fullname, sizeof(fullname), "%F", &tm);
    cleanupfilename(fullname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", fullname);
  }

  /* Append per-channel directory */

  if(dvr_flags & DVR_DIR_PER_CHANNEL) {

    char *chname = strdup(de->de_channel->ch_name);
    cleanupfilename(chname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", chname);
    free(chname);
  }

  /* Append per-title directory */

  if(dvr_flags & DVR_DIR_PER_TITLE) {

    char *title = strdup(de->de_title);
    cleanupfilename(title);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", title);
    free(title);
  }


  /* */
  if(makedirs(path) != 0)
    return -1;
  

  /* Construct final name */
  
  snprintf(fullname, sizeof(fullname), "%s/%s.%s",
	   path, filename, dvr_file_postfix);

  while(1) {
    if(stat(fullname, &st) == -1) {
      tvhlog(LOG_DEBUG, "pvr", "File \"%s\" -- %s -- Using for recording",
	     fullname, strerror(errno));
      break;
    }

    tvhlog(LOG_DEBUG, "pvr", "Overwrite protection, file \"%s\" exists", 
	   fullname);

    tally++;

    snprintf(fullname, sizeof(fullname), "%s/%s-%d.%s",
	     path, filename, tally, dvr_file_postfix);
  }

  tvh_str_set(&de->de_filename, fullname);

  free(filename);
  return 0;
}

/**
 *
 */
static void
dvr_rec_fatal_error(dvr_entry_t *de, const char *fmt, ...)
{
  char msgbuf[256];

  va_list ap;
  va_start(ap, fmt);

  vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
  va_end(ap);

  tvhlog(LOG_ERR, "pvr", 
	 "Recording error: \"%s\": %s",
	 de->de_filename ?: de->de_title, msgbuf);
}


/**
 *
 */
static void
dvr_rec_set_state(dvr_entry_t *de, dvr_rs_state_t newstate, int error)
{
  de->de_rec_state = newstate;
  de->de_last_error = error;
  if(error)
    de->de_errors++;
  dvr_entry_notify(de);
}

/**
 *
 */
static void
dvr_rec_start(dvr_entry_t *de, const streaming_start_t *ss)
{
  const source_info_t *si = &ss->ss_si;
  dvr_rec_stream_t *drs;
  AVOutputFormat *fmt;
  AVFormatContext *fctx;
  AVCodecContext *ctx;
  AVCodec *codec;
  enum CodecID codec_id;
  enum CodecType codec_type;
  const char *codec_name;
  char urlname[512];
  int err, r;
  int i;

  if(pvr_generate_filename(de) != 0) {
    dvr_rec_fatal_error(de, "Unable to create directories");
    return;
  }

  /* Find lavf format */

  fmt = guess_format(dvr_format, NULL, NULL);
  if(fmt == NULL) {
    dvr_rec_fatal_error(de, "Unable to open file format \"%s\" for output",
			dvr_format);
    return;
  }

  /* Init format context */

  fctx = avformat_alloc_context();

  av_strlcpy(fctx->title, de->de_ititle ?: "", 
	     sizeof(fctx->title));

  av_strlcpy(fctx->comment, de->de_desc ?: "", 
	     sizeof(fctx->comment));

  av_strlcpy(fctx->copyright, de->de_channel->ch_name, 
	     sizeof(fctx->copyright));

  fctx->oformat = fmt;

  /* Open output file */

  snprintf(urlname, sizeof(urlname), "file:%s", de->de_filename);

  if((err = url_fopen(&fctx->pb, urlname, URL_WRONLY)) < 0) {
    av_free(fctx);
    dvr_rec_fatal_error(de, "Unable to create output file \"%s\". "
			"FFmpeg error %d", urlname, err);
    return;
  }

  av_set_parameters(fctx, NULL);


  /**
   * Setup each stream
   */ 
  for(i = 0; i < ss->ss_num_components; i++) {
    const streaming_start_component_t *ssc = &ss->ss_components[i];

    switch(ssc->ssc_type) {
    case SCT_AC3:
      codec_id = CODEC_ID_AC3;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "AC-3";
      break;
    case SCT_MPEG2AUDIO:
      codec_id = CODEC_ID_MP2;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "MPEG";
      break;
    case SCT_MPEG2VIDEO:
      codec_id = CODEC_ID_MPEG2VIDEO;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "MPEG-2";
      break;
    case SCT_H264:
      codec_id = CODEC_ID_H264;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "H264";
      break;
    default:
      continue;
    }

    codec = avcodec_find_decoder(codec_id);
    if(codec == NULL) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot find codec for %s, ignoring stream", 
	     de->de_ititle, codec_name);
      continue;
    }

    drs = calloc(1, sizeof(dvr_rec_stream_t));
    drs->drs_source_index = ssc->ssc_index;

    drs->drs_lavf_stream = av_new_stream(fctx, fctx->nb_streams);

    ctx = drs->drs_lavf_stream->codec;
    ctx->codec_id   = codec_id;
    ctx->codec_type = codec_type;

    pthread_mutex_lock(&ffmpeg_lock);
    r = avcodec_open(ctx, codec);
    pthread_mutex_unlock(&ffmpeg_lock);

    if(r < 0) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot open codec for %s, ignoring stream", 
	     de->de_ititle, codec_name);
      free(ctx);
      continue;
    }

    memcpy(drs->drs_lavf_stream->language, ssc->ssc_lang, 4);

    LIST_INSERT_HEAD(&de->de_streams, drs, drs_link);
  }

  de->de_fctx = fctx;
  de->de_ts_offset = AV_NOPTS_VALUE;


  tvhlog(LOG_INFO, "dvr", "%s from "
	 "adapter: \"%s\", "
	 "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	 "service: \"%s\"",
	 de->de_ititle,
	 si->si_adapter  ?: "<N/A>",
	 si->si_network  ?: "<N/A>",
	 si->si_mux      ?: "<N/A>",
	 si->si_provider ?: "<N/A>",
	 si->si_service  ?: "<N/A>");
}


/**
 *
 */
static void *
dvr_thread(void *aux)
{
  dvr_entry_t *de = aux;
  streaming_queue_t *sq = &de->de_sq;
  streaming_message_t *sm;
  int run = 1;

  pthread_mutex_lock(&sq->sq_mutex);

  while(run) {
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      pthread_cond_wait(&sq->sq_cond, &sq->sq_mutex);
      continue;
    }
    
    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);

    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {
    case SMT_PACKET:
      if(dispatch_clock > de->de_start - (60 * de->de_start_extra))
	dvr_thread_new_pkt(de, sm->sm_data);
      pkt_ref_dec(sm->sm_data);
      break;

    case SMT_START:
      assert(de->de_fctx == NULL);

      pthread_mutex_lock(&global_lock);
      dvr_rec_start(de, sm->sm_data);
      de->de_header_written = 0;
      dvr_rec_set_state(de, DVR_RS_WAIT_AUDIO_LOCK, 0);
      pthread_mutex_unlock(&global_lock);
      break;

    case SMT_STOP:

      if(sm->sm_code == 0) {
	/* Completed */

	de->de_last_error = 0;

	tvhlog(LOG_INFO, 
	       "pvr", "Recording completed: \"%s\"",
	       de->de_filename ?: de->de_title);

      } else {

	if(de->de_last_error != sm->sm_code) {
	  dvr_rec_set_state(de, DVR_RS_ERROR, sm->sm_code);

	  tvhlog(LOG_ERR,
		 "pvr", "Recording stopped: \"%s\": %s",
		 de->de_filename ?: de->de_title,
		 streaming_code2txt(sm->sm_code));
	}
      }

      dvr_thread_epilog(de);
      break;

    case SMT_TRANSPORT_STATUS:
      printf("TSS: %x\n", sm->sm_code);
      if(sm->sm_code & TSS_PACKETS) {
	
      } else if(sm->sm_code & (TSS_GRACEPERIOD | TSS_ERRORS)) {

	int code = SM_CODE_UNDEFINED_ERROR;


	if(sm->sm_code & TSS_NO_DESCRAMBLER)
	  code = SM_CODE_NO_DESCRAMBLER;

	if(sm->sm_code & TSS_NO_ACCESS)
	  code = SM_CODE_NO_ACCESS;

	if(de->de_last_error != code) {
	  dvr_rec_set_state(de, DVR_RS_ERROR, code);
	  tvhlog(LOG_ERR,
		 "pvr", "Streaming error: \"%s\": %s",
		 de->de_filename ?: de->de_title,
		 streaming_code2txt(code));
	}
      }
      break;

    case SMT_NOSTART:

      if(de->de_last_error != sm->sm_code) {
	dvr_rec_set_state(de, DVR_RS_ERROR, sm->sm_code);

	tvhlog(LOG_ERR,
	       "pvr", "Recording unable to start: \"%s\": %s",
	       de->de_filename ?: de->de_title,
	       streaming_code2txt(sm->sm_code));
      }
      break;

    case SMT_MPEGTS:
      break;

    case SMT_EXIT:
      run = 0;
      break;
    }

    free(sm);
    pthread_mutex_lock(&sq->sq_mutex);
  }
  pthread_mutex_unlock(&sq->sq_mutex);
  return NULL;
}


/** 
 * Check if all streams of the given type has been decoded
 */
static int
is_all_decoded(dvr_entry_t *de, enum CodecType type)
{
  dvr_rec_stream_t *drs;
  AVStream *st;

  LIST_FOREACH(drs, &de->de_streams, drs_link) {
    st = drs->drs_lavf_stream;
    if(st->codec->codec->type == type && drs->drs_decoded == 0)
      return 0;
  }
  return 1;
}


/**
 *
 */
static void
dvr_thread_new_pkt(dvr_entry_t *de, th_pkt_t *pkt)
{
  AVFormatContext *fctx = de->de_fctx;
  dvr_rec_stream_t *drs;
  AVStream *st, *stx;
  AVCodecContext *ctx;
  AVPacket avpkt;
  void *abuf;
  AVFrame pic;
  int r, data_size, i;
  void *buf = pkt->pkt_payload;
  size_t bufsize = pkt->pkt_payloadlen;
  char txt[100];
  int64_t pts, dts;

  pkt = pkt_merge_global(pkt);

  LIST_FOREACH(drs, &de->de_streams, drs_link)
    if(drs->drs_source_index == pkt->pkt_componentindex)
      break;

  if(drs == NULL)
    return;

  st = drs->drs_lavf_stream;
  ctx = st->codec;

  if(de->de_ts_offset == AV_NOPTS_VALUE)
    de->de_ts_offset = pkt->pkt_dts;

  switch(de->de_rec_state) {
  default:
    break;

  case DVR_RS_WAIT_AUDIO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_AUDIO || drs->drs_decoded)
      break;

    data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    abuf = av_malloc(data_size);

    r = avcodec_decode_audio2(ctx, abuf, &data_size, buf, bufsize);
    av_free(abuf);

    if(r != 0 && data_size) {
      tvhlog(LOG_DEBUG, "dvr", "%s - "
	     "Stream #%d: \"%s\" decoded a complete audio frame: "
	     "%d channels in %d Hz",
	     de->de_ititle, st->index, ctx->codec->name,
	     ctx->channels, ctx->sample_rate);
	
      drs->drs_decoded = 1;
    }

    if(is_all_decoded(de, CODEC_TYPE_AUDIO))
      dvr_rec_set_state(de, DVR_RS_WAIT_VIDEO_LOCK, 0);
    break;
    
  case DVR_RS_WAIT_VIDEO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_VIDEO || drs->drs_decoded)
      break;

    r = avcodec_decode_video(st->codec, &pic, &data_size, buf, bufsize);
    if(r != 0 && data_size) {
      tvhlog(LOG_DEBUG, "dvr", "%s - "
	     "Stream #%d: \"%s\" decoded a complete video frame: "
	     "%d x %d at %.2fHz",
	     de->de_ititle, st->index,  ctx->codec->name,
	     ctx->width, st->codec->height,
	     (float)ctx->time_base.den / (float)ctx->time_base.num);
      
      drs->drs_decoded = 1;
      
      st->sample_aspect_ratio = st->codec->sample_aspect_ratio;
    }
    
    if(!is_all_decoded(de, CODEC_TYPE_VIDEO))
      break;

    /* All Audio & Video decoded, start recording */

    dvr_rec_set_state(de, DVR_RS_RUNNING, 0);

    if(!de->de_header_written) {

      if(av_write_header(fctx)) {
	tvhlog(LOG_ERR, "dvr",
	       "%s - Unable to write header", 
	       de->de_ititle);
 	break;
      }

      de->de_header_written = 1;

      tvhlog(LOG_INFO, "dvr",
	     "%s - Header written to file, stream dump:", 
	     de->de_ititle);
      
      for(i = 0; i < fctx->nb_streams; i++) {
	stx = fctx->streams[i];
	
	avcodec_string(txt, sizeof(txt), stx->codec, 1);
	
	tvhlog(LOG_INFO, "dvr", "%s - Stream #%d: %s [%d/%d]",
	       de->de_ititle, i, txt, 
	       stx->time_base.num, stx->time_base.den);
	
      }
    }
    /* FALLTHRU */
      
  case DVR_RS_RUNNING:
     
    if(de->de_header_written == 0)
      break;

    if(pkt->pkt_commercial == COMMERCIAL_YES &&
       st->codec->codec->type == CODEC_TYPE_VIDEO &&
       pkt->pkt_frametype == PKT_I_FRAME) {
      
      de->de_ts_com_start = pkt->pkt_dts;

      dvr_rec_set_state(de, DVR_RS_COMMERCIAL, 0);
      break;
    }
  outputpacket:
    dts = pkt->pkt_dts - de->de_ts_offset - de->de_ts_com_offset;
    pts = pkt->pkt_pts - de->de_ts_offset - de->de_ts_com_offset;
    if(dts < 0 || pts < 0)
      break;

    av_init_packet(&avpkt);
    avpkt.stream_index = st->index;

    avpkt.dts = av_rescale_q(dts, mpeg_tc, st->time_base);
    avpkt.pts = av_rescale_q(pts, mpeg_tc, st->time_base);
    avpkt.data = buf;
    avpkt.size = bufsize;
    avpkt.duration = av_rescale_q(pkt->pkt_duration, mpeg_tc, st->time_base);
    avpkt.flags = pkt->pkt_frametype >= PKT_P_FRAME ? 0 : PKT_FLAG_KEY;
    r = av_interleaved_write_frame(fctx, &avpkt);
    break;


  case DVR_RS_COMMERCIAL:

    if(pkt->pkt_commercial != COMMERCIAL_YES &&
       st->codec->codec->type == CODEC_TYPE_VIDEO &&
       pkt->pkt_frametype == PKT_I_FRAME) {

      /* Switch out of commercial mode */
      
      de->de_ts_com_offset += (pkt->pkt_dts - de->de_ts_com_start);
      dvr_rec_set_state(de, DVR_RS_RUNNING, 0);

      tvhlog(LOG_INFO, "dvr", 
	     "%s - Skipped %" PRId64 " seconds of commercials",
	     de->de_ititle, (pkt->pkt_dts - de->de_ts_com_start) / 90000);
      goto outputpacket;
    }
    break;
  }
}

/**
 *
 */
static void
dvr_spawn_postproc(dvr_entry_t *de)
{
  char *fmap[256];
  char **args;
  char start[16];
  char stop[16];
  char *fbasename; /* filename dup for basename */
  int i;

  args = htsstr_argsplit(dvr_postproc);
  /* no arguments at all */
  if(!args[0]) {
    htsstr_argsplit_free(args);
    return;
  }

  fbasename = strdup(de->de_filename); 
  snprintf(start, sizeof(start), "%ld", de->de_start - de->de_start_extra);
  snprintf(stop, sizeof(stop),   "%ld", de->de_stop  + de->de_stop_extra);

  memset(fmap, 0, sizeof(fmap));
  fmap['f'] = de->de_filename; /* full path to recoding */
  fmap['b'] = basename(fbasename); /* basename of recoding */
  fmap['c'] = de->de_channel->ch_name; /* channel name */
  fmap['C'] = de->de_creator; /* user who created this recording */
  fmap['t'] = de->de_title; /* program title */
  fmap['d'] = de->de_desc; /* program description */
  /* error message, empty if no error (FIXME:?) */
  fmap['e'] = tvh_strdupa(streaming_code2txt(de->de_last_error));
  fmap['S'] = start; /* start time, unix epoch */
  fmap['E'] = stop; /* stop time, unix epoch */

  /* format arguments */
  for(i = 0; args[i]; i++) {
    char *s;

    s = htsstr_format(args[i], fmap);
    free(args[i]);
    args[i] = s;
  }
  
  spawnv(args[0], (void *)args);
    
  free(fbasename);
  htsstr_argsplit_free(args);
}

/**
 *
 */
static void
dvr_thread_epilog(dvr_entry_t *de)
{
  AVFormatContext *fctx = de->de_fctx;
  dvr_rec_stream_t *drs;
  AVStream *st;
  int i;

  if(fctx == NULL)
    return;

  /* Write trailer if we've written anything at all */
    
  if(de->de_header_written)
    av_write_trailer(fctx);

  /* Close lavf streams and format */
  
  for(i = 0; i < fctx->nb_streams; i++) {
    st = fctx->streams[i];
    pthread_mutex_lock(&ffmpeg_lock);
    avcodec_close(st->codec);
    pthread_mutex_unlock(&ffmpeg_lock);
    free(st->codec);
    free(st);
  }
  
  url_fclose(fctx->pb);
  free(fctx);

  de->de_fctx = NULL;

  while((drs = LIST_FIRST(&de->de_streams)) != NULL) {
    LIST_REMOVE(drs, drs_link);
    free(drs);
  }

  if(dvr_postproc)
    dvr_spawn_postproc(de);
}
