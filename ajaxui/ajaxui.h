/*
 *  tvheadend, AJAX user interface
 *  Copyright (C) 2007 Andreas Öman
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
 *  along with this program.  If not, see <htmlui://www.gnu.org/licenses/>.
 */

#ifndef AJAXUI_H_
#define AJAXUI_H_

#include "access.h"

#define AJAX_ACCESS_CONFIG (ACCESS_WEB_INTERFACE | ACCESS_CONFIGURE)
#define AJAX_ACCESS_ACCESSCTRL \
 (ACCESS_WEB_INTERFACE | ACCESS_CONFIGURE | ACCESS_ACCESSCONTROL)

typedef enum {
  AJAX_BOX_FILLED,
  AJAX_BOX_SIDEBOX,
  AJAX_BOX_BORDER,
} ajax_box_t;


void ajax_box_begin(tcp_queue_t *tq, ajax_box_t type,
		    const char *id, const char *style, const char *title);

void ajax_box_end(tcp_queue_t *tq, ajax_box_t type);



typedef struct {
  int columns;
  int curcol;
  
  int inrow;
  const char *rowid;

  int csize[20];
  tcp_queue_t *tq;
  int rowcol;
} ajax_table_t;

void ajax_table_top(ajax_table_t *t, http_connection_t *hc, tcp_queue_t *tq,
		    const char *names[], int weights[]);
void ajax_table_row_start(ajax_table_t *t, const char *id);
void ajax_table_cell(ajax_table_t *t, const char *id, const char *fmt, ...);
void ajax_table_bottom(ajax_table_t *t);
void ajax_table_cell_detail_toggler(ajax_table_t *t);
void ajax_table_cell_checkbox(ajax_table_t *t);
void ajax_table_details_start(ajax_table_t *t);
void ajax_table_details_end(ajax_table_t *t);
void ajax_table_subrow_start(ajax_table_t *t);
void ajax_table_subrow_end(ajax_table_t *t);




void ajax_js(tcp_queue_t *tq, const char *fmt, ...);


TAILQ_HEAD(ajax_menu_entry_queue, ajax_menu_entry);

#define AJAX_TAB_CHANNELS      0
#define AJAX_TAB_RECORDER      1
#define AJAX_TAB_CONFIGURATION 2
#define AJAX_TAB_ABOUT         3
#define AJAX_TABS              4


void ajax_mailbox_init(void);
void ajaxui_start(void);
void ajax_channels_init(void);
void ajax_config_init(void);

void ajax_menu_bar_from_array(tcp_queue_t *tq, const char *name, 
			      const char **vec, int num, int cur);

int ajax_channelgroup_tab(http_connection_t *hc, http_reply_t *hr);
int ajax_config_tab(http_connection_t *hc, http_reply_t *hr);

int ajax_config_channels_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_channels_init(void);

int ajax_config_dvb_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_dvb_init(void);

int ajax_config_xmltv_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_xmltv_init(void);

int ajax_config_access_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_access_init(void);

void ajax_config_transport_init(void);

int ajax_transport_build_list(http_connection_t *hc, tcp_queue_t *tq,
			      struct th_transport_list *tlist,
			      int ntransports);

const char *ajaxui_escape_apostrophe(const char *content);
void ajax_generate_select_functions(tcp_queue_t *tq, const char *fprefix,
				    char **selvector);

void ajax_a_jsfuncf(tcp_queue_t *tq, const char *innerhtml,
		    const char *fmt, ...);

void ajax_button(tcp_queue_t *tq, const char *caption, const char *code, ...);

#endif /* AJAXUI_H_ */
