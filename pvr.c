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
#include "output_client.h"
#include "pvr.h"
#include "pvr_rec.h"
#include "epg.h"

pthread_mutex_t pvr_mutex = PTHREAD_MUTEX_INITIALIZER;
struct pvr_rec_list pvrr_work_list[PVRR_WORK_MAX];
struct pvr_rec_list pvrr_global_list;

static void pvr_database_load(void);
static void *pvr_main_thread(void *aux);
static void pvr_unrecord(pvr_rec_t *pvrr);

/****************************************************************************
 *
 * Externally visible functions
 *
 */

void
pvr_init(void)
{
  pthread_t ptid;
  pthread_attr_t attr;

  pvr_database_load();

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_create(&ptid, &attr, pvr_main_thread, NULL);
}

char 
pvr_prog_status(programme_t *pr)
{
  pvr_rec_t *pvrr;

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link) {
    if(pvrr->pvrr_start >= pr->pr_start &&
       pvrr->pvrr_stop <= pr->pr_stop &&
       pvrr->pvrr_channel == pr->pr_ch) {
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

static void
pvrr_launch(pvr_rec_t *pvrr)
{
  pthread_t ptid;
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptid, &attr, pvr_recorder_thread, pvrr);
}


static void *
pvr_main_thread(void *aux)
{
  pvr_rec_t *pvrr;
  time_t now;

  pthread_mutex_lock(&pvr_mutex);

  while(1) {

    pthread_mutex_unlock(&pvr_mutex);
    sleep(1);
    pthread_mutex_lock(&pvr_mutex);

    time(&now);
    
    LIST_FOREACH(pvrr, &pvrr_work_list[PVRR_WORK_SCHEDULED], pvrr_work_link) {
      
      /* We start 30 seconds early to allow
       * - Transponder to lock 
       * - MPEG I-frame lock
       * - Acquire additional info from TV network (if program is delayed
       *   and such things)
       */

      if(pvrr->pvrr_start - 30 < now) {
	LIST_REMOVE(pvrr, pvrr_work_link);
	pvrr_launch(pvrr);
      }
    }
  }
}


void
pvr_inform_status_change(pvr_rec_t *pvrr)
{
  programme_t *pr;

  clients_enq_ref(pvrr->pvrr_ref);

  pr = epg_find_programme(pvrr->pvrr_channel, pvrr->pvrr_start,
			  pvrr->pvrr_stop);

  if(pr != NULL)
    clients_enq_ref(pr->pr_ref);
}



static void
pvr_free(pvr_rec_t *pvrr)
{
  LIST_REMOVE(pvrr, pvrr_work_link);
  LIST_REMOVE(pvrr, pvrr_global_link);
  free(pvrr->pvrr_title);
  free(pvrr->pvrr_desc);
  free(pvrr->pvrr_printname);
  free(pvrr->pvrr_filename);
  free(pvrr);
}




static void
pvr_unrecord(pvr_rec_t *pvrr)
{
  pvr_rec_t *x;

  pvr_inform_status_change(pvrr);

  if(pvrr->pvrr_status == HTSTV_PVR_STATUS_SCHEDULED) {
    x = LIST_NEXT(pvrr, pvrr_work_link);
    pvr_free(pvrr);
  } else {
    pvrr->pvrr_status = HTSTV_PVR_STATUS_ABORTED;
  }

  clients_enq_ref(-1);
  pvr_database_save();

}


static int
pvr_rec_cmp(pvr_rec_t *a, pvr_rec_t *b)
{
  return a->pvrr_start - b->pvrr_start;
}

static int
pvr_glob_cmp(pvr_rec_t *a, pvr_rec_t *b)
{
  return b->pvrr_start - a->pvrr_start;
}


static void
pvr_link_pvrr(pvr_rec_t *pvrr)
{
  struct pvr_rec_list *l;

  switch(pvrr->pvrr_status) {
  case HTSTV_PVR_STATUS_SCHEDULED:
    l = &pvrr_work_list[PVRR_WORK_SCHEDULED];
    break;

  case HTSTV_PVR_STATUS_RECORDING:
    l = &pvrr_work_list[PVRR_WORK_RECORDING];
    break;

  default:
    l = &pvrr_work_list[PVRR_WORK_DONE];
    break;
  }

  pvrr->pvrr_ref = ++reftally;

  LIST_INSERT_SORTED(&pvrr_global_list, pvrr, pvrr_global_link, pvr_glob_cmp);
  LIST_INSERT_SORTED(l, pvrr, pvrr_work_link, pvr_rec_cmp);
  clients_enq_ref(-1);
  pvr_inform_status_change(pvrr);
}


void
pvr_add_recording_by_pr(th_channel_t *ch, programme_t *pr)
{
  time_t start = pr->pr_start;
  time_t stop = pr->pr_stop;
  time_t now;
  pvr_rec_t *pvrr;

  time(&now);

  if(stop < now)
    return;

  /* Try to see if we already have a scheduled or active recording */

  LIST_FOREACH(pvrr, &pvrr_global_list, pvrr_global_link) {
    if(pvrr->pvrr_channel == ch && pvrr->pvrr_start == start &&
       pvrr->pvrr_stop == stop) {
      pvr_unrecord(pvrr);
      return;
    }
  }

  pvrr = calloc(1, sizeof(pvr_rec_t));
  pvrr->pvrr_status  = HTSTV_PVR_STATUS_SCHEDULED;
  pvrr->pvrr_channel = ch;
  pvrr->pvrr_start   = start;
  pvrr->pvrr_stop    = stop;
  pvrr->pvrr_title   = pr->pr_title ? strdup(pr->pr_title) : NULL;
  pvrr->pvrr_desc    = pr->pr_desc ?  strdup(pr->pr_desc)  : NULL;

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
  th_channel_t *ch;
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

    if(!strcmp(key, "channel")) {
      TAILQ_FOREACH(ch, &channels, ch_global_link)
	if(!strcmp(ch->ch_name, val))
	  break;
      pvrr->pvrr_channel = ch;
    }
    
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


