/*
 *  IPTV Input
 *
 *  Copyright (C) 2013 Andreas Öman
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

#include "iptv_private.h"
#include "tvhpoll.h"
#include "tcp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

#if defined(PLATFORM_FREEBSD)
#  ifndef IPV6_ADD_MEMBERSHIP
#    define IPV6_ADD_MEMBERSHIP	IPV6_JOIN_GROUP
#    define IPV6_DROP_MEMBERSHIP	IPV6_LEAVE_GROUP
#  endif
#endif

/*
 * Globals
 */
iptv_input_t    iptv_input;
iptv_network_t  iptv_network;
tvhpoll_t      *iptv_poll;
pthread_t       iptv_thread;
pthread_mutex_t iptv_lock;


/*
 * URL processing - TODO: move to a library
 */

typedef struct url
{
  char      scheme[16];
  char      user[128];
  char      pass[128];
  char      host[256];
  uint16_t  port;
  char      path[256];
} url_t;

#define UC "[a-z0-9_\\-\\.!£$%^&]"
#define PC UC
#define HC "[a-z0-9\\-\\.]"
#define URL_RE "^(\\w+)://(("UC"+)(:("PC"+))?@)?("HC"+)(:([0-9]+))?(/.*)?"

regex_t urlre;

static int 
urlparse ( const char *str, url_t *url )
{
  regmatch_t m[12];
  char buf[16];
  
  memset(m, 0, sizeof(m));
  
  if (regexec(&urlre, str, 12, m, 0))
    return 1;
    
#define copy(x, i)\
  {\
    int len = m[i].rm_eo - m[i].rm_so;\
    if (len >= sizeof(x) - 1)\
      len = sizeof(x) - 1;\
    memcpy(x, str+m[i].rm_so, len);\
    x[len] = 0;\
  }(void)0
  copy(url->scheme, 1);
  copy(url->user,   3);
  copy(url->pass,   5);
  copy(url->host,   6);
  copy(url->path,   9);
  copy(buf,         8);
  url->port = atoi(buf);
  return 0;
}

/*
 * HTTP client
 */

static int http_connect (url_t *url)
{
  int fd, c, i;
  char buf[1024];

  if (!url->port)
    url->port = strcmp("https", url->scheme) ? 80 : 443;

  /* Make connection */
  // TODO: move connection to thread
  // TODO: this is really only for testing and to allow use of TVH webserver as input
  tvhlog(LOG_DEBUG, "iptv", "connecting to http %s %d",
         url->host, url->port);
  fd = tcp_connect(url->host, url->port, buf, sizeof(buf), 10);
  if (fd < 0) {
    tvhlog(LOG_ERR, "iptv", "tcp_connect() failed %s", buf);
    return -1;
  }

  /* Send request (VERY basic) */
  c = snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\n", url->path);
  tvh_write(fd, buf, c);
  c = snprintf(buf, sizeof(buf), "Hostname: %s\r\n", url->host);
  tvh_write(fd, buf, c);
  tvh_write(fd, "\r\n", 2);

  /* Read back header */
  // TODO: do this properly
  i = 0;
  while (1) {
    if (!(c = read(fd, buf+i, 1)))
      continue;
    i++;
    if (i == 4 && !strncmp(buf, "\r\n\r\n", 4))
      break;
    memmove(buf, buf+1, 3); i = 3;
  }

  return fd;
}

/*
 * Input definition
 */
static const char *
iptv_input_class_get_title ( idnode_t *self )
{
  return "IPTV";
}
extern const idclass_t mpegts_input_class;
const idclass_t iptv_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "iptv_input",
  .ic_caption    = "IPTV Input",
  .ic_get_title  = iptv_input_class_get_title,
  .ic_properties = (const property_t[]){
    {}
  }
};

/*
 * HTTP
 */
static int
iptv_input_start_http
  ( iptv_mux_t *im, url_t *url, const char *name )
{
  /* Setup connection */
  return http_connect(url);
}

/*
 * UDP
 *
 * TODO: add IPv6 support
 */
static int
iptv_input_start_udp
  ( iptv_mux_t *im, url_t *url, const char *name )
{
  int fd, solip, rxsize, reuse;
  struct ifreq ifr;
  struct ip_mreqn m;
  struct addrinfo *addr;
  struct sockaddr_in sin;

  /* Open socket */
  if ((fd = tvh_socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    tvherror("iptv", "%s - failed to create socket [%s]",
             name, strerror(errno));
    return -1;
  }

  /* Bind to interface */
  if (im->mm_iptv_interface && *im->mm_iptv_interface) {
    printf("have interface\n");
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", im->mm_iptv_interface);
    if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
      tvherror("iptv", "%s - could not find interface %s",
               name, im->mm_iptv_interface);
      goto error;
    }
  }

  /* Lookup hostname */
  if (getaddrinfo(url->host, NULL, NULL, &addr)) {
    tvherror("iptv", "%s - failed to lookup host %s", name, url->host);
    return -1;
  }

  /* Bind to address */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family      = AF_INET;
  sin.sin_port        = htons(url->port); // TODO: default?
  sin.sin_addr.s_addr = ((struct sockaddr_in*)addr->ai_addr)->sin_addr.s_addr;
  freeaddrinfo(addr);
  reuse = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    tvherror("iptv", "%s - cannot bind %s:%d [%s]",
             name, inet_ntoa(sin.sin_addr), 0, strerror(errno));
    goto error;
  }

  
  /* Join IPv4 group */
  memset(&m, 0, sizeof(m));
  m.imr_multiaddr.s_addr = sin.sin_addr.s_addr;
  m.imr_address.s_addr = 0;
#if defined(PLATFORM_LINUX)
  m.imr_ifindex = ifr.ifr_ifindex;
#elif defined(PLATFORM_FREEBSD)
  m.imr_ifindex = ifr.ifr_index;
#endif

#ifdef SOL_IP
  solip = SOL_IP;
#else
  {
    struct protoent *pent;
    pent = getprotobyname("ip");
    solip = (pent != NULL) ? pent->p_proto : 0;
  }
#endif

  if (setsockopt(fd, solip, IP_ADD_MEMBERSHIP, &m,
                 sizeof(struct ip_mreqn)) == -1) {
    tvherror("iptv", "%s - cannot join %s [%s]",
             name, inet_ntoa(m.imr_multiaddr), strerror(errno));
    goto error;
  }

  /* Increase RX buffer size */
  rxsize = IPTV_PKT_SIZE;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rxsize, sizeof(rxsize)) == -1)
    tvhwarn("iptv", "%s - cannot increase UDP rx buffer size [%s]",
            name, strerror(errno));

  /* Done */
  return fd;

error:
  close(fd);
  return -1;
}

static int
iptv_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int fd, ret = SM_CODE_TUNING_FAILED;
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_iptv_instance);
  char buf[256];
  url_t url;

  im->mm_display_name((mpegts_mux_t*)im, buf, sizeof(buf));
  if (urlparse(im->mm_iptv_url ?: "", &url)) {
    tvherror("iptv", "%s - invalid URL [%s]", buf, im->mm_iptv_url);
    return ret;
  }

  pthread_mutex_lock(&iptv_lock);
  if (!im->mm_active) {
    
    /* HTTP */
    // TODO: this needs to happen in a thread
    if (!strcmp("http", url.scheme))
      fd = iptv_input_start_http(im, &url, buf);

    /* UDP/RTP (both mcast) */
    else if (!strcmp(url.scheme, "udp") ||
             !strcmp(url.scheme, "rtp"))
      fd = iptv_input_start_udp(im, &url, buf);
  
    /* Unknown */
    else {
      tvherror("iptv", "%s - invalid scheme [%s]", buf, url.scheme);
      fd = -1;
    }

    /* OK */
    if (fd != -1) {
      tvhpoll_event_t ev;
      memset(&ev, 0, sizeof(ev));
      ev.events          = TVHPOLL_IN;
      ev.fd = ev.data.fd = im->mm_iptv_fd = fd;
      if (tvhpoll_add(iptv_poll, &ev, 1) == -1) {
        tvherror("iptv", "%s - failed to add to poll q", buf);
        close(fd);
      } else {
        im->mm_active   = mmi;
        im->mm_iptv_pos = 0;
        im->mm_iptv_tsb = calloc(1, IPTV_PKT_SIZE);

        /* Install table handlers */
        mpegts_table_add(mmi->mmi_mux, DVB_PAT_BASE, DVB_PAT_MASK,
                         dvb_pat_callback, NULL, "pat",
                         MT_QUICKREQ| MT_CRC, DVB_PAT_PID);

        ret = 0;
      }
    }
  }
  pthread_mutex_unlock(&iptv_lock);

  return ret;
}

static void
iptv_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  iptv_mux_t *im = (iptv_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_iptv_instance);
  
  pthread_mutex_lock(&iptv_lock);
  if (im->mm_active) {

    // TODO: multicast will require additional work
    // TODO: need to leave group?

    /* Close file */
    close(im->mm_iptv_fd); // removes from poll
    im->mm_iptv_fd = -1;

    /* Free memory */
    free(im->mm_iptv_tsb);
    im->mm_iptv_tsb = NULL;
  }
  pthread_mutex_unlock(&iptv_lock);
}

static int
iptv_input_is_free ( mpegts_input_t *mi )
{
  return 1; // unlimited number of muxes
}

static int
iptv_input_get_weight ( mpegts_input_t *mi )
{
  return 0; // unlimited number of muxes
}

static void
iptv_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "IPTV");
}

static void *
iptv_input_thread ( void *aux )
{
  int hlen, nfds, fd, r;
  iptv_mux_t *im;
  mpegts_mux_t *mm;
  tvhpoll_event_t ev;

  while ( 1 ) {
    nfds = tvhpoll_wait(iptv_poll, &ev, 1, -1);
    if ( nfds < 0 ) {
      tvhlog(LOG_ERR, "iptv", "poll() error %s, sleeping 1 second",
             strerror(errno));
      sleep(1);
      continue;
    } else if ( nfds == 0 ) {
      continue;
    }
    fd = ev.data.fd;

    /* Find mux */
    pthread_mutex_lock(&iptv_lock);
    LIST_FOREACH(mm, &iptv_network.mn_muxes, mm_network_link) {
      if (((iptv_mux_t*)mm)->mm_iptv_fd == fd)
        break;
    }
    if (!mm) {
      pthread_mutex_unlock(&iptv_lock);
      continue;
    }
    im  = (iptv_mux_t*)mm;

    /* Read data */
    //tvhtrace("iptv", "read(%d)", fd);
    r  = read(fd, im->mm_iptv_tsb+im->mm_iptv_pos,
              IPTV_PKT_SIZE-im->mm_iptv_pos);

    /* Error */
    if (r < 0) {
      tvhlog(LOG_ERR, "iptv", "read() error %s", strerror(errno));
      // TODO: close and remove?
      continue;
    }
    r += im->mm_iptv_pos;

    /* RTP */
    hlen = 0;
    if (!strncmp(im->mm_iptv_url, "rtp", 3)) {
      if (r < 12)
        goto done;
      if ((im->mm_iptv_tsb[0] & 0xC0) != 0x80)
        goto done;
      if ((im->mm_iptv_tsb[1] & 0x7F) != 33)
        goto done;
      hlen = ((im->mm_iptv_tsb[0] & 0xf) * 4) + 12;
      if (im->mm_iptv_tsb[0] & 0x10) {
        if (r < hlen+4)
          goto done;
        hlen += (im->mm_iptv_tsb[hlen+2] << 8) | (im->mm_iptv_tsb[hlen+3]*4);
        hlen += 4;
      }
      if (r < hlen || ((r - hlen) % 188) != 0)
        goto done;
    }

    /* TS */
#if 0
    tvhtrace("iptv", "recv data %d (%d)", (int)r, hlen);
    tvhlog_hexdump("iptv", im->mm_iptv_tsb, r);
#endif
    im->mm_iptv_pos = mpegts_input_recv_packets((mpegts_input_t*)&iptv_input,
                                    &im->mm_iptv_instance,
                                    im->mm_iptv_tsb+hlen, r-hlen, NULL, NULL, "iptv");
    //tvhtrace("iptv", "pos = %d", im->mm_iptv_pos);
done:
    pthread_mutex_unlock(&iptv_lock);
  }
  return NULL;
}

/*
 * Network definition
 */
extern const idclass_t mpegts_network_class;
const idclass_t iptv_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "iptv_network",
  .ic_caption    = "IPTV Network",
  .ic_properties = (const property_t[]){
    {}
  }
};

static mpegts_mux_t *
iptv_network_create_mux2
  ( mpegts_network_t *mm, htsmsg_t *conf )
{
  return (mpegts_mux_t*)iptv_mux_create(NULL, conf);
}

static mpegts_service_t *
iptv_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return (mpegts_service_t*)
    iptv_service_create0((iptv_mux_t*)mm, sid, pmt_pid, NULL, NULL);
}

static const idclass_t *
iptv_network_mux_class ( mpegts_network_t *mm )
{
  extern const idclass_t iptv_mux_class;
  return &iptv_mux_class;
}

/*
 * Intialise and load config
 */
void iptv_init ( void )
{
  pthread_t tid;
  
  /* Initialise URL RE */
  if (regcomp(&urlre, URL_RE, REG_ICASE | REG_EXTENDED)) {
    tvherror("iptv", "failed to compile regexp");
    exit(1);
  }

  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)&iptv_input,
                       &iptv_input_class, NULL, NULL);
  iptv_input.mi_start_mux      = iptv_input_start_mux;
  iptv_input.mi_stop_mux       = iptv_input_stop_mux;
  iptv_input.mi_is_free        = iptv_input_is_free;
  iptv_input.mi_get_weight     = iptv_input_get_weight;
  iptv_input.mi_display_name   = iptv_input_display_name;
  iptv_input.mi_enabled        = 1;

  /* Init Network */
  mpegts_network_create0((mpegts_network_t*)&iptv_network,
                         &iptv_network_class, NULL, "IPTV Network", NULL);
  iptv_network.mn_create_service = iptv_network_create_service;
  iptv_network.mn_mux_class      = iptv_network_mux_class;
  iptv_network.mn_mux_create2    = iptv_network_create_mux2;

  /* Link */
  mpegts_input_set_network((mpegts_input_t*)&iptv_input,
                           (mpegts_network_t*)&iptv_network);
  /* Set table thread */
  tvhthread_create(&tid, NULL, mpegts_input_table_thread, &iptv_input, 1);

  /* Setup TS thread */
  // TODO: could set this up only when needed
  iptv_poll = tvhpoll_create(10);
  pthread_mutex_init(&iptv_lock, NULL);
  tvhthread_create(&iptv_thread, NULL, iptv_input_thread, NULL, 1);

  /* Load config */
  iptv_mux_load_all();
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
