/*
 *  tvheadend, AJAX / HTML user interface
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

#include "tvhead.h"
#include "http.h"
#include "ajaxui.h"
#include "channels.h"



/**
 * Render a channel group widget
 */
static void
ajax_chgroup_build(tcp_queue_t *tq, channel_group_t *tcg)
{
  tcp_qprintf(tq, "<li id=\"chgrp_%d\">", tcg->tcg_tag);
  
  ajax_box_begin(tq, AJAX_BOX_BORDER, NULL, NULL, NULL);
  
  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
  
  tcp_qprintf(tq,
	      "<div style=\"float: left; width: 60%\">"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"$('cheditortab').innerHTML=''; "
	      "new Ajax.Updater('groupeditortab', "
	      "'/ajax/chgroup_editor/%d', "
	      "{method: 'get', evalScripts: true})\" >"
	      "%s</a></div>",
	      tcg->tcg_tag, tcg->tcg_name);


  if(tcg != defgroup) {
    tcp_qprintf(tq,
		"<div style=\"float: left; width: 40%\" "
		"class=\"chgroupaction\">"
		"<a href=\"javascript:void(0)\" "
		"onClick=\"dellistentry('/ajax/chgroup_del','%d', '%s');\""
		">Delete</a></div>",
		tcg->tcg_tag, tcg->tcg_name);
  }
  

  tcp_qprintf(tq, "</div>");
  ajax_box_end(tq, AJAX_BOX_BORDER);
  tcp_qprintf(tq, "</li>");
}

/**
 * Update order of channel groups
 */
static int
ajax_chgroup_updateorder(http_connection_t *hc, http_reply_t *hr, 
			 const char *remain, void *opaque)
{
  channel_group_t *tcg;
  tcp_queue_t *tq = &hr->hr_tq;
  http_arg_t *ra;

  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    if(strcmp(ra->key, "channelgrouplist[]") ||
       (tcg = channel_group_by_tag(atoi(ra->val))) == NULL)
      continue;
    
    TAILQ_REMOVE(&all_channel_groups, tcg, tcg_global_link);
    TAILQ_INSERT_TAIL(&all_channel_groups, tcg, tcg_global_link);
  }

  channel_group_settings_write();

  tcp_qprintf(tq, "<span id=\"updatedok\">Updated on server</span>");
  ajax_js(tq, "Effect.Fade('updatedok')");
  http_output_html(hc, hr);
  return 0;
}



/**
 * Add a new channel group
 */
static int
ajax_chgroup_add(http_connection_t *hc, http_reply_t *hr, 
		 const char *remain, void *opaque)
{
  channel_group_t *tcg;
  tcp_queue_t *tq = &hr->hr_tq;
  const char *name;
  
  if((name = http_arg_get(&hc->hc_req_args, "name")) != NULL) {

    TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link)
      if(!strcmp(name, tcg->tcg_name))
	break;

    if(tcg == NULL) {
      tcg = channel_group_find(name, 1);

      ajax_chgroup_build(tq, tcg);

      /* We must recreate the Sortable object */

      ajax_js(tq, "Sortable.destroy(\"channelgrouplist\")");

      ajax_js(tq, "Sortable.create(\"channelgrouplist\", "
	      "{onUpdate:function(){updatelistonserver("
	      "'channelgrouplist', "
	      "'/ajax/chgroup_updateorder', "
	      "'list-info'"
	      ")}});");
    }
  }
  http_output_html(hc, hr);
  return 0;
}



/**
 * Delete a channel group
 */
static int
ajax_chgroup_del(http_connection_t *hc, http_reply_t *hr, 
		 const char *remain, void *opaque)
{
  channel_group_t *tcg;
  tcp_queue_t *tq = &hr->hr_tq;
  const char *id;
  
  if((id = http_arg_get(&hc->hc_req_args, "id")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((tcg = channel_group_by_tag(atoi(id))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  tcp_qprintf(tq, "$('chgrp_%d').remove();", tcg->tcg_tag);
  http_output(hc, hr, "text/javascript; charset=UTF-8", NULL, 0);

  channel_group_destroy(tcg);
  return 0;
}



/**
 * Channel group & channel configuration
 */
int
ajax_config_channels_tab(http_connection_t *hc, http_reply_t *hr)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_group_t *tcg;

  tcp_qprintf(tq, "<div style=\"float: left; width: 30%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, "channelgroups",
		 NULL, "Channel groups");

  tcp_qprintf(tq, "<div style=\"height:15px; text-align:center\" "
	      "id=\"list-info\"></div>");
   
  tcp_qprintf(tq, "<ul id=\"channelgrouplist\" class=\"draglist\">");

  TAILQ_FOREACH(tcg, &all_channel_groups, tcg_global_link) {
    if(tcg->tcg_hidden)
      continue;
    ajax_chgroup_build(tq, tcg);
  }

  tcp_qprintf(tq, "</ul>");
 
  ajax_js(tq, "Sortable.create(\"channelgrouplist\", "
	  "{onUpdate:function(){updatelistonserver("
	  "'channelgrouplist', "
	  "'/ajax/chgroup_updateorder', "
	  "'list-info'"
	  ")}});");

  /**
   * Add new group
   */

  tcp_qprintf(tq, "<hr>");

  ajax_box_begin(tq, AJAX_BOX_BORDER, NULL, NULL, NULL);

  tcp_qprintf(tq,
	      "<div style=\"height: 20px\">"
	      "<div style=\"float: left\">"
	      "<input class=\"textinput\" type=\"text\" id=\"newchgrp\">"
	      "</div>"
	      "<div style=\"padding-top:2px\" class=\"chgroupaction\">"
	      "<a href=\"javascript:void(0)\" "
	      "onClick=\"javascript:addlistentry_by_widget("
	      "'channelgrouplist', 'chgroup_add', 'newchgrp');\">"
	      "Add</a></div>"
	      "</div>");
    
  ajax_box_end(tq, AJAX_BOX_BORDER);
    
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  tcp_qprintf(tq, "</div>");

  tcp_qprintf(tq, 
	      "<div id=\"groupeditortab\" "
	      "style=\"overflow: auto; float: left; width: 30%\"></div>");

  tcp_qprintf(tq, 
	      "<div id=\"cheditortab\" "
	      "style=\"overflow: auto; float: left; width: 40%\"></div>");

  http_output_html(hc, hr);
  return 0;
}

/**
 * Display all channels within the group
 */
static int
ajax_chgroup_editor(http_connection_t *hc, http_reply_t *hr,
		    const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_t *ch;
  channel_group_t *tcg, *tcg2;
  int rowcol = 1;
  int disprows;

  if(remain == NULL || (tcg = channel_group_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  tcp_qprintf(tq, "<script type=\"text/javascript\">\r\n"
	      "//<![CDATA[\r\n");
  
  /* Select all */
  tcp_qprintf(tq, "select_all = function() {\r\n");
  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    tcp_qprintf(tq, 
		"$('sel_%d').checked = true;\r\n",
		ch->ch_tag);
  }
  tcp_qprintf(tq, "}\r\n");

  /* Select none */
  tcp_qprintf(tq, "select_none = function() {\r\n");
  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    tcp_qprintf(tq, 
		"$('sel_%d').checked = false;\r\n",
		ch->ch_tag);
  }
  tcp_qprintf(tq, "}\r\n");

  /* Invert selection */
  tcp_qprintf(tq, "select_invert = function() {\r\n");
  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    tcp_qprintf(tq, 
		"$('sel_%d').checked = !$('sel_%d').checked;\r\n",
		ch->ch_tag, ch->ch_tag);
  }
  tcp_qprintf(tq, "}\r\n");


  /* Invoke AJAX call containing all selected elements */
  tcp_qprintf(tq, 
	      "select_do = function(op, arg1, arg2) {\r\n"
	      "var h = new Hash();\r\n"
	      "h.set('arg1', arg1);\r\n"
	      "h.set('arg2', arg2);\r\n"
	      );
  
  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    tcp_qprintf(tq, 
		"if($('sel_%d').checked) {h.set('%d', 'selected') }\r\n",
		ch->ch_tag, ch->ch_tag);
  }

  tcp_qprintf(tq, " new Ajax.Request('/ajax/chop/' + op, "
	      "{parameters: h});\r\n");
  tcp_qprintf(tq, "}\r\n");


  tcp_qprintf(tq, 
	      "\r\n//]]>\r\n"
	      "</script>\r\n");


  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, tcg->tcg_name);

  tcp_qprintf(tq, "<div style=\"width: 100%%\">"
	      "Channels</div><hr>");

  disprows = 30;

  tcp_qprintf(tq, "<div id=\"chlist\" "
	      "style=\"height: %dpx; overflow: auto\" class=\"normallist\">",
	      disprows * 14);

  TAILQ_FOREACH(ch, &tcg->tcg_channels, ch_group_link) {
    tcp_qprintf(tq,
		"<div style=\"%swidth: 100%%; overflow: auto\">"
		"<div style=\"float: left; width: 90%\">"
		"<a href=\"javascript:void(0)\" "
		"onclick=\"new Ajax.Updater('cheditortab', "
		"'/ajax/cheditor/%d', {method: 'get'})\""
		">%s</a>"
		"</div>"
		"<input id=\"sel_%d\" type=\"checkbox\" class=\"nicebox\">"
		"</div>",
		rowcol ? "background: #fff; " : "",
		ch->ch_tag, ch->ch_name, ch->ch_tag);
    rowcol = !rowcol;
  }

  tcp_qprintf(tq, "</div>");
  tcp_qprintf(tq, "<hr>\r\n");
  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
  tcp_qprintf(tq, "<div class=\"infoprefixwide\">Select:</div><div>");

  ajax_a_jsfuncf(tq, "All",    "select_all();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "None",   "select_none();");
  tcp_qprintf(tq, " / ");
  ajax_a_jsfuncf(tq, "Invert", "select_invert();");

  tcp_qprintf(tq, "</div></div>\r\n");


  tcp_qprintf(tq, "<div style=\"margin-top: 5px; "
	      "overflow: auto; width: 100%\">");

  tcp_qprintf(tq,
	      "<div class=\"infoprefixwidefat\">Move to group:</div>"
	      "<div>"
	      "<select id=\"movetarget\" class=\"textinput\" "
	      "onChange=\"select_do('changegroup', "
	      "$('movetarget').value, '%d')\">", tcg->tcg_tag);
  tcp_qprintf(tq, "<option value="">Select group</option>");

  TAILQ_FOREACH(tcg2, &all_channel_groups, tcg_global_link) {
    if(tcg2->tcg_hidden || tcg == tcg2)
      continue;
    tcp_qprintf(tq, "<option value=\"%d\">%s</option>",
		tcg2->tcg_tag, tcg2->tcg_name);
  }
  tcp_qprintf(tq, "</select></div>");
  tcp_qprintf(tq, "</div>");
  tcp_qprintf(tq, "</div>");
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);


  http_output_html(hc, hr);
  return 0;
}


/**
 *
 */
static struct strtab sourcetypetab[] = {
  { "DVB",        TRANSPORT_DVB },
  { "V4L",        TRANSPORT_V4L },
  { "IPTV",       TRANSPORT_IPTV },
  { "AVgen",      TRANSPORT_AVGEN },
  { "File",       TRANSPORT_STREAMEDFILE },
};


static struct strtab cdlongname[] = {
  { "None",                  COMMERCIAL_DETECT_NONE },
  { "Swedish TV4 Teletext",  COMMERCIAL_DETECT_TTP192 },
};

/**
 * Display all channels within the group
 */
static int
ajax_cheditor(http_connection_t *hc, http_reply_t *hr,
	      const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_t *ch;
  th_transport_t *t;
  const char *s;
  int i;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, ch->ch_name);
  
  tcp_qprintf(tq, "<div>Sources:</div>");

  LIST_FOREACH(t, &ch->ch_transports, tht_channel_link) {
    ajax_box_begin(tq, AJAX_BOX_BORDER, NULL, NULL, NULL);
    tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
    tcp_qprintf(tq, "<div style=\"float: left; width: 13%%\">%s</div>",
		val2str(t->tht_type, sourcetypetab) ?: "???");
    tcp_qprintf(tq, "<div style=\"float: left; width: 87%%\">\"%s\"%s</div>",
		t->tht_servicename, t->tht_scrambled ? " - (scrambled)" : "");
    s = t->tht_sourcename ? t->tht_sourcename(t) : NULL;

    tcp_qprintf(tq, "</div><div style=\"overflow: auto; width: 100%\">");

    tcp_qprintf(tq,
		"<div style=\"float: left; width: 13%%\">"
		"<input %stype=\"checkbox\" class=\"nicebox\" "
		"onClick=\"new Ajax.Request('/ajax/transport_chdisable/%s', "
		"{parameters: {enabled: this.checked}});\">"
		"</div>", t->tht_disabled ? "" : "checked ",
		t->tht_identifier);

    if(s != NULL)
      tcp_qprintf(tq, "<div style=\"float: left; width: 87%%\">%s</div>",
		  s);

    tcp_qprintf(tq, "</div>");

    ajax_box_end(tq, AJAX_BOX_BORDER);
  }

  tcp_qprintf(tq, "<hr>\r\n");

  tcp_qprintf(tq, "<div style=\"overflow: auto; width:100%%\">");

  tcp_qprintf(tq, 
	      "<input type=\"button\" value=\"Rename...\" "
	      "onClick=\"channel_rename('%d', '%s')\">",
	      ch->ch_tag, ch->ch_name);

  tcp_qprintf(tq, 
	      "<input type=\"button\" value=\"Delete...\" "
	      "onClick=\"channel_delete('%d', '%s')\">",
	      ch->ch_tag, ch->ch_name);

  tcp_qprintf(tq, "</div>");

  tcp_qprintf(tq, "<hr>\r\n");

  tcp_qprintf(tq,
	      "<div class=\"infoprefixwidewidefat\">"
	      "Commercial detection:</div>"
	      "<div>"
	      "<select class=\"textinput\" "
	      "onChange=\"new Ajax.Request('/ajax/chsetcomdetect/%d', "
	      "{parameters: {how: this.value}});\">",
	      ch->ch_tag);

  for(i = 0; i < sizeof(cdlongname) / sizeof(cdlongname[0]); i++) {
    tcp_qprintf(tq, "<option %svalue=%d>%s</option>",
		cdlongname[i].val == ch->ch_commercial_detection ? 
		"selected " : "",
		cdlongname[i].val, cdlongname[i].str);
  }
  tcp_qprintf(tq, "</select></div>");
  tcp_qprintf(tq, "</div>");


  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  http_output_html(hc, hr);
  return 0;
}

/**
 * Change group for channel(s)
 */
static int
ajax_changegroup(http_connection_t *hc, http_reply_t *hr,
		 const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_t *ch;
  channel_group_t *tcg;
  http_arg_t *ra;
  const char *s;
  const char *curgrp;

  if((s = http_arg_get(&hc->hc_req_args, "arg1")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((curgrp = http_arg_get(&hc->hc_req_args, "arg2")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  tcg = channel_group_by_tag(atoi(s));
  if(tcg == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    if(strcmp(ra->val, "selected"))
      continue;
    
    if((ch = channel_by_tag(atoi(ra->key))) != NULL)
      channel_set_group(ch, tcg);
  }

  tcp_qprintf(tq,
	      "$('cheditortab').innerHTML=''; "
	      "new Ajax.Updater('groupeditortab', "
	      "'/ajax/chgroup_editor/%s', "
	      "{method: 'get', evalScripts: true});", curgrp);
  
  http_output(hc, hr, "text/javascript; charset=UTF-8", NULL, 0);
  return 0;
}

/**
 * Change commercial detection type for channel(s)
 */
static int
ajax_chsetcomdetect(http_connection_t *hc, http_reply_t *hr,
		    const char *remain, void *opaque)
{
  channel_t *ch;
  const char *s;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;
  
  if((s = http_arg_get(&hc->hc_req_args, "how")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;
  
  ch->ch_commercial_detection = atoi(s);

  channel_settings_write(ch);
  http_output(hc, hr, "text/javascript; charset=UTF-8", NULL, 0);
  return 0;
}


/**
 * Rename a channel
 */
static int
ajax_chrename(http_connection_t *hc, http_reply_t *hr,
	      const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_t *ch;
  const char *s;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;
  
  if((s = http_arg_get(&hc->hc_req_args, "newname")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if(channel_rename(ch, s)) {
    tcp_qprintf(tq, "alert('Channel already exist');");
  } else {
    tcp_qprintf(tq, 
		"new Ajax.Updater('groupeditortab', "
		"'/ajax/chgroup_editor/%d', "
		"{method: 'get', evalScripts: true});\r\n",
		ch->ch_group->tcg_tag);
 
    tcp_qprintf(tq, 
		"new Ajax.Updater('cheditortab', "
		"'/ajax/cheditor/%d', "
		"{method: 'get', evalScripts: true});\r\n",
		ch->ch_tag);
  }

  http_output(hc, hr, "text/javascript; charset=UTF-8", NULL, 0);
  return 0;
}


/**
 * Delete channel
 */
static int
ajax_chdelete(http_connection_t *hc, http_reply_t *hr,
	      const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  channel_t *ch;
  channel_group_t *tcg;

  if(remain == NULL || (ch = channel_by_tag(atoi(remain))) == NULL)
    return HTTP_STATUS_BAD_REQUEST;
  
  tcg = ch->ch_group;
  
  channel_delete(ch);

  tcp_qprintf(tq, 
	      "new Ajax.Updater('groupeditortab', "
	      "'/ajax/chgroup_editor/%d', "
	      "{method: 'get', evalScripts: true});\r\n",
	      tcg->tcg_tag);
 
  tcp_qprintf(tq, "$('cheditortab').innerHTML='';\r\n");

  http_output(hc, hr, "text/javascript; charset=UTF-8", NULL, 0);
  return 0;
}


/**
 *
 */
void
ajax_config_channels_init(void)
{
  http_path_add("/ajax/chgroup_add"        , NULL, ajax_chgroup_add,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chgroup_del"        , NULL, ajax_chgroup_del,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chgroup_updateorder", NULL, ajax_chgroup_updateorder,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chgroup_editor",      NULL, ajax_chgroup_editor,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/cheditor",            NULL, ajax_cheditor,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chop/changegroup",    NULL, ajax_changegroup,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chsetcomdetect",      NULL, ajax_chsetcomdetect,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chrename",            NULL, ajax_chrename,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/chdelete",            NULL, ajax_chdelete,
		AJAX_ACCESS_CONFIG);

}
