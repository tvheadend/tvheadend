/*
 *  Private Video Recorder, Recording functions
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <iconv.h>
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
#include "transports.h"
#include "pvr.h"
#include "epg.h"


/*
 *
 */

LIST_HEAD(ts_pid_head, ts_pid);

typedef struct ts_pid {

  LIST_ENTRY(ts_pid) tsp_link;

  int tsp_pid;
  int tsp_cc;
  int tsp_cc_errors;

  int tsp_pus;

  uint8_t *tsp_buf;
  size_t tsp_bufptr;
  size_t tsp_bufsize;

} ts_pid_t;


static int pvr_proc_tsb(pvr_rec_t *pvrr, struct ts_pid_head *pidlist,
			pvr_data_t *pd, th_subscription_t *s);

static void *pwo_init(th_subscription_t *s, pvr_rec_t *pvrr);

static int pwo_writepkt(pvr_rec_t *pvrr, th_subscription_t *s, 
			uint32_t startcode, int streamidx, uint8_t *buf, 
			size_t len, tv_streamtype_t type);


static int pwo_end(pvr_rec_t *pvrr);

static void pvr_generate_filename(pvr_rec_t *pvrr);


/*
 * Recording thread
 */

void *
pvr_recorder_thread(void *aux)
{
  pvr_rec_t *pvrr = aux;
  pvr_data_t *pd;
  char *t, txt2[50];
  int x, run = 1;
  struct ts_pid_head pids;
  ts_pid_t *tsp;
  th_subscription_t *s = pvrr->pvrr_s;
  void *opaque;
  time_t now;
  
  pvr_generate_filename(pvrr);
  

  opaque = pwo_init(s, pvrr);

  if(opaque == NULL) {
    pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
    return NULL;
  }

  ctime_r(&pvrr->pvrr_stop, txt2);
  t = strchr(txt2, '\n');
  if(t != NULL)
    *t = 0;

  syslog(LOG_INFO, "pvr: \"%s\" - Recording started, ends at %s",
	 pvrr->pvrr_printname, txt2);


  LIST_INIT(&pids);

  while(run) {

    switch(pvrr->pvrr_rec_status) {
    case PVR_REC_WAIT_FOR_START:

      time(&now);
      if(now >= pvrr->pvrr_start)
	pvrr->pvrr_rec_status = PVR_REC_WAIT_KEY_FRAME;

      break;

    case PVR_REC_WAIT_KEY_FRAME:
    case PVR_REC_RUNNING:
    case PVR_REC_COMMERCIAL:
      break;
      
    default:
      run = 0;
      continue;
    }
    
    if(pvrr->pvrr_stop < now) {
      syslog(LOG_INFO, "pvr: \"%s\" - Recording completed", 
	     pvrr->pvrr_printname);
      break;
    }

    pthread_mutex_lock(&pvrr->pvrr_dq_mutex);

    while((pd = TAILQ_FIRST(&pvrr->pvrr_dq)) == NULL)
      pthread_cond_wait(&pvrr->pvrr_dq_cond, &pvrr->pvrr_dq_mutex);
    
    TAILQ_REMOVE(&pvrr->pvrr_dq, pd, link);
    pvrr->pvrr_dq_len--;
    pthread_mutex_unlock(&pvrr->pvrr_dq_mutex);

    if(pvrr->pvrr_dq_len * 188 > 20000000) {
      /* Buffer exceeding 20Mbyte, target media is too slow,
	 bail out */
      syslog(LOG_INFO, "pvr: \"%s\" - Disk i/o too slow, aborting",
	     pvrr->pvrr_printname);

      pvrr->pvrr_error = HTSTV_PVR_STATUS_BUFFER_ERROR;
      break;
    }

    if(pd->tsb == NULL) {
      run = 0;
    } else {

      x = pvr_proc_tsb(pvrr, &pids, pd, s);
      free(pd->tsb);

      if(x != 0) {

	switch(errno) {
	case ENOSPC:
	  pvrr->pvrr_error = HTSTV_PVR_STATUS_DISK_FULL;
	  syslog(LOG_INFO, "pvr: \"%s\" - Disk full, aborting", 
		 pvrr->pvrr_printname);
	  break;
	default:
	  pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
	  syslog(LOG_INFO, "pvr: \"%s\" - File error, aborting", 
		 pvrr->pvrr_printname);
	  break;
	}
      }
    }
    free(pd);
  }

  pwo_end(pvrr);

  while((tsp = LIST_FIRST(&pids)) != NULL) {
    LIST_REMOVE(tsp, tsp_link);
    free(tsp->tsp_buf);
    free(tsp);
  }

  return NULL;
}





/**
 * Filename generator
 *
 * - convert from utf8
 * - avoid duplicate filenames
 *
 */

static void
deslashify(char *s)
{
  int i, len = strlen(s);
  for(i = 0; i < len; i++) if(s[i]  == '/')
    s[i] = '-';
}

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
  pvrr->pvrr_format = strdup("asf");

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



/*
 *  Transport stream parser and recording functions
 *
 */



#define getu32(b, l) ({						\
  uint32_t x = (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);	\
  b+=4;								\
  l-=4; 							\
  x;								\
})

#define getu16(b, l) ({						\
  uint16_t x = (b[0] << 8 | b[1]);	                        \
  b+=2;								\
  l-=2; 							\
  x;								\
})

#define getu8(b, l) ({						\
  uint8_t x = b[0];	                                        \
  b+=1;								\
  l-=1; 							\
  x;								\
})

#define getpts(b, l) ({					\
  int64_t _pts;						\
  _pts = (int64_t)((getu8(b, l) >> 1) & 0x07) << 30;	\
  _pts |= (int64_t)(getu16(b, l) >> 1) << 15;		\
  _pts |= (int64_t)(getu16(b, l) >> 1);			\
  _pts;							\
})


static int
pvr_proc_tsb(pvr_rec_t *pvrr, struct ts_pid_head *pidlist, pvr_data_t *pd,
	     th_subscription_t *s)
{
  int pid, adaptation_field_control;
  int len;
  uint8_t *payload, *tsb;
  ts_pid_t *tsp;

  tsb = pd->tsb;

  pid = (tsb[1] & 0x1f) << 8 | tsb[2];

  LIST_FOREACH(tsp, pidlist, tsp_link)
    if(tsp->tsp_pid == pid)
      break;

  if(tsp == NULL) {
    tsp = calloc(1, sizeof(ts_pid_t));
    LIST_INSERT_HEAD(pidlist, tsp, tsp_link);
    tsp->tsp_pid = pid;
  }

  adaptation_field_control = (tsb[3] >> 4) & 0x03;

  if(adaptation_field_control & 0x01) {

    if(adaptation_field_control) {
      uint32_t adaptation_field_length = 0;
      
      if(adaptation_field_control == 3)
        adaptation_field_length = 1 + tsb[4];

      payload = tsb + adaptation_field_length + 4;
      
      len = 188 - 4 - adaptation_field_length;

      if(len < 1)
	return 0;
      
      if(tsb[1] & 0x40) {

	if(tsp->tsp_bufptr > 6) {

	  uint8_t *b = tsp->tsp_buf;
	  size_t l = tsp->tsp_bufptr;
	  uint32_t sc;

	  sc = getu32(b, l);
	  getu16(b, l);  /* Skip len */

	  if(pwo_writepkt(pvrr, s, sc, pd->pi.tp_index, b, l, pd->pi.tp_type))
	    return 1;
	}
	tsp->tsp_bufptr = 0;
	tsp->tsp_pus = 1;
      }

      if(tsp->tsp_pus == 1) {

	if(tsp->tsp_bufptr + len >= tsp->tsp_bufsize) {
	  tsp->tsp_bufsize += len * 3;
	  tsp->tsp_buf = realloc(tsp->tsp_buf, tsp->tsp_bufsize);
	}
      
	memcpy(tsp->tsp_buf + tsp->tsp_bufptr, payload, len);
	tsp->tsp_bufptr += len;

      }
    }
  }
  return 0;
}





/******************************************************************************
 *
 *  ffmpeg based writeout
 *
 */


#define PWO_FFMPEG_MAXPIDS 16


typedef struct pwo_ffmpeg {
  
  AVOutputFormat *fmt;
  AVFormatContext *fctx;

  struct {
    int64_t source_dts;
    int64_t dts;
    int streamid;
    int decoded;
  } pids[PWO_FFMPEG_MAXPIDS];

  int64_t ref_clock;

  int iframe_lock;
  int hdr_written;
  int decode_ctd;

  pvrr_rec_status_t logged_status;

} pwo_ffmpeg_t;



static void *
pwo_init(th_subscription_t *s, pvr_rec_t *pvrr)
{
  char urlname[400];
  int i, err;
  th_transport_t *t = s->ths_transport;
  pwo_ffmpeg_t *pf;
  th_pid_t *p;
  AVStream *st;
  AVCodec *codec;
  const char *cname;

  pf = calloc(1, sizeof(pwo_ffmpeg_t));

  pf->fmt = guess_format(pvrr->pvrr_format, NULL, NULL);
  if(pf->fmt == NULL) {
    syslog(LOG_ERR, 
	   "pvr: \"%s\" - Unable to open file format \".%s\" for output",
	   pvrr->pvrr_printname, pvrr->pvrr_format);
    free(pf);
    return NULL;
  }

  pf->fctx = av_alloc_format_context();

  av_strlcpy(pf->fctx->title,   pvrr->pvrr_title ?: "", 
	     sizeof(pf->fctx->title));

  av_strlcpy(pf->fctx->comment, pvrr->pvrr_desc ?: "", 
	     sizeof(pf->fctx->comment));

  av_strlcpy(pf->fctx->copyright, pvrr->pvrr_channel->ch_name, 
	     sizeof(pf->fctx->copyright));

  pf->fctx->oformat = pf->fmt;

  snprintf(urlname, sizeof(urlname), "file:%s", pvrr->pvrr_filename);

  if((err = url_fopen(&pf->fctx->pb, urlname, URL_WRONLY)) < 0) {
    syslog(LOG_ERR, 
	   "pvr: \"%s\" - Unable to create output file \"%s\" -- %s\n", 
	   pvrr->pvrr_printname, pvrr->pvrr_filename, 
	   strerror(AVUNERROR(err)));
    av_free(pf->fctx);
    free(pf);
    return NULL;
  }

  av_set_parameters(pf->fctx, NULL);  /* Fix NULL -stuff */

  LIST_FOREACH(p, &t->tht_pids, tp_link) {
    i = p->tp_index;

    switch(p->tp_type) {
    case HTSTV_MPEG2VIDEO:
      break;
    case HTSTV_MPEG2AUDIO:
      break;
    case HTSTV_H264:
      break;
    case HTSTV_AC3:
      break;
    default:
      pf->pids[i].streamid = -1;
      continue;
    }

    st = av_mallocz(sizeof(AVStream));
    pf->fctx->streams[pf->fctx->nb_streams] = st;

    st->codec = avcodec_alloc_context();

    switch(p->tp_type) {
    default:
      continue;
    case HTSTV_MPEG2VIDEO:
      st->codec->codec_id = CODEC_ID_MPEG2VIDEO;
      st->codec->codec_type = CODEC_TYPE_VIDEO;
      cname = "mpeg2 video";
      break;

    case HTSTV_MPEG2AUDIO:
      st->codec->codec_id = CODEC_ID_MP2;
      st->codec->codec_type = CODEC_TYPE_AUDIO;
      cname = "mpeg2 audio";
      break;

    case HTSTV_AC3:
      st->codec->codec_id = CODEC_ID_AC3;
      st->codec->codec_type = CODEC_TYPE_AUDIO;
      cname = "ac3 audio";
      break;

    case HTSTV_H264:
      st->codec->codec_id = CODEC_ID_H264;
      st->codec->codec_type = CODEC_TYPE_VIDEO;
      cname = "h.264 video";
      break;
    }

    codec = avcodec_find_decoder(st->codec->codec_id);
    if(codec == NULL) {
      syslog(LOG_ERR, "pvr: \"%s\" - "
	     "Cannot find codec for %s, ignoring stream", 
	     pvrr->pvrr_printname, cname);
      continue;
    }

    if(avcodec_open(st->codec, codec) < 0) {
	syslog(LOG_ERR, "pvr: \"%s\" - "
	     "Cannot open codec for %s, ignoring stream", 
	     pvrr->pvrr_printname, cname);
      continue;
    }

    st->parser = av_parser_init(st->codec->codec_id);

    pf->pids[i].streamid = pf->fctx->nb_streams;
    pf->fctx->nb_streams++;
    pf->decode_ctd++;
  }
  pvrr->pvrr_opaque = pf;
  return pf;
}



static int
pwo_writepkt(pvr_rec_t *pvrr, th_subscription_t *s, uint32_t startcode,
	     int pidindex, uint8_t *buf, size_t len,
	     tv_streamtype_t type)
{
  pwo_ffmpeg_t *pf = pvrr->pvrr_opaque;
  th_transport_t *th = s->ths_transport;
  uint8_t flags, hlen, x;
  int64_t dts = AV_NOPTS_VALUE, pts = AV_NOPTS_VALUE;
  int r, rlen, i, g, fs;
  int lavf_index = pf->pids[pidindex].streamid;
  AVStream *st, *stx;
  AVPacket pkt;
  uint8_t *pbuf;
  int pbuflen, data_size;
  char txt[100];
  const char *tp;
  void *abuf;
  AVFrame pic;

  AVRational mpeg_tc = {1, 90000};

  int64_t pdelta;

  if(lavf_index == -1)
    return 0;

  x = getu8(buf, len);
  flags = getu8(buf, len);
  hlen = getu8(buf, len);
  
  if(len < hlen)
    return 0;
  
  if((x & 0xc0) != 0x80)
    return 0;

  if((flags & 0xc0) == 0xc0) {
    if(hlen < 10) 
      return 0;

    pts = getpts(buf, len);
    dts = getpts(buf, len);

    hlen -= 10;

    pdelta = pts - dts;

  } else if((flags & 0xc0) == 0x80) {
    if(hlen < 5)
      return 0;

    dts = pts = getpts(buf, len);
    hlen -= 5;
    pdelta = 0;
  } else {

    pdelta = 0;
   
  }

  buf += hlen;
  len -= hlen;

  st = pf->fctx->streams[lavf_index];

  if(dts != AV_NOPTS_VALUE)
    pf->pids[pidindex].source_dts = av_rescale_q(dts, mpeg_tc, AV_TIME_BASE_Q);

  pdelta = av_rescale_q(pdelta, mpeg_tc, AV_TIME_BASE_Q);

  while(len > 0) {
    rlen = av_parser_parse(st->parser, st->codec, &pbuf, &pbuflen,
			   buf, len, pts, dts);
    if(pbuflen == 0)
      return 0;

    if(pf->pids[pidindex].decoded == 0) {
      switch(st->codec->codec_type) {
      default:
	break;

      case CODEC_TYPE_VIDEO:
	r = avcodec_decode_video(st->codec, &pic, &data_size, buf, len);
	if(r != 0 && data_size) {
	  syslog(LOG_DEBUG, "pvr: \"%s\" - "
		 "Stream #%d: Decoded a complete video frame: "
		 "%d x %d in %.2fHz\n",
		 pvrr->pvrr_printname, lavf_index,
		 st->codec->width, st->codec->height,
		 (float)st->codec->time_base.den / 
		 (float)st->codec->time_base.num);

	  pf->pids[pidindex].decoded = 1;
	  pf->decode_ctd--;
	}
	break;
	
      case CODEC_TYPE_AUDIO:
	abuf = malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	r = avcodec_decode_audio(st->codec, abuf, &data_size, buf, len);

	free(abuf);

	if(r != 0 && data_size) {
	  syslog(LOG_DEBUG, "pvr: \"%s\" - "
		 "Stream #%d: Decoded a complete audio frame: "
		 "%d channels in %d Hz\n",
		 pvrr->pvrr_printname, lavf_index,
		 st->codec->channels,
		 st->codec->sample_rate);
	  pf->pids[pidindex].decoded = 1;
	  pf->decode_ctd--;
	}
	break;
      }
    }

    if(pf->decode_ctd > 0)
      goto next;

    if(pf->hdr_written == 0) {
      if(av_write_header(pf->fctx))
	return 0;
      pf->hdr_written = 1;
      syslog(LOG_DEBUG, "pvr: \"%s\" - Header written to file, stream dump:", 
	     pvrr->pvrr_printname);

      for(i = 1; i < PWO_FFMPEG_MAXPIDS; i++)
	pf->pids[i].dts = pf->pids[i].source_dts - pf->pids[0].source_dts;

      pf->pids[0].dts = 0;

      for(i = 0; i < pf->fctx->nb_streams; i++) {
	stx = pf->fctx->streams[i];
        g = ff_gcd(stx->time_base.num, stx->time_base.den);

	avcodec_string(txt, sizeof(txt), stx->codec, 1);
  
	syslog(LOG_DEBUG, "pvr: \"%s\" - Stream #%d: %s [%d/%d]",
	       pvrr->pvrr_printname, i, txt, 
	       stx->time_base.num/g, stx->time_base.den/g);
	       
      }
    }
    
    if(pf->logged_status != pvrr->pvrr_rec_status) {
      pf->logged_status = pvrr->pvrr_rec_status;
      
      switch(pf->logged_status) {
      case PVR_REC_WAIT_SUBSCRIPTION:
	tp = "wait for start";
	break;
      case PVR_REC_WAIT_KEY_FRAME:
	tp = "waiting for key frame";
	break;
      case PVR_REC_RUNNING:
	tp = "running";
	break;
      case PVR_REC_COMMERCIAL:
	tp = "commercial break";
	break;
      default:
	tp = NULL;
	break;
      }

      if(tp != NULL) {
	syslog(LOG_INFO, "pvr: \"%s\" - Recorder entering state \"%s\"",
	       pvrr->pvrr_printname, tp);
      }
    }
    
    switch(pvrr->pvrr_status) {
    case PVR_REC_WAIT_FOR_START:
      return 0;

    case PVR_REC_COMMERCIAL:
      if(th == NULL || th->tht_tt_commercial_advice != COMMERCIAL_YES) {
	pvrr->pvrr_status = PVR_REC_WAIT_KEY_FRAME;
      } else {
	return 0;
      }

      /* FALLTHRU */

    case PVR_REC_WAIT_KEY_FRAME:
      /* this check is not enough .. we need to scan for GOP start */
      if(st->codec->codec_type == CODEC_TYPE_VIDEO && 
	 st->parser->pict_type == FF_I_TYPE) {
	pvrr->pvrr_rec_status = PVR_REC_RUNNING;
      } else {
	return 0;
      }
      break;

    case PVR_REC_RUNNING:
      if(th != NULL && th->tht_tt_commercial_advice == COMMERCIAL_YES) {
	pvrr->pvrr_status = PVR_REC_COMMERCIAL;
	return 0;
      }

    default:
      break;
    }

    av_init_packet(&pkt);
    pkt.stream_index = lavf_index;

    if(pf->pids[pidindex].dts >= 0) {

      pkt.dts = av_rescale_q(pf->pids[pidindex].dts, 
			     AV_TIME_BASE_Q, st->time_base);

      pkt.pts = av_rescale_q(pf->pids[pidindex].dts + pdelta,
			   AV_TIME_BASE_Q, st->time_base);
      pkt.data = pbuf;
      pkt.size = pbuflen;

      if(st->codec->codec_type == CODEC_TYPE_VIDEO && 
	 st->parser->pict_type == FF_I_TYPE)
	pkt.flags |= PKT_FLAG_KEY;
      
      r = av_interleaved_write_frame(pf->fctx, &pkt);
    }

    switch(st->codec->codec_type) {
    case CODEC_TYPE_VIDEO:
      pf->pids[pidindex].dts += 1000000 * 
	st->codec->time_base.num / st->codec->time_base.den;
      break;

    case CODEC_TYPE_AUDIO:
      fs = st->codec->frame_size;
      pf->pids[pidindex].dts += 1000000 * fs / st->codec->sample_rate;
      break;

    default:
      break;
    }

  next:
    buf += rlen;
    len -= rlen;
  }
  return 0;
}

static int
pwo_end(pvr_rec_t *pvrr)
{
  pwo_ffmpeg_t *pf = pvrr->pvrr_opaque;
  AVStream *st;
  int i;

  av_write_trailer(pf->fctx);

  for(i = 0; i < pf->fctx->nb_streams; i++) {
    st = pf->fctx->streams[i];
    avcodec_close(st->codec);
    free(st->codec);
    free(st);
  }
  
  url_fclose(&pf->fctx->pb);
  free(pf->fctx);

  free(pf);
  return 0;
}

