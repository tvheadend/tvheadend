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


static struct strtab adapterstatus[] = {
  { "Hardware detected",    TDA_STATE_RUNNING },
  { "Hardware not found",   TDA_STATE_ZOMBIE },
};

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

  if(tdmi->tdmi_adapter->tda_fe_info->type == FE_QPSK) {
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
ajax_adaptersummary(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;
  char dispname[20];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tcp_init_queue(&tq, -1);

  snprintf(dispname, sizeof(dispname), "%s", tda->tda_displayname);
  strcpy(dispname + sizeof(dispname) - 4, "...");

  ajax_box_begin(&tq, AJAX_BOX_SIDEBOX, NULL, NULL, dispname);

  tcp_qprintf(&tq, "<div class=\"infoprefix\">Status:</div>"
	      "<div>%s</div>",
	      val2str(tda->tda_state, adapterstatus) ?: "invalid");
  tcp_qprintf(&tq, "<div class=\"infoprefix\">Type:</div>"
	      "<div>%s</div>", 
	      dvb_adaptertype_to_str(tda->tda_fe_info->type));
 
  tcp_qprintf(&tq, "<div style=\"text-align: center\">"
	      "<a href=\"javascript:void(0);\" "
	      "onClick=\"new Ajax.Updater('dvbadaptereditor', "
	      "'/ajax/dvbadaptereditor/%s', "
	      "{method: 'get', evalScripts: true})\""
	      "\">Edit</a></div>", tda->tda_identifier);
  ajax_box_end(&tq, AJAX_BOX_SIDEBOX);

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}


/**
 * DVB configuration
 */
int
ajax_config_dvb_tab(http_connection_t *hc)
{
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;

  tcp_init_queue(&tq, -1);

  tcp_qprintf(&tq, "<div style=\"overflow: auto; width: 100%\">");

  LIST_FOREACH(tda, &dvb_adapters, tda_global_link) {

    tcp_qprintf(&tq, "<div id=\"summary_%s\" "
		"style=\"float:left; width: 250px\"></div>",
		tda->tda_identifier);

    ajax_js(&tq, "new Ajax.Updater('summary_%s', "
	    "'/ajax/dvbadaptersummary/%s', {method: 'get'})",
	    tda->tda_identifier, tda->tda_identifier);

  }
  tcp_qprintf(&tq, "</div>");
  tcp_qprintf(&tq, "<div id=\"dvbadaptereditor\"></div>");
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}

/**
 * Generate the 'add new...' mux link
 *
 * if result is set we add a fade out of a result from a previous op
 */
static void
dvb_make_add_link(tcp_queue_t *tq, th_dvb_adapter_t *tda, const char *result)
{
  tcp_qprintf(tq,
	      "<p><a href=\"javascript:void(0);\" "
	      "onClick=\"new Ajax.Updater('addmux', "
	      "'/ajax/dvbadapteraddmux/%s', {method: 'get'})\""
	      ">Add new...</a></p>", tda->tda_identifier);

  if(result) {
    tcp_qprintf(tq, "<div id=\"result\">%s</div>", result);
    ajax_js(tq, "Effect.Fade('result')");
  }
}


/**
 * DVB adapter editor pane
 */
static int
ajax_adaptereditor(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;
  float a, b, c;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tcp_init_queue(&tq, -1);

  ajax_box_begin(&tq, AJAX_BOX_FILLED, NULL, NULL, NULL);

  tcp_qprintf(&tq, 
	      "<div style=\"text-align: center; font-weight: bold\">%s</div>",
	      tda->tda_displayname);
  
  ajax_box_end(&tq, AJAX_BOX_FILLED);

  /* Type */

  tcp_qprintf(&tq, "<div class=\"infoprefixwide\">Model:</div>"
	      "<div>%s (%s)</div>", 
	      tda->tda_fe_info->name,
	      dvb_adaptertype_to_str(tda->tda_fe_info->type));
  
 
  switch(tda->tda_fe_info->type) {
  case FE_QPSK:
    a = tda->tda_fe_info->frequency_min;
    b = tda->tda_fe_info->frequency_max;
    c = tda->tda_fe_info->frequency_stepsize;
    break;

  default:
    a = tda->tda_fe_info->frequency_min / 1000.0f;
    b = tda->tda_fe_info->frequency_max / 1000.0f;
    c = tda->tda_fe_info->frequency_stepsize / 1000.0f;
    break;
  }

  tcp_qprintf(&tq, "<div class=\"infoprefixwide\">Freq. Range:</div>"
	      "<div>%.2f - %.2f kHz, in steps of %.2f kHz</div>",
	      a, b, c);


  if(tda->tda_fe_info->symbol_rate_min) {
    tcp_qprintf(&tq, "<div class=\"infoprefixwide\">Symbolrate:</div>"
		"<div>%d - %d BAUD</div>",
		tda->tda_fe_info->symbol_rate_min,
		tda->tda_fe_info->symbol_rate_max);
  }

 
  /* Capabilities */


  //  tcp_qprintf(&tq, "<div class=\"infoprefixwide\">Capabilities:</div>");

  tcp_qprintf(&tq, "<div style=\"float: left; width:45%\">");

  ajax_box_begin(&tq, AJAX_BOX_SIDEBOX, NULL, NULL, "Multiplexes");

  tcp_qprintf(&tq, "<div id=\"dvbmuxlist%s\"></div>",
	      tda->tda_identifier);

  ajax_js(&tq, 
	  "new Ajax.Updater('dvbmuxlist%s', "
	  "'/ajax/dvbadaptermuxlist/%s', {method: 'get'}) ",
	  tda->tda_identifier, tda->tda_identifier);

  tcp_qprintf(&tq, "<hr><div id=\"addmux\">");
  dvb_make_add_link(&tq, tda, NULL);
  tcp_qprintf(&tq, "</div>");

  ajax_box_end(&tq, AJAX_BOX_SIDEBOX);
  tcp_qprintf(&tq, "</div>");

  /* Div for displaying services */

  tcp_qprintf(&tq, "<div id=\"servicepane\" "
	      "style=\"float: left; width:55%\">");
  tcp_qprintf(&tq, "</div>");

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}


/**
 * DVB adapter add mux
 */
static int
ajax_adapteraddmux(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;
  int caps;
  int fetype;
  char params[400];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  caps   = tda->tda_fe_info->caps;
  fetype = tda->tda_fe_info->type;

  tcp_init_queue(&tq, -1);

  tcp_qprintf(&tq, "<div style=\"text-align: center; font-weight: bold\">"
	      "Add new %s mux</div>",
	      dvb_adaptertype_to_str(tda->tda_fe_info->type));

  tcp_qprintf(&tq,
	      "<div class=\"infoprefixwidefat\">Frequency (%s):</div>"
	      "<div>"
	      "<input class=\"textinput\" type=\"text\" id=\"freq\">"
	      "</div>",
	      fetype == FE_QPSK ? "kHz" : "Hz");

  snprintf(params, sizeof(params), 
	   "freq: $F('freq')");
  

  /* Symbolrate */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
 
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Symbolrate:</div>"
		"<div>"
		"<input class=\"textinput\" type=\"text\" id=\"symrate\">"
		"</div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", symrate: $F('symrate')");
  }

  /* Bandwidth */

  if(fetype == FE_OFDM) {
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Bandwidth:</div>"
		"<div><select id=\"bw\" class=\"textinput\">");

    add_option(&tq, caps & FE_CAN_BANDWIDTH_AUTO, "AUTO");
    add_option(&tq, 1                           , "8MHz");
    add_option(&tq, 1                           , "7MHz");
    add_option(&tq, 1                           , "6MHz");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", bw: $F('bw')");
  }


  /* Constellation */

  if(fetype == FE_QAM || fetype == FE_OFDM) {
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Constellation:</div>"
		"<div><select id=\"const\" class=\"textinput\">");

    add_option(&tq, caps & FE_CAN_QAM_AUTO,  "AUTO");
    add_option(&tq, caps & FE_CAN_QPSK,      "QPSK");
    add_option(&tq, caps & FE_CAN_QAM_16,    "QAM16");
    add_option(&tq, caps & FE_CAN_QAM_32,    "QAM32");
    add_option(&tq, caps & FE_CAN_QAM_64,    "QAM64");
    add_option(&tq, caps & FE_CAN_QAM_128,   "QAM128");
    add_option(&tq, caps & FE_CAN_QAM_256,   "QAM256");

    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", const: $F('const')");
  }


  /* FEC */

  if(fetype == FE_QAM || fetype == FE_QPSK) {
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">FEC:</div>"
		"<div><select id=\"fec\" class=\"textinput\">");

    add_option(&tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(&tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(&tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(&tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(&tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(&tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(&tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(&tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(&tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fec: $F('fec')");
  }

  if(fetype == FE_QPSK) {
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Polarisation:</div>"
		"<div><select id=\"pol\" class=\"textinput\">");

    add_option(&tq, 1,  "Vertical");
    add_option(&tq, 1,  "Horizontal");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", pol: $F('pol')");
    

  }


  if(fetype == FE_OFDM) {
    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Transmission mode:</div>"
		"<div><select id=\"tmode\" class=\"textinput\">");
    
    add_option(&tq, caps & FE_CAN_TRANSMISSION_MODE_AUTO, "AUTO");
    add_option(&tq, 1                                   , "2k");
    add_option(&tq, 1                                   , "8k");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", tmode: $F('tmode')");

    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Guard interval:</div>"
		"<div><select id=\"guard\" class=\"textinput\">");
    
    add_option(&tq, caps & FE_CAN_GUARD_INTERVAL_AUTO, "AUTO");
    add_option(&tq, 1                                , "1/32");
    add_option(&tq, 1                                , "1/16");
    add_option(&tq, 1                                , "1/8");
    add_option(&tq, 1                                , "1/4");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", guard: $F('guard')");



    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">Hierarchy:</div>"
		"<div><select id=\"hier\" class=\"textinput\">");
    
    add_option(&tq, caps & FE_CAN_HIERARCHY_AUTO, "AUTO");
    add_option(&tq, 1                           , "1");
    add_option(&tq, 1                           , "2");
    add_option(&tq, 1                           , "4");
    add_option(&tq, 1                           , "NONE");
    tcp_qprintf(&tq, "</select></div>");


    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", hier: $F('hier')");



    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">FEC Hi:</div>"
		"<div><select id=\"fechi\" class=\"textinput\">");
    
    add_option(&tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(&tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(&tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(&tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(&tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(&tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(&tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(&tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(&tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", fechi: $F('fechi')");


    tcp_qprintf(&tq,
		"<div class=\"infoprefixwidefat\">FEC Low:</div>"
		"<div><select id=\"feclo\" class=\"textinput\">");
    
    add_option(&tq, caps & FE_CAN_FEC_AUTO,  "AUTO");
    add_option(&tq, caps & FE_CAN_FEC_1_2,   "1/2");
    add_option(&tq, caps & FE_CAN_FEC_2_3,   "2/3");
    add_option(&tq, caps & FE_CAN_FEC_3_4,   "3/4");
    add_option(&tq, caps & FE_CAN_FEC_4_5,   "4/5");
    add_option(&tq, caps & FE_CAN_FEC_5_6,   "5/6");
    add_option(&tq, caps & FE_CAN_FEC_6_7,   "6/7");
    add_option(&tq, caps & FE_CAN_FEC_7_8,   "7/8");
    add_option(&tq, caps & FE_CAN_FEC_8_9,   "8/9");
    tcp_qprintf(&tq, "</select></div>");

    snprintf(params + strlen(params), sizeof(params) - strlen(params), 
	     ", feclo: $F('feclo')");
  }

  tcp_qprintf(&tq,
	      "<div style=\"text-align: center\">"
	      "<input type=\"button\" value=\"Create\" "
	      "onClick=\"new Ajax.Updater('addmux', "
	      "'/ajax/dvbadaptercreatemux/%s', "
	      "{evalScripts: true, parameters: {%s}})"
	      "\">"
	      "</div>", tda->tda_identifier, params);

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}

/**
 *
 */
static int
ajax_adaptercreatemux_fail(http_connection_t *hc, th_dvb_adapter_t *tda, 
			   const char *errmsg)
{
  tcp_queue_t tq;
  tcp_init_queue(&tq, -1);
  dvb_make_add_link(&tq, tda, errmsg);
  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}

/**
 * DVB adapter create mux (come here on POST after addmux query)
 */
static int
ajax_adaptercreatemux(http_connection_t *hc, const char *remain, void *opaque)
{
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;
  const char *v;

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  v = dvb_mux_create_str(tda,
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
    return ajax_adaptercreatemux_fail(hc, tda, v);

  tcp_init_queue(&tq, -1);
  dvb_make_add_link(&tq, tda, "Successfully created");

  ajax_js(&tq, 
	  "new Ajax.Updater('dvbmuxlist%s', "
	  "'/ajax/dvbadaptermuxlist/%s', {method: 'get'}) ",
	  tda->tda_identifier, tda->tda_identifier);

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
  return 0;
}


/**
 * Return a list of all muxes on the given adapter
 */
static int
ajax_adaptermuxlist(http_connection_t *hc, const char *remain, void *opaque)
{
  th_dvb_mux_instance_t *tdmi;
  tcp_queue_t tq;
  th_dvb_adapter_t *tda;
  char buf[50], buf2[500], buf3[20];
  const char *txt;
  int fetype, n;
  th_transport_t *t;
  int o = 1, v;
  int csize[10];
  int nmuxes = 0;

  int displines = 21;

  const char *cells[10];

  if(remain == NULL || (tda = dvb_adapter_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  fetype = tda->tda_fe_info->type;

  tcp_init_queue(&tq, -1);

  /* List of muxes */
  
  LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link)
    nmuxes++;
  
  ajax_table_header(hc, &tq,
		    (const char *[])
		    {"Freq", "Status", "State", "Name", "Services", NULL},
		    (int[]){3,3,2,4,2},
		    nmuxes > displines,
		    csize);

  tcp_qprintf(&tq, "<hr>");

  v = displines;
  if(nmuxes < displines)
    v = nmuxes;

  tcp_qprintf(&tq, "<div id=\"dvbmuxlist%s\" "
	      "style=\"height: %dpx; overflow: auto\" class=\"normallist\">",
	      tda->tda_identifier, v * 14);

  if(nmuxes == 0) {
    tcp_qprintf(&tq, "<div style=\"text-align: center\">"
		"No muxes configured</div>");
  } else LIST_FOREACH(tdmi, &tda->tda_muxes, tdmi_adapter_link) {

    tdmi_displayname(tdmi, buf, sizeof(buf));

    snprintf(buf2, sizeof(buf2), 
	     "<a href=\"javascript:void(0);\" "
	     "onClick=\"new Ajax.Updater('servicepane', "
	     "'/ajax/dvbmuxeditor/%s', {method: 'get', evalScripts: true})\""
	     ">%s</a>", tdmi->tdmi_identifier, buf);

    cells[0] = buf2;
    cells[1] = dvb_mux_status(tdmi);
 
    switch(tdmi->tdmi_state) {
    case TDMI_IDLE:      txt = "Idle";      break;
    case TDMI_IDLESCAN:  txt = "Scanning";  break;
    case TDMI_RUNNING:   txt = "Running";   break;
    default:             txt = "???";       break;
    }

    cells[2] = txt;
 
    txt = tdmi->tdmi_network;
    if(txt == NULL)
      txt = "Unknown";

    cells[3] = txt;

    n = 0;
    LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link)
      n++;

    snprintf(buf3, sizeof(buf3), "%d", n);
    cells[4] = buf3;
    cells[5] = NULL;

    ajax_table_row(&tq, cells, csize, &o);

  }
  tcp_qprintf(&tq, "</div>");

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
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
ajax_dvbmuxeditor(http_connection_t *hc, const char *remain, void *opaque)
{
  th_dvb_mux_instance_t *tdmi;
  tcp_queue_t tq;
  char buf[1000];
  th_transport_t *t;
  struct th_transport_list head;

  if(remain == NULL || (tdmi = dvb_mux_find_by_identifier(remain)) == NULL)
    return HTTP_STATUS_NOT_FOUND;

  tcp_init_queue(&tq, -1);

  tdmi_displayname(tdmi, buf, sizeof(buf));

  LIST_INIT(&head);

  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    if(transport_servicetype_txt(t) == NULL)
      continue;
    LIST_INSERT_SORTED(&head, t, tht_tmp_link, dvbsvccmp);
  }

  ajax_box_begin(&tq, AJAX_BOX_SIDEBOX, NULL, NULL, buf);
  ajax_transport_build_list(&tq, &head);
  ajax_box_end(&tq, AJAX_BOX_SIDEBOX);

  http_output_queue(hc, &tq, "text/html; charset=UTF-8", 0);
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
  http_path_add("/ajax/dvbadaptereditor",     NULL, ajax_adaptereditor);
  http_path_add("/ajax/dvbadapteraddmux",     NULL, ajax_adapteraddmux);
  http_path_add("/ajax/dvbadaptercreatemux",  NULL, ajax_adaptercreatemux);
  http_path_add("/ajax/dvbmuxeditor",         NULL, ajax_dvbmuxeditor);

}
