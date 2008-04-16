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

typedef enum {
  AJAX_BOX_FILLED,
  AJAX_BOX_SIDEBOX,
  AJAX_BOX_BORDER,
} ajax_box_t;

void ajax_box_begin(tcp_queue_t *tq, ajax_box_t type,
		    const char *id, const char *style, const char *title);

void ajax_box_end(tcp_queue_t *tq, ajax_box_t type);

void ajax_js(tcp_queue_t *tq, const char *fmt, ...);


TAILQ_HEAD(ajax_menu_entry_queue, ajax_menu_entry);

#define AJAX_TAB_CHANNELS      0
#define AJAX_TAB_RECORDER      1
#define AJAX_TAB_CONFIGURATION 2
#define AJAX_TAB_ABOUT         3
#define AJAX_TABS              4


void ajaxui_start(void);
void ajax_channels_init(void);
void ajax_config_init(void);
void ajax_mailbox_init(void);
int ajax_mailbox_create(char *subscriptionid);


void ajax_menu_bar_from_array(tcp_queue_t *tq, const char *name, 
			      const char **vec, int num, int cur);

void ajax_a_jsfunc(tcp_queue_t *tq, const char *innerhtml, const char *func,
		   const char *trailer);

int ajax_channelgroup_tab(http_connection_t *hc, http_reply_t *hr);
int ajax_config_tab(http_connection_t *hc, http_reply_t *hr);

int ajax_config_channels_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_channels_init(void);

int ajax_config_dvb_tab(http_connection_t *hc, http_reply_t *hr);
void ajax_config_dvb_init(void);
void ajax_config_transport_init(void);

int ajax_transport_build_list(http_connection_t *hc, tcp_queue_t *tq,
			      struct th_transport_list *tlist,
			      int ntransports);


void ajax_table_header(http_connection_t *hc, tcp_queue_t *tq,
		       const char *names[], int weights[],
		       int scrollbar, int columnsizes[]);

void ajax_table_row(tcp_queue_t *tq, const char *cells[], int columnsizes[],
		    int *bgptr, const char *idprefix[], const char *idpostfix);

#endif /* AJAXUI_H_ */
