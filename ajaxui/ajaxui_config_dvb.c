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

#include "ajaxui_mailbox.h"


static void
add_option(tcp_queue_t *tq, int bol, const char *name)
{
  if(bol)
    tcp_qprintf(tq, "<option>%s</option>", name);
}


static void
tdmi_displayname(th_dvb_mux_instance_t *tdmi, char *buf, size_t len)
{
  int f = tdmi->tdmi_fe_params->frequency;

  if(tdmi->tdmi_adapter->tda_type == FE_QPSK) {
    snprintf(buf, len, "%d kHz %s", f,
	     dvb_polarisation_to_str(tdmi->tdmi_polarisation));
  } else {
    snprintf(buf, len, "%d kHz", f / 1000);
  }
}

/*
 * Adapter summary
 */
static int
ajax_adaptersummary(http_connection_t *hc, http_reply_t *hr, 
		    const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  th_dvb_adapter_t *tda;
  char dispname[20];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  snprintf(dispname, sizeof(dispname), "%s", tda->tda_displayname);
  strcpy(dispname + sizeof(dispname) - 4, "...");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, dispname);

  tcp_qprintf(tq, "<div class=\"infoprefix\">Device:</div>"
	      "<div>%s</div>",
	      tda->tda_rootpath ?: "<b><i>Not present</i></b>");
  tcp_qprintf(tq, "<div class=\"infoprefix\">Type:</div>"
	      "<div>%s</div>", 
	      dvb_adaptertype_to_str(tda->tda_type));
 
  tcp_qprintf(tq, "<div style=\"text-align: center\">"
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
  tcp_queue_t *tq = &hr->hr_tq;
  th_dvb_adapter_t *tda;

  tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");

  TAILQ_FOREACH(tda, &dvb_adapters, tda_global_link) {

    tcp_qprintf(tq, "<div id=\"summary_%s\" "
		"style=\"float:left; width: 250px\"></div>",
		tda->tda_identifier);

    ajax_js(tq, "new Ajax.Updater('summary_%s', "
	    "'/ajax/dvbadaptersummary/%s', {method: 'get'})",
	    tda->tda_identifier, tda->tda_identifier);

  }
  tcp_qprintf(tq, "</div>");
  tcp_qprintf(tq, "<div id=\"dvbadaptereditor\"></div>");
  http_output_html(hc, hr);
  return 0;
}

/**
 * Generate the 'add new...' mux link if the adapter has backing hardware
 *
 * if result is set we add a fade out of a result from a previous op
 */
static void
dvb_make_add_link(tcp_queue_t *tq, th_dvb_adapter_t *tda, const char *result)
{
  if(tda->tda_fe_info != NULL) {
    tcp_qprintf(tq,
		"<p><a href=\"javascript:void(0);\" "
		"onClick=\"new Ajax.Updater('addmux', "
		"'/ajax/dvbadapteraddmux/%s', {method: 'get'})\""
		">Add new...</a></p>", tda->tda_identifier);
  }

  if(result) {
    tcp_qprintf(tq, "<div id=\"result\">%s</div>", result);
    ajax_js(tq, "Effect.Fade('result')");
  }
}

/**
 *
 */
const char *
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



/**
 * DVB adapter editor pane
 */
static int
ajax_adaptereditor(http_connection_t *hc, http_reply_t *hr, 
		   const char *remain, void *opaque)
{
  tcp_queue_t *tq = &hr->hr_tq;
  th_dvb_adapter_t *tda;
  const char *s;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  ajax_box_begin(tq, AJAX_BOX_FILLED, NULL, NULL, NULL);

  tcp_qprintf(tq, 
	      "<div id=\"adaptername_%s\" "
	      "style=\"text-align: center; font-weight: bold\">%s</div>",
	      tda->tda_identifier, tda->tda_displayname);
  
  ajax_box_end(tq, AJAX_BOX_FILLED);

  /* Type */

  tcp_qprintf(tq, "<div style=\"overflow: auto; width:100%%\">");

  tcp_qprintf(tq, "<div style=\"float: left; width:45%%\">");

  tcp_qprintf(tq, "<div class=\"infoprefixwide\">Model:</div>"
	      "<div>%s (%s)</div>", 
	      tda->tda_fe_info ? tda->tda_fe_info->name : "<Unknown>",
	      dvb_adaptertype_to_str(tda->tda_type));
  


  if(tda->tda_fe_info != NULL) {

    s = tda->tda_type == FE_QPSK ? "kHz" : "Hz";
    tcp_qprintf(tq, "<div class=\"infoprefixwide\">Freq. Range:</div>"
		"<div>%s - %s %s, in steps of %s %s</div>",
		nicenum(tda->tda_fe_info->frequency_min),
		nicenum(tda->tda_fe_info->frequency_max),
		s,
		nicenum(tda->tda_fe_info->frequency_stepsize),
		s);


    if(tda->tda_fe_info->symbol_rate_min) {
      tcp_qprintf(tq, "<div class=\"infoprefixwide\">Symbolrate:</div>"
		  "<div>%s - %s Baud</div>",
		  nicenum(tda->tda_fe_info->symbol_rate_min),
		  nicenum(tda->tda_fe_info->symbol_rate_max));
    }
    /* Capabilities */
    //  tcp_qprintf(tq, "<div class=\"infoprefixwide\">Capabilities:</div>");
  }

  tcp_qprintf(tq, "</div>");

  ajax_a_jsfuncf(tq, "Rename adapter...",
		 "dvb_adapter_rename('%s', '%s');",
		 tda->tda_identifier, tda->tda_displayname);
 
  tcp_qprintf(tq, "</div>");

  /* Muxes and transports */


  tcp_qprintf(tq, "<div style=\"float: left; width:45%\">");

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, "Multiplexes");

  tcp_qprintf(tq, "<div id=\"dvbmuxlist_%s\"></div>",
	      tda->tda_identifier);

  ajax_js(tq, 
	  "new Ajax.Updater('dvbmuxlist_%s', "
	  "'/ajax/dvbadaptermuxlist/%s', {method: 'get', evalScripts: true})",
	  tda->tda_identifier, tda->tda_identifier);

  tcp_qprintf(tq, "<hr><div id=\"addmux\">");
  dvb_make_add_link(tq, tda, NULL);
  tcp_qprintf(tq, "</div>");

  ajax_box_end(tq, AJAX_BOX_SIDEBOX);
  tcp_qprintf(tq, "</div>");

  /* Div for displaying services */

  tcp_qprintf(tq, "<div id=\"servicepane\" "
	      "style=\"float: left; width:55%\">");
  tcp_qprintf(tq, "</div>");

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
  tcp_queue_t *tq = &hr->hr_tq;
  th_dvb_adapter_t *tda;
  int caps;
  int fetype;
  char params[400];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if(tda->tda_fe_info == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  caps   = tda->tda_fe_info->caps;
  fetype = tda->tda_fe_info->type;

  tcp_qprintf(tq, "<div style=\"text-align: center; font-weight: bold\">"
	      "Add new %s mux</div>",
	      dvb_adaptertype_to_str(tda->tda_fe_info->type));

  tcp_qprintf(tq,
	      "<div class=\"infoprefixwidefat\">Frequency (%s):</div>"
	      "<div>"
	      "<input class=\"textinput\" type=\"text\" id=\"freq\">"
	      "</div>",
	      fetype == FE_QPSK ? "kHz" : "Hz");

  snprintf(params, sizeof(params), 
	   "freq: $F('freq')");
  

  /* Symbolrate */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
 
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Symbolrate:</div>"
		"<div>"
		"<input class=\"textinput\" type=\"text\" id=\"symrate\">"
		"</div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", symrate: $F('symrate')");
  }

  /* Bandwidth */

  if(fetype == FE_OFDM) {
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Bandwidth:</div>"
		"<div><select id=\"bw\" class=\"textinput\">");

    add_option(tq, caps & FE_CAN_BANDWIDTH_AUTO, "AUTO");
    add_option(tq, 1                           , "8MHz");
    add_option(tq, 1                           , "7MHz");
    add_option(tq, 1                           , "6MHz");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", bw: $F('bw')");
  }


  /* Constellation */

  if(fetype == FE_QAM || fetype == FE_OFDM) {
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Constellation:</div>"
		"<div><select id=\"const\" class=\"textinput\">");

    add_option(tq, caps & FE_CAN_QAM_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_QPSK,      "QPSK");
    add_option(tq, caps & FE_CAN_QAM_16,    "QAM16");
    add_option(tq, caps & FE_CAN_QAM_32,    "QAM32");
    add_option(tq, caps & FE_CAN_QAM_64,    "QAM64");
    add_option(tq, caps & FE_CAN_QAM_128,   "QAM128");
    add_option(tq, caps & FE_CAN_QAM_256,   "QAM256");

    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", const: $F('const')");
  }


  /* FEC */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">FEC:</div>"
		"<div><select id=\"fec\" class=\"textinput\">");

    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fec: $F('fec')");
  }

  if(fetype == FE_QPSK) {
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Polarisation:</div>"
		"<div><select id=\"pol\" class=\"textinput\">");

    add_option(tq, 1,  "Vertical");
    add_option(tq, 1,  "Horizontal");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", pol: $F('pol')");
    

  }


  if(fetype == FE_OFDM) {
    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Transmission mode:</div>"
		"<div><select id=\"tmode\" class=\"textinput\">");
    
    add_option(tq, caps & FE_CAN_TRANSMISSION_MODE_AUTO, "AUTO");
    add_option(tq, 1                                   , "2k");
    add_option(tq, 1                                   , "8k");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", tmode: $F('tmode')");

    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Guard interval:</div>"
		"<div><select id=\"guard\" class=\"textinput\">");
    
    add_option(tq, caps & FE_CAN_GUARD_INTERVAL_AUTO, "AUTO");
    add_option(tq, 1                                , "1/32");
    add_option(tq, 1                                , "1/16");
    add_option(tq, 1                                , "1/8");
    add_option(tq, 1                                , "1/4");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", guard: $F('guard')");



    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">Hierarchy:</div>"
		"<div><select id=\"hier\" class=\"textinput\">");
    
    add_option(tq, caps & FE_CAN_HIERARCHY_AUTO, "AUTO");
    add_option(tq, 1                           , "1");
    add_option(tq, 1                           , "2");
    add_option(tq, 1                           , "4");
    add_option(tq, 1                           , "NONE");
    tcp_qprintf(tq, "</select></div>");


    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", hier: $F('hier')");



    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">FEC Hi:</div>"
		"<div><select id=\"fechi\" class=\"textinput\">");
    
    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fechi: $F('fechi')");


    tcp_qprintf(tq,
		"<div class=\"infoprefixwidefat\">FEC Low:</div>"
		"<div><select id=\"feclo\" class=\"textinput\">");
    
    add_option(tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", feclo: $F('feclo')");
  }

  tcp_qprintf(tq,
	      "<div style=\"text-align: center\">"
	      "<input type=\"button\" value=\"Create\" "
	      "onClick=\"new Ajax.Updater('addmux', "
	      "'/ajax/dvbadaptercreatemux/%s', "
	      "{evalScripts: true, parameters: {%s}})"
	      "\">"
	      "</div>", tda->tda_identifier, params);

  http_output_html(hc, hr);
  return 0;
}

/**
 *
 */
static int
ajax_adaptercreatemux_fail(http_connection_t *hc, http_reply_t *hr,
			   th_dvb_adapter_t *tda, const char *errmsg)
{
  tcp_queue_t *tq = &hr->hr_tq;
  dvb_make_add_link(tq, tda, errmsg);
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
  tcp_queue_t *tq;
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

  if(v != NULL)
    return ajax_adaptercreatemux_fail(hc, hr, tda, v);

  tq = &hr->hr_tq;
  dvb_make_add_link(tq, tda, "Successfully created");

  ajax_js(tq, 
	  "new Ajax.Updater('dvbmuxlist_%s', "
	  "'/ajax/dvbadaptermuxlist/%s', {method: 'get', evalScripts: true})",
	  tda->tda_identifier, tda->tda_identifier);

  http_output_html(hc, hr);
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
  tcp_queue_t *tq = &hr->hr_tq;
  th_dvb_adapter_t *tda;
  char buf[50];
  int fetype, n, m;
  th_transport_t *t;
  int nmuxes = 0;
  char **selvector;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  fetype = tda->tda_type;

  /* List of muxes */
  
  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
    nmuxes++;
  
  if(nmuxes == 0) {
    tcp_qprintf(tq, "<div style=\"text-align: center\">"
		"No muxes configured</div>");
  } else {


    selvector = alloca(sizeof(char *) * (nmuxes + 1));
    n = 0;
    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
      selvector[n++] = tdmi->tdmi_identifier;
    selvector[n] = NULL;

    ajax_generate_select_functions(tq, "mux", selvector);

    ajax_table_top(&ta, hc, tq,
		   (const char *[])
    {"Freq", "Status", "State", "Name", "Services", "", NULL},
		   (int[])
    {16,12,8,16,8,2});

    LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {

      tdmi_displayname(tdmi, buf, sizeof(buf));

      ajax_table_row_start(&ta, tdmi->tdmi_identifier);

      ajax_table_cell(&ta, NULL,
		      "<a href=\"javascript:void(0);\" "
		      "onClick=\"new Ajax.Updater('servicepane', "
		      "'/ajax/dvbmuxeditor/%s', "
		      "{method: 'get', evalScripts: true})\""
		      ">%s</a>",
		      tdmi->tdmi_identifier, buf);

      ajax_table_cell(&ta, "status", "%s", dvb_mux_status(tdmi));
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
    ajax_table_bottom(&ta);
    tcp_qprintf(tq, "<hr><div style=\"overflow: auto; width: 100%\">");
    tcp_qprintf(tq, "<div class=\"infoprefix\">Select:</div><div>");
    ajax_a_jsfuncf(tq, "All",     "mux_sel_all();");
    tcp_qprintf(tq, " / ");
    ajax_a_jsfuncf(tq, "None",    "mux_sel_none();");
    tcp_qprintf(tq, " / ");
    ajax_a_jsfuncf(tq, "Invert",  "mux_sel_invert();");
    tcp_qprintf(tq, "</div></div>\r\n");

    tcp_qprintf(tq, "<div style=\"overflow: auto; width: 100%\">");
    tcp_qprintf(tq, "<div class=\"infoprefix\">&nbsp</div><div>");
    ajax_a_jsfuncf(tq, "Delete selected", 
		   "mux_sel_do('dvbadapterdelmux/%s', '', '', true)",
		   tda->tda_identifier);
    tcp_qprintf(tq, "</div></div>\r\n");


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
  return a->tht_dvb_service_id - b->tht_dvb_service_id;
}

/**
 * Display detailes about a mux
 */
static int
ajax_dvbmuxeditor(http_connection_t *hc, http_reply_t *hr, 
		  const char *remain, void *opaque)
{
  th_dvb_mux_instance_t *tdmi;
  tcp_queue_t *tq = &hr->hr_tq;
  char buf[1000];
  th_transport_t *t;
  struct th_transport_list head;
  int n = 0;

  if(remain == NULL || (tdmi = dvb_mux_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tdmi_displayname(tdmi, buf, sizeof(buf));

  LIST_INIT(&head);

  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    if(transport_is_available(t)) {
      LIST_INSERT_SORTED(&head, t, tht_tmp_link, dvbsvccmp);
      n++;
    }
  }

  ajax_box_begin(tq, AJAX_BOX_SIDEBOX, NULL, NULL, buf);
  ajax_transport_build_list(hc, tq, &head, n);
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
  tcp_queue_t *tq = &hr->hr_tq;
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
 
  tcp_qprintf(tq,
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

  tcp_queue_t *tq = &hr->hr_tq;
  const char *s;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  if((s = http_arg_get(&hc->hc_req_args, "newname")) == NULL)
    return HTTP_STATUS_BAD_REQUEST;

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(s);
  dvb_tda_save(tda);
 
  tcp_qprintf(tq,
	      "$('adaptername_%s').innerHTML='%s';",
	      tda->tda_identifier, tda->tda_displayname);

  tcp_qprintf(tq,
	      "new Ajax.Updater('summary_%s', "
	      "'/ajax/dvbadaptersummary/%s', {method: 'get'})",
	    tda->tda_identifier, tda->tda_identifier);

  http_output(hc, hr, "text/javascript; charset=UTF8", NULL, 0);
  return 0;
}


/**
 *
 */
void
ajax_config_dvb_init(void)
{
  http_path_add("/ajax/dvbadaptermuxlist"   , NULL, ajax_adaptermuxlist);
  http_path_add("/ajax/dvbadaptersummary"   , NULL, ajax_adaptersummary);
  http_path_add("/ajax/dvbadapterrename"    , NULL, ajax_adapterrename);
  http_path_add("/ajax/dvbadaptereditor",     NULL, ajax_adaptereditor);
  http_path_add("/ajax/dvbadapteraddmux",     NULL, ajax_adapteraddmux);
  http_path_add("/ajax/dvbadapterdelmux",     NULL, ajax_adapterdelmux);
  http_path_add("/ajax/dvbadaptercreatemux",  NULL, ajax_adaptercreatemux);
  http_path_add("/ajax/dvbmuxeditor",         NULL, ajax_dvbmuxeditor);

}
