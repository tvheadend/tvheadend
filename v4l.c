/*
 *  TV Input - Linux analogue (v4lv2) interface
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

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#define __user
#include <linux/videodev2.h>

#include <libhts/htscfg.h>

#include "tvhead.h"
#include "v4l.h"
#include "channels.h"
#include "dispatch.h"
#include "transports.h"

struct th_v4l_adapter_list v4l_adapters;

static void v4l_fd_callback(int events, void *opaque, int fd);

static int v4l_setfreq(th_v4l_adapter_t *tva, int frequency);

static void v4l_add_adapter(const char *path);


static void v4l_stop_feed(th_transport_t *t);

static int v4l_start_feed(th_transport_t *t, unsigned int weight, int status,
			  int force_start);

/* 
 *
 */
void
v4l_init(void)
{
  v4l_add_adapter("/dev/video0");
}



/* 
 *
 */
int
v4l_configure_transport(th_transport_t *t, const char *muxname,
			const char *channel_name)
{
  config_entry_t *ce;
  char buf[100];

  if((ce = find_mux_config("v4lmux", muxname)) == NULL)
    return -1;

  t->tht_type = TRANSPORT_V4L;
  t->tht_start_feed = v4l_start_feed;
  t->tht_stop_feed  = v4l_stop_feed;

  t->tht_v4l_frequency = 
    atoi(config_get_str_sub(&ce->ce_sub, "frequency", "0"));

  t->tht_video = transport_add_stream(t, -1, HTSTV_MPEG2VIDEO);
  t->tht_audio = transport_add_stream(t, -1, HTSTV_MPEG2AUDIO);

  snprintf(buf, sizeof(buf), "Analog: %s (%.2f MHz)", muxname, 
	   (float)t->tht_v4l_frequency / 1000000.0f);
  t->tht_name = strdup(buf);

  t->tht_provider = strdup("Analog TV");
  snprintf(buf, sizeof(buf), "analog_%u", t->tht_v4l_frequency);
  t->tht_identifier = strdup(buf);

  t->tht_chname = strdup(channel_name);

  transport_map_channel(t, NULL);
  return 0;
}


/*
 *
 */
static void
v4l_add_adapter(const char *path)
{
  int fd, r;
  th_v4l_adapter_t *tva;
  struct v4l2_capability caps;

  fd = open(path, O_RDWR);
  if(fd == -1)
    return;

  r = ioctl(fd, VIDIOC_QUERYCAP, &caps);

  close(fd);
  if(r < 0)
    return;


  tva = calloc(1, sizeof(th_v4l_adapter_t));

  pthread_cond_init(&tva->tva_run_cond, NULL);

  tva->tva_path = strdup(path);
  LIST_INSERT_HEAD(&v4l_adapters, tva, tva_link);

  tva->tva_name = strdup((char *)caps.card);
}




/* 
 *
 */
static int 
v4l_setfreq(th_v4l_adapter_t *tva, int frequency)
{
  struct v4l2_frequency vf;
  struct v4l2_tuner vt;
  int result;

  memset(&vf, 0, sizeof(vf));
  memset(&vt, 0, sizeof(vt));

  vf.tuner = 0;
  vf.type = V4L2_TUNER_ANALOG_TV;
  vf.frequency = (frequency * 16) / 1000000;
  result = ioctl(tva->tva_fd, VIDIOC_S_FREQUENCY, &vf);
  if(result < 0) {
    tvhlog(LOG_ERR, "v4l",
	   "%s: Unable to tune to %dHz\n", tva->tva_path, frequency);
    return 1;
  }

  vt.index = 0;
  result = ioctl(tva->tva_fd, VIDIOC_G_TUNER, &vt);

  if(result < 0) {
    tvhlog(LOG_ERR, "v4l", "%s: Unable read tuner status\n", tva->tva_path);
    return 1;
  }
	
  tvhlog(LOG_DEBUG, "v4l", "%s: Tuned to %.3f MHz%s",
	 tva->tva_path, (float)frequency/1000000.0,
	 vt.signal ? "  (Signal Detected)" : "");

  return 0;
}


/* 
 *
 */
static void
v4l_stop(th_v4l_adapter_t *tva)
{
  if(tva->tva_dispatch_handle != NULL) {
    close(dispatch_delfd(tva->tva_dispatch_handle));
    tva->tva_dispatch_handle = NULL;
  }
  tva->tva_startcode = 0;
}



/* 
 *
 */
static void
v4l_stop_feed(th_transport_t *t)
{
  th_v4l_adapter_t *tva = t->tht_v4l_adapter;

  t->tht_v4l_adapter = NULL;
  LIST_REMOVE(t, tht_active_link);

  t->tht_runstatus = TRANSPORT_IDLE;

  if(LIST_FIRST(&tva->tva_transports) == NULL)
    v4l_stop(tva);
}



/* 
 *
 */
static void
v4l_adapter_clean(th_v4l_adapter_t *tva)
{
  th_transport_t *t;
  
  while((t = LIST_FIRST(&tva->tva_transports)) != NULL)
    v4l_stop_feed(t);

  v4l_stop(tva);
}





/* 
 *
 */
static int 
v4l_start_feed(th_transport_t *t, unsigned int weight, int status,
	       int force_start)
{
  th_v4l_adapter_t *tva, *cand = NULL;
  int w, fd;

  LIST_FOREACH(tva, &v4l_adapters, tva_link) {
    w = transport_compute_weight(&tva->tva_transports);
    if(w < weight)
      cand = tva;

    if(tva->tva_frequency == t->tht_v4l_frequency)
      break;
  }

  if(tva == NULL) {
    if(cand == NULL)
      return 1;

    v4l_adapter_clean(cand);
    tva = cand;
  }

  if(tva->tva_dispatch_handle == NULL) {
    fd = open(tva->tva_path, O_RDWR);
    if(fd == -1)
      return 1;

    tva->tva_dispatch_handle = 
      dispatch_addfd(fd, v4l_fd_callback, tva, DISPATCH_READ);
    tva->tva_fd = fd;
  }

  tva->tva_frequency = t->tht_v4l_frequency;

  if(v4l_setfreq(tva, tva->tva_frequency))
    return 1;

  LIST_INSERT_HEAD(&tva->tva_transports, t, tht_active_link);
  t->tht_v4l_adapter = tva;
  t->tht_runstatus = TRANSPORT_RUNNING;
  
  return 0;
}






static void
v4l_fd_callback(int events, void *opaque, int fd)
{
  th_v4l_adapter_t *tva = opaque;
  th_transport_t *t;
  th_stream_t *st;
  uint8_t buf[4000];
  uint8_t *ptr, *pkt;
  int len, l, r;

  if(!(events & DISPATCH_READ))
    return;

  len = read(fd, buf, 4000);
  if(len < 1)
    return;

  t = LIST_FIRST(&tva->tva_transports);
  if(t == NULL)
    return;

  ptr = buf;

  while(len > 0) {

    switch(tva->tva_startcode) {
    default:
      tva->tva_startcode = tva->tva_startcode << 8 | *ptr;
      tva->tva_lenlock = 0;
      ptr++; len--;
      continue;

    case 0x000001e0:
      st = t->tht_video;
      break;
    case 0x000001c0:
      st = t->tht_audio;
      break;
    }

    if(tva->tva_lenlock == 2) {
      l = st->st_buffer_size;
      st->st_buffer = pkt = realloc(st->st_buffer, l);
      
      r = l - st->st_buffer_ptr;
      if(r > len)
	r = len;
      memcpy(pkt + st->st_buffer_ptr, ptr, r);
      
      ptr += r;
      len -= r;

      st->st_buffer_ptr += r;
      if(st->st_buffer_ptr == l) {
	//	pes_packet_input(t, st, pkt, l);
	st->st_buffer_size = 0;
	tva->tva_startcode = 0;
      }
      
    } else {
      st->st_buffer_size = st->st_buffer_size << 8 | *ptr;
      tva->tva_lenlock++;
      if(tva->tva_lenlock == 2) {
	st->st_buffer_ptr = 0;
      }
      ptr++; len--;
    }
  }
}
