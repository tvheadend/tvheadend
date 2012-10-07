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

#define _GNU_SOURCE /* for splice() */
#include <fcntl.h>

#include <stdio.h>
#include <string.h>

#include "settings.h"
#include "tvheadend.h"
#include "channels.h"
#include "http.h"
#include "webui/webui.h"
#include "filebundle.h"
#include "iconserve.h"

/**
 * https://github.com/andyb2000 Function to provide local icon files
 */
int
page_logo(http_connection_t *hc, const char *remain, void *opaque)
{
  const char *homedir = hts_settings_get_root();
  char homepath[254];
  char iconpath[100];
  char iconstore[100];
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
  snprintf(iconstore, sizeof(iconstore), "/iconstorage/%s", remain);
  fp = fb_open(iconpath, 0, 1);
  if (!fp) {
    tvhlog(LOG_DEBUG, "page_logo", "failed to open %s", iconpath);
    /* Here we check if we should grab the file, if not just redirect
      to the location in the channel file */
    http_redirect(hc, "http://www.google.co.uk/");
  } else {
    tvhlog(LOG_DEBUG, "page_logo", "File %s opened", iconpath);
    fb_close(fp);
    http_redirect(hc, iconstore);
  };

  pthread_mutex_unlock(&global_lock);
  return 0;
}

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

  snprintf(homepath, sizeof(homepath), "%s/icons", homedir);
  /* loop through channels and load logo files */
  RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
    tvhlog(LOG_DEBUG, "logo_loader", "Loading channel icon %s", ch->ch_icon);
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
        tvhlog(LOG_DEBUG, "logo_loader", "Path check %s", outpath);
        snprintf(iconpath, sizeof(iconpath), "%s/%s", homepath, outpath);
        tvhlog(LOG_DEBUG, "logo_loader", "Filename %s", iconpath);
        fp = fb_open(iconpath, 0, 1);
        if (!fp) {
          tvhlog(LOG_DEBUG, "page_loader", "failed to open %s", iconpath);
        } else {
          tvhlog(LOG_DEBUG, "page_loader", "File %s opened", iconpath);
          fb_close(fp);
        };
      };
    };
  };
};
