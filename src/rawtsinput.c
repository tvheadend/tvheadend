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

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "tvhead.h"
#include "transports.h"
#include "rawtsinput.h"
#include "psi.h"
#include "tsdemux.h"

typedef struct rawts {
  int rt_fd;

  char *rt_identifier;
  psi_section_t rt_pat;

  struct th_transport_list rt_transports;

} rawts_t;


/**
 *
 */
static int
rawts_transport_start(th_transport_t *t, unsigned int weight, int status, 
		      int force_start)
{
  return 0; // Always ok
}

/**
 *
 */
static void
rawts_transport_stop(th_transport_t *t)
{
  
}

/**
 *
 */
static void
rawts_transport_save(th_transport_t *t)
{
}


/**
 *
 */
static int
rawts_transport_quality(th_transport_t *t)
{
  return 100;
}


/**
 * Generate a descriptive name for the source
 */
static void
rawts_transport_setsourceinfo(th_transport_t *t, struct source_info *si)
{
  memset(si, 0, sizeof(struct source_info));
}



/**
 *
 */
static th_transport_t *
rawts_transport_add(rawts_t *rt, uint16_t sid, int pmt_pid)
{
  th_transport_t *t;
  channel_t *ch;

  char tmp[200];

  LIST_FOREACH(t, &rt->rt_transports, tht_group_link) {
    if(t->tht_dvb_service_id == sid)
      return t;
  }
  
  snprintf(tmp, sizeof(tmp), "%s_%04x", rt->rt_identifier, sid);

  t = transport_create(tmp, TRANSPORT_DVB, THT_MPEG_TS);
  t->tht_flags |= THT_DEBUG;

  t->tht_dvb_service_id = sid;
  t->tht_pmt_pid        = pmt_pid;

  t->tht_start_feed = rawts_transport_start;
  t->tht_stop_feed  = rawts_transport_stop;
  t->tht_config_save = rawts_transport_save;
  t->tht_setsourceinfo = rawts_transport_setsourceinfo;
  t->tht_quality_index = rawts_transport_quality;

  t->tht_svcname = strdup(tmp);

  tvhlog(LOG_NOTICE, "rawts", "Added service %d (pmt: %d)", sid, pmt_pid);

  LIST_INSERT_HEAD(&rt->rt_transports, t, tht_group_link);

  ch = channel_find_by_name(tmp, 1);

  transport_map_channel(t, ch, 0);
  return t;
}


/*
 *
 */

static void
got_pmt(struct th_transport *t, th_stream_t *st,
	uint8_t *table, int table_len)
{
  if(table[0] != 2)
    return;

  pthread_mutex_lock(&global_lock);
  psi_parse_pmt(t, table + 3, table_len - 3, 1, 0);
  pthread_mutex_unlock(&global_lock);
}



/**
 *
 */
static void
got_pat(rawts_t *rt)
{
  th_transport_t *t;
  th_stream_t *st;
  int len = rt->rt_pat.ps_offset;
  uint8_t *ptr = rt->rt_pat.ps_data;
  uint16_t prognum;
  uint16_t pid;

  len -= 8;
  ptr += 8;

  if(len < 0)
    return;

  pthread_mutex_lock(&global_lock);

  while(len >= 4) {
    
    prognum =  ptr[0]         << 8 | ptr[1];
    pid     = (ptr[2] & 0x1f) << 8 | ptr[3];

    if(prognum != 0) {
      t = rawts_transport_add(rt, prognum, pid);

      if(t != NULL) {
	pthread_mutex_lock(&t->tht_stream_mutex);

	if(transport_stream_find(t, pid) == NULL) {
	  st = transport_stream_create(t, pid, SCT_PMT);
	  st->st_section_docrc = 1;
	  st->st_got_section = got_pmt;
	}
	pthread_mutex_unlock(&t->tht_stream_mutex);
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
rawts_pat(rawts_t *rt, uint8_t *tsb)
{
  int off  = tsb[3] & 0x20 ? tsb[4] + 5 : 4;
  int pusi = tsb[1] & 0x40;
  int len;

  if(off >= 188)
    return;

  if(pusi) {
    len = tsb[off++];
    if(len > 0) {
      if(len > 188 - off)
	return;
      if(!psi_section_reassemble(&rt->rt_pat, tsb + off, len, 0, 1))
	got_pat(rt);
      off += len;
    }
  }
    
  if(!psi_section_reassemble(&rt->rt_pat, tsb + off, 188 - off, pusi, 1))
    got_pat(rt);
}


/**
 *
 */
static void
process_ts_packet(rawts_t *rt, uint8_t *tsb)
{
  uint16_t pid;
  th_transport_t *t;

  pid = ((tsb[1] & 0x1f) << 8) | tsb[2];

  if(pid == 0) {
    /* PAT */
    rawts_pat(rt, tsb);
    return;
  }
  
  LIST_FOREACH(t, &rt->rt_transports, tht_group_link)
    ts_recv_packet1(t, tsb);
}


/**
 *
 */
static void *
raw_ts_reader(void *aux)
{
  rawts_t *rt = aux;
  uint8_t tsblock[188];
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
    usleep(1000);
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
  int fd = open(filename, O_RDONLY);

  if(fd == -1) {
    fprintf(stderr, "Unable to open %s -- %s\n", filename, strerror(errno));
    return;
  }

  rt = calloc(1, sizeof(rawts_t));
  rt->rt_fd = fd;

  rt->rt_identifier = strdup("rawts");

  pthread_create(&ptid, NULL, raw_ts_reader, rt);
}
