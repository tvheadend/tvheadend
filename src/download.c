/*
 *  Download a file from storage or network
 *
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
#include "download.h"

#include <fcntl.h>
#include <sys/stat.h>

/*
 *
 */
static int
download_file(download_t *dn, const char *filename)
{
  int fd, res;
  struct stat st;
  char *data, *last_url;
  ssize_t r;
  off_t off;

  fd = tvh_open(filename, O_RDONLY, 0);
  if (fd < 0) {
    tvherror(dn->log, "unable to open file '%s': %s",
             filename, strerror(errno));
    return -1;
  }
  if (fstat(fd, &st) || st.st_size == 0) {
    tvherror(dn->log, "unable to stat file '%s': %s",
             filename, strerror(errno));
    close(fd);
    return -1;
  }
  data = malloc(st.st_size+1);
  off = 0;
  do {
    r = read(fd, data + off, st.st_size - off);
    if (r < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    off += r;
  } while (off != st.st_size);
  close(fd);

  if (off == st.st_size) {
    data[off] = '\0';
    last_url = strrchr(filename, '/');
    if (last_url)
      last_url++;
    res = dn->process(dn->aux, last_url, data, off);
  } else {
    res = -1;
  }
  free(data);
  return res;
}

/*
 *
 */
static void
download_fetch_done(void *aux)
{
  http_client_t *hc = aux;
  download_t *dm = hc->hc_aux;
  if (dm->http_client) {
    dm->http_client = NULL;
    http_client_close((http_client_t *)aux);
  }
}

/*
 *
 */
static int
download_fetch_complete(http_client_t *hc)
{
  download_t *dn = hc->hc_aux;
  char *last_url = NULL;
  url_t u;

  switch (hc->hc_code) {
  case HTTP_STATUS_MOVED:
  case HTTP_STATUS_FOUND:
  case HTTP_STATUS_SEE_OTHER:
  case HTTP_STATUS_NOT_MODIFIED:
    return 0;
  }

  urlinit(&u);
  if (!urlparse(dn->url, &u)) {
    last_url = strrchr(u.path, '/');
    if (last_url)
      last_url++;
  }

  pthread_mutex_lock(&global_lock);

  if (dn->http_client == NULL)
    goto out;

  if (hc->hc_code == HTTP_STATUS_OK && hc->hc_result == 0 && hc->hc_data_size > 0)
    dn->process(dn->aux, last_url, hc->hc_data, hc->hc_data_size);
  else
    tvherror(dn->log, "unable to fetch data from url [%d-%d/%zd]",
             hc->hc_code, hc->hc_result, hc->hc_data_size);

  /* note: http_client_close must be called outside http_client callbacks */
  gtimer_arm(&dn->fetch_timer, download_fetch_done, hc, 0);

out:
  pthread_mutex_unlock(&global_lock);

  urlreset(&u);
  return 0;
}

/*
 *
 */
static void
download_fetch(void *aux)
{
  download_t *dn = aux;
  http_client_t *hc;
  url_t u;

  urlinit(&u);

  if (dn->url == NULL)
    goto done;

  if (strncmp(dn->url, "file://", 7) == 0) {
    download_file(dn, dn->url + 7);
    goto done;
  }

  if (dn->http_client) {
    http_client_close(dn->http_client);
    dn->http_client = NULL;
  }

  if (urlparse(dn->url, &u) < 0) {
    tvherror(dn->log, "wrong url");
    goto stop;
  }
  hc = http_client_connect(dn, HTTP_VERSION_1_1, u.scheme, u.host, u.port, NULL);
  if (hc == NULL) {
    tvherror(dn->log, "unable to open http client");
    goto stop;
  }
  hc->hc_handle_location = 1;
  hc->hc_data_limit = 1024*1024;
  hc->hc_data_complete = download_fetch_complete;
  http_client_register(hc);
  http_client_ssl_peer_verify(hc, dn->ssl_peer_verify);
  if (http_client_simple(hc, &u) < 0) {
    http_client_close(hc);
    tvherror(dn->log, "unable to send http command");
    goto stop;
  }

  dn->http_client = hc;
  goto done;

stop:
  if (dn->stop)
    dn->stop(dn->aux);
done:
  urlreset(&u);
}

/*
 *
 */
void
download_init( download_t *dn, const char *log )
{
  memset(dn, 0, sizeof(*dn));
  dn->log = strdup(log);
}

/*
 *
 */
void
download_start( download_t *dn, const char *url, void *aux )
{
  if (dn->http_client) {
    http_client_close(dn->http_client);
    dn->http_client = NULL;
  }
  if (url) {
    free(dn->url);
    dn->url = strdup(url);
  }
  dn->aux = aux;
  gtimer_arm(&dn->fetch_timer, download_fetch, dn, 0);
}

/*
 *
 */
void
download_done( download_t *dn )
{
  if (dn->http_client) {
    http_client_close(dn->http_client);
    dn->http_client = NULL;
  }
  gtimer_disarm(&dn->fetch_timer);
  free(dn->log); dn->log = NULL;
  free(dn->url); dn->url = NULL;
}
