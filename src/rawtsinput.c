/*
 *  Raw TS input (for debugging)
 *  Copyright (C) 2009 Andreas Öman
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

#define _XOPEN_SOURCE 600 // for clock_nanosleep()

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "tvheadend.h"
#include "rawtsinput.h"
#include "psi.h"
#include "tsdemux.h"
#include "channels.h"

typedef struct rawts {
  int rt_fd;

  char *rt_identifier;
  psi_section_t rt_pat;

  struct service_list rt_services;

  int rt_pcr_pid;

} rawts_t;


/**
 *
 */
static int
rawts_service_start(service_t *t, unsigned int weight, int force_start)
{
  return 0; // Always ok
}

/**
 *
 */
static void
rawts_service_stop(service_t *t)
{
}

/**
 *
 */
static void
rawts_service_save(service_t *t)
{
  htsmsg_t *m = htsmsg_create_map();
  
  pthread_mutex_lock(&t->s_stream_mutex); 
  psi_save_service_settings(m, t);
  pthread_mutex_unlock(&t->s_stream_mutex); 
  
  //htsmsg_print(m);
  htsmsg_destroy(m);

}


/**
 *
 */
static int
rawts_service_quality(service_t *t)
{
  return 100;
}


/**
 * Generate a descriptive name for the source
 */
static void
rawts_service_setsourceinfo(service_t *t, struct source_info *si)
{
  memset(si, 0, sizeof(struct source_info));
}



/**
 *
 */
static service_t *
rawts_service_add(rawts_t *rt, uint16_t sid, int pmt_pid)
{
  service_t *t;
  struct channel *ch;

  char tmp[200];

  LIST_FOREACH(t, &rt->rt_services, s_group_link) {
    if(t->s_dvb_service_id == sid)
      return t;
  }
  
  snprintf(tmp, sizeof(tmp), "%s_%04x", rt->rt_identifier, sid);

  t = service_create(tmp, SERVICE_TYPE_DVB, S_MPEG_TS);
  t->s_flags |= S_DEBUG;

  t->s_dvb_service_id = sid;
  t->s_pmt_pid        = pmt_pid;

  t->s_start_feed = rawts_service_start;
  t->s_stop_feed  = rawts_service_stop;
  t->s_config_save = rawts_service_save;
  t->s_setsourceinfo = rawts_service_setsourceinfo;
  t->s_quality_index = rawts_service_quality;

  t->s_svcname = strdup(tmp);

  pthread_mutex_lock(&t->s_stream_mutex); 
  service_make_nicename(t);
  pthread_mutex_unlock(&t->s_stream_mutex); 

  tvhlog(LOG_NOTICE, "rawts", "Added service %d (pmt: %d)", sid, pmt_pid);

  LIST_INSERT_HEAD(&rt->rt_services, t, s_group_link);

  ch = channel_find_by_name(tmp, 1, 0);

  service_map_channel(t, ch, 0);
  return t;
}


/*
 *
 */

static void
got_pmt(struct service *t, elementary_stream_t *st,
	const uint8_t *table, int table_len)
{
  if(table[0] != 2)
    return;

  pthread_mutex_lock(&global_lock);
  psi_parse_pmt(t, table + 3, table_len - 3, 1, 1);
  pthread_mutex_unlock(&global_lock);
}



/**
 *
 */
static void
got_pat(const uint8_t *ptr, size_t len, void *opaque)
{
  rawts_t *rt = opaque;
  service_t *t;
  elementary_stream_t *st;
  uint16_t prognum;
  uint16_t pid;

  len -= 8;
  ptr += 8;

  if(len <= 0)
    return;

  pthread_mutex_lock(&global_lock);

  while(len >= 4) {
    
    prognum =  ptr[0]         << 8 | ptr[1];
    pid     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      t = rawts_service_add(rt, prognum, pid);

      if(t != NULL) {
	pthread_mutex_lock(&t->s_stream_mutex);

	if(service_stream_find(t, pid) == NULL) {
	  st = service_stream_create(t, pid, SCT_PMT);
	  st->es_section_docrc = 1;
	  st->es_got_section = got_pmt;
	}
	pthread_mutex_unlock(&t->s_stream_mutex);
      }
    }
    ptr += 4;
    len -= 4;
  }  
  pthread_mutex_unlock(&global_lock);
}

/**
 *
 */
static void
rawts_pat(rawts_t *rt, const uint8_t *tsb)
{
  psi_section_reassemble(&rt->rt_pat, tsb, 1, got_pat, rt);
}


/**
 *
 */
static void
process_ts_packet(rawts_t *rt, uint8_t *tsb)
{
  uint16_t pid;
  service_t *t;
  int64_t pcr, d;
  int didsleep = 0;

  pid = ((tsb[1] & 0x1f) << 8) | tsb[2];
  if(pid == 0) {
    /* PAT */
    rawts_pat(rt, tsb);
    return;
  }
  
  LIST_FOREACH(t, &rt->rt_services, s_group_link) {
    pcr = PTS_UNSET;

    ts_recv_packet1(t, tsb, &pcr);

    if(pcr != PTS_UNSET) {
      
      if(rt->rt_pcr_pid == 0)
	rt->rt_pcr_pid = pid;

      if(rt->rt_pcr_pid == pid) {
	if(t->s_pcr_last != PTS_UNSET && didsleep == 0) {
	  struct timespec slp;
	  int64_t delta = pcr - t->s_pcr_last;

	  

	  if(delta > 90000)
	    delta = 90000;
	  delta *= 11;
	  d = delta + t->s_pcr_last_realtime;
	  slp.tv_sec  =  d / 1000000;
	  slp.tv_nsec = (d % 1000000) * 1000;
	
	  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &slp, NULL);
	  didsleep = 1;
	}
	t->s_pcr_last = pcr;
	t->s_pcr_last_realtime = getmonoclock();
      }
    }
  }
}


/**
 *
 */
static void *
raw_ts_reader(void *aux)
{
  rawts_t *rt = aux;
  uint8_t tsblock[188];
  struct timespec tm = {0, 0};
  int c = 0;
  int i;

  while(1) {

    for(i = 0; i < 2; i++) {
      if(read(rt->rt_fd, tsblock, 188) != 188) {
	lseek(rt->rt_fd, 0, SEEK_SET);
	continue;
      }
      c++;
      process_ts_packet(rt, tsblock);
    }
    nanosleep(&tm, NULL);
  }

  return NULL;
}


/**
 *
 */
void
rawts_init(const char *filename)
{
  pthread_t ptid;
  rawts_t *rt;
  int fd = tvh_open(filename, O_RDONLY, 0);

  if(fd == -1) {
    fprintf(stderr, "Unable to open %s -- %s\n", filename, strerror(errno));
    return;
  }

  rt = calloc(1, sizeof(rawts_t));
  rt->rt_fd = fd;

  rt->rt_identifier = strdup("rawts");

  pthread_create(&ptid, NULL, raw_ts_reader, rt);
}
