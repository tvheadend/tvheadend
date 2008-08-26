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
#include <dirent.h>

#include <libavutil/avstring.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "channels.h"
#include "subscriptions.h"
#include "pvr.h"
#include "epg.h"
#include "dispatch.h"
#include "buffer.h"
#include "ffmuxer.h"
#include "spawn.h"

static int pvr_id_ceil;  /* number generator for database entries */

struct pvr_rec_list pvrr_global_list;

static void pvr_database_save(pvr_rec_t *pvrr);
static void pvr_database_erase(pvr_rec_t *pvrr);
static void pvr_database_load(void);
static void pvr_fsm(pvr_rec_t *pvrr);
static void pvr_subscription_callback(struct th_subscription *s,
				      subscription_event_t event,
				      void *opaque);

static void *pvr_recorder_thread(void *aux);

static void postrec(pvr_rec_t *pvrr);

/**
 * Initialize PVR framework
 */
void
pvr_init(void)
{
  pvr_database_load();
}


/**
 * For the given event, return pvr recording entry (if we have a pvr
 * recording entry that matches the event)
 */
pvr_rec_t *
pvr_get_by_entry(event_t *e)
{
  pvr_rec_t *pvrr;
  channel_t *ch = e->e_channel;

  LIST_FOREACH(pvrr, &ch->ch_pvrrs, pvrr_channel_link) {
    if(pvrr->pvrr_start   >= e->e_start &&
       pvrr->pvrr_stop    <= e->e_start + e->e_duration) {
      return pvrr;
    }
  }
  return NULL;
}


/**
 * Find the pvr record entry based on increasing index
 */
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

/**
 * Find the pvr record entry based on reference tag
 */
pvr_rec_t *
pvr_get_tag_entry(int e)
{
  pvr_rec_t *pvrr;

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link)
    if(pvrr->pvrr_ref == e)
      return pvrr;
  return NULL;
}



/**
 * Inform clients about PVR entry status update
 */
void
pvr_inform_status_change(pvr_rec_t *pvrr)
{
  event_t *e;


  e = epg_event_find_by_time(pvrr->pvrr_channel, pvrr->pvrr_start);
  
}



/**
 * Free a pvr entry
 */
static void
pvr_free(pvr_rec_t *pvrr)
{
  dtimer_disarm(&pvrr->pvrr_timer);
  LIST_REMOVE(pvrr, pvrr_global_link);
  LIST_REMOVE(pvrr, pvrr_channel_link);
  free(pvrr->pvrr_title);
  free(pvrr->pvrr_desc);
  free(pvrr->pvrr_creator);
  free(pvrr->pvrr_printname);
  free(pvrr->pvrr_filename);
  free(pvrr);
}




/**
 * Abort a current recording
 */
int
pvr_abort(pvr_rec_t *pvrr)
{
  if(pvrr->pvrr_status != HTSTV_PVR_STATUS_RECORDING)
    return -1;

  pvrr->pvrr_error = HTSTV_PVR_STATUS_ABORTED;
  pvr_fsm(pvrr);

  pvr_database_save(pvrr);
  return 0;
}

/**
 * Clear current entry (only works if we are not recording)
 */
int
pvr_clear(pvr_rec_t *pvrr)
{
  if(pvrr->pvrr_status == HTSTV_PVR_STATUS_RECORDING)
    return -1;

  pvr_database_erase(pvrr);
  pvr_free(pvrr);
  return 0;
}

/**
 * Destroy all PVRs based on the given channel
 */
void
pvr_destroy_by_channel(channel_t *ch)
{
  pvr_rec_t *pvrr;

  while((pvrr = LIST_FIRST(&ch->ch_pvrrs)) != NULL) {
    if(pvrr->pvrr_status == HTSTV_PVR_STATUS_RECORDING)
      pvr_abort(pvrr);

    pvr_clear(pvrr);
    pvr_free(pvrr);
  }
}


/**
 * Insert a pvr entry skeleton into the list and start FSM
 */
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
    pvr_fsm(pvrr);
    break;
  }

  pvr_inform_status_change(pvrr);
}


/**
 * Remove log info about all completed recordings
 */
void
pvr_clear_all_completed(void)
{
  pvr_rec_t *pvrr, *next;
  for(pvrr = LIST_FIRST(&pvrr_global_list); pvrr != NULL; pvrr = next) {
    next = LIST_NEXT(pvrr, pvrr_global_link);

    switch(pvrr->pvrr_status) {
    case HTSTV_PVR_STATUS_SCHEDULED:
    case HTSTV_PVR_STATUS_RECORDING:
      break;
    default:
      pvr_database_erase(pvrr);
      pvr_free(pvrr);
      break;
    }
  }
}



/**
 * Create a PVR entry based on a given event
 */
pvr_rec_t *
pvr_schedule_by_event(event_t *e, const char *creator)
{
  channel_t *ch = e->e_channel;
  time_t start = e->e_start;
  time_t stop  = e->e_start + e->e_duration;
  time_t now;
  pvr_rec_t *pvrr;

  time(&now);

  if(stop < now)
    return NULL;

  /* Try to see if we already have a scheduled or active recording */

  LIST_FOREACH(pvrr, &ch->ch_pvrrs, pvrr_channel_link)
    if(pvrr->pvrr_start == start && pvrr->pvrr_stop == stop)
      break;

  if(pvrr != NULL)
    return NULL; /* Already exists */

  pvrr = calloc(1, sizeof(pvr_rec_t));
  pvrr->pvrr_status  = HTSTV_PVR_STATUS_SCHEDULED;
  pvrr->pvrr_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_pvrrs, pvrr, pvrr_channel_link);

  pvrr->pvrr_start   = start;
  pvrr->pvrr_stop    = stop;
  pvrr->pvrr_title   = e->e_title ? strdup(e->e_title) : NULL;
  pvrr->pvrr_desc    = e->e_desc ?  strdup(e->e_desc)  : NULL;
  pvrr->pvrr_creator = strdup(creator);
  pvr_link_pvrr(pvrr);  
  pvr_database_save(pvrr);
  return pvrr;
}




/**
 * Record based on a channel
 */
pvr_rec_t *
pvr_schedule_by_channel_and_time(channel_t *ch, int duration,
				 const char *creator)
{
  time_t now   = dispatch_clock;
  time_t start = now;
  time_t stop  = now + duration;
  pvr_rec_t *pvrr;

  pvrr = calloc(1, sizeof(pvr_rec_t));
  pvrr->pvrr_status  = HTSTV_PVR_STATUS_SCHEDULED;
  pvrr->pvrr_channel = ch;
  LIST_INSERT_HEAD(&ch->ch_pvrrs, pvrr, pvrr_channel_link);

  pvrr->pvrr_start   = start;
  pvrr->pvrr_stop    = stop;
  pvrr->pvrr_title   = strdup("Manual recording");
  pvrr->pvrr_desc    = NULL;
  pvrr->pvrr_creator = strdup(creator);

  pvr_link_pvrr(pvrr);  
  pvr_database_save(pvrr);
  return pvrr;
}




/*****************************************************************************
 *
 * The recording database is real simple.
 *
 * We just store meta information about each recording in a separate
 * textfile stored in a directory
 *
 */

static void
pvr_database_save(pvr_rec_t *pvrr)
{
  char buf[400];
  FILE *fp;

  if(pvrr->pvrr_id == 0)
    pvrr->pvrr_id = ++pvr_id_ceil;

  snprintf(buf, sizeof(buf), "%s/recordings/%d", settings_dir, pvrr->pvrr_id);
  
  if((fp = settings_open_for_write(buf)) == NULL)
    return;
  
  fprintf(fp, "channel = %s\n", pvrr->pvrr_channel->ch_name);
  fprintf(fp, "start = %ld\n", pvrr->pvrr_start);
  fprintf(fp, "stop = %ld\n", pvrr->pvrr_stop);

  if(pvrr->pvrr_title != NULL)
    fprintf(fp, "title = %s\n", pvrr->pvrr_title);

  if(pvrr->pvrr_desc != NULL)
    fprintf(fp, "description = %s\n", pvrr->pvrr_desc);

  if(pvrr->pvrr_creator != NULL)
    fprintf(fp, "creator = %s\n", pvrr->pvrr_creator);

  if(pvrr->pvrr_filename != NULL)
    fprintf(fp, "filename = %s\n", pvrr->pvrr_filename);
  
  fprintf(fp, "status = %c\n", pvrr->pvrr_status);
  fclose(fp);
}

/**
 * Erase status from a recording
 */
static void
pvr_database_erase(pvr_rec_t *pvrr)
{
  char buf[400];

  if(pvrr->pvrr_id == 0)
    return;

  snprintf(buf, sizeof(buf), "%s/recordings/%d", settings_dir, pvrr->pvrr_id);
  unlink(buf);
}

/**
 * Load database
 */
static void
pvr_database_load(void)
{
  struct config_head cl;
  char buf[400];
  struct dirent *d;
  const char *channel, *title, *desc, *fname, *status, *creator;
  DIR *dir;
  time_t start, stop;
  pvr_rec_t *pvrr;

  snprintf(buf, sizeof(buf), "%s/recordings", settings_dir);

  if((dir = opendir(buf)) == NULL)
    return;

  while((d = readdir(dir)) != NULL) {

    if(d->d_name[0] == '.')
      continue;

    snprintf(buf, sizeof(buf), "%s/recordings/%s", settings_dir, d->d_name);

    TAILQ_INIT(&cl);
    config_read_file0(buf, &cl);

    channel = config_get_str_sub(&cl, "channel", NULL);
    start = atoi(config_get_str_sub(&cl, "start", "0"));
    stop  = atoi(config_get_str_sub(&cl, "stop",  "0"));
    
    title = config_get_str_sub(&cl, "title", NULL);
    desc  = config_get_str_sub(&cl, "description", NULL);
    fname = config_get_str_sub(&cl, "filename", NULL);
    status = config_get_str_sub(&cl, "status", NULL);
    creator = config_get_str_sub(&cl, "creator", NULL);

    if(channel != NULL && start && stop && title && status) {
      pvrr = calloc(1, sizeof(pvr_rec_t));

      pvrr->pvrr_channel  = channel_find_by_name(channel, 1);
      pvrr->pvrr_start    = start;
      pvrr->pvrr_stop     = stop;
      pvrr->pvrr_status   = *status;
      pvrr->pvrr_filename = fname ? strdup(fname) : NULL;
      pvrr->pvrr_title    = title ? strdup(title) : NULL;
      pvrr->pvrr_desc     = desc ?  strdup(desc)  : NULL;
      pvrr->pvrr_creator  = creator ? strdup(creator) : NULL;
      
      pvrr->pvrr_id       = atoi(d->d_name);

      if(pvrr->pvrr_id > pvr_id_ceil)
	pvr_id_ceil = pvrr->pvrr_id;

      pvr_link_pvrr(pvrr);
    }
    config_free0(&cl);
  }
  closedir(dir);
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

  pvrr->pvrr_fmt_lavfname = "matroska";
  pvrr->pvrr_fmt_postfix  = "mkv";

  filename = utf8tofilename(name && name[0] ? name : "untitled");
  deslashify(filename);

  chname = utf8tofilename(pvrr->pvrr_channel->ch_name);
  deslashify(chname);

  snprintf(fullname, sizeof(fullname), "%s/%s-%s.%s",
	   config_get_str("pvrdir", "."), chname, filename,
	   pvrr->pvrr_fmt_postfix);

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
	     config_get_str("pvrdir", "."), chname, filename, tally,
	     pvrr->pvrr_fmt_postfix);
  }

  pvrr->pvrr_filename = strdup(fullname);

  if(pvrr->pvrr_printname != NULL)
    free(pvrr->pvrr_printname);

  x = strrchr(pvrr->pvrr_filename, '/');
  pvrr->pvrr_printname = strdup(x ? x + 1 :  pvrr->pvrr_filename);

  free(filename);
  free(chname);
}


/**
 * Timeout fired, call FSM
 */
static void
pvr_fsm_timeout(void *aux, int64_t now)
{
  pvr_rec_t *pvrr = aux;
  pvr_fsm(pvrr);
}



/**
 * Main PVR state machine
 */
static void
pvr_fsm(pvr_rec_t *pvrr)
{
  time_t delta;
  time_t now;
  th_ffmuxer_t *tffm = &pvrr->pvrr_tffm;

  dtimer_disarm(&pvrr->pvrr_timer);

  time(&now);

  switch(pvrr->pvrr_status) {
  case HTSTV_PVR_STATUS_NONE:
    break;

  case HTSTV_PVR_STATUS_SCHEDULED:
    delta = pvrr->pvrr_start - 30 - now;
    
    if(delta > 0) {
      dtimer_arm(&pvrr->pvrr_timer, pvr_fsm_timeout, pvrr, delta);
      break;
    }

    delta = pvrr->pvrr_stop - now;

    if(delta <= 0) {
      tvhlog(LOG_NOTICE, "pvr", "\"%s\" - Recording skipped, "
	     "program has already come to pass", pvrr->pvrr_printname);
      pvrr->pvrr_status = HTSTV_PVR_STATUS_DONE;
      pvr_inform_status_change(pvrr);
      pvr_database_save(pvrr);
      break;
    }

    /* Add a timer that fires when recording ends */

    dtimer_arm(&pvrr->pvrr_timer, pvr_fsm_timeout, pvrr, delta);

    TAILQ_INIT(&pvrr->pvrr_pktq);
    pthread_cond_init(&pvrr->pvrr_pktq_cond, NULL);
    pthread_mutex_init(&pvrr->pvrr_pktq_mutex, NULL);

    pvrr->pvrr_status = HTSTV_PVR_STATUS_RECORDING;
    pvr_inform_status_change(pvrr);
    tffm->tffm_state = TFFM_WAIT_SUBSCRIPTION; /* cant use set_state() since
						  tffm_printname is not
						  initialized */

    pvrr->pvrr_s = subscription_create(pvrr->pvrr_channel, 1000, "pvr",
				       pvr_subscription_callback,
				       pvrr, 0);
    pvrr->pvrr_error = HTSTV_PVR_STATUS_DONE; /* assume everything will
						 work out ok */
    break;

  case HTSTV_PVR_STATUS_RECORDING:
    /* recording completed (or aborted, or failed or somthing) */
    pvrr->pvrr_status = pvrr->pvrr_error;
    pvr_inform_status_change(pvrr);
    pvr_database_save(pvrr);

    subscription_unsubscribe(pvrr->pvrr_s);
    dtimer_disarm(&pvrr->pvrr_timer);
    postrec(pvrr);
    break;
  }
}



/**
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
    return;
  }

  pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);
  TAILQ_INSERT_TAIL(&pvrr->pvrr_pktq, pkt, pkt_queue_link);
  pvrr->pvrr_pktq_len++;
  pthread_cond_signal(&pvrr->pvrr_pktq_cond);
  pthread_mutex_unlock(&pvrr->pvrr_pktq_mutex);
}


/**
 * We've got a transport now, start recording
 */
static void
pvrr_transport_available(pvr_rec_t *pvrr, th_transport_t *t)
{
  th_ffmuxer_t *tffm = &pvrr->pvrr_tffm;
  th_muxer_t *tm = &tffm->tffm_muxer;
  AVFormatContext *fctx;
  AVOutputFormat *fmt;
  char urlname[500];
  char printname[500];
  int err;

  tm->tm_opaque = pvrr;
  tm->tm_new_pkt = pvrr_packet_input;

  pvr_generate_filename(pvrr);

  /* Find lavf format */

  fmt = guess_format(pvrr->pvrr_fmt_lavfname, NULL, NULL);
  if(fmt == NULL) {
    tvhlog(LOG_ERR, "pvr",
	   "\"%s\" - Unable to open file format \"%s\" for output",
	   pvrr->pvrr_printname, pvrr->pvrr_fmt_lavfname);
    pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
    pvr_fsm(pvrr);
    return;
  }

  /* Init format context */

  fctx = av_alloc_format_context();

  av_strlcpy(fctx->title, pvrr->pvrr_title ?: "", 
	     sizeof(fctx->title));

  av_strlcpy(fctx->comment, pvrr->pvrr_desc ?: "", 
	     sizeof(fctx->comment));

  av_strlcpy(fctx->copyright, pvrr->pvrr_channel->ch_name, 
	     sizeof(fctx->copyright));

  
  fctx->oformat = fmt;

  /* Open output file */

  snprintf(urlname, sizeof(urlname), "file:%s", pvrr->pvrr_filename);

  if((err = url_fopen(&fctx->pb, urlname, URL_WRONLY)) < 0) {
    tvhlog(LOG_ERR, "pvr",
	   "\"%s\" - Unable to create output file \"%s\" -- %s\n", 
	   pvrr->pvrr_printname, pvrr->pvrr_filename, 
	   strerror(AVUNERROR(err)));
    av_free(fctx);
    pvrr->pvrr_error = HTSTV_PVR_STATUS_FILE_ERROR;
    pvr_fsm(pvrr);
    return;
  }

  snprintf(printname, sizeof(printname), "pvr: \"%s\"", pvrr->pvrr_printname);

  tffm_open(tffm, t, fctx, printname);

  pthread_create(&pvrr->pvrr_ptid, NULL, pvr_recorder_thread, pvrr);
  LIST_INSERT_HEAD(&t->tht_muxers, tm, tm_transport_link);
}

/**
 * We've lost our transport, stop recording
 */
static void
pvrr_transport_unavailable(pvr_rec_t *pvrr, th_transport_t *t)
{
  th_ffmuxer_t *tffm = &pvrr->pvrr_tffm;
  th_muxer_t *tm = &tffm->tffm_muxer;
  th_muxstream_t *tms;
  th_pkt_t *pkt;

  LIST_REMOVE(tm, tm_transport_link);

  pvrr->pvrr_dts_offset = AV_NOPTS_VALUE;

  tffm_set_state(tffm, TFFM_STOP);
  pthread_cond_signal(&pvrr->pvrr_pktq_cond);
  pthread_join(pvrr->pvrr_ptid, NULL);

  tffm_close(tffm);

  /* Remove any pending packet for queue */

  while((pkt = TAILQ_FIRST(&pvrr->pvrr_pktq)) != NULL)
    pkt_deref(pkt);

  /* Destroy muxstreams */

  while((tms = LIST_FIRST(&tm->tm_streams)) != NULL) {
    LIST_REMOVE(tms, tms_muxer_link0);
    free(tms);
  }

}


/**
 * We get a callback here when the subscription status is updated, 
 * ie, when we are attached to a transport and when we are detached
 */
static void
pvr_subscription_callback(struct th_subscription *s,
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

/**
 * Recorder thread
 */
static void *
pvr_recorder_thread(void *aux)
{
  th_pkt_t *pkt;
  pvr_rec_t *pvrr = aux;
  th_ffmuxer_t *tffm = &pvrr->pvrr_tffm;
  char *t, txt2[50];
  int run = 1;
  time_t now;

  ctime_r(&pvrr->pvrr_stop, txt2);
  t = strchr(txt2, '\n');
  if(t != NULL)
    *t = 0;

  tvhlog(LOG_INFO, "pvr", "\"%s\" - Recording started, ends at %s",
	 pvrr->pvrr_printname, txt2);
  

  pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);

  while(run) {
    switch(tffm->tffm_state) {
    case TFFM_WAIT_FOR_START:

      time(&now);
      if(now >= pvrr->pvrr_start)
	tffm_set_state(tffm, TFFM_WAIT_AUDIO_LOCK);
      break;

    case TFFM_WAIT_AUDIO_LOCK:
    case TFFM_WAIT_VIDEO_LOCK:
    case TFFM_RUNNING:
    case TFFM_COMMERCIAL:
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

    tffm_record_packet(tffm, pkt);
    pkt_deref(pkt);

    pthread_mutex_lock(&pvrr->pvrr_pktq_mutex);
  }
  pthread_mutex_unlock(&pvrr->pvrr_pktq_mutex);
  
  tvhlog(LOG_INFO, "pvr", "\"%s\" - Recording completed", 
	 pvrr->pvrr_printname);

  return NULL;
}

/**
 * After recording is completed, execute a program of users choice
 */

static struct strtab pvrrstatustab[] = {
  { "ok",            HTSTV_PVR_STATUS_DONE           },
  { "aborted",       HTSTV_PVR_STATUS_ABORTED        },
  { "transponder",   HTSTV_PVR_STATUS_NO_TRANSPONDER },
  { "file-error",    HTSTV_PVR_STATUS_FILE_ERROR     },
  { "disk-full",     HTSTV_PVR_STATUS_DISK_FULL      },
  { "buffer-error",  HTSTV_PVR_STATUS_BUFFER_ERROR   },
};

static void
postrec(pvr_rec_t *pvrr)
{
  const char *prog, *status;
  const char *argv[16];

  if((prog = config_get_str("pvrpostproc", NULL)) == NULL)
    return;
  
  if((status = val2str(pvrr->pvrr_status, pvrrstatustab)) == NULL)
    return;

  argv[0] = prog;
  argv[1] = pvrr->pvrr_filename;
  argv[2] = status;
  argv[3] = "default";            /* recording class, currently unused */
  argv[4] = pvrr->pvrr_channel->ch_name;
  argv[5] = pvrr->pvrr_creator;
  argv[6] = pvrr->pvrr_title ?: "";
  argv[7] = pvrr->pvrr_desc  ?: "";
  argv[8] = NULL;

  spawnv(prog, (void *)argv);
}
