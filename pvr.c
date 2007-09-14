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
#include "transports.h"
#include "htsclient.h"
#include "pvr.h"
#include "pvr_rec.h"
#include "epg.h"
#include "dispatch.h"

struct pvr_rec_list pvrr_global_list;

static void pvr_database_load(void);
static void pvr_unrecord(pvr_rec_t *pvrr);
static void pvrr_fsm(pvr_rec_t *pvrr);

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

  clients_enq_ref(pvrr->pvrr_ref);

  e = epg_event_find_by_time(pvrr->pvrr_channel, pvrr->pvrr_start);
  
  if(e != NULL)
    clients_enq_ref(e->e_tag);
}



static void
pvr_free(pvr_rec_t *pvrr)
{
  if(pvrr->pvrr_timer != NULL)
    stimer_del(pvrr->pvrr_timer);

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
  clients_enq_ref(-1);
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
  clients_enq_ref(-1);
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


/**
 * wait for thread to exit
 */

static void 
pvr_wait_thread(pvr_rec_t *pvrr)
{
  pvr_data_t *pd;
  
  if(pvrr->pvrr_rec_status == PVR_REC_STOP)
    return;

  pvrr_set_rec_state(pvrr, PVR_REC_STOP);
    
  pd = malloc(sizeof(pvr_data_t));
  pd->tsb = NULL;
  pthread_mutex_lock(&pvrr->pvrr_dq_mutex);
  TAILQ_INSERT_TAIL(&pvrr->pvrr_dq, pd, link);
  pthread_cond_signal(&pvrr->pvrr_dq_cond);
  pthread_mutex_unlock(&pvrr->pvrr_dq_mutex);
  pthread_join(pvrr->pvrr_ptid, NULL);
}




/**
 * pvrr finite state machine
 */


static void pvr_record_callback(struct th_subscription *s, uint8_t *pkt,
				th_pid_t *pi, int64_t pcr);


static void
pvrr_fsm_timeout(void *aux)
{
  pvr_rec_t *pvrr = aux;

  pvrr->pvrr_timer = NULL;
  pvrr_fsm(pvrr);
}



static void
pvrr_fsm(pvr_rec_t *pvrr)
{
  time_t delta;
  time_t now;

  time(&now);

  switch(pvrr->pvrr_status) {
  case HTSTV_PVR_STATUS_NONE:
    break;

  case HTSTV_PVR_STATUS_SCHEDULED:
    delta = pvrr->pvrr_start - 30 - now;
    assert(pvrr->pvrr_timer == NULL);
    
    if(delta > 0) {
      pvrr->pvrr_timer = stimer_add(pvrr_fsm_timeout, pvrr, delta);
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

    pvrr->pvrr_timer = stimer_add(pvrr_fsm_timeout, pvrr, delta);

    TAILQ_INIT(&pvrr->pvrr_dq);
    pthread_cond_init(&pvrr->pvrr_dq_cond, NULL);
    pthread_mutex_init(&pvrr->pvrr_dq_mutex, NULL);

    pvrr->pvrr_s = channel_subscribe(pvrr->pvrr_channel, pvrr,
				     pvr_record_callback, 1000, "pvr");

    pvrr->pvrr_status = HTSTV_PVR_STATUS_RECORDING;
    pvr_inform_status_change(pvrr);
    pvrr_set_rec_state(pvrr,PVR_REC_WAIT_SUBSCRIPTION); 
    break;

  case HTSTV_PVR_STATUS_RECORDING:    
    /* recording completed */
    pvrr->pvrr_status = pvrr->pvrr_error;
    pvr_inform_status_change(pvrr);
    pvr_database_save();

    subscription_unsubscribe(pvrr->pvrr_s);

    pvr_wait_thread(pvrr);

    if(pvrr->pvrr_timer != NULL) {
      stimer_del(pvrr->pvrr_timer);
      pvrr->pvrr_timer = NULL;
    }
    break;
  }
}



/*
 * PVR data input callback
 */

static void 
pvr_record_callback(struct th_subscription *s, uint8_t *pkt, th_pid_t *pi,
		    int64_t pcr)
{
  pvr_data_t *pd;
  pvr_rec_t *pvrr = s->ths_opaque;
  
  if(pkt == NULL)
    return;

  pd = malloc(sizeof(pvr_data_t));
  pd->tsb = malloc(188);
  memcpy(pd->tsb, pkt, 188);
  pd->pi = *pi;
  pthread_mutex_lock(&pvrr->pvrr_dq_mutex);
  TAILQ_INSERT_TAIL(&pvrr->pvrr_dq, pd, link);
  pvrr->pvrr_dq_len++;
  pthread_cond_signal(&pvrr->pvrr_dq_cond);
  pthread_mutex_unlock(&pvrr->pvrr_dq_mutex);

  if(pvrr->pvrr_rec_status == PVR_REC_WAIT_SUBSCRIPTION) {
    /* ok, first packet, start recording thread */
    pvrr_set_rec_state(pvrr, PVR_REC_WAIT_FOR_START);
    pthread_create(&pvrr->pvrr_ptid, NULL, pvr_recorder_thread, pvrr);
  }
}


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

