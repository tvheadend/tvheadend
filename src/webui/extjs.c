/*
 *  tvheadend, EXTJS based interface
 *  Copyright (C) 2008 Andreas Öman
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

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <arpa/inet.h>

#include "htsmsg.h"
#include "htsmsg_json.h"

#include "tvheadend.h"
#include "http.h"
#include "webui.h"
#include "access.h"
#include "dtable.h"
#include "channels.h"
#include "psi.h"

#include "dvr/dvr.h"
#include "serviceprobe.h"
#include "epggrab.h"
#include "epg.h"
#include "muxer.h"
#include "iptv_input.h"
#include "epggrab/private.h"
#include "config2.h"
#include "lang_codes.h"
#include "subscriptions.h"

/**
 *
 */
static void
extjs_load(htsbuf_queue_t *hq, const char *script)
{
  htsbuf_qprintf(hq,
                 "<script type=\"text/javascript\" "
		             "src=\"%s\"></script>\n", script);
}

/**
 *
 */
static void
extjs_exec(htsbuf_queue_t *hq, const char *fmt, ...)
{
  va_list ap;

  htsbuf_qprintf(hq, "<script type=\"text/javascript\">\r\n");

  va_start(ap, fmt);
  htsbuf_vqprintf(hq, fmt, ap);
  va_end(ap);

  htsbuf_qprintf(hq, "\r\n</script>\r\n");
}

/**
 * PVR info, deliver info about the given PVR entry
 */
static int
extjs_root(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

#define EXTJSPATH "static/extjs"

  htsbuf_qprintf(hq, "<html>\n"
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/adapter/ext/ext-base.js\"></script>\n"
		 "<script type=\"text/javascript\" src=\""EXTJSPATH"/ext-all-debug.js\"></script>\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\""EXTJSPATH"/resources/css/ext-all.css\">\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/livegrid/resources/css/ext-ux-livegrid.css\">\n"
		 "<link rel=\"stylesheet\" type=\"text/css\" href=\"static/app/ext.css\">\n");

  extjs_exec(hq, "Ext.BLANK_IMAGE_URL = "
	     "'"EXTJSPATH"/resources/images/default/s.gif';");

#if 0
  htsbuf_qprintf(hq, 
		 "<script type='text/javascript' "
		 "src='http://getfirebug.com/releases/lite/1.2/firebug-lite-compressed.js'></script>");
#endif


  /**
   * Load extjs extensions
   */
  extjs_load(hq, "static/app/extensions.js");
  extjs_load(hq, "static/livegrid/livegrid-all.js");
  extjs_load(hq, "static/lovcombo/lovcombo-all.js");
  extjs_load(hq, "static/multiselect/multiselect.js");
  extjs_load(hq, "static/multiselect/ddview.js");

  /**
   * Create a namespace for our app
   */
  extjs_exec(hq, "Ext.namespace('tvheadend');");

  /**
   * Load all components
   */
  extjs_load(hq, "static/app/comet.js");
  extjs_load(hq, "static/app/tableeditor.js");
  extjs_load(hq, "static/app/cteditor.js");
  extjs_load(hq, "static/app/acleditor.js");
  extjs_load(hq, "static/app/cwceditor.js");
  extjs_load(hq, "static/app/capmteditor.js");
  extjs_load(hq, "static/app/tvadapters.js");
#if ENABLE_LINUXDVB
  extjs_load(hq, "static/app/dvb.js");
#endif
  extjs_load(hq, "static/app/iptv.js");
#if ENABLE_V4L
  extjs_load(hq, "static/app/v4l.js");
#endif
  extjs_load(hq, "static/app/chconf.js");
  extjs_load(hq, "static/app/epg.js");
  extjs_load(hq, "static/app/dvr.js");
  extjs_load(hq, "static/app/epggrab.js");
  extjs_load(hq, "static/app/config.js");
  extjs_load(hq, "static/app/status.js");

  /**
   * Finally, the app itself
   */
  extjs_load(hq, "static/app/tvheadend.js");
  extjs_exec(hq, "Ext.onReady(tvheadend.app.init, tvheadend.app);");
  



  htsbuf_qprintf(hq,
		 "<style type=\"text/css\">\n"
		 "html, body {\n"
		 "\tfont:normal 12px verdana;\n"
		 "\tmargin:0;\n"
		 "\tpadding:0;\n"
		 "\tborder:0 none;\n"
		 "\toverflow:hidden;\n"
		 "\theight:100%%;\n"
		 "}\n"
		 "#systemlog {\n"
		 "\tfont:normal 12px courier; font-weight: bold;\n"
		 "}\n"
		 "p {\n"
		 "\tmargin:5px;\n"
		 "}\n"
		 "</style>\n"
		 "<title>HTS Tvheadend %s</title>\n"
		 "</head>\n"
		 "<body>\n"
		 "<div id=\"systemlog\"></div>\n"
		 "</body></html>\n",
		 tvheadend_version);
  http_output_html(hc);
  return 0;
}

/**
 * 
 */
static int
page_about(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;

  htsbuf_qprintf(hq, 
		 "<center>"
		 "<div class=\"about-title\">"
		 "HTS Tvheadend %s"
		 "</div><br>"
		 "&copy; 2006 - 2012 Andreas \303\226man, et al.<br><br>"
		 "<img src=\"docresources/tvheadendlogo.png\"><br>"
		 "<a href=\"http://www.lonelycoder.com/tvheadend\">"
		 "http://www.lonelycoder.com/tvheadend</a><br><br>"
		 "Based on software from "
		 "<a target=\"_blank\" href=\"http://www.extjs.com/\">ExtJS</a>. "
		 "Icons from "
		 "<a target=\"_blank\" href=\"http://www.famfamfam.com/lab/icons/silk/\">"
		 "FamFamFam</a>"
		 "<br><br>"
		 "Build: %s"
     "<p>"
     "If you'd like to support the project, please consider a donation."
     "<br/>"
     "All proceeds are used to support server infrastructure and buy test "
     "equipment."
     "<br/>"
     "<a href='https://www.paypal.com/cgi-bin/webscr?cmd=_donations&#38;business=andreas%%40lonelycoder%%2ecom&#38;item_name=Donation%%20to%%20the%%20Tvheadend%%20project&#38;currency_code=USD'><img src='https://www.paypal.com/en_US/i/btn/btn_donate_LG.gif' alt='' /></a>"
		 "</center>",
		 tvheadend_version,
		 tvheadend_version);

  http_output_html(hc);
  return 0;
}

/**
 *
 */
static int
extjs_tablemgr(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  dtable_t *dt;
  htsmsg_t *out = NULL, *in, *array;

  const char *tablename = http_arg_get(&hc->hc_req_args, "table");
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");

  if(op == NULL)
    return 400;

  if(tablename == NULL || (dt = dtable_find(tablename)) == NULL)
    return 404;
  
  if(http_access_verify(hc, dt->dt_dtc->dtc_read_access))
    return HTTP_STATUS_UNAUTHORIZED;

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  pthread_mutex_lock(dt->dt_dtc->dtc_mutex);

  if(!strcmp(op, "create")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    out = dtable_record_create(dt);

  } else if(!strcmp(op, "get")) {
    array = dtable_record_get_all(dt);

    out = htsmsg_create_map();
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    if(in == NULL)
      goto bad;

    dtable_record_update_by_array(dt, in);

  } else if(!strcmp(op, "delete")) {
    if(http_access_verify(hc, dt->dt_dtc->dtc_write_access))
      goto noaccess;

    if(in == NULL)
      goto bad;

    dtable_record_delete_by_array(dt, in);

  } else {
  bad:
    pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);
    return HTTP_STATUS_BAD_REQUEST;

  noaccess:
    pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(dt->dt_dtc->dtc_mutex);

  if(in != NULL)
    htsmsg_destroy(in);

  if(out != NULL) {
    htsmsg_json_serialize(out, hq, 0);
    htsmsg_destroy(out);
  }
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
static void
extjs_channels_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  channel_t *ch;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link)
    if(f->hmf_type == HMF_S64 &&
       (ch = channel_find_by_identifier(f->hmf_s64)) != NULL)
      channel_delete(ch);
}

/**
 *
 */
static void
extjs_channels_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  channel_t *ch;
  htsmsg_t *c;
  uint32_t id;
  const char *s;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       htsmsg_get_u32(c, "id", &id))
      continue;

    if((ch = channel_find_by_identifier(id)) == NULL)
      continue;

    if((s = htsmsg_get_str(c, "name")) != NULL)
      channel_rename(ch, s);

    if((s = htsmsg_get_str(c, "ch_icon")) != NULL)
      channel_set_icon(ch, s);

    if((s = htsmsg_get_str(c, "tags")) != NULL)
      channel_set_tags_from_list(ch, s);

    if((s = htsmsg_get_str(c, "epg_pre_start")) != NULL)
      channel_set_epg_postpre_time(ch, 1, atoi(s));

    if((s = htsmsg_get_str(c, "epg_post_end")) != NULL)
      channel_set_epg_postpre_time(ch, 0, atoi(s));

    if((s = htsmsg_get_str(c, "number")) != NULL)
      channel_set_number(ch, atoi(s));

    if((s = htsmsg_get_str(c, "epggrabsrc")) != NULL) {
      char *tmp = strdup(s);
      char *sptr = NULL;
      char *modecid  = strtok_r(tmp, ",", &sptr);
      char *modid, *ecid;
      epggrab_module_t *mod;
      epggrab_channel_t *ec;
      epggrab_channel_link_t *ecl;

      /* Clear existing */
      LIST_FOREACH(mod, &epggrab_modules, link) {
        if (mod->type != EPGGRAB_OTA && mod->channels) {
          RB_FOREACH(ec, mod->channels, link) {
            LIST_FOREACH(ecl, &ec->channels, link) {
              if (ecl->channel == ch) {
                LIST_REMOVE(ecl, link);
                free(ecl);
                mod->ch_save(mod, ec);
                break;
              }
            }
          }
        }
      }

      /* Add new */
      while (modecid) {
        modid    = strtok(modecid, "|");
        ecid     = strtok(NULL, "|");
        modecid  = strtok_r(NULL, ",", &sptr);

        if (!(mod = epggrab_module_find_by_id(modid)))
          continue;
        if (!mod->channels)
          continue;
        if (!(ec = epggrab_channel_find(mod->channels, ecid, 0, NULL, mod)))
          continue;

        epggrab_channel_link(ec, ch);
      }

      /* Cleanup */
      free(tmp);
    }
  }
}

/**
 *
 */
static htsmsg_t *
build_record_channel ( channel_t *ch )
{
  char buf[1024];
  channel_tag_mapping_t *ctm;
  htsmsg_t *c;
  char *epggrabsrc;
  epggrab_module_t *mod;
  epggrab_channel_t *ec;
  epggrab_channel_link_t *ecl;

  c = htsmsg_create_map();
  htsmsg_add_str(c, "name", ch->ch_name);
  htsmsg_add_u32(c, "chid", ch->ch_id);

  if(ch->ch_icon != NULL)
    htsmsg_add_str(c, "ch_icon", ch->ch_icon);

  buf[0] = 0;
  LIST_FOREACH(ctm, &ch->ch_ctms, ctm_channel_link) {
	  snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		  "%s%d", strlen(buf) == 0 ? "" : ",",
		  ctm->ctm_tag->ct_identifier);
  }
  htsmsg_add_str(c, "tags", buf);

  htsmsg_add_s32(c, "epg_pre_start", ch->ch_dvr_extra_time_pre);
  htsmsg_add_s32(c, "epg_post_end",  ch->ch_dvr_extra_time_post);
  htsmsg_add_s32(c, "number",        ch->ch_number);

  epggrabsrc = NULL;
  LIST_FOREACH(mod, &epggrab_modules, link) {
    if (mod->type != EPGGRAB_OTA && mod->channels) {
      RB_FOREACH(ec, mod->channels, link) {
        LIST_FOREACH(ecl, &ec->channels, link) {
          if (ecl->channel == ch) {
            char id[100];
            sprintf(id, "%s|%s", mod->id, ec->id);
            if (!epggrabsrc) {
              epggrabsrc = strdup(id);
            } else {
              epggrabsrc = realloc(epggrabsrc, strlen(epggrabsrc) + 2 + strlen(id));
              strcat(epggrabsrc, ",");
              strcat(epggrabsrc, id);
            }
          }
        }
      }
    }
  }
  if (epggrabsrc) htsmsg_add_str(c, "epggrabsrc", epggrabsrc);
  free(epggrabsrc);
  return c;
}

/**
 *
 */
static int
extjs_channels(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *array;
  channel_t *ch;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");

  if(op == NULL)
    return 400;

  htsmsg_autodtor(in) =
    entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  htsmsg_autodtor(out) = htsmsg_create_map();

  scopedgloballock();

  if(!strcmp(op, "list")) {
    array = htsmsg_create_list();

    RB_FOREACH(ch, &channel_name_tree, ch_name_link) {
      htsmsg_add_msg(array, NULL, build_record_channel(ch));
    }
    
    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "create")) {
    out = build_record_channel(channel_create());

  } else if(!strcmp(op, "delete") && in != NULL) {
    extjs_channels_delete(in);

  } else if(!strcmp(op, "update") && in != NULL) {
    extjs_channels_update(in);
     
  } else {
    return 400;
  }

  htsmsg_json_serialize(out, hq, 0);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 * EPG Content Groups
 */
static int
extjs_ecglist(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array;

  out   = htsmsg_create_map();
  array = epg_genres_list_all(1, 0);
  htsmsg_add_msg(out, "entries", array);
  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
static htsmsg_t *
json_single_record(htsmsg_t *rec, const char *root)
{
  htsmsg_t *out, *array;
  
  out = htsmsg_create_map();
  array = htsmsg_create_list();

  htsmsg_add_msg(array, NULL, rec);
  htsmsg_add_msg(out, root, array);
  return out;

}

/**
 *
 */
static int
extjs_epggrab(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *r, *e;
  htsmsg_field_t *f;
  const char *str;
  uint32_t u32;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings (not the advanced schedule) */
  if(!strcmp(op, "loadSettings")) {

    pthread_mutex_lock(&epggrab_mutex);
    r = htsmsg_create_map();
    if (epggrab_module)
      htsmsg_add_str(r, "module", epggrab_module->id);
    htsmsg_add_u32(r, "interval", epggrab_interval);
    htsmsg_add_u32(r, "channel_rename", epggrab_channel_rename);
    htsmsg_add_u32(r, "channel_renumber", epggrab_channel_renumber);
    htsmsg_add_u32(r, "channel_reicon", epggrab_channel_reicon);
    pthread_mutex_unlock(&epggrab_mutex);

    out = json_single_record(r, "epggrabSettings");

  /* List of modules and currently states */
  } else if (!strcmp(op, "moduleList")) {
    out = htsmsg_create_map();
    pthread_mutex_lock(&epggrab_mutex);
    array = epggrab_module_list();
    pthread_mutex_unlock(&epggrab_mutex);
    htsmsg_add_msg(out, "entries", array);

  /* Channel list */
  } else if (!strcmp(op, "channelList")) {
    out = htsmsg_create_map();
    pthread_mutex_lock(&global_lock);
    array = epggrab_channel_list();
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", array);

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    int save = 0;
    pthread_mutex_lock(&epggrab_mutex);
    str = http_arg_get(&hc->hc_req_args, "channel_rename");
    save |= epggrab_set_channel_rename(str ? 1 : 0);
    str = http_arg_get(&hc->hc_req_args, "channel_renumber");
    save |= epggrab_set_channel_renumber(str ? 1 : 0);
    str = http_arg_get(&hc->hc_req_args, "channel_reicon");
    save |= epggrab_set_channel_reicon(str ? 1 : 0);
    if ( (str = http_arg_get(&hc->hc_req_args, "interval")) )
      save |= epggrab_set_interval(atoi(str));
    if ( (str = http_arg_get(&hc->hc_req_args, "module")) )
      save |= epggrab_set_module_by_id(str);
    if ( (str = http_arg_get(&hc->hc_req_args, "external")) ) {
      if ( (array = htsmsg_json_deserialize(str)) ) {
        HTSMSG_FOREACH(f, array) {
          if ( (e = htsmsg_get_map_by_field(f)) ) {
            str = htsmsg_get_str(e, "id");
            u32 = 0;
            htsmsg_get_u32(e, "enabled", &u32);
            if ( str ) save |= epggrab_enable_module_by_id(str, u32);
          }
        }
        htsmsg_destroy(array);
      }
    }
    if (save) epggrab_save();
    pthread_mutex_unlock(&epggrab_mutex);
    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  return 0;
}

/**
 *
 */
static int
extjs_channeltags(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;
  channel_tag_t *ct;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "listTags")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    TAILQ_FOREACH(ct, &channel_tags, ct_link) {
      if(!ct->ct_enabled)
	continue;

      e = htsmsg_create_map();
      htsmsg_add_u32(e, "identifier", ct->ct_identifier);
      htsmsg_add_str(e, "name", ct->ct_name);
      htsmsg_add_msg(array, NULL, e);
    }

    htsmsg_add_msg(out, "entries", array);

  } else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;

}

/**
 *
 */
static int
extjs_confignames(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;
  dvr_config_t *cfg;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    if (http_access_verify(hc, ACCESS_RECORDER_ALL))
      goto skip;

    LIST_FOREACH(cfg, &dvrconfigs, config_link) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "identifier", cfg->dvr_config_name);
      if (strlen(cfg->dvr_config_name) == 0)
        htsmsg_add_str(e, "name", "(default)");
      else
        htsmsg_add_str(e, "name", cfg->dvr_config_name);
      htsmsg_add_msg(array, NULL, e);
    }

skip:
    htsmsg_add_msg(out, "entries", array);

  } else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;

}

/**
 *
 */
static int
extjs_languages(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array, *e;

  pthread_mutex_lock(&global_lock);

  if(op != NULL && !strcmp(op, "list")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    const lang_code_t *c = lang_codes;
    while (c->code2b) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "identifier", c->code2b);
      htsmsg_add_str(e, "name", c->desc);
      htsmsg_add_msg(array, NULL, e);
      c++;
    }
  }
  else if(op != NULL && !strcmp(op, "config")) {

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    const lang_code_t **c = lang_code_split2(NULL);
    if(c) {
      int i = 0;
      while (c[i]) {
        e = htsmsg_create_map();
        htsmsg_add_str(e, "identifier", c[i]->code2b);
        htsmsg_add_str(e, "name", c[i]->desc);
        htsmsg_add_msg(array, NULL, e);
        i++;
      }
      free(c);
    }
  }
  else {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;

}

/**
 *
 */
static int
extjs_epg(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  epg_query_result_t eqr;
  epg_broadcast_t *e;
  epg_episode_t *ee = NULL;
  epg_genre_t *eg = NULL, genre;
  channel_t *ch;
  int start = 0, end, limit, i;
  const char *s;
  char buf[100];
  const char *channel = http_arg_get(&hc->hc_req_args, "channel");
  const char *tag     = http_arg_get(&hc->hc_req_args, "tag");
  const char *title   = http_arg_get(&hc->hc_req_args, "title");
  const char *lang    = http_arg_get(&hc->hc_args, "Accept-Language");

  if(channel && !channel[0]) channel = NULL;
  if(tag     && !tag[0])     tag = NULL;

  if((s = http_arg_get(&hc->hc_req_args, "start")) != NULL)
    start = atoi(s);

  if((s = http_arg_get(&hc->hc_req_args, "limit")) != NULL)
    limit = atoi(s);
  else
    limit = 20; /* XXX */

  if ((s = http_arg_get(&hc->hc_req_args, "contenttype"))) {
    genre.code = atoi(s);
    eg = &genre;
  }

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);

  epg_query(&eqr, channel, tag, eg, title, lang);

  epg_query_sort(&eqr);

  htsmsg_add_u32(out, "totalCount", eqr.eqr_entries);


  start = MIN(start, eqr.eqr_entries);
  end = MIN(start + limit, eqr.eqr_entries);

  for(i = start; i < end; i++) {
    e  = eqr.eqr_array[i];
    ee = e->episode;
    ch = e->channel;
    if (!ch||!ee) continue;

    m = htsmsg_create_map();

    htsmsg_add_str(m, "channel", ch->ch_name);
    htsmsg_add_u32(m, "channelid", ch->ch_id);
    if(ch->ch_icon != NULL)
	    htsmsg_add_str(m, "chicon", ch->ch_icon);

    if((s = epg_episode_get_title(ee, lang)))
      htsmsg_add_str(m, "title", s);
    if((s = epg_episode_get_subtitle(ee, lang)))
      htsmsg_add_str(m, "subtitle", s);

    if((s = epg_broadcast_get_description(e, lang)))
      htsmsg_add_str(m, "description", s);
    else if((s = epg_broadcast_get_summary(e, lang)))
      htsmsg_add_str(m, "description", s);

    if (epg_episode_number_format(ee, buf, 100, NULL, "Season %d", ".",
                                  "Episode %d", "/%d"))
      htsmsg_add_str(m, "episode", buf);

    htsmsg_add_u32(m, "id", e->id);
    htsmsg_add_u32(m, "start", e->start);
    htsmsg_add_u32(m, "end", e->stop);
    htsmsg_add_u32(m, "duration", e->stop - e->start);

    if(e->serieslink)
      htsmsg_add_str(m, "serieslink", e->serieslink->uri);
    
    if((eg = LIST_FIRST(&ee->genre))) {
      htsmsg_add_u32(m, "contenttype", eg->code);
    }

    dvr_entry_t *de;
    if((de = dvr_entry_find_by_event(e)) != NULL)
      htsmsg_add_str(m, "schedstate", dvr_entry_schedstatus(de));

    htsmsg_add_msg(array, NULL, m);
  }

  epg_query_free(&eqr);

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

static int
extjs_epgrelated(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  epg_broadcast_t *e, *ebc;
  epg_episode_t *ee, *ee2;
  channel_t *ch;
  uint32_t count = 0;
  const char *s;
  char buf[100];

  const char *lang  = http_arg_get(&hc->hc_args, "Accept-Language");
  const char *id    = http_arg_get(&hc->hc_req_args, "id");
  const char *type  = http_arg_get(&hc->hc_req_args, "type");

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  pthread_mutex_lock(&global_lock);
  if ( id && type ) {
    e = epg_broadcast_find_by_id(atoi(id), NULL);
    if ( e && e->episode ) {
      ee = e->episode;

      /* Alternative broadcasts */
      if (!strcmp(type, "alternative")) {
        LIST_FOREACH(ebc, &ee->broadcasts, ep_link) {
          ch = ebc->channel;
          if ( !ch ) continue; // skip something not viewable
          if ( ebc == e ) continue; // skip self
          count++;
          m = htsmsg_create_map();
          htsmsg_add_u32(m, "id", ebc->id);
          if ( ch->ch_name ) htsmsg_add_str(m, "channel", ch->ch_name);
          if ( ch->ch_icon ) htsmsg_add_str(m, "chicon", ch->ch_icon);
          htsmsg_add_u32(m, "start", ebc->start);
          htsmsg_add_msg(array, NULL, m);
        }
      
      /* Related */
      } else if (!strcmp(type, "related")) {
        if (ee->brand) {
          LIST_FOREACH(ee2, &ee->brand->episodes, blink) {
            if (ee2 == ee) continue;
            if (!ee2->title) continue;
            count++;
            m = htsmsg_create_map();
            htsmsg_add_str(m, "uri", ee2->uri);
            if ((s = epg_episode_get_title(ee2, lang)))
              htsmsg_add_str(m, "title", s);
            if ((s = epg_episode_get_subtitle(ee2, lang)))
              htsmsg_add_str(m, "subtitle", s);
            if (epg_episode_number_format(ee2, buf, 100, NULL, "Season %d",
                                          ".", "Episode %d", "/%d"))
              htsmsg_add_str(m, "episode", buf);
            htsmsg_add_msg(array, NULL, m);
          }
        } else if (ee->season) {
          LIST_FOREACH(ee2, &ee->season->episodes, slink) {
            if (ee2 == ee) continue;
            if (!ee2->title) continue;
            count++;
            m = htsmsg_create_map();
            htsmsg_add_str(m, "uri", ee2->uri);
            if ((s = epg_episode_get_title(ee2, lang)))
              htsmsg_add_str(m, "title", s);
            if ((s = epg_episode_get_subtitle(ee2, lang)))
              htsmsg_add_str(m, "subtitle", s);
            if (epg_episode_number_format(ee2, buf, 100, NULL, "Season %d",
                                          ".", "Episode %d", "/%d"))
              htsmsg_add_str(m, "episode", buf);
            htsmsg_add_msg(array, NULL, m);
          }
        }
      }
    }
  }
  pthread_mutex_unlock(&global_lock);

  htsmsg_add_u32(out, "totalCount", count);
  htsmsg_add_msg(out, "entries", array);
  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
static int
extjs_epgobject(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *array;

  if(op == NULL)
    return 400;

  if (!strcmp(op, "brandList")) {
    out   = htsmsg_create_map();
    pthread_mutex_lock(&global_lock);
    array = epg_brand_list();
    pthread_mutex_unlock(&global_lock);
    htsmsg_add_msg(out, "entries", array);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  return 0;
}

/**
 *
 */
static int
extjs_dvr(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *r;
  dvr_entry_t *de;
  const char *s;
  int flags = 0;
  dvr_config_t *cfg;
  epg_broadcast_t *e;

  if(op == NULL)
    op = "loadSettings";

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  if(!strcmp(op, "recordEvent") || !strcmp(op, "recordSeries")) {

    const char *config_name = http_arg_get(&hc->hc_req_args, "config_name");

    s = http_arg_get(&hc->hc_req_args, "eventId");
    if((e = epg_broadcast_find_by_id(atoi(s), NULL)) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    if (http_access_verify(hc, ACCESS_RECORDER_ALL)) {
      config_name = NULL;
      LIST_FOREACH(cfg, &dvrconfigs, config_link) {
        if (cfg->dvr_config_name && hc->hc_username &&
            strcmp(cfg->dvr_config_name, hc->hc_username) == 0) {
          config_name = cfg->dvr_config_name;
          break;
        }
      }
      if (config_name == NULL && hc->hc_username)
        tvhlog(LOG_INFO,"dvr","User '%s' has no dvr config with identical name, using default...", hc->hc_username);
    }

    if (!strcmp(op, "recordEvent"))
      dvr_entry_create_by_event(config_name,
                                e, 0, 0, 
                                hc->hc_representative, NULL, DVR_PRIO_NORMAL);
    else
      dvr_autorec_add_series_link(config_name, e, hc->hc_representative, "Created from EPG query");

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);
  } else if(!strcmp(op, "cancelEntry")) {
    s = http_arg_get(&hc->hc_req_args, "entryId");

    if((de = dvr_entry_find_by_id(atoi(s))) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    dvr_entry_cancel(de);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "deleteEntry")) {
    s = http_arg_get(&hc->hc_req_args, "entryId");

    if((de = dvr_entry_find_by_id(atoi(s))) == NULL) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    dvr_entry_delete(de);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "createEntry")) {

    const char *config_name = http_arg_get(&hc->hc_req_args, "config_name");
    const char *title    = http_arg_get(&hc->hc_req_args, "title");
    const char *datestr  = http_arg_get(&hc->hc_req_args, "date");
    const char *startstr = http_arg_get(&hc->hc_req_args, "starttime");
    const char *stopstr  = http_arg_get(&hc->hc_req_args, "stoptime");
    const char *channel  = http_arg_get(&hc->hc_req_args, "channelid");
    const char *pri      = http_arg_get(&hc->hc_req_args, "pri");

    channel_t *ch = channel ? channel_find_by_identifier(atoi(channel)) : NULL;

    if(ch == NULL || title == NULL || 
       datestr  == NULL || strlen(datestr)  != 10 ||
       startstr == NULL || strlen(startstr) != 5  ||
       stopstr  == NULL || strlen(stopstr)  != 5) {
      pthread_mutex_unlock(&global_lock);
      return HTTP_STATUS_BAD_REQUEST;
    }

    struct tm t = {0};
    t.tm_year = atoi(datestr + 6) - 1900;
    t.tm_mon = atoi(datestr) - 1;
    t.tm_mday = atoi(datestr + 3);
    t.tm_isdst = -1;

    t.tm_hour = atoi(startstr);
    t.tm_min = atoi(startstr + 3);
    
    time_t start = mktime(&t);

    t.tm_hour = atoi(stopstr);
    t.tm_min = atoi(stopstr + 3);
    
    time_t stop = mktime(&t);

    if(stop < start)
      stop += 86400;

    if (http_access_verify(hc, ACCESS_RECORDER_ALL)) {
      config_name = NULL;
      LIST_FOREACH(cfg, &dvrconfigs, config_link) {
        if (cfg->dvr_config_name && hc->hc_username &&
            strcmp(cfg->dvr_config_name, hc->hc_username) == 0) {
          config_name = cfg->dvr_config_name;
          break;
        }
      }
      if (config_name == NULL && hc->hc_username)
        tvhlog(LOG_INFO,"dvr","User '%s' has no dvr config with identical name, using default...", hc->hc_username);
    }

    dvr_entry_create(config_name,
                     ch, start, stop, 0, 0, title, NULL, NULL,
                     0, hc->hc_representative, 
		                 NULL, dvr_pri2val(pri));

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "createAutoRec")) {
    epg_genre_t genre, *eg = NULL;
    if ((s = http_arg_get(&hc->hc_req_args, "contenttype"))) {
      genre.code = atoi(s);
      eg = &genre;
    }

    dvr_autorec_add(http_arg_get(&hc->hc_req_args, "config_name"),
                    http_arg_get(&hc->hc_req_args, "title"),
		    http_arg_get(&hc->hc_req_args, "channel"),
		    http_arg_get(&hc->hc_req_args, "tag"),
        eg,
		    hc->hc_representative, "Created from EPG query");

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "loadSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    if (s == NULL)
      s = "";
    cfg = dvr_config_find_by_name_default(s);

    r = htsmsg_create_map();
    htsmsg_add_str(r, "storage", cfg->dvr_storage);
    htsmsg_add_str(r, "container", muxer_container_type2txt(cfg->dvr_mc));
    if(cfg->dvr_postproc != NULL)
      htsmsg_add_str(r, "postproc", cfg->dvr_postproc);
    htsmsg_add_u32(r, "retention", cfg->dvr_retention_days);
    htsmsg_add_u32(r, "preExtraTime", cfg->dvr_extra_time_pre);
    htsmsg_add_u32(r, "postExtraTime", cfg->dvr_extra_time_post);
    htsmsg_add_u32(r, "dayDirs",        !!(cfg->dvr_flags & DVR_DIR_PER_DAY));
    htsmsg_add_u32(r, "channelDirs",    !!(cfg->dvr_flags & DVR_DIR_PER_CHANNEL));
    htsmsg_add_u32(r, "channelInTitle", !!(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE));
    htsmsg_add_u32(r, "dateInTitle",    !!(cfg->dvr_flags & DVR_DATE_IN_TITLE));
    htsmsg_add_u32(r, "timeInTitle",    !!(cfg->dvr_flags & DVR_TIME_IN_TITLE));
    htsmsg_add_u32(r, "whitespaceInTitle", !!(cfg->dvr_flags & DVR_WHITESPACE_IN_TITLE));
    htsmsg_add_u32(r, "titleDirs", !!(cfg->dvr_flags & DVR_DIR_PER_TITLE));
    htsmsg_add_u32(r, "episodeInTitle", !!(cfg->dvr_flags & DVR_EPISODE_IN_TITLE));
    htsmsg_add_u32(r, "cleanTitle", !!(cfg->dvr_flags & DVR_CLEAN_TITLE));
    htsmsg_add_u32(r, "tagFiles", !!(cfg->dvr_flags & DVR_TAG_FILES));

    out = json_single_record(r, "dvrSettings");

  } else if(!strcmp(op, "saveSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    cfg = dvr_config_find_by_name(s);
    if (cfg == NULL)
      cfg = dvr_config_create(s);

    tvhlog(LOG_INFO,"dvr","Saving configuration '%s'", cfg->dvr_config_name);

    if((s = http_arg_get(&hc->hc_req_args, "storage")) != NULL)
      dvr_storage_set(cfg,s);
    
   if((s = http_arg_get(&hc->hc_req_args, "container")) != NULL)
      dvr_container_set(cfg,s);

    if((s = http_arg_get(&hc->hc_req_args, "postproc")) != NULL)
      dvr_postproc_set(cfg,s);

    if((s = http_arg_get(&hc->hc_req_args, "retention")) != NULL)
      dvr_retention_set(cfg,atoi(s));

   if((s = http_arg_get(&hc->hc_req_args, "preExtraTime")) != NULL)
     dvr_extra_time_pre_set(cfg,atoi(s));

   if((s = http_arg_get(&hc->hc_req_args, "postExtraTime")) != NULL)
     dvr_extra_time_post_set(cfg,atoi(s));

    if(http_arg_get(&hc->hc_req_args, "dayDirs") != NULL)
      flags |= DVR_DIR_PER_DAY;
    if(http_arg_get(&hc->hc_req_args, "channelDirs") != NULL)
      flags |= DVR_DIR_PER_CHANNEL;
    if(http_arg_get(&hc->hc_req_args, "channelInTitle") != NULL)
      flags |= DVR_CHANNEL_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "cleanTitle") != NULL)
      flags |= DVR_CLEAN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "dateInTitle") != NULL)
      flags |= DVR_DATE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "timeInTitle") != NULL)
      flags |= DVR_TIME_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "whitespaceInTitle") != NULL)
      flags |= DVR_WHITESPACE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "titleDirs") != NULL)
      flags |= DVR_DIR_PER_TITLE;
    if(http_arg_get(&hc->hc_req_args, "episodeInTitle") != NULL)
      flags |= DVR_EPISODE_IN_TITLE;
    if(http_arg_get(&hc->hc_req_args, "tagFiles") != NULL)
      flags |= DVR_TAG_FILES;

    dvr_flags_set(cfg,flags);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else if(!strcmp(op, "deleteSettings")) {

    s = http_arg_get(&hc->hc_req_args, "config_name");
    dvr_config_delete(s);

    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {

    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_BAD_REQUEST;
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;

}

/**
 *
 */
static int
extjs_dvrlist(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array, *m;
  dvr_query_result_t dqr;
  dvr_entry_t *de;
  int start = 0, end, limit, i;
  const char *s;
  off_t fsize;
  char buf[100];

  if((s = http_arg_get(&hc->hc_req_args, "start")) != NULL)
    start = atoi(s);

  if((s = http_arg_get(&hc->hc_req_args, "limit")) != NULL)
    limit = atoi(s);
  else
    limit = 20; /* XXX */

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_RECORDER)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  out = htsmsg_create_map();
  array = htsmsg_create_list();


  dvr_query(&dqr);

  dvr_query_sort(&dqr);

  htsmsg_add_u32(out, "totalCount", dqr.dqr_entries);

  start = MIN(start, dqr.dqr_entries);
  end = MIN(start + limit, dqr.dqr_entries);

  for(i = start; i < end; i++) {
    de = dqr.dqr_array[i];

    m = htsmsg_create_map();

    if(de->de_channel != NULL) {
      htsmsg_add_str(m, "channel", de->de_channel->ch_name);
      if(de->de_channel->ch_icon != NULL)
	htsmsg_add_str(m, "chicon", de->de_channel->ch_icon);
    }

    htsmsg_add_str(m, "config_name", de->de_config_name);

    if(de->de_title != NULL)
      htsmsg_add_str(m, "title", lang_str_get(de->de_title, NULL));

    if(de->de_desc != NULL)
      htsmsg_add_str(m, "description", lang_str_get(de->de_desc, NULL));

    if (de->de_bcast && de->de_bcast->episode)
      if (epg_episode_number_format(de->de_bcast->episode, buf, 100, NULL, "Season %d", ".", "Episode %d", "/%d"))
        htsmsg_add_str(m, "episode", buf);

    htsmsg_add_u32(m, "id", de->de_id);
    htsmsg_add_u32(m, "start", de->de_start);
    htsmsg_add_u32(m, "end", de->de_stop);
    htsmsg_add_u32(m, "duration", de->de_stop - de->de_start);
    
    htsmsg_add_str(m, "creator", de->de_creator);

    htsmsg_add_str(m, "pri", dvr_val2pri(de->de_pri));

    htsmsg_add_str(m, "status", dvr_entry_status(de));
    htsmsg_add_str(m, "schedstate", dvr_entry_schedstatus(de));


    if(de->de_sched_state == DVR_COMPLETED) {
      fsize = dvr_get_filesize(de);
      if(fsize > 0) {
	char url[100];
	htsmsg_add_s64(m, "filesize", fsize);

	snprintf(url, sizeof(url), "dvrfile/%d", de->de_id);
	htsmsg_add_str(m, "url", url);
      }
    }


    htsmsg_add_msg(array, NULL, m);
  }

  dvr_query_free(&dqr);

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
static int
extjs_subscriptions(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array;
  th_subscription_t *s;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  out = htsmsg_create_map();
  array = htsmsg_create_list();

  LIST_FOREACH(s, &subscriptions, ths_global_link)
    htsmsg_add_msg(array, NULL, subscription_create_msg(s));

  pthread_mutex_unlock(&global_lock);

  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}


/**
 *
 */
void
extjs_service_delete(htsmsg_t *in)
{
  htsmsg_field_t *f;
  service_t *t;
  const char *id;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((id = htsmsg_field_get_string(f)) != NULL &&
       (t = service_find_by_identifier(id)) != NULL)
      service_destroy(t);
  }
}

/**
 *
 */
static void
service_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  service_t *t;
  uint32_t u32;
  const char *id;
  const char *chname;
  const char *dvb_charset;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = service_find_by_identifier(id)) == NULL)
      continue;

    if(!htsmsg_get_u32(c, "enabled", &u32))
      service_set_enable(t, u32);

    if((chname = htsmsg_get_str(c, "channelname")) != NULL) 
      service_map_channel(t, channel_find_by_name(chname, 1, 0), 1);

    if((dvb_charset = htsmsg_get_str(c, "dvb_charset")) != NULL)
      service_set_dvb_charset(t, dvb_charset);

    if(!htsmsg_get_u32(c, "dvb_eit_enable", &u32))
      service_set_dvb_eit_enable(t, u32);
  }
}

/**
 *
 */
static int
extjs_servicedetails(http_connection_t *hc, 
		     const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *streams, *c;
  service_t *t;
  elementary_stream_t *st;
  caid_t *ca;
  char buf[128];

  pthread_mutex_lock(&global_lock);

  if(remain == NULL || (t = service_find_by_identifier(remain)) == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  streams = htsmsg_create_list();

  TAILQ_FOREACH(st, &t->s_components, es_link) {
    c = htsmsg_create_map();

    htsmsg_add_u32(c, "pid", st->es_pid);

    htsmsg_add_str(c, "type", streaming_component_type2txt(st->es_type));

    switch(st->es_type) {
    default:
      htsmsg_add_str(c, "details", "");
      break;

    case SCT_CA:
      buf[0] = 0;

      LIST_FOREACH(ca, &st->es_caids, link) {
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), 
		 "%s (0x%04x) [0x%08x]",
		 psi_caid2name(ca->caid), ca->caid, ca->providerid);
      }

      htsmsg_add_str(c, "details", buf);
      break;

    case SCT_AC3:
    case SCT_MP4A:
    case SCT_AAC:
    case SCT_MPEG2AUDIO:
      htsmsg_add_str(c, "details", st->es_lang);
      break;

    case SCT_DVBSUB:
      snprintf(buf, sizeof(buf), "%s (%04x %04x)",
	       st->es_lang, st->es_composition_id, st->es_ancillary_id);
      htsmsg_add_str(c, "details", buf);
      break;

    case SCT_MPEG2VIDEO:
    case SCT_H264:
      buf[0] = 0;
      if(st->es_frame_duration)
	snprintf(buf, sizeof(buf), "%2.2f Hz",
		 90000.0 / st->es_frame_duration);
      htsmsg_add_str(c, "details", buf);
      break;
    }

    htsmsg_add_msg(streams, NULL, c);
  }

  out = htsmsg_create_map();
  htsmsg_add_str(out, "title", t->s_svcname ?: "unnamed service");

  htsmsg_add_msg(out, "streams", streams);

  if(t->s_dvb_charset != NULL)
    htsmsg_add_str(out, "dvb_charset", t->s_dvb_charset);

  htsmsg_add_u32(out, "dvb_eit_enable", t->s_dvb_eit_enable);

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
static int
extjs_mergechannel(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *target = http_arg_get(&hc->hc_req_args, "targetID");
  htsmsg_t *out;
  channel_t *src, *dst;

  if(remain == NULL || target == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  src = channel_find_by_identifier(atoi(remain));
  dst = channel_find_by_identifier(atoi(target));

  if(src == NULL || dst == NULL) {
    pthread_mutex_unlock(&global_lock);
    return 404;
  }

  out = htsmsg_create_map();

  if(src != dst) {
    channel_merge(dst, src);
    htsmsg_add_u32(out, "success", 1);
  } else {

    htsmsg_add_u32(out, "success", 0);
    htsmsg_add_str(out, "msg", "Target same as source");
  }

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
static void
service_update_iptv(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  service_t *t;
  uint32_t u32;
  const char *id, *s;
  int save;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = service_find_by_identifier(id)) == NULL)
      continue;

    save = 0;

    if(!htsmsg_get_u32(c, "port", &u32)) {
      t->s_iptv_port = u32;
      save = 1;
    }

    if (!htsmsg_get_u32(c, "stype", &u32)) {
      t->s_servicetype = u32;
      save = 1;
    }

    if((s = htsmsg_get_str(c, "group")) != NULL) {
      if(!inet_pton(AF_INET, s, &t->s_iptv_group.s_addr)){
      	inet_pton(AF_INET6, s, &t->s_iptv_group6.s6_addr);
      }
      save = 1;
    }
    

    save |= tvh_str_update(&t->s_iptv_iface, htsmsg_get_str(c, "interface"));
    if(save)
      t->s_config_save(t); // Save config
  }
}

/**
 *
 */
static htsmsg_t *
build_record_iptv(service_t *t)
{
  htsmsg_t *r = htsmsg_create_map();
  char abuf[INET_ADDRSTRLEN];
  char abuf6[INET6_ADDRSTRLEN];
  htsmsg_add_str(r, "id", t->s_identifier);

  htsmsg_add_str(r, "channelname", t->s_ch ? t->s_ch->ch_name : "");
  htsmsg_add_str(r, "interface", t->s_iptv_iface ?: "");

  if(t->s_iptv_group.s_addr != 0){
    inet_ntop(AF_INET, &t->s_iptv_group, abuf, sizeof(abuf));
    htsmsg_add_str(r, "group", t->s_iptv_group.s_addr ? abuf : "");
  }
  else {
    inet_ntop(AF_INET6, &t->s_iptv_group6, abuf6, sizeof(abuf6));
    htsmsg_add_str(r, "group", t->s_iptv_group6.s6_addr ? abuf6 : "");
  }

  htsmsg_add_u32(r, "port", t->s_iptv_port);
  htsmsg_add_u32(r, "stype", t->s_servicetype);
  htsmsg_add_u32(r, "enabled", t->s_enabled);
  return r;
}

/**
 *
 */
static int
iptv_servicecmp(const void *A, const void *B)
{
  service_t *a = *(service_t **)A;
  service_t *b = *(service_t **)B;

  return memcmp(&a->s_iptv_group, &b->s_iptv_group, 4);
}

/**
 *
 */
static int
extjs_iptvservices(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *in, *array;
  const char *op        = http_arg_get(&hc->hc_req_args, "op");
  const char *entries   = http_arg_get(&hc->hc_req_args, "entries");
  service_t *t, **tvec;
  int count = 0, i = 0;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  in = entries != NULL ? htsmsg_json_deserialize(entries) : NULL;

  if(!strcmp(op, "get")) {
    LIST_FOREACH(t, &iptv_all_services, s_group_link)
      count++;
    tvec = alloca(sizeof(service_t *) * count);
    LIST_FOREACH(t, &iptv_all_services, s_group_link)
      tvec[i++] = t;

    out = htsmsg_create_map();
    array = htsmsg_create_list();

    qsort(tvec, count, sizeof(service_t *), iptv_servicecmp);

    for(i = 0; i < count; i++)
      htsmsg_add_msg(array, NULL, build_record_iptv(tvec[i]));

    htsmsg_add_msg(out, "entries", array);

  } else if(!strcmp(op, "update")) {
    if(in != NULL) {
      service_update(in);      // Generic service parameters
      service_update_iptv(in); // IPTV speicifc
    }

    out = htsmsg_create_map();

  } else if(!strcmp(op, "create")) {

    out = build_record_iptv(iptv_service_find(NULL, 1));

  } else if(!strcmp(op, "delete")) {
    if(in != NULL)
      extjs_service_delete(in);
    
    out = htsmsg_create_map();

  } else if (!strcmp(op, "servicetypeList")) {
    out   = htsmsg_create_map();
    array = servicetype_list();
    htsmsg_add_msg(out, "entries", array);

  } else {
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(in);
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_destroy(in);

  pthread_mutex_unlock(&global_lock);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
void
extjs_service_update(htsmsg_t *in)
{
  htsmsg_field_t *f;
  htsmsg_t *c;
  service_t *t;
  uint32_t u32;
  const char *id;
  const char *chname;
  const char *dvb_charset;

  TAILQ_FOREACH(f, &in->hm_fields, hmf_link) {
    if((c = htsmsg_get_map_by_field(f)) == NULL ||
       (id = htsmsg_get_str(c, "id")) == NULL)
      continue;
    
    if((t = service_find_by_identifier(id)) == NULL)
      continue;

    if(!htsmsg_get_u32(c, "enabled", &u32))
      service_set_enable(t, u32);

    if((chname = htsmsg_get_str(c, "channelname")) != NULL) 
      service_map_channel(t, channel_find_by_name(chname, 1, 0), 1);

    if((dvb_charset = htsmsg_get_str(c, "dvb_charset")) != NULL)
      service_set_dvb_charset(t, dvb_charset);

    if(!htsmsg_get_u32(c, "dvb_eit_enable", &u32))
      service_set_dvb_eit_enable(t, u32);
  }
}

/**
 *
 */
static int
extjs_tvadapter(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  htsmsg_t *out, *array;

  pthread_mutex_lock(&global_lock);

  /* Just list all adapters */
  array = htsmsg_create_list();

#if ENABLE_LINUXDVB
  extjs_list_dvb_adapters(array);
#endif

#if ENABLE_V4L
  extjs_list_v4l_adapters(array);
#endif

  pthread_mutex_unlock(&global_lock);
  out = htsmsg_create_map();
  htsmsg_add_msg(out, "entries", array);

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");
  return 0;
}

/**
 *
 */
static int
extjs_config(http_connection_t *hc, const char *remain, void *opaque)
{
  htsbuf_queue_t *hq = &hc->hc_reply;
  const char *op = http_arg_get(&hc->hc_req_args, "op");
  htsmsg_t *out, *m;
  const char *str;

  if(op == NULL)
    return 400;

  pthread_mutex_lock(&global_lock);

  if(http_access_verify(hc, ACCESS_ADMIN)) {
    pthread_mutex_unlock(&global_lock);
    return HTTP_STATUS_UNAUTHORIZED;
  }

  pthread_mutex_unlock(&global_lock);

  /* Basic settings (not the advanced schedule) */
  if(!strcmp(op, "loadSettings")) {
    pthread_mutex_lock(&global_lock);
    m = config_get_all();
    pthread_mutex_unlock(&global_lock);
    if (!m) return HTTP_STATUS_BAD_REQUEST;
    out = json_single_record(m, "config");

  /* Save settings */
  } else if (!strcmp(op, "saveSettings") ) {
    int save = 0;
    pthread_mutex_lock(&global_lock);
    if ((str = http_arg_get(&hc->hc_req_args, "muxconfpath")))
      save |= config_set_muxconfpath(str);
    if ((str = http_arg_get(&hc->hc_req_args, "language")))
      save |= config_set_language(str);
    if (save) config_save();
    pthread_mutex_unlock(&global_lock);
    out = htsmsg_create_map();
    htsmsg_add_u32(out, "success", 1);

  } else {
    return HTTP_STATUS_BAD_REQUEST;
  }

  htsmsg_json_serialize(out, hq, 0);
  htsmsg_destroy(out);
  http_output_content(hc, "text/x-json; charset=UTF-8");

  return 0;
}

/**
 * WEB user interface
 */
void
extjs_start(void)
{
  http_path_add("/about.html",     NULL, page_about,           ACCESS_WEB_INTERFACE);
  http_path_add("/extjs.html",     NULL, extjs_root,           ACCESS_WEB_INTERFACE);
  http_path_add("/tablemgr",       NULL, extjs_tablemgr,       ACCESS_WEB_INTERFACE);
  http_path_add("/channels",       NULL, extjs_channels,       ACCESS_WEB_INTERFACE);
  http_path_add("/epggrab",        NULL, extjs_epggrab,        ACCESS_WEB_INTERFACE);
  http_path_add("/channeltags",    NULL, extjs_channeltags,    ACCESS_WEB_INTERFACE);
  http_path_add("/confignames",    NULL, extjs_confignames,    ACCESS_WEB_INTERFACE);
  http_path_add("/epg",            NULL, extjs_epg,            ACCESS_WEB_INTERFACE);
  http_path_add("/epgrelated",     NULL, extjs_epgrelated,     ACCESS_WEB_INTERFACE);
  http_path_add("/epgobject",      NULL, extjs_epgobject,      ACCESS_WEB_INTERFACE);
  http_path_add("/dvr",            NULL, extjs_dvr,            ACCESS_WEB_INTERFACE);
  http_path_add("/dvrlist",        NULL, extjs_dvrlist,        ACCESS_WEB_INTERFACE);
  http_path_add("/subscriptions",  NULL, extjs_subscriptions,  ACCESS_WEB_INTERFACE);
  http_path_add("/ecglist",        NULL, extjs_ecglist,        ACCESS_WEB_INTERFACE);
  http_path_add("/config",         NULL, extjs_config,         ACCESS_WEB_INTERFACE);
  http_path_add("/languages",      NULL, extjs_languages,      ACCESS_WEB_INTERFACE);
  http_path_add("/mergechannel",   NULL, extjs_mergechannel,   ACCESS_ADMIN);
  http_path_add("/iptv/services",  NULL, extjs_iptvservices,   ACCESS_ADMIN);
  http_path_add("/servicedetails", NULL, extjs_servicedetails, ACCESS_ADMIN);
  http_path_add("/tv/adapter",     NULL, extjs_tvadapter,      ACCESS_ADMIN);

#if ENABLE_LINUXDVB
  extjs_start_dvb();
#endif

#if ENABLE_V4L
  extjs_start_v4l();
#endif
}
