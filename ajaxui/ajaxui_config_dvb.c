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

#define _GNU_SOURCE

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "http.h"
#include "ajaxui.h"
#include "channels.h"
#include "dvb.h"
#include "dvb_support.h"
#include "dvb_muxconfig.h"
#include "psi.h"
#include "transports.h"
#include "dispatch.h"

#include "ajaxui_mailbox.h"


static void
add_option(htsbuf_queue_t *tq, int bol, const char *name)
{
  if(bol)
    htsbuf_qprintf(tq, "<option>%s</option>", name);
}

/**
 *
 */
static const char *
nicenum(unsigned int v)
{
  static char buf[4][30];
  static int ptr;
  char *x;
  ptr = (ptr + 1) & 3;
  x = buf[ptr];

  if(v < 1000)
    snprintf(x, 30, "%d", v);
  else if(v < 1000000)
    snprintf(x, 30, "%d,%03d", v / 1000, v % 1000);
  else if(v < 1000000000)
    snprintf(x, 30, "%d,%03d,%03d", 
	     v / 1000000, (v % 1000000) / 1000, v % 1000);
  else 
    snprintf(x, 30, "%d,%03d,%03d,%03d", 
	     v / 1000000000, (v % 1000000000) / 1000000,
	     (v % 1000000) / 1000, v % 1000);
  return x;
}



static void
tdmi_displayname(th_dvb_mux_instance_t *tdmi, char *buf, size_t len)
{
  int f = tdmi->tdmi_fe_params.frequency;

  if(tdmi->tdmi_adapter->tda_type == FE_QPSK) {
    snprintf(buf, len, "%s kHz %s", nicenum(f),
	     dvb_polarisation_to_str(tdmi->tdmi_polarisation));
  } else {
    snprintf(buf, len, "%s kHz", nicenum(f / 1000));
  }
}

/*
 * Adapter summary
 */
static int
ajax_adaptersummary(http_connection_t *hc, http_reply_t *hr, 
		    const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda;
  char dispname[20];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  snprintf(dispname, sizeof(dispname), "%s", tda->tda_displayname);
  strcpy(dispname + sizeof(dispname) - 4, "...");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, dispname);

  htsbuf_qprintf(tq, "<div class=\"infoprefix\">Device:</div>"
	      "<div>%s</div>",
	      tda->tda_rootpath ?: "<b><i>Not present</i></b>");
  htsbuf_qprintf(tq, "<div class=\"infoprefix\">Type:</div>"
	      "<div>%s</div>", 
	      dvb_adaptertype_to_str(tda->tda_type));
 
  htsbuf_qprintf(tq, "<div style=\"text-align: center\">"
	      "<a href=\"javascript:void(0);\" "
	      "onClick=\"new Ajax.Updater('dvbadaptereditor', "
	      "'/ajax/dvbadaptereditor/%s', "
	      "{method: 'get', evalScripts: true})\""
	      "\">Edit</a></div>", tda->tda_identifier);
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  http_output_html(hc, hr);
  return 0;
}


/**
 * DVB configuration
 */
int
ajax_config_dvb_tab(http_connection_t *hc, http_reply_t *hr)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda;

  htsbuf_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  if(TAILQ_FIRST(&dvb_adapters) == NULL) {
    htsbuf_qprintf(tq, "<div style=\"text-align: center; font-weight: bold\">"
		"No adapters found</div>");
  }


  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {

    htsbuf_qprintf(tq, "<div id=\"summary_%s\" "
		"style=\"float:left; width: 250px\"></div>",
		tda->tda_identifier);

    ajax_js(tq, "new Ajax.Updater('summary_%s', "
	    "'/ajax/dvbadaptersummary/%s', {method: 'get'})",
	    tda->tda_identifier, tda->tda_identifier);

  }
  htsbuf_qprintf(tq, "</div>");
  htsbuf_qprintf(tq, "<div id=\"dvbadaptereditor\"></div>");
  http_output_html(hc, hr);
  return 0;
}



/**
 * DVB adapter editor pane
 */
static int
ajax_adaptereditor(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda, *tda2;
  const char *s;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_box_begin(tq, AJAX_BOX_FILLED, NULL, NULL, NULL);

  htsbuf_qprintf(tq, 
	      "<div id=\"adaptername_%s\" "
	      "style=\"text-align: center; font-weight: bold\">%s</div>",
	      tda->tda_identifier, tda->tda_displayname);
  
  ajax_box_end(tq, AJAX_BOX_FILLED);

  /* Type */

  htsbuf_qprintf(tq, "<div style=\"overflow: auto; width:100%%\">");

  htsbuf_qprintf(tq, "<div style=\"float: left; width:45%%\">");

  htsbuf_qprintf(tq, "<div class=\"infoprefixwide\">Model:</div>"
	      "<div>%s (%s)</div>", 
	      tda->tda_fe_info ? tda->tda_fe_info->name : "<Unknown>",
	      dvb_adaptertype_to_str(tda->tda_type));
  


  if(tda->tda_fe_info != NULL) {

    s = tda->tda_type == FE_QPSK ? "kHz" : "Hz";
    htsbuf_qprintf(tq, "<div class=\"infoprefixwide\">Freq. Range:</div>"
		"<div>%s - %s %s, in steps of %s %s</div>",
		nicenum(tda->tda_fe_info->frequency_min),
		nicenum(tda->tda_fe_info->frequency_max),
		s,
		nicenum(tda->tda_fe_info->frequency_stepsize),
		s);


    if(tda->tda_fe_info->symbol_rate_min) {
      htsbuf_qprintf(tq, "<div class=\"infoprefixwide\">Symbolrate:</div>"
		  "<div>%s - %s Baud</div>",
		  nicenum(tda->tda_fe_info->symbol_rate_min),
		  nicenum(tda->tda_fe_info->symbol_rate_max));
    }
    /* Capabilities */
    //  htsbuf_qprintf(tq, "<div class=\"infoprefixwide\">Capabilities:</div>");
  }

  htsbuf_qprintf(tq, "</div>");

  htsbuf_qprintf(tq, "<div style=\"float: left; width:55%%\">");

  htsbuf_qprintf(tq, "<div style=\"overflow: auto; width:100%%\">");

  htsbuf_qprintf(tq, 
	      "<input type=\"button\" value=\"Rename adapter...\" "
	      "onClick=\"dvb_adapter_rename('%s', '%s');\">",
	      tda->tda_identifier, tda->tda_displayname);

  if(tda->tda_rootpath == NULL) {
    htsbuf_qprintf(tq, 
		"<input type=\"button\" value=\"Delete adapter...\" "
		"onClick=\"dvb_adapter_delete('%s', '%s');\">",
		tda->tda_identifier, tda->tda_displayname);
  }

  //  htsbuf_qprintf(tq, "</div>");

  /* Clone adapter */

  //  htsbuf_qprintf(tq, "<div style=\"overflow: auto; width:100%%\">");
  htsbuf_qprintf(tq,
	      "<select "
	      "onChange=\"new Ajax.Request('/ajax/dvbadapterclone/%s', "
	      "{parameters: { source: this.value }})\">",
	      tda->tda_identifier);
  
  htsbuf_qprintf(tq, "<option value=\"n\">Clone settings from adapter:</option>");

  TAILQ_FOREACH(tda2, &dvb_adapters, tda_global_link) {
    if(tda2 == tda || tda2->tda_type != tda->tda_type)
      continue;

    htsbuf_qprintf(tq, "<option value=\"%s\">%s (%s)</option>",
		tda2->tda_identifier, tda2->tda_displayname,
		tda2->tda_rootpath ?: "not present");
  }
  
  htsbuf_qprintf(tq, "</select></div>");
  htsbuf_qprintf(tq, "</div>");

  htsbuf_qprintf(tq, "</div>");

  htsbuf_qprintf(tq, "</div>");

  /* Muxes and transports */


  htsbuf_qprintf(tq, "<div style=\"float: left; width:45%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "Multiplexes");

  htsbuf_qprintf(tq, "<div id=\"dvbmuxlist_%s\"></div>",
	      tda->tda_identifier);

  ajax_js(tq, 
	  "new Ajax.Updater('dvbmuxlist_%s', "
	  "'/ajax/dvbadaptermuxlist/%s', {method: 'get', evalScripts: true})",
	  tda->tda_identifier, tda->tda_identifier);

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  htsbuf_qprintf(tq, "</div>");

  /* Div for displaying services */

  htsbuf_qprintf(tq, "<div id=\"servicepane\" style=\"float: left; width:55%\">");
  htsbuf_qprintf(tq, "</div>");

  http_output_html(hc, hr);
  return 0;
}


/**
 * DVB adapter add mux
 */
static int
ajax_adapteraddmux(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda;
  int caps;
  int fetype;
  char params[400];
  int n, type;
  const char *networkname;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(tda->tda_fe_info == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  caps   = tda->tda_fe_info->caps;
  fetype = tda->tda_fe_info->type;

  snprintf(params, sizeof(params), "Add new %s mux on \"%s\"", 
	   dvb_adaptertype_to_str(tda->tda_fe_info->type),
	   tda->tda_displayname);

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, params);

  /* Manual configuration */

  htsbuf_qprintf(tq,
	      "<div style=\"text-align: center; font-weight: bold\">"
	      "Manual configuartion</div>");

  htsbuf_qprintf(tq,
	      "<div class=\"cell_50\">"
	      "<div class=\"infoprefixwidewidefat\">Frequency (%s):</div>"
	      "<div>"
	      "<input type=\"text\" id=\"freq\">"
	      "</div></div>",
	      fetype == FE_QPSK ? "kHz" : "Hz");

  snprintf(params, sizeof(params), 
	   "freq: $F('freq')");

  /* Symbolrate */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
 
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Symbolrate:</div>"
		"<div>"
		"<input  type=\"text\" id=\"symrate\">"
		"</div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", symrate: $F('symrate')");
  }

  /* Bandwidth */

  if(fetype == FE_OFDM) {
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Bandwidth:</div>"
		"<div><select id=\"bw\">");

    add_option(tq, caps & FE_CAN_BANDWIDTH_AUTO, "AUTO");
    add_option(tq, 1                           , "8MHz");
    add_option(tq, 1                           , "7MHz");
    add_option(tq, 1                           , "6MHz");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", bw: $F('bw')");
  }


  /* Constellation */


  if(fetype == FE_QAM || fetype == FE_OFDM) {
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Constellation:</div>"
		"<div><select id=\"const\">");

    add_option(tq, caps & FE_CAN_QAM_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_QPSK,      "QPSK");
    add_option(tq, caps & FE_CAN_QAM_16,    "QAM16");
    add_option(tq, caps & FE_CAN_QAM_32,    "QAM32");
    add_option(tq, caps & FE_CAN_QAM_64,    "QAM64");
    add_option(tq, caps & FE_CAN_QAM_128,   "QAM128");
    add_option(tq, caps & FE_CAN_QAM_256,   "QAM256");

    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", const: $F('const')");
  }


  /* FEC */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">FEC:</div>"
		"<div><select id=\"fec\">");

    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fec: $F('fec')");
  }

  if(fetype == FE_QPSK) {
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Polarisation:</div>"
		"<div><select id=\"pol\">");

    add_option(tq, 1,  "Vertical");
    add_option(tq, 1,  "Horizontal");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", pol: $F('pol')");
    

  }


  if(fetype == FE_OFDM) {
    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Transmission mode:</div>"
		"<div><select id=\"tmode\">");
    
    add_option(tq, caps & FE_CAN_TRANSMISSION_MODE_AUTO, "AUTO");
    add_option(tq, 1                                   , "2k");
    add_option(tq, 1                                   , "8k");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", tmode: $F('tmode')");

    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Guard interval:</div>"
		"<div><select id=\"guard\">");
    
    add_option(tq, caps & FE_CAN_GUARD_INTERVAL_AUTO, "AUTO");
    add_option(tq, 1                                , "1/32");
    add_option(tq, 1                                , "1/16");
    add_option(tq, 1                                , "1/8");
    add_option(tq, 1                                , "1/4");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", guard: $F('guard')");



    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">Hierarchy:</div>"
		"<div><select id=\"hier\">");
    
    add_option(tq, caps & FE_CAN_HIERARCHY_AUTO, "AUTO");
    add_option(tq, 1                           , "1");
    add_option(tq, 1                           , "2");
    add_option(tq, 1                           , "4");
    add_option(tq, 1                           , "NONE");
    htsbuf_qprintf(tq, "</select></div></div>");


    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", hier: $F('hier')");



    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">FEC Hi:</div>"
		"<div><select id=\"fechi\">");
    
    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fechi: $F('fechi')");


    htsbuf_qprintf(tq,
		"<div class=\"cell_50\">"
		"<div class=\"infoprefixwidewidefat\">FEC Low:</div>"
		"<div><select id=\"feclo\">");
    
    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    htsbuf_qprintf(tq, "</select></div></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", feclo: $F('feclo')");
  }

  htsbuf_qprintf(tq,
	      "<br>"
	      "<div style=\"text-align: center\">"
	      "<input type=\"button\" value=\"Add manually configured mux\" "
	      "onClick=\"new Ajax.Request('/ajax/dvbadaptercreatemux/%s', "
	      "{parameters: {%s}})"
	      "\">"
	      "</div>", tda->tda_identifier, params);

  /*
   * Preconfigured
   */

  htsbuf_qprintf(tq,
	      "<hr>"
	      "<div style=\"text-align: center; font-weight: bold\">"
	      "Preconfigured network</div>");

  htsbuf_qprintf(tq,
	      "<div style=\"text-align: center\">"
	      "<select id=\"network\" "
	      "onChange=\"new Ajax.Updater('networkinfo', "
	      "'/ajax/dvbnetworkinfo/' + this.value)\""
	      ">");

  htsbuf_qprintf(tq, "<option>Select a network</option>");

  n = 0;
  while((type = dvb_mux_preconf_get(n, &networkname, NULL)) >= 0) {
 
    if(type == fetype)
      htsbuf_qprintf(tq, "<option value=%d>%s</option>", n, networkname);
    n++;
  }
  htsbuf_qprintf(tq, "</select></div>");

  htsbuf_qprintf(tq,
	      "<div class=\"cell_100_center\" id=\"networkinfo\"></div>");

  htsbuf_qprintf(tq,
	      "<br>"
	      "<div style=\"text-align: center\">"
	      "<input type=\"button\" value=\"Add preconfigured network\" "
	      "onClick=\"new Ajax.Updater('addnetwork', "
	      "'/ajax/dvbadapteraddnetwork/%s', "
	      "{evalScripts: true, parameters: "
	      "{'network': $('network').value}})"
	      "\">"
	      "</div>", tda->tda_identifier, params);

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  http_output_html(hc, hr);
  return 0;
}

/**
 * DVB adapter create mux (come here on POST after addmux query)
 */
static int
ajax_adaptercreatemux(http_connection_t *hc, http_reply_t *hr, 
		      const char *remain, void *opaque)
{
  htsbuf_queue_t *tq;
  th_dvb_adapter_t *tda;
  const char *v;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  v = dvb_mux_create_str(tda,
			 "65535",
			 NULL,
			 http_arg_get(&hc->hc_req_args, "freq"),
			 http_arg_get(&hc->hc_req_args, "symrate"),
			 http_arg_get(&hc->hc_req_args, "const"),
			 http_arg_get(&hc->hc_req_args, "fec"),
			 http_arg_get(&hc->hc_req_args, "fechi"),
			 http_arg_get(&hc->hc_req_args, "feclo"),
			 http_arg_get(&hc->hc_req_args, "bw"),
			 http_arg_get(&hc->hc_req_args, "tmode"),
			 http_arg_get(&hc->hc_req_args, "guard"),
			 http_arg_get(&hc->hc_req_args, "hier"),
			 http_arg_get(&hc->hc_req_args, "pol"),
			 http_arg_get(&hc->hc_req_args, "port"), 1);


  tq = &hr->hr_q;

  if(v != NULL)
    htsbuf_qprintf(tq, "alert('%s');\r\n", v);

  htsbuf_qprintf(tq, 
	      "$('servicepane').innerHTML='';\r\n");

  htsbuf_qprintf(tq, 
	      "new Ajax.Updater('dvbmuxlist_%s', "
	      "'/ajax/dvbadaptermuxlist/%s', "
	      "{method: 'get', evalScripts: true});\r\n",
	      tda->tda_identifier, tda->tda_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 * Return a list of all muxes on the given adapter
 */
static int
ajax_adaptermuxlist(http_connection_t *hc, http_reply_t *hr,
		    const char *remain, void *opaque)
{
  ajax_table_t ta;
  th_dvb_mux_instance_t *tdmi;
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda;
  char buf[200];
  int fetype, n, m;
  th_transport_t *t;
  int nmuxes = 0;
  char **selvector;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  fetype = tda->tda_type;

  /* List of muxes */
  
  nmuxes = tda->tda_muxes.entries;
  
  if(nmuxes == 0) {
    htsbuf_qprintf(tq, "<div style=\"text-align: center\">"
		"No muxes configured</div>");
  } else {


    selvector = alloca(sizeof(char *) * (nmuxes + 1));
    n = 0;
    RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      selvector[n++] = tdmi->tdmi_identifier;
    selvector[n] = NULL;

    ajax_generate_select_functions(tq, "mux", selvector);

    ajax_table_top(&ta, hc, tq,
		   (const char *[])
    {"Freq", "Status", "Quality", "State", "Name", "Services", "", NULL},
		   (int[])
    {16,12,7,8,16,8,2});

    RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {

      tdmi_displayname(tdmi, buf, sizeof(buf));

      ajax_table_row_start(&ta, tdmi->tdmi_identifier);

      ajax_table_cell(&ta, NULL,
		      "<a href=\"javascript:void(0);\" "
		      "onClick=\"new Ajax.Updater('servicepane', "
		      "'/ajax/dvbmuxeditor/%s', "
		      "{method: 'get', evalScripts: true})\""
		      ">%s</a>",
		      tdmi->tdmi_identifier, buf);

      ajax_table_cell(&ta, "status", "%s", dvb_mux_status(tdmi, 0));
      ajax_table_cell(&ta, "qual", "%d%%",
		      dvb_quality_to_percent(tdmi->tdmi_quality));
      ajax_table_cell(&ta, "state", "%s", dvb_mux_state(tdmi));
      ajax_table_cell(&ta, "name", "%s", tdmi->tdmi_network ?: "Unknown");

      n = m = 0;
      LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
	n++;
	if(transport_is_available(t))
	  m++;
      }
      ajax_table_cell(&ta, "nsvc", "%d / %d", m, n);
      ajax_table_cell_checkbox(&ta);
    }
    
    ajax_table_row_start(&ta, NULL);
    
    ajax_table_cell(&ta, NULL,
		    "<a href=\"javascript:void(0);\" "
		    "onClick=\"new Ajax.Updater('servicepane', "
		    "'/ajax/dvbmuxall/%s', "
		    "{method: 'get', evalScripts: true})\""
		    ">All</a>", tda->tda_identifier);

    ajax_table_bottom(&ta);

    htsbuf_qprintf(tq, "<hr><div style=\"overflow: auto; width: 100%\">");

    ajax_button(tq, "Select all",  "mux_sel_all()");
    ajax_button(tq, "Select none", "mux_sel_none()");
    ajax_button(tq, "Delete selected...", 
		   "mux_sel_do('dvbadapterdelmux/%s', '', '', true)",
		   tda->tda_identifier);
    htsbuf_qprintf(tq, "</div>\r\n");
  }
  if(tda->tda_fe_info != NULL) {
    htsbuf_qprintf(tq, "<hr><div style=\"overflow: auto; width: 100%\">");

    ajax_button(tq, "Add new mux...",  
		"new Ajax.Updater('servicepane', "
		"'/ajax/dvbadapteraddmux/%s', "
		"{method: 'get', evalScripts: true})\"",
		tda->tda_identifier);
    htsbuf_qprintf(tq, "</div>\r\n");
  }
  http_output_html(hc, hr);
  return 0;
}

/**
 *
 */
static int
dvbsvccmp(th_transport_t *a, th_transport_t *b)
{
  if(a->tht_dvb_mux_instance == b->tht_dvb_mux_instance)
    return a->tht_dvb_service_id - b->tht_dvb_service_id;
  
  return a->tht_dvb_mux_instance->tdmi_fe_params.frequency - 
    b->tht_dvb_mux_instance->tdmi_fe_params.frequency;
}

/**
 * Display detailes about a mux
 */
static int
ajax_dvbmuxeditor(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  th_dvb_mux_instance_t *tdmi;
  htsbuf_queue_t *tq = &hr->hr_q;
  char buf[1000];
  th_transport_t *t;
  struct th_transport_tree tree;
  int n = 0;

  if(remain == NULL || (tdmi = dvb_mux_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tdmi_displayname(tdmi, buf, sizeof(buf));

  RB_INIT(&tree);

  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    if(transport_is_available(t)) {
      RB_INSERT_SORTED(&tree, t, tht_tmp_link, dvbsvccmp);
      n++;
    }
  }

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, buf);
  ajax_transport_build_list(hc, tq, &tree, n);
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  http_output_html(hc, hr);
  return 0;
}

/**
 * Display all transports on an adapter
 */
static int
ajax_dvbmuxall(http_connection_t *hc, http_reply_t *hr, 
	       const char *remain, void *opaque)
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  htsbuf_queue_t *tq = &hr->hr_q;
  th_transport_t *t;
  struct th_transport_tree tree;
  int n = 0;
  char buf[100];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  snprintf(buf, sizeof(buf), "All services on %s\n", tda->tda_displayname); 

  RB_INIT(&tree);

  RB_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {
    LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
      if(transport_is_available(t)) {
	RB_INSERT_SORTED(&tree, t, tht_tmp_link, dvbsvccmp);
	n++;
      }
    }
  }

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, buf);
  ajax_transport_build_list(hc, tq, &tree, n);
  ajax_box_end(tq, AJAX_BOX_SIDEBOX);

  http_output_html(hc, hr);
  return 0;
}


/**
 * Delete muxes on an adapter
 */
static int
ajax_adapterdelmux(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  th_dvb_adapter_t *tda;
  th_dvb_mux_instance_t *tdmi;
  htsbuf_queue_t *tq = &hr->hr_q;
  http_arg_t *ra;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  TAILQ_FOREACH(ra, &hc->hc_req_args, link) {
    if(strcmp(ra->val, "selected"))
      continue;

    if((tdmi = dvb_mux_find_by_identifier(ra->key)) == NULL)
      continue;

    dvb_mux_destroy(tdmi);
  }
 
  htsbuf_qprintf(tq,
	      "new Ajax.Updater('dvbadaptereditor', "
	      "'/ajax/dvbadaptereditor/%s', "
	      "{method: 'get', evalScripts: true});",
	      tda->tda_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}

/**
 * Rename adapter
 */
static int
ajax_adapterrename(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  th_dvb_adapter_t *tda;

  htsbuf_queue_t *tq = &hr->hr_q;
  const char *s;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((s = http_arg_get(&hc->hc_req_args, "newname")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(s);
  dvb_tda_save(tda);
 
  htsbuf_qprintf(tq,
	      "$('adaptername_%s').innerHTML='%s';",
	      tda->tda_identifier, tda->tda_displayname);

  htsbuf_qprintf(tq,
	      "new Ajax.Updater('summary_%s', "
	      "'/ajax/dvbadaptersummary/%s', {method: 'get'})",
	    tda->tda_identifier, tda->tda_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 * Rename adapter
 */
static int
ajax_dvbnetworkinfo(http_connection_t *hc, http_reply_t *hr, 
		    const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  const char *s;

  if(remain == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(dvb_mux_preconf_get(atoi(remain), NULL, &s) >= 0)
    htsbuf_qprintf(tq, "%s", s);

  http_output_html(hc, hr);
  return 0;
}




/**
 * Rename adapter
 */
static int
ajax_dvbadapteraddnetwork(http_connection_t *hc, http_reply_t *hr, 
			  const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  const char *s;
  th_dvb_adapter_t *tda;


  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((s = http_arg_get(&hc->hc_req_args, "network")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;


  dvb_mux_preconf_add(tda, atoi(s));

  htsbuf_qprintf(tq, 
	      "$('servicepane').innerHTML='';\r\n");

  htsbuf_qprintf(tq, 
	      "new Ajax.Updater('dvbmuxlist_%s', "
	      "'/ajax/dvbadaptermuxlist/%s', "
	      "{method: 'get', evalScripts: true});\r\n",
	      tda->tda_identifier, tda->tda_identifier);


  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}

/**
 * Clone adapter
 */
static int
ajax_dvbadapterclone(http_connection_t *hc, http_reply_t *hr, 
		     const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *src, *dst;
  const char *s;

  if(remain == NULL || (dst = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;
  
  if((s = http_arg_get(&hc->hc_req_args, "source")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  if((src = dvb_adapter_find_by_identifier(s)) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  printf("Clone from %s to %s\n", src->tda_displayname, dst->tda_displayname);

  dvb_tda_clone(dst, src);

  htsbuf_qprintf(tq, "new Ajax.Updater('dvbadaptereditor', "
	      "'/ajax/dvbadaptereditor/%s', "
	      "{method: 'get', evalScripts: true});\r\n",
	      dst->tda_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 * Delete adapter
 */
static int
ajax_dvbadapterdelete(http_connection_t *hc, http_reply_t *hr, 
		      const char *remain, void *opaque)
{
  htsbuf_queue_t *tq = &hr->hr_q;
  th_dvb_adapter_t *tda;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  htsbuf_qprintf(tq, "var o = $('summary_%s'); o.parentNode.removeChild(o);\r\n",
	      tda->tda_identifier);
  htsbuf_qprintf(tq, "$('dvbadaptereditor').innerHTML ='';\r\n");

  dvb_tda_destroy(tda);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 *
 */
void
ajax_config_dvb_init(void)
{
  http_path_add("/ajax/dvbadaptermuxlist"   , NULL, ajax_adaptermuxlist,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadaptersummary"   , NULL, ajax_adaptersummary,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadapterrename"    , NULL, ajax_adapterrename,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadaptereditor",     NULL, ajax_adaptereditor,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadapteraddmux",     NULL, ajax_adapteraddmux,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadapterdelmux",     NULL, ajax_adapterdelmux,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadaptercreatemux",  NULL, ajax_adaptercreatemux,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbmuxeditor",         NULL, ajax_dvbmuxeditor,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbmuxall",            NULL, ajax_dvbmuxall,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbnetworkinfo",       NULL, ajax_dvbnetworkinfo,
		AJAX_ACCESS_CONFIG);
  http_path_add("/ajax/dvbadapteraddnetwork", NULL, ajax_dvbadapteraddnetwork,
		AJAX_ACCESS_CONFIG);
 http_path_add("/ajax/dvbadapterclone",       NULL, ajax_dvbadapterclone,
		AJAX_ACCESS_CONFIG);
 http_path_add("/ajax/dvbadapterdelete",      NULL, ajax_dvbadapterdelete,
		AJAX_ACCESS_CONFIG);

}
