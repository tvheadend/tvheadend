/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2008 Andreas Öman
 *
 * Buffer management functions
 */

#ifndef HTSBUF_H__
#define HTSBUF_H__

#include <stdarg.h>
#include <inttypes.h>

#include "queue.h"


TAILQ_HEAD(htsbuf_data_queue, htsbuf_data);

typedef struct htsbuf_data {
  TAILQ_ENTRY(htsbuf_data) hd_link;
  uint8_t *hd_data;
  unsigned int hd_data_size; /* Size of allocation hb_data */
  unsigned int hd_data_len;  /* Number of valid bytes from hd_data */
  unsigned int hd_data_off;  /* Offset in data, used for partial writes */

} htsbuf_data_t;

typedef struct htsbuf_queue {
  struct htsbuf_data_queue hq_q;
  unsigned int hq_size;
  unsigned int hq_maxsize;
} htsbuf_queue_t;  

void htsbuf_queue_init(htsbuf_queue_t *hq, unsigned int maxsize);

htsbuf_queue_t *htsbuf_queue_alloc(unsigned int maxsize);

void htsbuf_queue_free(htsbuf_queue_t *hq);

void htsbuf_queue_flush(htsbuf_queue_t *hq);

void htsbuf_vqprintf(htsbuf_queue_t *hq, const char *fmt, va_list ap);

void htsbuf_qprintf(htsbuf_queue_t *hq, const char *fmt, ...)
  __attribute__((format(printf,2,3)));

void htsbuf_append(htsbuf_queue_t *hq, const void *buf, size_t len);

void htsbuf_append_prealloc(htsbuf_queue_t *hq, const void *buf, size_t len);

static inline void htsbuf_append_str(htsbuf_queue_t *hq, const char *str)
  { htsbuf_append(hq, str, strlen(str)); }

void htsbuf_data_free(htsbuf_queue_t *hq, htsbuf_data_t *hd);

static inline int htsbuf_empty(htsbuf_queue_t *hq) { return hq->hq_size == 0; }

size_t htsbuf_read(htsbuf_queue_t *hq, void *buf, size_t len);

size_t htsbuf_peek(htsbuf_queue_t *hq, void *buf, size_t len);

size_t htsbuf_drop(htsbuf_queue_t *hq, size_t len);

size_t htsbuf_find(htsbuf_queue_t *hq, uint8_t v);

void htsbuf_appendq(htsbuf_queue_t *hq, htsbuf_queue_t *src);

void htsbuf_append_and_escape_xml(htsbuf_queue_t *hq, const char *str);

void htsbuf_append_and_escape_url(htsbuf_queue_t *hq, const char *s);

void htsbuf_append_and_escape_rfc8187(htsbuf_queue_t *hq, const char *s);

void htsbuf_append_and_escape_jsonstr(htsbuf_queue_t *hq, const char *s);

void htsbuf_dump_raw_stderr(htsbuf_queue_t *hq);

char *htsbuf_to_string(htsbuf_queue_t *hq);

struct rstr;
struct rstr *htsbuf_to_rstr(htsbuf_queue_t *hq, const char *prefix);

void htsbuf_hexdump(htsbuf_queue_t *hq, const char *prefix);

#endif /* HTSBUF_H__ */
