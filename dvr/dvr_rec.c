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

#include <libavutil/avstring.h>

#include "tvhead.h"
#include "dvr.h"

static void dvr_transport_available(dvr_entry_t *de);
static void dvr_transport_unavailable(dvr_entry_t *de, const char *errmsg);

/**
 *
 */
static void
dvr_subscription_callback(struct th_subscription *s,
			  subscription_event_t event, void *opaque)
{
  dvr_entry_t *de = opaque;
  const char *notifymsg = NULL;

  switch(event) {
  case SUBSCRIPTION_EVENT_INVALID:
    abort();

  case SUBSCRIPTION_TRANSPORT_RUN:
    dvr_transport_available(de);
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
    dvr_transport_unavailable(de, "Lost transport");
    return;

  case SUBSCRIPTION_DESTROYED:
    dvr_transport_unavailable(de, NULL); /* Recording completed */
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
dvr_rec_start(dvr_entry_t *de)
{
  if(de->de_s != NULL)
    return;

  de->de_s = subscription_create_from_channel(de->de_channel, 1000, "pvr",
					      dvr_subscription_callback, de);
  
  
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
static void
pvr_generate_filename(dvr_entry_t *de)
{
  char fullname[1000];
  int tally = 0;
  struct stat st;
  char *chname;
  char *filename;

  if(de->de_filename != NULL) {
    free(de->de_filename);
    de->de_filename = NULL;
  }

  filename = strdup(de->de_title);
  deslashify(filename);

  chname = strdup(de->de_channel->ch_name);
  deslashify(chname);

  snprintf(fullname, sizeof(fullname), "%s/%s-%s.%s",
	   dvr_storage, chname, filename, dvr_file_postfix);

  while(1) {
    if(stat(fullname, &st) == -1) {
      tvhlog(LOG_DEBUG, "pvr", "File \"%s\" -- %s -- Using for recording",
	     fullname, strerror(errno));
      break;
    }

    tvhlog(LOG_DEBUG, "pvr", "Overwrite protection, file \"%s\" exists", 
	   fullname);

    tally++;

    snprintf(fullname, sizeof(fullname), "%s/%s-%s-%d.%s",
	     dvr_storage, chname, filename, tally,dvr_file_postfix);
  }

  de->de_filename = strdup(fullname);

  free(filename);
  free(chname);
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
dvr_transport_available(dvr_entry_t *de)
{
  AVOutputFormat *fmt;
  AVFormatContext *fctx;
  th_stream_t *st;
  th_muxstream_t *tms;
  AVCodecContext *ctx;
  AVCodec *codec;
  enum CodecID codec_id;
  enum CodecType codec_type;
  const char *codec_name;
  char urlname[512];
  int err;

  th_transport_t *t = de->de_s->ths_transport;

  pvr_generate_filename(de);

  /* Find lavf format */

  fmt = guess_format(dvr_format, NULL, NULL);
  if(fmt == NULL) {
    dvr_rec_fatal_error(de, "Unable to open file format \"%s\" for output",
			dvr_format);
    return;
  }

  /* Init format context */

  fctx = av_alloc_format_context();

  av_strlcpy(fctx->title, de->de_title ?: "", 
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


  /**
   * Setup each stream
   */ 
  LIST_FOREACH(st, &t->tht_streams, st_link) {
    switch(st->st_type) {
    default:
      continue;
    case HTSTV_MPEG2VIDEO:
      codec_id   = CODEC_ID_MPEG2VIDEO;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "mpeg2 video";
      break;

    case HTSTV_MPEG2AUDIO:
      codec_id   = CODEC_ID_MP2;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "mpeg2 audio";
      break;

    case HTSTV_AC3:
      codec_id   = CODEC_ID_AC3;
      codec_type = CODEC_TYPE_AUDIO;
      codec_name = "AC3 audio";
      break;

    case HTSTV_H264:
      codec_id   = CODEC_ID_H264;
      codec_type = CODEC_TYPE_VIDEO;
      codec_name = "h.264 video";
      break;
    }

    codec = avcodec_find_decoder(codec_id);
    if(codec == NULL) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot find codec for %s, ignoring stream", 
	     de->de_title, codec_name);
      continue;
    }

    tms = calloc(1, sizeof(th_muxstream_t));
    tms->tms_stream   = st;
    tms->tms_index = fctx->nb_streams;

    tms->tms_avstream = av_new_stream(fctx, fctx->nb_streams);

    ctx = tms->tms_avstream->codec;
    ctx->codec_id   = codec_id;
    ctx->codec_type = codec_type;

    if(avcodec_open(ctx, codec) < 0) {
      tvhlog(LOG_ERR, "dvr",
	     "%s - Cannot open codec for %s, ignoring stream", 
	     de->de_title, codec_name);
      free(ctx);
      continue;
    }

    //    LIST_INSERT_HEAD(&tm->tm_streams, tms, tms_muxer_link0);
    memcpy(tms->tms_avstream->language, tms->tms_stream->st_lang, 4);
  }

}

/**
 *
 */
static void
dvr_transport_unavailable(dvr_entry_t *de, const char *errmsg)
{


}
