/*
 *  Icon file server operations
 *  Copyright (C) 2012 Andy Brown
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

#define CURL_STATICLIB
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>

#include "settings.h"
#include "tvheadend.h"
#include "channels.h"
#include "http.h"
#include "webui/webui.h"
#include "filebundle.h"
#include "iconserve.h"
#include "config2.h"

size_t
write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

/**
 * https://github.com/andyb2000 Function to provide local icon files
 */
int
page_logo(http_connection_t *hc, const char *remain, void *opaque)
{
  const char *homedir = hts_settings_get_root();
  channel_t *ch = NULL;
  char *inpath, *inpath2;
  const char *outpath = "none";
  char homepath[254];
  char iconpath[100];
  pthread_mutex_lock(&global_lock);
  fb_file *fp;
  ssize_t size;
  char buf[4096];

  if(remain == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  };

  if(strstr(remain, "..")) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  };

/* we get passed the channel number now, so calc the logo from that */
  ch = channel_find_by_identifier(atoi(remain));
  if (ch == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  };

  snprintf(homepath, sizeof(homepath), "%s/icons", homedir);
  inpath = NULL;
  inpath2 = NULL;
  outpath = NULL;
  /* split icon to last component */
  inpath = strdup(ch->ch_icon);
  inpath2 = strtok(inpath, "/");
  while (inpath2 != NULL) {
    inpath2 = strtok(NULL, "/");
    if (inpath2 != NULL) {
      outpath = strdup(inpath2);
    };
  };
  snprintf(iconpath, sizeof(iconpath), "%s/%s", homepath, outpath);
  fp = fb_open(iconpath, 1, 0);
  if (!fp) {
    tvhlog(LOG_DEBUG, "page_logo", "failed to open %s redirecting to http link for icon (%s)", iconpath, ch->ch_icon);
    http_redirect(hc, ch->ch_icon);
  } else {
    tvhlog(LOG_DEBUG, "page_logo", "File %s opened", iconpath);
    size = fb_size(fp);
/*    http_send_header(hc, 200, "gzip", size, NULL, NULL, 300, 0, NULL);*/
    http_send_header(hc, 200, NULL, size, NULL, NULL, 300, 0, NULL);
    while (!fb_eof(fp)) {
      ssize_t c = fb_read(fp, buf, sizeof(buf));
      if (c < 0) {
        break;
      };
      if (write(hc->hc_fd, buf, c) != c) {
        break;
      };
    };
    fb_close(fp);
  };

  pthread_mutex_unlock(&global_lock);
  return 0;
}

/*
*  Logo loader functions, called from http htsp
*   Will return local cache url instead of icon stored
*/
const char
*logo_query(const char *ch_icon)
{
  const char *setting = config_get_iconserve();
  const char *serverip = config_get_serverip();
  char *inpath, *inpath2, *outpath;
  char outiconpath[255];
  char *return_icon = NULL;

  if (!setting || !*setting) {
    tvhlog(LOG_DEBUG, "logo_query", "logo_query - Disabled by config, skipping");
    return ch_icon;
  };
  if (!serverip || !*serverip) {
    tvhlog(LOG_DEBUG, "logo_query", "No server IP defined in config, skipping icon cache");
    return ch_icon;
  };

  inpath = strdup(ch_icon);
  inpath2 = strtok(inpath, "/");
  while (inpath2 != NULL) {
    inpath2 = strtok(NULL, "/");
    if (inpath2 != NULL)
      outpath = strdup(inpath2);
  };
  snprintf(outiconpath, sizeof(outiconpath), "http://%s:%d/logo/%s", serverip, webui_port, outpath);
  return_icon = strdup(outiconpath);
return return_icon;
};

/**
 * Loader for icons, check config params and pull them in one go
 * https://github.com/andyb2000
 */
void
logo_loader(void)
{
  channel_t *ch;
  char *inpath, *inpath2;
  const char *outpath = "none";
  const char *homedir = hts_settings_get_root();
  char homepath[254];
  char iconpath[100];
  fb_file *fp;
  CURL *curl;
  FILE *curl_fp;
  CURLcode res;
  const char *setting = config_get_iconserve();
  const char *serverip = config_get_serverip();
  struct stat fileStat;

  if (!setting || !*setting || (strcmp(setting, "off") == 0)) {
    tvhlog(LOG_DEBUG, "logo_loader", "Disabled by config, skipping");
    return;
  };

  if (!serverip || !*serverip) {
    tvhlog(LOG_ALERT, "logo_loader", "No server IP defined in config, skipping icon cache");
    return;
  };

  tvhlog(LOG_INFO, "logo_loader", "Caching logos locally");
  snprintf(homepath, sizeof(homepath), "%s/icons", homedir);
  if(stat(homepath, &fileStat) == 0 || mkdir(homepath, 0700) == 0) {
  curl = curl_easy_init();
  if (curl) {
  /* loop through channels and load logo files */
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    if (ch->ch_icon != NULL) {
      tvhlog(LOG_DEBUG, "logo_loader", "Loading channel icon %s", ch->ch_icon);
      inpath = NULL;
      inpath2 = NULL;
      outpath = NULL;
      curl_fp = NULL;
      /* split icon to last component */
      inpath = strdup(ch->ch_icon);
      inpath2 = strtok(inpath, "/");
      while (inpath2 != NULL) {
        inpath2 = strtok(NULL, "/");
        if (inpath2 != NULL)
          outpath = strdup(inpath2);
      };
      if (outpath != NULL) {
        snprintf(iconpath, sizeof(iconpath), "%s/%s", homepath, outpath);
        fp = fb_open(iconpath, 0, 1);
        if (!fp) {
          curl_fp = fopen(iconpath,"wb");
          curl_easy_setopt(curl, CURLOPT_URL, ch->ch_icon);
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_fp);
          res = curl_easy_perform(curl);
          if (res == 0) {
            tvhlog(LOG_INFO, "logo_loader", "Downloaded icon via curl");
          } else {
            tvhlog(LOG_WARNING, "logo_loader", "Error with icon download via curl");
          };
          fclose(curl_fp);
        } else {
          fb_close(fp);
        };
      };
    };
  };
  curl_easy_cleanup(curl);
  } else {
    tvhlog(LOG_WARNING, "iconserve", "CURL cannot initialise, aborting icon loader");
  };
  };
};
