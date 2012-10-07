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
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "settings.h"
#include "tvheadend.h"
#include "channels.h"
#include "http.h"
#include "webui/webui.h"
#include "filebundle.h"
#include "iconserve.h"
#include "config2.h"
#include "queue.h"
#include "spawn.h"

/* Queue, Cond to signal data and Mutex to protect it */
static TAILQ_HEAD(,iconserve_grab_queue) iconserve_queue;
static pthread_mutex_t                   iconserve_mutex;
static pthread_cond_t                    iconserve_cond;

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
  char       *mimetest_outbuf;

  if(remain == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  };

  if(strstr(remain, "..")) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  };

  ch = channel_find_by_identifier(atoi(remain));
  if (ch == NULL || ch->ch_icon == NULL) {
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
    tvhlog(LOG_DEBUG, "page_logo", 
           "failed to open %s redirecting to http link for icon (%s)", 
           iconpath, ch->ch_icon);
    http_redirect(hc, ch->ch_icon);
    iconserve_queue_add ( ch->ch_id, ch->ch_icon );
  } else {
    tvhlog(LOG_DEBUG, "page_logo", "File %s opened", iconpath);
    size = fb_size(fp);
    mimetest_outbuf = strdup("image/jpeg");
    http_send_header(hc, 200, mimetest_outbuf, size, NULL, NULL, 300, 0, NULL);
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
*logo_query(int ch_id, const char *ch_icon)
{
  const char *setting = config_get_iconserve();
  const char *serverip = config_get_serverip();
  char outiconpath[255];
  char *return_icon = strdup(ch_icon);

  if (!setting || !*setting || (strcmp(setting, "off") == 0)) {
    return return_icon;
  };

  if (!serverip || !*serverip) {
    return return_icon;
  };

  snprintf(outiconpath, sizeof(outiconpath), 
    "http://%s:%d/channellogo/%d", serverip, webui_port, ch_id);
  return_icon = strdup(outiconpath);
return return_icon;
};

/*
 * Icon grabber queue thread
 */
void *iconserve_thread ( void *aux )
{
  iconserve_grab_queue_t *qe;
  pthread_mutex_lock(&iconserve_mutex);
  char *inpath, *inpath2;
  const char *header_parse = NULL, *header_maxage = NULL;
  const char *outpath = "none";
  CURL *curl;
  FILE *curl_fp, *curl_fp_header;
  CURLcode res;
  fb_file *fp;
  char iconpath[100], iconpath_header[100];
  char homepath[254];
  const char *homedir = hts_settings_get_root();
  struct stat fileStat;
  int trigger_download = 0;
  char buf[256];
  int file = 0;
  time_t seconds;
  int dif, compare_seconds, rc;
  const char *periodicdownload = config_get_iconserve_periodicdownload();
  struct timespec timertrigger;
  channel_t *ch;

  tvhlog(LOG_INFO, "iconserve_thread", "Thread startup");
  curl = curl_easy_init();
  snprintf(homepath, sizeof(homepath), "%s/icons", homedir);
  if(stat(homepath, &fileStat) == 0 || mkdir(homepath, 0700) == 0) {
  if (curl) {
  while (1) {

    /* Get entry from queue */
    qe = TAILQ_FIRST(&iconserve_queue);
    /* Check for queue data */
    if (!qe) { /* Queue Empty */
      periodicdownload = config_get_iconserve_periodicdownload();
      if (!periodicdownload || !*periodicdownload || 
         (strcmp(periodicdownload, "off") == 0)) {
        tvhlog(LOG_DEBUG, "iconserve_thread", "Non-timer wakeup");
        rc = pthread_cond_wait(&iconserve_cond, &iconserve_mutex);
      } else {
        tvhlog(LOG_DEBUG, "iconserve_thread", "Timer wakeup set");
        timertrigger.tv_sec  = time(NULL) + 86400;
        timertrigger.tv_nsec = 0;
        rc = pthread_cond_timedwait(&iconserve_cond, 
                             &iconserve_mutex, &timertrigger);
      };
      if (rc == ETIMEDOUT) {
        tvhlog(LOG_INFO, "iconserve_thread", "Thread wakeup by timer");
        RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
          if (ch->ch_icon != NULL) {
            iconserve_grab_queue_t *qe = calloc(1, sizeof(iconserve_grab_queue_t));
            qe->chan_number = ch->ch_id;
            qe->icon_url = ch->ch_icon;
            TAILQ_INSERT_TAIL(&iconserve_queue, qe, iconserve_link);
          };
        };
      };
      continue;
    }
    TAILQ_REMOVE(&iconserve_queue, qe, iconserve_link);
    pthread_mutex_unlock(&iconserve_mutex);

    inpath = NULL;
    inpath2 = NULL;
    outpath = NULL;
    curl_fp = NULL;
    /* split icon to last component */
    inpath = strdup(qe->icon_url);
    inpath2 = strtok(inpath, "/");
    while (inpath2 != NULL) {
      inpath2 = strtok(NULL, "/");
      if (inpath2 != NULL)
        outpath = strdup(inpath2);
    };
    if (outpath != NULL) {
      snprintf(iconpath, sizeof(iconpath), "%s/%s", homepath, outpath);
      snprintf(iconpath_header, sizeof(iconpath_header), "%s/%s.head", 
               homepath, outpath);
      fp = fb_open(iconpath, 0, 1);
      if (!fp) {
        /* No file exists so grab immediately */
        tvhlog(LOG_INFO, "logo_loader", "No logo, downloading file %s", outpath);
        trigger_download = 1;
      } else {
        /* File exists so compare expiry times to re-grab */
        fb_close(fp);
        fp = fb_open(iconpath_header, 0, 0);
        while (!fb_eof(fp)) {
          memset(buf, 0, sizeof(buf));
          if (!fb_gets(fp, buf, sizeof(buf) - 1)) break;
          if (buf[strlen(buf) - 1] == '\n') {
            buf[strlen(buf) - 1] = '\0';
          };
          if(strstr(buf, "Cache-Control: ")) {
            header_parse = strtok(buf, "=");
            header_parse = strtok ( NULL, "=");
            header_maxage = strdup(header_parse);
          };
        };
        fb_close(fp);
        file=open(iconpath, O_RDONLY);
        fstat(file,&fileStat);
        seconds = time (NULL);
        dif = difftime (seconds,fileStat.st_mtime);
        compare_seconds=atoi(header_maxage);
        if (dif > compare_seconds) {
          tvhlog(LOG_DEBUG, "logo_loader", "Logo expired, downloading %s", outpath);
          trigger_download = 1;
        } else {
          tvhlog(LOG_INFO, "logo_loader", "Logo not expired %s", outpath);
        };
        close(file);
      };
      if (trigger_download == 1) {
        curl_fp=fopen(iconpath,"wb");
        curl_fp_header=fopen(iconpath_header,"w");
        curl_easy_setopt(curl, CURLOPT_URL, qe->icon_url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_fp);
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, curl_fp_header);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "TVHeadend");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        res = curl_easy_perform(curl);
        if (res == 0) {
          tvhlog(LOG_INFO, "logo_loader", "Downloaded icon via curl (%s)", 
                 qe->icon_url);
        } else {
          tvhlog(LOG_WARNING, "logo_loader", "Error with curl download (%s)",
                 qe->icon_url);
        };
        fclose(curl_fp);
        fclose(curl_fp_header);
        trigger_download = 0;
      };
    };
  }; /* while loop */
  curl_easy_cleanup(curl);
  } else {
    tvhlog(LOG_WARNING, "iconserve", "CURL cannot initialise");
  };
  };
  return NULL;
};

/*
 * Add data to the queue
 */
void iconserve_queue_add ( int chan_number, char *icon_url )
{
  /* Create entry */
  tvhlog(LOG_DEBUG, "iconserve_queue_add", "Adding chan_number to queue: %i",
         chan_number);
  iconserve_grab_queue_t *qe = calloc(1, sizeof(iconserve_grab_queue_t));
  qe->chan_number = chan_number;
  qe->icon_url = strdup(icon_url);

  pthread_mutex_lock(&iconserve_mutex);
  TAILQ_INSERT_TAIL(&iconserve_queue, qe, iconserve_link);
  pthread_cond_signal(&iconserve_cond);
  pthread_mutex_unlock(&iconserve_mutex);
}


/**
 * Loader for icons, check config params and pull them in one go
 */
void
logo_loader(void)
{
  channel_t *ch;
  const char *setting = config_get_iconserve();
  const char *serverip = config_get_serverip();

  if (!setting || !*setting || (strcmp(setting, "off") == 0)) {
    tvhlog(LOG_DEBUG, "logo_loader", "Disabled by config, skipping");
    return;
  };

  if (!serverip || !*serverip) {
    tvhlog(LOG_ALERT, "logo_loader", "No server IP, skipping icon cache");
    return;
  };


  pthread_t tid;
  pthread_mutex_init(&iconserve_mutex, NULL);
  pthread_cond_init(&iconserve_cond, NULL);
  TAILQ_INIT(&iconserve_queue);
  /* Start thread - presumably permanently active */
  pthread_create(&tid, NULL, iconserve_thread, NULL); // last param is passed as aux
                                              // as this is single global 
                                              // you can probably use global
                                              // vars

  tvhlog(LOG_INFO, "logo_loader", "Caching logos locally");
  /* loop through channels and load logo files */
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    if (ch->ch_icon != NULL) {
      iconserve_queue_add ( ch->ch_id, ch->ch_icon );
    };
  };
  pthread_mutex_lock(&iconserve_mutex);
  pthread_cond_signal(&iconserve_cond); // tell thread data is available
  pthread_mutex_unlock(&iconserve_mutex);
};
