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
  channel_t *ch;
  char *inpath, *inpath2;
  const char *outpath = "none";
  char homepath[254];
  char iconpath[100];
  pthread_mutex_lock(&global_lock);
  fb_file *fp;

  if(remain == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

/* Check if we already have downloaded the logo to user folders
   if we have, serve that. If we haven't, check if we should,
   download it and serve that, otherwise serve the url direct */
  snprintf(homepath, sizeof(homepath), "%s/icons", homedir);
  snprintf(iconpath, sizeof(iconpath), "%s/%s", homepath, remain);
  fp = fb_open(iconpath, 0, 1);
  if (!fp) {
    tvhlog(LOG_DEBUG, "page_logo", "failed to open %s", iconpath);
    /* Here we check if we should grab the file, if not just redirect
      to the location in the channel file */
    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      if (ch->ch_icon != NULL) {
        inpath = NULL;
        inpath2 = NULL;
        outpath = NULL;
        /* split icon to last component */
        inpath = strdup(ch->ch_icon);
        inpath2 = strtok(inpath, "/");
        while (inpath2 != NULL) {
          inpath2 = strtok(NULL, "/");
          if (inpath2 != NULL)
            outpath = strdup(inpath2);
        };
        if (outpath != NULL) {
          /* found just the icon, compare with what user requested */
          if(strcmp(outpath, remain) == 0) {
            tvhlog(LOG_DEBUG, "page_logo", "Channel with logo located %s",ch->ch_name);
            http_redirect(hc, ch->ch_icon);
            pthread_mutex_unlock(&global_lock);
            return 0;
          };
        };
      };
    };
    http_redirect(hc, "/static/icons/exclamation.png");
  } else {
    tvhlog(LOG_DEBUG, "page_logo", "File %s opened", iconpath);
    fb_close(fp);
    page_static_file(hc, remain, homepath);
    pthread_mutex_unlock(&global_lock);
    return 0;
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
