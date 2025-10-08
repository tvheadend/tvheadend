/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * Download a file from storage or network
 */

#ifndef __DOWNLOAD__
#define __DOWNLOAD__

#include "http.h"
#include "sbuf.h"

typedef struct download {
  int   subsys;
  char *url;
  void *aux;
  int   ssl_peer_verify;
  int (*process)(void *aux, const char *last_url, const char *host_url,
                 char *data, size_t len);
  void (*stop)(void *aux);
  /* internal members */
  http_client_t *http_client;
  mtimer_t       fetch_timer;
  mtimer_t       pipe_read_timer;
  sbuf_t         pipe_sbuf;
  int            pipe_fd;
  pid_t          pipe_pid;
} download_t;

void download_init ( download_t *dn, int subsys );
void download_start( download_t *dn, const char *url, void *aux );
void download_done ( download_t *dn );

#endif /* __DOWNLOAD__ */
