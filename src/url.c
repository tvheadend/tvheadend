/*
 *  Tvheadend - URL Processing
 *
 *  Copyright (C) 2013 Adam Sutton
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

#include "tvheadend.h"
#include "url.h"

#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>


void
urlreset ( url_t *url )
{
  free(url->scheme);
  free(url->user);
  free(url->pass);
  free(url->host);
  free(url->path);
  free(url->query);
  free(url->frag);
  free(url->raw);
  memset(url, 0, sizeof(*url));
}

void
urlcopy ( url_t *dst, const url_t *src )
{
  dst->scheme = src->scheme ? strdup(src->scheme) : NULL;
  dst->user   = src->user ? strdup(src->user) : NULL;
  dst->pass   = src->pass ? strdup(src->pass) : NULL;
  dst->host   = src->host ? strdup(src->host) : NULL;
  dst->port   = src->port;
  dst->path   = src->path ? strdup(src->path) : NULL;
  dst->query  = src->query ? strdup(src->query) : NULL;
  dst->frag   = src->frag ? strdup(src->frag) : NULL;
  dst->raw    = src->raw ? strdup(src->raw) : NULL;
}

int
urlrecompose( url_t *url )
{
  size_t len;
  char *raw, port[16];
  const int user = url->user && url->user[0];
  const int pass = url->pass && url->pass[0];

  len = (url->scheme ? strlen(url->scheme) : 0) + 4 +
        (user ? strlen(url->user) + 1 : 0) +
        (pass ? strlen(url->pass) + 1 : 0) +
        (url->host ? strlen(url->host) : 0) +
        (url->port > 0 ? 6 : 0) +
        (url->path ? strlen(url->path) : 0) +
        (url->query ? strlen(url->query) + 1 : 0);
  raw = malloc(len);
  if (raw == NULL)
    return -ENOMEM;
  if (url->port > 0 && url->port <= 65535)
    snprintf(port, sizeof(port), ":%d", url->port);
  else
    port[0] = '\0';
  snprintf(raw, len, "%s%s%s%s%s%s%s%s%s%s%s",
           url->scheme ?: "", url->scheme ? "://" : "",
           user ? url->user : "",
           (user && pass) ? ":" : "",
           (user && pass) ? url->pass : "",
           user ? "@" : "",
           url->host ?: "", port,
           url->path ?: "",
           (url->query && url->query[0]) ? "?" : "",
           url->query ?: "");
  free(url->raw);
  url->raw = raw;
  return 0;
}

/* Use liburiparser if available */
#if ENABLE_URIPARSER
#include <uriparser/Uri.h>

int
urlparse ( const char *str, url_t *url )
{
  UriParserStateA state;
  UriPathSegmentA *path;
  UriUriA uri;
  char *s, buf[256];

  if (str == NULL || url == NULL)
    return -1;

  urlreset(url);

  /* Parse */
  state.uri = &uri;
  if (uriParseUriA(&state, str) != URI_SUCCESS) {
    uriFreeUriMembersA(&uri);
    return -1;
  }
  
  /* Store raw */
  url->raw = strdup(str);

  /* Copy */
#define uri_copy(y, x)\
  if (x.first) {\
    size_t len = x.afterLast - x.first;\
    y = strndup(x.first, len);\
  }
#define uri_copy_static(y, s, x)\
  if (x.first) {\
    size_t len = x.afterLast - x.first;\
    if (len > sizeof(y) - 1) \
    { y[0] = '\0'; s = strndup(x.first, len); } else \
    { s = NULL; strlcpy(y, x.first, len + 1); }\
  } else {\
    s = NULL;\
    y[0] = '\0';\
  }
  uri_copy(url->scheme, uri.scheme);
  uri_copy(url->host,   uri.hostText);
  uri_copy(url->user,   uri.userInfo);
  uri_copy(url->query,  uri.query);
  uri_copy(url->frag,   uri.fragment);
  uri_copy_static(buf, s, uri.portText);
  if (s) {
    url->port = atoi(s);
    free(s);
  } else if (*buf)
    url->port = atoi(buf);
  else
    url->port = 0;
  path       = uri.pathHead;
  while (path) {
    uri_copy_static(buf, s, path->text);
    if (url->path)
      url->path = realloc(url->path, strlen(url->path) + strlen(s ?: buf) + 2);
    else
      url->path = calloc(1, strlen(s ?: buf) + 2);
    strcat(url->path, "/");
    strcat(url->path, s ?: buf);
    path = path->next;
    free(s);
  }
  // TODO: query/fragment

  /* Split user/pass */
  if (url->user) {
    s = strstr(url->user, ":");
    if (s) {
      url->pass = strdup(s + 1);
      *s = 0;
    }
  }

  /* Cleanup */
  uriFreeUriMembersA(&uri);
  return 0;
}

void
urlparse_done( void )
{
}

/* Fallback to limited support */
#else /* ENABLE_URIPARSER */

/* URL regexp - I probably found this online */
// TODO: does not support ipv6
#define UC "[a-z0-9_\\.!£$%^&-]"
#define PC UC
#define HC "[a-z0-9_\\.-]"
#define URL_RE "^([A-Za-z]+)://(("UC"+)(:("PC"+))?@|@)?("HC"+)(:([0-9]+))?(/[^\\?]*)?(.([^#]*))?(#(.*))?"

static regex_t *urlparse_exp = NULL;

int
urlparse ( const char *str, url_t *url )
{
  regmatch_t m[16];
  char buf[16];

  if (str == NULL || url == NULL)
    return -1;

  urlreset(url);

  /* Create regexp */
  if (!urlparse_exp) {
    urlparse_exp = calloc(1, sizeof(regex_t));
    if (regcomp(urlparse_exp, URL_RE, REG_ICASE | REG_EXTENDED)) {
      tvherror(LS_URL, "failed to compile regexp");
      exit(1);
    }
  }

  /* Execute */
  if (regexec(urlparse_exp, str, ARRAY_SIZE(m), m, 0))
    return -1;
    
  /* Extract data */
#define copy(x, i)\
  {\
    x = strndup(str+m[i].rm_so, m[i].rm_eo - m[i].rm_so);\
  }(void)0
#define copy_static(x, i)\
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
  copy_static(buf,  8);
  url->port = atoi(buf);
  copy(url->query, 11);
  copy(url->frag,  13);

  url->raw = strdup(str);

  return 0;
}

void
urlparse_done( void )
{
  if (urlparse_exp) {
    regfree(urlparse_exp);
    free(urlparse_exp);
  }
}

#endif /* ENABLE_URIPARSER */
