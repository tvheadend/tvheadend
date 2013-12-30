/*
 *  tvheadend, TCP common functions
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
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "tcp.h"
#include "tvheadend.h"
#include "tvhpoll.h"
#include "queue.h"
#include "notify.h"

int tcp_preferred_address_family = AF_INET;

/**
 *
 */
int
tcp_connect(const char *hostname, int port, char *errbuf, size_t errbufsize,
	    int timeout)
{
  const char *errtxt;
  struct hostent hostbuf, *hp;
  char *tmphstbuf;
  size_t hstbuflen;
  int herr, fd, r, res, err;
  struct sockaddr_in6 in6;
  struct sockaddr_in in;
  socklen_t errlen = sizeof(int);

  hstbuflen = 1024;
  tmphstbuf = malloc(hstbuflen);

  while((res = gethostbyname_r(hostname, &hostbuf, tmphstbuf, hstbuflen,
			       &hp, &herr)) == ERANGE) {
    hstbuflen *= 2;
    tmphstbuf = realloc(tmphstbuf, hstbuflen);
  }
  
  if(res != 0) {
    snprintf(errbuf, errbufsize, "Resolver internal error");
    free(tmphstbuf);
    return -1;
  } else if(herr != 0) {
    switch(herr) {
    case HOST_NOT_FOUND:
      errtxt = "The specified host is unknown";
      break;
    case NO_ADDRESS:
      errtxt = "The requested name is valid but does not have an IP address";
      break;
      
    case NO_RECOVERY:
      errtxt = "A non-recoverable name server error occurred";
      break;
      
    case TRY_AGAIN:
      errtxt = "A temporary error occurred on an authoritative name server";
      break;
      
    default:
      errtxt = "Unknown error";
      break;
    }

    snprintf(errbuf, errbufsize, "%s", errtxt);
    free(tmphstbuf);
    return -1;
  } else if(hp == NULL) {
    snprintf(errbuf, errbufsize, "Resolver internal error");
    free(tmphstbuf);
    return -1;
  }
  fd = tvh_socket(hp->h_addrtype, SOCK_STREAM, 0);
  if(fd == -1) {
    snprintf(errbuf, errbufsize, "Unable to create socket: %s",
	     strerror(errno));
    free(tmphstbuf);
    return -1;
  }

  /**
   * Switch to nonblocking
   */
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  switch(hp->h_addrtype) {
  case AF_INET:
    memset(&in, 0, sizeof(in));
    in.sin_family = AF_INET;
    in.sin_port = htons(port);
    memcpy(&in.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
    r = connect(fd, (struct sockaddr *)&in, sizeof(struct sockaddr_in));
    break;

  case AF_INET6:
    memset(&in6, 0, sizeof(in6));
    in6.sin6_family = AF_INET6;
    in6.sin6_port = htons(port);
    memcpy(&in6.sin6_addr, hp->h_addr_list[0], sizeof(struct in6_addr));
    r = connect(fd, (struct sockaddr *)&in, sizeof(struct sockaddr_in6));
    break;

  default:
    snprintf(errbuf, errbufsize, "Invalid protocol family");
    free(tmphstbuf);
    return -1;
  }

  free(tmphstbuf);

  if(r == -1) {
    if(errno == EINPROGRESS) {
      struct pollfd pfd;

      pfd.fd = fd;
      pfd.events = POLLOUT;
      pfd.revents = 0;

      r = poll(&pfd, 1, timeout * 1000);
      if(r == 0) {
	      /* Timeout */
      	snprintf(errbuf, errbufsize, "Connection attempt timed out");
      	close(fd);
      	return -1;
      }
      
      if(r == -1) {
      	snprintf(errbuf, errbufsize, "poll() error: %s", strerror(errno));
      	close(fd);
      	return -1;
      }

      getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);
    } else {
      err = errno;
    }
  } else {
    err = 0;
  }

  if(err != 0) {
    snprintf(errbuf, errbufsize, "%s", strerror(err));
    close(fd);
    return -1;
  }
  
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
  return fd;
}


/**
 *
 */
int
tcp_write_queue(int fd, htsbuf_queue_t *q)
{
  htsbuf_data_t *hd;
  int l, r = 0;
  void *p;

  while((hd = TAILQ_FIRST(&q->hq_q)) != NULL) {
    if (!r) {
      l = hd->hd_data_len - hd->hd_data_off;
      p = hd->hd_data + hd->hd_data_off;
      r = tvh_write(fd, p, l);
    }
    htsbuf_data_free(q, hd);
  }
  q->hq_size = 0;
  return r;
}


/**
 *
 */
static int
tcp_fill_htsbuf_from_fd(int fd, htsbuf_queue_t *hq)
{
  htsbuf_data_t *hd = TAILQ_LAST(&hq->hq_q, htsbuf_data_queue);
  int c;

  if(hd != NULL) {
    /* Fill out any previous buffer */
    c = hd->hd_data_size - hd->hd_data_len;

    if(c > 0) {

      c = read(fd, hd->hd_data + hd->hd_data_len, c);
      if(c < 1)
	return -1;

      hd->hd_data_len += c;
      hq->hq_size += c;
      return 0;
    }
  }
  
  hd = malloc(sizeof(htsbuf_data_t));
  
  hd->hd_data_size = 1000;
  hd->hd_data = malloc(hd->hd_data_size);

  c = read(fd, hd->hd_data, hd->hd_data_size);
  if(c < 1) {
    free(hd->hd_data);
    free(hd);
    return -1;
  }
  hd->hd_data_len = c;
  hd->hd_data_off = 0;
  TAILQ_INSERT_TAIL(&hq->hq_q, hd, hd_link);
  hq->hq_size += c;
  return 0;
}


/**
 *
 */
char *
tcp_read_line(int fd, htsbuf_queue_t *spill)
{
  int len;
  char *buf;

  do {
    len = htsbuf_find(spill, 0xa);

    if(len == -1) {
      if(tcp_fill_htsbuf_from_fd(fd, spill) < 0)
        return NULL;
    }
  } while (len == -1);

  buf = malloc(len+1);
  
  htsbuf_read(spill, buf, len);
  buf[len] = 0;
  while(len > 0 && buf[len - 1] < 32)
    buf[--len] = 0;
  htsbuf_drop(spill, 1); /* Drop the \n */
  return buf;
}



/**
 *
 */
int
tcp_read_data(int fd, char *buf, const size_t bufsize, htsbuf_queue_t *spill)
{
  int x, tot = htsbuf_read(spill, buf, bufsize);

  if(tot == bufsize)
    return 0;

  x = recv(fd, buf + tot, bufsize - tot, MSG_WAITALL);
  if(x != bufsize - tot)
    return -1;

  return 0;
}

/**
 *
 */
int
tcp_read(int fd, void *buf, size_t len)
{
  int x = recv(fd, buf, len, MSG_WAITALL);

  if(x == -1)
    return errno;
  if(x != len)
    return ECONNRESET;
  return 0;

}

/**
 *
 */
int
tcp_read_timeout(int fd, void *buf, size_t len, int timeout)
{
  int x, tot = 0;
  struct pollfd fds;

  assert(timeout > 0);

  fds.fd = fd;
  fds.events = POLLIN;
  fds.revents = 0;

  while(tot != len) {

    x = poll(&fds, 1, timeout);
    if(x == 0)
      return ETIMEDOUT;

    x = recv(fd, buf + tot, len - tot, MSG_DONTWAIT);
    if(x == -1) {
      if(errno == EAGAIN)
	      continue;
      return errno;
    }

    if(x == 0)
      return ECONNRESET;

    tot += x;
  }

  return 0;

}

/**
 *
 */
char *
tcp_get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  if(sa == NULL || s == NULL)
    return NULL;

  switch(sa->sa_family)
  {
    case AF_INET:
      inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), s, maxlen);
      break;
    case AF_INET6:
      inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), s, maxlen);
      break;
    default:
      strncpy(s, "Unknown AF", maxlen);
      return NULL;
  }

  return s;
}

/**
 *
 */
static tvhpoll_t *tcp_server_poll;

typedef struct tcp_server {
  int serverfd;
  tcp_server_ops_t ops;
  void *opaque;
} tcp_server_t;

typedef struct tcp_server_launch {
  int fd;
  tcp_server_ops_t ops;
  void *opaque;
  struct sockaddr_storage peer;
  struct sockaddr_storage self;
  time_t started;
  LIST_ENTRY(tcp_server_launch) link;
} tcp_server_launch_t;

static LIST_HEAD(, tcp_server_launch) tcp_server_launches = { 0 };

/**
 *
 */
static void *
tcp_server_start(void *aux)
{
  tcp_server_launch_t *tsl = aux;
  struct timeval to;
  int val;

  val = 1;
  setsockopt(tsl->fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
  
#ifdef TCP_KEEPIDLE
  val = 30;
  setsockopt(tsl->fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val));
#endif

#ifdef TCP_KEEPINVL
  val = 15;
  setsockopt(tsl->fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val));
#endif

#ifdef TCP_KEEPCNT
  val = 5;
  setsockopt(tsl->fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val));
#endif

  val = 1;
  setsockopt(tsl->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));

  to.tv_sec  = 30;
  to.tv_usec =  0;
  setsockopt(tsl->fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

  /* Start */
  time(&tsl->started);
  if (tsl->ops.status) {
    pthread_mutex_lock(&global_lock);
    LIST_INSERT_HEAD(&tcp_server_launches, tsl, link);
    notify_reload("connections");
    pthread_mutex_unlock(&global_lock);
  }
  pthread_mutex_lock(&global_lock);
  tsl->ops.start(tsl->fd, &tsl->opaque, &tsl->peer, &tsl->self);

  /* Stop */
  if (tsl->ops.stop) tsl->ops.stop(tsl->opaque);
  if (tsl->ops.status) {
    LIST_REMOVE(tsl, link);
    notify_reload("connections");
  }
  pthread_mutex_unlock(&global_lock);

  free(tsl);
  return NULL;
}


/**
 *
 */
static void *
tcp_server_loop(void *aux)
{
  int r;
  tvhpoll_event_t ev;
  tcp_server_t *ts;
  tcp_server_launch_t *tsl;
  pthread_attr_t attr;
  pthread_t tid;
  socklen_t slen;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  while(1) {
    r = tvhpoll_wait(tcp_server_poll, &ev, 1, -1);
    if(r == -1) {
      perror("tcp_server: tchpoll_wait");
      continue;
    }

    if (r == 0) continue;

    ts = ev.data.ptr;

    if(ev.events & TVHPOLL_HUP) {
	    close(ts->serverfd);
    	free(ts);
      continue;
    } 

    if(ev.events & TVHPOLL_IN) {
	    tsl = malloc(sizeof(tcp_server_launch_t));
      tsl->ops    = ts->ops;
      tsl->opaque = ts->opaque;
      slen = sizeof(struct sockaddr_storage);

      tsl->fd = accept(ts->serverfd, 
			                 (struct sockaddr *)&tsl->peer, &slen);
     	if(tsl->fd == -1) {
     	  perror("accept");
     	  free(tsl);
     	  sleep(1);
     	  continue;
     	}

     	slen = sizeof(struct sockaddr_storage);
	    if(getsockname(tsl->fd, (struct sockaddr *)&tsl->self, &slen)) {
        close(tsl->fd);
        free(tsl);
        continue;
     	}

     	tvhthread_create(&tid, &attr, tcp_server_start, tsl, 1);
    }
  }
  return NULL;
}

/**
 *
 */
void *
tcp_server_create
  (const char *bindaddr, int port, tcp_server_ops_t *ops, void *opaque)
{
  int fd, x;
  tvhpoll_event_t ev;
  tcp_server_t *ts;
  struct addrinfo hints, *res, *ressave, *use = NULL;
  char port_buf[6];
  int one = 1;
  int zero = 0;

  memset(&ev, 0, sizeof(ev));

  snprintf(port_buf, 6, "%d", port);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_PASSIVE;
  if (bindaddr != NULL)
      hints.ai_flags |= AI_NUMERICHOST;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  x = getaddrinfo(bindaddr, port_buf, &hints, &res);

  if(x != 0) {
    tvhlog(LOG_ERR, "tcp", "getaddrinfo: %s: %s", bindaddr != NULL ? bindaddr : "*",
      x == EAI_SYSTEM ? strerror(errno) : gai_strerror(x));
    return NULL;
  }

  ressave = res;
  while(res)
  {
    if(res->ai_family == tcp_preferred_address_family)
    {
      use = res;
      break;
    }
    else if(use == NULL)
    {
      use = res;
    }
    res = res->ai_next;
  }

  fd = tvh_socket(use->ai_family, use->ai_socktype, use->ai_protocol);
  if(fd == -1)
    return NULL;

  if(use->ai_family == AF_INET6)
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(int));

  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
  
  x = bind(fd, use->ai_addr, use->ai_addrlen);
  freeaddrinfo(ressave);

  if(x != 0)
  {
    tvhlog(LOG_ERR, "tcp", "bind: %s: %s", bindaddr != NULL ? bindaddr : "*", strerror(errno));
    close(fd);
    return NULL;
  }

  listen(fd, 1);

  ts = malloc(sizeof(tcp_server_t));
  ts->serverfd = fd;
  ts->ops    = *ops;
  ts->opaque = opaque;

  ev.fd       = fd;
  ev.events   = TVHPOLL_IN;
  ev.data.ptr = ts;
  tvhpoll_add(tcp_server_poll, &ev, 1);

  return ts;
}

/*
 * Connections status
 */
htsmsg_t *
tcp_server_connections ( void )
{
  tcp_server_launch_t *tsl;
  lock_assert(&global_lock);
  htsmsg_t *l, *e, *m;
  char buf[1024];
  int c = 0;
  
  /* Build list */
  l = htsmsg_create_list();
  LIST_FOREACH(tsl, &tcp_server_launches, link) {
    if (!tsl->ops.status) continue;
    c++;
    e = htsmsg_create_map();
    tcp_get_ip_str((struct sockaddr*)&tsl->peer, buf, sizeof(buf));
    htsmsg_add_str(e, "peer", buf);
    htsmsg_add_s64(e, "started", tsl->started);
    tsl->ops.status(tsl->opaque, e);
    htsmsg_add_msg(l, NULL, e);
  }

  /* Output */
  m = htsmsg_create_map();
  htsmsg_add_msg(m, "entries", l);
  htsmsg_add_u32(m, "totalCount", c);
  return m;
}

/**
 *
 */
void
tcp_server_init(int opt_ipv6)
{
  pthread_t tid;

  if(opt_ipv6)
    tcp_preferred_address_family = AF_INET6;

  tcp_server_poll = tvhpoll_create(10);
  tvhthread_create(&tid, NULL, tcp_server_loop, NULL, 1);
}
