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
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <inttypes.h>

#include "settings.h"

#include "tvhead.h"
#include "transports.h"
#include "v4l.h"
#include "parsers.h"
#include "notify.h"
#include "psi.h"


struct v4l_adapter_queue v4l_adapters;

static void v4l_adapter_notify(v4l_adapter_t *va);


/**
 *
 */
static void
v4l_input(v4l_adapter_t *va)
{
  th_transport_t *t = va->va_current_transport;
  th_stream_t *st;
  uint8_t buf[4000];
  uint8_t *ptr, *pkt;
  int len, l, r;

  len = read(va->va_fd, buf, 4000);
  if(len < 1)
    return;

  ptr = buf;

  pthread_mutex_lock(&t->tht_stream_mutex);

  while(len > 0) {

    switch(va->va_startcode) {
    default:
      va->va_startcode = va->va_startcode << 8 | *ptr;
      va->va_lenlock = 0;
      ptr++; len--;
      continue;

    case 0x000001e0:
      st = t->tht_video;
      break;
    case 0x000001c0:
      st = t->tht_audio;
      break;
    }

    if(va->va_lenlock == 2) {
      l = st->st_buffer2_size;
      st->st_buffer2 = pkt = realloc(st->st_buffer2, l);
      
      r = l - st->st_buffer2_ptr;
      if(r > len)
	r = len;
      memcpy(pkt + st->st_buffer2_ptr, ptr, r);
      
      ptr += r;
      len -= r;

      st->st_buffer2_ptr += r;
      if(st->st_buffer2_ptr == l) {
	parse_mpeg_ps(t, st, pkt + 6, l - 6);

	st->st_buffer2_size = 0;
	va->va_startcode = 0;
      } else {
	assert(st->st_buffer2_ptr < l);
      }
      
    } else {
      st->st_buffer2_size = st->st_buffer2_size << 8 | *ptr;
      va->va_lenlock++;
      if(va->va_lenlock == 2) {
	st->st_buffer2_size += 6;
	st->st_buffer2_ptr = 6;
      }
      ptr++; len--;
    }
  }
  pthread_mutex_unlock(&t->tht_stream_mutex);
}


/**
 *
 */
static void *
v4l_thread(void *aux)
{
  v4l_adapter_t *va = aux;
  struct pollfd pfd[2];
  int r;

  pfd[0].fd = va->va_pipe[0];
  pfd[0].events = POLLIN;
  pfd[1].fd = va->va_fd;
  pfd[1].events = POLLIN;

  while(1) {

    r = poll(pfd, 2, -1);
    if(r < 0) {
      tvhlog(LOG_ALERT, "v4l", "%s: poll() error %s, sleeping one second",
	     va->va_path, strerror(errno));
      sleep(1);
      continue;
    }

    if(pfd[0].revents & POLLIN) {
      // Message on control pipe, used to exit thread, do so
      break;
    }

    if(pfd[1].revents & POLLIN) {
      v4l_input(va);
    }
  }

  close(va->va_pipe[0]);
  return NULL;
}



/**
 *
 */
static int
v4l_transport_start(th_transport_t *t, unsigned int weight, int status, 
		    int force_start)
{
  v4l_adapter_t *va = t->tht_v4l_adapter;
  int frequency = t->tht_v4l_frequency;
  struct v4l2_frequency vf;
  int result;
  v4l2_std_id std = 0xff;
  int fd;

  fd = open(va->va_path, O_RDWR | O_NONBLOCK);
  if(fd == -1) {
    tvhlog(LOG_ERR, "v4l",
	   "%s: Unable to open device: %s\n", va->va_path, 
	   strerror(errno));
    return -1;
  }

  result = ioctl(fd, VIDIOC_S_STD, &std);
  if(result < 0) {
    tvhlog(LOG_ERR, "v4l",
	   "%s: Unable to set PAL -- %s", va->va_path, strerror(errno));
    close(fd);
    return -1;
  }

  memset(&vf, 0, sizeof(vf));

  vf.tuner = 0;
  vf.type = V4L2_TUNER_ANALOG_TV;
  vf.frequency = (frequency * 16) / 1000000;
  result = ioctl(fd, VIDIOC_S_FREQUENCY, &vf);
  if(result < 0) {
    tvhlog(LOG_ERR, "v4l",
	   "%s: Unable to tune to %dHz", va->va_path, frequency);
    close(fd);
    return -1;
  }

  tvhlog(LOG_DEBUG, "v4l",
	 "%s: Tuned to %dHz", va->va_path, frequency);

  if(pipe(va->va_pipe)) {
    tvhlog(LOG_ERR, "v4l",
	   "%s: Unable to create control pipe", va->va_path, strerror(errno));
    close(fd);
    return -1;
  }


  va->va_fd = fd;
  va->va_current_transport = t;
  t->tht_status = status;
  pthread_create(&va->va_thread, NULL, v4l_thread, va);
  v4l_adapter_notify(va);
  return 0;
}


/**
 *
 */
static void
v4l_transport_refresh(th_transport_t *t)
{

}


/**
 *
 */
static void
v4l_transport_stop(th_transport_t *t)
{
  char c = 'q';
  v4l_adapter_t *va = t->tht_v4l_adapter;

  assert(va->va_current_transport != NULL);

  if(write(va->va_pipe[1], &c, 1) != 1)
    tvhlog(LOG_ERR, "v4l", "Unable to close video thread -- %s",
	   strerror(errno));
  
  pthread_join(va->va_thread, NULL);

  close(va->va_pipe[1]);
  close(va->va_fd);

  va->va_current_transport = NULL;
  t->tht_status = TRANSPORT_IDLE;
  v4l_adapter_notify(va);
}


/**
 *
 */
static void
v4l_transport_save(th_transport_t *t)
{
  v4l_adapter_t *va = t->tht_v4l_adapter;
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "frequency", t->tht_v4l_frequency);
  
  if(t->tht_ch != NULL) {
    htsmsg_add_str(m, "channelname", t->tht_ch->ch_name);
    htsmsg_add_u32(m, "mapped", 1);
  }
  

  pthread_mutex_lock(&t->tht_stream_mutex);
  psi_save_transport_settings(m, t);
  pthread_mutex_unlock(&t->tht_stream_mutex);
  
  hts_settings_save(m, "v4lservices/%s/%s",
		    va->va_identifier, t->tht_identifier);

  htsmsg_destroy(m);
}


/**
 *
 */
static int
v4l_transport_quality(th_transport_t *t)
{
  return 100;
}


/**
 * Generate a descriptive name for the source
 */
static htsmsg_t *
v4l_transport_sourceinfo(th_transport_t *t)
{
  htsmsg_t *m = htsmsg_create_map();
  char buf[30];

  htsmsg_add_str(m, "adapter", t->tht_v4l_adapter->va_displayname);

  snprintf(buf, sizeof(buf), "%d Hz", t->tht_v4l_frequency);
  htsmsg_add_str(m, "mux", buf);

  return m;
}


/**
 *
 */
th_transport_t *
v4l_transport_find(v4l_adapter_t *va, const char *id, int create)
{
  th_transport_t *t;
  char buf[200];

  int vaidlen = strlen(va->va_identifier);

  if(id != NULL) {

    if(strncmp(id, va->va_identifier, vaidlen))
      return NULL;

    LIST_FOREACH(t, &va->va_transports, tht_group_link)
      if(!strcmp(t->tht_identifier, id))
	return t;
  }

  if(create == 0)
    return NULL;
  
  if(id == NULL) {
    va->va_tally++;
    snprintf(buf, sizeof(buf), "%s_%d", va->va_identifier, va->va_tally);
    id = buf;
  } else {
    va->va_tally = MAX(atoi(id + vaidlen + 1), va->va_tally);
  }

  t = transport_create(id, TRANSPORT_V4L, 0);

  t->tht_start_feed    = v4l_transport_start;
  t->tht_refresh_feed  = v4l_transport_refresh;
  t->tht_stop_feed     = v4l_transport_stop;
  t->tht_config_save   = v4l_transport_save;
  t->tht_sourceinfo    = v4l_transport_sourceinfo;
  t->tht_quality_index = v4l_transport_quality;
  t->tht_iptv_fd = -1;

  pthread_mutex_lock(&t->tht_stream_mutex); 
  t->tht_video = transport_stream_create(t, -1, SCT_MPEG2VIDEO); 
  t->tht_audio = transport_stream_create(t, -1, SCT_MPEG2AUDIO); 
  pthread_mutex_unlock(&t->tht_stream_mutex); 

  t->tht_v4l_adapter = va;

  LIST_INSERT_HEAD(&va->va_transports, t, tht_group_link);

  return t;
}


/**
 *
 */
static void
v4l_adapter_add(const char *path, const char *displayname, 
		const char *devicename)
{
  v4l_adapter_t *va;
  int i, r;

  va = calloc(1, sizeof(v4l_adapter_t));

  va->va_identifier = strdup(path);

  r = strlen(va->va_identifier);
  for(i = 0; i < r; i++)
    if(!isalnum((int)va->va_identifier[i]))
      va->va_identifier[i] = '_';

  va->va_displayname = strdup(displayname);
  va->va_path = path ? strdup(path) : NULL;
  va->va_devicename = devicename ? strdup(devicename) : NULL;

  TAILQ_INSERT_TAIL(&v4l_adapters, va, va_global_link);
}


/**
 *
 */
static void
v4l_adapter_check(const char *path, int fd)
{
  int r, i;

  char devicename[100];

  struct v4l2_capability caps;

  r = ioctl(fd, VIDIOC_QUERYCAP, &caps);

  if(r) {
    tvhlog(LOG_DEBUG, "v4l", 
	   "Can not query capabilities on %s, device skipped", path);
    return;
  }

  if(!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    tvhlog(LOG_DEBUG, "v4l", 
	   "Device %s not a video capture device, device skipped", path);
    return;
  }

  if(!(caps.capabilities & V4L2_CAP_TUNER)) {
    tvhlog(LOG_DEBUG, "v4l", 
	   "Device %s does not have a built-in tuner, device skipped", path);
    return;
  }

  /* Enum video standards */

  for(i = 0;; i++) {
    struct v4l2_standard standard;
    memset(&standard, 0, sizeof(standard));
    standard.index = i;

    if(ioctl(fd, VIDIOC_ENUMSTD, &standard))
      break;
#if 0
    printf("%3d: %016llx %24s %d/%d %d lines\n",
	   standard.index, 
	   standard.id,
	   standard.name,
	   standard.frameperiod.numerator,
	   standard.frameperiod.denominator,
	   standard.framelines);
#endif
  }


  /* Enum formats */
  for(i = 0;; i++) {

    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.index = i;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
      tvhlog(LOG_DEBUG, "v4l", 
	     "Device %s has no suitable formats, device skipped", path);
      return; 
    }

    if(fmtdesc.pixelformat == V4L2_PIX_FMT_MPEG)
      break;
  }

  snprintf(devicename, sizeof(devicename), "%s %s %s",
	   caps.card, caps.driver, caps.bus_info);

  tvhlog(LOG_INFO, "v4l",
	 "Found adapter %s (%s)", path, devicename);

  v4l_adapter_add(path, devicename, devicename);
}


/**
 *
 */
static void 
v4l_adapter_probe(const char *path)
{
  int fd;

  fd = open(path, O_RDWR | O_NONBLOCK);

  if(fd == -1) {
    if(errno != ENOENT)
      tvhlog(LOG_ALERT, "v4l",
	     "Unable to open %s -- %s", path, strerror(errno));
    return;
  }

  v4l_adapter_check(path, fd);

  close(fd);
}





/**
 * Save config for the given adapter
 */
static void
v4l_adapter_save(v4l_adapter_t *va)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_str(m, "displayname", va->va_displayname);
  htsmsg_add_u32(m, "logging", va->va_logging);
  hts_settings_save(m, "v4ladapters/%s", va->va_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
htsmsg_t *
v4l_adapter_build_msg(v4l_adapter_t *va)
{
  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_str(m, "identifier", va->va_identifier);
  htsmsg_add_str(m, "name", va->va_displayname);
  htsmsg_add_str(m, "type", "v4l");

  if(va->va_path)
    htsmsg_add_str(m, "path", va->va_path);

  if(va->va_devicename)
    htsmsg_add_str(m, "devicename", va->va_devicename);

  if(va->va_current_transport != NULL) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%d Hz", 
	     va->va_current_transport->tht_v4l_frequency);
    htsmsg_add_str(m, "currentMux", buf);
  } else {
    htsmsg_add_str(m, "currentMux", "- inactive -");
  }

  return m;
}


/**
 *
 */
static void
v4l_adapter_notify(v4l_adapter_t *va)
{
  notify_by_msg("tvAdapter", v4l_adapter_build_msg(va));
}


/**
 *
 */
v4l_adapter_t *
v4l_adapter_find_by_identifier(const char *identifier)
{
  v4l_adapter_t *va;
  
  TAILQ_FOREACH(va, &v4l_adapters, va_global_link)
    if(!strcmp(identifier, va->va_identifier))
      return va;
  return NULL;
}


/**
 *
 */
void
v4l_adapter_set_displayname(v4l_adapter_t *va, const char *name)
{
  lock_assert(&global_lock);

  if(!strcmp(name, va->va_displayname))
    return;

  tvhlog(LOG_NOTICE, "v4l", "Adapter \"%s\" renamed to \"%s\"",
	 va->va_displayname, name);

  tvh_str_set(&va->va_displayname, name);
  v4l_adapter_save(va);
  v4l_adapter_notify(va);
}


/**
 *
 */
void
v4l_adapter_set_logging(v4l_adapter_t *va, int on)
{
  if(va->va_logging == on)
    return;

  lock_assert(&global_lock);

  tvhlog(LOG_NOTICE, "v4l", "Adapter \"%s\" detailed logging set to: %s",
	 va->va_displayname, on ? "On" : "Off");

  va->va_logging = on;
  v4l_adapter_save(va);
  v4l_adapter_notify(va);
}



/**
 *
 */
static void
v4l_service_create_by_msg(v4l_adapter_t *va, htsmsg_t *c, const char *name)
{
  const char *s;
  unsigned int u32;

  th_transport_t *t = v4l_transport_find(va, name, 1);

  if(t == NULL)
    return;

  s = htsmsg_get_str(c, "channelname");
  if(htsmsg_get_u32(c, "mapped", &u32))
    u32 = 0;
  
  if(!htsmsg_get_u32(c, "frequency", &u32))
    t->tht_v4l_frequency = u32;

  if(s && u32)
    transport_map_channel(t, channel_find_by_name(s, 1), 0);
}

/**
 *
 */
static void
v4l_service_load(v4l_adapter_t *va)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load("v4lservices/%s", va->va_identifier)) == NULL)
    return;
 
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;
    
    v4l_service_create_by_msg(va, c, f->hmf_name);
  }
  htsmsg_destroy(l);
}

/**
 *
 */
void
v4l_init(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  char buf[256];
  int i;
  v4l_adapter_t *va;

  TAILQ_INIT(&v4l_adapters);
  
  for(i = 0; i < 8; i++) {
    snprintf(buf, sizeof(buf), "/dev/video%d", i);
    v4l_adapter_probe(buf);
  }

  l = hts_settings_load("v4ladapters");
  if(l != NULL) {
    HTSMSG_FOREACH(f, l) {
      if((c = htsmsg_get_map_by_field(f)) == NULL)
	continue;

      if((va = v4l_adapter_find_by_identifier(f->hmf_name)) == NULL) {
	/* Not discovered by hardware, create it */

	va = calloc(1, sizeof(v4l_adapter_t));
	va->va_identifier = strdup(f->hmf_name);
	va->va_path = NULL;
	va->va_devicename = NULL;
      }

      tvh_str_update(&va->va_displayname, htsmsg_get_str(c, "displayname"));
      htsmsg_get_u32(c, "logging", &va->va_logging);
    }
    htsmsg_destroy(l);
  }

  TAILQ_FOREACH(va, &v4l_adapters, va_global_link)
    v4l_service_load(va);
}
