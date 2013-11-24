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

  /* Parse */
  state.uri = &uri;
  if (uriParseUriA(&state, str) != URI_SUCCESS) {
    uriFreeUriMembersA(&uri);
    return -1;
  }
  
  /* Store raw */
  strncpy(url->raw, str, sizeof(url->raw));

  /* Copy */
#define uri_copy(y, x)\
  if (x.first) {\
    size_t len = x.afterLast - x.first;\
    strncpy(y, x.first, len);\
    y[len] = 0;\
  } else {\
    *y = 0;\
  }
  uri_copy(url->scheme, uri.scheme);
  uri_copy(url->host,   uri.hostText);
  uri_copy(url->user,   uri.userInfo);
  uri_copy(url->query,  uri.query);
  uri_copy(url->frag,   uri.fragment);
  uri_copy(buf,         uri.portText);
  if (*buf)
    url->port = atoi(buf);
  else
    url->port = 0;
  *url->path = 0;
  path       = uri.pathHead;
  while (path) {
    strcat(url->path, "/");
    uri_copy(buf, path->text);
    strcat(url->path, buf);
    path = path->next;
  }
  // TODO: query/fragment

  /* Split user/pass */
  s = strstr(url->user, ":");
  if (s) {
    strcpy(url->pass, s+1);
    *s = 0;
  } else {
    *url->pass = 0;
  }

  /* Cleanup */
  uriFreeUriMembersA(&uri);
  return 0;
}

/* Fallback to limited support */
#else /* ENABLE_URIPARSER */

/* URL regexp - I probably found this online */
// TODO: does not support ipv6
#define UC "[a-z0-9_\\-\\.!£$%^&]"
#define PC UC
#define HC "[a-z0-9\\-\\.]"
#define URL_RE "^(\\w+)://(("UC"+)(:("PC"+))?@)?("HC"+)(:([0-9]+))?(/.*)?"


int
urlparse ( const char *str, url_t *url )
{
  static regex_t *exp = NULL;
  regmatch_t m[12];
  char buf[16];

  /* Create regexp */
  if (!exp) {
    exp = calloc(1, sizeof(regex_t));
    if (regcomp(exp, URL_RE, REG_ICASE | REG_EXTENDED)) {
      tvherror("iptv", "failed to compile regexp");
      exit(1);
    }
  }

  /* Execute */
  if (regexec(exp, str, 12, m, 0))
    return 1;
    
  /* Extract data */
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
  *url->query = 0;
  *url->frag  = 0;

  strncpy(url->raw, str, sizeof(url->raw));

  return 0;
}

#endif /* ENABLE_URIPARSER */
