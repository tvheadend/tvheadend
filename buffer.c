/*
 *  Packet / Buffer management
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

#define _XOPEN_SOURCE 500
#include <unistd.h>

#include <pthread.h>
#include <syslog.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tvhead.h"
#include "buffer.h"


int64_t store_mem_size;
int64_t store_mem_size_max;
int64_t store_disk_size;
int64_t store_disk_size_max;
int store_packets;




static struct th_pkt_queue store_mem_queue;
static struct th_pkt_queue store_disk_queue;

static int store_chunk_size;
static const char *store_path;
static th_storage_t *curstore;
static int store_tally;


static void storage_wipe(void);
static void storage_mem_enq(th_pkt_t *pkt);
static void storage_disk_enq(th_pkt_t *pkt);

static void storage_deref(th_storage_t *s);

/*
 *
 */
void
pkt_init(void)
{
  store_path = config_get_str("trickplay-path", "/storage/streambuffer");
  
  if(store_path != NULL)
    storage_wipe();

  TAILQ_INIT(&store_mem_queue);
  TAILQ_INIT(&store_disk_queue);

  store_mem_size_max  = 1024 * 1024 * 10ULL;
  store_disk_size_max = 1024 * 1024 * 4000ULL;

  store_chunk_size = store_disk_size_max / 32;
}

/*
 *
 */
static void
pkt_free(th_pkt_t *pkt)
{
  assert(pkt->pkt_storage == NULL);
  free(pkt->pkt_payload);
  memset(pkt, 0xff, sizeof(th_pkt_t));
  store_packets--;
  free(pkt);
}


/*
 *
 */
void
pkt_deref(th_pkt_t *pkt)
{
  assert(pkt->pkt_refcount > 0);
  if(pkt->pkt_refcount > 1) {
    pkt->pkt_refcount--;
    return;
  }
  pkt_free(pkt);
}

/*
 *
 */
th_pkt_t *
pkt_ref(th_pkt_t *pkt)
{
  pkt->pkt_refcount++;
  return pkt;
}

/*
 *
 */
th_pkt_t *
pkt_alloc(void *data, size_t datalen, int64_t pts, int64_t dts)
{
  th_pkt_t *pkt;

  pkt = calloc(1, sizeof(th_pkt_t));
  pkt->pkt_payloadlen = datalen;
  pkt->pkt_payload = malloc(datalen);
  if(data != NULL)
    memcpy(pkt->pkt_payload, data, datalen);

  pkt->pkt_dts = dts;
  pkt->pkt_pts = pts;
  pkt->pkt_refcount = 1;

  store_packets++;
  return pkt;
}

/*
 *
 */
th_pkt_t *
pkt_copy(th_pkt_t *orig)
{
  th_pkt_t *pkt;

  pkt_load(orig);
  if(orig->pkt_payload == NULL)
    return NULL;

  pkt = malloc(sizeof(th_pkt_t));
  memcpy(pkt, orig, sizeof(th_pkt_t));

  pkt->pkt_payload = malloc(pkt->pkt_payloadlen);
  memcpy(pkt->pkt_payload, orig->pkt_payload, pkt->pkt_payloadlen);

  pkt->pkt_on_stream_queue = 0;
  pkt->pkt_storage = NULL;
  pkt->pkt_refcount = 1;
  return pkt;
}


/*
 *
 */
void
pkt_store(th_pkt_t *pkt)
{
  th_stream_t *st = pkt->pkt_stream;

  if(pkt->pkt_on_stream_queue)
    return;

  pkt->pkt_on_stream_queue = 1;
  pkt->pkt_refcount++;
  TAILQ_INSERT_TAIL(&st->st_pktq, pkt, pkt_queue_link);

  /* Persistent buffer management */
  
  storage_mem_enq(pkt);
  storage_disk_enq(pkt);

  if(pkt->pkt_storage)
    pwrite(pkt->pkt_storage->ts_fd, pkt->pkt_payload, pkt->pkt_payloadlen,
	   pkt->pkt_storage_offset);
}


/*
 * Force flush of a packet (if a transport is stopped)
 */
void
pkt_unstore(th_pkt_t *pkt)
{
  th_stream_t *st = pkt->pkt_stream;

  assert(pkt->pkt_on_stream_queue == 1);
  TAILQ_REMOVE(&st->st_pktq, pkt, pkt_queue_link);
  pkt->pkt_on_stream_queue = 0;

  if(pkt->pkt_storage != NULL) {
    storage_deref(pkt->pkt_storage);
    TAILQ_REMOVE(&store_disk_queue, pkt, pkt_disk_link);
    store_disk_size -= pkt->pkt_payloadlen;

    if(pkt->pkt_payload != NULL) {
      TAILQ_REMOVE(&store_mem_queue, pkt, pkt_mem_link);
      store_mem_size -= pkt->pkt_payloadlen;
    }

    pkt->pkt_storage = NULL;
  }
  pkt_deref(pkt);
}


/*
 *
 */
void
pkt_load(th_pkt_t *pkt)
{
  if(pkt->pkt_payload == NULL && pkt->pkt_storage != NULL) {
    pkt->pkt_payload = malloc(pkt->pkt_payloadlen);
    pread(pkt->pkt_storage->ts_fd, pkt->pkt_payload, pkt->pkt_payloadlen,
	  pkt->pkt_storage_offset);
    storage_mem_enq(pkt);
  }
  if(pkt->pkt_payload == NULL)
    printf("Packet %p load failed\n", pkt);
}



/*
 *
 */
static void
storage_deref(th_storage_t *s)
{
  if(s->ts_refcount > 1) {
    s->ts_refcount--;
    return;
  }
  if(curstore == s)
    curstore = NULL;

  close(s->ts_fd);
  unlink(s->ts_filename);
  free(s->ts_filename);
  free(s);
}




/*
 *
 */
static void
storage_mem_enq(th_pkt_t *pkt)
{
  TAILQ_INSERT_TAIL(&store_mem_queue, pkt, pkt_mem_link);
  store_mem_size += pkt->pkt_payloadlen;

  while(store_mem_size >= store_mem_size_max) {
    pkt = TAILQ_FIRST(&store_mem_queue);

    TAILQ_REMOVE(&store_mem_queue, pkt, pkt_mem_link);
    store_mem_size -= pkt->pkt_payloadlen;

    free(pkt->pkt_payload);
    pkt->pkt_payload = NULL;
  }
}




/*
 *
 */
static void
storage_disk_enq(th_pkt_t *pkt)
{
  th_storage_t *s;
  char fbuf[500];
  int fd;

  TAILQ_INSERT_TAIL(&store_disk_queue, pkt, pkt_disk_link);
  store_disk_size += pkt->pkt_payloadlen;

  if(curstore == NULL) {
    snprintf(fbuf, sizeof(fbuf), "%s/s%d", store_path, ++store_tally);
    
    fd = open(fbuf, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fd == -1) {
      s = NULL;
    } else {
      s = calloc(1, sizeof(th_storage_t));
      s->ts_fd = fd;
      s->ts_filename = strdup(fbuf);
    }
    curstore = s;
  } else {
    s = curstore;
  }


  if(s != NULL) {
    s->ts_refcount++;
    pkt->pkt_storage = s;
    pkt->pkt_storage_offset = s->ts_offset;
    s->ts_offset += pkt->pkt_payloadlen;
    if(s->ts_offset > store_chunk_size)
      curstore = NULL;
  }

  while(store_disk_size > store_disk_size_max) {
    pkt = TAILQ_FIRST(&store_disk_queue);
    if(pkt->pkt_refcount > 1)
      printf("UNSTORE of reference packet %p\n", pkt);
    pkt_unstore(pkt);
  }
}









/**
 * Erase all old files
 */
static void
storage_wipe(void)
{
  DIR *dir;
  struct dirent *d;
  char fbuf[500];

  if((dir = opendir(store_path)) != NULL) {

    while((d = readdir(dir)) != NULL) {
      if(d->d_name[0] == '.')
	continue;

      snprintf(fbuf, sizeof(fbuf), "%s/%s", store_path, d->d_name);
      unlink(fbuf);
    }
  }
  closedir(dir);
}
