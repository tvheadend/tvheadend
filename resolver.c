/*
 *  Async resolver
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>

#include <libhts/htsmsg.h>
#include <libhts/htsmsg_binary.h>

#include <libhts/htsq.h>

#include "resolver.h"
#include "dispatch.h"

static int res_seq_tally;

static int to_thread_fd[2];
static int from_thread_fd[2];

LIST_HEAD(res_list, res);

static struct res_list pending_resolve;

static void *resolver_loop(void *aux);

typedef struct res {
  void (*cb)(void *opaque, struct sockaddr *so, const char *error);
  void *opaque;
  int seqno;
  LIST_ENTRY(res) link;
} res_t;

static char response_buf[256];
static int  response_buf_ptr;

/**
 * Process response from resolver thread and invoke callback
 */
static void 
resolver_socket_callback(int events, void *opaque, int fd)
{
  htsmsg_t *m;
  int r, msglen;
  uint32_t seq;
  const char *err;
  res_t *res;
  struct sockaddr_in si;
  const void *ipv4;
  size_t ipv4len;

  if(response_buf_ptr < 4) {
    msglen = 4;
  } else {
    msglen = (response_buf[0] << 24 | response_buf[1] << 16 |
      response_buf[2] << 8 | response_buf[3]) + 4;
    if(msglen > 250) {
      fprintf(stderr, "Internal resolver error, max msglen exceeded\n");
      exit(1);
    }
  }

  r = read(from_thread_fd[0], response_buf + response_buf_ptr, 
	   msglen - response_buf_ptr);
  if(r < 1) {
    fprintf(stderr, "Internal resolver error, reading failed (%d)\n", r);
    perror("read");
    exit(1);
  }

  response_buf_ptr += r;

  if(msglen > 4 && response_buf_ptr == msglen) {
    m = htsmsg_binary_deserialize(response_buf + 4, msglen - 4, NULL);

    htsmsg_get_u32(m, "seq", &seq);

    LIST_FOREACH(res, &pending_resolve, link) 
      if(res->seqno == seq)
	break;
    
    if(res != NULL) {

      err = htsmsg_get_str(m, "error");
      if(err) {
	res->cb(res->opaque, NULL, err);
      } else if(htsmsg_get_bin(m, "ipv4", &ipv4, &ipv4len) == 0) {
	memset(&si, 0, sizeof(si));
	si.sin_family = AF_INET;
	memcpy(&si.sin_addr, ipv4, 4);
	res->cb(res->opaque, (struct sockaddr *)&si, NULL);
      }

      LIST_REMOVE(res, link);
      free(res);
    }
    response_buf_ptr = 0;
  }
}




/**
 * Setup async resolver
 */
static void
async_resolver_init(void)
{
  static int inited;
  pthread_t ptid;
  int fd;

  if(inited)
    return;

  inited = 1;

  if(pipe(to_thread_fd) == -1) {
    perror("Async resolver, cannot create pipe");
    exit(1);
  }

  if(pipe(from_thread_fd) == -1) {
    perror("Async resolver, cannot create pipe");
    exit(1);
  }

  pthread_create(&ptid, NULL, resolver_loop, NULL);

  fd = from_thread_fd[0];
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
  dispatch_addfd(fd, resolver_socket_callback, NULL, DISPATCH_READ);
}



/**
 * Resolver thread
 */

static void *
resolver_loop(void *aux)
{
  htsmsg_t *m;
  void *buf;
  int r, res, herr;
  unsigned int l;
  size_t len;
  struct hostent hostbuf, *hp;
  size_t hstbuflen;
  char *tmphstbuf;
  const char *hostname;
  const char *errtxt;

  hstbuflen = 1024;
  tmphstbuf = malloc(hstbuflen);

  while(1) {
  
    r = read(to_thread_fd[0], &l, 4);
    if(r != 4) {
      fprintf(stderr, "resolver: read error: header, r = %d\n", r);
      perror("read");
      break;
    }

    l = ntohl(l);
    buf = malloc(l);

    if(read(to_thread_fd[0], buf, l) != l) {
      free(buf);
      fprintf(stderr, "resolver: read error: payload, r = %d\n", r);
      perror("read");
      break;
    }

    m = htsmsg_binary_deserialize(buf, l, buf);
    free(buf);
    if(m == NULL) {
      fprintf(stderr, "resolver: cannot deserialize\n");
      continue;
    }
    hostname = htsmsg_get_str(m, "hostname");
    if(hostname == NULL) {
      fprintf(stderr, "resolver: missing hostname\n");
      break;
    }
    while((res = gethostbyname_r(hostname, &hostbuf, tmphstbuf, hstbuflen,
				 &hp, &herr)) == ERANGE) {
      hstbuflen *= 2;
      tmphstbuf = realloc (tmphstbuf, hstbuflen);
    }

    if(res != 0) {
      htsmsg_add_str(m, "error", "internal error");
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
      htsmsg_add_str(m, "error", errtxt);
    } else if(hp == NULL) {
      htsmsg_add_str(m, "error", "internal error");
    } else {
      switch(hp->h_addrtype) {

      case AF_INET:
	htsmsg_add_bin(m, "ipv4", hp->h_addr, 4);
	break;

      default:
	htsmsg_add_str(m, "error", "Unsupported address family");
	break;
      }
    }

    if(htsmsg_binary_serialize(m, &buf, &len, -1) < 0) {
      fprintf(stderr, "Resolver: serialization error\n");
      break;
    }

    write(from_thread_fd[1], buf, len);
free(buf);
  }
  fprintf(stderr, "Internal resolver error\n");
  exit(1);
}


/**
 * Public function for resolving a hostname with a callback and opaque
 */
void *
async_resolve(const char *hostname,
	      void (*cb)(void *opaque, struct sockaddr *so, const char *error),
	      void *opaque)
{
  htsmsg_t *m;
  void *buf;
  size_t len;
  int r;
  res_t *res;

  async_resolver_init();

  res_seq_tally++;

  m = htsmsg_create();
  htsmsg_add_str(m, "hostname", hostname);
  htsmsg_add_u32(m, "seq", res_seq_tally);

  if(htsmsg_binary_serialize(m, &buf, &len, -1) < 0) {
    htsmsg_destroy(m);
    return NULL;
  }

  r = write(to_thread_fd[1], buf, len);

  free(buf);
  htsmsg_destroy(m);

  if(r < 0)
    return NULL;

  res = calloc(1, sizeof(res_t));
  res->cb = cb;
  res->opaque = opaque;
  res->seqno = res_seq_tally;

  LIST_INSERT_HEAD(&pending_resolve, res, link);
  return res;
}

/**
 * Cancel a pending resolve
 */
void
async_resolve_cancel(void *ar)
{
  res_t *res = ar;

  LIST_REMOVE(res, link);
  free(res);
}
