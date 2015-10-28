/*
 *  tvheadend, XMLTV exporter
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
#include "config.h"
#include "webui.h"
#include "channels.h"
#include "http.h"

/*
 *
 */
static void
http_xmltv_time(char *dst, time_t t)
{
  struct tm tm;
  /* 20140817060000 +0200 */
  strftime(dst, 32, "%Y%m%d%H%M%S %z", localtime_r(&t, &tm));
}

/*
 *
 */
static void
http_xmltv_begin(htsbuf_queue_t *hq)
{
  htsbuf_qprintf(hq, "\
<?xml version=\"1.0\" ?>\n\
<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n\
<tv generator-info-name=\"TVHeadend-%s\" source-info-name=\"tvh-%s\">\n\
", tvheadend_version, config.server_name);
}

/*
 *
 */
static void
http_xmltv_end(htsbuf_queue_t *hq)
{
  htsbuf_qprintf(hq, "</tv>\n");
}

/*
 *
 */
static void
http_xmltv_channel_add(htsbuf_queue_t *hq, const char *hostpath, channel_t *ch)
{
  const char *icon = channel_get_icon(ch);
  htsbuf_qprintf(hq, "\
<channel id=\"%s\">\n\
  <display-name>%s</display-name>\n\
", idnode_uuid_as_sstr(&ch->ch_id), channel_get_name(ch));
  if (icon) {
    if (strncmp(icon, "imagecache/", 11) == 0)
      htsbuf_qprintf(hq, "  <icon src=\"%s/%s\"/>\n", hostpath, icon);
    else
      htsbuf_qprintf(hq, "  <icon src=\"%s\"/>\n", icon);
  }
  htsbuf_qprintf(hq, "</channel>\n");
}

/*
 *
 */
static void
http_xmltv_programme_one(htsbuf_queue_t *hq, const char *hostpath,
                         channel_t *ch, epg_broadcast_t *ebc)
{
  char start[32], stop[32];
  epg_episode_t *e = ebc->episode;
  lang_str_ele_t *lse;

  if (e == NULL || e->title == NULL) return;
  http_xmltv_time(start, ebc->start);
  http_xmltv_time(stop, ebc->stop);
  htsbuf_qprintf(hq, "<programe start=\"%s\" stop=\"%s\" channel=\"%s\">\n",
                 start, stop, idnode_uuid_as_sstr(&ch->ch_id));
  RB_FOREACH(lse, e->title, link)
    htsbuf_qprintf(hq, "  <title lang=\"%s\">%s</title>\n", lse->lang, lse->str);
  if (e->subtitle)
    RB_FOREACH(lse, e->subtitle, link)
      htsbuf_qprintf(hq, "  <sub-title lang=\"%s\">%s</sub-title>\n", lse->lang, lse->str);
  if (ebc->description)
    RB_FOREACH(lse, ebc->description, link)
      htsbuf_qprintf(hq, "  <desc lang=\"%s\">%s</desc>\n", lse->lang, lse->str);
  htsbuf_qprintf(hq, "</programe>\n");
}

/*
 *
 */
static void
http_xmltv_programme_add(htsbuf_queue_t *hq, const char *hostpath, channel_t *ch)
{
  epg_broadcast_t *ebc;

  RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link)
    http_xmltv_programme_one(hq, hostpath, ch, ebc);
}

/**
 * Output a XMLTV containing a single channel
 */
static int
http_xmltv_channel(http_connection_t *hc, channel_t *channel)
{
  char *hostpath;

  if (http_access_verify_channel(hc, ACCESS_STREAMING, channel, 1))
    return HTTP_STATUS_UNAUTHORIZED;

  hostpath = http_get_hostpath(hc);
  http_xmltv_begin(&hc->hc_reply);
  http_xmltv_channel_add(&hc->hc_reply, hostpath, channel);
  http_xmltv_programme_add(&hc->hc_reply, hostpath, channel);
  http_xmltv_end(&hc->hc_reply);
  free(hostpath);
  return 0;
}


/**
 * Output a playlist containing all channels with a specific tag
 */
static int
http_xmltv_tag(http_connection_t *hc, channel_tag_t *tag)
{
  idnode_list_mapping_t *ilm;
  char *hostpath;
  channel_t *ch;

  if (hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hostpath = http_get_hostpath(hc);

  http_xmltv_begin(&hc->hc_reply);
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;
    http_xmltv_channel_add(&hc->hc_reply, hostpath, ch);
  }
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;
    http_xmltv_programme_add(&hc->hc_reply, hostpath, ch);
  }
  http_xmltv_end(&hc->hc_reply);

  free(hostpath);
  return 0;
}

/**
 * Output a flat playlist with all channels
 */
static int
http_xmltv_channel_list(http_connection_t *hc)
{
  channel_t *ch;
  char *hostpath;

  if (hc->hc_access == NULL ||
     access_verify2(hc->hc_access, ACCESS_STREAMING))
    return HTTP_STATUS_UNAUTHORIZED;

  hostpath = http_get_hostpath(hc);

  http_xmltv_begin(&hc->hc_reply);
  CHANNEL_FOREACH(ch) {
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;
    http_xmltv_channel_add(&hc->hc_reply, hostpath, ch);
  }
  CHANNEL_FOREACH(ch) {
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch, 0))
      continue;
    http_xmltv_programme_add(&hc->hc_reply, hostpath, ch);
  }
  http_xmltv_end(&hc->hc_reply);

  free(hostpath);
  return 0;
}

/**
 * Handle requests for XMLTV export.
 */
int
page_xmltv(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2], *cmd;
  int nc, r;
  channel_t *ch = NULL;
  channel_tag_t *tag = NULL;

  if (!remain || *remain == '\0') {
    http_redirect(hc, "/xmltv/channels", &hc->hc_req_args, 0);
    return HTTP_STATUS_FOUND;
  }

  nc = http_tokenize((char *)remain, components, 2, '/');
  if (!nc)
    return HTTP_STATUS_BAD_REQUEST;

  cmd = tvh_strdupa(components[0]);

  if (nc == 2)
    http_deescape(components[1]);

  pthread_mutex_lock(&global_lock);

  if (nc == 2 && !strcmp(components[0], "channelid"))
    ch = channel_find_by_id(atoi(components[1]));
  else if (nc == 2 && !strcmp(components[0], "channelnumber"))
    ch = channel_find_by_number(components[1]);
  else if (nc == 2 && !strcmp(components[0], "channelname"))
    ch = channel_find_by_name(components[1]);
  else if (nc == 2 && !strcmp(components[0], "channel"))
    ch = channel_find(components[1]);
  else if (nc == 2 && !strcmp(components[0], "tagid"))
    tag = channel_tag_find_by_identifier(atoi(components[1]));
  else if (nc == 2 && !strcmp(components[0], "tagname"))
    tag = channel_tag_find_by_name(components[1], 0);
  else if (nc == 2 && !strcmp(components[0], "tag"))
    tag = channel_tag_find_by_uuid(components[1]);

  if (ch) {
    r = http_xmltv_channel(hc, ch);
  } else if (tag) {
    r = http_xmltv_tag(hc, tag);
  } else {
    if (!strcmp(cmd, "channels")) {
      r = http_xmltv_channel_list(hc);
    } else {
      r = HTTP_STATUS_BAD_REQUEST;
    }
  }

  pthread_mutex_unlock(&global_lock);

  if (r == 0)
    http_output_content(hc, "text/xml");

  return r;
}
