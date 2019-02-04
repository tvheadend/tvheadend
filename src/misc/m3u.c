/*
 *  M3U parser
 *  Copyright (C) 2015 Jaroslav Kysela
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
#include "htsmsg.h"
#include "intlconv.h"
#include "m3u.h"

/*
 *
 */
static char *get_m3u_str(char *data, char **res, int *last)
{
  char *p = data, first = *data;

  if (first == '"' || first == '\'') {
    data++; p++;
    while (*data && *data != first && *data != '\n' && *data != '\r')
      data++;
    if (*data == first) {
      *data = '\0';
      data++;
    }
  }
  while (*data && *data != ',' && *data > ' ')
    data++;
  *last = '\0';
  if (*data) {
    *last = *data;
    *data = '\0';
    data++;
  }
  *res = data;
  return p;
}

/*
 *
 */
static void get_m3u_str_post(char **data, int delim)
{
  if (delim == '\n' || delim == '\r') {
    (*data)--;
    **data = delim;
  }
}

/*
 *
 */
static char *until_eol(char *d)
{
  while (*d && *d != '\r' && *d != '\n') d++;
  if (*d) { *d = '\0'; d++; }
  while (*d && (*d == '\r' || *d == '\n')) d++;
  return d;
}

/*
 *
 */
static int is_full_url(const char *url)
{
  if (strncmp(url, "file://", 7) == 0) return 7;
  if (strncmp(url, "pipe://", 7) == 0) return 7;
  if (strncmp(url, "http://", 7) == 0) return 7;
  if (strncmp(url, "https://", 8) == 0) return 8;
  if (strncmp(url, "rtsp://", 7) == 0) return 7;
  if (strncmp(url, "rtsps://", 8) == 0) return 8;
  if (strncmp(url, "udp://", 6) == 0) return 6;
  if (strncmp(url, "rtp://", 6) == 0) return 6;
  return 0;
}

/*
 *
 */
static const char *get_url
  (char *buf, size_t buflen, const char *rel, const char *url)
{
  char *url2, *p;
  int l;

  if (url == NULL)
    return rel;
  if ((l = is_full_url(url)) == 0 || is_full_url(rel))
    return rel;

  url2 = tvh_strdupa(url);
  if (rel[0] == '/') {
    p = strchr(url2 + l, '/');
    if (p == NULL)
      return rel;
    *p = '\0';
  } else {
    p = strrchr(url2 + l, '/');
    if (p == NULL)
      return rel;
    *(p + 1) = '\0';
  }
  snprintf(buf, buflen, "%s%s", url2, rel);
  return buf;
}

/*
 * Note: text in data pointer is not preserved (must be read/write)
 */
htsmsg_t *parse_m3u
  (char *data, const char *charset, const char *url)
{
  char *p, *x, *y;
  char *charset_id = intlconv_charset_id(charset, 0, 1);
  const char *multi_name;
  int delim;
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *item = NULL, *l = NULL, *t, *key = NULL;
  char buf[512];

  while (*data && *data <= ' ') data++;
  p = data;
  data = until_eol(data);
  if (strncmp(p, "#EXTM3U", 7)) {
    htsmsg_add_msg(m, "items", htsmsg_create_list());
    return m;
  }
  while (*data) {
    if (strncmp(data, "#EXTINF:", 8) == 0) {
      if (item == NULL)
        item = htsmsg_create_map();
      data += 8;
      p = data;
      if (*data == '-') data++;
      while (*data >= '0' && *data <= '9') data++;
      delim = *data;
      *data = '\0';
      htsmsg_add_s64(item, "m3u-duration", strtoll(p, NULL, 10));
      *data = delim;
      while (*data > ' ' && *data != ',') data++;
      while (delim && delim != ',' && delim != '\n' && delim != '\r') {
        while (*data && *data <= ' ') data++;
        if (*data == '\0' || *data == ',') break;
        p = data++;
        while (*data && *data != ',' && *data != '=') data++;
        if (*data == '=') {
          *data = '\0';
          x = get_m3u_str(data + 1, &data, &delim);
          if (*p && *x) {
            y = intlconv_to_utf8safestr(charset_id, x, strlen(x)*2);
            htsmsg_add_str(item, p, y ?: ".invalid.charset.");
            free(y);
          }
          get_m3u_str_post(&data, delim);
        }
      }
      p = NULL;
      if (*data == ',') {
        delim = ',';
        data++;
      }
      if (delim == ',') {
        while (*data && *data <= ' ' && *data != '\n' && *data != '\r') data++;
        if (*data)
          p = data;
      }
      data = until_eol(data);
      if (p && *p) {
        y = intlconv_to_utf8safestr(charset_id, p, strlen(p)*2);
        htsmsg_add_str(item, "m3u-name", y ?: ".invalid.charset.");
        free(y);
      }
      continue;
    } else if (strncmp(data, "#EXT-X-VERSION:", 15) == 0) {
      htsmsg_add_s64(m, "version", strtoll(data + 15, NULL, 10));
      data = until_eol(data + 15);
      continue;
    } else if (strncmp(data, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0) {
      htsmsg_add_s64(m, "media-sequence", strtoll(data + 22, NULL, 10));
      data = until_eol(data + 22);
      continue;
    } else if (strncmp(data, "#EXT-X-TARGETDURATION:", 22) == 0) {
      htsmsg_add_s64(m, "targetduration", strtoll(data + 22, NULL, 10));
      data = until_eol(data + 22);
      continue;
    } else if (strncmp(data, "#EXT-X-STREAM-INF:", 18) == 0) {
      data += 18;
      multi_name = "stream-inf";
multi:
      t = htsmsg_create_map();
      delim = 0;
      while (*data && delim != '\n' && delim != '\r') {
        while (*data && *data <= ' ') data++;
        p = data;
        while (*data && *data >= ' ' && *data != '=') data++;
        if (*data == '=') {
          *data = '\0';
          x = get_m3u_str(data + 1, &data, &delim);
          if (*x && *p)
            htsmsg_add_str(t, p, x);
          get_m3u_str_post(&data, delim);
        } else {
          while (*data && *data <= ' ' && *data != '\n' && *data != '\r') data++;
          if (*data != ',') break;
        }
      }
      if (strcmp(multi_name, "x-key") == 0) {
        htsmsg_destroy(key);
        key = t;
      } else {
        if (item == NULL)
          item = htsmsg_create_map();
        htsmsg_add_msg(item, multi_name, t);
      }
      data = until_eol(data);
      continue;
    } else if (strncmp(data, "#EXT-X-MEDIA:", 13) == 0) {
      data += 13;
      multi_name = "x-media";
      goto multi;
    } else if (strncmp(data, "#EXT-X-KEY:", 11) == 0) {
      data += 11;
      multi_name = "x-key";
      goto multi;
    } else if (strncmp(data, "#EXT-X-ENDLIST", 14) == 0) {
      htsmsg_add_bool(m, "x-endlist", 1);
      data = until_eol(data + 14);
      continue;
    } else if (strncmp(data, "#EXTVLCOPT:program=", 19) == 0) {
      if (item == NULL)
        item = htsmsg_create_map();
      htsmsg_add_s64(item, "vlc-program", strtoll(data + 19, NULL, 10));
      data = until_eol(data + 19);
    } else if (strncmp(data, "#EXT", 4) == 0) {
      data = until_eol(data + 4);
      continue;
    }
    while (*data && *data <= ' ') data++;
    p = data;
    data = until_eol(data);
    if (*p == '#')
      continue;
    if (*p && *p > ' ') {
      if (item == NULL)
        item = htsmsg_create_map();
      if (strncmp(p, "http://", 7) == 0 ||
          strncmp(p, "https://", 8) == 0) {
        delim = 0;
        x = p;
        while (*x && *x != ' ' && *x != '|') x++;
        if (*x) { delim = *x; *x = '\0'; x++; }
        t = NULL;
        while (*x) {
          while (*x && *x <= ' ') x++;
          y = x;
          while (*x && *x != delim && *x != '&') x++;
          if (*x) { *x = '\0'; x++; }
          if (*y) {
            if (t == NULL)
              t = htsmsg_create_list();
            htsmsg_add_str(t, NULL, y);
          }
        }
        if (t)
          htsmsg_add_msg(item, "m3u-http-headers", t);
      }

      htsmsg_add_str(item, "m3u-url", get_url(buf, sizeof(buf), p, url));
    } else if (item) {
      htsmsg_destroy(item);
      item = NULL;
    }

    if (item) {
      if (l == NULL)
        l = htsmsg_create_list();
      if (key)
        htsmsg_add_msg(item, "x-key", htsmsg_copy(key));
      htsmsg_add_msg(l, NULL, item);
      item = NULL;
    }
  }

  htsmsg_destroy(key);
  if (l == NULL)
    l = htsmsg_create_list();
  htsmsg_add_msg(m, "items", l);
  return m;
}
