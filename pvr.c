/*
 *  Private Video Recorder
 *  Copyright (C) 2007 Andreas Öman
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avstring.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "htsclient.h"
#include "pvr.h"
#include "epg.h"
#include "dispatch.h"
#include "buffer.h"

struct pvr_rec_list pvrr_global_list;

static void pvr_database_load(void);
static void pvr_unrecord(pvr_rec_t *pvrr);
static void pvrr_fsm(pvr_rec_t *pvrr);
static void pvrr_subscription_callback(struct th_subscription *s,
				       subscription_event_t event,
				       void *opaque);

static void *pvr_recorder_thread(void *aux);
static void pvrr_record_packet(pvr_rec_t *pvrr, th_pkt_t *pkt);

/****************************************************************************
 *
 * Externally visible functions
 *
 */

void
pvr_init(void)
{
  pvr_database_load();
}


char 
pvr_prog_status(event_t *e)
{
  pvr_rec_t *pvrr;

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link) {
    if(pvrr->pvrr_start   >= e->e_start &&
       pvrr->pvrr_stop    <= e->e_start + e->e_duration &&
       pvrr->pvrr_channel == e->e_ch) {
      return pvrr->pvrr_status;
    }
  }
  return HTSTV_PVR_STATUS_NONE;
}



pvr_rec_t *
pvr_get_log_entry(int e)
{
  pvr_rec_t *pvrr = LIST_FIRST(&pvrr_global_list);

  while(pvrr) {
    if(e == 0)
      return pvrr;
    e--;
    pvrr = LIST_NEXT(pvrr, pvrr_global_link);
  }

  return NULL;
}

pvr_rec_t *
pvr_get_tag_entry(int e)
{
  pvr_rec_t *pvrr;

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    if(pvrr->pvrr_ref == e)
      return pvrr;
  return NULL;
}

/****************************************************************************
 *
 * work queue
 *
 */


void
pvr_inform_status_change(pvr_rec_t *pvrr)
{
  event_t *e;

  clients_send_ref(pvrr->pvrr_ref);

  e = epg_event_find_by_time(pvrr->pvrr_channel, pvrr->pvrr_start);
  
  if(e != NULL)
    clients_send_ref(e->e_tag);
}



static void
pvr_free(pvr_rec_t *pvrr)
{
  dtimer_disarm(&pvrr->pvrr_timer);
  LIST_REMOVE(pvrr, pvrr_global_link);
  free(pvrr->pvrr_title);
  free(pvrr->pvrr_desc);
  free(pvrr->pvrr_printname);
  free(pvrr->pvrr_filename);
  free(pvrr->pvrr_format);
  free(pvrr);
}




static void
pvr_unrecord(pvr_rec_t *pvrr)
{
  if(pvrr->pvrr_status == HTSTV_PVR_STATUS_SCHEDULED) {
    pvr_free(pvrr);
  } else {
    pvrr->pvrr_error = HTSTV_PVR_STATUS_ABORTED;
    pvrr_fsm(pvrr);
  }
  
  pvr_database_save();
  clients_send_ref(-1);
}



static void
pvr_link_pvrr(pvr_rec_t *pvrr)
{
  pvrr->pvrr_ref = tag_get();

  pvrr->pvrr_printname = strdup(pvrr->pvrr_title ?: "");

  LIST_INSERT_HEAD(&pvrr_global_list, pvrr, pvrr_global_link);

  switch(pvrr->pvrr_status) {
  case HTSTV_PVR_STATUS_FILE_ERROR:
  case HTSTV_PVR_STATUS_DISK_FULL:
  case HTSTV_PVR_STATUS_ABORTED:
  case HTSTV_PVR_STATUS_BUFFER_ERROR:
  case HTSTV_PVR_STATUS_NONE:
    break;

  case HTSTV_PVR_STATUS_SCHEDULED:
  case HTSTV_PVR_STATUS_RECORDING:
    pvrr->pvrr_status = HTSTV_PVR_STATUS_SCHEDULED;
    pvrr_fsm(pvrr);
    break;
  }


  pvr_inform_status_change(pvrr);
  clients_send_ref(-1);
}


void
pvr_event_record_op(th_channel_t *ch, event_t *e, recop_t op)
{
  time_t start = e->e_start;
  time_t stop  = e->e_start + e->e_duration;
  time_t now;
  pvr_rec_t *pvrr;
  char buf[100];

  time(&now);

  event_time_txt(start, e->e_duration, buf, sizeof(buf));

  if(stop < now)
    return;

  /* Try to see if we already have a scheduled or active recording */

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link) {
    if(pvrr->pvrr_channel == ch && pvrr->pvrr_start == start &&
       pvrr->pvrr_stop == stop)
      break;
  }

  if(pvrr != NULL) {
    switch(pvrr->pvrr_status) {
    case HTSTV_PVR_STATUS_ABORTED:
    case HTSTV_PVR_STATUS_NO_TRANSPONDER:
    case HTSTV_PVR_STATUS_FILE_ERROR:
    case HTSTV_PVR_STATUS_DISK_FULL:
    case HTSTV_PVR_STATUS_BUFFER_ERROR:
      pvr_free(pvrr);
      pvrr = NULL;
      break;
    }
  }

  switch(op) {
  case RECOP_TOGGLE:
    if(pvrr != NULL) {
      pvr_unrecord(pvrr);
      return;
    }
    break;

  case RECOP_CANCEL:
    if(pvrr != NULL)
      pvr_unrecord(pvrr);
    return;

  case RECOP_ONCE:
    if(pvrr != NULL)
      return;
    break;

  case RECOP_DAILY:
  case RECOP_WEEKLY:
    syslog(LOG_ERR,"Recording type not supported yet");
    return;
  }

  pvrr = calloc(1, sizeof(pvr_rec_t));
  pvrr->pvrr_status  = HTSTV_PVR_STATUS_SCHEDULED;
  pvrr->pvrr_channel = ch;
  pvrr->pvrr_start   = start;
  pvrr->pvrr_stop    = stop;
  pvrr->pvrr_title   = e->e_title ? strdup(e->e_title) : NULL;
  pvrr->pvrr_desc    = e->e_desc ?  strdup(e->e_desc)  : NULL;

  pvr_link_pvrr(pvrr);  
  pvr_database_save();
}




void
pvr_channel_record_op(th_channel_t *ch, int duration)
{
  time_t now   = dispatch_clock;
  time_t start = now;
  time_t stop  = now + duration;
  pvr_rec_t *pvrr;

  pvrr = calloc(1, sizeof(pvr_rec_t));
  pvrr->pvrr_status  = HTSTV_PVR_STATUS_SCHEDULED;
  pvrr->pvrr_channel = ch;
  pvrr->pvrr_start   = start;
  pvrr->pvrr_stop    = stop;
  pvrr->pvrr_title   = strdup("Manual recording");
  pvrr->pvrr_desc    = NULL;

  pvr_link_pvrr(pvrr);  
  pvr_database_save();
}




/*****************************************************************************
 *
 * Plain text "database" of pvr programmes
 *
 */


static char *
pvr_schedule_path_get(void)
{
  static char path[400];
  snprintf(path, sizeof(path), "%s/.pvrschedule.sav",
	   config_get_str("pvrdir", "."));
  return path;
}


void
pvr_database_save(void)
{
  pvr_rec_t *pvrr, *next;
  FILE *fp;
  struct stat st;
  char status;

  fp = fopen(pvr_schedule_path_get(), "w");
  if(fp == NULL)
    return;

  for(pvrr = LIST_FIRST(&pvrr_global_list); pvrr ; pvrr = next) {
    next = LIST_NEXT(pvrr, pvrr_global_link);

    if(pvrr->pvrr_filename) {

      /* Check if file is still around */

      if(stat(pvrr->pvrr_filename, &st) == -1) {
	/* Nope, dont save this entry, instead remove it from memory too */
	pvr_free(pvrr);
	break;
      }

      fprintf(fp, "filename %s\n", pvrr->pvrr_filename);
    }

    fprintf(fp, "channel %s\n", pvrr->pvrr_channel->ch_name);
    fprintf(fp, "start %ld\n", pvrr->pvrr_start);
    fprintf(fp, "stop %ld\n", pvrr->pvrr_stop);

    if(pvrr->pvrr_title)
      fprintf(fp, "title %s\n", pvrr->pvrr_title);

    if(pvrr->pvrr_desc)
      fprintf(fp, "desc %s\n", pvrr->pvrr_desc);

    status = pvrr->pvrr_status;


    fprintf(fp, "status %c\n", pvrr->pvrr_status);
    fprintf(fp, "end of record ------------------------------\n");
  }
  fclose(fp);
}


static void
pvr_database_load(void)
{
  char line[4000];
  pvr_rec_t *pvrr = NULL;
  FILE *fp;
  int l;
  char *key, *val;

  fp = fopen(pvr_schedule_path_get(), "r");
  if(fp == NULL) {
    perror("Cant open schedule");
    return;
  }
  while(!feof(fp)) {

    if(pvrr == NULL)
      pvrr = calloc(1, sizeof(pvr_rec_t));
    
    if(fgets(line, sizeof(line), fp) == NULL)
      break;

    l = strlen(line) - 1;

    while(l > 0 && line[l] < 32)
      line[l--] = 0;
    key = line;
    val = strchr(line, ' ');
    if(val == NULL)
      continue;
    *val++ = 0;

    if(!strcmp(key, "channel"))
      pvrr->pvrr_channel = channel_find(val, 1);
    
    else if(!strcmp(key, "start"))
      pvrr->pvrr_start = atoi(val);
      
    else if(!strcmp(key, "stop"))
      pvrr->pvrr_stop = atoi(val);

    else if(!strcmp(key, "title"))
      pvrr->pvrr_title = strdup(val);

    else if(!strcmp(key, "desc"))
      pvrr->pvrr_desc = strdup(val);

    else if(!strcmp(key, "filename"))
      pvrr->pvrr_filename = strdup(val);
    
    else if(!strcmp(key, "status"))
      pvrr->pvrr_status = *val;

    else if(!strcmp(key, "end")) {

      if(pvrr->pvrr_channel == NULL || 
	 pvrr->pvrr_start == 0 || 
	 pvrr->pvrr_stop == 0 || 
	 pvrr->pvrr_status == 0) {
	memset(pvrr, 0, sizeof(pvr_rec_t));
	continue;
      }

      pvr_link_pvrr(pvrr);
      pvrr = NULL;
    }
  }

  if(pvrr)
    free(pvrr);

  pvr_database_save();
  fclose(fp);
}

/*
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
pvr_generate_filename(pvr_rec_t *pvrr)
{
  char fullname[1000];
  char *x;
  int tally = 0;
  struct stat st;
  char *name = pvrr->pvrr_title;

  char *chname;
  char *filename;


  if(pvrr->pvrr_filename != NULL) {
    free(pvrr->pvrr_filename);
    pvrr->pvrr_filename = NULL;
  }

  free(pvrr->pvrr_format);
  pvrr->pvrr_format = strdup("matroska");

  filename = utf8tofilename(name && name[0] ? name : "untitled");
  deslashify(filename);

  chname = utf8tofilename(pvrr->pvrr_channel->ch_name);
  deslashify(chname);

  snprintf(fullname, sizeof(fullname), "%s/%s-%s.%s",
	   config_get_str("pvrdir", "."), chname, filename, pvrr->pvrr_format);

  while(1) {
    if(stat(fullname, &st) == -1) {
      syslog(LOG_DEBUG, "pvr: File \"%s\" -- %s -- Using for recording",
	     fullname, strerror(errno));
      break;
    }

    syslog(LOG_DEBUG, "pvr: Overwrite protection, file \"%s\" exists", 
	   fullname);

    tally++;
    snprintf(fullname, sizeof(fullname), "%s/%s-%s-%d.%s",
	     config_get_str("pvrdir", "."), chname, filename, tally,
	     pvrr->pvrr_format);

  }

  pvrr->pvrr_filename = strdup(fullname);

  if(pvrr->pvrr_printname != NULL)
    free(pvrr->pvrr_printname);

  x = strrchr(pvrr->pvrr_filename, '/');
  pvrr->pvrr_printname = strdup(x ? x + 1 :  pvrr->pvrr_filename);

  free(filename);
  free(chname);
}



static void
pvrr_fsm_timeout(void *aux, int64_t now)
{
  pvr_rec_t *pvrr = aux;
  pvrr_fsm(pvrr);
}



static void
pvrr_fsm(pvr_rec_t *pvrr)
{
  time_t delta;
  time_t now;

  dtimer_disarm(&pvrr->pvrr_timer);

  time(&now);

  switch(pvrr->pvrr_status) {
  case HTSTV_PVR_STATUS_NONE:
    break;

  case HTSTV_PVR_STATUS_SCHEDULED:
    delta = pvrr->pvrr_start - 30 - now;
    
    if(delta > 0) {
      dtimer_arm(&pvrr->pvrr_timer, pvrr_fsm_timeout, pvrr, delta);
      break;
    }

    delta = pvrr->pvrr_stop - now;

    if(delta <= 0) {
      syslog(LOG_NOTICE, "pvr: \"%s\" - Recording skipped, "
	     "program has already come to pass", pvrr->pvrr_printname);
      pvrr->pvrr_status = HTSTV_PVR_STATUS_DONE;
      pvr_inform_status_change(pvrr);
      pvr_database_save();
      break;
    }

    /* Add a timer that fires when recording ends */

    dtimer_arm(&pvrr->pvrr_timer, pvrr_fsm_timeout, pvrr, delta);

    TAILQ_INIT(&pvrr->pvrr_pktq);
    pthread_cond_init(&pvrr->pvrr_pktq_cond, NULL);
    pthread_mutex_init(&pvrr->pvrr_pktq_mutex, NULL);

    pvrr->pvrr_status = HTSTV_PVR_STATUS_RECORDING;
    pvr_inform_status_change(pvrr);
    pvrr_set_rec_state(pvrr,PVR_REC_WAIT_SUBSCRIPTION); 

    pvrr->pvrr_s = subscription_create(pvrr->pvrr_channel, 1000, "pvr",
				       pvrr_subscription_callback,
				       pvrr);
    break;

  case HTSTV_PVR_STATUS_RECORDING:
    /* recording completed (or aborted, or failed or somthing) */
    pvrr->pvrr_status = pvrr->pvrr_error;
    pvr_inform_status_change(pvrr);
    pvr_database_save();

    subscription_unsubscribe(pvrr->pvrr_s);
    dtimer_disarm(&pvrr->pvrr_timer);
    break;
  }
}



/*
 * PVR new packet received
 */
static void
pvrr_packet_input(th_muxer_t *tm, th_stream_t *st, th_pkt_t *pkt)
{
  pvr_rec_t *pvrr = tm->tm_opaque;

  if(pvrr->pvrr_dts_offset == AV_NOPTS_VALUE)
    pvrr->pvrr_dts_offset = pkt->pkt_dts;

  pkt = pkt_copy(pkt);

  pkt->pkt_dts -= pvrr->pvrr_dts_offset;
  pkt->pkt_pts -= pvrr->pvrr_dts_offset;

  if(pkt->pkt_dts < 0 || pkt->pkt_pts < 0) {
    pkt_deref(pkt);
    printf("trashing negative dts/pts\n");
    return;
  }

  pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);
  TAILQ_INSERT_TAIL(&pvrr->pvrr_pktq, pkt, pkt_queue_link);
  pvrr->pvrr_pktq_len++;
  pthread_cond_signal(&pvrr->pvrr_pktq_cond);
  pthread_mutex_unlock(&pvrr->pvrr_pktq_mutex);
}

/*
 *  Internal recording state
 */
void
pvrr_set_rec_state(pvr_rec_t *pvrr, pvrr_rec_status_t status)
{
  const char *tp;

  if(pvrr->pvrr_rec_status == status)
    return;

  switch(status) {
  case PVR_REC_STOP:
    tp = "stopped";
    break;
  case PVR_REC_WAIT_SUBSCRIPTION:
    tp = "waiting for subscription";
    break;
  case PVR_REC_WAIT_FOR_START:
    tp = "waiting for program start";
    break;
  case PVR_REC_WAIT_AUDIO_LOCK:
    tp = "waiting for audio lock";
    break;
  case PVR_REC_WAIT_VIDEO_LOCK:
    tp = "waiting for video lock";
    break;
  case PVR_REC_RUNNING:
    tp = "running";
    break;
  case PVR_REC_COMMERCIAL:
    tp = "commercial break";
    break;
  default:
    tp = "<invalid state>";
    break;
  }

  syslog(LOG_INFO, "pvr: \"%s\" - Recorder entering state \"%s\"",
	 pvrr->pvrr_printname, tp);

  pvrr->pvrr_rec_status = status;
}

/*
 * We've got a transport now, start recording
 */
static void
pvrr_transport_available(pvr_rec_t *pvrr, th_transport_t *t)
{
  th_muxer_t *tm = &pvrr->pvrr_muxer;
  th_stream_t *st;
  th_muxstream_t *tms;
  AVFormatContext *fctx;
  AVOutputFormat *fmt;
  AVCodecContext *ctx;
  AVCodec *codec;
  enum CodecID codec_id;
  enum CodecType codec_type;
  const char *codec_name;
  char urlname[500];
  int err;

  assert(pvrr->pvrr_rec_status == PVR_REC_WAIT_SUBSCRIPTION);

  tm->tm_opaque = pvrr;
  tm->tm_new_pkt = pvrr_packet_input;

  pvr_generate_filename(pvrr);

  /* Find lavf format */

  fmt = guess_format(pvrr->pvrr_format, NULL, NULL);
  if(fmt == NULL) {
    syslog(LOG_ERR, 
	   "pvr: \"%s\" - Unable to open file format \".%s\" for output",
	   pvrr->pvrr_printname, pvrr->pvrr_format);
    pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
    pvrr_fsm(pvrr);
    return;
  }

  /* Init format context */

  fctx = tm->tm_avfctx = av_alloc_format_context();

  av_strlcpy(fctx->title, pvrr->pvrr_title ?: "", 
	     sizeof(fctx->title));

  av_strlcpy(fctx->comment, pvrr->pvrr_desc ?: "", 
	     sizeof(tm->tm_avfctx->comment));

  av_strlcpy(fctx->copyright, pvrr->pvrr_channel->ch_name, 
	     sizeof(fctx->copyright));

  
  fctx->oformat = fmt;

  /* Open output file */

  snprintf(urlname, sizeof(urlname), "file:%s", pvrr->pvrr_filename);

  if((err = url_fopen(&fctx->pb, urlname, URL_WRONLY)) < 0) {
    syslog(LOG_ERR, 
	   "pvr: \"%s\" - Unable to create output file \"%s\" -- %s\n", 
	   pvrr->pvrr_printname, pvrr->pvrr_filename, 
	   strerror(AVUNERROR(err)));
    av_free(fctx);
    tm->tm_avfctx = NULL;
    pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
    pvrr_fsm(pvrr);
    return;
  }


  av_set_parameters(tm->tm_avfctx, NULL);

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
      syslog(LOG_ERR, 
	     "pvr: \"%s\" - Cannot find codec for %s, ignoring stream", 
	     pvrr->pvrr_printname, codec_name);
      continue;
    }

    ctx = avcodec_alloc_context();
    ctx->codec_id   = codec_id;
    ctx->codec_type = codec_type;

    if(avcodec_open(ctx, codec) < 0) {
      syslog(LOG_ERR,
	     "pvr: \"%s\" - Cannot open codec for %s, ignoring stream", 
	     pvrr->pvrr_printname, codec_name);
      free(ctx);
      continue;
    }

    tms = calloc(1, sizeof(th_muxstream_t));
    tms->tms_stream   = st;
    LIST_INSERT_HEAD(&tm->tm_media_streams, tms, tms_muxer_media_link);


    tms->tms_avstream = av_mallocz(sizeof(AVStream));
    tms->tms_avstream->codec = ctx;

    tms->tms_index = fctx->nb_streams;
    tm->tm_avfctx->streams[fctx->nb_streams] = tms->tms_avstream;
    fctx->nb_streams++;
  }

  /* Fire up recorder thread */

  pvrr_set_rec_state(pvrr, PVR_REC_WAIT_FOR_START);
  pthread_create(&pvrr->pvrr_ptid, NULL, pvr_recorder_thread, pvrr);
  LIST_INSERT_HEAD(&t->tht_muxers, tm, tm_transport_link);
}


/*
 * We've lost our transport, stop recording
 */

static void
pvrr_transport_unavailable(pvr_rec_t *pvrr, th_transport_t *t)
{
  th_muxer_t *tm = &pvrr->pvrr_muxer;
  th_muxstream_t *tms;
  th_pkt_t *pkt;
  AVFormatContext *fctx = tm->tm_avfctx;
  AVStream *avst;
  int i;

  LIST_REMOVE(tm, tm_transport_link);

  pvrr->pvrr_dts_offset = AV_NOPTS_VALUE;

  pvrr_set_rec_state(pvrr, PVR_REC_STOP);
  pthread_cond_signal(&pvrr->pvrr_pktq_cond);
  pthread_join(pvrr->pvrr_ptid, NULL);

  if(fctx != NULL) {

    /* Write trailer if we've written anything at all */
    
    if(pvrr->pvrr_header_written)
      av_write_trailer(tm->tm_avfctx);

    /* Close streams and format */

    for(i = 0; i < fctx->nb_streams; i++) {
      avst = fctx->streams[i];
      avcodec_close(avst->codec);
      free(avst->codec);
      free(avst);
    }
    
    url_fclose(&fctx->pb);
    free(fctx);
  }

  /* Remove any pending packet for queue */

  while((pkt = TAILQ_FIRST(&pvrr->pvrr_pktq)) != NULL)
    pkt_deref(pkt);

  /* Destroy muxstreams */

  while((tms = LIST_FIRST(&tm->tm_media_streams)) != NULL) {
    LIST_REMOVE(tms, tms_muxer_media_link);
    free(tms);
  }

}



/*
 * We get a callback here when the subscription status is updated, 
 * ie, when we are attached to a transport and when we are detached
 */
static void
pvrr_subscription_callback(struct th_subscription *s,
			   subscription_event_t event, void *opaque)
{
  th_transport_t *t = s->ths_transport;
  pvr_rec_t *pvrr = opaque;

  switch(event) {
  case TRANSPORT_AVAILABLE:
    pvrr_transport_available(pvrr, t);
    break;

  case TRANSPORT_UNAVAILABLE:
    pvrr_transport_unavailable(pvrr, t);
    break;
  }
}

/*
 * Recorder thread
 */
static void *
pvr_recorder_thread(void *aux)
{
  th_pkt_t *pkt;
  pvr_rec_t *pvrr = aux;
  char *t, txt2[50];
  int run = 1;
  time_t now;

  ctime_r(&pvrr->pvrr_stop, txt2);
  t = strchr(txt2, '\n');
  if(t != NULL)
    *t = 0;

  syslog(LOG_INFO, "pvr: \"%s\" - Recording started, ends at %s",
	 pvrr->pvrr_printname, txt2);
  

  pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);

  while(run) {
    switch(pvrr->pvrr_rec_status) {
    case PVR_REC_WAIT_FOR_START:

      time(&now);
      if(now >= pvrr->pvrr_start)
	pvrr->pvrr_rec_status = PVR_REC_WAIT_AUDIO_LOCK;
      break;

    case PVR_REC_WAIT_AUDIO_LOCK:
    case PVR_REC_WAIT_VIDEO_LOCK:
    case PVR_REC_RUNNING:
    case PVR_REC_COMMERCIAL:
      break;
      
    default:
      run = 0;
      continue;
    }
    
    if(pvrr->pvrr_stop < now)
      break;

    if((pkt = TAILQ_FIRST(&pvrr->pvrr_pktq)) == NULL) {
      pthread_cond_wait(&pvrr->pvrr_pktq_cond, &pvrr->pvrr_pktq_mutex);
      continue;
    }

    TAILQ_REMOVE(&pvrr->pvrr_pktq, pkt, pkt_queue_link);
    pvrr->pvrr_pktq_len--;
    pthread_mutex_unlock(&pvrr->pvrr_pktq_mutex);

    pvrr_record_packet(pvrr, pkt);
    pkt_deref(pkt);

    pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);
  }
  pthread_mutex_unlock(&pvrr->pvrr_pktq_mutex);
  
  syslog(LOG_INFO, "pvr: \"%s\" - Recording completed", 
	 pvrr->pvrr_printname);

  return NULL;
}



/** 
 * Check if all streams of the given type has been decoded
 */
static int
is_all_decoded(th_muxer_t *tm, enum CodecType type)
{
  th_muxstream_t *tms;
  AVStream *st;

  LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link) {
    st = tms->tms_avstream;
    if(st->codec->codec->type == type && tms->tms_decoded == 0)
      return 0;
  }
  return 1;
}



/** 
 * Write a packet to output file
 */
static void
pvrr_record_packet(pvr_rec_t *pvrr, th_pkt_t *pkt)
{
  th_muxer_t *tm = &pvrr->pvrr_muxer;
  AVFormatContext *fctx = tm->tm_avfctx;
  th_muxstream_t *tms;
  AVStream *st, *stx;
  AVCodecContext *ctx;
  AVPacket avpkt;
  void *abuf;
  AVFrame pic;
  int r, data_size, i;
  void *buf;
  char txt[100];
  size_t bufsize;

  LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link)
    if(tms->tms_stream == pkt->pkt_stream)
      break;

  if(tms == NULL)
    return;

  st = tms->tms_avstream;
  ctx = st->codec;

  /* Make sure packet is in memory */

  buf = pkt_payload(pkt);
  bufsize = pkt_len(pkt);
  
  switch(pvrr->pvrr_rec_status) {
  default:
    break;

  case PVR_REC_WAIT_AUDIO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_AUDIO || tms->tms_decoded)
      break;

    abuf = malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    r = avcodec_decode_audio(ctx, abuf, &data_size, buf, bufsize);
    free(abuf);

    if(r != 0 && data_size) {
      syslog(LOG_DEBUG, "pvr: \"%s\" - "
	     "Stream #%d: \"%s\" decoded a complete audio frame: "
	     "%d channels in %d Hz\n",
	     pvrr->pvrr_printname, tms->tms_index,
	     st->codec->codec->name,
	     st->codec->channels,
	     st->codec->sample_rate);
	
      tms->tms_decoded = 1;
    }

    if(is_all_decoded(tm, CODEC_TYPE_AUDIO))
      pvrr_set_rec_state(pvrr, PVR_REC_WAIT_VIDEO_LOCK);
    break;
    
  case PVR_REC_WAIT_VIDEO_LOCK:
    if(ctx->codec_type != CODEC_TYPE_VIDEO || tms->tms_decoded)
      break;

    r = avcodec_decode_video(st->codec, &pic, &data_size, buf, bufsize);
    if(r != 0 && data_size) {
      syslog(LOG_DEBUG, "pvr: \"%s\" - "
	     "Stream #%d: \"%s\" decoded a complete video frame: "
	     "%d x %d at %.2fHz\n",
	     pvrr->pvrr_printname, tms->tms_index,
	     ctx->codec->name,
	     ctx->width, st->codec->height,
	     (float)ctx->time_base.den / (float)ctx->time_base.num);
      
      tms->tms_decoded = 1;
    }
    
    if(!is_all_decoded(tm, CODEC_TYPE_VIDEO))
      break;

    /* All Audio & Video decoded, start recording */

    pvrr_set_rec_state(pvrr, PVR_REC_RUNNING);

    if(!pvrr->pvrr_header_written) {
      pvrr->pvrr_header_written = 1;

      if(av_write_header(fctx))
	break;
      
      syslog(LOG_DEBUG, 
	     "pvr: \"%s\" - Header written to file, stream dump:", 
	     pvrr->pvrr_printname);
      
      for(i = 0; i < fctx->nb_streams; i++) {
	stx = fctx->streams[i];
	
	avcodec_string(txt, sizeof(txt), stx->codec, 1);
	
	syslog(LOG_DEBUG, "pvr: \"%s\" - Stream #%d: %s [%d/%d]",
	       pvrr->pvrr_printname, i, txt, 
	       stx->time_base.num, stx->time_base.den);
	
      }
    }
    /* FALLTHRU */
      
  case PVR_REC_RUNNING:
     
    if(pkt->pkt_commercial == COMMERCIAL_YES) {
      pvrr_set_rec_state(pvrr, PVR_REC_COMMERCIAL);
      break;
    }

    av_init_packet(&avpkt);
    avpkt.stream_index = tms->tms_index;

    avpkt.dts = av_rescale_q(pkt->pkt_dts, AV_TIME_BASE_Q, st->time_base);
    avpkt.pts = av_rescale_q(pkt->pkt_pts, AV_TIME_BASE_Q, st->time_base);
    avpkt.data = buf;
    avpkt.size = bufsize;
    avpkt.duration = pkt->pkt_duration;

    r = av_interleaved_write_frame(fctx, &avpkt);
    break;


  case PVR_REC_COMMERCIAL:

    if(pkt->pkt_commercial != COMMERCIAL_YES) {

      LIST_FOREACH(tms, &tm->tm_media_streams, tms_muxer_media_link)
	tms->tms_decoded = 0;
      
      pvrr_set_rec_state(pvrr, PVR_REC_WAIT_AUDIO_LOCK);
    }
    break;
  }
}

