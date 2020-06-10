/*
 *  Buffer management functions
 *  Copyright (C) 2008 Andreas Öman
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tvheadend.h"
#include "htsbuf.h"

/**
 *
 */
void
htsbuf_queue_init(htsbuf_queue_t *hq, unsigned int maxsize)
{
  if(maxsize == 0)
    maxsize = INT32_MAX;
  TAILQ_INIT(&hq->hq_q);
  hq->hq_size = 0;
  hq->hq_maxsize = maxsize;
}


/**
 *
 */
htsbuf_queue_t *
htsbuf_queue_alloc(unsigned int maxsize)
{
  htsbuf_queue_t *hq = malloc(sizeof(htsbuf_queue_t));
  htsbuf_queue_init(hq, maxsize);
  return hq;
}

/**
 *
 */
void
htsbuf_queue_free(htsbuf_queue_t *hq)
{
  htsbuf_queue_flush(hq);
  free(hq);
}

/**
 *
 */
void
htsbuf_data_free(htsbuf_queue_t *hq, htsbuf_data_t *hd)
{
  hq->hq_size -= hd->hd_data_size - hd->hd_data_off;
  TAILQ_REMOVE(&hq->hq_q, hd, hd_link);
  free(hd->hd_data);
  free(hd);
}


/**
 *
 */
void
htsbuf_queue_flush(htsbuf_queue_t *hq)
{
  htsbuf_data_t *hd;

  hq->hq_size = 0;

  while((hd = TAILQ_FIRST(&hq->hq_q)) != NULL)
    htsbuf_data_free(hq, hd);
}

/**
 *
 */
void
htsbuf_append(htsbuf_queue_t *hq, const void *buf, size_t len)
{
  htsbuf_data_t *hd = TAILQ_LAST(&hq->hq_q, htsbuf_data_queue);
  int c;
  hq->hq_size += len;

  if(hd != NULL) {
    /* Fill out any previous buffer */
    c = MIN(hd->hd_data_size - hd->hd_data_len, len);
    memcpy(hd->hd_data + hd->hd_data_len, buf, c);
    hd->hd_data_len += c;
    buf += c;
    len -= c;
  }
  if(len == 0)
    return;
  
  hd = malloc(sizeof(htsbuf_data_t));
  TAILQ_INSERT_TAIL(&hq->hq_q, hd, hd_link);
  
  c = MAX(len, 1000); /* Allocate 1000 bytes to support lots of small writes */

  hd->hd_data = malloc(c);
  hd->hd_data_size = c;
  hd->hd_data_len = len;
  hd->hd_data_off = 0;
  memcpy(hd->hd_data, buf, len);
}


/**
 *
 */
void
htsbuf_append_prealloc(htsbuf_queue_t *hq, const void *buf, size_t len)
{
  htsbuf_data_t *hd;

  hq->hq_size += len;

  hd = malloc(sizeof(htsbuf_data_t));
  TAILQ_INSERT_TAIL(&hq->hq_q, hd, hd_link);
  
  hd->hd_data = (void *)buf;
  hd->hd_data_size = len;
  hd->hd_data_len = len;
  hd->hd_data_off = 0;
}

/**
 *
 */
size_t
htsbuf_read(htsbuf_queue_t *hq, void *buf, size_t len)
{
  size_t r = 0;
  int c;

  htsbuf_data_t *hd;
  
  while(len > 0) {
    hd = TAILQ_FIRST(&hq->hq_q);
    if(hd == NULL)
      break;

    c = MIN(hd->hd_data_len - hd->hd_data_off, len);
    memcpy(buf, hd->hd_data + hd->hd_data_off, c);

    r += c;
    buf += c;
    len -= c;
    hd->hd_data_off += c;
    hq->hq_size -= c;
    if(hd->hd_data_off == hd->hd_data_len)
      htsbuf_data_free(hq, hd);
  }
  return r;
}


/**
 *
 */
size_t
htsbuf_find(htsbuf_queue_t *hq, uint8_t v)
{
  htsbuf_data_t *hd;
  int i, o = 0;

  TAILQ_FOREACH(hd, &hq->hq_q, hd_link) {
    for(i = hd->hd_data_off; i < hd->hd_data_len; i++) {
      if(hd->hd_data[i] == v) 
	return o + i - hd->hd_data_off;
    }
    o += hd->hd_data_len - hd->hd_data_off;
  }
  return -1;
}



/**
 *
 */
size_t
htsbuf_peek(htsbuf_queue_t *hq, void *buf, size_t len)
{
  size_t r = 0;
  int c;

  htsbuf_data_t *hd = TAILQ_FIRST(&hq->hq_q);
  
  while(len > 0 && hd != NULL) {
    c = MIN(hd->hd_data_len - hd->hd_data_off, len);
    memcpy(buf, hd->hd_data + hd->hd_data_off, c);

    r += c;
    buf += c;
    len -= c;

    hd = TAILQ_NEXT(hd, hd_link);
  }
  return r;
}

/**
 *
 */
size_t
htsbuf_drop(htsbuf_queue_t *hq, size_t len)
{
  size_t r = 0;
  int c;
  htsbuf_data_t *hd;
  
  while(len > 0) {
    hd = TAILQ_FIRST(&hq->hq_q);
    if(hd == NULL)
      break;

    c = MIN(hd->hd_data_len - hd->hd_data_off, len);
    len -= c;
    hd->hd_data_off += c;
    hq->hq_size -= c;
    r += c;
    if(hd->hd_data_off == hd->hd_data_len)
      htsbuf_data_free(hq, hd);
  }
  return r;
}

/**
 * Inspired by vsnprintf man page
 */
void
htsbuf_vqprintf(htsbuf_queue_t *hq, const char *fmt, va_list ap0)
{
  // First try to format it on-stack
  va_list ap;
  int n, size;
  char buf[100], *p, *np;

  va_copy(ap, ap0);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if(n > -1 && n < sizeof(buf)) {
    htsbuf_append(hq, buf, n);
    return;
  }

  // Else, do allocations
  size = sizeof(buf) * 2;

  p = malloc(size);
  while (1) {
    /* Try to print in the allocated space. */
    va_copy(ap, ap0);
    n = vsnprintf(p, size, fmt, ap);
    va_end(ap);
    if(n > -1 && n < size) {
      htsbuf_append_prealloc(hq, p, n);
      return;
    }
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
      size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ((np = realloc (p, size)) == NULL) {
      free(p);
      abort();
    } else {
      p = np;
    }
  }
}


/**
 *
 */
void
htsbuf_qprintf(htsbuf_queue_t *hq, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);
}


void
htsbuf_appendq(htsbuf_queue_t *hq, htsbuf_queue_t *src)
{
  htsbuf_data_t *hd;

  hq->hq_size += src->hq_size;
  src->hq_size = 0;

  while((hd = TAILQ_FIRST(&src->hq_q)) != NULL) {
    TAILQ_REMOVE(&src->hq_q, hd, hd_link);
    TAILQ_INSERT_TAIL(&hq->hq_q, hd, hd_link);
  }
}


void
htsbuf_dump_raw_stderr(htsbuf_queue_t *hq)
{
  htsbuf_data_t *hd;
  char n = '\n';

  TAILQ_FOREACH(hd, &hq->hq_q, hd_link) {
    if(write(2, hd->hd_data + hd->hd_data_off,
	     hd->hd_data_len - hd->hd_data_off)
       != hd->hd_data_len - hd->hd_data_off)
      break;
  }
  if(write(2, &n, 1) != 1)
    return;
}


void
htsbuf_hexdump(htsbuf_queue_t *hq, const char *prefix)
{
  void *r = malloc(hq->hq_size);
  htsbuf_peek(hq, r, hq->hq_size);
  hexdump(prefix, r, hq->hq_size);
  free(r);
}


/**
 *
 */
void
htsbuf_append_and_escape_xml(htsbuf_queue_t *hq, const char *s)
{
  const char *c = s;
  const char *e = s + strlen(s);
  const char *esc = 0;
  int h;

  if(e == s)
    return;

  while(c<e) {
    h = *c++;

    if (h & 0x80) {
      // Start of UTF-8.  But we sometimes have bad UTF-8 (#5909).
      // So validate and handle bad characters.

      // Number of top bits set indicates the total number of  bytes.
      const int num_bytes =
        ((h & 240) == 240) ? 4 :
        ((h & 224) == 224) ? 3 :
        ((h & 192) == 192) ? 2 : 0;

      if (!num_bytes) {
        // Completely invalid sequence, so we replace it with a space.
        htsbuf_append(hq, s, c - s - 1);
        htsbuf_append(hq, " ", 1);
        s=c;
        continue;
      } else {
        // Start of valid UTF-8.
        if (e - c < num_bytes - 1) {
          // Invalid sequence - too few characters left in buffer for the sequence.
          // Append what we already have accumulated and ignore remaining characters.
          htsbuf_append(hq, s, c - s - 1);
          break;
        } else {
          // We should probably check each character in the range is also valid.
          htsbuf_append(hq, s, c - s - 1);
          htsbuf_append(hq, c-1, num_bytes);
          s=c-1;
          s+=num_bytes;
          c=s;
          continue;
        }
      }
    }

    switch(h) {
    case '<':  esc = "&lt;";   break;
    case '>':  esc = "&gt;";   break;
    case '&':  esc = "&amp;";  break;
    case '\'': esc = "&apos;"; break;
    case '"':  esc = "&quot;"; break;
    default:   esc = NULL;     break;
    }
    
    if(esc != NULL) {
      htsbuf_append(hq, s, c - s - 1);
      htsbuf_append_str(hq, esc);
      s = c;
    } else if (h < 0x20 && h != 0x09 && h != 0x0a && h != 0x0d) {
      /* allow XML 1.0 valid characters only */
      htsbuf_append(hq, s, c - s - 1);
      s = c;
    }
    
    if(c == e) {
      htsbuf_append(hq, s, c - s);
      break;
    }
  }
}


/**
 *
 */
void
htsbuf_append_and_escape_url(htsbuf_queue_t *hq, const char *s)
{
  const char *c = s;
  const char *e = s + strlen(s);
  char C;
  if(e == s)
    return;

  while(1) {
    const char *esc;
    char buf[4];
    C = *c++;
    
    /* RFC 3986, section 3.4 */
    if((C >= '0' && C <= '9') ||
       (C >= 'a' && C <= 'z') ||
       (C >= 'A' && C <= 'Z') ||
       C == '/'  ||
       C == ':'  ||
       C == '@'  ||
       C == '-'  ||
       C == '.'  ||
       C == '~'  ||
       C == '!'  ||
       C == '$'  ||
       C == '\'' ||
       C == '('  ||
       C == ')'  ||
       C == '*'  ||
       C == ','  ||
       C == ';') {
      esc = NULL;
    } else {
      static const char hexchars[16] = "0123456789ABCDEF";
      buf[0] = '%';
      buf[1] = hexchars[(C >> 4) & 0xf];
      buf[2] = hexchars[C & 0xf];
      buf[3] = 0;
      esc = buf;
    }

    if(esc != NULL) {
      htsbuf_append(hq, s, c - s - 1);
      htsbuf_append_str(hq, esc);
      s = c;
    }
    
    if(c == e) {
      htsbuf_append(hq, s, c - s);
      break;
    }
  }
}

/**
 * RFC8187 (RFC5987) HTTP Header non-ASCII field encoding
 */
void
htsbuf_append_and_escape_rfc8187(htsbuf_queue_t *hq, const char *s)
{
  const char *c = s;
  const char *e = s + strlen(s);
  char C;
  if(e == s)
    return;

  while(1) {
    const char *esc;
    char buf[4];
    C = *c++;

    /* RFC 8187, section 3.2.1, attr-char */
    if((C >= '0' && C <= '9') ||
       (C >= 'a' && C <= 'z') ||
       (C >= 'A' && C <= 'Z') ||
       C == '!'  ||
       C == '#'  ||
       C == '$'  ||
       C == '&'  ||
       C == '+'  ||
       C == '-'  ||
       C == '.'  ||
       C == '^'  ||
       C == '_'  ||
       C == '`'  ||
       C == '|'  ||
       C == '~') {
      esc = NULL;
    } else {
      static const char hexchars[16] = "0123456789ABCDEF";
      buf[0] = '%';
      buf[1] = hexchars[(C >> 4) & 0xf];
      buf[2] = hexchars[C & 0xf];
      buf[3] = 0;
      esc = buf;
    }

    if(esc != NULL) {
      htsbuf_append(hq, s, c - s - 1);
      htsbuf_append_str(hq, esc);
      s = c;
    }

    if(c == e) {
      htsbuf_append(hq, s, c - s);
      break;
    }
  }
}

/**
 *
 */
void
htsbuf_append_and_escape_jsonstr(htsbuf_queue_t *hq, const char *str)
{
  const char *s = str;

  htsbuf_append(hq, "\"", 1);

  while(*s != 0) {
    if(*s == '"' || *s == '\\' || *s == '\n' || *s == '\r' || *s == '\t') {
      htsbuf_append(hq, str, s - str);

      if(*s == '"')
	htsbuf_append(hq, "\\\"", 2);
      else if(*s == '\n') 
	htsbuf_append(hq, "\\n", 2);
      else if(*s == '\r') 
	htsbuf_append(hq, "\\r", 2);
      else if(*s == '\t') 
	htsbuf_append(hq, "\\t", 2);
      else
	htsbuf_append(hq, "\\\\", 2);
      s++;
      str = s;
    } else {
      s++;
    }
  }
  htsbuf_append(hq, str, s - str);
  htsbuf_append(hq, "\"", 1);
}



/**
 *
 */
char *
htsbuf_to_string(htsbuf_queue_t *hq)
{
  char *r = malloc(hq->hq_size + 1);
  r[hq->hq_size] = 0;
  htsbuf_read(hq, r, hq->hq_size);
  return r;
}
