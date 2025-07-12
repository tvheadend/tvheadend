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
#include "string_list.h"
#include "imagecache.h"
#include "epggrab.h"

#define XMLTV_FLAG_LCN		(1<<0)

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
  htsbuf_append_str(hq, "\
<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n\
<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n\
<tv generator-info-name=\"TVHeadend-");
  htsbuf_append_and_escape_xml(hq, tvheadend_version);
  htsbuf_append_str(hq, "\" source-info-name=\"tvh-");
  htsbuf_append_and_escape_xml(hq, config_get_server_name());
  htsbuf_append_str(hq, "\">\n");
}

/*
 *
 */
static void
http_xmltv_end(htsbuf_queue_t *hq)
{
  htsbuf_append_str(hq, "</tv>\n");
}


/** Determine name to use for the channel based on the
 * user's settings. This is done because some TVs have
 * broken parsers that require a "user readable" name
 * for the channel.
 *
 * @param buf Buffer that is used if we return an idnode.
 *
 * @return Buffer containing the name. This might not
 * be the same as the passed in temporary buffer.
 *
 */
static const char *
http_xmltv_channel_get_name(const http_connection_t *hc,
                            const channel_t *ch,
                            char *buf,
                            size_t buf_len)
{
  const int of = hc->hc_access->aa_xmltv_output_format;

  if (of == ACCESS_XMLTV_OUTPUT_FORMAT_BASIC_NO_HASH)
    return channel_get_name(ch, idnode_uuid_as_str(&ch->ch_id, buf));
  else
    return idnode_uuid_as_str(&ch->ch_id, buf);
}


/*
 *
 */
static void
http_xmltv_channel_add(http_connection_t *hc, htsbuf_queue_t *hq, int flags, const char *hostpath, channel_t *ch)
{
  const char *icon = channel_get_icon(ch);
  char ubuf[UUID_HEX_SIZE];
  const char *tag;
  int64_t lcn;
  htsbuf_qprintf(hq, "<channel id=\"");
  htsbuf_append_and_escape_xml(hq, http_xmltv_channel_get_name(hc, ch, ubuf, sizeof ubuf));
  htsbuf_qprintf(hq, "\">\n  <display-name>");
  htsbuf_append_and_escape_xml(hq, channel_get_name(ch, ""));
  htsbuf_append_str(hq, "</display-name>\n");
  lcn = channel_get_number(ch);
  if (lcn > 0) {
    tag = (flags & XMLTV_FLAG_LCN) ? "lcn" : "display-name";
    if (channel_get_minor(lcn)) {
      htsbuf_qprintf(hq, "  <%s>%u.%u</%s>\n",
                     tag, channel_get_major(lcn),
                     channel_get_minor(lcn), tag);
    } else {
      htsbuf_qprintf(hq, "  <%s>%u</%s>\n",
                     tag, channel_get_major(lcn), tag);
    }
  }
  if (icon) {
    int id = imagecache_get_id(icon);
    if (id) {
      htsbuf_qprintf(hq, "  <icon src=\"%s/imagecache/%d\"/>\n", hostpath, id);
    } else {
      htsbuf_append_str(hq, "  <icon src=\"");
      htsbuf_append_and_escape_xml(hq, icon);
      htsbuf_append_str(hq, "\"/>\n");
    }
  }
  htsbuf_append_str(hq, "</channel>\n");
}


/// Write each entry in the string list to the xml queue using the given tag_name.
static void
_http_xmltv_programme_write_string_list(htsbuf_queue_t *hq, const string_list_t* sl, const char* tag_name)
{
  if (!hq || !sl)
    return;

  const string_list_item_t *item;
  RB_FOREACH(item, sl, h_link) {
    htsbuf_qprintf(hq, "  <%s>", tag_name);
    htsbuf_append_and_escape_xml(hq, item->id);
    htsbuf_qprintf(hq, "</%s>\n", tag_name);
  }
}

static void
_http_xmltv_add_episode_num(htsbuf_queue_t *hq, uint16_t num, uint16_t cnt)
{
  /* xmltv numbers are zero-based not one-based, however the xmltv
   * counts are one-based.
   */
  if (num) htsbuf_qprintf(hq, "%d", num - 1);
  /* Some clients can not handle "X", only "X/Y" or "/Y",
   * so only output "/" if needed.
   */
  if (cnt) {
    htsbuf_append_str(hq, "/");
    htsbuf_qprintf(hq, "%d", cnt);
  }
}

/// Output a start tag for the tag and include a lang="xx" _only_ if we
/// have more than one language. This avoids outputting lots of tags for
/// the common case of only having one language, so is useful for very low
/// memory devices.
#define HTTP_XMLTV_OUTPUT_START_TAG_WITH_LANG(hq,rb_tree,lang_str,tag)  \
  do {                                                                  \
    htsbuf_qprintf(hq, "  <%s", tag);                                   \
    if (rb_tree->entries != 1)                                          \
      htsbuf_qprintf(hq, " lang=\"%s\"", lang_str->lang);               \
    htsbuf_append_str(hq,">");                                          \
  } while(0)

/** Output long description fields of the programme which are
 * not output for basic/limited devices.
 */
static void
http_xmltv_programme_one_long(const http_connection_t *hc,
                              htsbuf_queue_t *hq, const char *hostpath,
                              const channel_t *ch, const epg_broadcast_t *ebc)
{
  lang_str_ele_t *lse;
  epg_genre_t *genre;
  char buf[64];
  char prop_buf[512];
  const char *str;

  if (ebc->subtitle)
    RB_FOREACH(lse, ebc->subtitle, link) {
      /* Ignore empty sub-titles */
      if (!strempty(lse->str)) {
          HTTP_XMLTV_OUTPUT_START_TAG_WITH_LANG(hq, ebc->subtitle, lse, "sub-title");
          htsbuf_append_and_escape_xml(hq, lse->str);
          htsbuf_append_str(hq, "</sub-title>\n");
        }
    }

  if (ebc->description)
    RB_FOREACH(lse, ebc->description, link) {
      HTTP_XMLTV_OUTPUT_START_TAG_WITH_LANG(hq, ebc->description, lse, "desc");
      htsbuf_append_and_escape_xml(hq, lse->str);
      htsbuf_append_str(hq, "</desc>\n");
    }
  else if (ebc->summary)
    RB_FOREACH(lse, ebc->summary, link) {
      HTTP_XMLTV_OUTPUT_START_TAG_WITH_LANG(hq, ebc->summary, lse, "desc");
      htsbuf_append_and_escape_xml(hq, lse->str);
      htsbuf_append_str(hq, "</desc>\n");
    }
  if (ebc->image) {
      htsbuf_append_str(hq, "  <icon src=\"");
      htsbuf_append_and_escape_xml(hq, ebc->image);
      htsbuf_append_str(hq, "\"/>\n");
  }
  if (ebc->credits) {
    htsbuf_append_str(hq, "  <credits>\n");
    htsmsg_field_t *f;
    HTSMSG_FOREACH(f, ebc->credits) {
      htsbuf_qprintf(hq, "    <%s>", f->u.str);
      htsbuf_append_and_escape_xml(hq, htsmsg_field_name(f));
      htsbuf_qprintf(hq, "</%s>\n", f->u.str);
    }
    htsbuf_append_str(hq, "  </credits>\n");
  }

  if (epggrab_conf.epgdb_processparentallabels && ebc->rating_label) {
      const char *system = ebc->rating_label->rl_authority;
      if (!system) system = ebc->rating_label->rl_country;
      if (system && (ebc->rating_label->rl_display_label || ebc->rating_label->rl_display_age || ebc->rating_label->rl_age)) {
          htsbuf_append_str(hq, "  <rating system=\"");
          htsbuf_append_and_escape_xml(hq, system);
          htsbuf_append_str(hq, "\">\n");
          if (ebc->rating_label->rl_display_label) {
              htsbuf_append_str(hq, "    <value>");
              htsbuf_append_and_escape_xml(hq, ebc->rating_label->rl_display_label);
              htsbuf_append_str(hq, "</value>\n");
          } else if (ebc->rating_label->rl_display_age) {
              htsbuf_append_str(hq, "    <value>");
              htsbuf_qprintf(hq, "%d", ebc->rating_label->rl_display_age);
              htsbuf_append_str(hq, "</value>\n");
          } else if (ebc->rating_label->rl_age) {
              htsbuf_append_str(hq, "    <value>");
              htsbuf_qprintf(hq, "%d", ebc->rating_label->rl_age);
              htsbuf_append_str(hq, "</value>\n");
          }
          if (ebc->rating_label->rl_icon) {
              str = ebc->rating_label->rl_icon;
              if (!strempty(str)) {
                  str = imagecache_get_propstr(str, prop_buf, sizeof(prop_buf));
                  if (str) {
                      htsbuf_append_str(hq, "    <icon src=\"");
                      htsbuf_append_and_escape_xml(hq, str);
                      htsbuf_append_str(hq, "\"/>\n");
                  }
              }
          }
          htsbuf_append_str(hq, "  </rating>\n");
      } else if (ebc->age_rating) {
        htsbuf_append_str(hq, "  <rating system=\"AGE\">\n");
        htsbuf_append_str(hq, "    <value>");
        htsbuf_qprintf(hq, "%d", ebc->age_rating);
        htsbuf_append_str(hq, "</value>\n");
        htsbuf_append_str(hq, "  </rating>\n");
      }
  } else if (ebc->age_rating) {
      htsbuf_append_str(hq, "  <rating system=\"AGE\">\n");
      htsbuf_append_str(hq, "    <value>");
      htsbuf_qprintf(hq, "%d", ebc->age_rating);
      htsbuf_append_str(hq, "</value>\n");
      htsbuf_append_str(hq, "  </rating>\n");
  }

  _http_xmltv_programme_write_string_list(hq, ebc->category, "category");
  LIST_FOREACH(genre, &ebc->genre, link) {
    if (genre && genre->code) {
      if (epg_genre_get_str(genre, 0, 1, buf, sizeof(buf), "en")) {
        htsbuf_qprintf(hq, "  <category lang=\"en\">");
        htsbuf_append_and_escape_xml(hq, buf);
        htsbuf_append_str(hq, "</category>\n");
      }
    }
  }
  _http_xmltv_programme_write_string_list(hq, ebc->keyword, "keyword");
}

/*
 *
 */
static void
http_xmltv_programme_one(const http_connection_t *hc,
                         htsbuf_queue_t *hq, const char *hostpath,
                         const channel_t *ch, const epg_broadcast_t *ebc)
{
  epg_episode_num_t epnum;
  char start[32], stop[32], ubuf[UUID_HEX_SIZE];
  lang_str_ele_t *lse;
  const int of = hc->hc_access->aa_xmltv_output_format;

  if (ebc->title == NULL) return;
  http_xmltv_time(start, ebc->start);
  http_xmltv_time(stop, ebc->stop);
  htsbuf_qprintf(hq, "<programme start=\"%s\" stop=\"%s\" channel=\"",
                 start, stop);
  htsbuf_append_and_escape_xml(hq, http_xmltv_channel_get_name(hc, ch, ubuf, sizeof ubuf));
  htsbuf_qprintf(hq, "\">\n");
  RB_FOREACH(lse, ebc->title, link) {
    HTTP_XMLTV_OUTPUT_START_TAG_WITH_LANG(hq, ebc->title, lse, "title");
    htsbuf_append_and_escape_xml(hq, lse->str);
    htsbuf_append_str(hq, "</title>\n");
  }

  /* Basic formats are for low-memory devices that
   * only want very basic information.
   */
  if (of != ACCESS_XMLTV_OUTPUT_FORMAT_BASIC &&
      of != ACCESS_XMLTV_OUTPUT_FORMAT_BASIC_NO_HASH) {
    http_xmltv_programme_one_long(hc, hq, hostpath, ch, ebc);
  }

  /* We can't use epg_broadcast_epnumber_format since we need a specific
   * format whereas that can return an arbitrary text string.
   */
  epg_broadcast_get_epnum(ebc, &epnum);
  if (epnum.s_num || epnum.e_num || epnum.p_num) {
    htsbuf_append_str(hq, "  <episode-num system=\"xmltv_ns\">");
    _http_xmltv_add_episode_num(hq, epnum.s_num, epnum.s_cnt);
    htsbuf_append_str(hq," . ");
    _http_xmltv_add_episode_num(hq, epnum.e_num, epnum.e_cnt);
    htsbuf_append_str(hq," . ");
    _http_xmltv_add_episode_num(hq, epnum.p_num, epnum.p_cnt);
    htsbuf_append_str(hq, "  </episode-num>");
  }
  htsbuf_append_str(hq, "</programme>\n");
}

/*
 *
 */
static void
http_xmltv_programme_add(const http_connection_t *hc, htsbuf_queue_t *hq, const char *hostpath, channel_t *ch)
{
  epg_broadcast_t *ebc;

  RB_FOREACH(ebc, &ch->ch_epg_schedule, sched_link)
    http_xmltv_programme_one(hc, hq, hostpath, ch, ebc);
}

/**
 * Output a XMLTV containing a single channel
 */
static int
http_xmltv_channel(http_connection_t *hc, int flags, channel_t *channel)
{
  char hostpath[512];

  if (http_access_verify_channel(hc, ACCESS_STREAMING, channel))
    return http_noaccess_code(hc);

  http_get_hostpath(hc, hostpath, sizeof(hostpath));
  http_xmltv_begin(&hc->hc_reply);
  http_xmltv_channel_add(hc, &hc->hc_reply, flags, hostpath, channel);
  http_xmltv_programme_add(hc, &hc->hc_reply, hostpath, channel);
  http_xmltv_end(&hc->hc_reply);
  return 0;
}


/**
 * Output a playlist containing all channels with a specific tag
 */
static int
http_xmltv_tag(http_connection_t *hc, int flags, channel_tag_t *tag)
{
  idnode_list_mapping_t *ilm;
  char hostpath[512];
  channel_t *ch;

  if (access_verify2(hc->hc_access, ACCESS_STREAMING))
    return http_noaccess_code(hc);

  http_get_hostpath(hc, hostpath, sizeof(hostpath));

  http_xmltv_begin(&hc->hc_reply);
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    http_xmltv_channel_add(hc, &hc->hc_reply, flags, hostpath, ch);
  }
  LIST_FOREACH(ilm, &tag->ct_ctms, ilm_in1_link) {
    ch = (channel_t *)ilm->ilm_in2;
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    http_xmltv_programme_add(hc, &hc->hc_reply, hostpath, ch);
  }
  http_xmltv_end(&hc->hc_reply);

  return 0;
}

/**
 * Output a flat playlist with all channels
 */
static int
http_xmltv_channel_list(http_connection_t *hc, int flags)
{
  channel_t *ch;
  char hostpath[512];

  if (access_verify2(hc->hc_access, ACCESS_STREAMING))
    return http_noaccess_code(hc);

  http_get_hostpath(hc, hostpath, sizeof(hostpath));

  http_xmltv_begin(&hc->hc_reply);
  CHANNEL_FOREACH(ch) {
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    http_xmltv_channel_add(hc, &hc->hc_reply, flags, hostpath, ch);
  }
  CHANNEL_FOREACH(ch) {
    if (http_access_verify_channel(hc, ACCESS_STREAMING, ch))
      continue;
    http_xmltv_programme_add(hc, &hc->hc_reply, hostpath, ch);
  }
  http_xmltv_end(&hc->hc_reply);

  return 0;
}

/**
 * Handle requests for XMLTV export.
 */
int
page_xmltv(http_connection_t *hc, const char *remain, void *opaque)
{
  char *components[2], *cmd, *str;
  int nc, r, flags = 0;
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

  if ((str = http_arg_get(&hc->hc_req_args, "lcn")))
    if (atoll(str) > 0) flags |= XMLTV_FLAG_LCN;

  tvh_mutex_lock(&global_lock);

  if (nc == 2 && !strcmp(components[0], "channelid"))
    ch = channel_find_by_id(atoi(components[1]));
  else if (nc == 2 && !strcmp(components[0], "channelnumber"))
    ch = channel_find_by_number(components[1]);
  else if (nc == 2 && !strcmp(components[0], "channelname"))
    ch = channel_find_by_name(components[1]);
  else if (nc == 2 && !strcmp(components[0], "channel"))
    ch = channel_find(components[1]);
  else if (nc == 2 && !strcmp(components[0], "tagid"))
    tag = channel_tag_find_by_id(atoi(components[1]));
  else if (nc == 2 && !strcmp(components[0], "tagname"))
    tag = channel_tag_find_by_name(components[1], 0);
  else if (nc == 2 && !strcmp(components[0], "tag"))
    tag = channel_tag_find_by_uuid(components[1]);

  if (ch) {
    r = http_xmltv_channel(hc, flags, ch);
  } else if (tag) {
    r = http_xmltv_tag(hc, flags, tag);
  } else {
    if (!strcmp(cmd, "channels")) {
      r = http_xmltv_channel_list(hc, flags);
    } else {
      r = HTTP_STATUS_BAD_REQUEST;
    }
  }

  tvh_mutex_unlock(&global_lock);

  if (r == 0)
    http_output_content(hc, "text/xml");

  return r;
}
