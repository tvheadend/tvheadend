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

#include <libhts/htsstr.h>

#include "tvhead.h"
#include "streaming.h"
#include "dvr.h"
#include "spawn.h"

typedef struct dvr_rec_stream {
  LIST_ENTRY(dvr_rec_stream) drs_link;

  int drs_source_index;
  AVStream *drs_lavf_stream;
  
  int drs_decoded;

} dvr_rec_stream_t;


/**
 *
 */
static void dvr_rec_start(dvr_entry_t *de, streaming_pad_t *sp);
static void dvr_rec_stop(dvr_entry_t *de);
static void *dvr_thread(void *aux);
static void dvr_thread_new_pkt(dvr_entry_t *de, th_pkt_t *pkt);
static void dvr_spawn_postproc(dvr_entry_t *de);
static void dvr_thread_epilog(dvr_entry_t *de);

/**
 *
 */
static void
dvr_subscription_callback(struct th_subscription *s,
			  subscription_event_t event, void *opaque)
{
  dvr_entry_t *de = opaque;
  const char *notifymsg = NULL;
  th_transport_t *t;

  switch(event) {
  case SUBSCRIPTION_EVENT_INVALID:
    abort();

  case SUBSCRIPTION_TRANSPORT_RUN:
    t = s->ths_transport;
    dvr_rec_start(de, &t->tht_streaming_pad);
    return;

  case SUBSCRIPTION_NO_INPUT:
    notifymsg = "No input detected";
    break;

  case SUBSCRIPTION_NO_DESCRAMBLER:
    notifymsg = "No descrambler available";
    break;

  case SUBSCRIPTION_NO_ACCESS:
    notifymsg = "Access denied";
    break;

  case SUBSCRIPTION_RAW_INPUT:
    notifymsg = "Unable to reassemble packets from input";
    break;

  case SUBSCRIPTION_VALID_PACKETS:
    return;

  case SUBSCRIPTION_TRANSPORT_NOT_AVAILABLE:
    notifymsg = "No transport available at the moment, automatic retry";
    break;

  case SUBSCRIPTION_TRANSPORT_LOST:
    dvr_rec_stop(de);
    notifymsg = "Lost transport";
    break;

  case SUBSCRIPTION_DESTROYED:
    dvr_rec_stop(de); /* Recording completed */
    return;
  }
  if(notifymsg != NULL)
    tvhlog(LOG_WARNING, "dvr", "\"%s\" on \"%s\": %s",
	   de->de_title, de->de_channel->ch_name, notifymsg);
}


/**
 *
 */
void
dvr_rec_subscribe(dvr_entry_t *de)
{
  if(de->de_s != NULL)
    return;

  de->de_s = subscription_create_from_channel(de->de_channel, 1000, "pvr",
					      dvr_subscription_callback, de, 
					      0);
  
  
}

/**
 *
 */
void
dvr_rec_unsubscribe(dvr_entry_t *de)
{
  if(de->de_s == NULL)
    return;

  subscription_unsubscribe(de->de_s);
  de->de_s = NULL;
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

  tvhlog(LOG_DEBUG, "dvr", "Unable to create directory \"%s\" -- %s",
	 path, strerror(r));
  return r;
}


/**
 * Replace any slash chars in a string with dash
 */
static void
deslashify(char *s)
{
  int i, len = strlen(s);
  for(i = 0; i < len; i++) if(s[i]  == '/')
    s[i] = '-';
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
  char *chname;
  struct tm tm;

  filename = strdup(de->de_ititle);
  deslashify(filename);

  av_strlcpy(path, dvr_storage, sizeof(path));

  /* Append per-day directory */

  if(dvr_flags & DVR_DIR_PER_DAY) {
    localtime_r(&de->de_start, &tm);
    strftime(fullname, sizeof(fullname), "%F", &tm);
    deslashify(fullname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", fullname);
  }

  /* Append per-channel directory */

  if(dvr_flags & DVR_DIR_PER_CHANNEL) {

    chname = strdup(de->de_channel->ch_name);
    deslashify(chname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", chname);
    free(chname);
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
  
}


/**
 *
 */
static void
dvr_rec_start(dvr_entry_t *de, streaming_pad_t *sp)
{
  streaming_component_t *sc;
  dvr_rec_stream_t *drs;
  AVOutputFormat *fmt;
  AVFormatContext *fctx;
  AVCodecContext *ctx;
  AVCodec *codec;
  enum CodecID codec_id;
  enum CodecType codec_type;
  const char *codec_name;
  char urlname[512];
  int err;
  pthread_t ptid;
  pthread_attr_t attr;

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

  fctx = av_alloc_format_context();

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
    dvr_rec_fatal_error(de, "Unable to create output file \"%s\"",
			de->de_filename);
    return;
  }

  av_set_parameters(fctx, NULL);


  pthread_mutex_lock(sp->sp_mutex);

  /**
   * Setup each stream
   */ 
  LIST_FOREACH(sc, &sp->sp_components, sc_link) {

    switch(sc->sc_type) {
    default:
      continue;
    case SCT_MPEG2VIDEO:
      codec_id   = CODEC_ID_MPEG2VIDEO;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "mpeg2 video";
      break;

    case SCT_MPEG2AUDIO:
      codec_id   = CODEC_ID_MP2;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "mpeg2 audio";
      break;

    case SCT_AC3:
      codec_id   = CODEC_ID_AC3;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "AC3 audio";
      break;

    case SCT_H264:
      codec_id   = CODEC_ID_H264;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "h.264 video";
      break;
    }

    codec = avcodec_find_decoder(codec_id);
    if(codec == NULL) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot find codec for %s, ignoring stream", 
	     de->de_ititle, codec_name);
      continue;
    }

    drs = calloc(1, sizeof(dvr_rec_stream_t));
    drs->drs_source_index = sc->sc_index;

    drs->drs_lavf_stream = av_new_stream(fctx, fctx->nb_streams);

    ctx = drs->drs_lavf_stream->codec;
    ctx->codec_id   = codec_id;
    ctx->codec_type = codec_type;

    if(avcodec_open(ctx, codec) < 0) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot open codec for %s, ignoring stream", 
	     de->de_ititle, codec_name);
      free(ctx);
      continue;
    }

    memcpy(drs->drs_lavf_stream->language, sc->sc_lang, 4);
    LIST_INSERT_HEAD(&de->de_streams, drs, drs_link);
  }

  /* Link to the pad */
  
  streaming_target_init(&de->de_st, NULL, NULL);
  streaming_target_connect(sp, &de->de_st);
  de->de_st.st_status = ST_RUNNING;
  de->de_fctx = fctx;
  de->de_ts_offset = AV_NOPTS_VALUE;

  pthread_mutex_unlock(sp->sp_mutex);

  de->de_refcnt++;
  
  /* Start the recorder thread */
  
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_create(&ptid, &attr, dvr_thread, de);

}


/**
 * Called from subscription callback when we no longer have
 * access to stream
 */
static void
dvr_rec_stop(dvr_entry_t *de)
{
  streaming_target_t *st = &de->de_st;
  
  streaming_target_disconnect(&de->de_st);

  pthread_mutex_lock(&st->st_mutex);

  if(st->st_status == ST_RUNNING) {
    st->st_status = ST_STOP_REQ;

    pthread_cond_signal(&st->st_cond);

    while(st->st_status != ST_ZOMBIE)
      pthread_cond_wait(&st->st_cond, &st->st_mutex);
  }
  
  pktref_clear_queue(&st->st_queue);
  pthread_mutex_unlock(&st->st_mutex);
}


/**
 *
 */
static void *
dvr_thread(void *aux)
{
  dvr_entry_t *de = aux;
  streaming_target_t *st = &de->de_st;
  th_pktref_t *pr;

  pthread_mutex_lock(&st->st_mutex);

  de->de_header_written = 0;
  de->de_rec_state = DE_RS_WAIT_AUDIO_LOCK;

  while(st->st_status == ST_RUNNING) {

    pr = TAILQ_FIRST(&st->st_queue);
    if(pr == NULL) {
      pthread_cond_wait(&st->st_cond, &st->st_mutex);
      continue;
    }
    
    TAILQ_REMOVE(&st->st_queue, pr, pr_link);

    pthread_mutex_unlock(&st->st_mutex);

    if(dispatch_clock > de->de_start)
      dvr_thread_new_pkt(de, pr->pr_pkt);

    pkt_ref_dec(pr->pr_pkt);
    free(pr);

    pthread_mutex_lock(&st->st_mutex);
  }

  /* Signal back that we no longer is running */
  st->st_status = ST_ZOMBIE;
  pthread_cond_signal(&st->st_cond);

  pthread_mutex_unlock(&st->st_mutex);

  dvr_thread_epilog(de);

  pthread_mutex_lock(&global_lock);
  dvr_entry_dec_ref(de);                    /* Past this we may no longer
					       dereference de */
  pthread_mutex_unlock(&global_lock);

  /* Fade out ... */
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

  case DE_RS_WAIT_AUDIO_LOCK:
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
      de->de_rec_state = DE_RS_WAIT_VIDEO_LOCK;
    break;
    
  case DE_RS_WAIT_VIDEO_LOCK:
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

    de->de_rec_state = DE_RS_RUNNING;

    if(!de->de_header_written) {

      if(av_write_header(fctx)) {
	tvhlog(LOG_ERR, "dvr",
	       "%s - Unable to write header", 
	       de->de_ititle);
 	break;
      }

      de->de_header_written = 1;

      tvhlog(LOG_ERR, "dvr",
	     "%s - Header written to file, stream dump:", 
	     de->de_ititle);
      
      for(i = 0; i < fctx->nb_streams; i++) {
	stx = fctx->streams[i];
	
	avcodec_string(txt, sizeof(txt), stx->codec, 1);
	
	tvhlog(LOG_ERR, "dvr", "%s - Stream #%d: %s [%d/%d]",
	       de->de_ititle, i, txt, 
	       stx->time_base.num, stx->time_base.den);
	
      }
    }
    /* FALLTHRU */
      
  case DE_RS_RUNNING:
     
    if(de->de_header_written == 0)
      break;

    if(pkt->pkt_commercial == COMMERCIAL_YES &&
       st->codec->codec->type == CODEC_TYPE_VIDEO &&
       pkt->pkt_frametype == PKT_I_FRAME) {
      
      de->de_ts_com_start = pkt->pkt_dts;

      de->de_rec_state = DE_RS_COMMERCIAL;
      break;
    }
  outputpacket:
    dts = pkt->pkt_dts - de->de_ts_offset - de->de_ts_com_offset;
    pts = pkt->pkt_pts - de->de_ts_offset - de->de_ts_com_offset;
    if(dts < 0 || pts < 0)
      break;

    av_init_packet(&avpkt);
    avpkt.stream_index = st->index;

    avpkt.dts = av_rescale_q(dts, AV_TIME_BASE_Q, st->time_base);
    avpkt.pts = av_rescale_q(pts, AV_TIME_BASE_Q, st->time_base);
    avpkt.data = buf;
    avpkt.size = bufsize;
    avpkt.duration =
      av_rescale_q(pkt->pkt_duration, AV_TIME_BASE_Q, st->time_base);
    avpkt.flags = pkt->pkt_frametype >= PKT_P_FRAME ? 0 : PKT_FLAG_KEY;
    r = av_interleaved_write_frame(fctx, &avpkt);
    break;


  case DE_RS_COMMERCIAL:

    if(pkt->pkt_commercial != COMMERCIAL_YES &&
       st->codec->codec->type == CODEC_TYPE_VIDEO &&
       pkt->pkt_frametype == PKT_I_FRAME) {

      /* Switch out of commercial mode */
      
      de->de_ts_com_offset += (pkt->pkt_dts - de->de_ts_com_start);
      de->de_rec_state = DE_RS_RUNNING;

      tvhlog(LOG_ERR, "dvr", "%s - Skipped %lld seconds of commercials",
	     de->de_ititle, (pkt->pkt_dts - de->de_ts_com_start) / 1000000);
      goto outputpacket;
    }
    break;
  }
}

/**
 *
 */
static void dvr_spawn_postproc(dvr_entry_t *de)
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
  snprintf(start, sizeof(start), "%ld", de->de_start);
  snprintf(stop, sizeof(stop), "%ld", de->de_stop);

  memset(fmap, 0, sizeof(fmap));
  fmap['f'] = de->de_filename; /* full path to recoding */
  fmap['b'] = basename(fbasename); /* basename of recoding */
  fmap['c'] = de->de_channel->ch_name; /* channel name */
  fmap['C'] = de->de_creator; /* user who created this recording */
  fmap['t'] = de->de_title; /* program title */
  fmap['d'] = de->de_desc; /* program description */
  fmap['e'] = de->de_error; /* error message, empty if no error (FIXME:?) */
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

  assert(fctx != NULL);

  /* Write trailer if we've written anything at all */
    
  if(de->de_header_written)
    av_write_trailer(fctx);

  /* Close lavf streams and format */
  
  for(i = 0; i < fctx->nb_streams; i++) {
    st = fctx->streams[i];
    avcodec_close(st->codec);
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
